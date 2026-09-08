// game/dishonored/hands/weapon_attach.cpp - included by src/mod/dishonoredvr.cpp
// (unity build). See state chunk 57b for what this is and why it exists.
//
// VR-33 W2/W3: applies the SAME rigid palette correction the hands use to the
// buffer pairs the component sweep identified as weapons.

// ---- adopting what the sweep identified -------------------------------------

// Which hand an asset name belongs in. Matched on the asset because that is
// what the sweep reports and what the component actually calls itself; the
// bolt goes wherever the crossbow goes, since it is loaded in it.
static int WaHandFor(const char* asset)
{
    if (!asset) return g_waXbowHand;
    if (strstr(asset, "sword") || strstr(asset, "Sword")) return g_waSwordHand;
    return g_waXbowHand;      // crossbow_01, bolt_01, and anything else held
}


// Called by WiReport for a component that OWNED at least one buffer pair.
// Everything it needs is already in the sweep's table.
static void WaAdopt(const char* asset, int comp)
{
    if (!g_waOn) return;
    int added = 0;
    for (int i = 0; i < g_wiSigN && g_waMeshN < WA_MAX_MESH; i++) {
        uint32_t h0, s0, h1, s1;
        if (!WiOwns(i, comp, &h0, &s0, &h1, &s1)) continue;
        bool dup = false;
        for (int m = 0; m < g_waMeshN; m++)
            if (g_waMesh[m].vb == g_wiSig[i].vb && g_waMesh[m].ib == g_wiSig[i].ib)
                dup = true;
        if (dup) continue;
        WaMesh* w = &g_waMesh[g_waMeshN++];
        memset(w, 0, sizeof(*w));
        w->vb = g_wiSig[i].vb;
        w->ib = g_wiSig[i].ib;
        w->bones = g_wiSig[i].bones;
        w->hand = WaHandFor(asset);
        _snprintf(w->asset, sizeof(w->asset), "%s", asset ? asset : "?");
        w->asset[sizeof(w->asset) - 1] = 0;
        added++;
    }
    if (added)
        Log("wa: ADOPTED '%s' - %d buffer pair(s), into the %s hand. These "
            "draws now take the same rigid palette correction the hands take, "
            "from the same target palm. Nothing was typed in and nothing was "
            "guessed: the pair is the one that stopped arriving on BOTH hides "
            "of this component and came back on BOTH shows.",
            asset, added, WaHandFor(asset) ? "RIGHT" : "LEFT");
    else if (g_waMeshN >= WA_MAX_MESH)
        Log("wa: REFUSED '%s' - the attach table is full at %d pair(s). Later "
            "components cannot be attached this run.", asset, WA_MAX_MESH);
}


// ---- the draw ---------------------------------------------------------------

// Is this draw one of the adopted weapon meshes? Buffer identity only, and the
// references are released inside this call.
static WaMesh* WaFind(IDirect3DDevice9* dev)
{
    if (!g_waOn || !g_waMeshN || !dev) return NULL;
    IDirect3DVertexBuffer9* vb = NULL; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vb, &off, &stride)) || !vb) return NULL;
    void* vbp = vb; vb->Release();
    IDirect3DIndexBuffer9* ib = NULL;
    if (FAILED(dev->GetIndices(&ib)) || !ib) return NULL;
    void* ibp = ib; ib->Release();
    for (int m = 0; m < g_waMeshN; m++)
        if (g_waMesh[m].vb == vbp && g_waMesh[m].ib == ibp) return &g_waMesh[m];
    return NULL;
}


// The rigid assembly frame of a weapon, read from its own palette. A weapon is
// a rigid body: every bone moves together, so bone 0 IS the assembly frame and
// averaging over several would only add noise. Rows are 3 float4, row-major
// 3x4, translation in .w - the same layout MpBuild composes against.
static bool WaAssemblyFrame(const float* pal, dvr::hf::Mat3* R, float* t)
{
    for (int r = 0; r < 3; r++) {
        const float* row = pal + r * 4;
        for (int c = 0; c < 3; c++) R->m[r*3 + c] = row[c];
        t[r] = row[3];
    }
    // The palette carries the engine's own uniform scale, so normalise the
    // columns before this is used as an orientation. A frame that will not
    // normalise is not a rotation and the draw is left alone.
    for (int c = 0; c < 3; c++) {
        float n = 0.0f;
        for (int r = 0; r < 3; r++) n += R->m[r*3 + c] * R->m[r*3 + c];
        n = sqrtf(n);
        if (!(n > 1.0e-4f) || n != n) return false;
        for (int r = 0; r < 3; r++) R->m[r*3 + c] /= n;
    }
    for (int i = 0; i < 9; i++) if (R->m[i] != R->m[i]) return false;
    for (int i = 0; i < 3; i++) if (t[i] != t[i]) return false;
    return true;
}


// The hook. Returns true when it has DRAWN the weapon itself (patched palette,
// draw, palette restored); false means "not ours, or refused" and the caller
// draws exactly what the game asked for.
static bool WaDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount)
{
    WaMesh* w = WaFind(dev);
    if (!w) return false;
    InterlockedIncrement(&g_waDraws);

    const int hand = w->hand;

    // THE TARGET PALM. Published by the hand path for exactly this, and it is
    // valid before either hand has drawn - which matters, because the weapon
    // can be submitted first. If the hand path has not placed this frame there
    // is no target and the weapon is drawn where the engine wanted it.
    if (!g_mpPalmTargetOk[hand]) {
        g_waWhy = "the target palm for this hand has not been built this frame "
                  "- the hand path refused, or rotation is off";
        InterlockedIncrement(&w->refused);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
            "wa: '%s' NOT attached - %s. The engine's own weapon is drawn.",
            w->asset, g_waWhy);
        return false;
    }

    // HOW MANY REGISTERS TO CORRECT. A skinned assembly reports its palette
    // size; a STATIC attachment reports none, and the crossbow body is
    // described elsewhere in this tree as exactly that (vs_const_hook.cpp:
    // "static attachments (the crossbow's body): world position in .w"). One
    // bone - three registers - is the right correction for a rigid body with
    // no palette, and it is the same arithmetic: D times the one matrix.
    UINT regs = 0;
    if (w->bones)                          regs = w->bones * 3u;
    else if (g_dcPendingBones &&
             g_dcSinceUpload < DC_REUSE_WINDOW) regs = (UINT)g_dcPendingBones * 3u;
    else                                   regs = 3u;      // static: one bone
    if (!regs || regs > WA_MAX_REGS) {
        g_waWhy = "the palette size for this mesh is out of range";
        InterlockedIncrement(&w->refused);
        return false;
    }

    // The draw's own constants: the camera basis and this mesh's LocalToWorld.
    MpDrawCtx ctx;
    if (!MpAcquireCtx(dev, &ctx)) {
        g_waWhy = ctx.why;
        InterlockedIncrement(&w->refused);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
            "wa: '%s' NOT attached - the draw's constants were refused (%s).",
            w->asset, ctx.why);
        return false;
    }

    static float src[4 * WA_MAX_REGS];
    if (FAILED(dev->GetVertexShaderConstantF(6, src, regs))) {
        g_waWhy = "the bone palette could not be read back from c6";
        InterlockedIncrement(&w->refused);
        return false;
    }

    dvr::hf::Mat3 R_src;
    float q[3];
    if (!WaAssemblyFrame(src, &R_src, q)) {
        g_waWhy = "the assembly frame in the palette is not a usable rotation";
        InterlockedIncrement(&w->refused);
        return false;
    }

    // THE SAME DELTA THE HAND TAKES, from the SAME target. This is the whole
    // attachment: D carries the assembly from where the engine put it to the
    // tracked palm, and because every bone of the palette gets the same D the
    // loaded bolt keeps its animated relationship to the stock instead of
    // being pinned separately and having its animation cancelled.
    float palmLocal[3];
    dvr::hf::Xform D =
        dvr::hf::delta_from_target(ctx.R_L, ctx.t, g_mpPalmTarget[hand], R_src, q,
                                   palmLocal);
    // THE SAME FACTOR ABOUT THE SAME PALM as the hand. That is what keeps the
    // weapon in the hand at any size: both are scaled about the grip point, so
    // neither can drift out of the other.
    if (g_mpModelScale != 1.0f)
        D = dvr::hf::scale_about(D, palmLocal, g_mpModelScale);
    for (int i = 0; i < 3; i++)
        if (!MpFinite(D.t[i])) { g_waWhy = "non-finite weapon target"; InterlockedIncrement(&w->refused); return false; }
    for (int i = 0; i < 9; i++)
        if (!MpFinite(D.r.m[i])) { g_waWhy = "non-finite weapon rotation"; InterlockedIncrement(&w->refused); return false; }

    static float buf[4 * WA_MAX_REGS];
    MpBuild(buf, src, regs, &D);
    if (FAILED(dvr::frame::orig_set_vs_const(dev, 6, buf, regs))) {
        g_waWhy = "the corrected palette could not be uploaded";
        InterlockedIncrement(&w->refused);
        return false;
    }
    dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex, numVertices,
                                  startIndex, primCount);
    // c6 is CURRENT STATE, not a one-shot. Put the game's own block back or
    // every draw after this one inherits the weapon's delta.
    dvr::frame::orig_set_vs_const(dev, 6, src, regs);

    InterlockedIncrement(&w->placed);
    g_waWhy = "placed";
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "wa: '%s' ATTACHED to the %s hand - c6 x%u (%u bones), delta "
        "(%+.1f %+.1f %+.1f) uu | %ld placed, %ld refused over %ld weapon "
        "draw(s). Both counters move only if some draws of this mesh are "
        "refused; placed at 0 with refused rising means the target palm is "
        "never ready when this mesh is submitted.",
        w->asset, hand ? "RIGHT" : "LEFT", regs, regs / 3,
        (double)D.t[0], (double)D.t[1], (double)D.t[2],
        w->placed, w->refused, g_waDraws);
    return true;
}
