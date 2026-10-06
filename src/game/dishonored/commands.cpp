#include "core/gfx/reshade_runtime.h"   // the seam's `reshade effects on|off`
#include "core/framework/stage_profile.h"   // the seam's `stages on|off`
// game/dishonored/commands.cpp - the game side of the command seam and the
// status provider. Included by the unity build (it reads the mod's globals).
//
// Vocabulary (game words are tried before the core ones in command.h):
//   recenter                     same as F5
//   hands on|off                 the SkelControl hand drive
//   arms [probe|status]          VR-30: the arm-follow probe, read-only (also reports every 30 s)
//   blink on|off|probe           hand-aimed Blink; probe = one-shot survey
//   fov <deg|0>                  the FOV lever (0 disarms)
//   overlay on|off               the F10 settings panel
//   arms vis on|off|status|chain VR-31: per-bone arm hiding through the engine's
//                                own BoneVisibilityStates (ships off, live A/B)
//   arms mat census|hide <id>|show <id>|restore  VR-31 route (d): the material
//                                section census (auto, read-only) and hiding one
//                                section through the engine's own native
//   camera status                the per-eye camera seam (eye, ipd, field, fov, c5)
//   camera eyetest <uu> [field]  the write-point instrument (field 0x80|..|all; `camera eyetest stop`)
//   camera eyefield <name|none>  the field the eye offset writes to ([Camera] EyeField)
//   camera postest <R> [U] [F]   the positional instrument (uu along right/up/forward; `camera postest stop`)
//   postrack on|off|lane <l>     positional tracking and its lane (vp = the c0 patch, camera = the seam's write)
//   stereo <name>|status         the stereo method (mono|aer|reentry): live switch, fails soft
//   stereo projection on|off|auto  force/pin/follow the projection layer (on = the mono frame in both eyes of a projection layer)
//   reentry census|stack|probe|status  the scene-draw root instruments (game/dishonored/scene_probe.cpp)
//   capture mode <m>|status      the capture path (sync|deferred|shared|off): live switch, fails soft; off = the A/B control (frozen image)
//   capture sharedwait on|off    shared: deliver this present after its fence (on) or the previous slot (off, default)
//   device census|status         the creation census (core/gfx/device_census): the table and the 9Ex verdict
//   device upload                VR-15: the upload census - did the game's texture writes reach the GPU?
//   device shadowsurfaces on|off VR-15: redirect a GetSurfaceLevel lock to the twin (live A/B, ships off)
//   device ex on|off             [Device] Ex for the NEXT launch (the 9Ex device, core/gfx/d3d9ex)
//   device managed <m>           [Device] Managed=none|default|dynamic|shadow|paged for the NEXT launch
//   vrpace <args>                the runtime layer's pacing seam (on|off|thread|detach|feed|sync|spike|simidle|status)
//   vrmirror on|off|status       the desktop mirror pin (counted only on D3D9)
//   vrinput on|off|status        the virtual gamepad
//   swing status|on|off|mode edge|sustain|threshold|rearm|cooldown|pulse|polls|rel|filter raw|median|sword|output rt|rb|
//         log|force|sim <peak> [humpMs] [reps]|save   the motion sword (game/dishonored/swing.h) - VR-37
//   snapturn on|off|angle <deg>|threshold <v>|rearm <v>|repeat <ms>|fire [left|right]|mark|status
//                                     snap turn (game/dishonored/snap_turn.h) - VR-219
//   console <text>               run a game console command on the script lane
//   fullscreen on|off            VR-158: live fullscreen, through the engine's own
//                                resize (one device reset); windowed presents via DWM
//   vsync on|off                 VR-158: live vsync ([Perf] ForceNoVSync), which needs
//                                that same reset before UncapPresent can act
//   gameopts [read|system]       VR-157: READ the game's own option settings -
//                                the profile blob's value and the live
//                                SystemSettings mirror, side by side. Read-only.
//   dump frame|capture|eyes|hud [sink]
//   hud on|off|status|scale <f>  the HUD redirect (core/gfx/hud_capture) - VR-117
//   hud regions on|off           route elements by screen region (the probe)
//   hud anchor <el> <anchor>     an element's anchor: off|frame|window|hand
//   hud window|hand|place|region|menu|reset|layout   the layout (core/gfx/hud_layout)
//   draws on|off|status|regions|kill <key>|hud|unkill   the HUD draw census (core/gfx/hud_class)
//   cfg dump                     print the live values the seam can change
// Line numbers in comments refer to the original single file (commit 48766c07).

static char  g_dvrConsoleReq[256] = "";   // pending `console` text for the script lane
static char  g_dvrGameState[16] = "";     // last logged "[game] state:"

static bool DvrOnOff(const char* a, bool* out)
{
    if (!strcmp(a, "on") || !strcmp(a, "1")) { *out = true; return true; }
    if (!strcmp(a, "off") || !strcmp(a, "0")) { *out = false; return true; }
    return false;
}

static bool DvrGameCommand(const char* cmd, const char* args)
{
    bool b = false;
    if (!strcmp(cmd,"neckupright") && DvrOnOff(args,&b)) { dvr::camera::set_upright_pitch_arc(b); return true; }
    if (!strcmp(cmd,"movement")) {
        if(!strcmp(args,"head")) HeadMovementSet(true);
        else if(!strcmp(args,"character")) HeadMovementSet(false);
        else Log("movement: head | character; current=%s",HeadMovementEnabled()?"head":"character");
        return true;
    }
    if (!strcmp(cmd, "cineroll") && DvrOnOff(args, &b)) { CineRollSet(b); return true; }
    if (!strcmp(cmd, "cinepitch") && DvrOnOff(args, &b)) { CinePitchSet(b); return true; }
    if (!strcmp(cmd, "mantlehands") && DvrOnOff(args, &b)) { dvr::anim::set_mantle(b); return true; }
    if (!strcmp(cmd, "takedownarms") && DvrOnOff(args, &b)) { dvr::anim::set_takedown_arms_hidden(b); return true; }   // VR-283: on = arms hidden
    if (!strcmp(cmd, "cinehands") && DvrOnOff(args, &b)) { dvr::anim::set_cinematic(b); return true; }
    if (!strcmp(cmd, "cinefov") && DvrOnOff(args, &b)) { CineFovSet(b); return true; }
    if (!strcmp(cmd, "cinestereo") && DvrOnOff(args, &b)) { StereoStateSet(b); return true; }
    if (!strcmp(cmd, "possessionstereo") && DvrOnOff(args, &b)) { PossessionStereoSet(b); return true; }   // VR-135
    if (!strcmp(cmd, "rainrecovery") && DvrOnOff(args, &b)) { RainRecoverySet(b); return true; }
    if (!strcmp(cmd, "rainhide") && DvrOnOff(args, &b)) { RainHideSet(b); return true; }   // VR-136
    if (!strcmp(cmd, "swordtrail")) return SwordTrailCommand(args);   // VR-171
    if (!strcmp(cmd, "camshake")) return CamShakeCommand(args);   // VR-172
    if (!strcmp(cmd, "snapturn")) return dvr::snap::command(args);   // VR-219: snap turn
    if (!strcmp(cmd, "raindistance")) { RainDistanceSet(atoi(args)); return true; }   // VR-136: uu, -1 native
    if (!strcmp(cmd, "lensdistance")) { LensDistanceSet(atoi(args)); return true; }   // VR-137: uu, 0 native
    if (!strcmp(cmd, "lenskeepsize") && DvrOnOff(args, &b)) { LensKeepSizeSet(b); return true; }   // VR-137
    if (!strcmp(cmd, "lensfollow") && DvrOnOff(args, &b)) { LensFollowSet(b); return true; }       // VR-137
    if (!strcmp(cmd, "rainstrength")) { LensRainPctSet(atoi(args)); return true; }                 // VR-137: %, 100 native
    if (!strcmp(cmd, "heartback")) return HbCommand(args);
    if (!strcmp(cmd, "mirror")) return WmCommand(args);   // VR-138
    if (!strcmp(cmd, "occlusion")) return OcclusionCommand(args);   // VR-79
    if (!strcmp(cmd, "afw")) {   // VR-39: the held eye's stick/snap yaw correction, live A/B
        char sub[16] = "", v[8] = "";
        sscanf(args, "%15s %7s", sub, v);
        if (!strcmp(sub, "yaw") && DvrOnOff(v, &b)) { dvr::vr::set_held_body_yaw(b); return true; }
        if (!strcmp(sub, "warp") && DvrOnOff(v, &b)) { dvr::afw::set_enabled(b, "the seam"); return true; }
        if (!strcmp(sub, "body") && v[0]) { dvr::afw::set_body_depth((float)atof(v), "the seam"); return true; }
        if (!strcmp(sub, "stereo") && DvrOnOff(v, &b)) { dvr::afw::set_stereo(b, "the seam"); return true; }
        if (!strcmp(sub, "debug") && DvrOnOff(v, &b)) { dvr::afw::set_debug(b, "the seam"); return true; }
        if (!strcmp(sub, "matrices") && DvrOnOff(v, &b)) { dvr::afw::set_matrices(b, "the seam"); return true; }
        if (!strcmp(sub, "fg") && DvrOnOff(v, &b)) { dvr::afw::set_fg(b, "the seam"); return true; }
        if (!strcmp(sub, "fgmask") && DvrOnOff(v, &b)) { dvr::afw::set_fg_mask(b, "the seam"); return true; }
        if (!strcmp(sub, "ownhands") && v[0]) { dvr::afw::set_own_hands((float)atof(v), "the seam"); return true; }
        if (!strcmp(sub, "clean") && DvrOnOff(v, &b)) { dvr::afw::set_clean(b, "the seam"); return true; }
        if (!strcmp(sub, "stillshade") && DvrOnOff(v, &b)) { dvr::afw::set_still_shade(b, "the seam"); return true; }
        if (!strcmp(sub, "edgehands") && DvrOnOff(v, &b)) { dvr::afw::set_edge_hands(b, "the seam"); return true; }
        if (!strcmp(sub, "typedsrgb") && DvrOnOff(v, &b)) { dvr::afw::set_typed_srgb_decode(b, "the seam"); return true; }
        if (!strcmp(sub, "heldhands") && DvrOnOff(v, &b)) { dvr::afw::set_held_hands(b, "the seam"); return true; }
        if (!strcmp(sub, "stale") && v[0]) { dvr::afw::set_stale((float)atof(v), "the seam"); return true; }
        if (!strcmp(sub, "nearmiss") && v[0]) { dvr::afw::set_near_miss((float)atof(v), "the seam"); return true; }
        if (!strcmp(sub, "fgdepth") && v[0]) { dvr::afw::set_fg_depth((float)atof(v), "the seam"); return true; }
        if (!strcmp(sub, "dump")) { dvr::afw::request_dump(v[0] ? atoi(v) : 16, 0, dvr::paths::dumps_dir(), "the seam"); return true; }
        Log("afw: warp on|off (now %s) | stereo on|off (now %s, the hands from the fresh eye) | matrices on|off (now "
            "%s, walking in the held eye's world) | debug on|off (now %s, tint the held eye by source) | body <depth "
            "units> (now %.2f) | yaw on|off (now %s, the rotation-only fallback) - the method is `stereo afw`, active "
            "'%s'", dvr::afw::enabled() ? "on" : "off", dvr::afw::stereo() ? "on" : "off",
            dvr::afw::matrices() ? "on" : "off", dvr::afw::debug() ? "on" : "off", dvr::afw::body_depth(),
            dvr::vr::held_body_yaw() ? "on" : "off", dvr::stereo::active_name());
        return true;
    }
    if (!strcmp(cmd, "aer")) {   // VR-39: `stereo aer` selects the method; this word drives its clamp
        char sub[16] = "", v[16] = "";
        sscanf(args, "%15s %15s", sub, v);
        if (!strcmp(sub, "clamp") && DvrOnOff(v, &b)) { DeltaClampSet(b, "the seam"); return true; }
        if (DeltaClampCommand(sub, v)) return true;
        Log("aer: clamp on|off | lever bendtime|timedilation (clamp now %s; the method is `stereo aer`, active '%s') - "
            "the delta clamp: one world advance per eye pair", DeltaClampEnabled() ? "on" : "off", dvr::stereo::active_name());
        return true;
    }
    if (!strcmp(cmd, "cineborders") && DvrOnOff(args, &b)) { CineBordersSet(b); return true; }
    if (!strcmp(cmd, "uiguard") && DvrOnOff(args, &b)) { UiSurfaceSet(b); return true; }
    if (!strcmp(cmd, "monoanchor")) {
        if (!strcmp(args,"recenter")) dvr::vr::recenter_mono_anchor();
        else if (DvrOnOff(args,&b)) dvr::vr::set_mono_anchor(b,dvr::vr::mono_anchor_contexts());
        return true;
    }
    if (!strcmp(cmd, "cinehead") && DvrOnOff(args, &b)) { CineHeadSet(b); return true; }
    if (!strcmp(cmd, "cinetrace") && DvrOnOff(args, &b)) { CineTraceSet(b); return true; }
    if (!strcmp(cmd, "recenter")) { RecenterHead(); return true; }
    // VR-30: the arm-follow probe. Read-only, reports on its own every 30 s.
    if (!strcmp(cmd, "arms") && !strcmp(args, "yawtest")) { YawSelfTest(); return true; }
    if (!strcmp(cmd, "arms")) { if (ArmFollowCommand(args)) return true;
        Log("arms: usage - arms [probe|status|yawtest]  (read-only; it also reports itself every 30 s)");
        return true; }
    // VR-122: the camera half of the crouched tuck, live (`hands tuckcam on|off`).
    if (!strcmp(cmd, "hands") && !strncmp(args, "tuckcam", 7)) {
        const char* v = args + 7;
        while (*v == ' ') ++v;
        if (DvrOnOff(v, &b)) SkcTuckCameraSet(b, "seam");
        else Log("hands: tuckcam on|off (now %s, tucked=%d, camera slot %d)",
                 g_crawlTuckCamera ? "ON" : "off", (int)g_skcTucked, g_skcCamIdx);
        return true;
    }
    if (!strcmp(cmd, "hands") && DvrOnOff(args, &b)) {
        g_skcDrive = b; g_handMesh = b; g_autoHandDone = true;
        Log("hands: %s (seam)", b ? "ON" : "off");
        return true;
    }
    // 41.2 (VR-31): the mod's own drawn hands - the live A/B for [VRHands]
    // Enabled. Ships OFF; `vrhands on` needs a projection method (reentry) to
    // show anything, and says so on the log if it does not have one.
    if (!strcmp(cmd, "vrhands")) {
        if (DvrOnOff(args, &b)) {
            g_hmEnable = b;
            g_hmNextBeat = 0.0;                    // a clean beat window per arming
            g_hmCalls = g_hmDraws = g_hmTris = 0;
            Log("vrhands: %s (seam) - method '%s'%s. The beat line every 3 s "
                "says calls/draws/tris and names any zero.",
                b ? "ON" : "off", dvr::stereo::active_name(),
                (b && !dvr::stereo::wants_projection())
                    ? ", which is NOT a projection layer: nothing will draw until `stereo reentry`"
                    : "");
            return true;
        }
        if (!strcmp(args, "calib on") || !strcmp(args, "calib off")) {
            g_hmCalib = !strcmp(args, "calib on");
            Log("vrhands: calibration triangle %s (seam)", g_hmCalib ? "ARMED" : "off");
            return true;
        }
        if (!strcmp(args, "status")) {
            Log("vrhands: %s | method '%s' projection=%d | pipeline=%d depth=%ux%u "
                "| models L=%d R=%d scale %.2f | last %s",
                g_hmEnable ? "ON" : "off", dvr::stereo::active_name(),
                (int)dvr::stereo::wants_projection(), (int)g_hmReady,
                g_hmDepthW, g_hmDepthH, g_hmModel[0], g_hmModel[1], g_hmScale,
                HmWhy(g_hmLastWhy));
            return true;
        }
        Log("vrhands: usage - vrhands on|off|status|calib on|calib off");
        return true;
    }
    if (!strcmp(cmd, "anim")) return dvr::anim::command(args);   // VR-88: shipped, not legacy
    if (!strcmp(cmd, "swing")) return dvr::swing::command(args);  // VR-37: the motion sword
    if (!strcmp(cmd, "drop")) return dvr::drop::command(args);    // drop takedowns from above
    if (!strcmp(cmd, "dc")) return DcCommand(args);
    if (!strcmp(cmd, "ms")) return MsCommand(args);
    if (!strcmp(cmd, "pose")) return PrCommand(args);
#if DVR_WITH_LEGACY
    if (!strcmp(cmd, "bq")) return BqCommand(args);
#endif
#if DVR_WITH_LEGACY
    if (!strcmp(cmd, "handmove")) return HmCommand(args);
#endif
#if DVR_WITH_LEGACY
    if (!strcmp(cmd, "pcap")) return PcCommand(args);
    if (!strcmp(cmd, "rfl")) return RflCommand(args);
    if (!strcmp(cmd, "startup")) return SuCommand(args);
    if (!strcmp(cmd, "uistate")) return UiCommand(args);
#endif
    if (!strcmp(cmd, "blink")) {
        if (!strcmp(args, "probe")) { BlinkProbeArm(); return true; }
        // VR-36: the A/B between the published ray and the legacy MotionAim one,
        // live. `blink ray` alone reports which is driving and why.
        if (!strncmp(args, "ray", 3)) {
            const char* a = args + 3;
            while (*a == ' ') a++;
            if (!strcmp(a, "aim") || !strcmp(a, "published")) g_blkUseAimRay = true;
            else if (!strcmp(a, "legacy") || !strcmp(a, "motionaim")) g_blkUseAimRay = false;
            else if (*a) return false;
            Log("blink: ray=%s | ControllerAim=%d AimAtSource=%d (source seam, the "
                "engine still traces and refuses) | last ray: %s, controller %.0f uu "
                "from the camera | destination seam read-only, its settled point sits "
                "%.2f deg off the handed vector (-1 = not measured yet)",
                g_blkUseAimRay ? "published (shared with the dot and the shots)"
                               : "legacy MotionAim",
                (int)g_blkAimOnCfg, (int)g_blkDirAim, g_blkRayWhy, g_blkRayGapUU,
                g_blkDstAngleDeg);
            return true;
        }
        if (DvrOnOff(args, &b)) { g_blkAimOnCfg = b; g_blkDriveUI = b; Log("blink: hand aim %s (seam)", b ? "ON" : "off"); return true; }
        return false;
    }
    if (!strcmp(cmd, "projectionfov")) {
        float f=0; char extra=0;
        if (!strcmp(args,"off")) ProjectionFovSet(0);
        else if (sscanf(args,"%f %c",&f,&extra)==1) ProjectionFovSet(f);
        else Log("projectionfov: %.2f; use 60..120 or off",ProjectionFovGet());
        return true;
    }
    if (!strcmp(cmd, "fov")) {
        float f = (float)atof(args);
        if (f != 0.0f && (f < 40.0f || f > 150.0f)) { Log("fov: %g out of range (40..150, or 0 to disarm)", f); return true; }
        g_fovLever = f;
        dvr::camera::set_fov_deg(g_fovLever);
        Log("fov: lever -> %.0f (seam)", f);
        return true;
    }
    if (!strcmp(cmd, "overlay") && DvrOnOff(args, &b)) { g_ovlVisible = b; return true; }
    if (!strcmp(cmd, "arms")) return ArmsCommand(args);   // VR-31: the per-bone visibility lever
    if (!strcmp(cmd, "res") && !strncmp(args, "live ", 5)) {
        // Uncap deep dive (2026-09-27): a LIVE render size for the A/B plan's scaling test,
        // through the engine resize the F10 control and DLSS SR already use (ResLiveQueue:
        // byte-verified, refuses with a reason). Nothing is written to either ini, so the
        // next launch renders the configured size again. `res live pct <25..200>` is a percent
        // per axis of the configured [Screen] RenderWidth/Height; `res live <W>x<H>` is exact.
        unsigned w = 0, h = 0, pct = 0;
        static uint32_t baseW = 0, baseH = 0;   // the configured size, before any live change moved g_resWant
        if (!baseW && g_resWantW && g_resWantH) { baseW = g_resWantW; baseH = g_resWantH; }
        if (sscanf(args + 5, "pct %u", &pct) == 1 && pct >= 25 && pct <= 200 && baseW && baseH) {
            w = (baseW * pct / 100 + 1) & ~1u; h = (baseH * pct / 100 + 1) & ~1u;
        } else if (sscanf(args + 5, "%ux%u", &w, &h) != 2) {
            Log("res live: pct <25..200> (of the configured %ux%u) | <W>x<H> - nothing written to the ini", baseW, baseH);
            return true;
        }
        Log("res live: asking %ux%u for this session only (configured %ux%u stays in the ini)", w, h, baseW, baseH);
        g_resLiveSession.store(true);
        ResLiveQueue(w, h);
        return true;
    }
    if (!strcmp(cmd, "res")) return ResCommand(args);   // 41.1: the render-resolution picker
    if (!strcmp(cmd, "neck")) {
        // 41.1: `neck off|add|cancel [below] [behind]` - the pitch pivot lever (head_track.cpp NeckSet)
        char mode[16] = "";
        float below = g_neckBelowM, behind = g_neckBehindM;
        const int n = sscanf(args, "%15s %f %f", mode, &below, &behind);
        if (n >= 1 && !_stricmp(mode, "crouch")) {
            // VR-78: `neck crouch same` or `neck crouch <below> <behind>`
            float cb = -1.0f, cf = -1.0f;
            if (strstr(args, "same") || sscanf(args, "%*s %f %f", &cb, &cf) == 2) NeckCrouchSet(cb, cf, "seam");
            else Log("neck: crouch same|<below m> <behind m> (now %.3f / %.3f, -1 = standing)", g_neckCrouchBelowM, g_neckCrouchBehindM);
            return true;
        }
        if (n >= 1) {
            const int m = !_stricmp(mode, "off") ? 0 : !_stricmp(mode, "add") ? 1 : !_stricmp(mode, "cancel") ? 2 : -1;
            if (m < 0) { Log("neck: off|add|cancel [below m] [behind m] (now %s %.3f/%.3f)", NeckModeName(g_neckMode), g_neckBelowM, g_neckBehindM); return true; }
            NeckSet(m, below, behind, "seam");
            return true;
        }
        Log("neck: mode %s, pivot below %.3f m behind %.3f m, arc now R%+.1f U%+.1f F%+.1f uu (neck off|add|cancel [below] [behind])",
            NeckModeName(g_neckMode), g_neckBelowM, g_neckBehindM, g_neckArcUu[0], g_neckArcUu[1], g_neckArcUu[2]);
        return true;
    }
    if (!strcmp(cmd, "postrack")) {
        char sub[16] = "", lane[16] = "";
        if (sscanf(args, "%15s %15s", sub, lane) == 2 && !strcmp(sub, "lane")) {
            dvr::camera::set_pos_lane(lane);   // logs the refusal itself
            return true;
        }
        if (DvrOnOff(args, &b)) {
            g_posTrack = b;
            if (!b) { g_leanRightUU = 0; g_leanUpUU = 0; g_leanFwdUU = 0; }
            Log("postrack: %s (seam)", b ? "ON" : "off");
            return true;
        }
        float pos[3]; dvr::camera::position_offset_uu(pos);
        Log("postrack: %s lane=%s offset R%+.1f U%+.1f F%+.1f uu scale=%.0f uu/m (postrack on|off|lane vp|camera)",
            g_posTrack ? "ON" : "off", dvr::camera::pos_lane_name(), pos[0], pos[1], pos[2], g_posScaleUU);
        return true;
    }
    if (!strcmp(cmd, "camera")) {
        char sub[32] = "", fld[16] = "all";
        float uu = 0.0f;
        if (sscanf(args, "%31s", sub) == 1 && !strcmp(sub, "eyetest")) {
            if (strstr(args, "stop")) { dvr::camera::eyetest_stop("seam"); return true; }
            if (dvr::stereo::reentry_family_active()) {
                Log("camera/eyetest: refused while the %s method is active (presents with different eyes would "
                    "destroy the verdict) - `stereo mono` first", dvr::stereo::active_name());
                return true;
            }
            sscanf(args, "%*s %f %15s", &uu, fld);
            dvr::camera::eyetest_start(uu > 0.0f ? uu : 100.0f, fld);
            return true;
        }
        if (!strcmp(sub, "postest")) {
            if (strstr(args, "stop")) { dvr::camera::postest_stop("seam"); return true; }
            float r = 0.0f, u = 0.0f, f = 0.0f;
            const int n = sscanf(args, "%*s %f %f %f", &r, &u, &f);
            if (n < 1) { Log("camera: postest <R> [U] [F] in uu (e.g. `camera postest 30 0 0` = lean 30 cm right at 100 uu/m)"); return true; }
            dvr::camera::postest_start(r, u, f);
            return true;
        }
        if (!strcmp(sub, "pitchtest")) {
            if (strstr(args, "stop")) { dvr::camera::pitchtest_stop("seam"); return true; }
            float deg = 30.0f;
            sscanf(args, "%*s %f", &deg);
            dvr::camera::pitchtest_start(deg);
            return true;
        }
        if (!strcmp(sub, "zaccount")) {
            // VR-78: the vertical accounting probe (z_account.h)
            char v[16] = "";
            sscanf(args, "%*s %15s", v);
            if (!_stricmp(v, "on")) dvr::zacct::set_enabled(true, "seam");
            else if (!_stricmp(v, "off")) dvr::zacct::set_enabled(false, "seam");
            else if (!_stricmp(v, "reset")) dvr::zacct::reset("seam reset");
            else if (!_stricmp(v, "roll") || !_stricmp(v, "pitch")) {
                // VR-91: `camera zaccount roll [on|off]` / `... pitch`. Roll mode
                // bins by head roll and measures laterally; the pitch mode rejects
                // rolled samples outright, so it cannot answer a roll question.
                char m[16] = "";
                sscanf(args, "%*s %*s %15s", m);
                const bool wantRoll = !_stricmp(v, "roll") ? _stricmp(m, "off") != 0
                                                           : _stricmp(m, "off") == 0;
                dvr::zacct::set_roll_mode(wantRoll, "seam");
                if (!dvr::zacct::enabled()) dvr::zacct::set_enabled(true, "seam (roll mode implies on)");
            }
            else dvr::zacct::log_status();
            return true;
        }
        if (!strcmp(sub, "eyefield")) {
            fld[0] = 0;
            sscanf(args, "%*s %15s", fld);
            dvr::camera::set_eye_field(fld);
            return true;
        }
        dvr::camera::log_status();
        return true;
    }
    if (!strcmp(cmd, "stereo")) {
        if (!args[0] || !strcmp(args, "status")) { dvr::stereo::log_status(); return true; }
        char sub[16] = "", v[16] = "";
        if (sscanf(args, "%15s %15s", sub, v) == 2 && !strcmp(sub, "projection")) {
            if (!strcmp(v, "auto")) dvr::stereo::set_projection_override(-1);
            else if (DvrOnOff(v, &b)) dvr::stereo::set_projection_override(b ? 1 : 0);
            else Log("stereo: projection on|off|auto");
            return true;
        }
        if (!strcmp(sub, "hold")) {   // 41.1: the untagged hold (the one-frame mono flicker)
            int n = -1;
            if (sscanf(v, "%d", &n) == 1) dvr::stereo::set_hold_untagged(n);
            else Log("stereo: hold <n> - hold up to n consecutive UNTAGGED presents back so the compositor "
                     "keeps the previous pair instead of flipping both eyes to mono (now %d, %lu held this "
                     "run); 0 = off", dvr::stereo::hold_untagged(), (unsigned long)dvr::stereo::holds_done());
            return true;
        }
        if (!strcmp(sub, "arm")) {   // 41.1: the tickbox on the seam
            if (DvrOnOff(v, &b)) dvr::stereo::set_armed(b);
            else Log("stereo: arm on|off (now %s, selected '%s')", dvr::stereo::armed() ? "armed" : "parked", dvr::stereo::wanted_name());
            return true;
        }
        dvr::stereo::choose(args);   // logs the refusal itself; an explicit choice is the selection
        return true;
    }
    if (!strcmp(cmd, "capture")) {
        char sub[16] = "", m[16] = "";
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "mode")) {
            dvr::capture::set_mode(m);   // logs the refusal itself
            return true;
        }
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "sharedwait") && DvrOnOff(m, &b)) {
            dvr::capture::set_shared_wait(b);
            return true;
        }
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "timeout") &&
            (!strcmp(m, "deliver") || !strcmp(m, "refuse"))) {   // the live A/B for [Capture] TimeoutRefuse
            dvr::capture::set_timeout_refuse(!strcmp(m, "refuse"), "the seam");
            return true;
        }
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "depth")) {   // uncap deep dive
            dvr::capture::set_shared_depth(atoi(m), "the seam");
            return true;
        }
        if (!strcmp(args, "reinit")) { dvr::capture::request_reinit(); return true; }   // 41.1 (session 9)
        // 41.1: the content-bbox cadence. Each sample is a full-frame CPU
        // readback on the present thread even in shared mode - see capture.h.
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "bbox")) {
            if (!strcmp(m, "off")) { dvr::capture::set_bbox_ms(0); return true; }
            unsigned ms = 0;
            if (sscanf(m, "%u", &ms) == 1) { dvr::capture::set_bbox_ms(ms); return true; }
            Log("capture: bbox wants off or an interval in ms (capture bbox off|<ms>) - got '%s'", m);
            return true;
        }
        const dvr::capture::Cost c = dvr::capture::cost();
        Log("capture: mode=%s probe=%s cost/present rtd=%u lock=%u copy=%u upload=%u blit=%u total=%u us "
            "(%u grabs) delivered serial %lu of %lu tag=%d slot=%d sharedWait=%d fenceWaits=%u timeouts=%u readWaits=%u "
            "readTimeouts=%u reinits=%u bboxEvery=%ums(%u samples, each a full-frame CPU readback) "
            "(capture mode sync|deferred|shared|off, capture sharedwait on|off, capture depth 1|2|3, capture bbox off|<ms>, "
            "capture reinit)",
            dvr::capture::mode_name(),
            !dvr::capture::probed() ? "not yet" : dvr::capture::shared_available() ? "shared AVAILABLE" : "shared REFUSED",
            c.rtdUs, c.lockUs, c.copyUs, c.uploadUs, c.blitUs, c.totalUs, c.grabsInWindow,
            (unsigned long)dvr::capture::delivered_serial(), (unsigned long)dvr::capture::serial(),
            dvr::capture::delivered_tag(), dvr::capture::delivered_slot(), dvr::capture::shared_wait() ? 1 : 0,
            dvr::capture::fence_waits(), dvr::capture::fence_timeouts(), dvr::capture::read_waits(),
            dvr::capture::read_timeouts(), dvr::capture::reinits(), dvr::capture::bbox_ms(),
            dvr::capture::bbox_samples());
        return true;

    }
    if (!strcmp(cmd, "device")) {   // 41.1 (session 8): the creation census and the 9Ex levers
        if (!args[0] || !strcmp(args, "status")) { dvr::census::log_status(); dvr::d3d9ex::log_status(); return true; }
        if (!strcmp(args, "census")) { dvr::census::log_summary("device census"); return true; }
        // VR-15: did what the game wrote into a texture ever reach the GPU?
        if (!strcmp(args, "upload")) { dvr::census::log_upload("device upload"); return true; }
        char sub[16] = "", v[16] = "";
        if (sscanf(args, "%15s %15s", sub, v) == 2) {
            if (!strcmp(sub, "ex") && DvrOnOff(v, &b)) { DeviceSetEx(b, "seam"); return true; }
            if (!strcmp(sub, "managed")) { DeviceSetManaged(v, "seam"); return true; }
            // VR-15: the surface-bypass redirect, live A/B (no relaunch)
            if (!strcmp(sub, "shadowsurfaces") && DvrOnOff(v, &b)) { dvr::census::set_shadow_surfaces(b); return true; }
            // VR-15: the per-level push, the candidate fix for black-at-distance
            if (!strcmp(sub, "shadowfullcopy") && DvrOnOff(v, &b)) { dvr::d3d9ex::set_full_copy(b); return true; }
        }
        Log("device: usage - device census|status|upload | device ex on|off | device managed none|default|dynamic|shadow|paged "
            "| device shadowsurfaces on|off | device shadowfullcopy on|off");
        return true;
    }
    if (!strcmp(cmd, "pe")) {   // route 2: the script lane's fast path A/B (ue3/pe_fast.h)
        char sub[16] = "", val[16] = "";
        const int n = args ? sscanf(args, "%15s %15s", sub, val) : 0;
        bool on;
        if (n >= 2 && !strcmp(sub, "fast") && DvrOnOff(val, &on)) {
            InterlockedExchange(&g_peFast, on ? 1 : 0);
            ConfigWriteKey("Perf", "PeFast", on ? "1" : "0", "the seam");
            Log("pe: script-lane fast path %s (live) - the pe/cost line reports the difference", on ? "ON" : "off");
            return true;
        }
        if (n >= 2 && !strcmp(sub, "fn") && DvrOnOff(val, &on)) {
            InterlockedExchange(&g_peFnOn, on ? 1 : 0);
            Log("pe: per-statement cost split %s (pe/cost-fn every 5 s; diagnostic, not saved)", on ? "ON" : "off");
            return true;
        }
        if (n >= 2 && !strcmp(sub, "heavydraw") && DvrOnOff(val, &on)) {
            InterlockedExchange(&g_peHeavyInDraw, on ? 1 : 0);
            ConfigWriteKey("Perf", "PeHeavyInDraw", on ? "1" : "0", "the seam");
            Log("pe: heavy writers inside the draw %s", on ? "EVERY event (safe default)" : "throttled like the tick");
            return true;
        }
        if (n >= 2 && !strcmp(sub, "heavy")) {
            PeHeavySet(atoi(val));
            char v[8]; _snprintf(v, sizeof(v), "%ld", InterlockedCompareExchange(&g_peHeavyUs, 0, 0) / 1000);
            ConfigWriteKey("Perf", "PeHeavyMs", v, "the seam");
            return true;
        }
        Log("pe: fast on|off, heavy <ms> (now fast %s, heavy %ld ms) - the ProcessEvent hook's caches and cadence",
            InterlockedCompareExchange(&g_peFast, 0, 0) ? "on" : "off", InterlockedCompareExchange(&g_peHeavyUs, 0, 0) / 1000);
        return true;
    }
    if (!strcmp(cmd, "reentry")) {
        if (SceneDrawCommand(args)) return true;
        if (SceneProbeCommand(args)) return true;
        Log("reentry: pulse [n] | skip2 [n] | rearm [n] | c5pair on|off | latetag on|off | reset | hook on|off | status | census on|off|report | stack event <name>|caller <hex>|present|off | probe <hex> [len] | findstart <hex>");

        return true;
    }
    if (!strcmp(cmd, "vrpace"))   { dvr::vr::handle_pace_command(args); return true; }
    if (!strcmp(cmd, "crosshair")) { dvr::aim::command(args); return true; }
    if (!strcmp(cmd, "fireaim")) {
        bool on;
        if (DvrOnOff(args,&on)) FireAimSet(on,"command seam");
        else Log("fireaim: %s; use fireaim on|off",FireAimEnabled()?"ON":"off");
        return true;
    }
    if (!strcmp(cmd, "armslens")) {   // VR-39: armslens [gain <x>]
        const char* a = args ? args : "";
        while (*a == ' ') ++a;
        if (!strncmp(a, "gain", 4)) { const float g = (float)atof(a + 4); if (g > 0.0f) AfwFgGainSet(g, "command seam"); }
        char st[160]; ArmsLensStatus(st, sizeof(st));
        Log("armslens: hands switch %s, AFW foreground gain %.3f | %s (armslens gain <0.80-1.00>)",
            HandsWorldFovGet() ? "ON" : "off", AfwFgGainGet(), st);
        return true;
    }
    if (!strcmp(cmd, "propwatch")) {
        bool on;
        if (DvrOnOff(args,&on)) PwSet(on,"command seam");
        else Log("propwatch: %s; use propwatch on|off",PwEnabled()?"ON":"off");
        return true;
    }
    if (!strcmp(cmd, "vrmirror")) { dvr::vr::handle_mirror_command(args); return true; }
    if (!strcmp(cmd, "vrinput")) {
        if (DvrOnOff(args, &b)) { g_padEnabled = b; Log("input: virtual pad %s (seam)", b ? "ON" : "off"); return true; }
        Log("input: pad %s active=%d polls=%ld actions=%s haptics=%d (vrinput on|off|status)",
            g_padEnabled ? "enabled" : "disabled", (int)g_padActive, (long)g_padPolls,
            dvr::vr::input_attached() ? "attached" : "not attached", (int)(g_padHaptics && g_xrHaptics));
        return true;
    }
    if (!strcmp(cmd, "binds")) {   // controller bind remapping: binds status|reset|swap on|off|<Action> <Source>
        auto l = dvr::binds::layout();
        char act[48] = "", src[48] = "";
        sscanf_s(args, "%47s %47s", act, (unsigned)sizeof(act), src, (unsigned)sizeof(src));
        int a = 0; dvr::binds::Source s;
        if (!act[0] || !_stricmp(act, "status")) BindsLog("asked");
        else if (!_stricmp(act, "reset")) BindsSet(dvr::binds::Layout{}, "the seam");
        else if (!_stricmp(act, "swap") && DvrOnOff(src, &b)) { l.swapSticks = b; BindsSet(l, "the seam"); }
        else if (dvr::binds::parse_action(act, &a) && dvr::binds::parse_source(src, &s)) { l.src[a] = s; BindsSet(l, "the seam"); }
        else Log("input/binds: use binds status | reset | swap on|off | <Action> <Source> (asked '%s')", args);
        return true;
    }
    if (!strcmp(cmd, "console")) {
        strncpy(g_dvrConsoleReq, args, sizeof(g_dvrConsoleReq) - 1);
        g_dvrConsoleReq[sizeof(g_dvrConsoleReq) - 1] = 0;
        Log("console: queued '%s' for the script lane", g_dvrConsoleReq);
        return true;
    }
    if (!strcmp(cmd, "gameopts")) return GameOptsCommand(args);   // VR-157
    // VR-158: the live display A/B. Both provoke one device reset.
    if (!strcmp(cmd, "fullscreen")) {
        if (DvrOnOff(args, &b)) { ResLiveSetFullscreen(b, "seam"); ConfigWriteKey("Screen", "RenderFullscreen", b ? "1" : "0", "the seam"); return true; }
        Log("res/live: fullscreen on|off (device is %s; `vsync on|off` is the other half)",
            ResLiveFullscreen() ? "fullscreen" : "windowed");
        return true;
    }
    if (!strcmp(cmd, "vsync")) {
        if (DvrOnOff(args, &b)) { ResLiveSetVsync(b, "seam"); ConfigWriteKey("Perf", "ForceNoVSync", b ? "0" : "1", "the seam"); return true; }
        Log("res/live: vsync on|off (present is %s; ForceNoVSync=%d)",
            g_forceNoVSync ? "uncapped" : "vsynced", (int)g_forceNoVSync);
        return true;
    }
    if (!strcmp(cmd, "cammod")) return CamModCommand(args);       // VR-165
    if (!strcmp(cmd, "camspring")) return CamSpringCommand(args); // VR-165: kick a camera spring on demand
    if (!strcmp(cmd, "aimsrc")) return AimSourceCommand(args);    // VR-166
    if (!strcmp(cmd, "interactaim")) return InteractAimCommand(args); // VR-166
    if (!strcmp(cmd, "grab")) return GrabAnimCommand(args);              // the grab animation (hands/mesh_split.cpp)
    if (!strcmp(cmd, "pickup")) return PickupCommand(args);           // loot picked up by reaching for it
    if (!strcmp(cmd, "throwaim")) return ThrowAimCommand(args);    // VR-166
    if (!strcmp(cmd, "gadgetaim")) return GadgetAimCommand(args);  // VR-166
    if (!strcmp(cmd, "carryaim")) return CarryThrowAimCommand(args); // VR-181
    if (!strcmp(cmd, "poweraim")) return PowerAimCommand(args);    // VR-44
    // VR-165: not "swing" - that word is the motion sword's (VR-37) on VR-Main.
    if (!strcmp(cmd, "swingtrace")) return SwingTraceCommand(args);  // VR-165
    if (!strcmp(cmd, "dump")) {
        FrameDumpRequest(args[0] ? args : "frame");
        return true;
    }
    if (!strcmp(cmd, "hud")) {   // VR-117: the HUD redirect and its layout
        if (!strncmp(args, "regions", 7)) {
            const char* a = args + 7;
            while (*a == ' ') ++a;
            if (DvrOnOff(a, &b)) { dvr::hudclass::set_regions_enabled(b); ConfigWriteKey("Hud", "Regions", b ? "1" : "0", "the seam"); }
            else dvr::hudclass::log_regions("asked");
            return true;
        }
        if (DvrOnOff(args, &b)) ConfigWriteKey("Hud", "Panel", b ? "1" : "0", "the seam");
        return dvr::hudcap::command(args);
    }
    if (!strcmp(cmd, "draws")) {
        if (DvrOnOff(args, &b)) ConfigWriteKey("Draws", "Census", b ? "1" : "0", "the seam");
        return dvr::hudclass::command(args);
    }
    if (!strcmp(cmd, "clarity")) {   // anti-aliasing and clarity on the eye image (core/gfx/clarity.h)
        const bool ok = dvr::clarity::command(args);
        ConfigWriteKey("Clarity", "Resolve", dvr::clarity::resolve_on() ? "1" : "0", "the seam");
        ConfigWriteKey("Clarity", "Temporal", dvr::clarity::temporal_on() ? "1" : "0", "the seam");
        ConfigWriteKey("Clarity", "MotionVectors", dvr::clarity::motion_on() ? "1" : "0", "the seam");
        char v[16];
        _snprintf(v, sizeof(v), "%.1f", dvr::clarity::depth_scale());
        ConfigWriteKey("Clarity", "MotionDepthScale", v, "the seam");
        _snprintf(v, sizeof(v), "%.2f", dvr::clarity::blend());
        ConfigWriteKey("Clarity", "TemporalBlend", v, "the seam");
        _snprintf(v, sizeof(v), "%.2f", dvr::clarity::sharpen());
        ConfigWriteKey("Clarity", "Sharpen", v, "the seam");
        return ok;
    }
    if (!strcmp(cmd, "dlss")) {      // NVIDIA DLAA through the x64 helper (core/gfx/dlss.h)
        const bool ok = dvr::dlss::command(args);
        ConfigWriteKey("Clarity", "DLAA", dvr::dlss::mode() ? "1" : "0", "the seam");
        char v[16];
        _snprintf(v, sizeof(v), "%d", dvr::dlss::preset());
        ConfigWriteKey("Clarity", "DlssPreset", v, "the seam");
        _snprintf(v, sizeof(v), "%d", dvr::dlss::quality());
        ConfigWriteKey("Clarity", "DlssQuality", v, "the seam");
        ConfigWriteKey("Clarity", "DlssModel", dvr::dlss::model() ? "1" : "0", "the seam");
        {
            uint32_t dow = 0, doh = 0; dvr::dlss::output(&dow, &doh);
            _snprintf(v, sizeof(v), "%u", dow); ConfigWriteKey("Clarity", "DlssOutputWidth", v, "the seam");
            _snprintf(v, sizeof(v), "%u", doh); ConfigWriteKey("Clarity", "DlssOutputHeight", v, "the seam");
        }
        ConfigWriteKey("Clarity", "DlssMask", dvr::dlss::mask_on() ? "1" : "0", "the seam");
        ConfigWriteKey("Clarity", "DlssJitter", dvr::dlss::jitter::enabled() ? "1" : "0", "the seam");
        ConfigWriteKey("Clarity", "DlssJitterWide", dvr::dlss::jitter::wide() ? "1" : "0", "the seam");
        _snprintf(v, sizeof(v), "%.3f", dvr::dlss::mask_lo());
        ConfigWriteKey("Clarity", "DlssMaskLo", v, "the seam");
        _snprintf(v, sizeof(v), "%.3f", dvr::dlss::mask_hi());
        ConfigWriteKey("Clarity", "DlssMaskHi", v, "the seam");
        _snprintf(v, sizeof(v), "%.2f", dvr::clarity::body_depth());
        ConfigWriteKey("Clarity", "DlssBodyDepth", v, "the seam");
        return ok;
    }
    if (!strcmp(cmd, "stages")) {   // pre-release audit: the engine's own render stages, timed (core/framework/stage_profile.h)
        // `stages on|off` sets the engine's draw-event switch (byte-verified) and the collector together;
        // `stages gpu on|off` adds GPU time from timestamp queries. Session only, nothing is saved.
        static int verified = 0;
        if (!verified) {
            const bool a = RangeReadable((const void*)kEmitDrawEventsReader, sizeof(kEmitDrawEventsReaderBytes)) &&
                           !memcmp((const void*)kEmitDrawEventsReader, kEmitDrawEventsReaderBytes, sizeof(kEmitDrawEventsReaderBytes));
            const bool b = RangeReadable((const void*)kEmitDrawEventsToggle, sizeof(kEmitDrawEventsToggleBytes)) &&
                           !memcmp((const void*)kEmitDrawEventsToggle, kEmitDrawEventsToggleBytes, sizeof(kEmitDrawEventsToggleBytes));
            const bool g = RangeReadable((const void*)kEmitDrawEvents, 4);
            verified = a && b && g ? 1 : -1;
            Log("stages: engine draw-event switch %s (scene-render reader %s, TOGGLEDRAWEVENTS writer %s, switch %s)",
                verified > 0 ? "verified" : "REFUSED - the stage profile is unavailable on this build",
                a ? "ok" : "MISMATCH", b ? "ok" : "MISMATCH", g ? "readable" : "UNREADABLE");
        }
        if (verified > 0) {
            if (!strcmp(args, "on") || !strcmp(args, "off")) {
                const bool on = !strcmp(args, "on");
                dvr::stageprof::set_enabled(on);
                *(volatile uint32_t*)kEmitDrawEvents = on ? 1u : 0u;
            } else if (!strcmp(args, "gpu on")) dvr::stageprof::set_gpu(true);
            else if (!strcmp(args, "gpu off")) dvr::stageprof::set_gpu(false);
            else if (!strncmp(args, "skip ", 5)) {   // `stages skip odd|all <stage name>` / `stages skip off`
                const char* a = args + 5;
                const int mode = !strncmp(a, "odd ", 4) ? 1 : !strncmp(a, "all ", 4) ? 2 : 0;
                wchar_t wname[48] = L"";
                if (mode) { const char* n = a + 4; int i = 0; for (; n[i] && i < 47; ++i) wname[i] = (wchar_t)(unsigned char)n[i]; wname[i] = 0; }
                dvr::stageprof::set_skip(mode, wname);
            }
            Log("stages: collector %s, GPU time %s, engine switch now %u | words: stages on|off, stages gpu on|off, stages skip odd|all <stage>|off | a `perf/stages:` table "
                "follows every 5 s while on", dvr::stageprof::enabled() ? "ON" : "off", dvr::stageprof::gpu() ? "on" : "off",
                *(volatile uint32_t*)kEmitDrawEvents);
        }
        return true;
    }
    if (!strcmp(cmd, "reshade")) {   // pre-release audit: `reshade effects on|off` for an A/B plan row (session only)
        const int want = !strcmp(args, "effects on") ? 1 : !strcmp(args, "effects off") ? 0 : -1;
        const int now = dvr::reshade_runtime::set_effects(want);
        Log("reshade: effects %s (the seam; session only, the preset is not saved) | words: reshade effects on|off",
            now < 0 ? "unavailable - no running effect runtime" : now ? "ON" : "OFF");
        return true;
    }
    if (!strcmp(cmd, "aniso")) {     // the texture-filter levers (core/gfx/sampler_force.h)
        const bool ok = dvr::samplers::command(args);
        char v[16];
        _snprintf(v, sizeof(v), "%d", dvr::samplers::anisotropy());
        ConfigWriteKey("Clarity", "Anisotropy", v, "the seam");
        ConfigWriteKey("Clarity", "TrilinearMips", dvr::samplers::trilinear() ? "1" : "0", "the seam");
        return ok;
    }
    if (!strcmp(cmd, "depthprobe")) return dvr::depthprobe::command(args);   // motion vectors, step 1
    if (!strcmp(cmd, "frameid")) {   // 41.1 (session 9): the frame-identity trace
        if (DvrOnOff(args, &b)) { dvr::frameid::set_enabled(b); return true; }
        { char sub[16] = "", v[16] = ""; if (sscanf(args, "%15s %15s", sub, v) == 2 && !strcmp(sub, "every")) { dvr::frameid::set_every((uint32_t)atoi(v)); return true; } }
        dvr::frameid::log_status();
        return true;
    }
    if (!strcmp(cmd, "cfg") && !strcmp(args, "dump")) {
        Log("cfg: gamepadOnly=%d%s hands=%d handMesh=%d blink=%d fov=%.0f fpsCap=%.1f posTrack=%d melee=%d",
            (int)g_gamepadOnly,
            g_gamepadOnly ? " (hands/blink/melee READ 0 BY DESIGN, not because they failed)" : "",
            (int)g_skcDrive, (int)g_handMesh, (int)g_blkAimOnCfg, g_fovLever,
            g_fpsCap, (int)g_posTrack, (int)g_meleeOn);
        return true;
    }
    return false;
}

// Runs on the script lane (called from PeHandler next to the other console
// users) so the engine's console sees a normal game-thread caller.
static void DvrConsoleApply()
{
    if (!g_dvrConsoleReq[0]) return;
    // RunConsole calls the engine's ProcessEvent, which is OUR hook, which
    // runs this function again: with the request still pending that recursed
    // until the stack overflowed (0xc00000fd on the game thread, run 05 of
    // 2026-09-03 - the first time a console word ever reached the engine on
    // 41.x). The re-entry flag stops the nested call, and the request is taken
    // off the seam BEFORE the engine runs it.
    if (g_peReentry) return;
    char req[256];
    strncpy(req, g_dvrConsoleReq, sizeof(req) - 1);
    req[sizeof(req) - 1] = 0;
    g_dvrConsoleReq[0] = 0;
    wchar_t w[256];
    MultiByteToWideChar(CP_UTF8, 0, req, -1, w, 256);
    char reply[512] = "";
    int n = RunConsole(w, reply, sizeof(reply));
    if (n < 0)
        Log("console: '%s' -> -1 (%s)", req,
            !g_fnConsoleCmd ? "no ConsoleCommand UFunction in the name tables"
                            : "no PlayerController latched yet - wait for GAMEPLAY");
    else
        Log("console: '%s' -> %d %s", req, n, n ? reply : "(empty reply)");
}

// 41.1: is the engine's view pipeline dispatching? ProcessViewRotation fires
// every tick while a player camera is being driven (gameplay, and the title
// screen's attract camera); a LOADING screen dispatches nothing (measured run
// 21: headwrites 0/3s on "press any key to continue"). 750 ms of silence
// with a live pawn is a loading screen, not gameplay.
static bool DvrScriptViewLive()
{
    // A loading screen dispatches a short burst about once a second (run 22:
    // GAMEPLAY/LOADING flapping), so leaving LOADING needs a full second of
    // continuous dispatches, and entering it 750 ms of silence.
    // 41.1 (session 8): a PAUSE MENU silences the dispatches too, and the
    // one-second rule then held every resume on the flat quad for 1-1.5 s
    // (32 pause/resumes in the 2026-09-03 headset run, each one a stereo ->
    // flat -> stereo flip the eyes felt). A silence that began while a menu
    // was open is the menu's, not a load's: the first fresh dispatch after
    // it is live at once. The one-second rule stays for every other silence.
    static double silentSince = 0.0, resumedAt = 0.0;
    static bool live = false, menuSilence = false;
    const double now = MaimNowMs();
    // VR-98 ([Menu] NoteFastMono): the note movie owns the view while it is open. Not live at once, and
    // its silence is a menu's, so the first fresh dispatch after it closes is live without the hold.
    // A UI observer that stopped polling (500 ms) cannot keep it: a stuck note flag must not park mono.
    static bool noteSaid = false;
    if (g_uiNoteFastMono && g_uiNoteOpen && now - g_uiPollMs < 500.0) {
        if (!noteSaid) {
            noteSaid = true;
            DVR_LOG(dvr::log::Cat::menu, dvr::log::Level::Info,
                    "[game] view held by the NOTE screen (NoteFastMono): mono now, not after 750 ms of silence; "
                    "the first dispatch after it closes is live at once (last head write %.0f ms ago)",
                    g_scriptHeadOK ? now - g_scriptHeadMs : -1.0);
        }
        if (!silentSince) silentSince = now;
        menuSilence = true; resumedAt = 0.0; live = false;
        return false;
    }
    noteSaid = false;
    // A PVR dispatch deliberately left to the cinematic is still activity.
    // Do not classify our suppressed write counter as a silent game camera.
    const bool fresh = (g_scriptHeadOK && (now - g_scriptHeadMs) < 750.0) || CineHeadDispatchFresh();
    if (!fresh) {
        if (!silentSince) { silentSince = now; menuSilence = g_menuOpen || g_inMenu; }
        else if (g_menuOpen) menuSilence = true;
        resumedAt = 0.0; live = false;
    } else {
        silentSince = 0.0;
        if (resumedAt == 0.0) {
            resumedAt = now;
            if (menuSilence) {
                live = true;
                DVR_LOG(dvr::log::Cat::menu, dvr::log::Level::Info,
                        "[game] view live at once after a MENU's silence, or a note's with NoteFastMono (no one-second hold: the dispatches "
                        "stopped for the pause menu, not a load)");
            }
            menuSilence = false;
        }
        if (now - resumedAt >= 1000.0) live = true;
    }
    return live;
}

// "[game] state: GAMEPLAY|MENU|CINEMATIC|LOADING|NO_PAWN" on every transition -
// the line tools\boot.ps1 waits for. Present thread, once per frame.
static void GameStateTick()
{
    // VR-62: sample the terms INDIVIDUALLY, so the scoreboard scores exactly the
    // values this function decides on and cannot disagree with it.
    UiSurfacePoll();
    dvr::perf::part_mark("gs.uiSurfacePoll");
    const bool suCyl    = CylTruthLive();
    dvr::perf::part_mark("gs.cylTruth");
    const bool suNoMenu = !g_menuOpen && !g_inMenu && !g_mainMenu && !UiSurfaceBlocks();
    const bool suView   = DvrScriptViewLive();
    // VR-62 observation. Sampled with the same values the state machine is
    // about to decide on, so its "proposed" verdict cannot disagree with the
    // real one for any reason except the one substitution it makes.
    dvr::perf::part_mark("gs.viewLive");
    UiPoll(suCyl, suView);
    dvr::perf::part_mark("gs.uiPoll");

    const char* s;
    if (!suCyl)                        s = "NO_PAWN";
    else if (!suNoMenu)                s = "MENU";
    else if (!suView) {
        // A loading screen ends whatever cutscene the latch remembers.
        if (g_cineNow) { g_cineNow = false; Log("cine: latch cleared - a loading screen"); }
        s = "LOADING";
    }
    else if (g_cineNow)                s = "CINEMATIC";
    else                               s = "GAMEPLAY";

    // VR-62. The clock starts when we LEAVE gameplay, because that is the last
    // moment we know the picture was right; everything after it is the settle.
    const bool nowGameplay = !strcmp(s, "GAMEPLAY");
    static bool wasGameplay = false;
    if (wasGameplay && !nowGameplay) SuBeginLoad();
    wasGameplay = nowGameplay;
    // A load destroys the components the weapon contracts were matched to, and
    // the candidate list holds the pointers those contracts were matched to, so
    // leaving gameplay drops both and queues the UI observer's rescan. VR-93: a
    // MENU over a live pawn is not a load. With [Hands] AttachKeepOnMenu=1 it
    // suspends them instead and the resume validates them before use; =0 is
    // the old transition exactly. hands/menu_keep.h.
    MkPresentTick(s, suCyl);
    dvr::perf::part_mark("gs.menuKeep");
    SuTick(suCyl, suNoMenu, suView, !g_cineNow, DvrGameplayVerdict());
    dvr::perf::part_mark("gs.startupScore");

    if (strcmp(s, g_dvrGameState) != 0) {
        strncpy(g_dvrGameState, s, sizeof(g_dvrGameState) - 1);
        DVR_LOG(dvr::log::Cat::menu, dvr::log::Level::Info, "[game] state: %s", s);
        LookupCostReport(s);   // VR-102: what the lookups have cost the game thread so far
        if (!strcmp(s, "LOADING")) dvr::perf::note(dvr::perf::kFlagLevelLoad);   // the gap line's flag
        // 41.1 (session 8): the census summary once, when the first level is
        // up (the population that matters: the level's textures and meshes).
        static bool censusSaid = false;
        if (!censusSaid && !strcmp(s, "GAMEPLAY")) { censusSaid = true; dvr::census::log_summary("first GAMEPLAY"); }
    }
}

static void DvrStatusProvider(dvr::status::Writer& w)
{
    w.kv("version", DVR_VERSION);
    w.kv("build", DVR_BUILD_ID);
    w.kv("config", DVR_BUILD_CONFIG);
    w.kv("optimised", (bool)DVR_BUILD_OPTIMISED);
    w.kv("legacy", (bool)DVR_WITH_LEGACY);   // VR-180: a legacy build is never one to play
    w.kv("backend", "openxr");
    w.kv("runtime", dvr::vr::runtime_name());
    w.kv("session", dvr::vr::session_state_name());
    w.kv("vrReady", (bool)g_vrReady);
    w.kv("xrOn", (bool)g_xrOn);
    w.kv("state", g_dvrGameState);
    w.kv("frame", (unsigned long)g_frame);
    w.kv("capW", (int)dvr::capture::width()); w.kv("capH", (int)dvr::capture::height());
    w.kv("capMode", dvr::capture::mode_name());
    w.kv("capShared", dvr::capture::probed() && dvr::capture::shared_available());
    w.obj("perf"); dvr::perf::status(w); w.end_obj();   // 41.1 (session 8): the tick budget
    w.obj("census"); dvr::census::status(w); w.end_obj();   // 41.1 (session 8): the creation census
    w.obj("device"); dvr::d3d9ex::status(w); w.end_obj();   // 41.1 (session 8): the 9Ex device and the translation
    { uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh); w.kv("eyeW", (int)ew); w.kv("eyeH", (int)eh); }
    w.obj("stereo"); dvr::stereo::status(w); w.end_obj();
    w.obj("frameid"); dvr::frameid::status(w); w.end_obj();   // 41.1 (session 9): the frame-identity trace
    w.obj("camera"); dvr::camera::status(w); w.end_obj();
    w.obj("draws"); dvr::hudclass::status(w); w.end_obj();   // VR-117: the HUD draw census
    w.obj("hud"); dvr::hudcap::status(w); w.end_obj();       // VR-117: the redirect and the layout
    { const dvr::vr::HudQuadStats hq = dvr::vr::hud_quad_stats(); w.obj("hudQuads"); w.kv("submitted", (unsigned long)hq.submitted);
      w.kv("untracked", (unsigned long)hq.untracked); w.kv("hiddenBudget", (unsigned long)hq.hiddenBudget); w.end_obj(); }

    w.obj("head");
    w.kv("yaw", (double)g_hmdYaw); w.kv("pitch", (double)g_hmdPitch); w.kv("roll", (double)g_hmdRoll);
    w.kv("tracked", (bool)g_devPoseOk[0]);
    w.kv("rotInject", (bool)g_rotInject); w.kv("posTrack", (bool)g_posTrack);
    w.kv("scriptHeadOK", (bool)g_scriptHeadOK);
    w.end_obj();
    w.obj("res");    // 41.1: the render-resolution picker
    w.kv("wantW", (int)g_resWantW); w.kv("wantH", (int)g_resWantH); w.kv("wantFull", (bool)g_resWantFull);
    w.kv("virtualMode", (bool)g_resVirtual);
    w.end_obj();
    w.obj("neck");   // 41.1: the pitch pivot lever
    w.kv("mode", NeckModeName(g_neckMode));
    w.kv("belowM", (double)g_neckBelowM); w.kv("behindM", (double)g_neckBehindM);
    w.kv("arcRightUu", (double)g_neckArcUu[0]); w.kv("arcUpUu", (double)g_neckArcUu[1]); w.kv("arcFwdUu", (double)g_neckArcUu[2]);
    w.end_obj();
    w.arr("hands");
    for (int h = 0; h < 2; h++) {
        int idx = g_ctrlIdx[h];
        w.item((idx >= 0 && idx < 16 && g_devPoseOk[idx]) ? 1.0 : 0.0);
    }
    w.end_arr();
    w.obj("hooks");
    w.kv("processEvent", (bool)g_peInstalled);
    w.kv("blinkDir", (bool)g_blkDirOn); w.kv("blinkDst", (bool)g_blkDstOn); w.kv("blinkTrc", (bool)g_blkTrcOn);
    w.kv("pad", (bool)g_padActive);
    w.kv("xrInput", dvr::vr::input_attached());
    w.end_obj();
    w.obj("features");
    w.kv("gamepadOnly", (bool)g_gamepadOnly);   // 40.3: names the OWNER of the zeroes below
    w.kv("hands", (bool)g_skcDrive); w.kv("handMesh", (bool)g_handMesh); w.kv("handModels", (bool)g_hmEnable);
    w.kv("tuckCamera", (bool)g_crawlTuckCamera); w.kv("tucked", (bool)g_skcTucked);   // VR-122
    // 41.2 (VR-31): the drawn hands' own liveness. handModels alone cannot say
    // whether anything reached the screen - handModelTris is what does.
    w.kv("handModelTris", (int)g_hmTris); w.kv("handModelWhy", HmWhy(g_hmLastWhy));
    w.kv("blink", (bool)g_blkAimOnCfg); w.kv("melee", (bool)g_meleeOn);
    w.kv("fovLever", (double)g_fovLever);
    w.kv("fpsCap", (double)g_fpsCap);
    dvr::desktop_eye::status(w);
    dvr::aim::status(w);
    dvr::anim::status(w);
    dvr::swing::status(w);
    dvr::drop::status(w);
    SwordTrailStatus(w);   // VR-171
    CamShakeStatus(w);   // VR-172
    dvr::snap::status(w);   // VR-219
    w.end_obj();
    w.kv("menuOpen", (bool)g_menuOpen); w.kv("inMenu", (bool)g_inMenu); w.kv("mainMenu", (bool)g_mainMenu);
    w.kv("cine", (bool)g_cineNow);
    w.kv("uiBlocks", UiSurfaceBlocks()); w.kv("uiRides", UiSurfaceRidesHud());   // VR-117
    w.kv("exiting", InterlockedCompareExchange(&g_gameExiting, 0, 0) != 0);
    w.obj("counters");
    w.kv("submits", (unsigned long)dvr::frame::submit_count()); w.kv("gameFrames", (unsigned long)g_gameFrames);
    w.kv("padPolls", (unsigned long)g_padPolls); w.kv("headHits", (unsigned long)g_pvrHits);
    w.kv("headWrites", (unsigned long)g_pvrWrites); w.kv("handWrites", (unsigned long)g_fpWrites);
    w.kv("commands", (unsigned long)dvr::command::sequence());
    w.end_obj();
    w.kv("log", dvr::log::path());
    w.kv("dataDir", dvr::paths::data_dir());
}

// Called once from Direct3DCreate9 after the config is loaded.
// The game side's context for a `mark` line: the state, the melee swing age
// (reads -1 by design under GamepadOnly: no motion melee, no swing stamps),
// the motion-aim window, the ground-truth test, the menu flags.
static int DvrPerfContext(char* buf, size_t cap)
{
    const double nowMs = MaimNowMs();
    return _snprintf(buf, cap, "game: state=%s menu=%d/%d swingAge=%.0f ms (-1 by design under GamepadOnly) aimWin=%d gt=%d cal=%d",
                     g_dvrGameState[0] ? g_dvrGameState : "?", (int)g_inMenu, (int)g_menuOpen,
                     g_meleeLastMs ? nowMs - g_meleeLastMs : -1.0, (int)(nowMs < g_maimArmedUntil),
                     (int)g_gtActive, g_fpCalPhase);
}

static void DvrDebugInit()
{
    dvr::command::set_game_handler(DvrGameCommand);
    dvr::status::set_provider(DvrStatusProvider);
    dvr::perf::set_context_provider(DvrPerfContext);
    DVR_LOG(dvr::log::Cat::cmd, dvr::log::Level::Info,
            "command seam: %s\\command.txt (1 Hz), status: %s", dvr::paths::data_dir(), dvr::status::path());
}
