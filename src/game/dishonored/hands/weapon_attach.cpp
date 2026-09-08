// game/dishonored/hands/weapon_attach.cpp - included by src/mod/dishonoredvr.cpp
// (unity build). See state chunk 57b for the design and why it changed.
//
// VR-33 W2/W3: identify the weapon's draws by a bridged full-transform match,
// then carry them through the SAME correction the hand took.

// ---- reading a native component transform -----------------------------------

// The component's own LocalToWorld, read with the SAME extraction the shader
// constant gets in MpAcquireCtx: three basis registers then a translation.
// Convert native FMatrix rows to our column-vector convention. The same
// extraction is used for the shader registers. This is a convention to test
// against member draws, not proof that every component shares the bridge.
//
// Scale is RETAINED. It is evidence for the match, and dividing it out here
// would throw away the one quantity that separates a scaled duplicate from the
// real thing.
static bool WaReadCompXform(uint8_t* obj, dvr::hf::Mat3* R, float* t, float* scale)
{
    if (!LooksLikeObj(obj) || !RangeReadable(obj + kWaComponentLocalToWorld, 0x40)) return false;
    const float* M = (const float*)(obj + kWaComponentLocalToWorld);   // 0x60 / 0x70 / 0x80
    const float* T = (const float*)(obj + kWaComponentTranslation);
    float col[3][3];
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) col[j][i] = M[j * 4 + i];
    for (int j = 0; j < 3; j++) {
        float n = 0.0f;
        for (int i = 0; i < 3; i++) n += col[j][i] * col[j][i];
        n = sqrtf(n);
        if (!(n > 1.0e-4f) || n != n || n > 1.0e4f) return false;
        scale[j] = n;
        // Keep the full native basis; the matcher compares scale too.
    }
    for (int i = 0; i < 3; i++) {
        t[i] = T[i];
        if (t[i] != t[i] || t[i] > 1.0e9f || t[i] < -1.0e9f) return false;
    }
    *R = dvr::hf::basis_from_cols(col[0], col[1], col[2]);
    return true;
}


// Which hand a member belongs in. The asset-name rule is a DEFAULT, not
// measured attachment data - the review is right that the sword/crossbow
// sides here are inherited assumption. It is logged as an assumption and is
// overridable; an unknown asset is NOT silently swept into the crossbow hand.
static int WaHandFor(const char* asset, bool* known)
{
    if (known) *known = true;
    if (asset && (strstr(asset, "sword") || strstr(asset, "Sword")))
        return g_waSwordHand;
    if (asset && (strstr(asset, "crossbow") || strstr(asset, "Crossbow") ||
                  strstr(asset, "bolt")     || strstr(asset, "Bolt")))
        return g_waXbowHand;
    if (known) *known = false;
    return g_waXbowHand;
}


// ---- the component snapshot, SCRIPT LANE ------------------------------------
//
// Read-only. It does not call the legacy collect/restore writers, which are
// not discovery helpers however much they look like one.
static void WaCompTick(void)
{
    if (!g_waOn) return;
    const double now = MaimNowMs();
    if (now - g_waCompMs < 8.0) return;        // once per frame is plenty


    WaComp snapshot[WA_MAX_COMP] = {};
    int n = 0, dropped = 0;
    for (int i = 0; i < g_fpCandN && n < WA_MAX_COMP; i++) {
        FpCand* k = &g_fpCand[i];
        // A candidate that has gone away since the scan is DROPPED, and that
        // is worth counting: the failing run listed the body mesh among the
        // view models and then did not have it in this snapshot, which is the
        // difference between "never found" and "found and lost".
        if (!LooksLikeObj(k->obj)) { dropped++; continue; }
        WaComp c;
        memset(&c, 0, sizeof(c));
        c.obj = k->obj;
        _snprintf(c.asset, sizeof(c.asset), "%s", k->asset);
        _snprintf(c.name,  sizeof(c.name),  "%s", k->name);
        c.asset[sizeof(c.asset) - 1] = 0;
        c.name[sizeof(c.name) - 1] = 0;
        c.ok = WaReadCompXform(k->obj, &c.R, c.t, c.scale);
        // The BRIDGE ANCHOR is the body mesh - the one the split locks and
        // draws, and therefore the one whose draw we can already identify
        // without any of this machinery. That is the whole point: the offset
        // comes from a component identified by other means.
        c.isRef    = strstr(c.asset, "Skm_Player") != NULL;
        c.isMember = !c.isRef;
        bool known = false;
        c.hand = WaHandFor(c.asset, &known);
        if (c.isMember && !known) c.isMember = false;   // never guess a side
        snapshot[n++] = c;
    }
    // WHAT THE ATTACHMENT ACTUALLY HAS. Counted here rather than inferred from
    // a refusal counter later: without a REF there is no bridge and without a
    // MEMBER there is nothing to move, and those are different problems with
    // different answers.
    int refs = 0, members = 0;
    for (int i = 0; i < n; i++) {
        if (!snapshot[i].ok) continue;
        if (snapshot[i].isRef)    refs++;
        if (snapshot[i].isMember) members++;
    }

    AcquireSRWLockExclusive(&g_waCompLock);
    memcpy(g_waComp, snapshot, sizeof(snapshot));
    g_waCompMs = now;
    g_waCompN = n;
    g_waRefN = refs;
    g_waMemberN = members;
    g_waDroppedN = dropped;
    g_waCompGen++;
    ReleaseSRWLockExclusive(&g_waCompLock);

    // SAY SO, on its own cadence and at Warn, because a run that cannot
    // possibly attach should not look like a run that tried and failed.
    if (!refs || !members) {
        InterlockedIncrement(&g_waNotReady);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 4000,
            "wa: NOTHING TO ATTACH - %d component(s) resolved, %d usable as the "
            "bridge anchor (the body mesh), %d usable as a weapon%s. %s The "
            "weapon path is idle by definition until both exist; no counter "
            "below this describes a failure to place anything.",
            n, refs, members,
            dropped ? " (and some candidates went away between the scan and "
                      "this snapshot)" : "",
            !refs   ? "Without the body mesh there is no coordinate bridge."
                    : "Draw a weapon: nothing is equipped that this can move.");
    }

    static double said = 0;
    if (now - said < 10000) return;
    said = now;
    Log("wa/comp: %d component(s), generation %u", n, g_waCompGen);
    for (int i = 0; i < n; i++)
        Log(
            "wa/comp:   [%d] '%s' (%s) %s%s xform %s t=(%.1f %.1f %.1f) "
            "scale=(%.3f %.3f %.3f)",
            i, g_waComp[i].asset, g_waComp[i].name,
            g_waComp[i].isRef ? "REF" : "", g_waComp[i].isMember ? "MEMBER" : "",
            g_waComp[i].ok ? "ok" : "UNREADABLE",
            (double)g_waComp[i].t[0], (double)g_waComp[i].t[1],
            (double)g_waComp[i].t[2], (double)g_waComp[i].scale[0],
            (double)g_waComp[i].scale[1], (double)g_waComp[i].scale[2]);
}


// ---- the hand publishes its correction, PRESENT LANE ------------------------
//
// Called from the hand's own placement path once D is final - which is AFTER
// the model scale, so that factor is carried exactly once and WaDraw must not
// apply it again.
static void WaPublishCommon(int hand, const MpDrawCtx* c, const dvr::hf::Xform& D)
{
    if (!g_waOn || hand < 0 || hand > 1 || !c) return;
    dvr::hf::Xform L;
    L.r = c->R_L;
    for (int i = 0; i < 3; i++) L.t[i] = c->t[i];

    WaCommon w = {};
    w.L_hand  = L;
    dvr::hf::Xform invL;
    if (!dvr::wf::inverse(L, &invL)) return;
    w.D = dvr::hf::xform_mul(dvr::hf::xform_mul(L, D), invL);
    w.present = (uint32_t)dvr::frame::count();
    w.poseGen = c->pose.gen;
    w.eye     = g_mpEyeState;
    w.ok      = true;
    for (int i = 0; i < 9; i++) if (!MpFinite(w.D.r.m[i])) w.ok = false;
    for (int i = 0; i < 3; i++) if (!MpFinite(w.D.t[i]))   w.ok = false;
    AcquireSRWLockShared(&g_waCompLock);
    w.componentCount = g_waCompN;
    w.componentGen = g_waCompGen;
    memcpy(w.components, g_waComp, sizeof(w.components));
    const double age = MaimNowMs() - g_waCompMs;
    ReleaseSRWLockShared(&g_waCompLock);
    if (age < 0 || age > 100.0) { w.ok = false; InterlockedIncrement(&g_waStaleComp); }
    g_waCommon[hand] = w;
}


// Is there a correction for THIS view? Exact on Present, pose generation and
// eye - a correction from the previous eye is a whole IPD wrong, and borrowing
// one is the failure the old boolean could not even express.
static const WaCommon* WaCommonFor(int hand, const MpDrawCtx* c)
{
    if (hand < 0 || hand > 1) return NULL;
    const WaCommon* w = &g_waCommon[hand];
    if (!w->ok) return NULL;
    if (w->present != (uint32_t)dvr::frame::count()) return NULL;
    if (w->poseGen != c->pose.gen) return NULL;
    if (w->eye     != g_mpEyeState) return NULL;
    return w;
}



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


// ---- recognition by buffer identity -----------------------------------------

// Apply a known delta to whatever palette this shader declares. Shared by the
// buffer-identity path and the non-indexed path: both already know WHICH mesh
// they are looking at and need only the register and the delta.
static bool WaPatchAndDraw(IDirect3DDevice9* dev, WaMesh* w,
                           const dvr::hf::Xform& delta, bool indexed,
                           D3DPRIMITIVETYPE type, INT baseVertex, UINT minIndex,
                           UINT numVertices, UINT startIndex, UINT startVertex,
                           UINT primCount, HRESULT* hr)
{
    const int start = (g_pcLayBones >= 0) ? g_pcLayBones : g_pcLayBonesPartial;
    const int cnt   = (g_pcLayBones >= 0) ? g_pcLayBonesN : g_pcLayBonesNPartial;
    if (start < 0 || cnt <= 0 || (cnt % 3) != 0 || start > 256 - cnt ||
        cnt > WA_MAX_REGS) return false;

    static float source[WA_MAX_REGS*4], patched[WA_MAX_REGS*4];
    if (FAILED(dev->GetVertexShaderConstantF((UINT)start, source, (UINT)cnt))) {
        InterlockedIncrement(&g_waNoSource); return false;
    }
    MpBuild(patched, source, (UINT)cnt, &delta);
    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, patched, (UINT)cnt))) {
        if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, source, (UINT)cnt)))
            InterlockedIncrement(&g_waRestoreFail);
        InterlockedIncrement(&g_waNoSource); return false;
    }
    // THE DEPTH RANGE, exactly as the main path does it. The view model is
    // drawn into a compressed depth range so it always sits in front of the
    // world; once it has been moved OUT into the world it needs the full range
    // or it renders in front of geometry it is now behind. The hands' own
    // record names this as what fixed both their occlusion AND their duplicate
    // ("correct occlusion against world geometry, after restoring the depth
    // range. The ghost/duplicate hand is gone"), and these two paths were
    // patching the palette without it - the one step of the main path they did
    // not copy.
    D3DVIEWPORT9 savedVp; bool changedVp = false;
    if (g_mpDepth && SUCCEEDED(dev->GetViewport(&savedVp)) && savedVp.MaxZ < .5f) {
        D3DVIEWPORT9 full = savedVp; full.MinZ = 0; full.MaxZ = 1;
        changedVp = SUCCEEDED(dev->SetViewport(&full));
    }
    InterlockedIncrement(&g_waAttempted);
    const HRESULT drawHr = indexed
        ? dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex,
                                        numVertices, startIndex, primCount)
        : dvr::frame::orig_draw_prim(dev, type, startVertex, primCount);
    if (hr) *hr = drawHr;
    if (changedVp && FAILED(dev->SetViewport(&savedVp)))
        InterlockedIncrement(&g_waRestoreFail);
    if (SUCCEEDED(drawHr)) {
        InterlockedIncrement(&g_waSucceeded);
        InterlockedIncrement(&w->placed);
    }
    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)start, source, (UINT)cnt))) {
        InterlockedIncrement(&g_waRestoreFail);
        Log("wa/id: RESTORE FAILED for '%s' c%d x%d", w->asset, start, cnt);
    }
    return SUCCEEDED(drawHr);
}


// The non-indexed entry, which the weapon router never reached before.
static bool WaDrawPrim(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
                       UINT startVertex, UINT primCount, HRESULT* hr)
{
    if (hr) *hr = D3D_OK;
    if (!g_waOn || !dev || !g_waMeshN) return false;
    InterlockedIncrement(&g_waPrimSeen);

    IDirect3DVertexBuffer9* vbo = NULL; UINT offset = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vbo, &offset, &stride)) || !vbo) return false;
    void* vb = vbo; vbo->Release();

    WaMesh* w = NULL;
    for (int i = 0; i < g_waMeshN; ++i)
        if (g_waMesh[i].vb == vb) { w = &g_waMesh[i]; break; }
    if (!w) return false;
    InterlockedIncrement(&g_waPrimVbHit);

    PcRefreshLayout(dev);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "wa/prim: a NON-INDEXED draw shares '%s' vertex buffer %p - stride %u "
        "(contract %u), start %u, prim %u, type %d. This entry never reached "
        "the weapon router before. BoneMatrices %s.",
        w->asset, vb, stride, w->stride, startVertex, primCount, (int)type,
        (g_pcLayBones >= 0 || g_pcLayBonesPartial >= 0) ? "declared" : "NOT declared");

    if (g_pcLayBones < 0 && g_pcLayBonesPartial < 0) {
        InterlockedIncrement(&g_waPrimNoBone); return false;
    }
    if (!w->dmOk || w->dmPresent != (uint32_t)dvr::frame::count()) {
        InterlockedIncrement(&g_waPrimNoDelta); return false;
    }
    if (!WaPatchAndDraw(dev, w, w->dm, false, type, 0, 0, 0, 0, startVertex,
                        primCount, hr)) return false;
    InterlockedIncrement(&g_waPrimFixed);
    return true;
}


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
static bool WaDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr)
{
    if (hr) *hr = D3D_OK;
    if (!g_waOn || !dev) return false;
    InterlockedIncrement(&g_waSeen);

    // BUFFER IDENTITY FIRST, AND UNBUDGETED. This is the arm fix: a draw bound
    // to a weapon's buffers IS that weapon whatever its constants look like, so
    // recognising it must not depend on the shader declaring the full layout
    // that IDENTIFYING it needs. Two device reads on the indexed path, the same
    // pair the mesh lock already pays for every frame.
    if (g_waMeshN) {
        IDirect3DVertexBuffer9* vbo = NULL; UINT off0 = 0, str0 = 0;
        if (SUCCEEDED(dev->GetStreamSource(0, &vbo, &off0, &str0)) && vbo) {
            void* vb0 = vbo; vbo->Release();
            IDirect3DIndexBuffer9* ibo = NULL; void* ib0 = NULL;
            if (SUCCEEDED(dev->GetIndices(&ibo)) && ibo) { ib0 = ibo; ibo->Release(); }
            WaMesh* known = NULL;
            for (int i = 0; i < g_waMeshN; ++i)
                if (g_waMesh[i].vb == vb0 ||
                    (ib0 && g_waMesh[i].ib == ib0)) { known = &g_waMesh[i]; break; }
            if (known) {
                PcRefreshLayout(dev);
                const bool sameContract =
                    known->vb == vb0 && known->ib == ib0 &&
                    known->stride == str0 && known->streamOffset == off0 &&
                    known->type == type && known->baseVertex == baseVertex &&
                    known->minIndex == minIndex && known->startIndex == startIndex &&
                    known->numVerts == numVertices && known->primCount == primCount;
                // A draw on known buffers that is NOT the contract's own draw is
                // another pass of it - the copy left at the native position.
                if (!sameContract) {
                    InterlockedIncrement(&g_waIdSeen);
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                        "wa/id: another pass of '%s' on the same buffers - vb %p "
                        "(%s) ib %p (%s), prim %u (contract %u), verts %u (%u), "
                        "start %u (%u), stride %u (%u). Recognised by buffer "
                        "identity, which does not need the LocalToWorld this "
                        "pass never declares. BoneMatrices %s.",
                        known->asset, vb0, vb0 == known->vb ? "same" : "different",
                        ib0, ib0 == known->ib ? "same" : "different",
                        primCount, known->primCount, numVertices, known->numVerts,
                        startIndex, known->startIndex, str0, known->stride,
                        (g_pcLayBones >= 0 || g_pcLayBonesPartial >= 0)
                            ? "declared" : "NOT declared");
                    if (g_pcLayBones < 0 && g_pcLayBonesPartial < 0)
                        InterlockedIncrement(&g_waIdNoBone);
                    else if (!known->dmOk ||
                             known->dmPresent != (uint32_t)dvr::frame::count())
                        InterlockedIncrement(&g_waIdNoDelta);
                    else if (g_waGhostFix &&
                             WaPatchAndDraw(dev, known, known->dm, true, type,
                                            baseVertex, minIndex, numVertices,
                                            startIndex, 0, primCount, hr)) {
                        InterlockedIncrement(&g_waIdCorrected);
                        InterlockedIncrement(&known->ghosts);
                        return true;
                    }
                    return false;
                }
            }
        }
    }

    PcRefreshLayout(dev);
    if (g_pcLayL2W < 0 || g_pcLayL2W > 252) {
        InterlockedIncrement(&g_waNoLayout);
        // Before giving up: is this another PASS of a weapon already
        // identified? That pass has no LocalToWorld to be identified by, but
        // its geometry is known and its sibling's correction is in hand.
        if (g_waGhostFix && g_waMeshN && WaPrimCountKnown(primCount) &&
            WaGhostPass(dev, type, baseVertex, minIndex, numVertices,
                        startIndex, primCount, hr))
            return true;
        // Nothing matched on the strict key. Ask what this draw actually IS,
        // because "0 ghost passes" and "the ghost is not an indexed draw of a
        // known buffer" are different answers and the first build could not
        // tell them apart.
        WaProbeRefused(dev, type, baseVertex, minIndex, numVertices,
                       startIndex, primCount);
        return false;
    }
    // The CTAB declaration bounds the patch. Never extend to the end of the
    // constant bank: that includes LocalToWorld, projection and lighting.
    if (!dvr::wf::palette_range(g_pcLayBones, g_pcLayBonesN, g_pcLayVp, g_pcLayL2W)) {
        InterlockedIncrement(&g_waNoSource);
        g_waWhy = "no validated, non-overlapping BoneMatrices declaration";
        return false;
    }
    // The weapon never solves an XR pose or optical centre. Requiring the
    // hand's symmetric camera projection here excludes depth/shadow passes.
    MpDrawCtx ctx = {};
    if (FAILED(dev->GetVertexShaderConstantF(g_pcLayL2W, ctx.l2w, 4))) {
        InterlockedIncrement(&g_waNoLayout); return false;
    }
    for (int r = 0; r < 3; ++r) {
        ctx.t[r] = ctx.l2w[12+r];
        for (int col = 0; col < 3; ++col) ctx.R_L.m[r*3+col] = ctx.l2w[col*4+r];
    }
    if (!g_mpPoseCsOk) return false;
    EnterCriticalSection(&g_mpPoseCs);
    ctx.pose = g_mpPosePub;
    LeaveCriticalSection(&g_mpPoseCs);
    const WaCommon* views[2] = {WaCommonFor(0, &ctx), WaCommonFor(1, &ctx)};
    if (!views[0] && !views[1]) {
        InterlockedIncrement(&g_waNoCommon); return false;
    }
    static uint32_t tryPresent = 0;
    static int tries = 0;
    const uint32_t present = (uint32_t)dvr::frame::count();
    if (tryPresent != present) { tryPresent = present; tries = 0; }
    // Budget diagnostics only, not placement authority. A scene with many
    // world draws must not starve its later first-person draws indefinitely.
    if (++tries > g_waMaxTry) InterlockedIncrement(&g_waBudgetSkip);

    dvr::hf::Xform draw = {ctx.R_L, {ctx.t[0], ctx.t[1], ctx.t[2]}};
    dvr::wf::Candidate candidates[64];
    const WaComp* members[64];
    dvr::hf::Xform corrections[64];
    int count = 0;
    for (int h = 0; h < 2; ++h) {
        const WaCommon* v = views[h];
        if (!v) continue;
        const WaComp* ref = NULL;
        bool duplicate = false;
        for (int i = 0; i < v->componentCount; ++i) {
            const WaComp* k = &v->components[i];
            if (!k->ok || !k->isRef) continue;
            if (ref) { duplicate = true; break; }
            ref = k;
        }
        if (!ref || duplicate) { InterlockedIncrement(&g_waNoBridge); continue; }
        dvr::hf::Xform nativeRef = {ref->R, {ref->t[0], ref->t[1], ref->t[2]}}, bridge;
        if (!dvr::wf::bridge(nativeRef, v->L_hand, &bridge)) {
            InterlockedIncrement(&g_waNoBridge); continue;
        }
        dvr::hf::Xform invBridge;
        if (!dvr::wf::inverse(bridge, &invBridge)) continue;
        const dvr::hf::Xform nativeDelta = dvr::hf::xform_mul(
            dvr::hf::xform_mul(invBridge, v->D), bridge);
        for (int i = 0; i < v->componentCount && count + 1 < 64; ++i) {
            const WaComp* k = &v->components[i];
            if (!k->ok || !k->isMember || k->hand != h) continue;
            dvr::hf::Xform native = {k->R, {k->t[0], k->t[1], k->t[2]}};
            dvr::wf::Candidate c;
            c.predicted = dvr::hf::xform_mul(bridge, native);
            c.hand = h;
            c.assembly = (strstr(k->asset, "sword") || strstr(k->asset, "Sword")) ? 1 : 2;
            candidates[count] = c; corrections[count] = v->D; members[count++] = k;
            // A world-space pass has a second independently known prediction:
            // the native component transform itself. It also needs the delta
            // converted back out of the reference draw's rebased coordinates.
            candidates[count] = c; candidates[count].predicted = native;
            corrections[count] = nativeDelta; members[count++] = k;
            InterlockedIncrement(&g_waHandCompared[h]);
            const dvr::wf::Result one = dvr::wf::match(draw, &c, 1, g_waAngTolDeg, g_waPosTolUU, g_waMarginX);
            if (one.score < g_waNearestScore[h]) {
                g_waNearestScore[h] = one.score; g_waNearestAngle[h] = one.angle;
                g_waNearestPos[h] = one.position; g_waNearestScale[h] = one.scale;
                strcpy_s(g_waNearestName[h], k->asset);
            }
        }
    }
    InterlockedIncrement(&g_waCandChecked);
    const dvr::wf::Result match = dvr::wf::match(draw, candidates, count,
        g_waAngTolDeg, g_waPosTolUU, g_waMarginX);
    if (match.best < 0) { InterlockedIncrement(&g_waNoCandidate); return false; }
    if (match.ambiguous) { InterlockedIncrement(&g_waAmbiguous); return false; }
    const WaComp* member = members[match.best];
    const int hand = candidates[match.best].hand;
    const WaCommon* wc = views[hand];

    IDirect3DVertexBuffer9* vb = NULL; UINT offset = 0, stride = 0;
    IDirect3DIndexBuffer9* ib = NULL;
    IDirect3DVertexDeclaration9* decl = NULL;
    IDirect3DVertexShader9* vs = NULL;
    dev->GetStreamSource(0, &vb, &offset, &stride);
    dev->GetIndices(&ib); dev->GetVertexDeclaration(&decl); dev->GetVertexShader(&vs);
    const bool geometryOk = vb && ib && decl && vs;
    if (vb) vb->Release(); if (ib) ib->Release();
    if (decl) decl->Release(); if (vs) vs->Release();
    if (!geometryOk) return false;
    WaMesh* w = NULL;
    for (int i = 0; i < g_waMeshN; ++i) {
        WaMesh* k = &g_waMesh[i];
        if (k->vb == vb && k->ib == ib && k->decl == decl && k->vs == vs &&
            k->stride == stride && k->streamOffset == offset && k->type == type &&
            k->baseVertex == baseVertex && k->minIndex == minIndex &&
            k->startIndex == startIndex && k->numVerts == numVertices && k->primCount == primCount &&
            k->hand == hand && !strcmp(k->asset, member->asset)) { w = k; break; }
    }
    if (w) InterlockedIncrement(&g_waCacheHit);
    else {
        // Full reporting table does not prevent a new weapon from attaching.
        const bool full = g_waMeshN == WA_MAX_MESH;
        int slot = full ? 0 : g_waMeshN++;
        if (full)
            for (int i = 1; i < g_waMeshN; ++i)
                if (g_waMesh[i].lastVerifyMs < g_waMesh[slot].lastVerifyMs) slot = i;
        w = &g_waMesh[slot]; memset(w, 0, sizeof(*w));
        w->vb = vb; w->ib = ib; w->decl = decl; w->vs = vs;
        w->stride = stride; w->streamOffset = offset; w->type = type;
        w->baseVertex = baseVertex; w->minIndex = minIndex; w->startIndex = startIndex;
        w->numVerts = numVertices; w->primCount = primCount; w->hand = hand;
        strcpy_s(w->asset, member->asset);
        InterlockedIncrement(&g_waMatched);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "wa: MATCH '%s' hand %d: %.4f deg %.4f uu scale error %.5f; c%d x%d; component snapshot %u",
            w->asset, hand, match.angle, match.position, match.scale,
            g_pcLayBones, g_pcLayBonesN, wc->componentGen);
    }
    w->lastVerifyMs = MaimNowMs();
    w->boneReg = g_pcLayBones; w->regs = (UINT)g_pcLayBonesN;
    dvr::hf::Xform inverse;
    if (!dvr::wf::inverse(draw, &inverse)) { InterlockedIncrement(&w->refused); return false; }
    const dvr::hf::Xform delta = dvr::hf::xform_mul(
        dvr::hf::xform_mul(inverse, corrections[match.best]), draw);
    for (int i = 0; i < 9; ++i) if (!MpFinite(delta.r.m[i])) return false;
    for (int i = 0; i < 3; ++i) if (!MpFinite(delta.t[i])) return false;
    // Publish it for this contract's OTHER passes. Same mesh, same Present,
    // same place - so the depth and shadow copies take the identical delta
    // instead of being left at the native position.
    w->dm = delta; w->dmPresent = (uint32_t)dvr::frame::count(); w->dmOk = true;
    static float source[WA_MAX_REGS*4], patched[WA_MAX_REGS*4];
    if (FAILED(dev->GetVertexShaderConstantF(w->boneReg, source, w->regs))) {
        InterlockedIncrement(&g_waNoSource); return false;
    }
    MpBuild(patched, source, w->regs, &delta);
    if (FAILED(dvr::frame::orig_set_vs_const(dev, w->boneReg, patched, w->regs))) {
        if (FAILED(dvr::frame::orig_set_vs_const(dev, w->boneReg, source, w->regs)))
            InterlockedIncrement(&g_waRestoreFail);
        InterlockedIncrement(&g_waNoSource); return false;
    }
    D3DVIEWPORT9 savedVp; bool changedVp = false;
    if (g_mpDepth && SUCCEEDED(dev->GetViewport(&savedVp)) && savedVp.MaxZ < .5f) {
        D3DVIEWPORT9 full = savedVp; full.MinZ = 0; full.MaxZ = 1;
        changedVp = SUCCEEDED(dev->SetViewport(&full));
    }
    InterlockedIncrement(&g_waAttempted);
    const HRESULT drawHr = dvr::frame::orig_draw_indexed(dev, type, baseVertex,
        minIndex, numVertices, startIndex, primCount);
    if (hr) *hr = drawHr;
    if (SUCCEEDED(drawHr)) { InterlockedIncrement(&g_waSucceeded); InterlockedIncrement(&w->placed); }
    if (changedVp && FAILED(dev->SetViewport(&savedVp))) InterlockedIncrement(&g_waRestoreFail);
    if (FAILED(dvr::frame::orig_set_vs_const(dev, w->boneReg, source, w->regs))) {
        InterlockedIncrement(&g_waRestoreFail);
        Log("wa: RESTORE FAILED for '%s' c%d x%u", w->asset, w->boneReg, w->regs);
    }
    g_waWhy = "placed";
    return true;
}

static void WaBeat(void)
{
    if (!g_waOn) return;
    static double said = 0;
    const double now = MaimNowMs();
    if (now - said < 5000) return;
    said = now;
    Log("wa: beat v2 | routed %ld compared %ld; member comparisons L %ld R %ld; "
        "contracts %ld ambiguity %ld misses %ld; attempted %ld succeeded %ld restore-fail %ld; "
        "no-layout %ld no-source %ld no-view %ld no-bridge %ld stale-snapshot %ld over-budget %ld | "
        "ghost passes seen %ld fixed %ld (no bone decl %ld, no sibling delta %ld, bad range %ld) | "
        "probe ran %ld: shares our vertex buffer %ld (same index buffer %ld), not ours %ld, "
        "over budget %ld | other passes on known buffers %ld: corrected %ld "
        "(no bone decl %ld, no sibling delta %ld) | non-indexed %ld examined %ld: "
        "on known buffers %ld corrected %ld (no bone decl %ld, no delta %ld) | "
        "components: %d bridge anchor(s), "
        "%d member(s), %d dropped, %ld tick(s) with nothing to attach | "
        "stance %s (eye %.1f uu) | %s",
        g_waSeen, g_waCandChecked, g_waHandCompared[0], g_waHandCompared[1],
        g_waMatched, g_waAmbiguous, g_waNoCandidate, g_waAttempted, g_waSucceeded,
        g_waRestoreFail, g_waNoLayout, g_waNoSource, g_waNoCommon, g_waNoBridge, g_waStaleComp,
        g_waBudgetSkip, g_waGhostSeen, g_waGhostFixed, g_waGhostNoBone,
        g_waGhostNoDelta, g_waGhostRange, g_waProbeRan, g_waProbeVbHit,
        g_waProbeIbHit, g_waProbeMiss, g_waProbeCapped,
        g_waIdSeen, g_waIdCorrected, g_waIdNoBone, g_waIdNoDelta,
        g_waNonIndexed, g_waPrimSeen, g_waPrimVbHit, g_waPrimFixed,
        g_waPrimNoBone, g_waPrimNoDelta,
        g_waRefN, g_waMemberN, g_waDroppedN, g_waNotReady,
        // THE STANCE. It read "standing (eye 0.0 uu)" for every sample of a run
        // in which the tester deliberately spent half the time crouched: the
        // eye height was never resolved, so the flag was 0 BY DESIGN and the
        // line said "standing" anyway. A zero that is expected has to say so ON
        // THE LINE (CLAUDE.md), and this one did not - it cost the correlation
        // the run was made to capture.
        (!g_actorLocFound || g_eyeNowUU == 0.0f)
            ? "UNKNOWN (Actor.Location unresolved - NOT a report of standing)"
            : (g_eyeCrouched ? "CROUCHED" : "standing"),
        (double)g_eyeNowUU,
        g_waWhy);
    for (int h = 0; h < 2; ++h) {
        if (g_waNearestScore[h] != FLT_MAX)
            Log("wa: interval nearest hand %d '%s': %.4f deg / %.4f uu / scale %.5f (bands %.3f / %.3f / .005)",
                h, g_waNearestName[h], g_waNearestAngle[h], g_waNearestPos[h],
                g_waNearestScale[h], g_waAngTolDeg, g_waPosTolUU);
        g_waNearestScore[h] = FLT_MAX;
    }
    for (int i = 0; i < g_waMeshN; ++i) {
        const WaMesh* w = &g_waMesh[i];
        Log("wa: contract '%s' hand %d c%d x%u placed %ld refused %ld ghost passes %ld age %.0f ms",
            w->asset, w->hand, w->boneReg, w->regs, w->placed, w->refused,
            w->ghosts, now - w->lastVerifyMs);
    }
}
