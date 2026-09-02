// game/dishonored/commands.cpp - the game side of the command seam and the
// status provider. Included by the unity build (it reads the mod's globals).
//
// Vocabulary (game words are tried before the core ones in command.h):
//   recenter                     same as F5
//   hands on|off                 the SkelControl hand drive
//   blink on|off|probe           hand-aimed Blink; probe = one-shot survey
//   fov <deg|0>                  the FOV lever (0 disarms)
//   pace delay <0-3> | stamp live|render | fix <0-2> | cap <hz>
//   layer proj|cyl|quad          OpenXR presentation layer mode
//   hud on|off                   wrist HUD
//   mirror 0|1|2                 desktop spectator view
//   overlay on|off               the F10 settings panel
//   console <text>               run a game console command on the script lane
//   dump frame|capture|eyes|hud
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
        Log("fov: lever -> %.0f (seam)", f);
        return true;
    }
    if (!strcmp(cmd, "pace")) {
        char a[16] = "", v[16] = "";
        sscanf(args, "%15s %15s", a, v);
        if (!strcmp(a, "delay")) { g_xrPoseDelay = atoi(v); if (g_xrPoseDelay < 0) g_xrPoseDelay = 0; if (g_xrPoseDelay > 3) g_xrPoseDelay = 3; }
        else if (!strcmp(a, "stamp")) { g_stampLive = !strcmp(v, "live"); }
        else if (!strcmp(a, "fix")) { g_stampFix = atoi(v); if (g_stampFix < 0 || g_stampFix > 2) g_stampFix = 0; }
        else if (!strcmp(a, "cap")) { g_fpsCap = (float)atof(v); if (g_fpsCap < 0) g_fpsCap = 0; if (g_fpsCap > 0 && g_fpsCap < 20) g_fpsCap = 20; }
        else if (strcmp(a, "status")) { Log("pace: usage - pace delay <0-3> | stamp live|render | fix <0-2> | cap <hz> | status"); return true; }
        Log("pace: delay=%d stampLive=%d stampFix=%d cap=%.1f layer=%d xrOn=%d", g_xrPoseDelay, (int)g_stampLive,
            g_stampFix, g_fpsCap, g_xrLayerMode, (int)g_xrOn);
        return true;
    }
    if (!strcmp(cmd, "layer")) {
        g_xrLayerMode = (args[0] == 'c') ? 1 : (args[0] == 'q') ? 2 : 0;
        g_quadAspect = 0.0f;   // rebuild the eye quads for the new mode
        Log("layer: %s (seam)", g_xrLayerMode == 0 ? "projection" : g_xrLayerMode == 1 ? "cylinder" : "quad");
        return true;
    }
    if (!strcmp(cmd, "hud") && DvrOnOff(args, &b)) { g_wristHud = b; Log("hud: wrist %s (seam)", b ? "ON" : "off"); return true; }
    if (!strcmp(cmd, "mirror")) { g_mirrorMode = atoi(args); Log("mirror: mode %d (seam)", g_mirrorMode); return true; }
    if (!strcmp(cmd, "overlay") && DvrOnOff(args, &b)) { g_ovlVisible = b; return true; }
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
        Log("cfg: gamepadOnly=%d%s hands=%d handMesh=%d blink=%d fov=%.0f wristHud=%d mirror=%d layer=%d poseDelay=%d stampLive=%d stampFix=%d fpsCap=%.1f posTrack=%d melee=%d",
            (int)g_gamepadOnly,
            g_gamepadOnly ? " (hands/blink/melee READ 0 BY DESIGN, not because they failed)" : "",
            (int)g_skcDrive, (int)g_handMesh, (int)g_blkAimOnCfg, g_fovLever, (int)g_wristHud, g_mirrorMode,
            g_xrLayerMode, g_xrPoseDelay, (int)g_stampLive, g_stampFix, g_fpsCap, (int)g_posTrack, (int)g_meleeOn);
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

// "[game] state: GAMEPLAY|MENU|CINEMATIC|NO_PAWN" on every transition - the
// line tools\boot.ps1 waits for. Present thread, once per frame.
static void GameStateTick()
{
    const char* s;
    if (!CylTruthLive())               s = "NO_PAWN";
    else if (g_menuOpen || g_inMenu)   s = "MENU";
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
    w.kv("backend", g_xrBackend ? "openxr" : "openvr");
    w.kv("runtime", g_xrBackend ? g_xriRuntimeName : "SteamVR");
    w.kv("vrReady", (bool)g_vrReady);
    w.kv("xrOn", (bool)g_xrOn);
    w.kv("mode", g_mode == MODE_SCENE ? "scene" : g_mode == MODE_THEATER ? "theater" : "none");
    w.kv("state", g_dvrGameState);
    w.kv("frame", (unsigned long)g_frame);
    w.kv("capW", (int)g_capW); w.kv("capH", (int)g_capH);
    w.kv("eyeW", (int)g_eyeW); w.kv("eyeH", (int)g_eyeH);
    w.kv("liveFovX", (double)g_liveFovX);
    w.kv("stereo", (bool)g_stereoEnabled);
    w.kv("mono", (bool)g_sbsMonoNow);
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
    w.kv("xrInput", (bool)g_xrInpAttached);
    w.end_obj();
    w.obj("features");
    w.kv("gamepadOnly", (bool)g_gamepadOnly);   // 40.3: names the OWNER of the zeroes below
    w.kv("hands", (bool)g_skcDrive); w.kv("handMesh", (bool)g_handMesh); w.kv("handModels", (bool)g_hmEnable);
    w.kv("blink", (bool)g_blkAimOnCfg); w.kv("melee", (bool)g_meleeOn);
    w.kv("fovLever", (double)g_fovLever); w.kv("wristHud", (bool)g_wristHud);
    w.kv("layer", g_xrLayerMode); w.kv("poseDelay", g_xrPoseDelay);
    w.kv("stampLive", (bool)g_stampLive); w.kv("stampFix", g_stampFix); w.kv("fpsCap", (double)g_fpsCap);
    w.kv("mirror", g_mirrorMode);
    w.end_obj();
    w.kv("menuOpen", (bool)g_menuOpen); w.kv("inMenu", (bool)g_inMenu);
    w.kv("cine", (bool)g_cineNow);
    w.kv("exiting", InterlockedCompareExchange(&g_gameExiting, 0, 0) != 0);
    w.obj("counters");
    w.kv("submits", (unsigned long)g_submits); w.kv("gameFrames", (unsigned long)g_gameFrames);
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
