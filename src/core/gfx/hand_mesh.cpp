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


static void HmRenderEye(int eye)
{
    if (!g_hmEnable || !g_hmReady || !g_sys) return;
    if (!g_devPoseOk[0]) return;
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
    if (g_sys) {
        g_sys->GetProjectionRaw((EVREye)eye, &l, &r, &t, &b);
        HmdMatrix34_t e2h = g_sys->GetEyeToHeadTransform((EVREye)eye);
        if (fabsf(e2h.m[0][0] - 1.0f) < 0.2f && fabsf(e2h.m[0][3]) < 0.2f)
            { ex = e2h.m[0][3]; ey = e2h.m[1][3]; ez = e2h.m[2][3]; }
    } else if (g_eyeFrOk) {                       // 37.3: XR-maintained cache
        l = g_eyeFr[eye][0]; r = g_eyeFr[eye][1];
        t = g_eyeFr[eye][2]; b = g_eyeFr[eye][3];
        ex = g_eyeOffs[eye][0]; ey = g_eyeOffs[eye][1]; ez = g_eyeOffs[eye][2];
    } else return;
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
        float B[9]; RtdBuildYPR(rad, B);
        for (int q = 0; q < g_hmGeoN[h] && nt < HM_MAXTRI * 2; q++) {
            HmTri* tri = &g_hmGeo[h][q];
            float es[3][3]; bool bad = false; float zsum = 0;
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
                if (es[k][2] > -0.03f) bad = true;             // at or behind the eye
                zsum += es[k][2];
            }
            if (bad) continue;
            for (int k = 0; k < 3; k++) {
                float x = es[k][0], y = es[k][1], z = es[k][2];
                HandVert* v = &vtx[nt*3 + k];
                v->uv[0] = tri->uv[k][0]; v->uv[1] = tri->uv[k][1];
                v->pos[0] = 2*idx*x + (r + l)*idx*z;
                v->pos[1] = 2*idy*y + (b + t)*idy*z;
                v->pos[2] = 0.5f * (-z);
                v->pos[3] = -z;
                for (int a = 0; a < 4; a++) v->col[a] = tri->c[a];
            }
            key[nt] = zsum;
            order[nt] = tri->tex;      // batch key: which skin this triangle uses
            nt++;
        }
    }
    if (!nt) return;
    // Depth testing means draw ORDER no longer matters, so group by skin
    // instead of by distance - one draw call per texture, no sorting at all.
    static int runStart[8], runLen[8];
    for (int q = 0; q < 8; q++) { runStart[q] = 0; runLen[q] = 0; }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(g_ctx11->Map(g_hmVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
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
