// game/dishonored/fov_lever.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

#include "game/dishonored/fov_lever_policy.h"
#include "game/dishonored/stereo_state_policy.h"

// VR-39 run 14: AFW's foreground gain (see the feed in FovLeverApply). A live A/B lever: [Stereo] AfwForegroundGain,
// F10 (the hands' FOV controls), `armslens gain <x>`.
static std::atomic<float> g_afwFgGain{0.911f};
static float AfwFgGainGet() { return g_afwFgGain.load(); }
static void AfwFgGainSet(float g, const char* who)
{
    if (!(g >= 0.80f && g <= 1.0f)) { Log("armslens: AFW foreground gain %.3f refused (0.80-1.00) (%s)", g, who ? who : "?"); return; }
    if (fabsf(g_afwFgGain.exchange(g) - g) > 0.0005f)
        Log("armslens: AFW foreground gain %.3f (%s): the hands are rebuilt at %.2f deg for a %.2f deg world", g, who ? who : "?",
            2.0f * atanf(tanf(ProjectionFovGet() * 0.5f * 0.0174533f) / g) * 57.29578f, ProjectionFovGet());
}

static inline void LevWrite(uint8_t* p, float t)
{
    if (!PeReadable(p, 4)) return;   // route 2: region-cached (game thread, live engine objects)
    float* f = (float*)p;
    if (*f > 5.0f && *f < 175.0f) *f = t;
}


static inline void FovLeverApply()
{
    static dvr::fov_lever::CinematicRecovery recovery;
    static unsigned recoveryEpoch = 0;
    // The base this session trusted and our last write survive a re-arm (the owners
    // and g_fovNatural reset on loads): a recapture is judged against them.
    static float keptNatural = 0.0f, lastWrite = 0.0f;
    const unsigned epoch = UiSurfaceEpoch();
    if (recoveryEpoch != epoch) { recovery = {}; recoveryEpoch = epoch; }
    if (UiSurfaceOwnsPresentation()) { recovery = {}; dvr::camera::set_eye_ceiling(0.0f,false); return; }   // VR-117: the lever follows the projection claim while a screen rides
    float deg = dvr::camera::fov_deg();   // 41.0: the seam's target (= [Screen] FovLever unless a method moved it)
    if (!(deg >= 40.0f && deg <= 160.0f)) {
        recovery = {};
        if (g_fovNatural != 0.0f) {          // lever just turned off - re-arm
            g_fovNatural = 0.0f;
            memset(g_levLast, 0, sizeof(g_levLast));
            memset(g_levBase, 0, sizeof(g_levBase));
        }
        return;
    }
    // VR-213: refresh live identities after loads/menu epochs before any write.
    if (!FovLeverOwnersReady()) {
        recovery = {};
        dvr::camera::set_eye_ceiling(0.0f, false);
        return;
    }
    // VR-50: draw-scoped gameplay FOV is restored after both eyes. Feeding that
    // temporary value into the persistent ratio writer would narrow it every frame.
    if (!ProjectionFovScopeActive()) {
        // capture the engine's natural base once, from the field that tracked the
        // rendered FOV during the zoom test
        if (g_fovNatural == 0.0f) {
            recovery = {}; // New owner, load or rearmed lever.
            if (!g_camObj || !RangeReadable(g_camObj + kFovSensor, 4)) return;
            const float reading = *(float*)(g_camObj + kFovSensor);
            dvr::fov_lever::Rearm why;
            const float nat = dvr::fov_lever::rearm_natural(reading, keptNatural, lastWrite, deg, &why);
            if (!(nat > 30.0f && nat < 140.0f)) {
                // Said once per refused reading, so a lever that writes nothing after a load explains itself.
                static float saidRefused = 0.0f;
                if (why == dvr::fov_lever::Rearm::Invalid && fabsf(reading - saidRefused) > 0.5f) {
                    saidRefused = reading;
                    Log("fovlever: natural base NOT captured from a reading of %.2f deg (under the %.0f floor: a zoom, or a save loaded "
                        "zoomed) - the lever writes nothing until a plausible reading arrives", reading, dvr::fov_lever::kNaturalFloorDeg);
                }
                return;
            }
            g_fovNatural = nat;
            keptNatural = nat;
            Log("fovlever: natural base %.1f deg (read %.2f, last write %.2f: %s), target %.2f, effective base %.2f "
                "(ratio %.3f; %s)",
                nat, reading, lastWrite, dvr::fov_lever::rearm_name(why), deg, nat < deg ? nat : deg,
                deg / (nat < deg ? nat : deg),
                nat < deg ? "narrow values widen back each pass"
                          : "RATIO 1: nothing pulls a narrowed view back - expected only when the game's own FOV is at or above the target");
        }
        // VR-213: readback includes our own interpolated output. Never apply
        // a ratio below one to it; that recursively narrows toward the clamp.
        float sensor = g_fovNatural;
        if (g_camObj && RangeReadable(g_camObj + kFovSensor, 4)) {
            float s = *(float*)(g_camObj + kFovSensor);
            if (s > 10.0f && s < 175.0f) { sensor = s; dvr::camera::note_rendered_fov(s); }
        }
        float t = dvr::fov_lever::target(sensor, g_fovNatural, deg);
        if (t == 0.0f) { recovery = {}; dvr::afw::set_fg_fov(0.0f); return; }   // lever off: the arms' FOV is unknown
        const auto state = dvr::anim::snapshot();
        const bool walking = !strcmp(state.state[0], "StatePlayerMasterWalk") ||
            !strcmp(state.state[0], "StatePlayerMasterFalling") ||
            !strcmp(state.state[0], "StatePlayerMasterJump");
        const bool cinematicEligible = CineFovEnabled() && state.valid &&
            !UiSurfaceBlocks() && !g_menuOpen && !g_inMenu && !g_mainMenu && !g_gameExiting &&
            dvr::stereo::wants_projection() && dvr::vr::session_live() &&
            !dvr::vr::cinematic_active() && !dvr::camera::eyetest_active() && !dvr::camera::postest_active();
        const float cinematicTarget = recovery.update(cinematicEligible,
            dvr::scene_state::cinematic(state.state[0]), walking, sensor, deg, GetTickCount64());
        if (cinematicTarget > 0) t = cinematicTarget;
        // A dispatch inside an active cinematic draw must preserve that draw's FOV.
        const float scoped=CineFovScopeTarget();
        if (scoped>0) t=scoped;
        // VR-39 run 14: AFW's foreground projection. The draws put the arms at the WORLD's FOV (fgproj:, with the
        // arms' lens forced on or off), yet the rebuild matches the next native frame only with the foreground widened
        // by a fixed gain: replayed on the run-12 capture (arms drawn at 103), 12.46% of the hand band differs at 103,
        // 5.29 at 106.5, 2.10 at 108.07, 2.93 at 109.5, 5.43 at 111 - and run 6 measured the same 0.911 disparity gain
        // with the camera at 108. So it is a property of the foreground pass (most likely its depth), not a FOV and
        // not the headset's: tan(fg/2) = tan(world/2) / [Stereo] AfwForegroundGain (0.911; 1 = like the world). Only in
        // plain gameplay; a scripted zoom or a scope feeds 0 (the world's FOV throughout).
        const float worldFov = ProjectionFovGet() > 0.0f ? ProjectionFovGet() : sensor;
        const float g = AfwFgGainGet();
        const float fgFov = (worldFov > 5.0f && g > 0.0f)
            ? 2.0f * atanf(tanf(worldFov * 0.5f * 0.0174533f) / g) * 57.29578f : 0.0f;
        dvr::afw::set_fg_fov(cinematicTarget <= 0 && scoped <= 0 && fabsf(sensor - lastWrite) < 0.5f ? fgFov : 0.0f);
        if (t < 20.0f)  t = 20.0f;
        if (t > 160.0f) t = 160.0f;
        if (IsLiveObject(g_peCtrl))
            for (int i = 0; i < 3; i++) LevWrite(g_peCtrl + kLevCtrl[i], t);
        // 2026-10-07: a magnified zoom ([Screen] ZoomMagnify) draws the world narrower under a claim held at the
        // target; the game's lock-arms zoom copies m_fCurFOV_Arms into the arms' lens, so that field keeps the
        // target and the hands stay their true size while the world magnifies. Only while the scene scope has
        // published a narrower draw; otherwise the field gets the lever's value as it always has.
        const float published = CineFovClaim();
        const bool zooming = ZoomMagnifyGet() && published > 0.0f && published < deg - 0.5f;
        if (IsLiveObject(g_camObj))
            for (int i = 0; i < 7; i++) {
                if (kLevCam[i] == kFovSensor) continue;             // never directly write readback
                LevWrite(g_camObj + kLevCam[i], zooming && kLevCam[i] == kFovArms ? deg : t);
            }
        lastWrite = t;
        InterlockedIncrement(&g_fovLeverWrites);
        static double nextLog = 0;
        const double now = MaimNowMs();
        if (now >= nextLog) {
            nextLog = now + 1000;
            Log("fovlever: feedback sensor=%.2f natural=%.2f target=%.2f write=%.2f arms=%.2f%s scoped=%d cinematicRecovery=%d master=%s",
                sensor, g_fovNatural, deg, t, zooming ? deg : t, zooming ? " (zoom: the arms hold the target)" : "",
                scoped > 0, cinematicTarget > 0, state.state[0]);
        }
    }

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
    // VR-78: the accounting probe's clamp record (z_account.h), one per pass.
    // It reads what the fields held before and after THIS clamp; a value that
    // was our own previous write is flagged, never called the engine's eye.
    const bool za = dvr::zacct::enabled();
    dvr::zacct::Clamp zc;
    if (za) {
        static uint32_t zcSeq = 0;
        zc.seq = ++zcSeq;
        zc.ms = dvr::zacct::now_ms();
        zc.cyl = g_cylLast;
        zc.cylAgeMs = MaimNowMs() - g_cylOkMs;
    }
    // Capsule and pawn Z must belong to the same owner across a reload.
    uint8_t* clampPawn = g_pawnFromController ? PawnForCollision() : g_pePawn;
    if (g_eyeClampCfg && IsLiveObject(clampPawn) &&
        (!g_pawnFromController || clampPawn == g_cylMeasuredPawn) && g_actorLocFound && g_cylLast > 10.0f &&
        (MaimNowMs() - g_cylOkMs) < 1500.0 &&
        RangeReadable(clampPawn + g_actorLocOff, 12)) {
        float pz = ((const float*)(clampPawn + g_actorLocOff))[2];
        float zmax = pz + g_cylLast - g_eyeClampMargin;
        {   // 2026-10-07 eye/probe: where the camera sits against the pawn's eye, in the pawn's yaw frame (UE3: X forward,
            // Y right, Z up; yaw in 65536ths). A report of "too far back" in an authored scene is arithmetic with this
            // line: ahead/behind is the number to read, with the positional-tracking offset included.
            static double nextProbe = 0.0; const double pn = MaimNowMs();
            if (pn >= nextProbe) {
                nextProbe = pn + 5000.0;
                const uint32_t rotOff = RflOffsetOf("Actor", "Rotation"), eyeOff = RflOffsetOf("Pawn", "EyeHeight");
                const float* pl = (const float*)(clampPawn + g_actorLocOff);
                const float* cl = RangeReadable(g_camObj + kPovOffs[0], 12) ? (const float*)(g_camObj + kPovOffs[0]) : NULL;
                float eyeH = NAN; if (eyeOff && RangeReadable(clampPawn + eyeOff, 4)) memcpy(&eyeH, clampPawn + eyeOff, 4);
                int32_t rot[3] = { 0, 0, 0 }; const bool rotOk = rotOff && RangeReadable(clampPawn + rotOff, 12);
                if (rotOk) memcpy(rot, clampPawn + rotOff, 12);
                if (cl && std::isfinite(eyeH) && rotOk) {
                    const float yaw = (float)rot[1] * (6.2831853f / 65536.0f), fx = cosf(yaw), fy = sinf(yaw);
                    const float dx = cl[0] - pl[0], dy = cl[1] - pl[1], dz = cl[2] - (pl[2] + eyeH);
                    Log("eye/probe: the camera sits %+.1f uu ahead(+)/behind(-) the pawn's eye, %+.1f right, %+.1f up (pawn EyeHeight %.1f, "
                        "positional offset included) master=%s", dx * fx + dy * fy, -dx * fy + dy * fx, dz, eyeH,
                        dvr::anim::snapshot().state[0]);
                } else {
                    Log("eye/probe: unavailable (camera location %d, EyeHeight %d, rotation %d)", cl != NULL, (int)std::isfinite(eyeH), (int)rotOk);
                }
            }
        }
        if (za) {
            memcpy(zc.pawn, (const float*)(clampPawn + g_actorLocOff), sizeof(zc.pawn));
            zc.ceilRaw = zmax;
        }
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
        // 41.1: the camera seam's position write (lean, crouch on the camera
        // lane) runs after this pass and caps what it writes at the same ceiling.
        dvr::camera::set_eye_ceiling(zmax, true);
        static const uint32_t kCamLoc[4] = { kCamLoc0, kPovOffs[0], kPovOffs[1], kPovOffs[2] };
        bool did = false; float was = 0.0f;
        for (int ci = 0; ci < 4; ci++) {
            if (za) zc.fieldOff[ci] = kCamLoc[ci];
            if (!g_camObj || !RangeReadable(g_camObj + kCamLoc[ci], 12))
                continue;
            float* lp = (float*)(g_camObj + kCamLoc[ci]);
            float z = lp[2];
            bool ours = false;
            if (z > zmax && (z - pz) < 250.0f && (z - pz) > -250.0f) {
                if (!did) was = z;
                ours = dvr::camera::clamp_location_z(g_camObj, kCamLoc[ci], zmax);
                did = true;
            }
            if (za) { zc.readable[ci] = true; zc.pre[ci] = z; zc.post[ci] = lp[2]; zc.ours[ci] = ours; }
        }
        if (za) { zc.ceilEased = zmax; zc.ran = true; }
        if (did) {
            static double tl = 0.0; double nw = MaimNowMs();
            if (nw - tl > 1000.0) { tl = nw;
                Log("eyeclamp: camZ %.1f -> %.1f (pawnZ %.1f cyl %.1f) - "
                    "the eye stays inside the capsule", was, zmax, pz,
                    g_cylLast);
            }
        }
    } else {
        dvr::camera::set_eye_ceiling(0.0f, false);
    }
    if (za) dvr::zacct::note_clamp(zc);
}
