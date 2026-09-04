// game/dishonored/commands.cpp - the game side of the command seam and the
// status provider. Included by the unity build (it reads the mod's globals).
//
// Vocabulary (game words are tried before the core ones in command.h):
//   recenter                     same as F5
//   hands on|off                 the SkelControl hand drive
//   blink on|off|probe           hand-aimed Blink; probe = one-shot survey
//   fov <deg|0>                  the FOV lever (0 disarms)
//   overlay on|off               the F10 settings panel
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
//   device ex on|off             [Device] Ex for the NEXT launch (the 9Ex device, core/gfx/d3d9ex)
//   device managed <m>           [Device] Managed=none|default|dynamic|shadow for the NEXT launch
//   vrpace <args>                the runtime layer's pacing seam (on|off|thread|detach|feed|sync|spike|simidle|status)
//   vrmirror on|off|status       the desktop mirror pin (counted only on D3D9)
//   vrinput on|off|status        the virtual gamepad
//   console <text>               run a game console command on the script lane
//   dump frame|capture|eyes
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
    if (!strcmp(cmd, "recenter")) { RecenterHead(); return true; }
    if (!strcmp(cmd, "hands") && DvrOnOff(args, &b)) {
        g_skcDrive = b; g_handMesh = b; g_autoHandDone = true;
        Log("hands: %s (seam)", b ? "ON" : "off");
        return true;
    }
    if (!strcmp(cmd, "blink")) {
        if (!strcmp(args, "probe")) { BlinkProbeArm(); return true; }
        if (DvrOnOff(args, &b)) { g_blkAimOnCfg = b; g_blkDriveUI = b; Log("blink: hand aim %s (seam)", b ? "ON" : "off"); return true; }
        return false;
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
    if (!strcmp(cmd, "res")) return ResCommand(args);   // 41.1: the render-resolution picker
    if (!strcmp(cmd, "neck")) {
        // 41.1: `neck off|add|cancel [below] [behind]` - the pitch pivot lever (head_track.cpp NeckSet)
        char mode[16] = "";
        float below = g_neckBelowM, behind = g_neckBehindM;
        const int n = sscanf(args, "%15s %f %f", mode, &below, &behind);
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
            if (!strcmp(dvr::stereo::active_name(), "reentry")) {
                Log("camera/eyetest: refused while the reentry method is active (two presents per tick with "
                    "different eyes would destroy the verdict) - `stereo mono` first");
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
        if (!strcmp(sub, "swap")) {   // 41.1: the blunt unconditional L/R A/B
            if (DvrOnOff(v, &b)) dvr::stereo::set_eye_swap(b);
            else Log("stereo: swap on|off (now %s) - inverts the eye sign unconditionally",
                     dvr::stereo::eye_swap() ? "ON" : "off");
            return true;
        }
        if (!strcmp(sub, "parity")) {   // 41.1: the measured pair geometry owns the eye sign
            if (!strcmp(v, "invert")) { dvr::stereo::set_parity_polarity(-dvr::stereo::parity_polarity()); return true; }
            if (DvrOnOff(v, &b)) dvr::stereo::set_parity_guard(b);
            else Log("stereo: parity on|off (now %s) - inverts the eye sign while the pair geometry "
                     "reports the eyes reversed", dvr::stereo::parity_guard() ? "ON" : "off");
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
        const dvr::capture::Cost c = dvr::capture::cost();
        Log("capture: mode=%s probe=%s cost/present rtd=%u lock=%u copy=%u upload=%u blit=%u total=%u us "
            "(%u grabs) delivered serial %lu of %lu tag=%d sharedWait=%d fenceWaits=%u timeouts=%u readWaits=%u "
            "readTimeouts=%u (capture mode sync|deferred|shared|off, capture sharedwait on|off)",
            dvr::capture::mode_name(),
            !dvr::capture::probed() ? "not yet" : dvr::capture::shared_available() ? "shared AVAILABLE" : "shared REFUSED",
            c.rtdUs, c.lockUs, c.copyUs, c.uploadUs, c.blitUs, c.totalUs, c.grabsInWindow,
            (unsigned long)dvr::capture::delivered_serial(), (unsigned long)dvr::capture::serial(),
            dvr::capture::delivered_tag(), dvr::capture::shared_wait() ? 1 : 0, dvr::capture::fence_waits(),
            dvr::capture::fence_timeouts(), dvr::capture::read_waits(), dvr::capture::read_timeouts());
        return true;
    }
    if (!strcmp(cmd, "device")) {   // 41.1 (session 8): the creation census and the 9Ex levers
        if (!args[0] || !strcmp(args, "status")) { dvr::census::log_status(); dvr::d3d9ex::log_status(); return true; }
        if (!strcmp(args, "census")) { dvr::census::log_summary("device census"); return true; }
        char sub[16] = "", v[16] = "";
        if (sscanf(args, "%15s %15s", sub, v) == 2) {
            if (!strcmp(sub, "ex") && DvrOnOff(v, &b)) { DeviceSetEx(b, "seam"); return true; }
            if (!strcmp(sub, "managed")) { DeviceSetManaged(v, "seam"); return true; }
        }
        Log("device: usage - device census|status | device ex on|off | device managed none|default|dynamic|shadow");
        return true;
    }
    if (!strcmp(cmd, "reentry")) {
        if (SceneDrawCommand(args)) return true;
        if (SceneProbeCommand(args)) return true;
        Log("reentry: pulse [n] | skip2 [n] | reset | hook on|off | status | census on|off|report | stack event <name>|caller <hex>|present|off | probe <hex> [len] | findstart <hex>");
        return true;
    }
    if (!strcmp(cmd, "vrpace"))   { dvr::vr::handle_pace_command(args); return true; }
    if (!strcmp(cmd, "vrmirror")) { dvr::vr::handle_mirror_command(args); return true; }
    if (!strcmp(cmd, "vrinput")) {
        if (DvrOnOff(args, &b)) { g_padEnabled = b; Log("input: virtual pad %s (seam)", b ? "ON" : "off"); return true; }
        Log("input: pad %s active=%d polls=%ld actions=%s haptics=%d (vrinput on|off|status)",
            g_padEnabled ? "enabled" : "disabled", (int)g_padActive, (long)g_padPolls,
            dvr::vr::input_attached() ? "attached" : "not attached", (int)(g_padHaptics && g_xrHaptics));
        return true;
    }
    if (!strcmp(cmd, "console")) {
        strncpy(g_dvrConsoleReq, args, sizeof(g_dvrConsoleReq) - 1);
        g_dvrConsoleReq[sizeof(g_dvrConsoleReq) - 1] = 0;
        Log("console: queued '%s' for the script lane", g_dvrConsoleReq);
        return true;
    }
    if (!strcmp(cmd, "dump")) {
        FrameDumpRequest(args[0] ? args : "frame");
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
    const bool fresh = g_scriptHeadOK && (now - g_scriptHeadMs) < 750.0;
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
                        "[game] view live at once after a MENU's silence (no one-second hold: the dispatches "
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
    const char* s;
    if (!CylTruthLive())               s = "NO_PAWN";
    else if (g_menuOpen || g_inMenu || g_mainMenu) s = "MENU";
    else if (!DvrScriptViewLive()) {
        // A loading screen ends whatever cutscene the latch remembers.
        if (g_cineNow) { g_cineNow = false; Log("cine: latch cleared - a loading screen"); }
        s = "LOADING";
    }
    else if (g_cineNow)                s = "CINEMATIC";
    else                               s = "GAMEPLAY";
    if (strcmp(s, g_dvrGameState) != 0) {
        strncpy(g_dvrGameState, s, sizeof(g_dvrGameState) - 1);
        DVR_LOG(dvr::log::Cat::menu, dvr::log::Level::Info, "[game] state: %s", s);
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
    w.obj("camera"); dvr::camera::status(w); w.end_obj();
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
    w.kv("blink", (bool)g_blkAimOnCfg); w.kv("melee", (bool)g_meleeOn);
    w.kv("fovLever", (double)g_fovLever);
    w.kv("fpsCap", (double)g_fpsCap);
    w.end_obj();
    w.kv("menuOpen", (bool)g_menuOpen); w.kv("inMenu", (bool)g_inMenu); w.kv("mainMenu", (bool)g_mainMenu);
    w.kv("cine", (bool)g_cineNow);
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
