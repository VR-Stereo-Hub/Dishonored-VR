// ---- the probe --------------------------------------------------------------

// What IS this refused draw, relative to the weapons we have identified? Runs
// only on draws the layout gate turned away, only while contracts exist, and
// only within a per-Present budget.
static void WaProbeRefused(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                           INT baseVertex, UINT minIndex, UINT numVertices,
                           UINT startIndex, UINT primCount)
{
    if (!g_waProbe || !g_waMeshN) return;
    static uint32_t probePresent = 0; static int used = 0;
    const uint32_t present = (uint32_t)dvr::frame::count();
    if (present != probePresent) { probePresent = present; used = 0; }
    if (used >= g_waProbeBudget) { InterlockedIncrement(&g_waProbeCapped); return; }
    used++;
    InterlockedIncrement(&g_waProbeRan);

    IDirect3DVertexBuffer9* vbo = NULL; UINT offset = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vbo, &offset, &stride)) || !vbo) return;
    void* vb = vbo; vbo->Release();

    const WaMesh* hit = NULL;
    for (int i = 0; i < g_waMeshN; ++i)
        if (g_waMesh[i].vb == vb) { hit = &g_waMesh[i]; break; }
    if (!hit) { InterlockedIncrement(&g_waProbeMiss); return; }
    InterlockedIncrement(&g_waProbeVbHit);

    IDirect3DIndexBuffer9* ibo = NULL;
    void* ib = NULL;
    if (SUCCEEDED(dev->GetIndices(&ibo)) && ibo) { ib = ibo; ibo->Release(); }
    IDirect3DVertexShader9* vso = NULL;
    void* vs = NULL;
    if (SUCCEEDED(dev->GetVertexShader(&vso)) && vso) { vs = vso; vso->Release(); }
    if (ib && ib == hit->ib) InterlockedIncrement(&g_waProbeIbHit);

    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "wa/probe: a REFUSED draw shares '%s' vertex buffer %p. index buffer %p "
        "(%s), shader %p (%s), prim %u (contract %u), verts %u (%u), start %u "
        "(%u), base %d (%d), stride %u (%u), type %d (%d), BoneMatrices %s. "
        "This is what the dark copy at the native position actually is: same "
        "buffers and a different range means another section or LOD, a "
        "different index buffer means another mesh built from the same "
        "vertices.",
        hit->asset, vb, ib, (ib == hit->ib) ? "SAME" : "different",
        vs, (vs == hit->vs) ? "SAME" : "different",
        primCount, hit->primCount, numVertices, hit->numVerts,
        startIndex, hit->startIndex, baseVertex, hit->baseVertex,
        stride, hit->stride, (int)type, (int)hit->type,
        g_pcLayBonesPartial >= 0 ? "declared" : "NOT declared");
}


