// game/dishonored/fov_lever.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static inline void LevWrite(uint8_t* p, float t)
{
    if (!RangeReadable(p, 4)) return;
    float* f = (float*)p;
    if (*f > 5.0f && *f < 175.0f) *f = t;
}


static inline void FovLeverApply()
{
    float deg = dvr::camera::fov_deg();   // 41.0: the seam's target (= [Screen] FovLever unless a method moved it)
    if (!(deg >= 40.0f && deg <= 160.0f)) {
        if (g_fovNatural != 0.0f) {          // lever just turned off - re-arm
            g_fovNatural = 0.0f;
            memset(g_levLast, 0, sizeof(g_levLast));
            memset(g_levBase, 0, sizeof(g_levBase));
        }
        return;
    }
    // 32.6: the cheap liveness test runs on EVERY dispatch. The old code only
    // revalidated once every 256 - and its test could not detect a destroyed
    // camera anyway - so a level load left this writing floats into a freed
    // UObject thousands of times a second. That is the same hazard the crash
    // trace pointed at for the SkelControls.
    if (g_camObj && !CamAlive()) { g_camObj = NULL; g_fovNatural = 0.0f; return; }
    static LONG revalidate = 0;
    if ((InterlockedIncrement(&revalidate) & 255) == 0) {
        if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    }
    // capture the engine's natural base once, from the field that tracked the
    // rendered FOV during the zoom test
    if (g_fovNatural == 0.0f) {
        if (!g_camObj || !RangeReadable(g_camObj + 0x53c, 4)) return;
        float nat = *(float*)(g_camObj + 0x53c);
        if (!(nat > 30.0f && nat < 140.0f)) return;
        g_fovNatural = nat;
        Log("fovlever: natural base %.1f deg, target %.0f (ratio %.3f)",
            nat, deg, deg / nat);
    }
    // 30.52: read the engine's intent from a SENSOR field we never write
    // (+0x53c tracked the rendered FOV through the spyglass zoom), and bound
    // it at the natural base. Game-authored FOV changes only ever go NARROWER
    // (zoom, cutscene framing), so any reading WIDER than natural can only be
    // our own writes echoing back - the runaway that fisheyed the view in
    // 30.51. Clamping the sensor at natural breaks that loop by construction,
    // while genuine zooms pass straight through and get scaled.
    float sensor = g_fovNatural;
    if (g_camObj && RangeReadable(g_camObj + 0x53c, 4)) {
        float s = *(float*)(g_camObj + 0x53c);
        if (s > 10.0f && s < 175.0f) { sensor = s; dvr::camera::note_rendered_fov(s); }
    }
    if (sensor > g_fovNatural) sensor = g_fovNatural;      // the loop breaker
    float R = deg / g_fovNatural;
    float t = sensor * R;
    if (t < 20.0f)  t = 20.0f;
    if (t > 160.0f) t = 160.0f;
    if (g_peCtrl)
        for (int i = 0; i < 3; i++) LevWrite(g_peCtrl + kLevCtrl[i], t);
    if (g_camObj)
        for (int i = 0; i < 7; i++) {
            if (kLevCam[i] == 0x53c) continue;             // sensor stays clean
            LevWrite(g_camObj + kLevCam[i], t);
        }
    InterlockedIncrement(&g_fovLeverWrites);

    // 38.24 EYE CLAMP - THE crouch fix, correct by construction. Measured
    // (dishonored_vr_headclip.log): the eye interpolates back to 78 uu above
    // the pawn in BOTH stances - crouch shrinks Corvo's capsule to 65 but
    // the camera stays ABOVE HIS OWN PHYSICAL HEAD, inside whatever he
    // ducked under (the head-in-the-table screenshot; the first complaint of
    // this whole saga). The law: the camera may never sit above the capsule
    // that contains it. zmax = pawnZ + CollisionHeight - margin, from the
    // game's own live values - no stance detection, no invented heights.
    // Standing is untouched by arithmetic (78 < 87.5-8); crouch clamps to
    // just under the capsule top; vents follow their 33 automatically; the
    // stance flapping is harmless because the clamp just follows the
    // capsule. Rides this dispatch-cadence writer, which the FOV lever
    // proved sticks. [PosTrack] EyeClamp=0 reverts.
    if (g_eyeClampCfg && g_pePawn && g_actorLocFound && g_cylLast > 10.0f &&
        (MaimNowMs() - g_cylOkMs) < 1500.0 &&
        RangeReadable(g_pePawn + g_actorLocOff, 12)) {
        float pz = ((const float*)(g_pePawn + g_actorLocOff))[2];
        float zmax = pz + g_cylLast - g_eyeClampMargin;
        // 38.26: EASE THE CEILING DOWN. 38.25 measured the clamp working but
        // TELEPORTING: the capsule resizes in one engine tick (87.5 -> 65 ->
        // 33 and back), pawnZ jumps 32-55 uu with it, so zmax - and the
        // camera welded to it - snapped by half a metre several times per
        // second while crawling. That is the "not smooth" in the report, and
        // a view that teleports is a view you cannot judge a gap with, which
        // is how a bump turns into a wedge. The clamp height now TIGHTENS at
        // a finite rate (default 300 uu/s ~ 0.18 s for a full stance change,
        // the same order as UE3's own eye interpolator) and RELEASES
        // instantly - so standing up, jumping and blinking never fight a
        // lagging ceiling, only the duck is eased. A jump bigger than 400 uu
        // (blink, teleport, level load) snaps outright. Rate 0 = 38.24
        // behaviour. [PosTrack] EyeClampRate.
        if (g_eyeClampRate > 0.0f) {
            double ecNow = MaimNowMs();
            float dt = (g_ecCeilMs > 0.0) ? (float)((ecNow - g_ecCeilMs) * 0.001) : 0.0f;
            g_ecCeilMs = ecNow;
            if (dt < 0.0f) dt = 0.0f;
            if (dt > 0.25f) dt = 0.25f;          // a hitch must not free-fall
            if (!g_ecCeilOn || fabsf(zmax - g_ecCeil) > 400.0f) {
                g_ecCeil = zmax; g_ecCeilOn = true;   // first frame / teleport
            } else if (zmax >= g_ecCeil) {
                g_ecCeil = zmax;                      // release: instant
            } else {
                float step = g_eyeClampRate * dt;     // tighten: rate-limited
                g_ecCeil = (g_ecCeil - step > zmax) ? (g_ecCeil - step) : zmax;
            }
            zmax = g_ecCeil;
        }
        static const uint32_t kCamLoc[4] = { kCamLoc0, kPovOffs[0], kPovOffs[1], kPovOffs[2] };
        bool did = false; float was = 0.0f;
        for (int ci = 0; ci < 4; ci++) {
            if (!g_camObj || !RangeReadable(g_camObj + kCamLoc[ci], 12))
                continue;
            float* lp = (float*)(g_camObj + kCamLoc[ci]);
            float z = lp[2];
            if (z > zmax && (z - pz) < 250.0f && (z - pz) > -250.0f) {
                if (!did) was = z;
                lp[2] = zmax;
                did = true;
            }
        }
        if (did) {
            static double tl = 0.0; double nw = MaimNowMs();
            if (nw - tl > 1000.0) { tl = nw;
                Log("eyeclamp: camZ %.1f -> %.1f (pawnZ %.1f cyl %.1f) - "
                    "the eye stays inside the capsule", was, zmax, pz,
                    g_cylLast);
            }
        }
    }
}
