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
//   stereo <name>|status         the stereo method (mono|aer|reentry): live switch, fails soft
//   vrpace <args>                the runtime layer's pacing seam (on|off|thread|detach|feed|sync|spike|simidle|status)
//   vrmirror on|off|status       the desktop mirror pin (counted only on D3D9)
//   vreyetag rendered|located    what the projection layer claims each eye was
//                                rendered from (BRVR calls this a major flicker fix)
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
    if (!strcmp(cmd, "camera")) {
        char sub[32] = "", fld[16] = "all";
        float uu = 0.0f;
        if (sscanf(args, "%31s", sub) == 1 && !strcmp(sub, "eyetest")) {
            if (strstr(args, "stop")) { dvr::camera::eyetest_stop("seam"); return true; }
            sscanf(args, "%*s %f %15s", &uu, fld);
            dvr::camera::eyetest_start(uu > 0.0f ? uu : 100.0f, fld);
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
        dvr::stereo::select(args);   // logs the refusal itself
        return true;
    }
    if (!strcmp(cmd, "vrpace"))   { dvr::vr::handle_pace_command(args); return true; }
    if (!strcmp(cmd, "vrmirror")) { dvr::vr::handle_mirror_command(args); return true; }
    // The cinematic quad drops the PROJECTION layer for the flat screen when the
    // gameplay verdict reads false. `vrcine off` is the live A/B that separates
    // "the per-eye method is wrong" from "the verdict is wrong".
    if (!strcmp(cmd, "vrcine")) { dvr::vr::handle_cine_command(args); return true; }
    // `vreyetag rendered|located|status` - the live A/B for what the projection
    // layer CLAIMS each eye was rendered from. `located` (the default, and the
    // behaviour every run so far) stamps the runtime's own per-eye poses;
    // `rendered` rebuilds them as the PARALLEL pair the game actually drew
    // (located midpoint, shared orientation, +/- ipd/2 along its right axis) -
    // the same shape as the camera seam's write.
    //
    // Why it matters here: BioShock Remastered VR records this as "a major
    // flicker fix" and its render invariants say plainly "the projection layer
    // carries the latched pose the image was rendered from, not the freshest
    // pose at submit time" (docs/reference/bioshock-remastered-vr/docs/
    // INVARIANTS.md and modules/render.md). A claim that misdescribes the
    // render makes the compositor reproject each eye by a different wrong
    // delta, which is per-eye shimmer that comes and goes with head speed.
    if (!strcmp(cmd, "vreyetag")) {
        if (strstr(args, "rendered"))     dvr::vr::set_eye_tag_rendered(true);
        else if (strstr(args, "located")) dvr::vr::set_eye_tag_rendered(false);
        else Log("xr: eye tags = %s (vreyetag rendered|located|status). rendered = the "
                 "parallel pair the game drew; located = the runtime's raw per-eye poses",
                 dvr::vr::eye_tag_rendered() ? "RENDERED-POSE" : "located");
        if (dvr::vr::eye_tag_rendered()) dvr::vr::set_eye_tag_ipd_mm(dvr::camera::ipd_m() * 1000.0f);
        return true;
    }
    // Find the FOREGROUND (viewmodel) fov field. UE3 draws the first-person
    // weapon and hands with their own frustum, and a projection layer can only
    // claim ONE fov - so anything drawn with the other one lands at the wrong
    // apparent depth and carries the wrong per-eye disparity.
    if (!strcmp(cmd, "fovprobe")) { FovPropHunt(); return true; }
    if (!strcmp(cmd, "vrinput")) {
        if (DvrOnOff(args, &b)) { g_padEnabled = b; dvr::pad::set_enabled(b);
            Log("input: virtual pad %s (seam)", b ? "ON" : "off"); return true; }
        Log("input: pad %s active=%d polls=%ld actions=%s haptics=%d (vrinput on|off|status)",
            g_padEnabled ? "enabled" : "disabled", (int)dvr::pad::active(), dvr::pad::polls(),
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

// "[game] state: GAMEPLAY|MENU|CINEMATIC|NO_PAWN" on every transition - the
// line tools\boot.ps1 waits for. Present thread, once per frame.
static void GameStateTick()
{
    const char* s;
    if (!CylTruthLive())               s = "NO_PAWN";
    else if (g_menuOpen || g_inMenu)   s = "MENU";
    else if (g_cineNow)                s = "CINEMATIC";
    else                               s = "GAMEPLAY";
    // 41.1: the runtime's cinematic fallback drops the PROJECTION layer to the
    // quad screen unless a game adapter keeps publishing a gameplay verdict -
    // and an adapter that never publishes reads STALE, which is indistinguish-
    // able from "a cutscene is running". Dishonored never published, so every
    // per-eye method rendered its eyes correctly and then had them thrown away
    // for the mono quad: the alternating camera showed up as the picture
    // flickering side to side instead of as stereo. This is the publish, on
    // the present thread, every present - staleness is measured in wall time,
    // so it has to be unconditional and not only on a transition.
    dvr::vr::publish_gameplay_view(s[0] == 'G');
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
    w.kv("pad", (bool)dvr::pad::hooked());        // the IAT patch is in
    w.kv("padActive", (bool)dvr::pad::active());  // ...and controllers are feeding it
    w.kv("xrInput", dvr::vr::input_attached());
    w.end_obj();
    w.obj("features");
    w.kv("gamepadOnly", (bool)g_gamepadOnly);   // 40.3: names the OWNER of the zeroes below
    w.kv("hands", (bool)g_skcDrive); w.kv("handMesh", (bool)g_handMesh); w.kv("handModels", (bool)g_hmEnable);
    w.kv("blink", (bool)g_blkAimOnCfg); w.kv("melee", (bool)g_meleeOn);
    w.kv("fovLever", (double)g_fovLever);
    w.kv("fpsCap", (double)g_fpsCap);
    w.end_obj();
    w.kv("menuOpen", (bool)g_menuOpen); w.kv("inMenu", (bool)g_inMenu);
    w.kv("cine", (bool)g_cineNow);
    w.kv("exiting", InterlockedCompareExchange(&g_gameExiting, 0, 0) != 0);
    w.obj("counters");
    w.kv("submits", (unsigned long)dvr::frame::submit_count()); w.kv("gameFrames", (unsigned long)g_gameFrames);
    w.kv("padPolls", (unsigned long)dvr::pad::polls()); w.kv("headHits", (unsigned long)g_pvrHits);
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
