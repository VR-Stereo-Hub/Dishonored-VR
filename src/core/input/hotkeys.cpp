// core/input/hotkeys.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Called once per game frame from the Present hook. The live-tuning hotkeys.
// No game memory is written (the retired camera-write path is gone).
static void StereoUpdate()
{
    {
        static bool f9Was = false;
        bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (f9 && !f9Was) {
            g_skcDrive = !g_skcDrive;
            Log("hands: drive %s (F9)", g_skcDrive ? "ON" : "OFF - stock arms back");
        }
        f9Was = f9;
    }
    {
        static bool f7pWas = false;
        bool f7p = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (f7p && !f7pWas) {
            // 33.6: F7 cycles OFF -> ROTATION DRIVE -> pin test -> OFF
            if (!g_skcRotDrive && !g_skcRotPin) {
                g_skcRotDrive = true;
                Log("skc/rot: DRIVE ON (F7) - weapons follow the CONTROLLER's "
                    "orientation. Hold hands neutral and press END to zero.");
            } else if (g_skcRotDrive) {
                g_skcRotDrive = false; g_skcRotPin = true;
                Log("skc/rot: pin test (F7 again for off)");
            } else {
                g_skcRotPin = false;
                Log("skc/rot: off (F7)");
            }
        }
        f7pWas = f7p;
    }

    // 31.5: F8 = the world-space pin test. On a KEY rather than in the panel,
    // because three separate attempts to add this checkbox silently failed to
    // match and never rendered - a hotkey cannot fail that way.
    {
        static bool f8Was = false;
        bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (f8 && !f8Was) {
            g_skcPinTest = !g_skcPinTest;
            g_skcPinHave = false;
            Log("skc/pin: %s (F8) - set Hands space=0, then walk and turn. "
                "Hands staying put in the air = space 0 really is world space.",
                g_skcPinTest ? "ON" : "off");
        }
        f8Was = f8;
    }

    // 30.38: F10 toggles the in-game settings overlay.
    {
        static bool f10Was = false;
        bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
        if (f10 && !f10Was) {
            g_ovlVisible = !g_ovlVisible;
            Log("overlay: %s (F10)", g_ovlVisible ? "OPEN" : "closed");
        }
        f10Was = f10;
    }

    // 41.1 (Dishonored): F2 stamps the freeze/fault marker. The F10 overlay
    // has a MARK button, but a headset tester cannot open a panel that covers
    // the view to report something they are looking AT - the marker has to be
    // one key, eyes-free. F2 is the only F-key free on this game: its sole
    // binding is Alt+F2 (viewmode unlit), and the mod uses F3-F10 already
    // (F5/F9 are the game's quicksave/quickload, F11/F12 BendTime).
    // Alternating text so a log reads as spells, not points: each press says
    // whether it opened or closed one, and how long the last one lasted.
    {
        static bool f2Was = false;
        static bool inSpell = false;
        static ULONGLONG spellStart = 0;
        const bool f2 = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (f2 && !f2Was) {
            const ULONGLONG now = GetTickCount64();
            char text[96];
            if (!inSpell) {
                inSpell = true; spellStart = now;
                _snprintf(text, sizeof(text), "F2 SPELL BEGIN (tester says the fault is visible NOW)");
            } else {
                inSpell = false;
                _snprintf(text, sizeof(text), "F2 SPELL END after %.1f s (the fault is gone NOW)",
                          (double)(now - spellStart) / 1000.0);
            }
            text[sizeof(text) - 1] = 0;
            dvr::perf::mark(text, "F2");
        }
        f2Was = f2;
    }

    // 30.37: live WORLD SCALE (game units per meter). One knob drives both
    // stereo separation and positional parallax, so all depth cues agree.
    // PageUp = world feels bigger (scale down), PageDown = world smaller.
    {
        static bool puWas = false, pdWas = false;
        bool pu = (GetAsyncKeyState(VK_PRIOR) & 0x8000) != 0;
        bool pd = (GetAsyncKeyState(VK_NEXT)  & 0x8000) != 0;
        if (pu && !puWas) {
            g_posScaleUU /= 1.05f; if (g_posScaleUU < 10.0f)  g_posScaleUU = 10.0f;
            Log("world: scale %.1f uu/m (PgUp - world bigger)", g_posScaleUU);
        }
        if (pd && !pdWas) {
            g_posScaleUU *= 1.05f; if (g_posScaleUU > 200.0f) g_posScaleUU = 200.0f;
            Log("world: scale %.1f uu/m (PgDn - world smaller)", g_posScaleUU);
        }
        puWas = pu; pdWas = pd;
    }

    // VR-31 route (d): step through the hideable sections at a human pace.
    // These only SET A REQUEST - every ShowMaterialSection dispatch happens on
    // the script lane in MatCycleTick, because ProcessEvent does not belong on
    // the present thread.
    if (g_matCycleCfg) {
        static bool n1Was = false, n2Was = false, n3Was = false;
        const bool n1 = (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0;
        const bool n2 = (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0;
        const bool n3 = (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0;
        if (n1 && !n1Was) g_matCycleReq = -1;
        if (n2 && !n2Was) g_matCycleReq =  2;
        if (n3 && !n3Was) g_matCycleReq =  1;
        n1Was = n1; n2Was = n2; n3Was = n3;
    }

    // VR-31 route (b): step through the censused DRAWS. Same split as the
    // material cycler - the hotkey only posts a request, and DcCycleTick acts
    // on it from the tick, never from here.
    if (g_dcOn) {
        static bool n4Was = false, n5Was = false, n6Was = false;
        const bool n4 = (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0;
        const bool n5 = (GetAsyncKeyState(VK_NUMPAD5) & 0x8000) != 0;
        const bool n6 = (GetAsyncKeyState(VK_NUMPAD6) & 0x8000) != 0;
        if (n4 && !n4Was) g_dcCycleReq = -1;
        if (n5 && !n5Was) g_dcCycleReq =  2;
        if (n6 && !n6Was) g_dcCycleReq =  1;
        n4Was = n4; n5Was = n5; n6Was = n6;
        // 7/8/9 cut the LOCKED mesh into eighths and hide one at a time, which
        // is how we find where the hands sit in the triangle order.
        static bool n7Was = false, n8Was = false, n9Was = false;
        const bool n7 = (GetAsyncKeyState(VK_NUMPAD7) & 0x8000) != 0;
        const bool n8 = (GetAsyncKeyState(VK_NUMPAD8) & 0x8000) != 0;
        const bool n9 = (GetAsyncKeyState(VK_NUMPAD9) & 0x8000) != 0;
        if (n7 && !n7Was) g_dcSliceReq = -1;
        if (n8 && !n8Was) g_dcSliceReq =  2;
        if (n9 && !n9Was) g_dcSliceReq =  1;
        n7Was = n7; n8Was = n8; n9Was = n9;
    }

    // VR-31 step 2: the split derived from BONE INFLUENCE (mesh_split.cpp).
    // Separate keys from the slice walk on purpose - the slice mask is still
    // the fallback if a driver refuses to let the buffers be read, and losing
    // the muscle memory for it would cost a headset session.
    //
    //   Numpad 0  next mode: hands -> all -> arms -> other -> off -> hands
    //   Numpad +  keep MORE as hand (the cut moves up the arm)
    //   Numpad -  keep LESS as hand (the cut moves toward the fingers)
    //   Numpad *  which arm + / - moves: both -> side A -> side B
    //   Numpad .  step size for + / -: coarse -> fine -> ultrafine
    //   Numpad /  re-derive the whole split from the buffers
    if (g_msOn) {
        static bool n0Was = false, adWas = false, sbWas = false,
                    mlWas = false, dvWas = false, dcWas = false;
        const bool n0 = (GetAsyncKeyState(VK_NUMPAD0)  & 0x8000) != 0;
        const bool ad = (GetAsyncKeyState(VK_ADD)      & 0x8000) != 0;
        const bool sb = (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0;
        const bool ml = (GetAsyncKeyState(VK_MULTIPLY) & 0x8000) != 0;
        const bool dv = (GetAsyncKeyState(VK_DIVIDE)   & 0x8000) != 0;
        const bool dc = (GetAsyncKeyState(VK_DECIMAL)  & 0x8000) != 0;
        if (n0 && !n0Was) g_msModeReq   =  1;
        if (ad && !adWas) g_msWristReq  =  1;
        if (sb && !sbWas) g_msWristReq  = -1;
        if (ml && !mlWas) g_msSideReq   =  1;
        if (dv && !dvWas) g_msRebuildReq = 1;
        if (dc && !dcWas) g_msStepReq    = 1;

        // AUTO-REPEAT on + / - only. At the ultrafine step the ring moves a
        // tenth of a percent of the arm per press, so placing it by hand would
        // be a hundred taps in a headset. Held down, it repeats after a pause,
        // like any key that has to travel a distance.
        {
            static double heldSince = 0.0, nextRep = 0.0;
            const double now = MaimNowMs();
            const int dir = ad ? 1 : (sb ? -1 : 0);
            if (!dir) { heldSince = 0.0; }
            else {
                if (heldSince == 0.0) { heldSince = now; nextRep = now + 400.0; }
                else if (now >= nextRep) { g_msWristReq = dir; nextRep = now + 60.0; }
            }
        }
        n0Was = n0; adWas = ad; sbWas = sb; mlWas = ml; dvWas = dv; dcWas = dc;
    }

    (void)g_camRefindIn; (void)g_camNameIdx; (void)g_camObj;
    (void)kCamRight; (void)kCamLoc0; (void)kCamLoc1; (void)kCamLoc2;
    (void)&FindLiveCamera; (void)&CamStillValid;
}
