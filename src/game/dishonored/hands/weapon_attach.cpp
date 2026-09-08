// game/dishonored/hands/weapon_attach.cpp - included by src/mod/dishonoredvr.cpp
// (unity build). See state chunk 57b for the design and why it changed.
//
// VR-33 W2/W3: identify the weapon's draws by a bridged full-transform match,
// then carry them through the SAME correction the hand took.

// ---- reading a native component transform -----------------------------------

// The component's own LocalToWorld, read with the SAME extraction the shader
// constant gets in MpAcquireCtx: three basis registers then a translation.
// Doing it identically on both sides is deliberate - whichever of row- or
// column-major the pair really is, the convention cancels in the comparison
// rather than being asserted from memory and silently wrong.
//
// Scale is RETAINED. It is evidence for the match, and dividing it out here
// would throw away the one quantity that separates a scaled duplicate from the
// real thing.
static bool WaReadCompXform(uint8_t* obj, dvr::hf::Mat3* R, float* t, float* scale)
{
    if (!LooksLikeObj(obj) || !RangeReadable(obj + 0x60, 0x40)) return false;
    const float* M = (const float*)(obj + 0x60);   // 0x60 / 0x70 / 0x80
    const float* T = (const float*)(obj + 0x90);
    float col[3][3];
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) col[j][i] = M[j * 4 + i];
    for (int j = 0; j < 3; j++) {
        float n = 0.0f;
        for (int i = 0; i < 3; i++) n += col[j][i] * col[j][i];
        n = sqrtf(n);
        if (!(n > 1.0e-4f) || n != n || n > 1.0e4f) return false;
        scale[j] = n;
        for (int i = 0; i < 3; i++) col[j][i] /= n;
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
    g_waCompMs = now;

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
        g_waComp[n++] = c;
    }
    g_waCompN = n;
    g_waCompGen++;

    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 10000,
        "wa/comp: %d component(s) snapshotted, generation %u. The anchor is the "
        "one marked REF; members are what may be attached.", n, g_waCompGen);
    for (int i = 0; i < n; i++)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 10000,
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

    WaCommon w;
    w.L_hand  = L;
    w.D       = dvr::hf::xform_mul(dvr::hf::xform_mul(L, D), dvr::hf::xform_inv(L));
    w.present = (uint32_t)dvr::frame::count();
    w.poseGen = c->pose.gen;
    w.eye     = g_mpEyeState;
    w.ok      = true;
    for (int i = 0; i < 9; i++) if (!MpFinite(w.D.r.m[i])) w.ok = false;
    for (int i = 0; i < 3; i++) if (!MpFinite(w.D.t[i]))   w.ok = false;
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


// ---- the match --------------------------------------------------------------

// Does this draw's contract match a cached one? Full discriminators: a buffer
// pair alone can carry several ranges and several instances.
static WaMesh* WaCached(void* vb, void* ib, void* decl, void* vs, UINT stride,
                        INT baseVertex, UINT startIndex, UINT numVerts,
                        UINT primCount)
{
    for (int m = 0; m < g_waMeshN; m++) {
        WaMesh* w = &g_waMesh[m];
        if (w->vb == vb && w->ib == ib && w->decl == decl && w->vs == vs &&
            w->stride == stride && w->baseVertex == baseVertex &&
            w->startIndex == startIndex && w->numVerts == numVerts &&
            w->primCount == primCount)
            return w;
    }
    return NULL;
}


// Compare this draw against every member component, through the bridge, and
// return the winner ONLY if it is unique by a margin.
//
// The bridge: eyeOrigin comes from the anchor component, whose draw transform
// the hand path already published. A candidate's own residual is never used to
// validate that candidate.
static int WaMatch(const MpDrawCtx* c, const WaCommon* w, int hand,
                   float* bestAng, float* bestPos)
{
    // The anchor, and the offset between the two spaces.
    int ref = -1;
    for (int i = 0; i < g_waCompN; i++)
        if (g_waComp[i].isRef && g_waComp[i].ok) { ref = i; break; }
    if (ref < 0) return -2;                    // no anchor: cannot bridge

    float eyeOrigin[3];
    for (int i = 0; i < 3; i++)
        eyeOrigin[i] = g_waComp[ref].t[i] - w->L_hand.t[i];

    int best = -1, next = -1;
    float bAng = 1.0e9f, bPos = 1.0e9f, nAng = 1.0e9f, nPos = 1.0e9f;
    for (int i = 0; i < g_waCompN; i++) {
        const WaComp* k = &g_waComp[i];
        if (!k->ok || !k->isMember) continue;
        if (k->hand != hand) continue;
        const float ang = dvr::hf::rotation_diff_deg(c->R_L, k->R);
        float d = 0.0f;
        for (int j = 0; j < 3; j++) {
            const float pred = k->t[j] - eyeOrigin[j];
            d += (pred - c->t[j]) * (pred - c->t[j]);
        }
        const float pos = sqrtf(d);
        // One scalar to rank on, with position weighted so a degree and a
        // unit are comparable at the tolerances in force.
        const float score = ang / (g_waAngTolDeg > 0.0f ? g_waAngTolDeg : 1.0f) +
                            pos / (g_waPosTolUU  > 0.0f ? g_waPosTolUU  : 1.0f);
        const float bScore = bAng / (g_waAngTolDeg > 0.0f ? g_waAngTolDeg : 1.0f) +
                             bPos / (g_waPosTolUU  > 0.0f ? g_waPosTolUU  : 1.0f);
        if (best < 0 || score < bScore) {
            next = best; nAng = bAng; nPos = bPos;
            best = i;    bAng = ang; bPos = pos;
        } else {
            const float nScore = nAng / (g_waAngTolDeg > 0.0f ? g_waAngTolDeg : 1.0f) +
                                 nPos / (g_waPosTolUU  > 0.0f ? g_waPosTolUU  : 1.0f);
            if (next < 0 || score < nScore) { next = i; nAng = ang; nPos = pos; }
        }
    }
    *bestAng = bAng; *bestPos = bPos;
    g_waBestAng = bAng; g_waBestPos = bPos;
    g_waNextAng = nAng; g_waNextPos = nPos;
    _snprintf(g_waBestName, sizeof(g_waBestName), "%s",
              best >= 0 ? g_waComp[best].asset : "none");
    g_waBestName[sizeof(g_waBestName) - 1] = 0;

    if (best < 0) return -1;
    if (bAng > g_waAngTolDeg || bPos > g_waPosTolUU) return -1;

    // UNIQUENESS. Equal rotations are common - a bolt in the crossbow's
    // channel shares one - so a winner that is not clearly better than the
    // runner-up is an ambiguity, not an identity.
    if (next >= 0) {
        const float bScore = bAng / g_waAngTolDeg + bPos / g_waPosTolUU;
        const float nScore = nAng / g_waAngTolDeg + nPos / g_waPosTolUU;
        if (nScore < bScore * g_waMarginX) {
            // Both indistinguishable AND both members of the same assembly in
            // the same hand: placement is identical either way, so an assembly
            // match is enough and no member identity is invented.
            if (g_waComp[best].hand == g_waComp[next].hand) return best;
            return -3;                          // different hands: refuse
        }
    }
    return best;
}


// ---- the draw ---------------------------------------------------------------

// Patch the member's palette with the common correction, draw it, restore.
// Returns true when it has HANDLED the draw; `hr` receives the real result of
// the draw that was submitted.
static bool WaDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr)
{
    if (hr) *hr = D3D_OK;
    if (!g_waOn || !dev) return false;
    InterlockedIncrement(&g_waSeen);

    // Cheap device state first - no reflection, no constant readback yet.
    IDirect3DVertexBuffer9* vbo = NULL; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vbo, &off, &stride)) || !vbo) return false;
    void* vb = vbo; vbo->Release();
    IDirect3DIndexBuffer9* ibo = NULL;
    if (FAILED(dev->GetIndices(&ibo)) || !ibo) return false;
    void* ib = ibo; ibo->Release();
    IDirect3DVertexDeclaration9* dclo = NULL;
    if (FAILED(dev->GetVertexDeclaration(&dclo)) || !dclo) return false;
    void* decl = dclo; dclo->Release();
    IDirect3DVertexShader9* vso = NULL;
    if (FAILED(dev->GetVertexShader(&vso)) || !vso) return false;
    void* vs = vso; vso->Release();

    WaMesh* w = WaCached(vb, ib, decl, vs, stride, baseVertex, startIndex,
                         numVertices, primCount);
    const bool cached = (w != NULL);
    if (cached) InterlockedIncrement(&g_waCacheHit);

    // THE LAYOUT OF THE SHADER ACTUALLY BOUND. MpAcquireCtx reads registers by
    // number, and those numbers belong to whichever shader was reflected last -
    // the hand's, if nobody refreshes. A reflected register is only meaningful
    // with its own shader current.
    PcRefreshLayout(dev);
    if (g_pcLayVp < 0 || g_pcLayL2W < 0) {
        if (cached) InterlockedIncrement(&g_waNoLayout);
        return false;
    }

    MpDrawCtx ctx;
    if (!MpAcquireCtx(dev, &ctx)) {
        if (cached) { InterlockedIncrement(&g_waNoLayout); g_waWhy = ctx.why; }
        return false;
    }

    const int hand = cached ? w->hand : -1;

    // Which hand's correction? For an uncached draw we do not know yet, so try
    // each hand that has a live correction for this exact view.
    const WaCommon* wc = NULL;
    int useHand = -1;
    for (int h = 0; h < 2; h++) {
        if (hand >= 0 && h != hand) continue;
        const WaCommon* cand = WaCommonFor(h, &ctx);
        if (cand) { wc = cand; useHand = h; break; }
    }
    if (!wc) {
        InterlockedIncrement(&g_waNoCommon);
        if (cached) {
            // A member drawing before its hand in this view is a real ordering
            // problem with one specific fix, so it is reported as itself and
            // not folded into a generic refusal.
            InterlockedIncrement(&g_waEarlyMember);
            g_waWhy = "no hand correction exists yet for THIS Present, pose and "
                      "eye - this member draws before the hand does";
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
                "wa: '%s' NOT attached - %s. Borrowing the previous view's "
                "correction would be a whole IPD wrong, so the native draw is "
                "left alone. The fix is a same-generation hand source at an "
                "earlier boundary, not a looser check.", w->asset, g_waWhy);
        }
        return false;
    }

    // Identify, if this contract is not already known.
    if (!cached) {
        static uint32_t tryPresent = 0; static int tries = 0;
        const uint32_t pres = (uint32_t)dvr::frame::count();
        if (pres != tryPresent) { tryPresent = pres; tries = 0; }
        if (++tries > g_waMaxTry) return false;
        if (g_waMeshN >= WA_MAX_MESH) return false;

        InterlockedIncrement(&g_waCandChecked);
        float ang = 0.0f, pos = 0.0f;
        const int comp = WaMatch(&ctx, wc, useHand, &ang, &pos);
        if (comp == -3) {
            InterlockedIncrement(&g_waAmbiguous);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                "wa: AMBIGUOUS - best '%s' at %.3f deg / %.2f uu, runner-up at "
                "%.3f deg / %.2f uu, and they are in different hands. Refusing: "
                "a transform match that cannot separate two owners is not an "
                "identity.", g_waBestName, (double)g_waBestAng,
                (double)g_waBestPos, (double)g_waNextAng, (double)g_waNextPos);
            return false;
        }
        if (comp < 0) {
            InterlockedIncrement(&g_waNoCandidate);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 5000,
                "wa: no candidate - closest was '%s' at %.3f deg / %.2f uu "
                "against bands of %.2f deg / %.2f uu.", g_waBestName,
                (double)g_waBestAng, (double)g_waBestPos,
                (double)g_waAngTolDeg, (double)g_waPosTolUU);
            return false;
        }
        w = &g_waMesh[g_waMeshN++];
        memset(w, 0, sizeof(*w));
        w->vb = vb; w->ib = ib; w->decl = decl; w->vs = vs;
        w->stride = stride; w->baseVertex = baseVertex;
        w->startIndex = startIndex; w->numVerts = numVertices;
        w->primCount = primCount;
        w->comp = comp; w->hand = useHand;
        w->boneReg = g_pcLayBones;
        w->lastVerifyMs = MaimNowMs();
        _snprintf(w->asset, sizeof(w->asset), "%s", g_waComp[comp].asset);
        w->asset[sizeof(w->asset) - 1] = 0;
        InterlockedIncrement(&g_waMatched);
        Log("wa: MATCHED '%s' to a draw - %.3f deg / %.2f uu from its predicted "
            "transform, runner-up %.3f deg / %.2f uu. Bridge from the body "
            "mesh, not from this candidate's own residual. Contract: vb %p ib "
            "%p decl %p vs %p stride %u base %d start %u verts %u prim %u, "
            "bones at c%d. Into the %s hand.",
            w->asset, (double)g_waBestAng, (double)g_waBestPos,
            (double)g_waNextAng, (double)g_waNextPos, vb, ib, decl, vs, stride,
            baseVertex, startIndex, numVertices, primCount, w->boneReg,
            useHand ? "RIGHT" : "LEFT");
    }

    // A SKINNED path only. The static case needs its own component transform
    // and its dependent constants handled, which is a different backend; it is
    // refused BY NAME rather than pretended to be a one-bone palette.
    if (w->boneReg < 0) {
        InterlockedIncrement(&g_waNoSource);
        g_waWhy = "this shader declares no bone matrices - it is an unskinned "
                  "path and this build does not modify those";
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "wa: '%s' NOT attached - %s. Treating three registers as a one-bone "
            "palette, as the previous build did, would corrupt whatever those "
            "registers really are.", w->asset, g_waWhy);
        return false;
    }

    // HOW MANY REGISTERS. Bounded by the device and by our buffer; the size of
    // the last c6 write is not a palette size and is not consulted.
    UINT regs = WA_MAX_REGS;
    if ((UINT)w->boneReg + regs > 256u) regs = 256u - (UINT)w->boneReg;
    if (w->regs) regs = w->regs;
    if (!regs || regs > WA_MAX_REGS) {
        InterlockedIncrement(&g_waNoSource);
        g_waWhy = "the bone register range does not fit the device limit";
        return false;
    }

    static float src[4 * WA_MAX_REGS];
    if (FAILED(dev->GetVertexShaderConstantF((UINT)w->boneReg, src, regs))) {
        InterlockedIncrement(&g_waNoSource);
        g_waWhy = "the bone block could not be read back";
        InterlockedIncrement(&w->refused);
        return false;
    }
    w->regs = regs;

    // THE CORRECTION. Conjugate the common transform into this member's own
    // local space and compose it onto every matrix in the block. No weapon
    // pivot, no grip solve, no per-member target: the whole assembly moves
    // through one transform, so the game's own hand-to-weapon and
    // weapon-to-bolt relationships - and the bolt's internal animation -
    // survive untouched.
    dvr::hf::Xform L;
    L.r = ctx.R_L;
    for (int i = 0; i < 3; i++) L.t[i] = ctx.t[i];
    const dvr::hf::Xform Dm =
        dvr::hf::xform_mul(dvr::hf::xform_mul(dvr::hf::xform_inv(L), wc->D), L);
    for (int i = 0; i < 9; i++)
        if (!MpFinite(Dm.r.m[i])) { InterlockedIncrement(&w->refused); return false; }
    for (int i = 0; i < 3; i++)
        if (!MpFinite(Dm.t[i]))   { InterlockedIncrement(&w->refused); return false; }

    static float buf[4 * WA_MAX_REGS];
    MpBuild(buf, src, regs, &Dm);

    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)w->boneReg, buf, regs))) {
        InterlockedIncrement(&g_waNoSource);
        g_waWhy = "the corrected block could not be uploaded";
        InterlockedIncrement(&w->refused);
        return false;
    }

    InterlockedIncrement(&g_waAttempted);
    const HRESULT drawHr = dvr::frame::orig_draw_indexed(
        dev, type, baseVertex, minIndex, numVertices, startIndex, primCount);
    if (hr) *hr = drawHr;
    if (SUCCEEDED(drawHr)) { InterlockedIncrement(&g_waSucceeded); InterlockedIncrement(&w->placed); }

    // RESTORE ON EVERY EXIT. A constant is current device state, so the game's
    // own block goes back to the EXACT values in force before this draw or
    // every later draw inherits the correction. A failure here is louder than
    // the draw failing.
    if (FAILED(dvr::frame::orig_set_vs_const(dev, (UINT)w->boneReg, src, regs))) {
        InterlockedIncrement(&g_waRestoreFail);
        Log("wa: RESTORE FAILED after '%s' - c%d x%u was not put back. Later "
            "draws in this frame may inherit the weapon's correction.",
            w->asset, w->boneReg, regs);
    }

    g_waWhy = "placed";
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "wa: '%s' ATTACHED to the %s hand - c%d x%u, delta (%+.1f %+.1f %+.1f) "
        "uu | placed %ld refused %ld | routed %ld, matched %ld, cache hits %ld, "
        "ambiguous %ld, no-candidate %ld, no-correction %ld, early %ld, "
        "attempted %ld, succeeded %ld, restore failures %ld.",
        w->asset, w->hand ? "RIGHT" : "LEFT", w->boneReg, regs,
        (double)Dm.t[0], (double)Dm.t[1], (double)Dm.t[2],
        w->placed, w->refused, g_waSeen, g_waMatched, g_waCacheHit,
        g_waAmbiguous, g_waNoCandidate, g_waNoCommon, g_waEarlyMember,
        g_waAttempted, g_waSucceeded, g_waRestoreFail);
    return true;
}


// The heartbeat. It must print even when nothing has ever matched, because
// "nothing attached" and "never entered" produced identical silence for three
// runs and that is what made them unreadable.
static void WaBeat(void)
{
    if (!g_waOn) return;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "wa: beat - %d component(s) known (gen %u), %d contract(s) matched | "
        "routed %ld | no correction for the view %ld (of which members drawing "
        "early %ld) | compared %ld -> matched %ld, ambiguous %ld, none %ld | "
        "cache hits %ld | no layout %ld, no source %ld | attempted %ld, "
        "succeeded %ld, restore failed %ld | closest so far '%s' at %.3f deg / "
        "%.2f uu against bands %.2f / %.2f. Routed rising with compared at 0 "
        "means no hand correction is reaching the weapon's view; compared "
        "rising with matched at 0 means the bridge or the tolerances are wrong.",
        g_waCompN, g_waCompGen, g_waMeshN, g_waSeen, g_waNoCommon,
        g_waEarlyMember, g_waCandChecked, g_waMatched, g_waAmbiguous,
        g_waNoCandidate, g_waCacheHit, g_waNoLayout, g_waNoSource,
        g_waAttempted, g_waSucceeded, g_waRestoreFail, g_waBestName,
        (double)g_waBestAng, (double)g_waBestPos,
        (double)g_waAngTolDeg, (double)g_waPosTolUU);
}
