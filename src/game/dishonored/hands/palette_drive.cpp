// game/dishonored/hands/palette_drive.cpp - VR-33: the static hand, placed by
// rewriting the bone palette at the constant upload. See state chunk 56 for
// why this lane and not one of the three that are closed.

// The palette stores each bone as three float4 rows, translation in each row's
// w. Read one bone's origin.
static inline void PdBoneOrigin(const float* pal, int bone, float* out)
{
    const int b = bone * 3;
    out[0] = pal[(b + 0) * 4 + 3];
    out[1] = pal[(b + 1) * 4 + 3];
    out[2] = pal[(b + 2) * 4 + 3];
}


// M_new = D . M_old for one bone, where D is a rigid delta given as a 3x3
// rotation and a translation. Applied to the 3x4 the palette holds: the
// rotation multiplies the basis rows AND the translation column, then the
// delta's own translation is added.
static void PdApplyDelta(float* pal, int bone, const float (*R)[3], const float* T)
{
    const int b = bone * 3;
    float m[3][4];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 4; c++) m[r][c] = pal[(b + r) * 4 + c];

    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 4; c++) {
            float v = 0.0f;
            for (int k = 0; k < 3; k++) v += R[r][k] * m[k][c];
            pal[(b + r) * 4 + c] = v;
        }
        pal[(b + r) * 4 + 3] += T[r];
    }
}


// The controller's pose, delivered in the space the palette lives in, as a
// rotation and a position in unreal units. Returns false when the pose is not
// good, and the caller then leaves the game's own constants alone.
static bool PdControllerInRig(int hand, float (*R)[3], float* P)
{
    const int dev = (hand >= 0 && hand < 2) ? g_ctrlIdx[hand] : -1;
    if (dev < 0 || dev >= 16 || !g_devPoseOk[dev] || !g_devPoseOk[0]) return false;

    float (*hc)[4] = g_devPose[dev];
    float (*hm)[4] = g_devPose[0];

    // The head's yaw-flattened basis, the same construction skelcontrol uses -
    // right, up, forward - so the two lanes cannot disagree about what "in
    // front of you" means.
    float f[3] = { -hm[0][2], -hm[1][2], -hm[2][2] };
    float fl[3] = { f[0], 0.0f, f[2] };
    if (V3Norm(fl) < 0.2f) return false;
    float up[3] = { 0.0f, 1.0f, 0.0f }, r[3];
    V3Cross(fl, up, r); if (V3Norm(r) < 0.2f) return false;
    float u[3]; V3Cross(r, fl, u); V3Norm(u);

    // Position: the controller's offset from the LIVE head, in that basis, and
    // then straight into the palette's axes - which are the SAME order, so
    // there is no re-ordering at all.
    //
    // MEASURED 2026-09-06, not assumed. The first build mapped this as though
    // the palette were UE3 world axes (X forward, Y right, Z up) and the hands
    // went under the floor. The probe says otherwise: at rest the two hand
    // bones sit at L (-76, -122, +101) and R (+65, -85, +76), and in a menu
    // they are exactly symmetric at (+-31.07, 8.99, -12.50). The component
    // that SEPARATES the two hands is the first one, so palette X is LATERAL,
    // not forward. Both hands share a large negative second component and a
    // large positive third, which for hands held in front of and below the eye
    // makes the second UP and the third FORWARD.
    //
    // So the palette is (X right, Y up, Z forward) and the head basis is
    // (right, up, forward). Identity.
    const float d[3] = { hc[0][3] - hm[0][3],
                         hc[1][3] - hm[1][3],
                         hc[2][3] - hm[2][3] };
    const float ph[3] = { V3Dot(d, r), V3Dot(d, u), V3Dot(d, fl) };
    P[0] = ph[0] * g_pdScaleUU;      // right
    P[1] = ph[1] * g_pdScaleUU;      // up
    P[2] = ph[2] * g_pdScaleUU;      // forward

    // Rotation, in the same order. R[i][j] is how far the controller's j-th
    // axis lies along the palette's i-th, with both listed (right, up,
    // forward), so a controller at rest gives the identity.
    const float cf[3] = { -hc[0][2], -hc[1][2], -hc[2][2] };   // controller forward
    const float cr[3] = {  hc[0][0],  hc[1][0],  hc[2][0] };   // right
    const float cu[3] = {  hc[0][1],  hc[1][1],  hc[2][1] };   // up
    R[0][0] = V3Dot(cr, r);  R[0][1] = V3Dot(cu, r);  R[0][2] = V3Dot(cf, r);
    R[1][0] = V3Dot(cr, u);  R[1][1] = V3Dot(cu, u);  R[1][2] = V3Dot(cf, u);
    R[2][0] = V3Dot(cr, fl); R[2][1] = V3Dot(cu, fl); R[2][2] = V3Dot(cf, fl);

    for (int a = 0; a < 3; a++)
        if (!(P[a] == P[a])) return false;
    return true;
}


// Rewrite the hand bones in one palette upload. `pal` is OUR copy; the game's
// data is never touched. Returns the number of bone matrices replaced.
static int PdRewrite(float* pal, UINT count)
{
    const int nb = (int)(count / 3);
    int hit = 0;

    for (int sd = 1; sd <= 2; sd++) {
        const int href = g_msHandBone[sd];
        if (href < 0 || href >= nb) { g_pdNoBones++; continue; }

        // Side A is the LEFT hand by the split's own ordering; the knob can
        // swap them if the asset disagrees, which is a question about the
        // asset and not about this arithmetic.
        const int hand = sd - 1;

        float R[3][3], P[3];
        if (!PdControllerInRig(hand, R, P)) { g_pdNoPose++; continue; }

        // The reference the whole hand is moved FROM: the hand bone's own
        // origin, read out of the palette this very frame, so a level load or
        // an animation cannot leave it stale.
        float ref[3];
        PdBoneOrigin(pal, href, ref);

        // Rotate about the GRIP, not about the bone origin at the wrist:
        // place the result so the grip point lands on the controller. The grip
        // is in the palette's own order - right, up, forward - like everything
        // else here, so the knob and this arithmetic cannot disagree.
        const float* G = g_pdGrip[hand];
        float RG[3];
        for (int k = 0; k < 3; k++)
            RG[k] = R[k][0] * G[0] + R[k][1] * G[1] + R[k][2] * G[2];

        // D = T(P - R*G) . R . T(-ref), so T = P - R*G - R*ref.
        float Rref[3];
        for (int k = 0; k < 3; k++)
            Rref[k] = R[k][0] * ref[0] + R[k][1] * ref[1] + R[k][2] * ref[2];
        float T[3];
        for (int k = 0; k < 3; k++) T[k] = P[k] - RG[k] - Rref[k];

        // One delta, every hand bone on this side, so the hand keeps its shape
        // and moves as one piece.
        for (int b = 0; b < nb; b++) {
            if (b >= MS_MAX_BONES) break;
            if (g_msBoneSide[b] != sd || !g_msBoneHand[b]) continue;
            PdApplyDelta(pal, b, R, T);
            hit++;
        }
    }
    return hit;
}


// Capture what the palette actually says, so the space, the scale and the axis
// order stop being assumptions. Read-only: nothing here modifies the upload.
static void PdProbeCapture(const float* pal, UINT count)
{
    g_pdProbeCount = count;
    const int nb = (int)(count / 3);
    for (int sd = 1; sd <= 2; sd++) {
        const int href = g_msHandBone[sd];
        g_pdProbeOk[sd - 1] = 0;
        if (href < 0 || href >= nb) continue;
        PdBoneOrigin(pal, href, g_pdProbeOrigin[sd - 1]);
        float R[3][3], P[3];
        if (PdControllerInRig(sd - 1, R, P)) {
            memcpy(g_pdProbeWant[sd - 1], P, sizeof(P));
            g_pdProbeOk[sd - 1] = 1;
        }
    }
}


// The seam into the constant upload. Returns true when `out` holds a rewritten
// copy the caller should upload instead of the game's own.
static bool PdIntercept(UINT startReg, const float* data, UINT count, float* out)
{
    if (startReg != 6 || !data || count < 3 || count > PD_MAX_REG) return false;
    g_pdSeen++;
    g_pdLastCount = count;
    // The arms draw is the one whose palette is the size the split validated.
    // Anything else is a weapon, an NPC or a static prop and is left alone.
    if (!g_msReady) { g_pdNoSplit++; return false; }
    // Anything that is not the arms is a weapon, an NPC or a static prop.
    // Counted as nothing rather than as a refusal: it is not this draw.
    if ((int)(count / 3) != g_msBones) return false;

    if (g_pdProbe) PdProbeCapture(data, count);
    if (!g_pdOn) return false;

    memcpy(out, data, sizeof(float) * 4 * count);
    const int hit = PdRewrite(out, count);
    if (!hit) return false;
    g_pdDraws++;
    g_pdBonesHit += (uint32_t)hit;
    return true;
}


static void PdTick(void)
{
    if (g_pdToggleReq) {
        g_pdToggleReq = 0;
        g_pdOn = !g_pdOn;
        Log("pd: >>> palette hand drive %s <<< - %s", g_pdOn ? "ON" : "OFF",
            g_pdOn ? "the hand is placed at the controller by rewriting its "
                     "bone matrices at the constant upload"
                   : "the game's own palette goes through untouched, which is "
                     "the head-locked hand");
    }
    if (g_pdSpaceReq) {
        g_pdSpaceReq = 0;
        g_pdSpace = !g_pdSpace;
        Log("pd: palette space -> %s. If the hand swings with your head, this "
            "is the switch: a camera-relative palette needs the head-relative "
            "pose, a world one does not.",
            g_pdSpace ? "WORLD" : "CAMERA (the ENGINE_NOTES answer)");
    }
    if (g_pdSideReq) {
        g_pdSideReq = 0;
        g_pdSide = (g_pdSide + 1) % 3;
        Log("pd: the grip knob now moves %s",
            g_pdSide == 0 ? "BOTH hands" : g_pdSide == 1 ? "the LEFT" : "the RIGHT");
    }
    if (g_pdAxisReq) {
        g_pdAxisReq = 0;
        g_pdAxis = (g_pdAxis + 1) % 3;
        Log("pd: the grip knob now moves %s, %.2f uu a press",
            g_pdAxis == 0 ? "RIGHT (across the palm)" :
            g_pdAxis == 1 ? "UP (out of the back of the hand)"
                          : "FORWARD (out along the arm)", kPdStep);
    }
    if (g_pdNudgeReq) {
        const int d = g_pdNudgeReq; g_pdNudgeReq = 0;
        for (int s = 0; s < 2; s++) {
            if (g_pdSide && (g_pdSide - 1) != s) continue;
            g_pdGrip[s][g_pdAxis] += (d > 0 ? kPdStep : -kPdStep);
            if (g_pdGrip[s][g_pdAxis] >  60.0f) g_pdGrip[s][g_pdAxis] =  60.0f;
            if (g_pdGrip[s][g_pdAxis] < -60.0f) g_pdGrip[s][g_pdAxis] = -60.0f;
            g_pdGripSet[s] = 1;
        }
        Log("pd: grip now L (R %.2f U %.2f F %.2f) R (R %.2f U %.2f F %.2f) uu "
            "- those six are what [Hands] GripLR/LU/LF and GripRR/RU/RF take",
            g_pdGrip[0][0], g_pdGrip[0][1], g_pdGrip[0][2],
            g_pdGrip[1][0], g_pdGrip[1][1], g_pdGrip[1][2]);
    }

    const double now = MaimNowMs();
    if (g_pdProbe && now >= g_pdProbeNext) {
        g_pdProbeNext = now + 1000.0;
        if (!g_pdProbeCount) {
            Log("pd/probe: the arms palette has not arrived yet - last c6 was "
                "x%u and the split wants x%d. Until those agree nothing here "
                "can be measured, and that is a DIFFERENT fault from the "
                "transform being wrong.", g_pdLastCount, g_msBones * 3);
        } else {
            Log("pd/probe: c6 x%u | hmdYaw %+7.1f deg | cam c5 (%8.1f %8.1f "
                "%8.1f) | L bone origin (%8.2f %8.2f %8.2f) want (%8.2f %8.2f "
                "%8.2f)%s | R bone origin (%8.2f %8.2f %8.2f) want (%8.2f "
                "%8.2f %8.2f)%s. TURN ON THE SPOT: an origin that holds still "
                "is a CAMERA-relative palette, one that swings with the yaw is "
                "WORLD. MOVE ONE CONTROLLER along one real axis: whichever "
                "component of `want` follows it names the axis order.",
                g_pdProbeCount, g_hmdYaw * 57.2958f,
                g_camPosC5[0], g_camPosC5[1], g_camPosC5[2],
                g_pdProbeOrigin[0][0], g_pdProbeOrigin[0][1], g_pdProbeOrigin[0][2],
                g_pdProbeWant[0][0], g_pdProbeWant[0][1], g_pdProbeWant[0][2],
                g_pdProbeOk[0] ? "" : " (NO CONTROLLER POSE)",
                g_pdProbeOrigin[1][0], g_pdProbeOrigin[1][1], g_pdProbeOrigin[1][2],
                g_pdProbeWant[1][0], g_pdProbeWant[1][1], g_pdProbeWant[1][2],
                g_pdProbeOk[1] ? "" : " (NO CONTROLLER POSE)");
        }
    }
    if (now >= g_pdNextReport) {
        g_pdNextReport = now + 15000.0;
        Log("pd: beat - %u palette upload(s) rewritten (%u bone matrices), out "
            "of %u c6 upload(s) of any size; last c6 was x%u and the split "
            "wants x%d. Refusals: %u no split, %u no controller pose, %u no "
            "hand bone on a side. ALL ZERO with a live split means the arms "
            "draw never matched the size the split was built from, which is a "
            "different fault from the drive not working. Split ready=%d "
            "(bones %d). Drive %s, space %s.",
            g_pdDraws, g_pdBonesHit, g_pdSeen, g_pdLastCount, g_msBones * 3,
            g_pdNoSplit, g_pdNoPose, g_pdNoBones, (int)g_msReady, g_msBones,
            g_pdOn ? "ON" : "off", g_pdSpace ? "world" : "camera");
        g_pdDraws = g_pdSeen = g_pdBonesHit = 0;
        g_pdNoSplit = g_pdNoPose = g_pdNoBones = 0;
    }
}


static bool PdCommand(const char* args)
{
    if (args) {
        while (*args == ' ') args++;
        if (!strncmp(args, "on", 2))         g_pdOn = true;
        else if (!strncmp(args, "off", 3))   g_pdOn = false;
        else if (!strncmp(args, "space", 5)) g_pdSpaceReq = 1;
        else if (!strncmp(args, "scale", 5)) {
            float v = 0; if (sscanf(args + 5, "%f", &v) == 1 && v > 1.0f)
                g_pdScaleUU = v;
        }
    }
    Log("pd: status - drive %s, space %s, scale %.1f uu/m. Grip L (R %.2f U "
        "%.2f F %.2f) R (R %.2f U %.2f F %.2f). Split %s, palette wants x%d, "
        "last c6 x%u. Home cycles the grip axis, Insert/Delete nudge it, End "
        "toggles the drive, Pause picks the hand, PgUp swaps the space.",
        g_pdOn ? "ON" : "off", g_pdSpace ? "world" : "camera", g_pdScaleUU,
        g_pdGrip[0][0], g_pdGrip[0][1], g_pdGrip[0][2],
        g_pdGrip[1][0], g_pdGrip[1][1], g_pdGrip[1][2],
        g_msReady ? "ready" : "NOT READY - no bones are named yet",
        g_msBones * 3, g_pdLastCount);
    return true;
}
