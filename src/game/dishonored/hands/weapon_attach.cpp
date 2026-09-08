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
    int n = 0;
    for (int i = 0; i < g_fpCandN && n < WA_MAX_COMP; i++) {
        FpCand* k = &g_fpCand[i];
        if (!LooksLikeObj(k->obj)) continue;
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
    AcquireSRWLockExclusive(&g_waCompLock);
    memcpy(g_waComp, snapshot, sizeof(snapshot));
    g_waCompMs = now;
    g_waCompN = n;
    g_waCompGen++;
    ReleaseSRWLockExclusive(&g_waCompLock);

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



// A cache records reporting/geometry history only. Every draw must still match
// a current owned component, even when a world instance shares these buffers.
static bool WaDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr)
{
    if (hr) *hr = D3D_OK;
    if (!g_waOn || !dev) return false;
    InterlockedIncrement(&g_waSeen);
    PcRefreshLayout(dev);
    if (g_pcLayL2W < 0 || g_pcLayL2W > 252) {
        InterlockedIncrement(&g_waNoLayout); return false;
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
        "no-layout %ld no-source %ld no-view %ld no-bridge %ld stale-snapshot %ld over-budget %ld | %s",
        g_waSeen, g_waCandChecked, g_waHandCompared[0], g_waHandCompared[1],
        g_waMatched, g_waAmbiguous, g_waNoCandidate, g_waAttempted, g_waSucceeded,
        g_waRestoreFail, g_waNoLayout, g_waNoSource, g_waNoCommon, g_waNoBridge, g_waStaleComp,
        g_waBudgetSkip, g_waWhy);
    for (int h = 0; h < 2; ++h) {
        if (g_waNearestScore[h] != FLT_MAX)
            Log("wa: interval nearest hand %d '%s': %.4f deg / %.4f uu / scale %.5f (bands %.3f / %.3f / .005)",
                h, g_waNearestName[h], g_waNearestAngle[h], g_waNearestPos[h],
                g_waNearestScale[h], g_waAngTolDeg, g_waPosTolUU);
        g_waNearestScore[h] = FLT_MAX;
    }
    for (int i = 0; i < g_waMeshN; ++i) {
        const WaMesh* w = &g_waMesh[i];
        Log("wa: contract '%s' hand %d c%d x%u placed %ld refused %ld age %.0f ms",
            w->asset, w->hand, w->boneReg, w->regs, w->placed, w->refused, now-w->lastVerifyMs);
    }
}
