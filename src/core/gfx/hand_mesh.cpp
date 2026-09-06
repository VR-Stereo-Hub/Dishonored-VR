// core/gfx/hand_mesh.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

static void HmTriAdd(int h, const float* a, const float* b, const float* c,
                     const float* col, float shade)
{
    if (!g_hmGeo[h]) {
        g_hmGeo[h] = (HmTri*)calloc(HM_MAXTRI, sizeof(HmTri));
        if (!g_hmGeo[h]) return;
    }
    if (g_hmGeoN[h] >= HM_MAXTRI) return;
    HmTri* t = &g_hmGeo[h][g_hmGeoN[h]++];
    for (int k = 0; k < 3; k++) { t->p[0][k] = a[k]; t->p[1][k] = b[k]; t->p[2][k] = c[k]; }
    for (int k = 0; k < 3; k++) t->c[k] = col[k] * shade;
    t->c[3] = 1.0f;
    for (int k = 0; k < 3; k++) { t->uv[k][0] = 0.5f; t->uv[k][1] = 0.5f; }
    t->tex = g_hmTexSlot;
}

// A box that may taper toward its -Z end, which is enough to make a blade.
static void HmBox(int h, float cx, float cy, float cz, float hx, float hy, float hz,
                  float tipScale, const float* col)
{
    float v[8][3];
    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) ? 1.0f : -1.0f;
        float sy = (i & 2) ? 1.0f : -1.0f;
        float sz = (i & 4) ? 1.0f : -1.0f;
        float k = (sz < 0) ? tipScale : 1.0f;        // -Z end is the tip
        v[i][0] = cx + sx * hx * k;
        v[i][1] = cy + sy * hy * k;
        v[i][2] = cz + sz * hz;
    }
    static const int f[6][4] = { {0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4} };
    static const float sh[6] = { 0.55f, 0.75f, 0.65f, 0.85f, 1.0f, 0.45f };
    for (int q = 0; q < 6; q++) {
        HmTriAdd(h, v[f[q][0]], v[f[q][1]], v[f[q][2]], col, sh[q]);
        HmTriAdd(h, v[f[q][0]], v[f[q][2]], v[f[q][3]], col, sh[q]);
    }
}

// ---------------------------------------------------------------------------
// 41.2 (VR-31): the hand pass's OWN trim rotation.
//
// This used to call `RtdBuildYPR`, which lives in `src/legacy/rtd_drive.cpp`
// and is compiled ONLY under -DDVR_WITH_LEGACY=ON. Every shipped build takes
// the stub in `src/legacy/legacy_stubs.inc` instead:
//
//     static void RtdBuildYPR(const float* ypr, float* out) {}
//
// The call site declared `float B[9];` and never initialised it, so in every
// normal build each hand vertex was rotated by nine floats of whatever was on
// the stack. That is undefined behaviour with a very specific signature: the
// triangle counter still climbs (the geometry is "built"), and NOTHING is
// visible, because garbage can scatter the vertices anywhere or make them
// non-finite - and a NaN sails straight through the near-plane reject below,
// since `NaN > -0.03f` is false. Found by review, 2026-09-06; it is the reason
// the first run drew 252 triangles per present and showed nothing.
//
// So the math is a live dependency now, local to this file. The retired
// experiment stays retired: we take the twelve lines we need, not the
// subsystem.
//
// CONVENTION, and it differs from the legacy helper on purpose. RtdBuildYPR is
// commented "Z yaw, Y pitch, X roll", which is the UE3 world frame (Z up).
// These meshes are built in the CONTROLLER's frame, which is Y-up: +X right,
// +Y up, -Z along the grip. So yaw is about Y, pitch about X, roll about Z,
// and the ini keys LYaw/LPitch/LRoll now mean what they say when you trim a
// hand. This changes nothing in the current configuration - every trim value
// in the shipped ini is 0.0 - and at zero it must produce EXACTLY identity,
// which is the property the stub failed to have.
static void HmBuildYPR(const float* ypr, float* out)   // Y yaw, X pitch, Z roll
{
    const float cy = cosf(ypr[0]), sy = sinf(ypr[0]);
    const float cp = cosf(ypr[1]), sp = sinf(ypr[1]);
    const float cr = cosf(ypr[2]), sr = sinf(ypr[2]);
    // R = Ry(yaw) * Rx(pitch) * Rz(roll), written out so there is no dependency
    // on a matrix helper that could itself be stubbed.
    out[0] =  cy*cr + sy*sp*sr;  out[1] = -cy*sr + sy*sp*cr;  out[2] =  sy*cp;
    out[3] =  cp*sr;             out[4] =  cp*cr;             out[5] = -sp;
    out[6] = -sy*cr + cy*sp*sr;  out[7] =  sy*sr + cy*sp*cr;  out[8] =  cy*cp;
}


// A gripping hand: palm, four fingers curled round the grip (which runs down
// -Z), a thumb on the inside, and a coat cuff behind so it does not read as a
// severed hand floating in the air. side = +1 right, -1 left, which flips the
// thumb to the correct side.
static void HmHand(int h, float side)
{
    const float glove[3] = { 0.15f, 0.13f, 0.12f };
    const float cuff [3] = { 0.24f, 0.19f, 0.15f };
    HmBox(h, 0, 0, 0.020f, 0.028f, 0.040f, 0.048f, 1.0f, glove);        // palm
    for (int f = 0; f < 4; f++) {                                        // fingers
        float z = 0.052f - f * 0.026f;
        float w = (f == 3) ? 0.011f : 0.013f;
        HmBox(h, side * 0.004f, -0.030f, z, w, 0.016f, 0.011f, 1.0f, glove);
    }
    HmBox(h, -side * 0.030f, -0.008f, 0.028f, 0.012f, 0.022f, 0.016f, 1.0f, glove); // thumb
    HmBox(h, 0, 0, 0.098f, 0.041f, 0.048f, 0.040f, 1.0f, cuff);          // cuff
}


// Drop every loaded skin so the next build re-reads them from disk. Cheap, and
// it means editing a texture behaves the same as editing the mesh.
static void HmDropSkins()
{
    for (int q = 1; q < 8; q++) {
        if (g_hmSkin[q]) { g_hmSkin[q]->Release(); g_hmSkin[q] = NULL; }
        g_hmSkinName[q][0] = 0; g_hmSkinPath[q][0] = 0;
    }
    g_hmSkinN = 1;
}


static ID3D11ShaderResourceView* HmMakeSolidWhite()
{
    uint32_t px = 0xffffffffu;
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
    td.Width = td.Height = 1; td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof(sd));
    sd.pSysMem = &px; sd.SysMemPitch = 4;
    ID3D11Texture2D* t = NULL; ID3D11ShaderResourceView* v = NULL;
    if (SUCCEEDED(g_dev11->CreateTexture2D(&td, &sd, &t))) {
        g_dev11->CreateShaderResourceView(t, NULL, &v);
        t->Release();
    }
    return v;
}


static int HmLoadSkin(const char* name)
{
    if (!name || !name[0] || !g_dev11) return 0;
    for (int q = 1; q < g_hmSkinN; q++)
        if (!strcmp(g_hmSkinName[q], name)) return q;      // already loaded
    if (g_hmSkinN >= 8) return 0;

    // WIC is COM, and the render thread has never had COM started on it - miss
    // this and every skin load fails silently with a class-not-registered.
    static bool comUp = false;
    if (!comUp) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        comUp = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE || hr == S_FALSE;
        if (!comUp) { Log("vrhands: CoInitializeEx failed (0x%08x)", (unsigned)hr); return 0; }
    }

    static const char* ext[4] = { "png", "jpg", "bmp", "tif" };
    IWICImagingFactory* fac = NULL;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (void**)&fac)) || !fac)
        return 0;
    IWICBitmapDecoder* dec = NULL;
    char gotPath[MAX_PATH] = "";
    for (int e = 0; e < 4 && !dec; e++) {
        char path[MAX_PATH]; WCHAR wp[MAX_PATH];
        _snprintf(path, MAX_PATH, "%s\\vrhands\\%s.%s", g_dir, name, ext[e]);
        MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
        if (FAILED(fac->CreateDecoderFromFilename(wp, NULL, GENERIC_READ,
                   WICDecodeMetadataCacheOnDemand, &dec))) dec = NULL;
        else strncpy(gotPath, path, MAX_PATH - 1);
    }
    if (!dec) { fac->Release(); return 0; }

    IWICBitmapFrameDecode* frm = NULL;
    IWICFormatConverter* cvt = NULL;
    int slot = 0;
    if (SUCCEEDED(dec->GetFrame(0, &frm)) &&
        SUCCEEDED(fac->CreateFormatConverter(&cvt)) &&
        SUCCEEDED(cvt->Initialize(frm, GUID_WICPixelFormat32bppRGBA,
                                  WICBitmapDitherTypeNone, NULL, 0.0,
                                  WICBitmapPaletteTypeCustom))) {
        UINT w = 0, hgt = 0;
        cvt->GetSize(&w, &hgt);
        if (w && hgt && w <= 4096 && hgt <= 4096) {
            BYTE* buf = (BYTE*)malloc((size_t)w * hgt * 4);
            if (buf && SUCCEEDED(cvt->CopyPixels(NULL, w * 4, w * hgt * 4, buf))) {
                D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
                td.Width = w; td.Height = hgt; td.MipLevels = 1; td.ArraySize = 1;
                td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
                td.Usage = D3D11_USAGE_IMMUTABLE;
                td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA sd; memset(&sd, 0, sizeof(sd));
                sd.pSysMem = buf; sd.SysMemPitch = w * 4;
                ID3D11Texture2D* tex = NULL;
                if (SUCCEEDED(g_dev11->CreateTexture2D(&td, &sd, &tex))) {
                    slot = g_hmSkinN;
                    if (SUCCEEDED(g_dev11->CreateShaderResourceView(tex, NULL,
                                                                    &g_hmSkin[slot]))) {
                        strncpy(g_hmSkinName[slot], name, 63);
                        strncpy(g_hmSkinPath[slot], gotPath, MAX_PATH - 1);
                        { WIN32_FILE_ATTRIBUTE_DATA fa;
                          if (GetFileAttributesExA(gotPath, GetFileExInfoStandard, &fa))
                              g_hmSkinTime[slot] = fa.ftLastWriteTime; }
                        g_hmSkinN++;
                        Log("vrhands: skin '%s' loaded (%ux%u)", name, w, hgt);
                    } else slot = 0;
                    tex->Release();
                }
            }
            free(buf);
        }
    }
    if (cvt) cvt->Release();
    if (frm) frm->Release();
    dec->Release(); fac->Release();
    return slot;
}


static bool HmLoadObj(int h, const char* name)
{
    char path[MAX_PATH];
    _snprintf(path, MAX_PATH, "%s\\vrhands\\%s.obj", g_dir, name);
    FILE* f = fopen(path, "r");
    if (!f) return false;

    static float vx[60000], vy[60000], vz[60000];
    static float tu[60000], tv[60000];
    int nv = 0, nt2 = 0, added = 0;
    float col[3] = { 0.62f, 0.63f, 0.66f };
    char line[512];

    // Per-material colours, if a sibling .mtl is there. Model the grip, guard
    // and blade as separate materials in Blender and they come through shaded
    // differently instead of one flat grey lump.
    struct Mtl { char nm[64]; float c[3]; };
    static Mtl mtl[64]; int nmtl = 0;
    {
        char mp[MAX_PATH];
        _snprintf(mp, MAX_PATH, "%s\\vrhands\\%s.mtl", g_dir, name);
        FILE* mf = fopen(mp, "r");
        if (mf) {
            char ml[512];
            while (fgets(ml, sizeof(ml), mf)) {
                if (!strncmp(ml, "newmtl", 6) && nmtl < 64) {
                    nmtl++;
                    sscanf(ml + 6, "%63s", mtl[nmtl-1].nm);
                    mtl[nmtl-1].c[0] = mtl[nmtl-1].c[1] = mtl[nmtl-1].c[2] = 0.65f;
                } else if (!strncmp(ml, "Kd", 2) && nmtl > 0) {
                    sscanf(ml + 2, "%f %f %f", &mtl[nmtl-1].c[0],
                           &mtl[nmtl-1].c[1], &mtl[nmtl-1].c[2]);
                }
            }
            fclose(mf);
            if (nmtl) Log("vrhands: %s.mtl gave %d materials", name, nmtl);
        }
    }
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t')) {
            if (nv < 60000) {
                float a = 0, b = 0, c = 0;
                if (sscanf(line + 2, "%f %f %f", &a, &b, &c) == 3) {
                    // OBJ is Y-up right-handed; the controller frame is too,
                    // so only the unit scale changes here.
                    vx[nv] = a * g_hmObjScale;
                    vy[nv] = b * g_hmObjScale;
                    vz[nv] = c * g_hmObjScale;
                    nv++;
                }
            }
        } else if (line[0] == 'v' && line[1] == 't') {
            if (nt2 < 60000) {
                float a = 0, b = 0;
                if (sscanf(line + 2, "%f %f", &a, &b) == 2) {
                    tu[nt2] = a; tv[nt2] = 1.0f - b;   // OBJ V runs bottom-up
                    nt2++;
                }
            }
        } else if (!strncmp(line, "usemtl", 6)) {
            char want[64] = "";
            sscanf(line + 6, "%63s", want);
            for (int q = 0; q < nmtl; q++)
                if (!strcmp(mtl[q].nm, want)) {
                    col[0] = mtl[q].c[0]; col[1] = mtl[q].c[1]; col[2] = mtl[q].c[2];
                    break;
                }
        } else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t')) {
            int idx[32], uvi[32]; int n = 0;
            const char* c = line + 1;
            while (*c && n < 32) {
                while (*c == ' ' || *c == '\t') c++;
                if (!*c || *c == '\n' || *c == '\r') break;
                int vi = atoi(c);                 // "v", "v/t", "v/t/n" all start with v
                int ti = -1;
                const char* sl = c;
                while (*sl && *sl != ' ' && *sl != '\t' && *sl != '/') sl++;
                if (*sl == '/' && sl[1] != '/') ti = atoi(sl + 1) - 1;
                if (vi < 0) vi = nv + vi; else vi -= 1;
                if (vi >= 0 && vi < nv) {
                    uvi[n] = (ti >= 0 && ti < nt2) ? ti : -1;
                    idx[n++] = vi;
                }
                while (*c && *c != ' ' && *c != '\t') c++;
            }
            for (int q = 1; q + 1 < n; q++) {     // fan-triangulate
                float A[3] = { vx[idx[0]],   vy[idx[0]],   vz[idx[0]] };
                float B[3] = { vx[idx[q]],   vy[idx[q]],   vz[idx[q]] };
                float C[3] = { vx[idx[q+1]], vy[idx[q+1]], vz[idx[q+1]] };
                // flat shade from the face normal so the silhouette reads
                float u[3] = { B[0]-A[0], B[1]-A[1], B[2]-A[2] };
                float w[3] = { C[0]-A[0], C[1]-A[1], C[2]-A[2] };
                float nrm[3]; V3Cross(u, w, nrm); V3Norm(nrm);
                float lit = 0.45f + 0.55f * fabsf(nrm[1] * 0.6f + nrm[2] * 0.4f);
                HmTriAdd(h, A, B, C, col, lit);
                if (g_hmGeoN[h] > 0) {          // attach the UVs we just parsed
                    HmTri* nt3 = &g_hmGeo[h][g_hmGeoN[h] - 1];
                    const int fi[3] = { uvi[0], uvi[q], uvi[q+1] };
                    for (int k = 0; k < 3; k++)
                        if (fi[k] >= 0) { nt3->uv[k][0] = tu[fi[k]]; nt3->uv[k][1] = tv[fi[k]]; }
                }
                added++;
            }
        }
    }
    fclose(f);
    if (!added) { Log("vrhands: %s.obj had no faces", name); return false; }
    {
        int slot = strcmp(name, "hand") ? 0 : 1;
        strncpy(g_hmObjPath[h][slot], path, MAX_PATH - 1);
        WIN32_FILE_ATTRIBUTE_DATA fa;
        if (GetFileAttributesExA(path, GetFileExInfoStandard, &fa))
            g_hmObjTime[h][slot] = fa.ftLastWriteTime;
    }
    Log("vrhands: loaded %s.obj (%d triangles, scale %.4f)", name, added, g_hmObjScale);
    return true;
}


static void HmBuildGeometry()
{
    const float steel[3] = { 0.66f, 0.68f, 0.74f };
    const float brass[3] = { 0.52f, 0.40f, 0.16f };
    const float wood [3] = { 0.28f, 0.18f, 0.10f };
    const float iron [3] = { 0.30f, 0.31f, 0.34f };
    const float blood[3] = { 0.45f, 0.09f, 0.11f };
    for (int h = 0; h < 2; h++) {
        g_hmGeoN[h] = 0;
        int m = g_hmModel[h];
        if (m == HM_NONE) continue;
        static const char* on[HM_COUNT] =
            { "", "sword", "crossbow", "hand", "pistol", "grenade", "razor", "heart" };
        g_hmTexSlot = HmLoadSkin("hand");
        if (!HmLoadObj(h, "hand")) { g_hmTexSlot = 0; HmHand(h, h ? 1.0f : -1.0f); }
        g_hmTexSlot = (m > 0 && m < HM_COUNT) ? HmLoadSkin(on[m]) : 0;
        g_hmObjLoaded[h] = (m > 0 && m < HM_COUNT) ? HmLoadObj(h, on[m]) : false;
        if (!g_hmObjLoaded[h]) g_hmTexSlot = 0;
        if (g_hmObjLoaded[h]) continue;          // extracted mesh wins
        switch (m) {
        case HM_SWORD:
            HmBox(h, 0, 0,  0.045f, 0.016f, 0.016f, 0.055f, 1.0f, wood);
            HmBox(h, 0, 0, -0.020f, 0.075f, 0.010f, 0.012f, 1.0f, brass);
            HmBox(h, 0, 0, -0.400f, 0.018f, 0.006f, 0.370f, 0.15f, steel);
            break;
        case HM_XBOW:
            HmBox(h, 0, 0,        0.045f, 0.020f, 0.030f, 0.055f, 1.0f, wood);
            HmBox(h, 0, 0.015f,  -0.110f, 0.022f, 0.020f, 0.130f, 1.0f, wood);
            HmBox(h, 0, 0.030f,  -0.210f, 0.150f, 0.008f, 0.014f, 1.0f, steel);
            HmBox(h, 0, 0.030f,  -0.130f, 0.008f, 0.010f, 0.075f, 1.0f, brass);
            break;
        case HM_PISTOL:
            HmBox(h, 0, 0,        0.040f, 0.018f, 0.034f, 0.048f, 1.0f, wood);
            HmBox(h, 0, 0.042f,  -0.055f, 0.016f, 0.018f, 0.100f, 1.0f, iron);
            HmBox(h, 0, 0.070f,  -0.120f, 0.030f, 0.014f, 0.030f, 1.0f, brass);
            break;
        case HM_GRENADE:
            HmBox(h, 0, -0.005f, -0.055f, 0.035f, 0.035f, 0.040f, 0.7f, iron);
            HmBox(h, 0,  0.032f, -0.055f, 0.010f, 0.012f, 0.012f, 1.0f, brass);
            break;
        case HM_RAZOR:
            HmBox(h, 0, 0,       -0.050f, 0.055f, 0.014f, 0.055f, 1.0f, iron);
            HmBox(h, 0, 0.020f,  -0.050f, 0.020f, 0.010f, 0.020f, 1.0f, brass);
            break;
        case HM_HEART:
            HmBox(h, 0, 0,       -0.050f, 0.040f, 0.042f, 0.038f, 0.6f, blood);
            HmBox(h, 0, 0.038f,  -0.050f, 0.014f, 0.012f, 0.014f, 1.0f, brass);
            break;
        default: break;                        // HM_HAND: the hand alone
        }
    }
    Log("vrhands: geometry rebuilt (L model %d, %d tris | R model %d, %d tris)",
        g_hmModel[0], g_hmGeoN[0], g_hmModel[1], g_hmGeoN[1]);
}


static bool HmEnsurePipeline()
{
    if (g_hmReady) return true;
    if (!g_dev11) return false;
    EnsureCommonStates();   // 41.0: the shared sampler and depth states
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    if (!compiler) return false;
    PFN_D3DCompile compile = (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile");
    if (!compile) return false;
    ID3DBlob *vsb = NULL, *psb = NULL, *err = NULL;
    if (FAILED(compile(kHandShaderSrc, strlen(kHandShaderSrc), NULL, NULL, NULL,
                       "vsmain", "vs_4_0", 0, 0, &vsb, &err))) {
        Log("vrhands: VS compile failed: %s", err ? (char*)err->GetBufferPointer() : "?");
        return false;
    }
    if (FAILED(compile(kHandShaderSrc, strlen(kHandShaderSrc), NULL, NULL, NULL,
                       "psmain", "ps_4_0", 0, 0, &psb, &err))) {
        Log("vrhands: PS compile failed"); return false;
    }
    if (FAILED(g_dev11->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), NULL, &g_hmVS)) ||
        FAILED(g_dev11->CreatePixelShader (psb->GetBufferPointer(), psb->GetBufferSize(), NULL, &g_hmPS))) {
        Log("vrhands: shader objects failed"); return false;
    }
    D3D11_INPUT_ELEMENT_DESC il[3] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(g_dev11->CreateInputLayout(il, 3, vsb->GetBufferPointer(),
                                          vsb->GetBufferSize(), &g_hmLayout))) {
        Log("vrhands: input layout failed"); return false;
    }
    vsb->Release(); psb->Release();
    D3D11_BUFFER_DESC bd; memset(&bd, 0, sizeof(bd));
    bd.ByteWidth = sizeof(HandVert) * 3 * (HM_MAXTRI * 2);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(g_dev11->CreateBuffer(&bd, NULL, &g_hmVB))) {
        Log("vrhands: vertex buffer failed"); return false;
    }
    HmBuildGeometry();
    g_hmReady = true;
    Log("vrhands: pipeline ready");
    return true;
}

static int HmSortCmp(const void* a, const void* b)
{
    float ka = g_hmSortKey[*(const int*)a], kb = g_hmSortKey[*(const int*)b];
    return (ka < kb) ? -1 : (ka > kb ? 1 : 0);
}


// Returns the number of triangles submitted, or a NEGATIVE reason code. The
// codes exist because every one of these early returns used to be a silent
// `return` and they are not the same fault: "the hands are off", "the head is
// not tracked" and "the runtime has not handed us a frustum yet" all produced
// an empty screen and an empty log. HmWhy() turns a code back into words.
enum { HM_WHY_OFF = -1, HM_WHY_NOHEAD = -2, HM_WHY_NOFRUSTUM = -3,
       HM_WHY_NOMAP = -4,
       // ...and the seam's own refusals, which never reach HmRenderEye at all.
       HM_WHY_NOPROJ = -5, HM_WHY_CTX = -6, HM_WHY_PIPE = -7, HM_WHY_DEPTH = -8 };

static int HmRenderEye(int eye)
{
    if (!g_hmEnable || !g_hmReady) return HM_WHY_OFF;
    if (!g_devPoseOk[0]) return HM_WHY_NOHEAD;
    if (g_hmHotReload && eye == 0) {
        static double next = 0.0;
        double now = MaimNowMs();
        if (now >= next) {
            next = now + 1000.0;
            for (int q = 1; q < g_hmSkinN && !g_hmRebuild; q++) {
                if (!g_hmSkinPath[q][0]) continue;
                WIN32_FILE_ATTRIBUTE_DATA fa;
                if (!GetFileAttributesExA(g_hmSkinPath[q], GetFileExInfoStandard, &fa))
                    continue;
                if (CompareFileTime(&fa.ftLastWriteTime, &g_hmSkinTime[q])) {
                    Log("vrhands: skin %s changed - reloading", g_hmSkinPath[q]);
                    g_hmRebuild = 1;
                }
            }
            for (int hh = 0; hh < 2 && !g_hmRebuild; hh++)
                for (int sl = 0; sl < 2; sl++) {
                    if (!g_hmObjPath[hh][sl][0]) continue;
                    WIN32_FILE_ATTRIBUTE_DATA fa;
                    if (!GetFileAttributesExA(g_hmObjPath[hh][sl],
                                              GetFileExInfoStandard, &fa)) continue;
                    if (CompareFileTime(&fa.ftLastWriteTime, &g_hmObjTime[hh][sl])) {
                        Log("vrhands: %s changed on disk - reloading",
                            g_hmObjPath[hh][sl]);
                        g_hmRebuild = 1;
                        break;
                    }
                }
        }
    }
    if (InterlockedExchange(&g_hmRebuild, 0)) { HmDropSkins(); HmBuildGeometry(); }

    float l, r, t, b;
    float ex = (eye == 0) ? -0.032f : 0.032f, ey = 0, ez = 0;
    if (g_eyeFrOk) {                              // the runtime's per-eye frustum
        l = g_eyeFr[eye][0]; r = g_eyeFr[eye][1];
        t = g_eyeFr[eye][2]; b = g_eyeFr[eye][3];
        ex = g_eyeOffs[eye][0]; ey = g_eyeOffs[eye][1]; ez = g_eyeOffs[eye][2];
    } else return HM_WHY_NOFRUSTUM;
    float idx = 1.0f / (r - l), idy = 1.0f / (b - t);
    float (*hm)[4] = g_devPose[0];

    static HandVert vtx[3 * HM_MAXTRI * 2];
    static float    key[HM_MAXTRI * 2];
    static int      order[HM_MAXTRI * 2];
    int nt = 0;

    for (int h = 0; h < 2; h++) {
        int dev = g_ctrlIdx[h];
        if (dev < 0 || dev >= 16 || !g_devPoseOk[dev] || !g_hmGeoN[h]) continue;
        float (*hc)[4] = g_devPose[dev];
        // per-hand local offset: rotation (yaw, pitch, roll) then translation
        int mi = g_hmModel[h]; if (mi < 0 || mi >= HM_COUNT) mi = 0;
        float rad[3] = { (g_hmRot[h][0] + g_hmMRot[mi][0]) * 0.01745329f,
                         (g_hmRot[h][1] + g_hmMRot[mi][1]) * 0.01745329f,
                         (g_hmRot[h][2] + g_hmMRot[mi][2]) * 0.01745329f };
        float B[9]; HmBuildYPR(rad, B);   // never the legacy stub: see HmBuildYPR
        for (int q = 0; q < g_hmGeoN[h] && nt < HM_MAXTRI * 2; q++) {
            HmTri* tri = &g_hmGeo[h][q];
            float es[3][3]; bool nonFinite = false, nearRej = false; float zsum = 0;
            for (int k = 0; k < 3; k++) {
                const float* lp = tri->p[k];
                float sc[3] = { lp[0]*g_hmScale, lp[1]*g_hmScale, lp[2]*g_hmScale };
                float rl[3];                                   // model rotation
                for (int a = 0; a < 3; a++)
                    rl[a] = B[a*3+0]*sc[0] + B[a*3+1]*sc[1] + B[a*3+2]*sc[2];
                for (int a = 0; a < 3; a++) rl[a] += g_hmPos[h][a] + g_hmMPos[mi][a];
                float room[3];                                 // controller -> room
                for (int a = 0; a < 3; a++)
                    room[a] = hc[a][0]*rl[0] + hc[a][1]*rl[1] + hc[a][2]*rl[2] + hc[a][3];
                float d[3] = { room[0]-hm[0][3], room[1]-hm[1][3], room[2]-hm[2][3] };
                float hs[3];                                   // room -> head
                for (int a = 0; a < 3; a++)
                    hs[a] = hm[0][a]*d[0] + hm[1][a]*d[1] + hm[2][a]*d[2];
                // 38.1: respect the eye-to-head ROTATION here too (cached
                // 3x3 from BuildEyeQuads; identity on Vive/Index).
                float dx = hs[0]-ex, dy = hs[1]-ey, dz = hs[2]-ez;
                if (g_eyeCantCfg) {
                    const float* R = g_eyeRot[eye];
                    es[k][0] = R[0]*dx + R[3]*dy + R[6]*dz;
                    es[k][1] = R[1]*dx + R[4]*dy + R[7]*dz;
                    es[k][2] = R[2]*dx + R[5]*dy + R[8]*dz;
                } else {
                    es[k][0] = dx; es[k][1] = dy; es[k][2] = dz;
                }
                // Finiteness FIRST, and explicitly. A NaN does not trip the
                // near-plane test - `NaN > -0.03f` is false - so before this
                // check a non-finite vertex was submitted like any other and
                // counted as a healthy triangle.
                if (!HmFinite(es[k][0]) || !HmFinite(es[k][1]) || !HmFinite(es[k][2]))
                    nonFinite = true;
                else if (es[k][2] > -kHmNearM) nearRej = true;   // at or behind the eye
                zsum += es[k][2];
            }
            if (nonFinite) { g_hmNonFinite++; continue; }
            if (nearRej)   { g_hmNearRej++;   continue; }
            float ndc[3][2];
            for (int k = 0; k < 3; k++) {
                float x = es[k][0], y = es[k][1], z = es[k][2];
                const float w = -z;          // > kHmNearM, by the test above
                HandVert* v = &vtx[nt*3 + k];
                v->uv[0] = tri->uv[k][0]; v->uv[1] = tri->uv[k][1];
                v->pos[0] = 2*idx*x + (r + l)*idx*z;
                v->pos[1] = 2*idy*y + (b + t)*idy*z;
                // 41.2: a depth that actually varies with distance. This was
                // `0.5f * (-z)`, which divides by w = -z to EXACTLY 0.5 at
                // every distance, so the depth buffer could not order anything:
                // the first fragment to reach a pixel won and every later one
                // failed LESS. The painter's sort was removed on the assumption
                // depth had replaced it, and it had not. Standard D3D mapping,
                // 0 at the near plane and 1 at the far.
                v->pos[2] = (kHmFarM / (kHmFarM - kHmNearM)) * (w - kHmNearM);
                v->pos[3] = w;
                for (int a = 0; a < 4; a++) v->col[a] = tri->c[a];
                ndc[k][0] = v->pos[0] / w;
                ndc[k][1] = v->pos[1] / w;
                if (w < g_hmDistMin) g_hmDistMin = w;
                if (w > g_hmDistMax) g_hmDistMax = w;
            }
            {   // Where did it land, and can it produce a pixel at all?
                const float ax = ndc[1][0] - ndc[0][0], ay = ndc[1][1] - ndc[0][1];
                const float bx = ndc[2][0] - ndc[0][0], by = ndc[2][1] - ndc[0][1];
                if (fabsf(ax * by - ay * bx) * 0.5f < 1e-9f) g_hmDegenerate++;
                float mnx = ndc[0][0], mxx = ndc[0][0];
                float mny = ndc[0][1], mxy = ndc[0][1];
                for (int k = 1; k < 3; k++) {
                    if (ndc[k][0] < mnx) mnx = ndc[k][0];
                    if (ndc[k][0] > mxx) mxx = ndc[k][0];
                    if (ndc[k][1] < mny) mny = ndc[k][1];
                    if (ndc[k][1] > mxy) mxy = ndc[k][1];
                }
                if (mxx >= -1.0f && mnx <= 1.0f && mxy >= -1.0f && mny <= 1.0f)
                    g_hmOnScreen++;
                if (mnx < g_hmNdcMin[0]) g_hmNdcMin[0] = mnx;
                if (mxx > g_hmNdcMax[0]) g_hmNdcMax[0] = mxx;
                if (mny < g_hmNdcMin[1]) g_hmNdcMin[1] = mny;
                if (mxy > g_hmNdcMax[1]) g_hmNdcMax[1] = mxy;
            }
            key[nt] = zsum;
            order[nt] = tri->tex;      // batch key: which skin this triangle uses
            nt++;
        }
    }
    if (!nt) return 0;
    // Depth testing means draw ORDER no longer matters, so group by skin
    // instead of by distance - one draw call per texture, no sorting at all.
    static int runStart[8], runLen[8];
    for (int q = 0; q < 8; q++) { runStart[q] = 0; runLen[q] = 0; }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(g_ctx11->Map(g_hmVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return HM_WHY_NOMAP;
    HandVert* dst = (HandVert*)ms.pData;
    int out = 0;
    for (int sl = 0; sl < 8; sl++) {
        runStart[sl] = out;
        for (int i = 0; i < nt; i++) {
            if (order[i] != sl) continue;
            memcpy(dst + out*3, vtx + i*3, sizeof(HandVert)*3);
            out++;
        }
        runLen[sl] = out - runStart[sl];
    }
    g_ctx11->Unmap(g_hmVB, 0);
    (void)key;

    UINT stride = sizeof(HandVert), off = 0;
    g_ctx11->IASetInputLayout(g_hmLayout);
    g_ctx11->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx11->VSSetShader(g_hmVS, NULL, 0);
    g_ctx11->PSSetShader(g_hmPS, NULL, 0);
    g_ctx11->IASetVertexBuffers(0, 1, &g_hmVB, &stride, &off);
    g_ctx11->PSSetSamplers(0, 1, &g_sampler);
    if (g_dsOn) g_ctx11->OMSetDepthStencilState(g_dsOn, 0);
    if (!g_hmSkin[0]) g_hmSkin[0] = HmMakeSolidWhite();
    for (int sl = 0; sl < 8; sl++) {
        if (!runLen[sl]) continue;
        ID3D11ShaderResourceView* srv = g_hmSkin[sl] ? g_hmSkin[sl] : g_hmSkin[0];
        g_ctx11->PSSetShaderResources(0, 1, &srv);
        g_ctx11->Draw(runLen[sl] * 3, runStart[sl] * 3);
    }
    if (g_dsOff) g_ctx11->OMSetDepthStencilState(g_dsOff, 0);
    return nt;
}


// ---------------------------------------------------------------------------
// 41.2 (VR-31): THE DRAW SEAM.
//
// VR-31 asked whether floating HANDS were reachable. Route (d) proved that
// hiding by material section works and, in the same breath, that it cannot
// give hands: the census read ONE section on Skm_Player, so the arms and the
// hands are the same piece of geometry and no finer cut exists in the asset.
// The answer is therefore a different SOURCE for the hands, and the mod has
// carried one since 30.77 - this file. Hide the game's body, draw our own.
//
// What was missing was not the renderer but its CALLER. HmRenderEye drew into
// the per-eye render targets of the side-by-side pipeline, and that pipeline
// was deleted in 41.0 (cc2fa936). Since then HmEnsurePipeline and HmRenderEye
// have had no call site at all: the whole subsystem compiled, shipped and did
// nothing, and [VRHands] Enabled=1 would have changed nothing on screen.
//
// So the stereo method calls us now, between the game image and the F10 panel.
// Three things the old caller supplied and this one has to rebuild:
//
//   1. A DEPTH BUFFER. The pass batches by skin and depth-tests; the sort it
//      used to do is gone. Without a bound DSV the depth STATE is inert and
//      the back of a hand paints over its front.
//   2. The right EYE. The tag of the pixels in the target, not the eye the
//      next game draw will render.
//   3. A frustum that matches the LAYER'S CLAIM, not the headset's raw
//      half-angles - see DvrPresentPoses, where g_eyeFr is filled.
//
// Ships OFF ([VRHands] Enabled=0) with `vrhands on|off` as the live A/B.
// UNVERIFIED in a headset as of this commit.
// ---------------------------------------------------------------------------

static void HmReleaseDepth()
{
    if (g_hmDSV) { g_hmDSV->Release(); g_hmDSV = NULL; }
    if (g_hmDepthTex) { g_hmDepthTex->Release(); g_hmDepthTex = NULL; }
    g_hmDepthW = g_hmDepthH = 0;
}


static bool HmEnsureDepth(ID3D11Device* dev, uint32_t w, uint32_t h)
{
    if (g_hmDSV && g_hmDepthW == w && g_hmDepthH == h) return true;
    HmReleaseDepth();
    D3D11_TEXTURE2D_DESC td; memset(&td, 0, sizeof(td));
    td.Width = w; td.Height = h;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(dev->CreateTexture2D(&td, NULL, &g_hmDepthTex)) ||
        FAILED(dev->CreateDepthStencilView(g_hmDepthTex, NULL, &g_hmDSV))) {
        Log("vrhands: REFUSED - depth buffer %ux%u would not create. Without "
            "one the depth STATE is inert and the back of a hand paints over "
            "its front, so the pass declines rather than drawing it wrong.",
            w, h);
        HmReleaseDepth();
        return false;
    }
    g_hmDepthW = w; g_hmDepthH = h;
    Log("vrhands: depth buffer %ux%u D32 (%.1f MB), one shared by both eyes and "
        "cleared per draw - a present carries one eye's picture and one eye's "
        "hands, so they never overlap",
        w, h, (double)w * (double)h * 4.0 / 1048576.0);
    return true;
}


static const char* HmWhy(int code)
{
    switch (code) {
    case HM_WHY_OFF:       return "the hands are off or the pipeline is not up";
    case HM_WHY_NOHEAD:    return "no head pose this present";
    case HM_WHY_NOFRUSTUM: return "no per-eye frustum yet (the runtime has not located the views)";
    case HM_WHY_NOMAP:     return "the vertex buffer would not map";
    case HM_WHY_NOPROJ:    return "the active method is not a projection layer";
    case HM_WHY_CTX:       return "the seam's context is not g_ctx11";
    case HM_WHY_PIPE:      return "the pipeline never came up";
    case HM_WHY_DEPTH:     return "no depth buffer";
    default:               return "drew";
    }
}


// Every 3 s while the hands are on. It must be able to print the UNWELCOME
// answer, so it names the reason for a zero instead of leaving one.
static void HmBeat()
{
    if (!g_hmEnable) return;        // silence while the lever is off
    const double now = MaimNowMs();
    if (now < g_hmNextBeat) return;
    const bool first = (g_hmNextBeat == 0.0);
    g_hmNextBeat = now + 3000.0;
    if (first) return;              // the first window is a fragment
    const bool anyNdc = g_hmNdcMax[0] >= g_hmNdcMin[0];
    Log("vrhands: beat calls=%u draws=%u last=%s | tris submitted=%u ON-SCREEN=%u "
        "| dropped: nonFinite=%u nearPlane=%u | degenerate=%u | NDC x[%.2f %.2f] "
        "y[%.2f %.2f] (the viewport is [-1 1] on both) | eye distance %.2f..%.2f m",
        g_hmCalls, g_hmDraws, HmWhy(g_hmLastWhy), g_hmTris, g_hmOnScreen,
        g_hmNonFinite, g_hmNearRej, g_hmDegenerate,
        anyNdc ? g_hmNdcMin[0] : 0.0f, anyNdc ? g_hmNdcMax[0] : 0.0f,
        anyNdc ? g_hmNdcMin[1] : 0.0f, anyNdc ? g_hmNdcMax[1] : 0.0f,
        g_hmDistMax >= g_hmDistMin ? g_hmDistMin : 0.0f,
        g_hmDistMax >= g_hmDistMin ? g_hmDistMax : 0.0f);
    // How to read it, spelled out, because the first version of this line
    // reported a healthy `tris` for geometry that could never draw a pixel:
    //   calls 0                     the stereo method never reached the seam
    //   draws 0, calls > 0          the pass refused - the reason is `last`
    //   nonFinite > 0               the transform is producing NaN/inf
    //   submitted > 0, on-screen 0  the geometry is off the viewport, and the
    //                               NDC box says which way and by how much
    //   degenerate ~= submitted     the triangles collapsed to no area
    //   on-screen > 0, still blank  the draw path, not the geometry: turn on
    //                               [VRHands] CalibTriangle
    if (g_hmDraws && !g_hmOnScreen && g_hmTris)
        Log("vrhands:   ^ SUBMITTED BUT NOTHING ON SCREEN - %u triangle(s) went "
            "to the GPU and not one had bounds inside the viewport. This is a "
            "GEOMETRY fault, not a draw-path fault; the NDC box above says "
            "which axis and by how much.", g_hmTris);
    if (g_hmNonFinite)
        Log("vrhands:   ^ %u NON-FINITE triangle(s) - the transform is producing "
            "NaN or inf. Nothing downstream can fix that.", g_hmNonFinite);
    g_hmCalls = g_hmDraws = g_hmTris = 0;
    g_hmNonFinite = g_hmNearRej = g_hmDegenerate = g_hmOnScreen = 0;
    g_hmNdcMin[0] = g_hmNdcMin[1] = 1e30f;
    g_hmNdcMax[0] = g_hmNdcMax[1] = -1e30f;
    g_hmDistMin = 1e30f; g_hmDistMax = -1e30f;
}


// 41.2 (VR-31): the calibration triangle - the FALLBACK instrument.
//
// One triangle at fixed NDC coordinates, through the same shader, the same
// vertex buffer, the same states and the same target as the hands. It cannot
// be wrong about geometry because it has none to get wrong: if this appears
// and the hands do not, the draw path is sound and the fault is in the
// transform. If this does not appear either, the fault is the target or the
// submission and the hunt moves there.
//
// It is a FALLBACK, not the first move, and the outcomes are not fully
// exclusive: a visible calibration triangle proves ITS path works, and hands
// could still fail on degenerate geometry or a texture slot. The beat's
// counters discriminate those; this only separates path from geometry.
// Default OFF ([VRHands] CalibTriangle).
static void HmDrawCalibration(ID3D11DeviceContext* ctx)
{
    HandVert v[3];
    memset(v, 0, sizeof(v));
    // NDC directly: w = 1, so pos IS the clip position. Mid-depth so it neither
    // hides behind nor punches through anything the hands draw.
    static const float pts[3][2] = { { -0.5f, -0.5f }, { 0.0f, 0.5f }, { 0.5f, -0.5f } };
    for (int k = 0; k < 3; k++) {
        v[k].pos[0] = pts[k][0]; v[k].pos[1] = pts[k][1];
        v[k].pos[2] = 0.5f;      v[k].pos[3] = 1.0f;
        v[k].col[0] = 1.0f; v[k].col[1] = 0.0f; v[k].col[2] = 1.0f; v[k].col[3] = 1.0f;
        v[k].uv[0] = 0.5f;  v[k].uv[1] = 0.5f;
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(ctx->Map(g_hmVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    memcpy(ms.pData, v, sizeof(v));
    ctx->Unmap(g_hmVB, 0);
    UINT stride = sizeof(HandVert), off = 0;
    ctx->IASetInputLayout(g_hmLayout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_hmVS, NULL, 0);
    ctx->PSSetShader(g_hmPS, NULL, 0);
    ctx->IASetVertexBuffers(0, 1, &g_hmVB, &stride, &off);
    ctx->PSSetSamplers(0, 1, &g_sampler);
    if (!g_hmSkin[0]) g_hmSkin[0] = HmMakeSolidWhite();
    ctx->PSSetShaderResources(0, 1, &g_hmSkin[0]);
    if (g_dsOn) ctx->OMSetDepthStencilState(g_dsOn, 0);
    ctx->Draw(3, 0);
    static bool said = false;
    if (!said) {
        said = true;
        Log("vrhands: CALIBRATION TRIANGLE armed - a magenta triangle in the "
            "middle of each eye, at fixed NDC, through the hands' own shader, "
            "buffer, states and target. Visible + no hands = the draw path is "
            "fine and the geometry is wrong. Neither visible = the target or "
            "the submission. `[VRHands] CalibTriangle=0` turns it off.");
    }
}


// The stereo seam's entry point. Present thread, target already carrying the
// game image; we hand it back bound to `rtv` with NO depth, which is exactly
// what the F10 overlay draws into next.
static void HmDrawIntoEye(ID3D11Device* dev, ID3D11DeviceContext* ctx,
                          ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h,
                          int eyeSign)
{
    if (!g_hmEnable) return;                 // silent: the lever is OFF by default
    g_hmCalls++;   // the beat itself ticks from the present tick, not here
    if (!dev || !ctx || !rtv || !w || !h) return;

    // The quad screen is a PICTURE at [Screen] DistanceMeters, not a world.
    // Hands composed through an eye frustum would sit at a depth the screen
    // does not have, and would be wrong by exactly that disagreement. Refuse,
    // and say which rung would work - a silent skip reads as broken hands.
    if (!dvr::stereo::wants_projection()) {
        static bool said = false;
        if (!said) {
            said = true;
            Log("vrhands: NOT DRAWN on method '%s' - it shows the game on a "
                "head-locked quad, and a quad is a picture, not a world. This "
                "is the wrong rung, not a fault. `stereo reentry` puts a "
                "projection layer up and the hands draw.",
                dvr::stereo::active_name());
        }
        g_hmLastWhy = HM_WHY_NOPROJ;
        return;
    }
    // Never take the game's context for granted: HmRenderEye writes through
    // g_ctx11, so a target on a different context would silently draw nowhere.
    if (ctx != g_ctx11) {
        static bool said = false;
        if (!said) {
            said = true;
            Log("vrhands: REFUSED - the seam handed us a context that is not "
                "g_ctx11; the pass writes through g_ctx11 and would have drawn "
                "into nothing");
        }
        g_hmLastWhy = HM_WHY_CTX;
        return;
    }
    if (!HmEnsurePipeline()) {
        static bool said = false;
        if (!said) { said = true; Log("vrhands: pipeline would not come up - no hands this session"); }
        g_hmLastWhy = HM_WHY_PIPE;
        return;
    }
    if (!HmEnsureDepth(dev, w, h)) { g_hmLastWhy = HM_WHY_DEPTH; return; }

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    ctx->RSSetViewports(1, &vp);
    ctx->ClearDepthStencilView(g_hmDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
    ctx->OMSetRenderTargets(1, &rtv, g_hmDSV);
    if (g_hmCalib) HmDrawCalibration(ctx);
    // eyeSign is -1 left, +1 right, 0 mono. HmRenderEye indexes 0/1; a mono
    // present has no eye of its own and gets the left frustum.
    const int why = HmRenderEye(eyeSign > 0 ? 1 : 0);
    g_hmDraws++;
    g_hmLastWhy = why;
    if (why > 0) g_hmTris += (uint32_t)why;
    // Hand the target back the way the overlay expects it: colour only, no
    // depth. Leaving the DSV bound would depth-test the F10 panel against the
    // hands, and the panel is 2D.
    ctx->OMSetRenderTargets(1, &rtv, NULL);
}


// Pick each hand's model from the equipped weapon. Runs on the GAME thread,
// where the candidate list is valid; the rebuild itself is deferred to the
// render thread so geometry is never swapped mid-draw.
static void HmPickModels()
{
    if (!g_hmEnable || !g_hmAuto) return;
    int want[2] = { 0, 0 };
    for (int i = 0; i < g_fpCandN; i++) {
        if (!FpIsViewModel(&g_fpCand[i])) continue;
        const char* a = g_fpCand[i].asset;
        if (!a || !a[0]) continue;
        int m = 0, hand = 0;
        if (strstr(a, "Sword") || strstr(a, "sword") ||
            strstr(a, "Blade") || strstr(a, "blade"))          { m = HM_SWORD;   hand = 1; }
        else if (strstr(a, "crossbow") || strstr(a, "Crossbow")) m = HM_XBOW;
        else if (strstr(a, "istol") || strstr(a, "Gun") ||
                 strstr(a, "gun"))                              m = HM_PISTOL;
        else if (strstr(a, "renade"))                           m = HM_GRENADE;
        else if (strstr(a, "azor") || strstr(a, "Mine"))        m = HM_RAZOR;
        else if (strstr(a, "eart"))                             m = HM_HEART;
        if (m) want[hand] = m;
    }
    for (int h = 0; h < 2; h++) if (!want[h]) want[h] = HM_HAND;  // powers = bare hand
    if (want[0] != g_hmModel[0] || want[1] != g_hmModel[1]) {
        g_hmModel[0] = want[0]; g_hmModel[1] = want[1];
        g_hmRebuild = 1;
        Log("vrhands: equipped models now L=%d R=%d", want[0], want[1]);
    }
}
