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
//   capture mode <m>|status      the capture path (sync|deferred|shared): live switch, fails soft
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
        dvr::stereo::select(args);   // logs the refusal itself
        return true;
    }
    if (!strcmp(cmd, "capture")) {
        char sub[16] = "", m[16] = "";
        if (sscanf(args, "%15s %15s", sub, m) == 2 && !strcmp(sub, "mode")) {
            dvr::capture::set_mode(m);   // logs the refusal itself
            return true;
        }
        const dvr::capture::Cost c = dvr::capture::cost();
        Log("capture: mode=%s probe=%s cost/present rtd=%u lock=%u copy=%u upload=%u blit=%u total=%u us "
            "(%u grabs) delivered serial %lu of %lu tag=%d fenceLate=%u (capture mode sync|deferred|shared)",
            dvr::capture::mode_name(),
            !dvr::capture::probed() ? "not yet" : dvr::capture::shared_available() ? "shared AVAILABLE" : "shared REFUSED",
            c.rtdUs, c.lockUs, c.copyUs, c.uploadUs, c.blitUs, c.totalUs, c.grabsInWindow,
            (unsigned long)dvr::capture::delivered_serial(), (unsigned long)dvr::capture::serial(),
            dvr::capture::delivered_tag(), dvr::capture::fence_late());
        return true;
    }
    if (!strcmp(cmd, "reentry")) {
        if (SceneDrawCommand(args)) return true;
        if (SceneProbeCommand(args)) return true;
        Log("reentry: pulse [n] | reset | hook on|off | status | census on|off|report | stack event <name>|caller <hex>|present|off | probe <hex> [len] | findstart <hex>");
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
    wchar_t w[256];
    MultiByteToWideChar(CP_UTF8, 0, g_dvrConsoleReq, -1, w, 256);
    char reply[512] = "";
    int n = RunConsole(w, reply, sizeof(reply));
    Log("console: '%s' -> %d %s", g_dvrConsoleReq, n, reply);
    g_dvrConsoleReq[0] = 0;
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
    static double silentSince = 0.0, resumedAt = 0.0;
    static bool live = false;
    const double now = MaimNowMs();
    const bool fresh = g_scriptHeadOK && (now - g_scriptHeadMs) < 750.0;
    if (!fresh) { silentSince = silentSince ? silentSince : now; resumedAt = 0.0; live = false; }
    else {
        silentSince = 0.0;
        if (resumedAt == 0.0) resumedAt = now;
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
    { uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh); w.kv("eyeW", (int)ew); w.kv("eyeH", (int)eh); }
    w.obj("stereo"); dvr::stereo::status(w); w.end_obj();
    w.obj("camera"); dvr::camera::status(w); w.end_obj();
    w.obj("head");
    w.kv("yaw", (double)g_hmdYaw); w.kv("pitch", (double)g_hmdPitch); w.kv("roll", (double)g_hmdRoll);
    w.kv("tracked", (bool)g_devPoseOk[0]);
    w.kv("rotInject", (bool)g_rotInject); w.kv("posTrack", (bool)g_posTrack);
    w.kv("scriptHeadOK", (bool)g_scriptHeadOK);
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
static void DvrDebugInit()
{
    dvr::command::set_game_handler(DvrGameCommand);
    dvr::status::set_provider(DvrStatusProvider);
    DVR_LOG(dvr::log::Cat::cmd, dvr::log::Level::Info,
            "command seam: %s\\command.txt (1 Hz), status: %s", dvr::paths::data_dir(), dvr::status::path());
}
