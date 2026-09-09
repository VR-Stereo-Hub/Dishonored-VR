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
    // Twice a frame at 90 Hz. The snapshot's AGE is what limits the match when
    // the view model is swaying, so halving the mean age is worth the walk.
    if (now - g_waCompMs < 4.0) return;


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
    g_waSnapAgeAtUse = (float)age;
    if (age < 0 || age > (double)g_waSnapMaxMs) {
        w.ok = false;
        InterlockedIncrement(&g_waStaleComp);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 5000,
            "wa: component snapshot is %.1f ms old, past the %.0f ms bound - "
            "refusing to publish a correction from it. While the view model is "
            "swaying, an old snapshot predicts where the weapon WAS.",
            age, (double)g_waSnapMaxMs);
    }
    g_waCommon[hand] = w;
}


// Is there a correction for THIS view? Exact on Present, pose generation and
// eye - a correction from the previous eye is a whole IPD wrong, and borrowing
// one is the failure the old boolean could not even express.
// THE POSE GENERATION WAS THE WRONG TEST, AND IT WAS POSITION-DEPENDENT.
//
// This used to demand that the weapon draw's own freshly-read pose snapshot
// carry the SAME generation as the hand's. The weapon does not use its pose for
// anything - it needs the hand's correction, which is a property of the FRAME.
// Meanwhile the pose tick publishes on its own schedule, about ninety times a
// second, so any publication landing between the hand's draw and the weapon's
// draw within one Present bumped the generation and the correction was refused
// as stale when it was perfectly current.
//
// How often that happens depends on how much wall-clock passes between those
// two draws, which depends on how heavy the scene is - which is why the tester
// could stand on one spot and have both weapons lock, walk forward and have
// them unlock, and walk back onto the same spot and have them relock, every
// time. It was never about where the player was; it was about how long the
// frame took there.
//
// Present and EYE are kept. Both are genuine properties of the view: a
// correction from another Present is stale, and one from the other eye is a
// whole IPD wrong. The generation is recorded for reporting only.
static const WaCommon* WaCommonFor(int hand, const MpDrawCtx* c)
{
    if (hand < 0 || hand > 1) return NULL;
    const WaCommon* w = &g_waCommon[hand];
    if (!w->ok) return NULL;
    if (w->present != (uint32_t)dvr::frame::count()) return NULL;
    if (w->eye     != g_mpEyeState) return NULL;
    // Measure what the old test would have thrown away, so the claim above is
    // checkable rather than asserted.
    if (c && w->poseGen != c->pose.gen) InterlockedIncrement(&g_waPoseGenDiff);
    return w;
}



#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_refused_probe.cpp"
#endif

// ---- the view model census --------------------------------------------------

// Record one distinct skinned geometry that reached the matcher. Deduplicated
// on the full draw contract, logged once, never used as a gate.
static void WaCensusNote(IDirect3DDevice9* dev, const MpDrawCtx* ctx,
                         INT baseVertex, UINT numVertices, UINT startIndex,
                         UINT primCount, const char* nearest, float angle,
                         float position, bool corrected)
{
    if (!g_waCensusOn || !dev) return;
    IDirect3DVertexBuffer9* vbo = NULL; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vbo, &off, &stride)) || !vbo) return;
    void* vb = vbo; vbo->Release();
    IDirect3DIndexBuffer9* ibo = NULL; void* ib = NULL;
    if (SUCCEEDED(dev->GetIndices(&ibo)) && ibo) { ib = ibo; ibo->Release(); }
    IDirect3DVertexShader9* vso = NULL; void* vs = NULL;
    if (SUCCEEDED(dev->GetVertexShader(&vso)) && vso) { vs = vso; vso->Release(); }

    for (int i = 0; i < g_waCensusN; ++i) {
        WaCensus* c = &g_waCensus[i];
        if (c->vb == vb && c->ib == ib && c->vs == vs &&
            c->primCount == primCount && c->numVerts == numVertices &&
            c->startIndex == startIndex) {
            InterlockedIncrement(&c->draws);
            if (corrected) c->corrected = true;
            return;
        }
    }
    if (g_waCensusN >= WA_MAX_CENSUS) return;
    WaCensus* c = &g_waCensus[g_waCensusN++];
    memset(c, 0, sizeof(*c));
    c->vb = vb; c->ib = ib; c->vs = vs;
    c->primCount = primCount; c->numVerts = numVertices;
    c->startIndex = startIndex; c->stride = stride;
    for (int j = 0; j < 3; ++j) c->l2w[j] = ctx ? ctx->t[j] : 0.0f;
    c->angle = angle; c->position = position;
    c->corrected = corrected;
    c->draws = 1;
    _snprintf(c->nearest, sizeof(c->nearest), "%s", nearest ? nearest : "?");
    c->nearest[sizeof(c->nearest) - 1] = 0;
    Log("wa/census: [%d] vb %p ib %p vs %p | prim %u verts %u start %u stride %u "
        "| LocalToWorld t=(%.1f %.1f %.1f) | nearest '%s' at %.3f deg / %.2f uu "
        "| %s. One line per distinct geometry on the view-model rig; between "
        "them they account for every piece of it, including whatever is still "
        "standing at the native position.",
        g_waCensusN - 1, vb, ib, vs, primCount, numVertices, startIndex, stride,
        (double)c->l2w[0], (double)c->l2w[1], (double)c->l2w[2],
        c->nearest, (double)angle, (double)position,
        corrected ? "CORRECTED" : "left where the engine drew it");
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


#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_primitive_sibling.cpp"
#endif

// ---- VR-59: verify a draw against the instance the contract was matched to --
//
// Returns the verdict and, for the log, the offset and which reference answered.
// The engine-read component transform is authoritative and is preferred whenever
// the bridge can be built; the recent-verification cache exists only so that a
// Present in which no correction was published does not force every draw to be
// refused, which would blink the weapons.
//
// NOTE the failure direction. When nothing can answer, this returns NO_REF and
// the caller hands the draw back untouched. Refusing to touch a draw costs at
// most a duplicate pass of a weapon; correcting one on faith is what dragged
// world objects onto the hand.
static dvr::wf::Instance WaVerifyDraw(const WaMesh* w, const MpDrawCtx* c2,
                                      float* offsetOut, dvr::wf::InstanceRef* refOut)
{
    *offsetOut = 0.0f;
    *refOut = dvr::wf::IREF_NONE;
    if (!w || !c2) return dvr::wf::INSTANCE_NO_REF;

    // 1. THE ENGINE'S OWN ANSWER. The component snapshot is republished twice a
    // frame and does not depend on any correction having succeeded, so unlike
    // lastL2W it cannot go stale while the weapon is in view.
    const WaCommon* v = &g_waCommon[w->hand];
    if (v->ok && v->present == (uint32_t)dvr::frame::count()) {
        const WaComp* self = NULL;
        const WaComp* ref  = NULL;
        for (int i = 0; i < v->componentCount; ++i) {
            const WaComp* k = &v->components[i];
            if (!k->ok) continue;
            if (k->isRef && !ref) ref = k;
            // The POINTER first - that is the instance. The asset name is a
            // fallback for a contract adopted before this field existed, and it
            // is only ever as good as "some component with this mesh".
            if (!self && w->compObj && k->obj == (uint8_t*)w->compObj) self = k;
        }
        if (!self && !w->compObj)
            for (int i = 0; i < v->componentCount && !self; ++i) {
                const WaComp* k = &v->components[i];
                if (k->ok && k->isMember && k->hand == w->hand &&
                    !strcmp(k->asset, w->asset)) self = k;
            }
        if (self) {
            dvr::hf::Xform native = {self->R, {self->t[0], self->t[1], self->t[2]}};
            dvr::hf::Xform expected = native;
            bool haveExpected = w->useNative;
            if (!w->useNative && ref) {
                dvr::hf::Xform nr = {ref->R, {ref->t[0], ref->t[1], ref->t[2]}}, br;
                if (dvr::wf::bridge(nr, v->L_hand, &br)) {
                    expected = dvr::hf::xform_mul(br, native);
                    haveExpected = true;
                }
            }
            if (haveExpected) {
                *offsetOut = dvr::wf::offset3(c2->t, expected.t);
                *refOut = dvr::wf::IREF_COMPONENT;
                InterlockedIncrement(&g_waRefComponent);
                return dvr::wf::verify_instance(dvr::wf::IREF_COMPONENT,
                                                *offsetOut, g_waPassRadiusUU);
            }
        }
    }

    // 2. WHERE THIS CONTRACT VERIFIED AS HELD A MOMENT AGO. Refreshed on every
    // verified draw, so it tracks reality rather than the matcher's schedule.
    const uint32_t now = (uint32_t)dvr::frame::count();
    if (w->heldOk &&
        (now - w->heldPresent) <= (uint32_t)(g_waHeldMaxPresents < 0 ? 0
                                                                    : g_waHeldMaxPresents)) {
        *offsetOut = dvr::wf::offset3(c2->t, w->heldAt);
        *refOut = dvr::wf::IREF_RECENT;
        InterlockedIncrement(&g_waRefRecent);
        return dvr::wf::verify_instance(dvr::wf::IREF_RECENT, *offsetOut,
                                        g_waPassRadiusUU);
    }
    return dvr::wf::INSTANCE_NO_REF;
}


// Record that this contract verified as the held item here. Both routes call it,
// which is the whole point: a reference is only useful if the path that uses it
// also maintains it.
static void WaNoteHeld(WaMesh* w, const MpDrawCtx* c2)
{
    if (!w || !c2) return;
    for (int i = 0; i < 3; ++i) w->heldAt[i] = c2->t[i];
    w->heldPresent = (uint32_t)dvr::frame::count();
    w->heldOk = true;
}

static bool WaDrawInner(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr, bool* onWeaponBuffers)
{
    if (hr) *hr = D3D_OK;
    if (!g_waOn || !dev) return false;
    InterlockedIncrement(&g_waSeen);

    // VR-59: the verdict of the instance gates, kept at function scope. A
    // refusal in the identity block is EVIDENCE, and losing it on the way to
    // the matcher is how a fired bolt got rescued by the relaxed band after
    // being correctly refused a few lines earlier.
    // STRONG means positive evidence that this draw is a world instance:
    // drawn away from where its mesh drew this frame, or belonging to a
    // weapon that is stowed. A missing reference is NOT strong - it is an
    // absence of evidence, and the normal state of a weapon just re-equipped.
    bool instStrongVeto = false;
    // The verdict itself, at function scope. It was previously only reachable
    // inside the block that computed it, and that is why a refused draw still
    // reached AttachDropUncorrected below and was eaten: the rule existed and
    // the site that needed it could not see it.
    // UNVERIFIED IS THE STARTING POINT, NOT HELD. A draw whose geometry does not
    // match the contract exactly never reaches the verification block at all -
    // a different range in a shared buffer, which is what a fired bolt and the
    // pistol both produce. Defaulting this to HELD let those draws fall into the
    // drop path unverified and be deleted, which is why the bolts were invisible
    // for a whole run rather than misplaced.
    //
    // With verification off the default stays HELD, so the lever remains a true
    // A/B against every build before it rather than a third behaviour.
    dvr::wf::Instance instVerdict = g_waVerifyInstance ? dvr::wf::INSTANCE_NO_REF
                                                       : dvr::wf::INSTANCE_HELD;

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
            if (onWeaponBuffers) *onWeaponBuffers = false;
            WaMesh* known = NULL;
            for (int i = 0; i < g_waMeshN; ++i)
                if (g_waMesh[i].vb == vb0 ||
                    (ib0 && g_waMesh[i].ib == ib0)) { known = &g_waMesh[i]; break; }
            // Whatever happens from here, this draw is a weapon's geometry.
            if (known && onWeaponBuffers) *onWeaponBuffers = true;
            if (known) {
                PcRefreshLayout(dev);
                // THE SHADER IS PART OF THE KEY, and leaving it out is what hid
                // the copy for five builds. The census showed two rows with the
                // SAME vertex buffer, index buffer, range and primitive count,
                // differing only in the vertex shader: one corrected, one left
                // where the engine drew it. Without `vs` here the second looked
                // like the contract's own draw, fell through to the transform
                // match, missed by 9.5 degrees and was counted as an ordinary
                // miss - which is why "other passes on known buffers" read 0
                // while a whole uncorrected pass was on screen.
                IDirect3DVertexShader9* vso0 = NULL; void* vs0 = NULL;
                if (SUCCEEDED(dev->GetVertexShader(&vso0)) && vso0)
                    { vs0 = vso0; vso0->Release(); }
                const bool sameContract =
                    known->vb == vb0 && known->ib == ib0 && known->vs == vs0 &&
                    known->stride == str0 && known->streamOffset == off0 &&
                    known->type == type && known->baseVertex == baseVertex &&
                    known->minIndex == minIndex && known->startIndex == startIndex &&
                    known->numVerts == numVertices && known->primCount == primCount;
                // The same GEOMETRY under a different shader is another pass of
                // an identified member. It does not need identifying again and
                // it must not be re-matched: its twin already answered the
                // question, and a transform match that disagrees is the bug,
                // not new information.
                const bool sameGeometry =
                    known->vb == vb0 && known->ib == ib0 &&
                    known->startIndex == startIndex &&
                    known->numVerts == numVertices &&
                    known->primCount == primCount;
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
                    if (g_pcLayBones < 0 && g_pcLayBonesPartial < 0) {
                        InterlockedIncrement(&g_waIdNoBone);
                        return false;
                    }
                    // Build the correction from THIS view and THIS draw's own
                    // transform rather than waiting for the twin. The copy
                    // draws FIRST in the frame - the census order shows it - so
                    // a sibling delta from this Present does not exist yet, and
                    // that is precisely why the earlier sibling-only attempt
                    // corrected nothing.
                    dvr::hf::Xform corr;
                    bool haveCorr = false;
                    if (sameGeometry && g_pcLayL2W >= 0 && g_pcLayL2W <= 252) {
                        MpDrawCtx c2 = {};
                        if (SUCCEEDED(dev->GetVertexShaderConstantF(g_pcLayL2W, c2.l2w, 4))) {
                            for (int r = 0; r < 3; ++r) {
                                c2.t[r] = c2.l2w[12+r];
                                for (int cc = 0; cc < 3; ++cc)
                                    c2.R_L.m[r*3+cc] = c2.l2w[cc*4+r];
                            }
                            // SAME BUFFERS IS NOT SAME INSTANCE. Another pass of
                            // this contract is drawn where the contract is - the
                            // census measured 0.3 uu between the uncorrected
                            // crossbow pass and its corrected twin - while a
                            // world copy of the same mesh is metres away. The
                            // twin's own position is used only to decide that,
                            // never to place anything.
                            // ONLY AGAINST A FRESH REFERENCE, AND NEVER A DEAD
                            // END. This gate rejected 43,651 draws in one run
                            // and collapsed the contract table from twelve to
                            // three: it was comparing against a twin position
                            // recorded frames earlier, and a draw it refused
                            // never reached the normal matcher, so new contracts
                            // could not form. A stale reference must be ignored
                            // rather than trusted, and a refusal here means
                            // "not another pass of this contract", which is a
                            // reason to fall through to identification - not a
                            // reason to abandon the draw.
                            // VERIFY THIS DRAW, DO NOT TRUST THE CONTRACT.
                            float vOff = 0.0f;
                            dvr::wf::InstanceRef vRef = dvr::wf::IREF_NONE;
                            dvr::wf::Instance verdict = dvr::wf::INSTANCE_HELD;
                            if (g_waVerifyInstance) {
                                verdict = WaVerifyDraw(known, &c2, &vOff, &vRef);
                                switch (verdict) {
                                case dvr::wf::INSTANCE_HELD:
                                    InterlockedIncrement(&g_waVerifiedHeld);
                                    if (vOff > g_waHeldWorst) g_waHeldWorst = vOff;
                                    WaNoteHeld(known, &c2);
                                    break;
                                case dvr::wf::INSTANCE_ELSEWHERE:
                                    InterlockedIncrement(&g_waVerifiedAway);
                                    if (vOff > g_waAwayFarthest) g_waAwayFarthest = vOff;
                                    InterlockedIncrement(&g_waOffPass);
                                    instStrongVeto = true;
                                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                        "wa/id: a draw on '%s' buffers is %.0f uu from "
                                        "where the engine puts that component (radius %.0f, "
                                        "reference %s) - a DIFFERENT INSTANCE. Handed back "
                                        "exactly as the engine drew it: not corrected, not "
                                        "suppressed. A fired bolt or a dropped item is its "
                                        "own object and its passes are its own.",
                                        known->asset, (double)vOff,
                                        (double)g_waPassRadiusUU,
                                        vRef == dvr::wf::IREF_COMPONENT
                                            ? "the component transform"
                                            : "a recent verification");
                                    break;
                                default:
                                    InterlockedIncrement(&g_waVerifyNoRef);
                                    instStrongVeto = true;
                                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                        "wa/id: cannot verify a draw on '%s' buffers - no "
                                        "component transform this Present and no recent "
                                        "verification within %d present(s). Left untouched. "
                                        "Correcting on no evidence is what dragged world "
                                        "objects onto the hand.",
                                        known->asset, g_waHeldMaxPresents);
                                    break;
                                }
                            }
                            instVerdict = verdict;
                            const bool instanceOk = dvr::wf::may_correct(verdict);
                            // A REFUSED DRAW IS NOT OURS. Handing the buffers
                            // back is what stops AttachSuppressUnplaced eating
                            // a world instance, and it is why fired bolts
                            // vanished past an angle before this.
                            if (!instanceOk && g_waVetoFrees &&
                                onWeaponBuffers && *onWeaponBuffers) {
                                *onWeaponBuffers = false;
                                InterlockedIncrement(&g_waVetoFreed);
                            }
                            if (instanceOk) {
                            const WaCommon* v2 = &g_waCommon[known->hand];
                            if (v2->ok &&
                                v2->present == (uint32_t)dvr::frame::count()) {
                                dvr::hf::Xform space = v2->D;
                                if (known->useNative) {
                                    const WaComp* ref2 = NULL;
                                    for (int q = 0; q < v2->componentCount; ++q)
                                        if (v2->components[q].ok &&
                                            v2->components[q].isRef)
                                            { ref2 = &v2->components[q]; break; }
                                    dvr::hf::Xform br, ibr;
                                    if (ref2) {
                                        dvr::hf::Xform nr = {ref2->R,
                                            {ref2->t[0], ref2->t[1], ref2->t[2]}};
                                        if (dvr::wf::bridge(nr, v2->L_hand, &br) &&
                                            dvr::wf::inverse(br, &ibr))
                                            space = dvr::hf::xform_mul(
                                                dvr::hf::xform_mul(ibr, v2->D), br);
                                        else ref2 = NULL;
                                    }
                                    if (!ref2) { InterlockedIncrement(&g_waNoBridge); return false; }
                                }
                                dvr::hf::Xform L2 = {c2.R_L,
                                    {c2.t[0], c2.t[1], c2.t[2]}}, iL2;
                                if (dvr::wf::inverse(L2, &iL2)) {
                                    corr = dvr::hf::xform_mul(
                                        dvr::hf::xform_mul(iL2, space), L2);
                                    haveCorr = true;
                                }
                            }
                            }
                        }
                    }
                    // The shared delta is for ANOTHER PASS of the held item,
                    // never for another instance of its mesh. Before VR-59 it
                    // was reached whatever the instance tests had said, which
                    // made every refusal above advisory.
                    if (!haveCorr && !instStrongVeto && known->dmOk &&
                        known->dmPresent == (uint32_t)dvr::frame::count()) {
                        corr = known->dm; haveCorr = true;
                    }
                    // NOTHING USABLE FOR THIS PASS. Two choices, and drawing it
                    // untouched is the worse one: this is another pass of a mesh
                    // the frame will also draw correctly, so leaving it alone
                    // puts a copy at the native position - which is the whole
                    // defect. The arm split faced this and dropped what it could
                    // not place rather than drawing it wrong.
                    //
                    // Suppressing costs at most a depth or shadow contribution
                    // for one pass of one mesh; drawing it costs a duplicate
                    // weapon standing in the room. The copies return when the
                    // tester MOVES, which is when a correction for this exact
                    // view is most often missing, and that is exactly this case.
                    if (!haveCorr) {
                        InterlockedIncrement(&g_waIdNoDelta);
                        // ONLY A VERIFIED PASS OF THE HELD ITEM MAY BE DROPPED.
                        // Dropping is a claim that this draw is a DUPLICATE of
                        // geometry the frame draws correctly elsewhere. That is
                        // true of another pass of the held weapon and false of a
                        // world instance, which is the only copy of itself there
                        // is - dropping it deletes the object. Fired bolts were
                        // invisible for exactly this reason: verification
                        // correctly refused them and then this line consumed the
                        // draw anyway.
                        if (!dvr::wf::may_suppress(instVerdict)) {
                            InterlockedIncrement(&g_waHandedBack);
                            if (onWeaponBuffers) *onWeaponBuffers = false;
                            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                "wa/id: handing a %s draw of '%s' back to the "
                                "engine undrawn-by-us and unsuppressed. It is not a "
                                "duplicate of anything this frame draws correctly, so "
                                "dropping it would delete the object rather than move "
                                "it.",
                                dvr::wf::instance_name(instVerdict), known->asset);
                            return false;
                        }
                        if (g_waDropUncorrected) {
                            InterlockedIncrement(&g_waIdDropped);
                            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                "wa/id: DROPPING an uncorrectable pass of '%s' - "
                                "no correction exists for this view, and drawing "
                                "it untouched would put a copy at the native "
                                "position. The corrected pass of this mesh still "
                                "draws this frame.", known->asset);
                            if (hr) *hr = D3D_OK;
                            return true;
                        }
                    }
                    else {
                    for (int q = 0; q < 9; ++q)
                        if (!MpFinite(corr.r.m[q])) return false;
                    for (int q = 0; q < 3; ++q)
                        if (!MpFinite(corr.t[q])) return false;
                    bool ok = true;
                    for (int q = 0; q < 9 && ok; ++q)
                        if (!MpFinite(corr.r.m[q])) ok = false;
                    for (int q = 0; q < 3 && ok; ++q)
                        if (!MpFinite(corr.t[q])) ok = false;
                    if (ok && g_waGhostFix &&
                        WaPatchAndDraw(dev, known, corr, true, type, baseVertex,
                                       minIndex, numVertices, startIndex, 0,
                                       primCount, hr)) {
                        InterlockedIncrement(&g_waIdCorrected);
                        InterlockedIncrement(&known->ghosts);
                        return true;
                    }
                    }
                }
            }
        }
    }

    PcRefreshLayout(dev);
    if (g_pcLayL2W < 0 || g_pcLayL2W > 252) {
        InterlockedIncrement(&g_waNoLayout);
#if DVR_WITH_LEGACY
        // Before giving up: is this another PASS of a weapon already
        // identified? That pass has no LocalToWorld to be identified by, but
        // its geometry is known and its sibling's correction is in hand.
        if (g_waGhostFix && g_waMeshN && WaPrimCountKnown(primCount) &&
            WaGhostPass(dev, type, baseVertex, minIndex, numVertices,
                        startIndex, primCount, hr))
            return true;
#endif
#if DVR_WITH_LEGACY
        // Nothing matched on the strict key. Ask what this draw actually IS,
        // because "0 ghost passes" and "the ghost is not an indexed draw of a
        // known buffer" are different answers and the first build could not
        // tell them apart.
        WaProbeRefused(dev, type, baseVertex, minIndex, numVertices,
                       startIndex, primCount);
#endif
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
            // ON THE RIG, OR NOT A MEMBER. A fired bolt keeps its name and its
            // mesh but leaves the view model, and nothing else here would tell
            // the difference - the tester found one attached to the hand and
            // hanging in the sky. Rig components sit together; this one is
            // measured against the bridge anchor we already trust.
            {
                float d = 0.0f;
                for (int q = 0; q < 3; ++q) {
                    const float e = k->t[q] - ref->t[q];
                    d += e * e;
                }
                if (sqrtf(d) > g_waRigRadiusUU) {
                    InterlockedIncrement(&g_waOffRig);
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                        "wa: '%s' is %.0f uu from the body mesh, past the %.0f uu "
                        "rig radius - it is not on the view model any more (a "
                        "fired bolt keeps its name and its mesh), so it is not a "
                        "member and will not be moved to the hand.",
                        k->asset, (double)sqrtf(d), (double)g_waRigRadiusUU);
                    continue;
                }
            }
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
    dvr::wf::Result match = dvr::wf::match(draw, candidates, count,
        g_waAngTolDeg, g_waPosTolUU, g_waMarginX);

    // ON THE VIEW MODEL? Measured, not guessed: every view-model draw in the
    // census sat within ~170 uu of the camera and the nearest world draw was
    // 1880. Inside that radius the strict band is the wrong instrument - it
    // was rejecting the sword at 11 degrees and the bolt at 12 while naming
    // both correctly - so a relaxed band picks the member instead. It is still
    // narrow enough to refuse the body mesh, which named a member at 175
    // degrees and 143 uu.
    if (match.best < 0 && count > 0) {
        const float distCam = sqrtf(draw.t[0]*draw.t[0] + draw.t[1]*draw.t[1] +
                                    draw.t[2]*draw.t[2]);
        // A STRONG INSTANCE VETO OUTRANKS THE RELAXED BAND. The identity
        // block has already established that this draw is a world instance
        // of a known mesh - drawn away from where that mesh drew this frame,
        // or belonging to a weapon that is stowed. The relaxed band exists
        // to rescue a socket-mounted member that the strict band misses, not
        // to overturn that. A bolt fired two metres away is inside the view
        // model radius on merit, which is exactly why proximity cannot be
        // the last word here.
        //
        // "No fresh reference" deliberately does NOT reach this. That is an
        // absence of evidence, and it is the normal state of a weapon just
        // re-equipped - barring the relaxed band on it would stop a
        // re-equipped sword relocking at all.
        if (g_waVetoRelaxed && instStrongVeto) {
            InterlockedIncrement(&g_waVetoedRelaxed);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                "wa: refusing the relaxed view-model band for a draw the "
                "identity block vetoed as a different INSTANCE - %.0f uu from "
                "the camera, so proximity would have accepted it. %ld so far.",
                (double)distCam, g_waVetoedRelaxed);
        } else if (distCam <= g_waViewModelUU) {
            // THE MARGIN HAS TO SHRINK WHEN THE BAND WIDENS. At 20 degrees
            // several members qualify at once, and a margin of 4x calls
            // anything within four times the winner's score a tie - so the
            // relaxed path accepted NOTHING and refused 1146 draws while the
            // census showed the winner was correct every time. A near-tie is
            // still a tie; four-to-one is not.
            const dvr::wf::Result near2 = dvr::wf::match(
                draw, candidates, count, g_waNearAngDeg, g_waNearPosUU,
                g_waNearMargin);
            if (near2.best >= 0 && !near2.ambiguous) {
                InterlockedIncrement(&g_waNearAccepted);
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                    "wa: '%s' accepted on the view model - %.2f uu from the "
                    "camera, %.3f deg / %.2f uu from its predicted transform. "
                    "Outside the strict %.2f deg band but inside the view "
                    "model's own, where proximity has already excluded every "
                    "world instance. Socket-mounted members do not track their "
                    "component transform as closely as a held one.",
                    members[near2.best]->asset, (double)distCam,
                    (double)near2.angle, (double)near2.position,
                    (double)g_waAngTolDeg);
                match = near2;
            } else {
                InterlockedIncrement(&g_waNearRejected);
            }
        }
    }
    if (match.best < 0) {
        InterlockedIncrement(&g_waNoCandidate);
        // CHARACTERISE THE MISS. The copy that stays behind animates correctly,
        // so it is this mesh under the game's own palette and it has to be in
        // here somewhere. Keep the closest one and the geometry that drew it:
        // its buffers are what the correction needs, and nothing else in this
        // build reports them.
        for (int h = 0; h < 2; ++h) {
            if (!views[h]) continue;
            dvr::wf::Result near_ = dvr::wf::match(draw, candidates, count,
                                                   1.0e9f, 1.0e9f, 1.0f);
            if (near_.best < 0) continue;
            if (candidates[near_.best].hand != h) continue;
            if (g_waMiss[h].ok && near_.score >= g_waMiss[h].score) continue;
            IDirect3DVertexBuffer9* mvb = NULL; UINT moff = 0, mstr = 0;
            IDirect3DIndexBuffer9* mib = NULL;
            IDirect3DVertexShader9* mvs = NULL;
            dev->GetStreamSource(0, &mvb, &moff, &mstr);
            dev->GetIndices(&mib); dev->GetVertexShader(&mvs);
            WaMiss m;
            m.angle = near_.angle; m.position = near_.position;
            m.scale = near_.scale; m.score = near_.score;
            m.vb = mvb; m.ib = mib; m.vs = mvs;
            m.primCount = primCount; m.numVerts = numVertices;
            m.startIndex = startIndex; m.stride = mstr; m.ok = true;
            strcpy_s(m.asset, members[near_.best]->asset);
            if (mvb) mvb->Release();
            if (mib) mib->Release();
            if (mvs) mvs->Release();
            g_waMiss[h] = m;
            WaCensusNote(dev, &ctx, baseVertex, numVertices, startIndex,
                         primCount, members[near_.best]->asset, near_.angle,
                         near_.position, false);
        }
        return false;
    }
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
        if (full) {
            for (int i = 1; i < g_waMeshN; ++i)
                if (g_waMesh[i].lastVerifyMs < g_waMesh[slot].lastVerifyMs) slot = i;
            // AN EVICTION IS A FAULT, NOT HOUSEKEEPING. The evicted contract
            // stops being recognised, its passes go uncorrected until it is
            // identified again, and its copy comes back. Say so with the age of
            // what was thrown out: a victim that drew recently means the table
            // is too small and is thrashing, which is a different problem from
            // one that has not drawn for a minute.
            const double ageMs = MaimNowMs() - g_waMesh[slot].lastVerifyMs;
            Log("wa: EVICTING '%s' (hand %d, placed %ld, last drawn %.0f ms ago) "
                "to make room - the contract table is full at %d. An eviction of "
                "a contract that drew recently is THRASHING: that mesh stops "
                "being corrected until it is identified again, and its copy "
                "reappears in the meantime.",
                g_waMesh[slot].asset, g_waMesh[slot].hand,
                g_waMesh[slot].placed, ageMs, WA_MAX_MESH);
        }
        w = &g_waMesh[slot]; memset(w, 0, sizeof(*w));
        w->vb = vb; w->ib = ib; w->decl = decl; w->vs = vs;
        w->stride = stride; w->streamOffset = offset; w->type = type;
        w->baseVertex = baseVertex; w->minIndex = minIndex; w->startIndex = startIndex;
        w->numVerts = numVertices; w->primCount = primCount; w->hand = hand;
        strcpy_s(w->asset, member->asset);
        // THE INSTANCE, not just the geometry. Every later draw on these
        // buffers is verified against this component, so a second instance of
        // the same mesh can never inherit this contract's correction.
        w->compObj = member->obj;
        InterlockedIncrement(&g_waMatched);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "wa: MATCH '%s' hand %d: %.4f deg %.4f uu scale error %.5f; c%d x%d; component snapshot %u",
            w->asset, hand, match.angle, match.position, match.scale,
            g_pcLayBones, g_pcLayBonesN, wc->componentGen);
    }
    WaCensusNote(dev, &ctx, baseVertex, numVertices, startIndex, primCount,
                 member->asset, match.angle, match.position, true);
    // Odd candidates are the world-space prediction, even the rebased one.
    w->useNative = ((match.best & 1) != 0);
    for (int q = 0; q < 3; ++q) w->lastL2W[q] = ctx.t[q];
    w->lastL2WPresent = (uint32_t)dvr::frame::count();
    w->lastL2WOk = true;
    // The matcher just identified this draw by transform, which is the strongest
    // verification there is - so it maintains the same reference the identity
    // route uses. Both paths keeping it current is the fix for a reference that
    // was refreshed 4 times in 13 million draws.
    WaNoteHeld(w, &ctx);
    if (!w->compObj) w->compObj = member->obj;
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


// THE ROUTER. Place it, or do not draw it.
//
// A draw on a weapon's own buffers that this did not place is the copy, by
// every route that reaches here - no layout, no view, no delta, no match. The
// question the tester asked settles it: there is nothing to gain by putting the
// copy in the right place when the frame already draws that geometry correctly,
// and not drawing it needs none of the machinery that placing it does.
static bool WaDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr)
{
    bool onWeapon = false;
    if (WaDrawInner(dev, type, baseVertex, minIndex, numVertices, startIndex,
                    primCount, hr, &onWeapon))
        return true;
    if (!g_waSuppressUnplaced || !onWeapon) return false;
    InterlockedIncrement(&g_waSuppressed);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "wa: SUPPRESSED a weapon draw this build did not place - %ld so far. It "
        "is a duplicate of geometry the corrected pass draws this frame, so not "
        "drawing it and placing it correctly look the same. If the weapon starts "
        "z-fighting or loses a shadow, [Hands] AttachSuppressUnplaced=0 puts it "
        "back.", g_waSuppressed);
    if (hr) *hr = D3D_OK;
    return true;
}

static void WaBeat(void)
{
    if (!g_waOn) return;
    static double said = 0;
    const double now = MaimNowMs();
    if (now - said < 5000) return;
    said = now;
#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_legacy_beat.inc"
#else
    Log("wa: routed %ld matched %ld; attempted %ld succeeded %ld restore-failed %ld | "
        "known-buffer passes %ld corrected %ld; no-bones %ld no-delta %ld | "
        "contracts %d/%d; other-instance %ld off-rig %ld; no-view %ld "
        "stale-snapshot %ld (bound %.0f ms, last used %.1f ms old); suppressed %ld "
        "pose-gen mismatches tolerated %ld | %s",
        g_waSeen, g_waMatched, g_waAttempted, g_waSucceeded, g_waRestoreFail,
        g_waIdSeen, g_waIdCorrected, g_waIdNoBone, g_waIdNoDelta,
        g_waMeshN, (int)WA_MAX_MESH, g_waOffPass, g_waOffRig, g_waNoCommon,
        g_waStaleComp, (double)g_waSnapMaxMs, (double)g_waSnapAgeAtUse,
        g_waSuppressed,
        g_waPoseGenDiff, g_waWhy);
#endif
    // VR-59: PER-DRAW INSTANCE VERIFICATION. Read these together - each one
    // says what would move it, because a bare zero here has been misread
    // before (40.1, two counters that were zero by design).
    Log("wa: instance verify (AttachVerifyInstance=%d, radius %.0f uu) - held "
        "%ld, elsewhere %ld, unverifiable %ld | reference: component %ld, recent "
        "%ld (window %d present(s)) | worst offset accepted %.1f uu, farthest "
        "refused %.1f uu | buffers handed back %ld. HELD should be the large "
        "majority while a weapon is out - if it is 0 and the weapons are flat "
        "then verification is FAILING, not idle. ELSEWHERE moves the moment a "
        "bolt or a dropped item exists in the world, and a fired bolt staying "
        "put while this climbs is the fix working. If component is 0 the "
        "engine-read route is not running at all and only the cache is holding "
        "this up, which would be a latent failure. The two offsets bracket the "
        "radius from both sides: if the worst accepted approaches the farthest "
        "refused, the radius is in the wrong place.",
        (int)g_waVerifyInstance, (double)g_waPassRadiusUU,
        g_waVerifiedHeld, g_waVerifiedAway, g_waVerifyNoRef,
        g_waRefComponent, g_waRefRecent, g_waHeldMaxPresents,
        (double)g_waHeldWorst, (double)g_waAwayFarthest, g_waVetoFreed);
    Log("wa: handed back to the engine %ld draw(s) rather than dropped "
        "(dropped-as-duplicate %ld). Dropping CLAIMS a draw is a duplicate of "
        "geometry the frame draws correctly elsewhere; that is true of another "
        "pass of the held item and false of a world instance, which is the only "
        "copy of itself there is. If handed-back is 0 while fired bolts are "
        "invisible, a refused draw is still being consumed somewhere.",
        g_waHandedBack, g_waIdDropped);
    Log("wa: legacy instance levers - no-fresh-reference %ld (Require"
        "FreshRef=%d), stowed %ld (RequireLiveMember=%d), relaxed band refused "
        "%ld. Both gates were falsified in a headset and default OFF; they are "
        "kept so the measurement stays reproducible.",
        g_waNoFreshRef, (int)g_waReqFreshRef, g_waStowed,
        (int)g_waReqLiveMember, g_waVetoedRelaxed);
    for (int h = 0; h < 2; ++h) {
        if (g_waNearestScore[h] != FLT_MAX)
            Log("wa: interval nearest hand %d '%s': %.4f deg / %.4f uu / scale "
                "%.5f (bands %.3f / %.3f / .005), component snapshot %.1f ms old "
                "at the time. If the residual tracks that age, the miss is the "
                "view model swaying under a stale snapshot rather than a bad "
                "bridge.",
                h, g_waNearestName[h], g_waNearestAngle[h], g_waNearestPos[h],
                g_waNearestScale[h], g_waAngTolDeg, g_waPosTolUU,
                (double)g_waSnapAgeAtUse);
        g_waNearestScore[h] = FLT_MAX;
    }
    for (int h = 0; h < 2; ++h) {
        if (!g_waMiss[h].ok) continue;
        Log("wa: NEAREST MISS hand %d, closest to '%s': %.3f deg / %.2f uu / "
            "scale %.5f | vb %p ib %p vs %p prim %u verts %u start %u stride %u. "
            "This is the best a NON-matching draw managed this interval. The "
            "copy that stays behind animates correctly, so it is this mesh under "
            "the game's own palette; if these buffers are stable across "
            "intervals they are what it is drawn from, and buffer identity can "
            "then correct it without ever matching a transform.",
            h, g_waMiss[h].asset, (double)g_waMiss[h].angle,
            (double)g_waMiss[h].position, (double)g_waMiss[h].scale,
            g_waMiss[h].vb, g_waMiss[h].ib, g_waMiss[h].vs,
            g_waMiss[h].primCount, g_waMiss[h].numVerts,
            g_waMiss[h].startIndex, g_waMiss[h].stride);
        g_waMiss[h].ok = false;
    }
    for (int i = 0; i < g_waMeshN; ++i) {
        const WaMesh* w = &g_waMesh[i];
        // A contract that STOPS drawing is the copy coming back, and it was
        // only ever visible as a number that stopped rising. Name the
        // transition in both directions.
        {
            WaMesh* m = &g_waMesh[i];
            const bool live = (now - m->lastVerifyMs) < 500.0;
            if (live != m->wasLive) {
                m->wasLive = live;
                Log("wa: '%s' hand %d is %s - placed %ld, ghost passes %ld, last "
                    "drawn %.0f ms ago. eye %d, corrections published this "
                    "second L %s R %s. An unlock here is the copy coming back.",
                    m->asset, m->hand, live ? ">>> LOCKED <<<" : ">>> UNLOCKED <<<",
                    m->placed, m->ghosts, now - m->lastVerifyMs, g_mpEyeState,
                    g_waCommon[0].ok ? "yes" : "no",
                    g_waCommon[1].ok ? "yes" : "no");
            }
        }
        Log("wa: contract '%s' hand %d c%d x%u placed %ld refused %ld ghost passes %ld age %.0f ms",
            w->asset, w->hand, w->boneReg, w->regs, w->placed, w->refused,
            w->ghosts, now - w->lastVerifyMs);
    }
}
