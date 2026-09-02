// core/input/hotkeys.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Called once per game frame from VRFrame. Handles the live-tuning hotkeys and
// reports the current eye. The actual stereo shear happens in hkSetVSConstF
// (render time); this only sets which eye the current frame's draws target.
// No game memory is written (the retired camera-write path is gone).
static void StereoUpdate()
{
    bool f1  = (GetAsyncKeyState(VK_F1)  & 0x8000) != 0;
    bool f2  = (GetAsyncKeyState(VK_F2)  & 0x8000) != 0;
    bool f7  = (GetAsyncKeyState(VK_F7)  & 0x8000) != 0;
    // (30.8 key diet: F10/F11/F12 separation & convergence retired - those
    // are set-once values that live in [Stereo] in the ini now)

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

    if (f7 && !g_f7WasDown) {
        g_stereoEnabled = !g_stereoEnabled;
        Log("stereo: %s (F7)", g_stereoEnabled ? "ENABLED" : "disabled");
    }
    if (f1 && !g_f1Was) {   // shrink image (less fill, crisper 1:1, more border)
        g_fillScale -= 0.05f; if (g_fillScale < 0.6f) g_fillScale = 0.6f;
        g_quadAspect = 0.0f;  // force quad rebuild
        Log("screen: fill = %.2f (F1)", g_fillScale);
    }
    if (f2 && !g_f2Was) {   // grow image (fills the border; mild zoom)
        g_fillScale += 0.05f; if (g_fillScale > 3.2f) g_fillScale = 3.2f;
        g_quadAspect = 0.0f;  // force quad rebuild
        Log("screen: fill = %.2f (F2)", g_fillScale);
    }
    g_f7WasDown = f7;
    g_f1Was = f1; g_f2Was = f2;

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

    g_curEye = g_drawEye; // this frame's rendered image belongs to g_drawEye

    (void)g_sepUU; (void)g_writtenEye; (void)g_prevWriteLoc; (void)g_havePrevWrite;
    (void)g_stereoDiagDone; (void)g_camRefindIn; (void)g_camNameIdx; (void)g_camObj;
    (void)kCamRight; (void)kCamLoc0; (void)kCamLoc1; (void)kCamLoc2;
    (void)&FindLiveCamera; (void)&CamStillValid;
}
