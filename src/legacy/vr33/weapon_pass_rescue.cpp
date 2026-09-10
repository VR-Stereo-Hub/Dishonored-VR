// Superseded VR-33 cross-pass and stale rescue router.
// Kept for comparison; no shipping hook calls these functions.
static bool WaRetiredDrawPrim(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type,
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
    if (!WaPatchAndDraw(dev, w, w->dm, w->dm, false, type, 0, 0, 0, 0,
                        startVertex, primCount, hr)) return false;
    InterlockedIncrement(&g_waPrimFixed);
    return true;
}




static bool WaRetiredDrawInner(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr, bool* onWeaponBuffers,
                   WaMesh** knownOut)
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
            if (onWeaponBuffers) *onWeaponBuffers = false;
            WaMesh* known = NULL;
            for (int i = 0; i < g_waMeshN; ++i)
                if (g_waMesh[i].vb == vb0 ||
                    (ib0 && g_waMesh[i].ib == ib0)) { known = &g_waMesh[i]; break; }
            // Whatever happens from here, this draw is a weapon's geometry.
            if (known && onWeaponBuffers) *onWeaponBuffers = true;
            if (known && knownOut) *knownOut = known;
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
                    // The correction in its OWN space, kept beside the member
                    // -local delta so a later reuse can re-conjugate rather
                    // than replay - replaying is what misplaced the copy.
                    dvr::hf::Xform corrSpace;
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
                            const uint32_t nowPres = (uint32_t)dvr::frame::count();
                            bool instanceOk = true;
                            if (known->lastL2WOk &&
                                nowPres - known->lastL2WPresent <= 2u) {
                                float d = 0.0f;
                                for (int q = 0; q < 3; ++q) {
                                    const float e = c2.t[q] - known->lastL2W[q];
                                    d += e * e;
                                }
                                if (sqrtf(d) > g_waPassRadiusUU) {
                                    instanceOk = false;
                                    InterlockedIncrement(&g_waOffPass);
                                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                        "wa/id: a draw on '%s' buffers is %.0f uu "
                                        "from where that mesh was drawn this "
                                        "frame - a different INSTANCE, not another "
                                        "pass. Falling through to identification "
                                        "rather than dropping it.",
                                        known->asset, (double)sqrtf(d));
                                }
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
                                    corrSpace = space;
                                    haveCorr = true;
                                }
                            }
                            }
                        }
                    }
                    if (!haveCorr && known->dmOk &&
                        known->dmPresent == (uint32_t)dvr::frame::count()) {
                        corr = known->dm; corrSpace = known->dm; haveCorr = true;
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
                        WaPatchAndDraw(dev, known, corr, corrSpace, true, type, baseVertex,
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
        if (distCam <= g_waViewModelUU) {
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
    const dvr::hf::Xform spaceCorrection = corrections[match.best];
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
    if (SUCCEEDED(drawHr)) {
        InterlockedIncrement(&g_waSucceeded); InterlockedIncrement(&w->placed);
        // Remember the SPACE-level correction, not the member-local delta. A
        // delta belongs to the draw it was built for; the correction belongs to
        // the view and can be re-conjugated into any pass's own space.
        const int ei = (g_mpEyeState > 0) ? 1 : 0;
        w->lastGood[ei] = spaceCorrection;
        w->lastGoodPresent[ei] = (uint32_t)dvr::frame::count();
        w->lastGoodOk[ei] = true;
    }
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
static bool WaRetiredDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex,
                   UINT primCount, HRESULT* hr)
{
    bool onWeapon = false;
    WaMesh* known = NULL;
    if (WaRetiredDrawInner(dev, type, baseVertex, minIndex, numVertices, startIndex,
                    primCount, hr, &onWeapon, &known))
        return true;
    if (!g_waSuppressUnplaced || !onWeapon) return false;

    // BEFORE SUPPRESSING, TRY THE LAST CORRECTION THAT WORKED. Suppression
    // removed the copy and left a one-frame hole in its place: on a frame where
    // every pass fails placement, every pass is suppressed and the weapon is
    // not drawn at all. That is the flicker - the hands do not have it because
    // they are drawn by the split, which never suppresses.
    //
    // A correction one frame old is imperceptible on a held object. A missing
    // weapon is not. The reuse is bounded in frames and matched on EYE, so it
    // can be slightly stale but never a whole IPD wrong.
    if (known) {
        const int ei = (g_mpEyeState > 0) ? 1 : 0;
        const uint32_t now = (uint32_t)dvr::frame::count();
        // RE-CONJUGATE, NEVER REPLAY. The stored value is the correction in its
        // own space; the delta that reaches the palette must be built from THIS
        // draw's LocalToWorld. Replaying another pass's member-local delta is
        // what put the geometry on the weapon but slightly off in size and
        // alignment - the shadow that appeared to be locked to the weapon.
        dvr::hf::Xform reuse;
        bool haveReuse = false;
        if (known->lastGoodOk[ei] &&
            now - known->lastGoodPresent[ei] <= (uint32_t)g_waReuseMaxFrames &&
            (g_pcLayBones >= 0 || g_pcLayBonesPartial >= 0) &&
            g_pcLayL2W >= 0 && g_pcLayL2W <= 252) {
            float l2w[16];
            if (SUCCEEDED(dev->GetVertexShaderConstantF(g_pcLayL2W, l2w, 4))) {
                dvr::hf::Xform L;
                for (int r = 0; r < 3; ++r) {
                    L.t[r] = l2w[12 + r];
                    for (int cc = 0; cc < 3; ++cc) L.r.m[r*3+cc] = l2w[cc*4+r];
                }
                dvr::hf::Xform iL;
                if (dvr::wf::inverse(L, &iL)) {
                    reuse = dvr::hf::xform_mul(
                        dvr::hf::xform_mul(iL, known->lastGood[ei]), L);
                    haveReuse = true;
                    for (int q = 0; q < 9 && haveReuse; ++q)
                        if (!MpFinite(reuse.r.m[q])) haveReuse = false;
                    for (int q = 0; q < 3 && haveReuse; ++q)
                        if (!MpFinite(reuse.t[q])) haveReuse = false;
                }
            }
        }
        // ONLY IF NOTHING HAS DRAWN THIS MESH THIS FRAME, AND ONLY ONCE.
        if (known->drewPresent == now || known->rescuePresent == now) {
            haveReuse = false;
            InterlockedIncrement(&g_waSuppressedExtra);
        }
        if (haveReuse) {
            for (int q = 0; q < g_waMeshN; ++q)
                if (g_waMesh[q].vb == known->vb && g_waMesh[q].ib == known->ib)
                    g_waMesh[q].rescuePresent = now;
            if (WaPatchAndDraw(dev, known, reuse, known->lastGood[ei], true, type,
                               baseVertex, minIndex, numVertices, startIndex, 0,
                               primCount, hr)) {
                InterlockedIncrement(&g_waReusedLastGood);
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                    "wa: drew '%s' from the last correction that worked, %u "
                    "frame(s) old on this eye - %ld times so far. This frame's "
                    "correction was unavailable, and a slightly stale weapon is "
                    "better than a missing one.",
                    known->asset, now - known->lastGoodPresent[ei],
                    g_waReusedLastGood);
                return true;
            }
        }
    }
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

