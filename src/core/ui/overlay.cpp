// core/ui/overlay.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static void OverlayFrame()
{
    if (!g_ovlVisible) return;
    if (!g_ovlInit) {
        if (!g_dev11 || !g_ctx11 || !g_gameWnd) { g_ovlVisible = false; return; }
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui::GetStyle().ScaleAllSizes(1.6f);   // readable at headset distance
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;                   // no imgui.ini clutter
        io.FontGlobalScale = 1.6f;
        ImGui_ImplWin32_Init(g_gameWnd);
        ImGui_ImplDX11_Init(g_dev11, g_ctx11);
        InstallWindowSubclass("overlay init");   // 38.92: no-op if already on
        g_ovlInit = true;
        Log("overlay: initialized (F10 toggles)");
    }
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = true;                   // visible cursor in-headset
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    // 31.0: the controller drives the cursor when it is pointing at the panel.
    // Applied AFTER the Win32 backend so it wins over the desktop mouse, and
    // only while valid, so the mouse still works if you take the headset off.
    if (g_ovlPtrValid) {
        float cx = io.DisplaySize.x * 0.5f, cy = io.DisplaySize.y * 0.5f;
        float px = cx + g_ovlRayX * g_ovlPtrGain * cx;
        float py = cy - g_ovlRayY * g_ovlPtrGain * cy;
        if (px < -1.0e5f) px = -1.0e5f; if (px > 1.0e5f) px = 1.0e5f;
        if (py < -1.0e5f) py = -1.0e5f; if (py > 1.0e5f) py = 1.0e5f;
        io.MousePos = ImVec2(px, py);
        io.MouseDown[0] = g_ovlPtrDown;
        io.MouseDrawCursor = true;
    }
    ImGui::NewFrame();

    // 36.2: after a real-window resize WE caused, snap the panel back to the
    // center of the NEW logical space - its latched position may be outside
    // both the view and the mouse's reach.
    if (InterlockedExchange(&g_ovlRecenter, 0))
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.52f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    else
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.52f),
                                ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Dishonored VR", &g_ovlVisible,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);

    // The two things reached for most often, first and big.
    if (ImGui::Button("RECENTER  (F5)", ImVec2(-1, 0))) {
        g_posHaveRef = false;
        g_crouchRefOk = false;
        g_rtdReq = 3;
        Log("overlay: recentred (position reference)");
    }
    ImGui::SliderFloat("height offset (m)", &g_heightOffsetM, -1.0f, 1.0f, "%+.2f");
    // 38.60 SHIP CLEANUP: every diagnostic lives behind the "developer
    // tools" checkbox (bottom of the panel, [Overlay] DevTools). A player
    // sees settings; a debugger flips one box and gets the instruments.
    // 32.68: SAVE directly under RECENTER, and everything below it tabbed.
    // Re-applied onto the stable 32.52 base after the resolution work was
    // rolled back - the tabs were a good change that got reverted along with
    // things that were not.
    if (ImGui::Button("SAVE AS DEFAULTS", ImVec2(-1, 0)))
        OverlaySaveDefaults();
    ImGui::Separator();

    ImGui::Text("IPD %.0f mm", g_ipdM * 1000.0f);

    if (!ImGui::BeginTabBar("vrtabs")) { ImGui::End(); return; }

    if (ImGui::BeginTabItem("View")) {
    if (ImGui::SliderFloat("world scale (uu/m)", &g_posScaleUU, 10.0f, 200.0f, "%.0f"))
        { /* sep recomputed each frame from this */ }
    ImGui::TextDisabled("smaller = world feels bigger; life-size door test");

    if (ImGui::SliderFloat("screen distance (m)", &g_screenDist, 0.8f, 4.0f, "%.2f"))
        dvr::vr::set_screen(g_screenDist, g_screenWidth);
    if (ImGui::SliderFloat("screen width (m)", &g_screenWidth, 0.5f, 6.0f, "%.2f"))
        dvr::vr::set_screen(g_screenDist, g_screenWidth);

    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Comfort")) {
    // 30.54: input rescue
    {
        bool af = g_autoFocus;
        if (ImGui::Checkbox("auto-refocus game when SteamVR steals it", &af))
            g_autoFocus = af;
        ImGui::SameLine();
        if (ImGui::Button("Refocus now")) g_focusNow = true;
    }

    {
        bool pc = g_crouchOn;
        if (ImGui::Checkbox("real crouching crouches Corvo", &pc)) g_crouchOn = pc;
        ImGui::SameLine();
        ImGui::TextDisabled(g_crouchHeld ? "[DOWN]" : "");
        ImGui::SliderFloat("crouch trigger (m below standing)", &g_crouchDropM, 0.08f, 0.70f, "%.2f");
        ImGui::TextDisabled("F5 re-sets your standing height");
    }

    bool pt = g_posTrack;
    if (ImGui::Checkbox("positional head tracking", &pt)) {
        g_posTrack = pt;
        if (!pt) { g_leanRightUU = 0; g_leanUpUU = 0; g_leanFwdUU = 0; }
    }

    // 41.1 [Neck]: the pitch pivot three-way, here because the tester is in a
    // headset. Look up and down at something an arm's length away after each
    // button: with the right mode it stays put.
    ImGui::Separator();
    ImGui::TextUnformatted("neck (pitch pivot)");
    {
        ImGui::Text("now: %s | arc R%+.1f U%+.1f F%+.1f uu at pitch %+.0f deg", NeckModeName(g_neckMode),
                    g_neckArcUu[0], g_neckArcUu[1], g_neckArcUu[2], g_hmdPitch * 57.29578f);
        if (ImGui::Button("off"))    NeckSet(0, g_neckBelowM, g_neckBehindM, "F10 Comfort");
        ImGui::SameLine();
        if (ImGui::Button("add"))    NeckSet(1, g_neckBelowM, g_neckBehindM, "F10 Comfort");
        ImGui::SameLine();
        if (ImGui::Button("cancel")) NeckSet(2, g_neckBelowM, g_neckBehindM, "F10 Comfort");
        float below = g_neckBelowM, behind = g_neckBehindM;
        ImGui::SliderFloat("pivot below eyes (m)", &below, 0.0f, 0.30f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) NeckSet(g_neckMode, below, g_neckBehindM, "F10 Comfort slider");
        else g_neckBelowM = below;
        ImGui::SliderFloat("pivot behind eyes (m)", &behind, 0.0f, 0.30f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) NeckSet(g_neckMode, g_neckBelowM, behind, "F10 Comfort slider");
        else g_neckBehindM = behind;
        ImGui::TextDisabled("look up and down at something an arm's length away: with the right mode it stays put");
        ImGui::TextDisabled("`camera pitchtest` measures the engine's own neck; use those numbers with cancel");
    }

    ImGui::EndTabItem(); }

    // 38.61 ship polish: blink is a finished feature with good defaults -
    // its tuning panel is dev-only now (the aim itself stays ON).
    if (g_ovlDev)
    if (ImGui::BeginTabItem("Blink")) {
    {
        // 32.27: this is a working feature now, so it gets ONE control and
        // the scaffolding that found it is gone. Every dead experiment left in
        // this panel is a thing the user has to read past - and the overlay
        // being full of retired R&D has already been complained about three
        // times in this project.
        //   deleted: the +0x060 write test (proven not to reach the teleport),
        //   the 0xbf595f aim detour (fired zero times - wrong branch), and
        //   both hardware-breakpoint traces (their job is done; the addresses
        //   are recorded in the project notes if anyone needs to re-derive).
        bool dv = g_blkDriveUI;
        if (ImGui::Checkbox("Blink aims with your CONTROLLER", &dv)) {
            g_blkDriveUI = dv;
            g_blkAimOnCfg = dv;
            if (dv) g_blkDstReqUI = g_blkDstOnUI ? 0 : 1;
            Log("blink: controller aim %s", dv ? "ON" : "off");
        }
        ImGui::TextDisabled(g_blkDstOnUI
            ? "native detour live at 0xbf5e4f - aims with your %s hand"
            : "arming when the power loads...", g_maimHand ? "right" : "left");
        if (g_ovlDev)
        if (ImGui::Button("capture a frame WITH the marker (then hold an aim)",
                          ImVec2(-1, 0))) {
            g_blkAutoDump = 1;
            Log("blink: armed - the next Blink aim triggers the frame dump");
        }
        ImGui::SeparatorText("how far you blink");
        int rm = g_blkReachMode;
        ImGui::RadioButton("head distance (original)", &rm, 0);
        ImGui::RadioButton("fixed reach", &rm, 1);
        ImGui::RadioButton("HAND PITCH sets the distance", &rm, 2);
        if (rm != g_blkReachMode) {
            g_blkReachMode = rm;
            Log("blink: reach mode -> %d (0=head 1=fixed 2=hand pitch)", rm);
        }
        ImGui::SliderFloat("max reach (uu, 0 = auto)", &g_blkReachUU,
                           0.0f, 2000.0f, "%.0f");
        ImGui::TextDisabled("auto-learned max so far: %.0f uu", g_blkReachSeen);
        if (g_blkReachMode == 2) {
            ImGui::SliderFloat("near reach (uu)", &g_blkNearUU, 50.0f, 900.0f, "%.0f");
            ImGui::SliderFloat("pitch = near (deg)", &g_blkPitchNear,
                               -89.0f, 0.0f, "%.0f");
            ImGui::SliderFloat("pitch = far (deg)",  &g_blkPitchFar,
                               -60.0f, 45.0f, "%.0f");
            ImGui::TextDisabled("hand pitch right now: %+.0f deg  ->  %.0f uu",
                                g_blkPitchNow, g_blkAimDistUU);
            ImGui::TextDisabled("point down at your feet = short hop, level = max");
        }
        // 32.52: source aim is THE path now, so the three toggles that
        // existed to work around its absence are gone - the trace-redirect
        // option (two detour sites that never execute), the head-marker
        // suppression, and the source-aim switch itself. A setting whose
        // answer is settled is not a choice, it is clutter.
        ImGui::TextDisabled(g_blkDirOn
            ? "aiming at the source (0xbf55a3): the engine traces along your"
              " hand, so it does its own collision"
            : "arming when the power loads...");
        bool bm = g_blkMarker;
        if (ImGui::Checkbox("bright VR marker on the landing spot", &bm))
            g_blkMarker = bm;
        if (g_blkMarker) {
            ImGui::SliderFloat("marker pull-back (uu)", &g_blkMarkerBackUU,
                               0.0f, 250.0f, "%.0f");
            ImGui::TextDisabled("raise this if the dot vanishes against a wall");
        }
    }

    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Hands")) {
    {
    if (!g_skcPlayerN) {
        ImGui::TextDisabled("finding the hand controls... load a save and");
        ImGui::TextDisabled("stand in gameplay for a few seconds.");
    } else {
        // 38.61 ship polish: the raw drive experiment block is dev-only.
        // The drive itself is ON automatically (ini defaults) - a player
        // never needs these, and three complaints about retired R&D
        // cluttering this panel are on the record.
        if (g_ovlDev) {
        ImGui::Separator();
        ImGui::TextDisabled("%d SkelControl(s) found on the PLAYER rig:", g_skcPlayerN);
        bool dr = g_skcDrive;
        if (ImGui::Checkbox("DRIVE them (watch the arms)", &dr)) {
            g_skcDrive = dr;
            Log("skc: drive %s on %d player controls", dr ? "ON" : "off", g_skcPlayerN);
        }
        ImGui::SliderFloat("strength", &g_skcStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("move X (uu)", &g_skcTrans[0], -150.0f, 150.0f, "%.0f");
        ImGui::SliderFloat("move Y (uu)", &g_skcTrans[1], -150.0f, 150.0f, "%.0f");
        ImGui::SliderFloat("move Z (uu)", &g_skcTrans[2], -150.0f, 150.0f, "%.0f");
        ImGui::SliderInt("space (0 world 1 actor 2 comp 3 parent)", &g_skcSpace, 0, 5);
        ImGui::Separator();
        bool lv = g_skcLive;
        if (ImGui::Checkbox("drive from the CONTROLLERS (the real thing)", &lv)) {
            g_skcLive = lv; g_skcRecap = 1;
        }
        ImGui::TextDisabled("#1 = your left hand, #2 = your right.");
        ImGui::TextDisabled("The camera control is left alone.");
        }
        if (ImGui::Button(g_skcCalGo ? "CALIBRATING - hold still..."
                                     : "CALIBRATE HANDS  (3 s, saves forever)",
                          ImVec2(-1, 0)))
            g_skcCalReq = 1;
        ImGui::TextDisabled(g_skcNeutralSaved
            ? "neutral is saved - the hands land identically every launch"
            : "no saved neutral yet - it will save itself on first capture");
        ImGui::TextDisabled("only needed once. It MOVES the hands, so the trim");
        ImGui::TextDisabled("below has to be re-tuned after. F5 does not do this.");
        // 38.61: mode checkboxes, travel/reach scales, the mesh-rotation
        // experiment, clamp and placement mode - all dev-only. Hand size
        // stays: that one is a player preference.
        if (g_ovlDev) {
        bool dt = g_skcDoTrans, dr2 = g_skcDoRot, am = g_skcAddMode;
        if (ImGui::Checkbox("position", &dt)) g_skcDoTrans = dt;
        ImGui::SameLine();
        if (ImGui::Checkbox("rotation", &dr2)) g_skcDoRot = dr2;
        ImGui::SameLine();
        if (ImGui::Checkbox("add to anim", &am)) g_skcAddMode = am;
        ImGui::SliderFloat("hand travel (uu/m)", &g_skcScaleUU, 0.0f, 200.0f, "%.0f");
        }
    ImGui::SliderFloat("hand / weapon size", &g_skcHandSize, 0.4f, 1.6f, "%.2f");
    if (g_ovlDev) {
    ImGui::SliderFloat("world reach (uu/m)", &g_skcWorldScale, 40.0f, 200.0f, "%.0f");
    ImGui::TextDisabled("world mode only - too low puts the hands in your face");
    ImGui::TextDisabled("rotator writes: %ld/s (needs to be in the thousands)",
                        (long)g_skcRotWrites);
    ImGui::SliderFloat("camera look-at strength", &g_skcCamStrength, 0.0f, 1.0f, "%.2f");
    ImGui::TextDisabled("1 = stock. Lower it if the ARMS follow your view -");
    ImGui::TextDisabled("this is the game's own look-at-the-camera control.");
    ImGui::SliderFloat("left  hand control", &g_skcHandCtlStr[0], 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("right hand control", &g_skcHandCtlStr[1], 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("counter head turn", &g_skcCounterYaw, 0.0f, 1.5f, "%.2f");
    {
        bool rm = g_skcRemoveMeshRot;
        if (ImGui::Checkbox("experiment: remove mesh rotation", &rm))
            g_skcRemoveMeshRot = rm;
        ImGui::TextDisabled("only does anything with rotation on; last untried flag");
    }
    ImGui::TextDisabled("raise until turning your head stops taking the");
    ImGui::TextDisabled("hands with it (offset mode only)");
        ImGui::SliderFloat("clamp (uu)", &g_skcMax, 0.0f, 300.0f, "%.0f");
        bool wr = g_skcWorld;
        if (ImGui::Checkbox("world-space placement (kills head drift)", &wr))
            g_skcWorld = wr;
        ImGui::TextDisabled("off = offset along the forearm (drifts with head)");
    }
        // 32.25: the sliders edit whichever stance you are actually IN, so
        // crouch down, tune, stand up, and both are remembered. No mode to
        // pick, nothing to press.
        bool cst = g_skcCrouchTrimOn;
        if (ImGui::Checkbox("separate trim while crouched", &cst))
            g_skcCrouchTrimOn = cst;
        // 32.35: show BOTH sets, always. 32.25 only exposed the set matching
        // your current stance - which is unusable, because to tune the crouch
        // numbers you had to be crouched WHILE the overlay was open, and the
        // overlay takes the mouse. And since the crouch set starts as a copy of
        // the standing set, ticking the box changed nothing visible: it looked
        // broken when it was merely unreachable. The detection was fine all
        // along - the log has Pawn.bIsCrouched at +0x280 mask 0x8 and clean
        // CROUCHED/standing transitions.
        // 32.40: the practical blocker was never the detector - it was that
        // tuning the crouch numbers means being crouched WHILE the overlay is
        // open, and the overlay owns the controllers. This forces the crouched
        // pose on so the sliders can be dialled in with both hands free.
        bool fc = g_crouchForce;
        if (ImGui::Checkbox("FORCE crouched pose (for tuning the sliders)", &fc)) {
            g_crouchForce = fc;
            Log("crouch: force-crouched %s", fc ? "ON" : "off");
        }
        if (g_crouchForce)
            ImGui::TextDisabled("turn this off when you are done tuning");
        int cs = g_crouchSrc;
        ImGui::RadioButton("stance from MY CROUCH BUTTON (recommended)", &cs, 3);
        ImGui::RadioButton("stance from eye height", &cs, 1);
        ImGui::RadioButton("stance from Pawn.bIsCrouched", &cs, 0);
        ImGui::RadioButton("stance from Pawn.bWantsToCrouch", &cs, 2);
        if (cs != g_crouchSrc) {
            g_crouchSrc = cs;
            Log("crouch: source -> %d (0=bIsCrouched 1=eye 2=bWants 3=button)", cs);
        }
        if (g_crouchSrc == 3) {
            bool tg = g_crouchToggle;
            if (ImGui::Checkbox("my crouch button is a TOGGLE", &tg)) {
                g_crouchToggle = tg; g_crouchTogLock = true;
                Log("crouch/btn: set by hand to %s-style", tg ? "toggle" : "hold");
            }
            ImGui::TextDisabled("%s  -  button %s, stance %s",
                g_crouchTogLock ? "measured from your own crouches" : "assumed;"
                " crouch twice and it measures itself",
                (InterlockedCompareExchange(&g_sneakBtn, 0, 0) != 0) ? "DOWN" : "up",
                g_crouchBtnSt ? "CROUCHED" : "standing");
            ImGui::TextDisabled("crouch button mask 0x%04x   pad now 0x%04x",
                (unsigned)g_crouchBtnMask,
                (unsigned)InterlockedCompareExchange(&g_padBtnsPub, 0, 0));
            if (ImGui::Button(g_crouchBindArm
                    ? "...now press your crouch button"
                    : "bind my crouch button", ImVec2(-1, 0))) {
                g_crouchBindArm = !g_crouchBindArm;
                Log("crouch/btn: bind %s", g_crouchBindArm ? "armed" : "cancelled");
            }
            if (ImGui::Button("re-measure hold vs toggle", ImVec2(-1, 0))) {
                g_crouchTogLock = false; g_crouchTogVote = 0;
                Log("crouch/btn: re-measuring hold vs toggle");
            }
        }
        ImGui::SliderFloat("must hold for (ms)", &g_crouchHoldMs,
                           0.0f, 1000.0f, "%.0f");
        ImGui::TextDisabled("bWantsToCrouch %s", g_bWantsCrFound ? "found" : "not found yet");
        if (g_crouchSrc == 1) {
            ImGui::SliderFloat("min stance separation (uu)", &g_eyeDropUU,
                               6.0f, 60.0f, "%.0f");
            if (!g_actorLocFound)
                ImGui::TextDisabled("(looking for Actor.Location...)");
            else
                ImGui::TextDisabled("eye now %.0f   standing %.0f   crouched %.0f uu",
                                    g_eyeNowUU, g_eyeStandUU, g_eyeCrouchUU);
            if (ImGui::Button("re-learn my standing height", ImVec2(-1, 0))) {
                g_eyeHaveRef = false;
                Log("crouch/eye: baselines reset by the overlay");
            }
        } else if (!g_bIsCrouchFound) {
            ImGui::TextDisabled("(looking for Pawn.bIsCrouched...)");
        }
        ImGui::TextDisabled("pawn is %s right now",
                            g_pawnCrouched ? "CROUCHED" : "standing");
        ImGui::TextDisabled("trim in uu: forward / right / up");
        bool liveStand = !(g_skcCrouchTrimOn && g_pawnCrouched);
        ImGui::SeparatorText(liveStand ? "STANDING  <-- live now" : "STANDING");
        ImGui::SliderFloat("L fwd##s",   &g_skcTrim[0][0], -60.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("L right##s", &g_skcTrim[0][1], -60.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("L up##s",    &g_skcTrim[0][2], -60.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("R fwd##s",   &g_skcTrim[1][0], -60.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("R right##s", &g_skcTrim[1][1], -60.0f, 60.0f, "%.0f");
        ImGui::SliderFloat("R up##s",    &g_skcTrim[1][2], -60.0f, 60.0f, "%.0f");
        if (g_skcCrouchTrimOn) {
            ImGui::SeparatorText(liveStand
                ? "CROUCH OFFSET (added to standing)"
                : "CROUCH OFFSET (added to standing)  <-- live now");
            ImGui::TextDisabled("all zeros = crouching behaves exactly like standing");
            ImGui::SliderFloat("L fwd##c",   &g_skcTrimCrouch[0][0], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("L right##c", &g_skcTrimCrouch[0][1], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("L up##c",    &g_skcTrimCrouch[0][2], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R fwd##c",   &g_skcTrimCrouch[1][0], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R right##c", &g_skcTrimCrouch[1][1], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R up##c",    &g_skcTrimCrouch[1][2], -60.0f, 60.0f, "%.0f");
            if (ImGui::Button("zero the crouch offset", ImVec2(-1, 0))) {
                memset(g_skcTrimCrouch, 0, sizeof(g_skcTrimCrouch));
                Log("hands: crouch offset zeroed - crouch now matches standing");
            }
        }
        if (g_skcBlockTrimOn) {   // 34.9
            ImGui::SeparatorText(g_blockHeld && !g_pawnCrouched
                ? "BLOCK OFFSET (standing block)  <-- live now"
                : "BLOCK OFFSET (standing block)");
            ImGui::TextDisabled("arm too far out while blocking? pull fwd negative");
            ImGui::SliderFloat("L fwd##b",   &g_skcTrimBlock[0][0], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("L right##b", &g_skcTrimBlock[0][1], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("L up##b",    &g_skcTrimBlock[0][2], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R fwd##b",   &g_skcTrimBlock[1][0], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R right##b", &g_skcTrimBlock[1][1], -60.0f, 60.0f, "%.0f");
            ImGui::SliderFloat("R up##b",    &g_skcTrimBlock[1][2], -60.0f, 60.0f, "%.0f");
            if (ImGui::Button("zero the block offset", ImVec2(-1, 0))) {
                memset(g_skcTrimBlock, 0, sizeof(g_skcTrimBlock));
                Log("hands: block offset zeroed");
            }
        }
        if (g_ovlDev)
        if (g_graftDonorN && g_skcPlayerN) {   // 35.8: the real drive
            ImGui::SeparatorText("HAND ROTATION (donor graft)");
            bool go = g_graftWant;
            if (ImGui::Checkbox("rotation drive: hands follow controller twist", &go)) {
                g_graftWant = go;
                GraftTestSet(go);
            }
            if (g_graftOn) {
                ImGui::TextDisabled("engaged: %s%s   writes %ld",
                    g_graftHand[0] >= 0 ? (g_graftHand[0] ? "R" : "L") : "",
                    g_graftHand[1] >= 0 ? (g_graftHand[1] ? "R" : "L") : "",
                    (long)g_skcRotWrites);
                if (ImGui::Button("zero neutral (hold hands forward first)",
                                  ImVec2(-1, 0)))
                    SkcRotZeroNeutral("overlay");
                int spc = g_graftRotSpace;
                const char* spcNames[5] =
                    { "0 world", "1 actor", "2 comp", "3 parent", "4 bone" };
                if (ImGui::SliderInt("rot space", &spc, 0, 4,
                                     spcNames[spc < 0 ? 0 : (spc > 4 ? 4 : spc)]))
                    g_graftRotSpace = spc;
                bool fy = g_skcRotSignY < 0, fp = g_skcRotSignP < 0;
                if (ImGui::Checkbox("flip yaw##gr", &fy))
                    g_skcRotSignY = fy ? -1 : 1;
                ImGui::SameLine();
                if (ImGui::Checkbox("flip pitch##gr", &fp))
                    g_skcRotSignP = fp ? -1 : 1;
                bool aa = g_graftAimAbs;     // 36.4: the measured answer
                if (ImGui::Checkbox("absolute aim (no head math) <- measured fix", &aa))
                    g_graftAimAbs = aa;
                if (g_graftAimAbs) {         // 36.5: measure alpha by hand
                    ImGui::SliderFloat("head follow yaw", &g_graftHCY, -2.0f, 2.0f, "%.2f");
                    ImGui::SliderFloat("head follow pitch", &g_graftHCP, -2.0f, 2.0f, "%.2f");
                    ImGui::TextDisabled("turn your head; drag each until the"
                                        " weapon stops moving. then SAVE");
                }
                if (!g_graftAimAbs) {
                    bool hc = g_graftHeadComp;   // 36.0: world compose
                    if (ImGui::Checkbox("world-stable aim (view compose)", &hc))
                        g_graftHeadComp = hc;
                    if (g_graftRotSpace != 0 && g_graftHeadComp)
                        ImGui::TextDisabled("compose only acts in rot space 0");
                }
                ImGui::TextDisabled("wrong direction? flip signs. no effect at"
                                    " all? try another rot space");
            } else if (g_graftWant) {
                ImGui::TextDisabled("waiting for rig (re-grafts after reload)");
            }
        }
        if (g_ovlDev)
        {   // 36.7: finisher event capture (log-only diagnostic)
            ImGui::SeparatorText("FINISHER CAPTURE (diagnostic)");
            bool fc = g_finCapOn;
            if (ImGui::Checkbox("log player events (do 2-3 finishers, then off)", &fc)) {
                g_finCapOn = fc;
                Log("fincap: %s", fc ? "ON - kill somebody fancy" : "off");
            }
        }
        if (g_ovlDev)
        ImGui::TextDisabled("writes %ld/s", (long)g_skcHits);
        }
}

    // 31.5: the VR-hands model panel is DELETED from the overlay. We are not
    // using our own models - the engine drives the real hands now - so a dozen
    // controls for it were pure noise. The feature still exists behind
    // [VRHands] Enabled=1 for anyone who wants it; it just no longer occupies
    // the panel of a system that replaced it.
    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Display")) {
    // 41.1: the stereo arming tickbox, TICKED by default (the user's ask). It
    // parks the selected method on the mono screen without forgetting it; the
    // selection is the ini's [Stereo] Method or `stereo <name>`.
    {
        const bool monoWanted = !_stricmp(dvr::stereo::wanted_name(), "mono");
        bool armed = dvr::stereo::armed();
        if (monoWanted) ImGui::BeginDisabled();
        if (ImGui::Checkbox("stereo armed", &armed)) dvr::stereo::set_armed(armed);
        if (monoWanted) ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s: active %s, %s", dvr::stereo::wanted_name(), dvr::stereo::active_name(),
                            dvr::stereo::wants_projection() ? "projection layer" : "head-locked screen");
        if (monoWanted) {
            ImGui::SameLine();
            if (ImGui::Button("select reentry")) dvr::stereo::choose("reentry");
        }
        ImGui::Separator();
    }
    // 41.1: the render-resolution picker. Takes effect at the NEXT LAUNCH (the
    // engine's setres does nothing on this build, measured); the size goes into
    // the game's own ini, and a size the display does not list needs
    // VirtualMode (core/window/render_size.cpp). Defaults to the runtime's
    // recommended per-eye size, the user's ask.
    {
        static int  sel = -2;          // -2 = not chosen yet (defaults to the eye entry)
        static int  customW = 2496, customH = 2688;
        static bool full = true;
        static bool modesRead = false;
        if (!modesRead) { ResEnumModes("F10"); modesRead = true; }
        uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh);
        const uint32_t cw = dvr::capture::width(), ch = dvr::capture::height();
        const float claim = dvr::camera::fov_deg();
        const float dens = (cw && claim > 1.0f) ? (float)cw / (2.0f * tanf(claim * 0.5f * 0.0174533f)) / 57.29578f : 0.0f;
        float hh = 0.0f, hv = 0.0f; dvr::vr::headset_half_fov_deg(&hh, &hv);
        const float eyeDens = (ew && hh > 0.0f) ? (float)ew / (2.0f * tanf(hh * 0.0174533f)) / 57.29578f : 0.0f;
        ImGui::TextUnformatted("render resolution (takes effect at the next launch)");
        ImGui::Text("game renders %ux%u | asked %ux%u %s | eye recommended %ux%u", cw, ch, g_resWantW, g_resWantH,
                    g_resWantFull ? "fullscreen" : "windowed", ew, eh);
        ImGui::Text("claim %.1f deg -> centre %.1f px/deg (the eye wants %.1f)", claim, dens, eyeDens);
        // the combo: the eye entry, the display's modes, custom
        const int nModes = g_resModeN;
        const int eyeIdx = 0, customIdx = 1 + nModes;
        if (sel == -2) { sel = ew ? eyeIdx : customIdx; if (ew) { customW = (int)ew; customH = (int)eh; } }
        char label[64];
        if (sel == eyeIdx) _snprintf(label, sizeof(label), "Quest 3 per eye (runtime): %ux%u", ew, eh);
        else if (sel == customIdx) _snprintf(label, sizeof(label), "custom");
        else _snprintf(label, sizeof(label), "%ux%u", g_resModes[sel - 1][0], g_resModes[sel - 1][1]);
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("size", label)) {
            _snprintf(label, sizeof(label), "Quest 3 per eye (runtime): %ux%u%s", ew, eh,
                      ew && !ResIsMode(ew, eh) ? "  (not a display mode: needs VirtualMode)" : "");
            if (ImGui::Selectable(label, sel == eyeIdx)) sel = eyeIdx;
            for (int k = 0; k < nModes; ++k) {
                _snprintf(label, sizeof(label), "%ux%u", g_resModes[k][0], g_resModes[k][1]);
                if (ImGui::Selectable(label, sel == 1 + k)) sel = 1 + k;
            }
            if (ImGui::Selectable("custom", sel == customIdx)) sel = customIdx;
            ImGui::EndCombo();
        }
        if (sel == customIdx) {
            ImGui::SetNextItemWidth(120); ImGui::InputInt("W", &customW, 16, 128);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120); ImGui::InputInt("H", &customH, 16, 128);
        }
        ImGui::Checkbox("fullscreen", &full);
        ImGui::SameLine();
        bool virt = g_resVirtual;
        if (ImGui::Checkbox("VirtualMode (provide a size the display lacks)", &virt)) g_resVirtual = virt;
        if (ImGui::Button("Apply (writes the game's ini; relaunch)")) {
            uint32_t w = 0, h = 0;
            if (sel == eyeIdx) { w = ew; h = eh; }
            else if (sel == customIdx) { w = (uint32_t)(customW > 0 ? customW : 0); h = (uint32_t)(customH > 0 ? customH : 0); }
            else { w = g_resModes[sel - 1][0]; h = g_resModes[sel - 1][1]; }
            ResRequest(w, h, full, "F10 Display");
        }
        ImGui::SameLine();
        if (ImGui::Button("refresh modes")) ResEnumModes("F10");
        ImGui::SameLine();
        if (ImGui::Button("use the game's own size")) ResCommand("0x0");
        ImGui::TextDisabled("%s", g_resLastLine);
        ImGui::Separator();
    }
    // 41.1 (session 8): the tick budget and the freeze marker. The numbers are
    // the last 3 s window's (core/framework/perf); MARK stamps the log with
    // the ring's surroundings so an attack freeze becomes evidence.
    {
        const dvr::perf::Window pw = dvr::perf::last_window();
        ImGui::Text("tick %.1f ms (%.1f/s) = in %.1f + out %.1f (idle %.1f R %.1f) | capture %.1f [lock %.1f] wait %.1f%s",
                    pw.tickMs, pw.ticksPerS, pw.inMs, pw.outMs, pw.idleMs, pw.rMs, pw.captureMs, pw.lockMs,
                    pw.waitMs, pw.paceBound ? "  PACE-BOUND" : "");
        ImGui::Text("gpu %s: span %.1f ms (dma %.1f) idle %.1f | marker %s | capture mode %s",
                    pw.gpu[0] ? pw.gpu : "-", pw.gpuSpanMs, pw.gpuDmaMs, pw.gpuIdleMs,
                    pw.marker[0] ? pw.marker : "-", dvr::capture::mode_name());
        if (ImGui::Button("MARK (stamp a freeze now)", ImVec2(-1, 0))) dvr::perf::mark("F10", "F10");
        ImGui::TextDisabled("marks so far: %u (the log gets three Warn lines per press)", pw.marks);
        // 41.1 (session 8): the capture mode, live (the same setter as the seam
        // word; shared needs the 9Ex device below, off freezes the image on purpose).
        {
            static const char* kModes[] = {"sync (readback, waits)", "deferred (readback, one present late)",
                                           "shared (VRAM, needs the D3D9Ex device)", "off (frozen image: the A/B control)"};
            int mode = (int)dvr::capture::mode();
            ImGui::SetNextItemWidth(320);
            if (ImGui::Combo("capture mode", &mode, kModes, 4))
                dvr::capture::set_mode(mode == 0 ? "sync" : mode == 1 ? "deferred" : mode == 2 ? "shared" : "off");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", !dvr::capture::probed() ? "probe pending"
                                      : dvr::capture::shared_available() ? "shared AVAILABLE" : "shared refused (plain device)");
        }
        // 41.1 (session 9): THE EYES - the frame-identity readout and the one-
        // press versions of the headset run's words, so the whole run happens
        // in the headset (the same functions as the seam words).
        {
            ImGui::Separator();
            const dvr::frameid::Last fl = dvr::frameid::last();
            ImGui::Text("EYES: %s | pairs %u | L-R diff bb %.1f slot %.1f out %.1f sc %.1f (floor %.1f; one picture = below it)",
                        fl.pairs == 0 ? "no pairs yet (the quad screen, or not armed)" : fl.onePicture ? "ONE PICTURE" : "two pictures",
                        fl.pairs, fl.diff[0], fl.diff[1], fl.diff[2], fl.diff[3], fl.floorBb);
            ImGui::Text("      side %s | picture shift %+d px (negative = a true pair) | swapped pairs %u | c5 pairing %s",
                        fl.side, fl.shift, fl.swapped, dvr::stereo::reentry_c5_pair() ? "on" : "OFF");
            if (ImGui::Button("DUMP EYES (a left + a right PNG; no stall)")) FrameDumpRequest("eyes");
            ImGui::SameLine();
            if (ImGui::Button("REARM 2 (two single ticks, capture untouched)")) SceneDrawCommand("rearm 2");
            ImGui::SameLine();
            if (ImGui::Button("CAPTURE REINIT (rebuild the slots)")) dvr::capture::request_reinit();
            if (ImGui::Button("PROJECTION OFF (the quad)")) dvr::stereo::set_projection_override(0);
            ImGui::SameLine();
            if (ImGui::Button("PROJECTION AUTO (back to per-eye)")) dvr::stereo::set_projection_override(-1);
            ImGui::SameLine();
            bool c5 = dvr::stereo::reentry_c5_pair();
            if (ImGui::Checkbox("c5 pairing (off = the old order-only tags: the A/B)", &c5)) {
                dvr::stereo::set_reentry_c5_pair(c5);
                ConfigWriteKey("Stereo", "C5Pair", c5 ? "1" : "0", "F10 Display");
            }
            bool fid = dvr::frameid::enabled();
            if (ImGui::Checkbox("frame-identity trace (one pair every 8 ticks; off = no cost at all)", &fid)) {
                dvr::frameid::set_enabled(fid);
                ConfigWriteKey("Perf", "FrameId", fid ? "1" : "0", "F10 Display");
            }
            ImGui::TextDisabled("the log's `stereo: frameid` line has the same numbers once a second; use the capture mode combo above for deferred -> shared");
            ImGui::Separator();
        }
        // 41.2 (session 10): the HUD panel and the draw census that measured it.
        {
            ImGui::Text("HUD");
            bool hp = dvr::hudcap::enabled();
            if (ImGui::Checkbox("HUD panel (the game's HUD on a head-locked quad; off = in the frame)", &hp)) {
                dvr::hudcap::set_enabled(hp);
                ConfigWriteKey("Hud", "Panel", dvr::hudcap::enabled() ? "1" : "0", "F10 Display");
            }
            if (dvr::hudcap::enabled())
                ImGui::TextDisabled("while this is on the HUD is NOT in the eye textures or the desktop window; "
                                    "menus, loading and cutscenes leave it in the frame on purpose. Distance, "
                                    "width and height are the HUD sliders on the Runtime tab");
            else
                ImGui::TextDisabled("off: the game draws its HUD into the frame, as it always has");
            bool dc = dvr::draws::enabled();
            if (ImGui::Checkbox("draw census (a table and a VERDICT every 3 s; off = one bool per draw)", &dc)) {
                dvr::draws::set_enabled(dc);
                ConfigWriteKey("Draws", "Census", dvr::draws::enabled() ? "1" : "0", "F10 Display");
            }
            ImGui::Separator();
        }
        // 41.1 (session 8): the 9Ex device lever (next launch) - what lets the
        // capture keep the frame in VRAM ([Capture] Mode=shared).
        {
            static int exAsk = -1;
            if (exAsk < 0) exAsk = dvr::d3d9ex::ex_wanted() ? 1 : 0;
            bool ex = exAsk != 0;
            if (ImGui::Checkbox("D3D9Ex device at the NEXT launch (and capture mode shared with it)", &ex)) { exAsk = ex ? 1 : 0; DeviceSetEx(ex, "F10 Display"); }
            ImGui::SameLine();
            ImGui::TextDisabled("this run: %s", dvr::d3d9ex::device_is_ex() ? "9Ex" : "plain D3D9");
        }
        ImGui::Separator();
    }
    // 30.47: on-demand camera experiments (auto-start fired them at the main
    // menu before, where nobody could see the result).
    // 30.50: the FOV lever - enforced on every script dispatch so it outruns
    // the engine's per-tick recompute (bioshock-vr's mechanism).
    {
        float lever = g_fovLever;
        bool on = lever >= 40.0f;
        if (ImGui::Checkbox("FOV lever (force the game's rendered FOV)", &on))
            g_fovLever = on ? 95.0f : 0.0f;
            dvr::camera::set_fov_deg(g_fovLever);
        if (on) {
            if (ImGui::SliderFloat("  target FOV (deg)", &lever, 60.0f, 140.0f, "%.0f"))
                g_fovLever = lever;
                dvr::camera::set_fov_deg(g_fovLever);
            ImGui::TextDisabled("  the lever writes the game camera's FOV every dispatch");
        }
    }

    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Runtime")) {
        dvr::vr::draw_debug_ui();   // the runtime layer's own panel
    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Log")) {
        // The log ring (core/util/log.h): what the mod is doing right now,
        // readable in the headset. Filter by category and level; the same
        // lines are in dishonored_vr.log.
        static int  filterCat = -1;          // -1 = all
        static int  minLevel = 2;            // info
        static bool autoScroll = true;
        static dvr::log::RingLine lines[512];
        static const char* kLevels[] = { "error", "warn", "info", "debug", "trace" };
        ImGui::SetNextItemWidth(120);
        if (ImGui::BeginCombo("category", filterCat < 0 ? "all" : dvr::log::cat_name((dvr::log::Cat)filterCat))) {
            if (ImGui::Selectable("all", filterCat < 0)) filterCat = -1;
            for (int c = 0; c < (int)dvr::log::Cat::COUNT; c++)
                if (ImGui::Selectable(dvr::log::cat_name((dvr::log::Cat)c), filterCat == c)) filterCat = c;
            ImGui::EndCombo();
        }
        ImGui::SameLine(); ImGui::SetNextItemWidth(90);
        ImGui::Combo("show", &minLevel, kLevels, 5);
        ImGui::SameLine(); ImGui::Checkbox("follow", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("copy tail")) {
            size_t n = dvr::log::ring_copy(lines, 512);
            static char clip[512 * 240];
            size_t k = 0;
            for (size_t i = (n > 60 ? n - 60 : 0); i < n && k + 240 < sizeof(clip); i++)
                k += (size_t)snprintf(clip + k, sizeof(clip) - k, "[%lu] [%s] %s\n",
                                      (unsigned long)lines[i].tick, dvr::log::cat_name((dvr::log::Cat)lines[i].cat), lines[i].text);
            ImGui::SetClipboardText(clip);
        }
        ImGui::SameLine();
        if (ImGui::Button("status.json")) dvr::status::write_now();
        ImGui::BeginChild("logring", ImVec2(0, 0), true);
        size_t n = dvr::log::ring_copy(lines, 512);
        for (size_t i = 0; i < n; i++) {
            const dvr::log::RingLine& l = lines[i];
            if (filterCat >= 0 && l.cat != filterCat) continue;
            if (l.level > minLevel) continue;
            ImVec4 col = l.level == 0 ? ImVec4(1, 0.4f, 0.4f, 1) : l.level == 1 ? ImVec4(1, 0.85f, 0.4f, 1)
                       : l.level >= 3 ? ImVec4(0.6f, 0.6f, 0.6f, 1) : ImVec4(0.9f, 0.9f, 0.9f, 1);
            ImGui::TextColored(col, "[%s] %s", dvr::log::cat_name((dvr::log::Cat)l.cat), l.text);
        }
        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 40) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    ImGui::EndTabItem(); }

    if (ImGui::BeginTabItem("Advanced")) {
        bool dv = g_ovlDev;
        if (ImGui::Checkbox("developer tools", &dv)) g_ovlDev = dv;
        ImGui::SameLine();
        ImGui::TextDisabled("probes, rig tests, the SpaceBases oracle");
        ImGui::Separator();
        ImGui::TextDisabled("game window %ux%u", dvr::capture::width(), dvr::capture::height());
    ImGui::EndTabItem(); }

    ImGui::EndTabBar();
    ImGui::TextDisabled("F10 closes");
    ImGui::End();

    ImGui::Render();
}
