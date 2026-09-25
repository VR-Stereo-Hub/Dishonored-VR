// game/dishonored/fov_lever.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

#include "game/dishonored/fov_lever_policy.h"
#include "game/dishonored/stereo_state_policy.h"

static inline void LevWrite(uint8_t* p, float t)
{
    if (!RangeReadable(p, 4)) return;
    float* f = (float*)p;
    if (*f > 5.0f && *f < 175.0f) *f = t;
}


static inline void FovLeverApply()
{
    static dvr::fov_lever::CinematicRecovery recovery;
    static unsigned recoveryEpoch = 0;
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
            float nat = *(float*)(g_camObj + kFovSensor);
            if (!(nat > 30.0f && nat < 140.0f)) return;
            g_fovNatural = nat;
            Log("fovlever: natural base %.1f deg, target %.2f, effective base %.2f (ratio %.3f; no contraction feedback)",
                nat, deg, nat < deg ? nat : deg, deg / (nat < deg ? nat : deg));
        }
        // VR-213: readback includes our own interpolated output. Never apply
        // a ratio below one to it; that recursively narrows toward the clamp.
        float sensor = g_fovNatural;
        if (g_camObj && RangeReadable(g_camObj + kFovSensor, 4)) {
            float s = *(float*)(g_camObj + kFovSensor);
            if (s > 10.0f && s < 175.0f) { sensor = s; dvr::camera::note_rendered_fov(s); }
        }
        float t = dvr::fov_lever::target(sensor, g_fovNatural, deg);
        if (t == 0.0f) { recovery = {}; return; }
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
        if (t < 20.0f)  t = 20.0f;
        if (t > 160.0f) t = 160.0f;
        if (IsLiveObject(g_peCtrl))
            for (int i = 0; i < 3; i++) LevWrite(g_peCtrl + kLevCtrl[i], t);
        if (IsLiveObject(g_camObj))
            for (int i = 0; i < 7; i++) {
                if (kLevCam[i] == kFovSensor) continue;             // never directly write readback
                LevWrite(g_camObj + kLevCam[i], t);
            }
        InterlockedIncrement(&g_fovLeverWrites);
        static double nextLog = 0;
        const double now = MaimNowMs();
        if (now >= nextLog) {
            nextLog = now + 1000;
            Log("fovlever: feedback sensor=%.2f natural=%.2f target=%.2f write=%.2f scoped=%d cinematicRecovery=%d master=%s",
                sensor, g_fovNatural, deg, t, scoped > 0, cinematicTarget > 0, state.state[0]);
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
