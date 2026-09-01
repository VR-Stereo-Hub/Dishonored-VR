// legacy/rtd_drive.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).





// ============================================================================
// 30.70 - RENDER-TIME DRIVE: build this frame's rigid transform.
//
// Runs on the submit thread, right after the poses land, ~90 Hz. It reads
// NOTHING from the game: pure controller-vs-head pose maths, published for the
// render thread. That is deliberate - the whole reason the old drive fought the
// head is that it went through game state that the engine recomputes.
//
// Frames:
//   HEAD space  = {right, up, forward}, roll removed (the game camera has no
//                 roll either) - identical construction to HandRelFull, so the
//                 two agree by definition.
//   COMPONENT   = UE3 rig-local: X forward, Y right, Z up. The map from head
//                 space is the cyclic permutation (X,Y,Z) <- (fwd,right,up),
//                 which WeaponAttachPrepare already measured and uses. Being a
//                 CYCLIC permutation it is a proper rotation (det +1), so it
//                 conjugates rotations without flipping handedness.
//
// Position is head-RELATIVE on purpose. In the old (component-transform) drive
// that was wrong and cost us weeks - head motion read as hand motion, hence the
// room/body-anchor machinery. Here it is exactly right: the rig we are writing
// lives in a space that is itself glued to the camera, so head-relative IS the
// cancellation. Lean and the presented image leans with LeanVP while the
// head-relative hand offset changes by the opposite amount - net zero, the hand
// stays put in the room.
// ============================================================================
static void RtdBuildYPR(const float* ypr, float* out)   // Z yaw, Y pitch, X roll
{
    float cy = cosf(ypr[0]), sy = sinf(ypr[0]);
    float cp = cosf(ypr[1]), sp = sinf(ypr[1]);
    float cr = cosf(ypr[2]), sr = sinf(ypr[2]);
    float Rz[9] = {  cy, -sy, 0,   sy,  cy, 0,    0,  0, 1 };
    float Ry[9] = {  cp,  0, sp,    0,   1, 0,  -sp,  0, cp };
    float Rx[9] = {   1,  0,  0,    0,  cr,-sr,   0, sr, cr };
    float t[9]; M3Mul(Rz, Ry, t); M3Mul(t, Rx, out);
}


static void RtDriveOneHand(int hand, float (*hm)[4])
{
    g_rtdHandOk[hand] = false;
    int dev = g_ctrlIdx[hand];
    if (dev < 0 || dev >= 16 || !g_devPoseOk[dev]) return;
    float (*hc)[4] = g_devPose[dev];

    // The frame this drive projects through. Roll is always dropped (the game
    // camera has no roll). Yaw and pitch are BLENDED between the neutral pose
    // and the live head pose - see the Follow globals: 1 = the frame rides the
    // head (rig assumed camera-aligned), 0 = the frame stays where it was at
    // neutral (rig assumed world-aligned). This is the knob that decides which
    // of the two spaces the game actually uses, by nulling the swim.
    float fwdRaw[3] = { -hm[0][2], -hm[1][2], -hm[2][2] };
    if (V3Norm(fwdRaw) < 0.5f) return;
    float yawNow = atan2f(fwdRaw[0], -fwdRaw[2]);
    float fyc = fwdRaw[1] < -1.f ? -1.f : (fwdRaw[1] > 1.f ? 1.f : fwdRaw[1]);
    float pitchNow = asinf(fyc);

    if (!g_rtdHaveNeutral[hand]) { g_rtdYaw0[hand] = yawNow; g_rtdPitch0[hand] = pitchNow; }

    float dyaw = yawNow - g_rtdYaw0[hand];
    while (dyaw >  3.14159265f) dyaw -= 6.28318531f;   // shortest way round
    while (dyaw < -3.14159265f) dyaw += 6.28318531f;
    float yawEff   = g_rtdYaw0[hand]   + g_rtdFollowYaw   * dyaw;
    // MEASURED 30.72: the rig's space follows camera YAW (bone0 was invariant
    // across a 74 deg turn) but does NOT pitch - looking up/down re-poses the
    // arms by animation instead, which is why bone0.y swung -19 -> -127 over
    // 55 deg of pitch. So the frame this drive projects through has to be
    // LEVEL. FollowPitch now blends toward level (0), not toward the neutral
    // pitch; the old version could never reach level unless you happened to
    // capture neutral looking exactly at the horizon, which is why sweeping it
    // "did nothing".
    float pitchEff = g_rtdFollowPitch * pitchNow;
    if (pitchEff >  1.5f) pitchEff =  1.5f;
    if (pitchEff < -1.5f) pitchEff = -1.5f;

    float cp = cosf(pitchEff);
    float fwdT[3] = { sinf(yawEff) * cp, sinf(pitchEff), -cosf(yawEff) * cp };
    if (V3Norm(fwdT) < 0.5f) return;
    float upW[3] = { 0, 1, 0 };
    float rightT[3]; V3Cross(fwdT, upW, rightT);
    if (V3Norm(rightT) < 0.2f) return;
    float upT[3]; V3Cross(rightT, fwdT, upT); V3Norm(upT);

    float H[9] = { rightT[0], rightT[1], rightT[2],
                   upT[0],    upT[1],    upT[2],
                   fwdT[0],   fwdT[1],   fwdT[2] };   // rows: v_head = H * v_room

    float C[9] = { hc[0][0], hc[0][1], hc[0][2],
                   hc[1][0], hc[1][1], hc[1][2],
                   hc[2][0], hc[2][1], hc[2][2] };

    float d[3] = { hc[0][3]-hm[0][3], hc[1][3]-hm[1][3], hc[2][3]-hm[2][3] };
    float ph[3] = { V3Dot(d, rightT), V3Dot(d, upT), V3Dot(d, fwdT) };

    if (!g_rtdHaveNeutral[hand]) {
        g_rtdYaw0[hand] = yawNow; g_rtdPitch0[hand] = pitchNow;
        float Q0[9]; M3Mul(H, C, Q0);           // neutral pose in HEAD coords
        memcpy(g_rtdC0[hand], Q0, sizeof(g_rtdC0[hand]));
        memcpy(g_rtdP0[hand], ph, sizeof(g_rtdP0[hand]));
        g_rtdHaveNeutral[hand] = true; g_rtdSmOk[hand] = false;
        float uu0 = (g_rtdScaleUU > 1.0f) ? g_rtdScaleUU : g_posScaleUU;
        if (uu0 < 1.0f) uu0 = 50.0f;
        Log("handrt: %s neutral captured (controller %d) - hand at "
            "(%.3f,%.3f,%.3f) m head space = rig (fwd %.0f, right %.0f, up %.0f) uu",
            hand ? "RIGHT" : "LEFT", dev, ph[0], ph[1], ph[2],
            ph[2]*uu0, ph[0]*uu0, ph[1]*uu0);
    }

    // 30.75 - THE ORIENTATION FIX, and an admission: I broke this in 30.70.
    //
    // The constant dump shows there is NO world matrix for the arms - only c0
    // (view-projection), c4 and c5 (camera position) are in effect at the draw.
    // The rig is drawn in CAMERA space by construction, which is normal for a
    // first-person view model. So its rest orientation is camera-locked, and a
    // drive that only adds a DELTA on top leaves that rest orientation riding
    // the head. Which is precisely what you see: the sword keeps pointing
    // wherever you look, and no amount of trim touches it, because trim is a
    // translation.
    //
    // Wanted:  drawn_room = D_room * (rest orientation AT NEUTRAL)
    //          drawn_room = H' * D_comp * B     and rest_at_neutral = H0' * B
    //   =>     D_comp = H * D_room * H0'
    // The neutral frame H0 on the right is the counter-rotation that unsticks
    // it from the head. Storing the neutral controller pose in HEAD coordinates
    // gives exactly that for free: (H*C) * (H0*C0)'.
    //
    // That was the original pre-30.70 formulation. I replaced it with the
    // symmetric H*D*H', "verified" the change against a harness whose success
    // test asked whether the DELTA was head-free rather than whether the drawn
    // ORIENTATION was, and shipped the regression four builds running.
    float Q[9]; M3Mul(H, C, Q);                 // controller axes in head coords
    float Q0T[9]; M3T(g_rtdC0[hand], Q0T);      // g_rtdC0 now holds H0*C0
    float dR[9]; M3Mul(Q, Q0T, dR);
    if (g_rtdRotInvert) { float t[9]; M3T(dR, t); memcpy(dR, t, sizeof(dR)); }
    if (g_rtdRotScale < 0.999f) {          // fade toward identity (0 = translation only)
        float k = g_rtdRotScale < 0.0f ? 0.0f : g_rtdRotScale;
        static const float I[9] = { 1,0,0, 0,1,0, 0,0,1 };
        for (int n = 0; n < 9; n++) dR[n] = I[n] + k * (dR[n] - I[n]);
    }
    float dp[3] = { ph[0]-g_rtdP0[hand][0], ph[1]-g_rtdP0[hand][1], ph[2]-g_rtdP0[hand][2] };

    if (g_rtdLockTest) {
        // controllers out of the loop entirely: pure yaw counter-rotation
        // about the head-space up axis, which is head coords index 1
        float th = g_rtdLockGain * dyaw;
        float cth = cosf(th), sth = sinf(th);
        float R[9] = { cth, 0.0f, sth,
                       0.0f, 1.0f, 0.0f,
                      -sth, 0.0f, cth };
        memcpy(dR, R, sizeof(dR));
        dp[0] = dp[1] = dp[2] = 0.0f;
    }
    M3OrthoRows(dR);                       // a rotation must stay a rotation

    if (g_rtdSmooth > 0.001f) {            // optional EMA, for shaky hands
        float a = g_rtdSmooth; if (a > 0.95f) a = 0.95f;
        if (!g_rtdSmOk[hand]) {
            memcpy(g_rtdSmR[hand], dR, sizeof(dR));
            memcpy(g_rtdSmP[hand], dp, sizeof(dp));
            g_rtdSmOk[hand] = true;
        }
        for (int k = 0; k < 9; k++) g_rtdSmR[hand][k] += (1.0f-a) * (dR[k] - g_rtdSmR[hand][k]);
        for (int k = 0; k < 3; k++) g_rtdSmP[hand][k] += (1.0f-a) * (dp[k] - g_rtdSmP[hand][k]);
        M3OrthoRows(g_rtdSmR[hand]);
        memcpy(dR, g_rtdSmR[hand], sizeof(dR));
        memcpy(dp, g_rtdSmP[hand], sizeof(dp));
    }

    // head space -> component space. The old fixed permutation (X,Y,Z) <-
    // (fwd,right,up) is now in doubt: under a head-PITCH sweep the component
    // index that stayed invariant was 0, and pitch leaves the LATERAL axis
    // invariant - so index 0 looks like sideways, not forward. A wrong map
    // sends "push forward" out sideways, which would feel exactly as broken as
    // reported. Rather than swap one guess for another it is now data: the
    // overlay's nudge buttons show which way each index actually goes.
    const int* m = g_rtdMapSrc;
    const float* sg = g_rtdMapSgn;
    float Rc[9];
    for (int a = 0; a < 3; a++)
        for (int b = 0; b < 3; b++) Rc[a*3+b] = sg[a] * sg[b] * dR[m[a]*3 + m[b]];

    float uu = (g_rtdScaleUU > 1.0f) ? g_rtdScaleUU : g_posScaleUU;
    if (uu < 1.0f) uu = 50.0f;
    float tD[3], piv[3];
    for (int a = 0; a < 3; a++) {
        float v = sg[a] * dp[m[a]] * uu;
        if (v >  g_rtdPosMax) v =  g_rtdPosMax;
        if (v < -g_rtdPosMax) v = -g_rtdPosMax;
        tD[a]  = v;
        piv[a] = sg[a] * g_rtdP0[hand][m[a]] * uu * g_rtdPivotMix;
    }
    piv[2] += g_rtdPivotUp;                // rig origin may not be at the eye
    float tc[3];
    for (int a = 0; a < 3; a++)
        tc[a] = piv[a] - (Rc[a*3+0]*piv[0] + Rc[a*3+1]*piv[1] + Rc[a*3+2]*piv[2])
              + tD[a] + g_rtdTrim[hand][a] + g_rtdNudge[a];

    // Weapon rigs are separate components with their own LocalToWorld, so if
    // their axes are rotated relative to the arms' the shared transform pulls
    // them apart. Constant basis change, tunable live, identity by default.
    float Rw[9], tw[3];
    if (g_rtdWpnYPR[0] != 0.0f || g_rtdWpnYPR[1] != 0.0f || g_rtdWpnYPR[2] != 0.0f) {
        float rad[3] = { g_rtdWpnYPR[0]*0.01745329f, g_rtdWpnYPR[1]*0.01745329f,
                         g_rtdWpnYPR[2]*0.01745329f };
        float B[9], BT[9], t1[9];
        RtdBuildYPR(rad, B); M3T(B, BT);
        M3Mul(B, Rc, t1); M3Mul(t1, BT, Rw);
        for (int a = 0; a < 3; a++)
            tw[a] = B[a*3+0]*tc[0] + B[a*3+1]*tc[1] + B[a*3+2]*tc[2];
    } else {
        memcpy(Rw, Rc, sizeof(Rw)); memcpy(tw, tc, sizeof(tw));
    }

    for (int k = 0; k < 9; k++) if (!(Rc[k] == Rc[k]) || !(Rw[k] == Rw[k])) return;
    for (int k = 0; k < 3; k++) if (!(tc[k] == tc[k]) || !(tw[k] == tw[k])) return;

    memcpy(g_rtdR[hand],  Rc, sizeof(g_rtdR[hand]));
    memcpy(g_rtdT[hand],  tc, sizeof(g_rtdT[hand]));
    memcpy(g_rtdRw[hand], Rw, sizeof(g_rtdRw[hand]));
    memcpy(g_rtdTw[hand], tw, sizeof(g_rtdTw[hand]));
    g_rtdHandOk[hand] = true;
}


static void RtDriveUpdate()
{
    // HOME (g_handMesh) stays the master hands switch, so one key still turns
    // the whole feature off; the ini/overlay flag only chooses WHICH drive runs.
    if (!g_rtdEnable || !g_handMesh) { g_rtdOn = false; return; }
    if (!g_devPoseOk[0]) { g_rtdOn = false; return; }
    float (*hm)[4] = g_devPose[0];

    LONG req = InterlockedExchange(&g_rtdReq, 0);
    if (req & 1) g_rtdHaveNeutral[0] = false;
    if (req & 2) g_rtdHaveNeutral[1] = false;

    InterlockedIncrement(&g_rtdSeq);                   // odd: write in flight
    RtDriveOneHand(0, hm);
    RtDriveOneHand(1, hm);
    InterlockedIncrement(&g_rtdSeq);                   // even: stable
    g_rtdOn = (g_rtdHandOk[0] || g_rtdHandOk[1]);
}


// Render-thread reader. A torn read is simply skipped and the last consistent
// snapshot reused, so the worst case is one frame of staleness instead of a
// bone matrix built from half of two poses.
static bool RtdSnapshot(float* R, float* T, int hand, bool weapon)
{
    static float cR[2][2][9], cT[2][2][3];
    static bool  cOk[2][2] = { { false, false }, { false, false } };
    if (hand < 0 || hand > 1) return false;
    int s = weapon ? 1 : 0;
    LONG a = g_rtdSeq;
    if (!(a & 1) && g_rtdHandOk[hand]) {
        float r[9], t[3];
        memcpy(r, weapon ? g_rtdRw[hand] : g_rtdR[hand], sizeof(r));
        memcpy(t, weapon ? g_rtdTw[hand] : g_rtdT[hand], sizeof(t));
        if (g_rtdSeq == a) {
            memcpy(cR[hand][s], r, sizeof(r));
            memcpy(cT[hand][s], t, sizeof(t));
            cOk[hand][s] = true;
        }
    }
    if (!cOk[hand][s]) return false;
    memcpy(R, cR[hand][s], sizeof(cR[hand][s]));
    memcpy(T, cT[hand][s], sizeof(cT[hand][s]));
    return true;
}


// command the VIEW MODELS only - pMesh must never be rotated (it carries the
// camera), and commanding it also contaminates the very measurement we make
static void GtCommandVM(float pitchDeg, float yawDeg, float rollDeg,
                        float tx, float ty, float tz)
{
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        if (!FpIsViewModel(k)) continue;
        uint8_t* o = k->obj;
        if (!LooksLikeObj(o) || !FpFieldsLookRight(o)) continue;
        int32_t* r = (int32_t*)(o + kMeshRot);
        r[0] = (int32_t)(pitchDeg / 57.2958f * kUEPerRad);
        r[1] = (int32_t)(yawDeg   / 57.2958f * kUEPerRad);
        r[2] = (int32_t)(rollDeg  / 57.2958f * kUEPerRad);
        float* T = (float*)(o + kMeshTrans);
        T[0] = tx; T[1] = ty; T[2] = tz;
        k->lastCmdP = pitchDeg / 57.2958f;   // keep the live-parent recovery
        k->lastCmdY = yawDeg   / 57.2958f;   // exact through probes too
        k->lastCmdR = rollDeg  / 57.2958f;
    }
}








static float GtAngleOf(const float* R)          // rotation angle, degrees
{
    float c = (R[0] + R[4] + R[8] - 1.0f) * 0.5f;
    if (c >  1.0f) c =  1.0f;
    if (c < -1.0f) c = -1.0f;
    return acosf(c) * 57.2958f;
}


static void GtAxisOf(const float* R, float* ax) // unit axis (sign-consistent)
{
    ax[0] = R[7] - R[5]; ax[1] = R[2] - R[6]; ax[2] = R[3] - R[1];
    float l = sqrtf(ax[0]*ax[0] + ax[1]*ax[1] + ax[2]*ax[2]);
    if (l > 1e-6f) { ax[0] /= l; ax[1] /= l; ax[2] /= l; }
}


static float GtMismatch(const float* A, const float* B)  // deg between two rots
{
    float BT[9], E[9];
    M3T(B, BT);
    M3Mul(A, BT, E);
    return GtAngleOf(E);
}


static void GtProbeCmd(int phase, float* p, float* y, float* r,
                       float* tx, float* ty, float* tz)
{
    *p = *y = *r = *tx = *ty = *tz = 0.0f;
    if      (phase == 1) *y  = 30.0f;
    else if (phase == 3) *p  = 30.0f;
    else if (phase == 5) *r  = 30.0f;
    else if (phase == 7) *tx = 30.0f;
    else if (phase == 8) *ty = 30.0f;
    else if (phase == 9) *tz = 30.0f;
    else if (phase == 10) { *y = 30.0f; *tx = 30.0f; }  // are rot and trans
                                                        // independent? UE3 says
                                                        // yes; check anyway
}


static void GtCaptureT0()
{
    for (int i = 0; i < g_fpCandN && i < 24; i++)
        g_gtT0Ok[i] = FpIsViewModel(&g_fpCand[i]) &&
                      FpWorldPos(g_fpCand[i].obj, g_gtT0[i]);
}


static void GtReportT(int phase)
{
    for (int i = 0; i < g_fpCandN && i < 24; i++) {
        if (!g_gtT0Ok[i]) continue;
        float pw[3];
        if (!LooksLikeObj(g_fpCand[i].obj)) continue;
        if (!FpWorldPos(g_fpCand[i].obj, pw)) continue;
        Log("gt: '%s' %s: dW=(%+.1f,%+.1f,%+.1f) uu",
            g_fpCand[i].asset, kGtName[phase],
            pw[0] - g_gtT0[i][0], pw[1] - g_gtT0[i][1], pw[2] - g_gtT0[i][2]);
    }
}


static void GtCapture0(bool logIt)
{
    for (int i = 0; i < g_fpCandN && i < 24; i++) {
        g_gtB0Ok[i] = FpIsViewModel(&g_fpCand[i]) &&
                      FpBasis(g_fpCand[i].obj, g_gtB0[i]);
        if (g_gtB0Ok[i] && logIt) {
            const float* b = g_gtB0[i];
            Log("gt: '%s' rest basis  r0=(%+.2f,%+.2f,%+.2f) r1=(%+.2f,%+.2f,%+.2f) r2=(%+.2f,%+.2f,%+.2f)",
                g_fpCand[i].asset, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8]);
        }
    }
    g_gtRefOk = g_fpRef && LooksLikeObj(g_fpRef) && FpBasis(g_fpRef, g_gtRefB0);
    if (logIt) {
        float py = 0.0f; bool hp = FpPawnYaw(&py);
        Log("gt: view=%.1f pawn=%.1f%s hmd=%.1f deg",
            g_viewYawRad * 57.2958f, py * 57.2958f, hp ? "" : "(none)",
            g_hmdYaw * 57.2958f);
    }
}


static void GtReport(int probePhase)
{
    float cp, cy, cr, tx, ty, tz;
    GtProbeCmd(probePhase, &cp, &cy, &cr, &tx, &ty, &tz);
    float Mc[9], McT[9];
    GtUE3Rot(cp, cy, cr, Mc);
    M3T(Mc, McT);

    if (g_gtRefOk && g_fpRef && LooksLikeObj(g_fpRef)) {
        float rn[9];
        if (FpBasis(g_fpRef, rn)) {
            float d = GtMismatch(rn, g_gtRefB0);
            Log("gt: [%s] pMesh (untouched) moved %.1f deg during probe%s",
                kGtName[probePhase], d,
                d > 5.0f ? "  ** NOISY - hold head still, re-run **" : "");
        }
    }
    for (int i = 0; i < g_fpCandN && i < 24; i++) {
        if (!g_gtB0Ok[i]) continue;
        if (!LooksLikeObj(g_fpCand[i].obj)) continue;
        float b1[9];
        if (!FpBasis(g_fpCand[i].obj, b1)) continue;
        float b0T[9], dL[9], dR[9];
        M3T(g_gtB0[i], b0T);
        M3Mul(b1, b0T, dL);                  // b1 = dL * b0
        M3Mul(b0T, b1, dR);                  // b1 = b0 * dR
        float axL[3], axR[3];
        GtAxisOf(dL, axL);
        GtAxisOf(dR, axR);
        Log("gt: '%s' %s: dL ang=%.1f ax=(%+.2f,%+.2f,%+.2f) errL=%.1f errLi=%.1f | dR ang=%.1f ax=(%+.2f,%+.2f,%+.2f) errR=%.1f errRi=%.1f",
            g_fpCand[i].asset, kGtName[probePhase],
            GtAngleOf(dL), axL[0], axL[1], axL[2],
            GtMismatch(dL, Mc), GtMismatch(dL, McT),
            GtAngleOf(dR), axR[0], axR[1], axR[2],
            GtMismatch(dR, Mc), GtMismatch(dR, McT));
    }
}


static void GtStop(const char* why)
{
    GtCommandVM(0.0f, 0.0f, 0.0f);
    g_gtActive = false;
    Log("gt: ==== %s ====", why);
}


static void GtStart()
{
    if (!g_handMesh || !g_fpCandN) {
        Log("gt: weapon tracking must be ON first (F6, then HOME)");
        return;
    }
    int vm = 0;
    for (int i = 0; i < g_fpCandN; i++)
        if (FpIsViewModel(&g_fpCand[i])) vm++;
    if (!vm) {
        Log("gt: no view models (pPlayerMesh) in the candidate list yet");
        return;
    }
    g_gtActive = true;
    g_gtPhase  = 0;
    g_gtFrame  = 0;
    Log("gt: ==== GROUND TRUTH START (build 30.4, %d view model(s)) ====", vm);
    Log("gt: put BOTH controllers down where the base stations still see them,");
    Log("gt: hands off, head STILL for the first ~6 s. Do not touch anything.");
}


static void GtTick()
{
    const int kSettle = 60;                  // ~0.5 s per phase at 120 fps
    const int kHead   = 960;                 // ~8 s head-leak watch

    if (g_gtPhase <= 10) {
        float cp, cy, cr, tx, ty, tz;
        GtProbeCmd(g_gtPhase, &cp, &cy, &cr, &tx, &ty, &tz);
        GtCommandVM(cp, cy, cr, tx, ty, tz);
        if (++g_gtFrame < kSettle) return;
        g_gtFrame = 0;
        bool rot = (g_gtPhase == 1 || g_gtPhase == 3 || g_gtPhase == 5 ||
                    g_gtPhase == 10);
        bool trn = (g_gtPhase >= 7 && g_gtPhase <= 10);
        if (rot) GtReport(g_gtPhase);
        if (trn) GtReportT(g_gtPhase);
        if (!rot && !trn) {                  // fresh zero baselines each time
            GtCapture0(g_gtPhase == 0);
            GtCaptureT0();
        }
        g_gtPhase++;
        if (g_gtPhase == 11) {
            GtCommandVM(0.0f, 0.0f, 0.0f);
            for (int h = 0; h < 2; h++) {
                g_gtC0Ok[h]  = FpHandBasisWorld(h, g_gtC0[h]);
                g_gtHR0Ok[h] = HandRoomPos(h, g_gtHR0[h]);
                float yr, pa;
                g_gtRel0Ok[h] = HandAnglesPos(h, &yr, &pa, g_gtRel0[h]);
            }
            g_gtHmd0Ok = g_devPoseOk[0];
            if (g_gtHmd0Ok) {
                g_gtHmd0[0] = g_devPose[0][0][3];
                g_gtHmd0[1] = g_devPose[0][1][3];
                g_gtHmd0[2] = g_devPose[0][2][3];
            }
            g_gtPawn0Ok = FpPawnYaw(&g_gtPawn0);
            g_gtQMax[0] = g_gtQMax[1] = 0.0f;
            if (!g_gtC0Ok[0] && !g_gtC0Ok[1])
                Log("gt: WARNING - no controller pose, head test will be silent");
            Log("gt: probes done. HEAD TEST for 8 s: leave the controllers");
            Log("gt: still and MOVE YOUR HEAD - look around, tilt, lean.");
        }
        return;
    }

    GtCommandVM(0.0f, 0.0f, 0.0f);           // stay at rest during head test
    // parent motion under head movement: the pawn follows the view, so its
    // yaw should track your head 1:1 here - and any LAG between the pawn's
    // actor yaw and pMesh's component matrix (our drift reference) is leak
    // we cannot currently compensate. Measure both.
    if (g_gtFrame % 120 == 0) {
        float py, pd = 0.0f;
        bool pok = g_gtPawn0Ok && FpPawnYaw(&py);
        if (pok) pd = FpWrapPi(py - g_gtPawn0) * 57.2958f;
        float md = -1.0f;
        if (g_gtRefOk && g_fpRef && LooksLikeObj(g_fpRef)) {
            float rn[9];
            if (FpBasis(g_fpRef, rn)) md = GtMismatch(rn, g_gtRefB0);
        }
        char vmtxt[64]; vmtxt[0] = 0;
        int shown = 0;
        for (int i = 0; i < g_fpCandN && i < 24 && shown < 2; i++) {
            if (!g_gtB0Ok[i]) continue;
            float b1[9];
            if (!LooksLikeObj(g_fpCand[i].obj)) continue;
            if (!FpBasis(g_fpCand[i].obj, b1)) continue;
            char one[24];
            snprintf(one, sizeof(one), " vm%d=%.1f", i, GtMismatch(b1, g_gtB0[i]));
            strncat(vmtxt, one, sizeof(vmtxt) - strlen(vmtxt) - 1);
            shown++;
        }
        Log("gt: headpar %ds pawnD=%+.1f pMeshD=%.1f%s deg",
            g_gtFrame / 120, pd, md, vmtxt);
    }
    for (int h = 0; h < 2; h++) {
        if (!g_gtC0Ok[h]) continue;
        float Cn[9], C0T[9], Q[9];
        if (!FpHandBasisWorld(h, Cn)) continue;
        M3T(g_gtC0[h], C0T);
        M3Mul(C0T, Cn, Q);
        float a = GtAngleOf(Q);
        if (a > g_gtQMax[h]) g_gtQMax[h] = a;
        if (g_gtFrame % 120 == 0) {
            float ax[3];
            GtAxisOf(Q, ax);
            Log("gt: head %ds %s |Q|=%.1f ax=(%+.2f,%+.2f,%+.2f) A=%.1f view=%.1f hmd=%.1f",
                g_gtFrame / 120, h ? "R" : "L", a, ax[0], ax[1], ax[2],
                (g_viewYawRad - g_hmdYaw) * 57.2958f,
                g_viewYawRad * 57.2958f, g_hmdYaw * 57.2958f);
            // the position story: room offset should sit still while the
            // head-relative offset (the old anchor) wanders with your head
            float pr[3], pf[3], yr, pa;
            if (g_gtHR0Ok[h] && g_gtRel0Ok[h] &&
                HandRoomPos(h, pr) && HandAnglesPos(h, &yr, &pa, pf)) {
                float dh[3] = { 0, 0, 0 };
                if (g_gtHmd0Ok && g_devPoseOk[0]) {
                    dh[0] = g_devPose[0][0][3] - g_gtHmd0[0];
                    dh[1] = g_devPose[0][1][3] - g_gtHmd0[1];
                    dh[2] = g_devPose[0][2][3] - g_gtHmd0[2];
                }
                Log("gt: headpos %ds %s dRoom=(%+.2f,%+.2f,%+.2f) dRel=(%+.2f,%+.2f,%+.2f) dHmd=(%+.2f,%+.2f,%+.2f) m",
                    g_gtFrame / 120, h ? "R" : "L",
                    pr[0] - g_gtHR0[h][0], pr[1] - g_gtHR0[h][1], pr[2] - g_gtHR0[h][2],
                    pf[0] - g_gtRel0[h][0], pf[1] - g_gtRel0[h][1], pf[2] - g_gtRel0[h][2],
                    dh[0], dh[1], dh[2]);
            }
        }
    }
    if (++g_gtFrame >= kHead) {
        Log("gt: head test max |Q|  L=%.1f  R=%.1f deg (near 0 = no head leak)",
            g_gtQMax[0], g_gtQMax[1]);
        GtStop("DONE - quit the game and send dishonored_vr.log");
    }
}


// ============================================================================
// 30.71 - IN-HEADSET ALIGNMENT MARKERS
//
// The calibration is "hold your controller where the game draws its hand, then
// press END". In a headset you cannot see your controller, which is why doing
// it by feel and then fighting three trim sliders was miserable. So: pin a
// bright ring to each controller as a SteamVR overlay - the same mechanism the
// reticle already uses, so no new render path - and billboard it at the HMD so
// it is visible from any angle. Park the ring on the in-game hand, press END,
// and the residual that causes head-turn swim is zero by construction.
//
// Left is cyan, right is orange, matching the trim rows in the overlay.
// ============================================================================
static void RtdMarkerTick()
{
    bool want = g_rtdMarkers && g_mode == MODE_SCENE;
    if (want && !g_ov && !GetFnTable(IVROverlay_Version, (void**)&g_ov)) {
        g_rtdMarkers = false;
        Log("handrt: no overlay interface - markers disabled");
        return;
    }
    for (int h = 0; h < 2; h++) {
        int dev = g_ctrlIdx[h];
        bool w = want && dev >= 0 && dev < 16 && g_devPoseOk[dev] && g_devPoseOk[0];

        if (w && !g_rtdMarkOv[h]) {
            char key[64], nm[64];
            _snprintf(key, sizeof(key), "gingasvr.dishonoredvr.align%d", h);
            _snprintf(nm,  sizeof(nm),  "DVR Align %s", h ? "R" : "L");
            if (g_ov->CreateOverlay(key, nm, &g_rtdMarkOv[h])
                != EVROverlayError_VROverlayError_None) {
                g_rtdMarkOv[h] = 0; g_rtdMarkers = false;
                Log("handrt: CreateOverlay failed - markers disabled");
                return;
            }
            // open ring + centre dot + crosshair ticks, so the exact centre is
            // readable against a busy scene instead of a fuzzy blob
            static uint32_t px[64 * 64];
            for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
                float dx = x - 31.5f, dy = y - 31.5f;
                float r = sqrtf(dx*dx + dy*dy);
                float dot  = 3.0f - r;
                float ring = 1.8f - fabsf(r - 24.0f);
                float tick = 0.0f;
                if (r > 8.0f && r < 20.0f) {
                    float ax = fabsf(dx), ay = fabsf(dy);
                    tick = 1.6f - (ax < ay ? ax : ay);
                }
                float a = dot; if (ring > a) a = ring; if (tick > a) a = tick;
                if (a < 0) a = 0; if (a > 1) a = 1;
                unsigned al = (unsigned)(255.0f * a);
                unsigned R = h ? 255u : 60u, G = h ? 150u : 230u, B = h ? 40u : 255u;
                px[y*64 + x] = (al << 24) | (B << 16) | (G << 8) | R;
            }
            g_ov->SetOverlayRaw(g_rtdMarkOv[h], px, 64, 64, 4);
            Log("handrt: alignment marker created (%s hand, device %d)",
                h ? "right" : "left", dev);
        }
        if (!g_rtdMarkOv[h]) continue;

        if (w) {
            g_ov->SetOverlayWidthInMeters(g_rtdMarkOv[h], g_rtdMarkSize);
            // Billboard: build a world basis whose +Z (overlay front) points at
            // the HMD, then express it in controller-local axes so the overlay
            // can stay device-relative and inherit the controller's position
            // with no tracking-universe assumptions.
            float (*hc)[4] = g_devPose[dev];
            float (*hd)[4] = g_devPose[0];
            float z[3] = { hd[0][3]-hc[0][3], hd[1][3]-hc[1][3], hd[2][3]-hc[2][3] };
            if (V3Norm(z) > 0.05f) {
                float up[3] = { 0, 1, 0 };
                float xw[3]; V3Cross(up, z, xw);
                if (V3Norm(xw) > 0.05f) {
                    float yw[3]; V3Cross(z, xw, yw); V3Norm(yw);
                    HmdMatrix34_t m; memset(&m, 0, sizeof(m));
                    const float* cols[3] = { xw, yw, z };
                    for (int i = 0; i < 3; i++)          // C' * basis
                        for (int j = 0; j < 3; j++)
                            m.m[i][j] = hc[0][i]*cols[j][0] + hc[1][i]*cols[j][1]
                                      + hc[2][i]*cols[j][2];
                    g_ov->SetOverlayTransformTrackedDeviceRelative(
                        g_rtdMarkOv[h], (TrackedDeviceIndex_t)dev, &m);
                }
            }
        }
        if (w != g_rtdMarkVis[h]) {
            g_rtdMarkVis[h] = w;
            if (w) g_ov->ShowOverlay(g_rtdMarkOv[h]);
            else   g_ov->HideOverlay(g_rtdMarkOv[h]);
        }
    }
}
