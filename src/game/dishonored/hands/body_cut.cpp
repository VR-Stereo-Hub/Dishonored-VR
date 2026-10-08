// game/dishonored/hands/body_cut.cpp - the CorvoBody body drawn WITHOUT its arms, at draw time.
// Included by src/mod/dishonoredvr.cpp after corvobody.cpp. docs/dishonored/CORVOBODY.md 7b.
//
// Bone hiding is a no-op in this engine build (no BoneVisibilityStates array exists on the
// component, measured 2026-10-07), so the body's arms come off the way the 1P arms are cut at
// the wrist (mesh_split.cpp): the draw is recognised, its buffers are read once, the triangles
// are classified, and OUR index buffer is drawn in its place. The classifier is geometric and
// name-driven, with no shipped mesh data: a vertex is arm geometry when it lies within
// ArmCutRadiusUu of the reference-pose polyline upper_arm -> lower_arm -> hand -> fingers of
// either arm, and farther than ArmCutStartUu from the upper-arm joint along the arm (so the
// deltoid stays and our IK upper arm emerges from it). The polylines are composed from the
// engine's own RefSkeleton; the quaternion convention is not assumed, the four candidate
// conventions are composed and the one the vertex buffer agrees with (the most vertices
// within the radius) is used. Measured on the PSK (tools/psk_arm_census.py): every arm vertex
// lies within 11.2 uu of its chain, and the only torso vertices within 12 uu sit on the first
// 11 uu of the upper arm.
//
// RENDER LANE. The draw is recognised by the vertex shader's LocalToWorld constant (register
// from the shader's own constant table, as palette_capture does) against the body component's
// LocalToWorld; later passes of the same mesh (depth, shadow) match the contract by their
// buffers alone. The body pointer and bone indices come from the script lane (corvobody.cpp).
// Fail soft: anything that does not read as expected draws the game's own call and says why.

struct BcContract {
    IDirect3DVertexBuffer9* vb; UINT stride, off; INT baseVertex; UINT minIndex, numVertices, startIndex, primCount;
    IDirect3DIndexBuffer9* ib; UINT ourPrims; D3DFORMAT fmt;
};
static BcContract g_bc[4];

static void BcWhy(const char* why)
{
    if (strncmp(g_bcWhy, why, sizeof(g_bcWhy) - 1) == 0) return;
    strncpy(g_bcWhy, why, sizeof(g_bcWhy) - 1); g_bcWhy[sizeof(g_bcWhy) - 1] = 0;
    Log("bodycut: %s", why);
}

static void BodyCutReset(const char* why)
{
    for (int i = 0; i < g_bcN; i++) { if (g_bc[i].ib) g_bc[i].ib->Release(); g_bc[i].ib = NULL; }
    if (g_bcN) Log("bodycut: %d contract(s) dropped (%s)", g_bcN, why);
    g_bcN = 0; g_bcChainOk = false; g_bcBodyFor = NULL; g_bcRefused = false; g_bcIdentTries = 0;
}

static inline float BcDistSeg(const float* p, const float* a, const float* b, float* tOut)
{
    float ab[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] }, ap[3] = { p[0]-a[0], p[1]-a[1], p[2]-a[2] };
    float l2 = ab[0]*ab[0] + ab[1]*ab[1] + ab[2]*ab[2];
    float t = l2 > 1e-6f ? (ap[0]*ab[0] + ap[1]*ab[1] + ap[2]*ab[2]) / l2 : 0.0f;
    if (t < 0) t = 0; if (t > 1) t = 1;
    float c[3] = { a[0]+ab[0]*t - p[0], a[1]+ab[1]*t - p[1], a[2]+ab[2]*t - p[2] };
    if (tOut) *tOut = t;
    return sqrtf(c[0]*c[0] + c[1]*c[1] + c[2]*c[2]);
}

// Arm geometry? The nearest segment of either chain within the radius, excluding the first
// ArmCutStartUu of the upper arm.
static bool BcIsArm(const float* p, const float chain[2][4][3])
{
    for (int arm = 0; arm < 2; arm++) {
        for (int s = 0; s < 3; s++) {
            float t = 0.0f, d = BcDistSeg(p, chain[arm][s], chain[arm][s+1], &t);
            if (d > g_bcRadius) continue;
            if (s == 0) {
                const float* a = chain[arm][0]; const float* b = chain[arm][1];
                float len = sqrtf((b[0]-a[0])*(b[0]-a[0]) + (b[1]-a[1])*(b[1]-a[1]) + (b[2]-a[2])*(b[2]-a[2]));
                if (t * len < g_bcStartUu) continue;
            }
            return true;
        }
    }
    return false;
}

// Above the base of the neck (the collar ring, the standing collar): dropped with NeckCut=1.
// CorvoBody's own HideTorso hid bones with no vertices on them, so the ring was always there.
static bool BcIsNeck(const float* p)
{
    if (!g_bcNeckCut) return false;
    float d[3] = { p[0]-g_bcNeck[0], p[1]-g_bcNeck[1], p[2]-g_bcNeck[2] };
    return d[0]*g_bcNeckUp[0] + d[1]*g_bcNeckUp[1] + d[2]*g_bcNeckUp[2] > g_bcNeckUu - 2.0f;
}

static void BcQMul(const float* a, const float* b, float* o)
{   // (x,y,z,w)
    o[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
    o[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
    o[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
    o[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
}
static void BcQRot(const float* q, const float* v, float* o)
{
    float t[3] = { 2*(q[1]*v[2] - q[2]*v[1]), 2*(q[2]*v[0] - q[0]*v[2]), 2*(q[0]*v[1] - q[1]*v[0]) };
    o[0] = v[0] + q[3]*t[0] + (q[1]*t[2] - q[2]*t[1]);
    o[1] = v[1] + q[3]*t[1] + (q[2]*t[0] - q[0]*t[2]);
    o[2] = v[2] + q[3]*t[2] + (q[0]*t[1] - q[1]*t[0]);
}

// The reference pose from USkeletalMesh::RefSkeleton (+0xEC, TArray of FMeshBone). The element
// is read as UE3's 68-byte FMeshBone (Name 0, Flags 8, Quat 12, Pos 28, sizes, NumChildren 56,
// ParentIndex 60, Color 64) and REFUSED unless the names at the indices MatchRefBone gave and the
// parent chain (hand -> lower arm -> upper arm -> shoulder) read as expected. `variant` picks the
// quaternion convention: bit 0 conjugates the root, bit 1 conjugates every child.
static bool BcRefChain(uint8_t* body, int variant, float out[2][4][3], float* neckOut, float* neckUpOut, bool* neckOk, char* why, size_t whyN)
{
    *neckOk = false;
    if (!RangeReadable(body + g_cbOffSkelMesh, 4)) { _snprintf(why, whyN, "body unreadable"); return false; }
    uint8_t* mesh = *(uint8_t**)(body + g_cbOffSkelMesh);
    if (!mesh || !RangeReadable(mesh + 0xEC, 12)) { _snprintf(why, whyN, "SkeletalMesh unreadable"); return false; }
    uint8_t* data = *(uint8_t**)(mesh + 0xEC);
    int32_t num = *(int32_t*)(mesh + 0xEC + 4);
    const int stride = 68;
    if (!data || num < 8 || num > 512 || !RangeReadable(data, (size_t)num * stride)) { _snprintf(why, whyN, "RefSkeleton array unreadable (num %d)", num); return false; }
    // names and parents at the known indices
    const int iHandL = g_cbBoneIdx[0], iHandR = g_cbBoneIdx[1], iShL = g_cbBoneIdx[2], iShR = g_cbBoneIdx[3];
    const int iUpL = g_cbBoneIdx[4], iUpR = g_cbBoneIdx[5], iLoL = g_cbBoneIdx[6], iLoR = g_cbBoneIdx[7];
    int need[8] = { iHandL, iHandR, iShL, iShR, iUpL, iUpR, iLoL, iLoR };
    for (int i = 0; i < 8; i++) {
        if (need[i] < 0 || need[i] >= num) { _snprintf(why, whyN, "bone index %d out of range (num %d)", need[i], num); return false; }
        if (*(uint32_t*)(data + need[i] * stride) != g_cbNameIdx[i]) {
            _snprintf(why, whyN, "RefSkeleton[%d] is not %s: the 68-byte FMeshBone layout does not fit this build", need[i], RealName(g_cbNameIdx[i]));
            return false;
        }
    }
    auto parentOf = [&](int b) { return *(int32_t*)(data + b * stride + 60); };
    if (parentOf(iHandL) != iLoL || parentOf(iLoL) != iUpL || parentOf(iUpL) != iShL ||
        parentOf(iHandR) != iLoR || parentOf(iLoR) != iUpR || parentOf(iUpR) != iShR) {
        _snprintf(why, whyN, "parent chain mismatch: hand_L %d lower_L %d upper_L %d (expected %d %d %d)", parentOf(iHandL), parentOf(iLoL), parentOf(iUpL), iLoL, iUpL, iShL);
        return false;
    }
    // compose: parents precede children in a UE3 ref skeleton
    static float wq[512][4], wp[512][3];
    for (int b = 0; b < num; b++) {
        const uint8_t* e = data + b * stride;
        float q[4]; memcpy(q, e + 12, 16); float p[3]; memcpy(p, e + 28, 12);
        int par = *(int32_t*)(e + 60);
        if (b == 0 || par < 0 || par >= b) {
            if (variant & 1) { q[0] = -q[0]; q[1] = -q[1]; q[2] = -q[2]; }
            memcpy(wq[b], q, 16); memcpy(wp[b], p, 12);
        } else {
            if (variant & 2) { q[0] = -q[0]; q[1] = -q[1]; q[2] = -q[2]; }
            float r[3]; BcQRot(wq[par], p, r);
            wp[b][0] = wp[par][0] + r[0]; wp[b][1] = wp[par][1] + r[1]; wp[b][2] = wp[par][2] + r[2];
            BcQMul(wq[par], q, wq[b]);
        }
    }
    // the neck joint and the spine direction for the neck cut (indices 8, 9 may be -1: then no neck cut)
    if (g_cbBoneIdx[8] >= 0 && g_cbBoneIdx[8] < num && g_cbBoneIdx[9] >= 0 && g_cbBoneIdx[9] < num) {
        memcpy(neckOut, wp[g_cbBoneIdx[8]], 12);
        float up[3] = { wp[g_cbBoneIdx[8]][0]-wp[g_cbBoneIdx[9]][0], wp[g_cbBoneIdx[8]][1]-wp[g_cbBoneIdx[9]][1], wp[g_cbBoneIdx[8]][2]-wp[g_cbBoneIdx[9]][2] };
        float L = sqrtf(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]); if (L < 1e-3f) L = 1.0f;
        neckUpOut[0] = up[0]/L; neckUpOut[1] = up[1]/L; neckUpOut[2] = up[2]/L; *neckOk = true;
    } else *neckOk = false;
    int chain[2][3] = { { iUpL, iLoL, iHandL }, { iUpR, iLoR, iHandR } };
    for (int arm = 0; arm < 2; arm++) {
        for (int j = 0; j < 3; j++) memcpy(out[arm][j], wp[chain[arm][j]], 12);
        float d[3] = { out[arm][2][0]-out[arm][1][0], out[arm][2][1]-out[arm][1][1], out[arm][2][2]-out[arm][1][2] };
        float L = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]); if (L < 1e-3f) L = 1.0f;
        for (int i = 0; i < 3; i++) out[arm][3][i] = out[arm][2][i] + d[i] / L * 20.0f;   // the fingers
    }
    return true;
}

// Where POSITION sits in the stream-0 vertex. FLOAT3 only; anything else is refused by name.
static bool BcPositionOff(IDirect3DVertexDeclaration9* d, UINT* off)
{
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH]; UINT n = 0;
    if (!d || FAILED(d->GetDeclaration(el, &n))) return false;
    if (n > MAXD3DDECLLENGTH) n = MAXD3DDECLLENGTH;
    bool blend = false;
    for (UINT i = 0; i < n; i++) {
        if (el[i].Type == D3DDECLTYPE_UNUSED || el[i].Stream != 0) continue;
        if (el[i].Usage == D3DDECLUSAGE_BLENDINDICES) blend = true;
        if (el[i].Usage == D3DDECLUSAGE_POSITION && el[i].UsageIndex == 0) {
            if (el[i].Type != D3DDECLTYPE_FLOAT3) return false;
            *off = el[i].Offset;
        }
    }
    return blend;   // a skinned stream
}

static bool BcBuild(IDirect3DDevice9* dev, INT baseVertex, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount)
{
    if (numVertices < 3 || primCount < 1 || numVertices > 65536 || primCount > 200000) { BcWhy("refused: draw size out of range"); return false; }
    bool ok = false;
    IDirect3DIndexBuffer9* ib = NULL; IDirect3DVertexBuffer9* vb = NULL; IDirect3DVertexDeclaration9* decl = NULL;
    UINT streamOff = 0, stride = 0;
    uint32_t* idx = NULL; uint8_t* armv = NULL; void* ourIdx = NULL;
    do {
        if (FAILED(dev->GetIndices(&ib)) || !ib) { BcWhy("refused: no index buffer bound"); break; }
        if (FAILED(dev->GetStreamSource(0, &vb, &streamOff, &stride)) || !vb || !stride) { BcWhy("refused: no stream 0"); break; }
        if (FAILED(dev->GetVertexDeclaration(&decl)) || !decl) { BcWhy("refused: no declaration"); break; }
        D3DINDEXBUFFER_DESC id; memset(&id, 0, sizeof(id)); D3DVERTEXBUFFER_DESC vd; memset(&vd, 0, sizeof(vd));
        if (FAILED(ib->GetDesc(&id)) || FAILED(vb->GetDesc(&vd))) { BcWhy("refused: buffer desc unreadable"); break; }
        if (id.Format != D3DFMT_INDEX16 && id.Format != D3DFMT_INDEX32) { BcWhy("refused: index format"); break; }
        UINT posOff = 0xffffffffu;
        if (!BcPositionOff(decl, &posOff) || posOff == 0xffffffffu || posOff + 12 > stride) { BcWhy("refused: not a skinned FLOAT3-position stream"); break; }
        const UINT ibStride = (id.Format == D3DFMT_INDEX16) ? 2u : 4u;
        const UINT ibOff = startIndex * ibStride, ibLen = primCount * 3u * ibStride;
        const UINT vbOff = streamOff + (UINT)((INT)minIndex + baseVertex) * stride, vbLen = numVertices * stride;
        if (ibOff + ibLen > id.Size || vbOff + vbLen > vd.Size) { BcWhy("refused: draw range past the buffers"); break; }
        // the chain: compose the four conventions, the vertex buffer picks
        float chains[4][2][4][3]; bool chainOk[4]; char why[160] = "";
        float necks[4][3], neckUps[4][3]; bool neckOks[4];
        int anyOk = 0;
        for (int v = 0; v < 4; v++) { chainOk[v] = BcRefChain(g_cbBody, v, chains[v], necks[v], neckUps[v], &neckOks[v], why, sizeof(why)); anyOk += chainOk[v]; }
        if (!anyOk) { char w2[200]; _snprintf(w2, sizeof(w2), "refused: RefSkeleton - %s", why); w2[sizeof(w2)-1] = 0; BcWhy(w2); break; }
        void* p = NULL;
        const DWORD vbFlag = (vd.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY;
        const DWORD ibFlag = (id.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY;
        if (FAILED(vb->Lock(vbOff, vbLen, &p, vbFlag)) || !p) { BcWhy("refused: the vertex buffer would not lock"); break; }
        armv = (uint8_t*)calloc(numVertices, 1);
        int counts[4] = { 0, 0, 0, 0 };
        for (int v = 0; v < 4; v++) {
            if (!chainOk[v]) continue;
            for (UINT i = 0; i < numVertices; i++) {
                const float* pos = (const float*)((const uint8_t*)p + (size_t)i * stride + posOff);
                if (BcIsArm(pos, chains[v])) counts[v]++;
            }
        }
        int best = -1;
        for (int v = 0; v < 4; v++) if (chainOk[v] && (best < 0 || counts[v] > counts[best])) best = v;
        Log("bodycut: quaternion conventions (root conj, child conj) -> arm vertices within %.0f uu: none %d, root %d, child %d, both %d; "
            "chosen %d; left chain joints (%.1f %.1f %.1f) (%.1f %.1f %.1f) (%.1f %.1f %.1f)",
            g_bcRadius, counts[0], counts[1], counts[2], counts[3], best,
            chains[best][0][0][0], chains[best][0][0][1], chains[best][0][0][2], chains[best][0][1][0], chains[best][0][1][1], chains[best][0][1][2],
            chains[best][0][2][0], chains[best][0][2][1], chains[best][0][2][2]);
        if (counts[best] < 200) { vb->Unlock(); BcWhy("refused: no convention places the arm chains in the vertex buffer (fewer than 200 arm vertices)"); break; }
        if (neckOks[best]) { memcpy(g_bcNeck, necks[best], 12); memcpy(g_bcNeckUp, neckUps[best], 12); }
        const bool neck = g_bcNeckCut && neckOks[best];
        int neckN = 0;
        for (UINT i = 0; i < numVertices; i++) {
            const float* pos = (const float*)((const uint8_t*)p + (size_t)i * stride + posOff);
            armv[i] = BcIsArm(pos, chains[best]) ? 1 : 0;
            if (neck && !armv[i] && BcIsNeck(pos)) { armv[i] = 2; neckN++; }
        }
        Log("bodycut: neck cut %s: %d vertices above the neck base (neck joint %.1f %.1f %.1f, up %.2f %.2f %.2f, offset %.0f)",
            neck ? "on" : "off", neckN, g_bcNeck[0], g_bcNeck[1], g_bcNeck[2], g_bcNeckUp[0], g_bcNeckUp[1], g_bcNeckUp[2], g_bcNeckUu);
        vb->Unlock(); p = NULL;
        memcpy(g_bcChain, chains[best], sizeof(g_bcChain)); g_bcChainOk = true; g_bcChainVariant = best; g_bcArmVerts = counts[best];
        if (FAILED(ib->Lock(ibOff, ibLen, &p, ibFlag)) || !p) { BcWhy("refused: the index buffer would not lock"); break; }
        idx = (uint32_t*)malloc((size_t)primCount * 3 * 4);
        if (ibStride == 2) { const uint16_t* s = (const uint16_t*)p; for (UINT i = 0; i < primCount * 3; i++) idx[i] = s[i]; }
        else memcpy(idx, p, (size_t)primCount * 3 * 4);
        ib->Unlock(); p = NULL;
        // classify and emit
        ourIdx = malloc((size_t)primCount * 3 * ibStride);
        UINT kept = 0, dropped = 0, bad = 0;
        for (UINT t = 0; t < primCount; t++) {
            int arm = 0; bool oob = false;
            for (int k = 0; k < 3; k++) {
                uint32_t vi = idx[t*3 + k];
                if (vi < minIndex || vi - minIndex >= numVertices) { oob = true; break; }
                arm += armv[vi - minIndex] ? 1 : 0;
            }
            if (oob) { bad++; }
            if (!oob && arm >= g_bcMinArm) { dropped++; continue; }
            if (ibStride == 2) { uint16_t* o = (uint16_t*)ourIdx; o[kept*3] = (uint16_t)idx[t*3]; o[kept*3+1] = (uint16_t)idx[t*3+1]; o[kept*3+2] = (uint16_t)idx[t*3+2]; }
            else { uint32_t* o = (uint32_t*)ourIdx; o[kept*3] = idx[t*3]; o[kept*3+1] = idx[t*3+1]; o[kept*3+2] = idx[t*3+2]; }
            kept++;
        }
        if (dropped == 0) { BcWhy("refused: the classifier dropped no triangle (the chains do not meet the geometry)"); break; }
        IDirect3DIndexBuffer9* our = NULL;
        if (FAILED(dev->CreateIndexBuffer(kept * 3 * ibStride, D3DUSAGE_WRITEONLY, id.Format, D3DPOOL_MANAGED, &our, NULL)) || !our) { BcWhy("refused: could not create the index buffer"); break; }
        if (FAILED(our->Lock(0, 0, &p, 0)) || !p) { our->Release(); BcWhy("refused: our index buffer would not lock"); break; }
        memcpy(p, ourIdx, (size_t)kept * 3 * ibStride); our->Unlock();
        BcContract& c = g_bc[g_bcN++];
        c.vb = vb; c.stride = stride; c.off = streamOff; c.baseVertex = baseVertex; c.minIndex = minIndex; c.numVertices = numVertices;
        c.startIndex = startIndex; c.primCount = primCount; c.ib = our; c.ourPrims = kept; c.fmt = id.Format;
        g_bcKept = (int)kept; g_bcDropped = (int)dropped;
        Log("bodycut: contract %d built on %p: %u verts / %u tris (vb %p stride %u off %u base %d min %u start %u), %d arm vertices, "
            "%u triangles dropped, %u kept, %u with out-of-window indices kept as they were. Radius %.0f uu, start %.0f uu, %d arm verts per triangle.",
            g_bcN, (void*)g_cbBody, numVertices, primCount, (void*)vb, stride, streamOff, baseVertex, minIndex, startIndex,
            g_bcArmVerts, dropped, kept, bad, g_bcRadius, g_bcStartUu, g_bcMinArm);
        BcWhy("cutting");
        ok = true;
    } while (0);
    free(idx); free(armv); free(ourIdx);
    if (decl) decl->Release(); if (vb) vb->Release(); if (ib) ib->Release();
    if (!ok) g_bcRefused = true;
    return ok;
}

// The hook (DcDrawIndexed): true = drawn by us.
static bool BodyCutDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex, UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount)
{
    uint8_t* body = g_cbBody;
    if (!body || !g_bcOn || !dev || type != D3DPT_TRIANGLELIST) return false;
    if (g_bcBodyFor != body) { BodyCutReset("new body"); g_bcBodyFor = body; }
    // a known contract?
    for (int i = 0; i < g_bcN; i++) {
        BcContract& c = g_bc[i];
        if (c.numVertices != numVertices || c.primCount != primCount || c.startIndex != startIndex || c.baseVertex != baseVertex || c.minIndex != minIndex) continue;
        IDirect3DVertexBuffer9* vb = NULL; UINT off = 0, stride = 0;
        if (FAILED(dev->GetStreamSource(0, &vb, &off, &stride)) || !vb) continue;
        vb->Release();
        if (vb != c.vb || off != c.off || stride != c.stride) continue;
        IDirect3DIndexBuffer9* saved = NULL;
        dev->GetIndices(&saved);
        bool drew = false;
        if (SUCCEEDED(dev->SetIndices(c.ib))) {
            dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex, numVertices, 0, c.ourPrims);
            drew = true;
        }
        dev->SetIndices(saved);
        if (saved) saved->Release();
        if (drew) { g_bcDraws++; return true; }
        g_bcMisses++;
        return false;
    }
    if (g_bcN >= 4 || g_bcRefused) return false;
    // identification: a big skinned draw whose LocalToWorld is the body's
    if (numVertices < 300 || primCount < 300) return false;
    if (!RangeReadable(body + 0x60 + 48, 12)) return false;
    PcRefreshLayout(dev);
    const double nowMs = MaimNowMs();
    const bool probe = nowMs < g_bcProbeUntil && g_bcProbeN < 40;
    float l2w[16] = { 0 };
    const bool haveL2w = g_pcLayL2W >= 0 && SUCCEEDED(dev->GetVertexShaderConstantF((UINT)g_pcLayL2W, l2w, 4));
    const float* bodyT = (const float*)(body + 0x60 + 48);
    if (probe) {
        // The census that says WHY a body draw is or is not recognised: every big draw for 6 s
        // after the body is found, with the shader's LocalToWorld against the component's.
        IDirect3DVertexDeclaration9* dcl = NULL; UINT posOff = 0xffffffffu; bool skinned = false;
        if (SUCCEEDED(dev->GetVertexDeclaration(&dcl)) && dcl) { skinned = BcPositionOff(dcl, &posOff); dcl->Release(); }
        g_bcProbeN++;
        Log("bodycut/probe: draw %u verts / %u tris base %d min %u start %u: %s, POSITION off %d, L2W reg %d %s (%.1f %.1f %.1f) vs body (%.1f %.1f %.1f)",
            numVertices, primCount, baseVertex, minIndex, startIndex, skinned ? "skinned" : "not skinned or no FLOAT3 position", (int)posOff,
            g_pcLayL2W, haveL2w ? "read" : "unavailable", l2w[12], l2w[13], l2w[14], bodyT[0], bodyT[1], bodyT[2]);
    }
    if (!haveL2w) return false;
    float d[3] = { l2w[12]-bodyT[0], l2w[13]-bodyT[1], l2w[14]-bodyT[2] };
    if (fabsf(d[0]) > 1.5f || fabsf(d[1]) > 1.5f || fabsf(d[2]) > 1.5f) return false;
    g_bcIdentTries++;
    Log("bodycut: draw %u verts / %u tris carries the body's LocalToWorld (%.1f %.1f %.1f): building the cut", numVertices, primCount, l2w[12], l2w[13], l2w[14]);
    if (!BcBuild(dev, baseVertex, minIndex, numVertices, startIndex, primCount)) return false;
    // draw this very call cut
    return BodyCutDraw(dev, type, baseVertex, minIndex, numVertices, startIndex, primCount);
}

static void BodyCutStatus(dvr::status::Writer& w)
{
    w.kv("on", g_bcOn);
    w.kv("contracts", g_bcN);
    w.kv("armVerts", g_bcArmVerts);
    w.kv("dropped", g_bcDropped);
    w.kv("kept", g_bcKept);
    w.kv("draws", (unsigned long)g_bcDraws);
    w.kv("variant", g_bcChainVariant);
    w.kv("why", g_bcWhy);
}
