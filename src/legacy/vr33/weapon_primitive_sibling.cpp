// ---- the ghost pass ---------------------------------------------------------

// The primitive count arrives as a call argument, so this costs nothing and
// keeps the device reads below off all but a handful of world draws.
static bool WaPrimCountKnown(UINT primCount)
{
    for (int i = 0; i < g_waMeshN; i++)
        if (g_waMesh[i].primCount == primCount && g_waMesh[i].dmOk) return true;
    return false;
}


// A draw refused for want of a layout, whose GEOMETRY is one a contract has
// already identified. Same buffers, same range, same primitive count - and a
// DIFFERENT shader, which is what makes it another pass rather than the same
// draw. It is the dark copy standing at the native position.
//
// It is not identified again: it takes the correction its sibling computed in
// THIS Present. Same mesh, same frame, same place, therefore same delta. The
// Present must match exactly - a stale delta would put the ghost somewhere new
// rather than leaving it where it was, which is worse than not fixing it.
static bool WaGhostPass(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                        INT baseVertex, UINT minIndex, UINT numVertices,
                        UINT startIndex, UINT primCount, HRESULT* hr)
{
    IDirect3DVertexBuffer9* vbo = NULL; UINT offset = 0, stride = 0;
    IDirect3DIndexBuffer9* ibo = NULL;
    IDirect3DVertexShader9* vso = NULL;
    if (FAILED(dev->GetStreamSource(0, &vbo, &offset, &stride)) || !vbo) return false;
    void* vb = vbo; vbo->Release();
    if (FAILED(dev->GetIndices(&ibo)) || !ibo) return false;
    void* ib = ibo; ibo->Release();
    if (FAILED(dev->GetVertexShader(&vso)) || !vso) return false;
    void* vs = vso; vso->Release();

    WaMesh* w = NULL;
    for (int i = 0; i < g_waMeshN; ++i) {
        WaMesh* k = &g_waMesh[i];
        if (k->vb == vb && k->ib == ib && k->stride == stride &&
            k->streamOffset == offset && k->type == type &&
            k->baseVertex == baseVertex && k->minIndex == minIndex &&
            k->startIndex == startIndex && k->numVerts == numVertices &&
            k->primCount == primCount && k->vs != vs) { w = k; break; }
    }
    if (!w) return false;
    InterlockedIncrement(&g_waGhostSeen);

    // NAME IT, whether or not it can be fixed. The handoff asks for the
    // silhouette's shader and range rather than an assertion from its
    // appearance, and this is where both are known.
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "wa/ghost: an UNCORRECTED PASS of '%s' - same buffers and range as a "
        "matched contract, drawn by a different shader %p that declares no "
        "LocalToWorld. This is the copy standing at the native position. "
        "prim %u verts %u start %u base %d stride %u | BoneMatrices %s",
        w->asset, vs, primCount, numVertices, startIndex, baseVertex, stride,
        g_pcLayBonesPartial >= 0 ? "declared" : "NOT declared");

    if (g_pcLayBonesPartial < 0) {
        InterlockedIncrement(&g_waGhostNoBone);
        return false;                       // nothing to patch; leave it alone
    }
    if (!w->dmOk || w->dmPresent != (uint32_t)dvr::frame::count()) {
        InterlockedIncrement(&g_waGhostNoDelta);
        return false;                       // its sibling has not drawn yet
    }
    // Bounds only: this shader declares no VP or LocalToWorld to overlap with,
    // so palette_range's overlap arguments do not apply. The block must still
    // be whole 3-register matrices inside the bank.
    const int start = g_pcLayBonesPartial, cnt = g_pcLayBonesNPartial;
    if (start < 0 || cnt <= 0 || (cnt % 3) != 0 || start > 256 - cnt ||
        cnt > WA_MAX_REGS) {
        InterlockedIncrement(&g_waGhostRange);
        return false;
    }

    static float source[WA_MAX_REGS*4], patched[WA_MAX_REGS*4];
    if (FAILED(dev->GetVertexShaderConstantF((UINT)start, source, (UINT)cnt))) {
        InterlockedIncrement(&g_waNoSource); return false;
    }
    MpBuild(patched, source, (UINT)cnt, &w->dm);
    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, patched, (UINT)cnt))) {
        if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, source, (UINT)cnt)))
            InterlockedIncrement(&g_waRestoreFail);
        InterlockedIncrement(&g_waNoSource); return false;
    }
    InterlockedIncrement(&g_waAttempted);
    const HRESULT drawHr = dvr::frame::orig_draw_indexed(dev, type, baseVertex,
        minIndex, numVertices, startIndex, primCount);
    if (hr) *hr = drawHr;
    if (SUCCEEDED(drawHr)) {
        InterlockedIncrement(&g_waSucceeded);
        InterlockedIncrement(&g_waGhostFixed);
        InterlockedIncrement(&w->ghosts);
    }
    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, source, (UINT)cnt))) {
        InterlockedIncrement(&g_waRestoreFail);
        Log("wa/ghost: RESTORE FAILED for '%s' c%d x%d", w->asset, start, cnt);
    }
    return true;
}


// A cache records reporting/geometry history only. Every draw must still match
// a current owned component, even when a world instance shares these buffers.
