// core/ui/overlay_tabs.inc - VR-196: the F10 panel's tabs, one function per tab.
// Included by overlay.cpp (unity build) just before OverlayFrame.
//
// Every control keeps the setter and the ini key it had before this rewrite; only where
// it is drawn, its tier (dvr::ovl) and its hover description changed. The per-control
// disposition, including what was removed and why, is docs/dishonored/F10_AUDIT.md.

static const int kOvlB = dvr::ovl::Basic, kOvlA = dvr::ovl::Advanced, kOvlD = dvr::ovl::Debug;
static bool OvlAt(int tier) { return dvr::ovl::show(tier); }
static void OvlTip(const char* text) { dvr::ovl::tip(text); }
static bool OvlSec(const char* name, int tier, const char* tipText) { return dvr::ovl::section(name, tier, tipText, !strcmp(name, "Hand size and position") || !strcmp(name, "Sleeve")); }
// Save the slider just submitted when it is released (VR-196: every Basic and Advanced control
// saves on change, so SAVE AS DEFAULTS is never needed to keep a player's setting).
static void OvlSaveOnRelease(const char* section, const char* key, float value, const char* fmt)
{
    if (!ImGui::IsItemDeactivatedAfterEdit()) return;
    char v[32]; _snprintf(v, sizeof(v), fmt, value); v[sizeof(v) - 1] = 0;
    ConfigWriteKey(section, key, v, "F10");
}
static void OvlSaveBool(const char* section, const char* key, bool on) { ConfigWriteKey(section, key, on ? "1" : "0", "F10"); }
static void OvlSaveInt(const char* section, const char* key, int value)
{
    char v[16]; _snprintf(v, sizeof(v), "%d", value); v[sizeof(v) - 1] = 0;
    ConfigWriteKey(section, key, v, "F10");
}
// The neck keys, the same ones SAVE AS DEFAULTS writes, after any neck change.
static void OvlSaveNeck()
{
    char v[32];
    ConfigWriteKey("Neck", "Mode", NeckModeName(g_neckMode), "F10 Comfort");
    _snprintf(v, sizeof(v), "%.3f", g_neckBelowM);        ConfigWriteKey("Neck", "PivotBelowM", v, "F10 Comfort");
    _snprintf(v, sizeof(v), "%.3f", g_neckBehindM);       ConfigWriteKey("Neck", "PivotBehindM", v, "F10 Comfort");
    _snprintf(v, sizeof(v), "%.3f", g_neckCrouchBelowM);  ConfigWriteKey("Neck", "CrouchPivotBelowM", v, "F10 Comfort");
    _snprintf(v, sizeof(v), "%.3f", g_neckCrouchBehindM); ConfigWriteKey("Neck", "CrouchPivotBehindM", v, "F10 Comfort");
    OvlSaveBool("Neck", "UprightPitchArc", dvr::camera::upright_pitch_arc());
}

// ---- the top of the panel, above the tabs --------------------------------------------------
static void OvlTopRow()
{
    // VR-197: the themed title, with the close button the removed title bar used to carry.
    {
        const float closeW = ImGui::GetFrameHeight();
        const ImVec2 start = ImGui::GetCursorPos();
        dvr::ovl::title("DISHONORED VR");
        const ImVec2 after = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(start.x + ImGui::GetContentRegionAvail().x - closeW, start.y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        if (dvr::ovl::button("X##ovlclose", ImVec2(closeW, closeW))) g_ovlVisible = false;
        ImGui::PopStyleColor();
        OvlTip("Close (F10, or click both sticks).");
        ImGui::SetCursorPos(after);
    }
    dvr::ovl::controller_hint();
    const bool resetPending = ConfigResetPending();
    if (resetPending) ImGui::TextWrapped("Defaults will be restored when you restart the game.");
    ImGui::BeginDisabled(resetPending);
    {   // the view level, as three pills
        const int lv = dvr::ovl::level();
        int want = lv;
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("SHOW");
        ImGui::SameLine();
        if (dvr::ovl::pill("Basic", lv == kOvlB)) want = kOvlB;
        OvlTip("Player settings: hands, reticle, controls, comfort, HUD and display.");
        ImGui::SameLine(0, 4);
        if (dvr::ovl::pill("Advanced", lv == kOvlA)) want = kOvlA;
        OvlTip("Adds preference detail most players never need: per-item options, HUD placement, "
               "neck pivot, effects.");
        ImGui::SameLine(0, 4);
        if (dvr::ovl::pill("Debug", lv == kOvlD)) want = kOvlD;
        OvlTip("Adds the fixes that should stay on, A/B switches, instruments and readouts. "
               "For troubleshooting and testing builds.");
        if (want != lv) {
            dvr::ovl::set_level(want);
            g_ovlDev = dvr::ovl::level() == kOvlD;
            ConfigWriteKey("Overlay", "Level", dvr::ovl::level_name(dvr::ovl::level()), "F10");
        }
        ImGui::SameLine();
        const char* ipd = "IPD 00 mm";
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ipd).x);
        ImGui::TextDisabled("IPD %.0f mm", g_ipdM * 1000.0f);
    }
    ImGui::Spacing();
    const float actionWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    dvr::ovl::push_primary();
    if (dvr::ovl::button("RECENTER", ImVec2(actionWidth, 0))) {
        g_posHaveRef = false;
        g_crouchRefOk = false;
        g_rtdReq = 3;
        Log("overlay: recentred (position reference)");
    }
    dvr::ovl::pop_primary();
    OvlTip("Makes where you are standing now the centre, and re-reads your standing height for "
           "real crouching. The same as F5.");
    ImGui::SameLine();
    if (dvr::ovl::button("SAVE AS DEFAULTS", ImVec2(actionWidth, 0)))
        OverlaySaveDefaults();
    OvlTip("Writes every current setting to dishonored_vr.ini. Every Basic and Advanced setting "
           "already saves the moment you change it; this also keeps the Debug switches, which "
           "otherwise last for this session only.");
    ImGui::SameLine();
    if (dvr::ovl::button("RESET TO DEFAULTS", ImVec2(actionWidth, 0))) ImGui::OpenPopup("Restore shipped defaults?");
    OvlTip("Restores the shipped settings on your next launch. Keeps your VR runtime and data folder.");
    static bool resetFailed = false;
    if (ImGui::BeginPopupModal("Restore shipped defaults?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Restore all mod settings on the next launch?");
        ImGui::TextUnformatted("Your current ini will be backed up. Runtime and data folder are kept.");
        if (dvr::ovl::button("Restore and restart later")) {
            resetFailed = !ConfigRequestReset();
            if (!resetFailed) ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (dvr::ovl::button("Cancel")) { resetFailed = false; ImGui::CloseCurrentPopup(); }
        if (resetFailed) ImGui::TextWrapped("Could not save the reset request. Check the log and try again.");
        ImGui::EndPopup();
    }
    dvr::ovl::slider_float("Height offset (m)", &g_heightOffsetM, -1.0f, 1.0f, "%+.2f");
    OvlTip("Raises or lowers your viewpoint.");
    OvlSaveOnRelease("Tracking", "HeightOffsetM", g_heightOffsetM, "%.3f");
    ImGui::EndDisabled();
    ImGui::Spacing();
}

// ---- Hands ----------------------------------------------------------------------------------
static void OvlTabHands()
{
    if (OvlSec("Hand size and position", kOvlB,
               "How big the hands are and where they sit on your controllers.")) {
        dvr::ovl::slider_float("Hand / weapon size", &g_mpModelScale, 0.4f, 1.6f, "%.2f");
        OvlTip("Scales the hands and whatever they hold, around your palm. Not the world scale. "
               "Saved when you let go.");
        OvlSaveOnRelease("Hands", "ModelScale", g_mpModelScale, "%.2f");
        MpTrimPanel();   // left, right and powers position steps
    }
    if (OvlSec("Sleeve", kOvlB, "How much of the arm is drawn with each hand.")) {
        const int cur = dvr::sleeve::match(g_msCutRel[1], g_msCutRel[2], g_msRoundDepth);
        if (ImGui::BeginCombo("Sleeve preset", cur >= 0 ? dvr::sleeve::kPresets[cur].name : "Custom")) {
            for (int i = 0; i < dvr::sleeve::kPresetCount; ++i)
                if (ImGui::Selectable(dvr::sleeve::kPresets[i].name, cur == i)) {
                    g_msRoundWrist = true;
                    ConfigWriteKey("Hands", "RoundedWrist", "1", "F10 Hands");
                    MsSleeveApply(dvr::sleeve::kPresets[i].cut, dvr::sleeve::kPresets[i].roundness, "F10 Sleeve preset");
                }
            ImGui::EndCombo();
        }
        OvlTip("Hands only, cuffs, or part of the forearm. Moving the length slider makes it Custom.");
        static float shownLen = 0.0f; static bool editing = false;
        if (!editing) shownLen = -g_msCutRel[1];
        dvr::ovl::slider_float("Sleeve length", &shownLen, 0.0f, 30.0f, "%.1f");
        editing = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) MsSleeveApply(-shownLen, g_msRoundDepth, "F10 Sleeve length");
        OvlTip("How far up the arm is kept. Applies when you let go of the slider (the cut is rebuilt).");
        if (OvlAt(kOvlA)) {
            bool wristChanged = dvr::ovl::checkbox("Rounded wrist ends", &g_msRoundWrist);
            wristChanged |= dvr::ovl::slider_float("Wrist roundness", &g_msRoundDepth, .05f, .8f, "%.2f");
            if (wristChanged) {
                g_msReclassReq = 1;
                ConfigWriteKey("Hands", "RoundedWrist", g_msRoundWrist ? "1" : "0", "F10 Hands");
                char amount[32]; _snprintf(amount, sizeof(amount), "%.3f", g_msRoundDepth);
                ConfigWriteKey("Hands", "RoundedWristDepth", amount, "F10 Hands");
            }
        }
    }
    if (OvlSec("Game arms during actions", kOvlA,
               "Which game actions show the game's own animated arms instead of your tracked hands, "
               "and which actions are allowed at all.")) {
        bool enabled = dvr::anim::enabled();
        if (dvr::ovl::checkbox("Use game arms for the selected actions", &enabled)) dvr::anim::set_enabled(enabled);
        OvlTip("Master switch. Off keeps your tracked hands for everything.");
        bool mantleHands = dvr::anim::mantle_enabled();
        if (dvr::ovl::checkbox("Game hands while mantling", &mantleHands)) {
            dvr::anim::set_mantle(mantleHands);
            ConfigWriteKey("Anim", "MantleHandBack", mantleHands ? "1" : "0", "F10 Hands");
        }
        bool cineHands = dvr::anim::cinematic_enabled();
        if (dvr::ovl::checkbox("Game hands and arms in cinematics", &cineHands)) {
            dvr::anim::set_cinematic(cineHands);
            ConfigWriteKey("Anim", "CinematicHandBack", cineHands ? "1" : "0", "F10 Hands");
        }
        bool melee = dvr::anim::hand_anim_melee();
        if (dvr::ovl::checkbox("Sword swing animation on the tracked hand", &melee)) dvr::anim::set_hand_anim_melee(melee);
        OvlTip("The hand plays the game's swing clip, then returns to the controller.");
        bool fire = dvr::anim::hand_anim_fire();
        if (dvr::ovl::checkbox("Shooting animation on the tracked hand (pistol, crossbow)", &fire)) dvr::anim::set_hand_anim_fire(fire);
        OvlTip("The hand plays the game's recoil clip, then returns to the controller.");
        if (!dvr::anim::action_gate_ready())
            ImGui::TextColored(ImVec4(1, .5f, .2f, 1), "Action gate unavailable: actions will not be blocked.");
        if (dvr::ovl::button("Reset arm choices")) dvr::anim::reset_arm_rules();
        ImGui::SameLine();
        if (dvr::ovl::button("Enable all actions")) for (int i = 0; i < dvr::anim::armRuleCount; ++i) dvr::anim::set_action_enabled(i, true);
        static ImGuiTextFilter filter; filter.Draw("Find action");
        const auto state = dvr::anim::snapshot();
        const char* groups[] = { "Whole body", "Upper body and weapon", "Left hand" };
        for (int lane = 0; lane < 3; ++lane) {
            if (!ImGui::TreeNode(groups[lane])) continue;
            for (int i = 0; i < dvr::anim::armRuleCount; ++i) {
                const auto& rule = dvr::anim::armRules[i];
                if (rule.lane != lane || (!filter.PassFilter(rule.label) && !filter.PassFilter(rule.state))) continue;
                ImGui::PushID(i); bool on = dvr::anim::arm_rule_enabled(i);
                ImGui::TextUnformatted(rule.label);
                if (dvr::anim::cancellable_action(i)) {
                    bool allowed = dvr::anim::action_enabled(i);
                    ImGui::BeginDisabled(!dvr::anim::action_gate_ready());
                    if (dvr::ovl::checkbox("Enable action", &allowed)) dvr::anim::set_action_enabled(i, allowed);
                    OvlTip("Off stops the game starting this action. One already underway finishes.");
                    ImGui::EndDisabled(); ImGui::SameLine();
                }
                if (dvr::ovl::checkbox("Show game arms", &on)) dvr::anim::set_arm_rule(i, on);
                OvlTip(rule.state);
                if (state.valid && !strcmp(state.state[lane], rule.state)) { ImGui::SameLine(); ImGui::TextDisabled("active"); }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
    }
    if (OvlSec("Hand fixes (debug)", kOvlD, "Fixes for hand and weapon placement. Leave them on.")) {
        dvr::ovl::checkbox("Correct weapon-specific lens", &g_waViewLens);
        OvlTip("Places weapons drawn with their own field of view correctly. Leave on.");
        bool mk = g_mkOn;
        if (dvr::ovl::checkbox("Keep weapon identity across a menu", &mk)) {
            g_mkOn = mk;
            Log("hands: AttachKeepOnMenu -> %d (%s)", mk ? 1 : 0,
                mk ? "a menu suspends, the resume validates" : "a menu drops everything, as before");
        }
        OvlTip("Off relearns the weapons after every menu, with about a second of weapon flicker. "
               "Leave on.");
        bool mn = g_mkNoteOn;
        if (dvr::ovl::checkbox("... and across a book or note", &mn)) {
            g_mkNoteOn = mn;
            Log("hands: AttachKeepOnNote -> %d", mn ? 1 : 0);
        }
        OvlTip("The same, for books and notes. Leave on.");
        bool uk = g_uiKeepOnMenu;
        if (dvr::ovl::checkbox("Skip the UI rescan when a menu kept them", &uk)) {
            g_uiKeepOnMenu = uk;
            Log("hands: UiKeepOnMenu -> %d (%s)", uk ? 1 : 0,
                uk ? "a kept menu queues no rescan" : "every menu queues the rescan");
        }
        OvlTip("Off adds about half a second of flat view on every resume. Leave on.");
        bool nc = g_nameIndexCacheOn;
        if (dvr::ovl::checkbox("Cache startup name lookups", &nc)) {
            g_nameIndexCacheOn = nc;
            Log("menu: CacheNameLookups -> %d (compare cold launches for startup timing)", nc ? 1 : 0);
        }
        OvlTip("Removes the long freeze after launch (VR-102). Leave on.");
        bool pc = g_pawnFromController;
        if (dvr::ovl::checkbox("Detect loaded player without crouching", &pc)) {
            g_pawnFromController = pc;
            g_cylOkMs = 0.0;
            Log("menu: PawnFromController -> %d", pc ? 1 : 0);
        }
        OvlTip("Finds the player right after a load, so stereo starts without a jump or crouch. Leave on.");
        bool nf = g_uiNoteFastMono;
        if (dvr::ovl::checkbox("Fast flat view for books and notes", &nf)) {
            g_uiNoteFastMono = nf;
            Log("menu: NoteFastMono -> %d (%s)", nf ? 1 : 0,
                nf ? "a note switches mono and back with its movie" : "a note waits on the load rules, as before");
        }
        OvlTip("Off takes about a second to switch around every note (VR-98). Leave on.");
        if (dvr::ovl::checkbox("Menu hand eye: recognise half-IPD steps", &g_mpEyeMenuHalfStep))
            Log("hands: menu signed half-step candidate=%d", (int)g_mpEyeMenuHalfStep);
        OvlTip("A test for hands in menus: uses the measured jump direction. Ships off.");
        bool pt = g_mpEyePredict;
        if (dvr::ovl::checkbox("Predict the eye when the jump is unreadable", &pt)) {
            g_mpEyePredict = pt;
            Log("hands: palette eye on an unreadable jump -> %s", pt ? "PREDICT the toggle" : "hold the previous eye");
        }
        OvlTip("VR-95 lever, ships off. Off holds the previous eye, which measured giving the left "
               "eye the right eye's hands during a head roll.");
        ImGui::TextDisabled("%ld eyes predicted | %ld weapon sets kept, %ld dropped", g_mpEyePredicted, g_mkKept, g_mkInvalidated);
        float viewRight = dvr::anim::view_right_cm();
        if (dvr::ovl::slider_float("Animation view left/right", &viewRight, -20.0f, 20.0f, "%.1f cm")) dvr::anim::set_view_right_cm(viewRight);
        OvlTip("Moves your viewpoint sideways during the game's animated camera moments only. 0 is unchanged.");
        if (dvr::ovl::button("Reset animation alignment")) dvr::anim::set_view_right_cm(0);
        const auto animState = dvr::anim::snapshot();
        ImGui::TextDisabled("Animation: %s | %s / %s / %s", animState.valid ? (animState.game ? "game owns body" : "player") : "unknown",
                            animState.state[0], animState.state[1], animState.state[2]);
    }
}

// ---- Aim ------------------------------------------------------------------------------------
static void OvlTabAim()
{
    if (OvlSec("Reticle", kOvlB, "The dot that shows where your hand is aiming.")) {
        if (dvr::aim::draw_reticle_ui()) {   // VR-141: saved at once
            const auto rc = dvr::aim::config(); char rv[32];
            ConfigWriteKey("Crosshair", "Dot", rc.dot ? "1" : "0", "F10 Aim");
            ConfigWriteKey("Crosshair", "Laser", rc.laser ? "1" : "0", "F10 Aim");
            ConfigWriteKey("Crosshair", "Hand", rc.hand ? "right" : "left", "F10 Aim");
            _snprintf(rv, sizeof(rv), "%.3f", rc.distanceM); ConfigWriteKey("Crosshair", "DistanceM", rv, "F10 Aim");
            _snprintf(rv, sizeof(rv), "%.3f", rc.sizeDeg); ConfigWriteKey("Crosshair", "SizeDeg", rv, "F10 Aim");
            _snprintf(rv, sizeof(rv), "%.2f", rc.otherXDeg); ConfigWriteKey("Crosshair", "OtherItemsX", rv, "F10 Aim");   // VR-189
            _snprintf(rv, sizeof(rv), "%.2f", rc.otherYDeg); ConfigWriteKey("Crosshair", "OtherItemsY", rv, "F10 Aim");
            const char* rk[3] = { "ColorR", "ColorG", "ColorB" };
            for (int i = 0; i < 3; ++i) { _snprintf(rv, sizeof(rv), "%d", rc.rgb[i]); ConfigWriteKey("Crosshair", rk[i], rv, "F10 Aim"); }
        }
        if (OvlAt(kOvlA)) {
            if (dvr::ovl::checkbox("Show the reticle while this panel is open", &g_ovlReticle)) {
                ConfigWriteKey("Overlay", "ReticleWhileOpen", g_ovlReticle ? "1" : "0", "F10 Aim");
                if (!g_ovlReticle) dvr::vr::set_aim_occluder(false);
            }
            OvlTip("Keeps the dot visible beside this panel so you can tune it. It hides behind the panel itself.");
        }
    }
    if (OvlSec("What aims with the controller", kOvlB,
               "For each thing you can aim: follow your controller, or your head like the flat game.")) {
        auto row = [](const char* label, bool hand, bool* changed, const char* tipText) {
            ImGui::PushID(label);
            ImGui::TextUnformatted(label); OvlTip(tipText); ImGui::SameLine(220);
            bool h = hand;
            if (dvr::ovl::radio_button("Head", !h)) h = false;
            OvlTip("Aims where you look, as in the flat game.");
            ImGui::SameLine();
            if (dvr::ovl::radio_button("Controller", h)) h = true;
            OvlTip("Aims where the reticle points.");
            ImGui::PopID();
            *changed = h != hand;
            return h;
        };
        bool changed = false;
        bool fire = row("Crossbow and pistol", FireAimEnabled(), &changed, "Where the bolt or bullet goes.");
        if (changed) { FireAimSet(fire, "F10"); ConfigWriteKey("Aim", "FireFromHand", fire ? "1" : "0", "F10 Aim"); }
        bool blink = row("Blink", g_blkDriveUI, &changed, "Where Blink takes you.");
        if (changed) {
            g_blkDriveUI = blink; g_blkAimOnCfg = blink;
            if (blink) g_blkDstReqUI = g_blkDstOnUI ? 0 : 1;
            ConfigWriteKey("Blink", "ControllerAim", blink ? "1" : "0", "F10 Aim");
            Log("blink: controller aim %s (F10 Aim)", blink ? "ON" : "off");
        }
        bool interact = row("Interactions", InteractAimEnabled(), &changed, "What you pick up, open or use.");
        if (changed) { InteractAimSet(interact, "F10"); ConfigWriteKey("Aim", "InteractFromHand", interact ? "1" : "0", "F10 Aim"); }
        bool thrown = row("Grenades", ThrowAimEnabled(), &changed, "Where grenades are thrown.");
        if (changed) { ThrowAimSet(thrown, "F10"); ConfigWriteKey("Aim", "ThrowFromHand", thrown ? "1" : "0", "F10 Aim"); }
        bool gadget = row("Spring razors", GadgetAimEnabled(), &changed, "Where spring razors are placed.");
        if (changed) { GadgetAimSet(gadget, "F10"); ConfigWriteKey("Aim", "GadgetFromHand", gadget ? "1" : "0", "F10 Aim"); }
        bool carry = row("Carried objects (throw)", CarryThrowAimEnabled(), &changed, "Where a carried bottle or object is thrown.");
        if (changed) { CarryThrowAimSet(carry, "F10"); ConfigWriteKey("Aim", "CarryThrowFromHand", carry ? "1" : "0", "F10 Aim"); }
        bool powers = row("Windblast, Possession, Swarm", PowerAimEnabled(), &changed, "Where these powers are aimed.");
        if (changed) { PowerAimSet(powers, "F10"); ConfigWriteKey("Aim", "PowersFromHand", powers ? "1" : "0", "F10 Aim"); }
        ImGui::TextDisabled("Not aimed by the mod: the sword (motion swing), carried bodies, the Heart.");
        if (g_gamepadOnly) ImGui::TextDisabled("[Mode] GamepadOnly=1: everything stays on the head.");
    }
    if (OvlSec("Held objects", kOvlB, "Where a carried bottle, rock or object sits in your hand.")) {
        ImGui::TextUnformatted("Position (steps follow your view; carry something to see it)");
        static int stepIx = 1;
        static const float kCm[3] = { 0.5f, 1.0f, 3.0f }, kDeg[3] = { 1.0f, 3.0f, 10.0f };
        dvr::ovl::radio_button("fine##chstep", &stepIx, 0); OvlTip("0.5 cm or 1 degree per press.");
        ImGui::SameLine();
        dvr::ovl::radio_button("normal##chstep", &stepIx, 1); OvlTip("1 cm or 3 degrees per press.");
        ImGui::SameLine();
        dvr::ovl::radio_button("coarse##chstep", &stepIx, 2); OvlTip("3 cm or 10 degrees per press.");
        struct Row { const char* name; const char* neg; const char* pos; bool rot; int axis; };
        static const Row kRows[6] = {
            { "move", "left", "right", false, 0 }, { "move", "down", "up", false, 2 },
            { "move", "back", "forward", false, 1 },
            { "turn", "pitch down", "pitch up", true, 1 }, { "turn", "yaw left", "yaw right", true, 0 },
            { "turn", "roll left", "roll right", true, 2 } };
        static const char* lastWhy = "";
        bool stepped = false;
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        for (int r = 0; r < 6; ++r)
            for (int sgn = 0; sgn < 2; ++sgn) {
                char lbl[48];
                _snprintf(lbl, sizeof(lbl), "%s %s##chv%d%d", kRows[r].name, sgn ? kRows[r].pos : kRows[r].neg, r, sgn);
                lbl[sizeof(lbl) - 1] = 0;
                if (sgn) ImGui::SameLine();
                if (dvr::ovl::button(lbl, ImVec2(ImGui::GetContentRegionAvail().x * (sgn ? 1.0f : 0.5f) - (sgn ? 0.0f : 4.0f), 0))) {
                    const float amt = (sgn ? 1.0f : -1.0f) * (kRows[r].rot ? kDeg[stepIx] : kCm[stepIx]);
                    if (CarryHoldViewStep(kRows[r].rot, kRows[r].axis, amt, &lastWhy)) stepped = true;
                }
                OvlTip("Moves or turns the held object the way the button says, as you see it now. "
                       "Hold to repeat. Saved at once.");
            }
        ImGui::PopItemFlag();
        if (stepped) CarryHoldSaveAdj("F10 Aim");
        if (lastWhy[0] && strcmp(lastWhy, "ok")) ImGui::TextDisabled("last step refused: %s", lastWhy);
        if (OvlAt(kOvlA)) {
            bool carryLt = CarryThrowLeftEnabled();
            if (dvr::ovl::checkbox("Throw carried objects with the left trigger", &carryLt)) {
                CarryThrowLeftSet(carryLt, "F10"); ConfigWriteKey("Aim", "CarryThrowLeftTrigger", carryLt ? "1" : "0", "F10 Aim");
            }
            OvlTip("Off uses the game's layout (the right trigger throws).");
            bool carryHold = CarryHoldEnabled();
            if (dvr::ovl::checkbox("Hold carried objects in the hand", &carryHold)) {
                CarryHoldSet(carryHold, "F10"); ConfigWriteKey("Aim", "CarryHoldAtHand", carryHold ? "1" : "0", "F10 Aim");
            }
            OvlTip("Off leaves the object floating in front of your view, as in the flat game.");
            bool holdRot = CarryHoldRotateEnabled();
            if (dvr::ovl::checkbox("Held object turns with the hand", &holdRot)) {
                CarryHoldSetRotate(holdRot); ConfigWriteKey("Aim", "CarryHoldRotate", holdRot ? "1" : "0", "F10 Aim");
            }
            bool holdKeep = CarryHoldKeepAngle();
            if (dvr::ovl::checkbox("Keep the angle it was picked up at", &holdKeep)) {
                CarryHoldSetKeepAngle(holdKeep); ConfigWriteKey("Aim", "CarryHoldKeepPickupAngle", holdKeep ? "1" : "0", "F10 Aim");
            }
            OvlTip("Off: every object sits the same way in the hand (then the turn steps above apply).");
            bool holdRet = CarryHoldReticleAnchored();
            if (dvr::ovl::checkbox("Held object ignores reticle changes", &holdRet)) {
                CarryHoldSetReticleAnchored(holdRet); ConfigWriteKey("Aim", "CarryHoldReticleAnchor", holdRet ? "1" : "0", "F10 Aim");
            }
            const auto rc = dvr::aim::config();
            ImGui::TextDisabled("tuned at reticle %+.1f / %+.1f deg; reticle now %+.1f / %+.1f",
                                CarryHoldReticleRef(0), CarryHoldReticleRef(1), rc.otherXDeg, rc.otherYDeg);
            if (dvr::ovl::button("Retune the hold against the current reticle")) {
                CarryHoldSetReticleRef(rc.otherXDeg, rc.otherYDeg);
                char b[16];
                _snprintf(b, sizeof(b), "%.2f", rc.otherXDeg); b[15] = 0; ConfigWriteKey("Aim", "CarryHoldReticleX", b, "F10 Aim");
                _snprintf(b, sizeof(b), "%.2f", rc.otherYDeg); b[15] = 0; ConfigWriteKey("Aim", "CarryHoldReticleY", b, "F10 Aim");
            }
            OvlTip("Moves the held object onto the current reticle's line; the steps above then tune from there.");
        }
        if (OvlAt(kOvlD)) {
            bool holdAnchor = CarryHoldGameAnchor();
            if (dvr::ovl::checkbox("Anchor the held object on the game camera", &holdAnchor)) {
                CarryHoldSetGameAnchor(holdAnchor); ConfigWriteKey("Aim", "CarryHoldAnchor", holdAnchor ? "1" : "0", "F10 Aim");
            }
            OvlTip("Fix for the held object jumping sideways (VR-181). Leave on.");
            bool holdDepth = CarryHoldWorldDepthEnabled();
            if (dvr::ovl::checkbox("Held object drawn in the world (not the weapon layer)", &holdDepth)) {
                CarryHoldSetWorldDepth(holdDepth); ConfigWriteKey("Aim", "CarryHoldWorldDepth", holdDepth ? "1" : "0", "F10 Aim");
            }
            OvlTip("Draws the object with the world so it is hidden behind walls. Leave on.");
            if (ImGui::TreeNode("Stored values (hand frame)##chraw")) {
                static const char* const holdLbl[6] = { "Held: forward (cm)", "Held: right (cm)", "Held: up (cm)",
                                                        "Held: pitch (deg)", "Held: yaw (deg)", "Held: roll (deg)" };
                static const float holdMin[6] = { -40, -40, -40, -180, -180, -180 }, holdMax[6] = { 60, 40, 40, 180, 180, 180 };
                for (int i = 0; i < 6; ++i) {
                    float v = CarryHoldAdj(i);
                    if (dvr::ovl::slider_float(holdLbl[i], &v, holdMin[i], holdMax[i], "%.1f")) CarryHoldSetAdj(i, v);
                    OvlTip("The raw offset in the hand's own frame ([Aim] CarryHold*). The steps above edit these.");
                    if (ImGui::IsItemDeactivatedAfterEdit()) CarryHoldSaveAdj("F10 Aim");
                }
                ImGui::TreePop();
            }
        }
    }
    if (OvlSec("Aim ray and Blink (debug)", kOvlD, "Where the aim ray comes from, and Blink's reach.")) {
        dvr::aim::draw_ui();
        bool ur = g_blkUseAimRay;
        if (dvr::ovl::checkbox("Blink uses the same ray as the reticle and shots", &ur)) {
            g_blkUseAimRay = ur;
            Log("blink: ray -> %s", ur ? "published" : "legacy MotionAim");
        }
        OvlTip("Off falls back to the old motion-aim ray. Leave on.");
        ImGui::TextDisabled(g_blkDirOn ? "Blink source seam live: the engine traces along %s"
                                       : "Blink arming when the power loads... (%s)",
                            g_blkUseAimRay ? "the published ray" : "the legacy MotionAim ray");
        ImGui::TextDisabled("last ray: %s | controller %.0f uu from the camera", g_blkRayWhy, g_blkRayGapUU);
        if (dvr::ovl::button("Capture a frame with the Blink marker (then hold an aim)")) {
            g_blkAutoDump = 1;
            Log("blink: armed - the next Blink aim triggers the frame dump");
        }
        OvlTip("The next Blink aim writes a frame dump next to the game, for a bug report.");
        int rm = g_blkReachMode;
        dvr::ovl::radio_button("Blink reach: head distance (original)", &rm, 0);
        OvlTip("Blink goes as far as the game's own aim would.");
        dvr::ovl::radio_button("Blink reach: fixed", &rm, 1);
        OvlTip("Always the max reach below.");
        dvr::ovl::radio_button("Blink reach: hand pitch sets the distance", &rm, 2);
        OvlTip("Point down for a short hop, level for the max.");
        if (rm != g_blkReachMode) {
            g_blkReachMode = rm;
            Log("blink: reach mode -> %d (0=head 1=fixed 2=hand pitch)", rm);
        }
        dvr::ovl::slider_float("Blink max reach (uu, 0 = auto)", &g_blkReachUU, 0.0f, 2000.0f, "%.0f");
        OvlTip("0 learns the power's own maximum as you use it.");
        ImGui::TextDisabled("auto-learned max so far: %.0f uu", g_blkReachSeen);
        if (g_blkReachMode == 2) {
            dvr::ovl::slider_float("Near reach (uu)", &g_blkNearUU, 50.0f, 900.0f, "%.0f");
            OvlTip("The hop length when pointing at the near pitch.");
            dvr::ovl::slider_float("Pitch for near (deg)", &g_blkPitchNear, -89.0f, 0.0f, "%.0f");
            OvlTip("Hand pitch that gives the near reach.");
            dvr::ovl::slider_float("Pitch for far (deg)", &g_blkPitchFar, -60.0f, 45.0f, "%.0f");
            OvlTip("Hand pitch that gives the max reach.");
            ImGui::TextDisabled("hand pitch now %+.0f deg -> %.0f uu", g_blkPitchNow, g_blkAimDistUU);
        }
    }
}

// ---- Controls -------------------------------------------------------------------------------
static void OvlTabControls()
{
    if (OvlSec("Controller layout", kOvlB, "D-pad shortcuts, the pause chord and what each button does.")) {
        auto c = dvr::controller::config();
        const char* mods[] = { "Off", "Right thumbrest", "R3 (right stick click)", "Left thumbrest" };
        const int modes[] = { dvr::controller::Off, dvr::controller::RightRest, dvr::controller::R3, dvr::controller::LeftRest };
        int selected = c.modifier == dvr::controller::LeftRest ? 3 : c.modifier;
        bool changed = ImGui::Combo("D-pad modifier", &selected, mods, 4);
        OvlTip("Hold this, then use a stick as the D-pad (item shortcuts). That stick stops moving or "
               "turning while you hold it. No thumbrest on your controller? Choose R3; its "
               "health-elixir hold is then unavailable.");
        if (changed) c.modifier = modes[selected];
        if (dvr::ovl::checkbox("Flip D-pad to right stick", &c.flip)) {
            if (c.modifier == dvr::controller::RightRest || c.modifier == dvr::controller::LeftRest)
                c.modifier = c.flip ? dvr::controller::LeftRest : dvr::controller::RightRest;
            changed = true;
        }
        OvlTip("The D-pad uses the right stick instead of the left. A thumbrest modifier moves to the other hand.");
        changed |= dvr::ovl::checkbox("X + Y pause chord", &c.pauseChord);
        OvlTip("Pressing X and Y together acts as the menu button, for runtimes that reserve it. "
               "Modifier + X + Y opens the journal.");
        if (changed) {
            c = dvr::controller::normalize(c);
            dvr::controller::configure(c);
            char value[16]; _snprintf(value, sizeof(value), "%d", c.modifier);
            ConfigWriteKey("Controllers", "DpadModifier", value, "F10 Controls");
            ConfigWriteKey("Controllers", "DpadFlip", c.flip ? "1" : "0", "F10 Controls");
            ConfigWriteKey("Controllers", "PauseChord", c.pauseChord ? "1" : "0", "F10 Controls");
        }
        ImGui::TextDisabled("Menu: tap to pause, hold (or modifier + menu) for the journal.");
        OvlTip("Hold Y and use the right stick to lean. Both grips keep their normal actions. "
               "Clicking both sticks recenters.");
    }
    dvr::swing::draw_ui();   // VR-37: the motion sword (its own section)
}

// ---- Comfort --------------------------------------------------------------------------------
static void OvlTabComfort()
{
    if (OvlSec("Field of view", kOvlB, "How much of the game world the view covers.")) {
        float projectionFov = ProjectionFovGet();
        bool customFov = projectionFov > 0;
        if (dvr::ovl::checkbox("Custom gameplay FOV", &customFov)) {
            projectionFov = customFov ? 103.0f : 0.0f;
            ProjectionFovSet(projectionFov);
            char fv[16]; _snprintf(fv, sizeof(fv), "%.2f", ProjectionFovGet()); ConfigWriteKey("Screen", "ProjectionFov", fv, "F10 Comfort");
        }
        OvlTip("Off uses the automatic FOV. Saved at once.");
        ImGui::BeginDisabled(!customFov);
        float degrees = customFov ? projectionFov : 103.0f;
        if (dvr::ovl::slider_float("Gameplay FOV (degrees)", &degrees, 60.0f, 120.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp))
            ProjectionFovSet(degrees);
        OvlTip("Lower is sharper with a smaller view. Higher covers more. Saved when you let go.");
        OvlSaveOnRelease("Screen", "ProjectionFov", ProjectionFovGet(), "%.2f");
        if (ImGui::SmallButton("Reset FOV to 103")) { ProjectionFovSet(103.0f); ConfigWriteKey("Screen", "ProjectionFov", "103.00", "F10 Comfort"); }
        ImGui::EndDisabled();
    }
    if (OvlSec("Crouch", kOvlB, "Crouching for real crouches Corvo.")) {
        bool pc = g_crouchOn;
        if (dvr::ovl::checkbox("Real crouching crouches Corvo", &pc)) {
            g_crouchOn = pc;
            ConfigWriteKey("Tracking", "PhysicalCrouch", pc ? "1" : "0", "F10 Comfort");
        }
        OvlTip("Duck in the room and Corvo sneaks. The crouch button still works.");
        ImGui::SameLine();
        ImGui::TextDisabled(g_crouchHeld ? "[DOWN]" : "");
        dvr::ovl::slider_float("Crouch height (m below standing)", &g_crouchDropM, 0.08f, 0.70f, "%.2f");
        OvlTip("How far below your standing height counts as crouching. RECENTER re-reads your standing height.");
        OvlSaveOnRelease("Tracking", "CrouchDropM", g_crouchDropM, "%.2f");
    }
    CamShakeDrawUi();   // VR-172: the game's own camera shake (its own section)
    if (OvlSec("World scale and tracking", kOvlB, "How big the world feels, and how your head moves in it.")) {
        dvr::ovl::slider_float("World scale (uu per metre)", &g_posScaleUU, 10.0f, 200.0f, "%.0f");
        OvlTip("Smaller makes the world feel bigger. Changes how far one real metre moves you. "
               "Saved when you let go.");
        OvlSaveOnRelease("PosTrack", "Scale", g_posScaleUU, "%.1f");
        if (OvlAt(kOvlA)) {
            bool pt = g_posTrack;
            if (dvr::ovl::checkbox("Positional head tracking", &pt)) {
                g_posTrack = pt;
                if (!pt) { g_leanRightUU = 0; g_leanUpUU = 0; g_leanFwdUU = 0; }
                ConfigWriteKey("PosTrack", "Enabled", pt ? "1" : "0", "F10 Comfort");
            }
            OvlTip("Leaning and moving your head moves the view. Off keeps rotation only.");
            bool headMovement = HeadMovementEnabled();
            if (dvr::ovl::checkbox("Head-based movement", &headMovement)) { HeadMovementSet(headMovement); OvlSaveBool("Camera", "HeadBasedMovement", headMovement); }
            OvlTip("The stick walks where you look. Off: it walks where your body faces.");
        }
    }
    if (OvlSec("Neck pivot", kOvlA,
               "Where your view rotates from when you look up and down. Look up and down at "
               "something an arm's length away: with the right setting it stays put.")) {
        bool uprightArc = dvr::camera::upright_pitch_arc();
        if (dvr::ovl::checkbox("Upright position at steep pitch", &uprightArc)) { dvr::camera::set_upright_pitch_arc(uprightArc); OvlSaveNeck(); }
        OvlTip("Keeps your head position upright when looking far up or down.");
        if (dvr::ovl::button("off")) { NeckSet(0, g_neckBelowM, g_neckBehindM, "F10 Comfort"); OvlSaveNeck(); }
        OvlTip("No neck: the view rotates around your eyes.");
        ImGui::SameLine();
        if (dvr::ovl::button("add")) { NeckSet(1, g_neckBelowM, g_neckBehindM, "F10 Comfort"); OvlSaveNeck(); }
        OvlTip("Adds a neck: the view swings around a point below and behind your eyes.");
        ImGui::SameLine();
        if (dvr::ovl::button("cancel")) { NeckSet(2, g_neckBelowM, g_neckBehindM, "F10 Comfort"); OvlSaveNeck(); }
        OvlTip("Cancels the game's own neck swing, so your real neck is the only one.");
        float below = g_neckBelowM, behind = g_neckBehindM;
        dvr::ovl::slider_float("Pivot below eyes (m)", &below, 0.0f, 0.30f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { NeckSet(g_neckMode, below, g_neckBehindM, "F10 Comfort slider"); OvlSaveNeck(); }
        else g_neckBelowM = below;
        dvr::ovl::slider_float("Pivot behind eyes (m)", &behind, 0.0f, 0.30f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { NeckSet(g_neckMode, g_neckBelowM, behind, "F10 Comfort slider"); OvlSaveNeck(); }
        else g_neckBehindM = behind;
        if (dvr::ovl::button("crouched: same as standing")) { NeckCrouchSet(-1.0f, -1.0f, "F10 Comfort"); OvlSaveNeck(); }
        ImGui::SameLine();
        if (dvr::ovl::button("crouched: no arc")) { NeckCrouchSet(0.0f, 0.0f, "F10 Comfort"); OvlSaveNeck(); }
        OvlTip("Crouched, the game has no neck swing of its own (measured), so none is added.");
        float cb = g_neckCrouchBelowM < 0.0f ? g_neckBelowM : g_neckCrouchBelowM;
        float cf = g_neckCrouchBehindM < 0.0f ? g_neckBehindM : g_neckCrouchBehindM;
        dvr::ovl::slider_float("Crouched pivot below (m)", &cb, 0.0f, 0.40f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { NeckCrouchSet(cb, cf, "F10 Comfort slider"); OvlSaveNeck(); }
        dvr::ovl::slider_float("Crouched pivot behind (m)", &cf, 0.0f, 0.30f, "%.3f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { NeckCrouchSet(cb, cf, "F10 Comfort slider"); OvlSaveNeck(); }
        if (OvlAt(kOvlD)) {
            ImGui::TextDisabled("now: %s | arc R%+.1f U%+.1f F%+.1f uu at pitch %+.0f deg", NeckModeName(g_neckMode),
                                g_neckArcUu[0], g_neckArcUu[1], g_neckArcUu[2], g_hmdPitch * 57.29578f);
            ImGui::TextDisabled("crouched pivot: %s | %s, pivot in use %.3f / %.3f m",
                                g_neckCrouchBelowM < 0.0f && g_neckCrouchBehindM < 0.0f ? "same as standing" : "its own",
                                g_neckStanceTarget ? "CROUCHED" : "standing", g_neckEffBelowM, g_neckEffBehindM);
        }
    }
    if (OvlSec("Cinematics", kOvlA, "How scripted scenes and special cameras behave.")) {
        bool pitchLock = CinePitchEnabled();
        if (dvr::ovl::checkbox("Suppress animation up/down tilt", &pitchLock)) { CinePitchSet(pitchLock); OvlSaveBool("Cine", "LockPitch", pitchLock); }
        bool rollLock = CineRollEnabled();
        if (dvr::ovl::checkbox("Suppress cinematic roll", &rollLock)) { CineRollSet(rollLock); OvlSaveBool("Cine", "LockRoll", rollLock); }
        bool fov = CineFovEnabled();
        if (dvr::ovl::checkbox("Suppress cinematic FOV zoom", &fov)) { CineFovSet(fov); OvlSaveBool("Cine", "LockFov", fov); }
        bool specialHead = SpecialHeadEnabled();
        if (dvr::ovl::checkbox("Natural head look in lean / keyholes", &specialHead)) { SpecialHeadSet(specialHead); OvlSaveBool("Cine", "SpecialHeadLook", specialHead); }
        OvlTip("Keeps the game's peek position, and lets your head look around past the game's limits.");
        bool borders = CineBordersEnabled();
        if (dvr::ovl::checkbox("Hide cinematic black borders", &borders)) { CineBordersSet(borders); OvlSaveBool("Cine", "HideBorders", borders); }
        OvlTip("Removes the letterbox bars in scenes. The picture under them is really there.");
    }
    if (OvlSec("Rain", kOvlB, "Remove the close rain overlay while keeping world rain.")) {
        bool rain = RainHideEnabled();
        if (dvr::ovl::checkbox("Hide close rain overlay", &rain)) { RainHideSet(rain); OvlSaveBool("Rain", "Hide", rain); }
        OvlTip("Hides the close rain lens overlay. Keeps the sky rain box and ground-impact system.");
    }
    if (OvlSec("Lens effects and rain distance", kOvlA, "Placement of rain and camera lens effects.")) {
        int rainDist = RainDistance();
        if (dvr::ovl::slider_int("Rain distance (uu, -1 native)", &rainDist, -1, 600)) RainDistanceSet(rainDist);
        if (ImGui::IsItemDeactivatedAfterEdit()) OvlSaveInt("Rain", "Distance", rainDist);
        OvlTip("How far in front of you the camera rain is drawn. -1 is the game's own.");
        int lensDist = LensDistance();
        if (dvr::ovl::slider_int("Lens effects distance (uu, 0 native)", &lensDist, 0, 90)) LensDistanceSet(lensDist);
        if (ImGui::IsItemDeactivatedAfterEdit()) OvlSaveInt("Lens", "Distance", lensDist);
        OvlTip("Pushes lens effects (blood, water) out from your eyes. 0 is the game's own.");
        bool keep = LensKeepSize();
        if (dvr::ovl::checkbox("Lens effects keep their size", &keep)) { LensKeepSizeSet(keep); OvlSaveBool("Lens", "KeepSize", keep); }
        OvlTip("Pushed effects are scaled up so they cover the same view.");
        bool follow = LensFollowHead();
        if (dvr::ovl::checkbox("Lens effects follow the head (per eye)", &follow)) { LensFollowSet(follow); OvlSaveBool("Lens", "FollowHead", follow); }
        OvlTip("Places lens effects for each eye, so they do not double.");
        int rainPct = LensRainPct();
        if (dvr::ovl::slider_int("Rain lens strength (%, 100 native)", &rainPct, 0, 100)) LensRainPctSet(rainPct);
        if (ImGui::IsItemDeactivatedAfterEdit()) OvlSaveInt("Lens", "RainStrength", rainPct);
    }
    if (OvlSec("Comfort fixes (debug)", kOvlD, "Fixes and levers for the view. Leave them as they are.")) {
        bool on = CineHeadEnabled();
        if (dvr::ovl::checkbox("Cinematic head look", &on)) CineHeadSet(on);
        OvlTip("Lets your head look around during cinematics. Leave on.");
        bool surface = UiSurfaceEnabled();
        if (dvr::ovl::checkbox("Guard menu/loading stereo and input", &surface)) UiSurfaceSet(surface);
        OvlTip("Detects menus and loads so stereo and the sticks switch correctly. Leave on.");
        bool sceneState = StereoStateEnabled();
        if (dvr::ovl::checkbox("Stereo in cinematic and dialogue states", &sceneState)) StereoStateSet(sceneState);
        OvlTip("Keeps 3D in scenes and conversations. Leave on.");
        bool possession = PossessionStereoEnabled();
        if (dvr::ovl::checkbox("Stereo while possessing", &possession)) PossessionStereoSet(possession);
        OvlTip("Keeps 3D during Possession. Leave on.");
        bool imagePose = dvr::vr::image_orientation_enabled();
        if (dvr::ovl::checkbox("Image-linked head orientation", &imagePose)) dvr::vr::set_image_orientation(imagePose);
        OvlTip("Ties the headset image to the head pose it was rendered with. Leave on.");
        bool mirror = WmEnabled();
        if (dvr::ovl::checkbox("Mirror pistol/crossbow (fill the missing side)", &mirror)) WmSet(mirror);
        OvlTip("The game models only one side of these weapons; this fills the other. Leave on.");
        ImGui::SameLine();
        if (dvr::ovl::button("Rebuild mirror")) WmReleaseAll("rebuild from F10");
        float g = g_afCounterYaw < 0.0f ? 0.0f : g_afCounterYaw;
        if (dvr::ovl::slider_float("Arm counter-yaw", &g, 0.0f, 2.0f, "%.2f")) ArmFollowSetCounterYaw(g, "F10");
        OvlTip("VR-30 lever: how much the arms are turned back against head yaw.");
        ImGui::SameLine();
        if (ImGui::SmallButton(g_afCounterYaw >= 0.0f ? "off" : "on"))
            ArmFollowSetCounterYaw(g_afCounterYaw >= 0.0f ? -1.0f : 1.0f, "F10");
        ImGui::BeginDisabled(HeadMovementEnabled());
        bool fon = g_frWant >= 0.0f;
        if (dvr::ovl::checkbox("Arms: head yaw does not turn the body", &fon)) ArmFollowSetFacing(fon ? 1.0f : -1.0f, "F10");
        OvlTip("Turning your head does not turn Corvo's body; the stick still does. Unavailable "
               "with head-based movement.");
        ImGui::EndDisabled();
    }
}

// ---- HUD ------------------------------------------------------------------------------------
static void OvlTabHud()
{
    bool hp = dvr::hudcap::enabled();
    if (dvr::ovl::checkbox("HUD on its own panels", &hp)) {
        dvr::hudcap::set_enabled(hp);
        ConfigWriteKey("Hud", "Panel", dvr::hudcap::enabled() ? "1" : "0", "F10 HUD");
    }
    OvlTip("The HUD floats on panels you can place (below). Off draws it into the game image like "
           "the flat game.");
    if (OvlAt(kOvlD)) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", dvr::hudcap::armed() ? "armed" : dvr::hudcap::enabled() ? "on but idle" : "off");
    }
    dvr::hudlayout::draw_ui();   // every HUD section, each with its tier
    if (OvlSec("HUD routing (debug)", kOvlD, "How HUD draws are recognised and sent to panels.")) {
        bool rg = dvr::hudclass::regions_enabled();
        if (dvr::ovl::checkbox("Route elements by screen region", &rg)) {
            dvr::hudclass::set_regions_enabled(rg);
            ConfigWriteKey("Hud", "Regions", rg ? "1" : "0", "F10 HUD");
        }
        OvlTip("Off sends the whole HUD to one panel ('all'). Leave on.");
        bool dc = dvr::hudclass::census_enabled();
        if (dvr::ovl::checkbox("Draw census (a table every 3 s)", &dc)) {
            dvr::hudclass::set_census_enabled(dc);
            ConfigWriteKey("Draws", "Census", dvr::hudclass::census_enabled() ? "1" : "0", "F10 HUD");
        }
        OvlTip("Logs which draws went where. Costs a lookup per draw.");
        const dvr::vr::HudQuadStats hq = dvr::vr::hud_quad_stats();
        ImGui::TextDisabled("quads last present %u; hand untracked %u; layer budget hits %u",
                            hq.submitted, hq.untracked, hq.hiddenBudget);
    }
}

// ---- Display --------------------------------------------------------------------------------
static void OvlTabDisplay()
{
    const int resizeState = ResLiveState();
    const bool busy = resizeState == 1 || resizeState == 2 || resizeState == 4;
    if (OvlSec("Resolution", kOvlB, "How many pixels the game renders for the headset.")) {
        static float pixelPercent = -1.0f;
        if (pixelPercent < 0)
            pixelPercent = g_resWantW && g_resWantH
                ? 100.0f * ((float)g_resWantW / 2750.0f) * ((float)g_resWantH / 2850.0f) : 120.0f;
        dvr::ovl::slider_float("Total pixels (%)", &pixelPercent, 50.0f, 200.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp);
        OvlTip("100% = 2750x2850. Higher is sharper and slower. Both axes scale together. Press Set to apply.");
        const float axisScale = sqrtf(pixelPercent * 0.01f);
        const uint32_t width = (uint32_t)(2750.0f * axisScale + 0.5f);
        const uint32_t height = (uint32_t)(2850.0f * axisScale + 0.5f);
        ImGui::Text("Preview: %ux%u | now %ux%u", width, height, dvr::capture::width(), dvr::capture::height());
        ImGui::BeginDisabled(busy);
        if (dvr::ovl::button("Set resolution")) ResLiveQueue(width, height);
        OvlTip("Applies now and saves. Expect a brief pause while the game resizes.");
        ImGui::EndDisabled();
        ImGui::TextDisabled("%s", ResLiveStatus());
    }
    if (OvlSec("Desktop window", kOvlB, "What the game window on your monitor shows.")) {
        bool mirrorOff = dvr::desktop_eye::mirror_off();
        if (dvr::ovl::checkbox("Disable desktop mirror", &mirrorOff)) {
            dvr::desktop_eye::set_mirror_off(mirrorOff);
            ConfigWriteKey("VR", "DesktopMirrorOff", mirrorOff ? "1" : "0", "F10 Display");
        }
        OvlTip("Freezes the monitor image while VR runs, which saves a little work. The headset keeps rendering.");
        if (OvlAt(kOvlA)) {
            bool strictOff = dvr::desktop_eye::strict_off();
            ImGui::BeginDisabled(!mirrorOff);
            if (dvr::ovl::checkbox("Keep desktop frozen across VR frame gaps", &strictOff)) {
                dvr::desktop_eye::set_strict_off(strictOff);
                ConfigWriteKey("VR", "DesktopMirrorStrictOff", strictOff ? "1" : "0", "F10 Display");
            }
            OvlTip("With the mirror off, also stay frozen when the headset briefly stops drawing.");
            ImGui::EndDisabled();
            ImGui::BeginDisabled(mirrorOff);
            bool reduced = dvr::desktop_eye::reduced_present();
            if (dvr::ovl::checkbox("Reduce desktop presentation", &reduced)) {
                dvr::desktop_eye::set_reduced_present(reduced);
                ConfigWriteKey("VR", "ReduceDesktopPresent", reduced ? "1" : "0", "F10 Display");
            }
            OvlTip("Updates the monitor less often when it is safe; the headset keeps both eyes.");
            ImGui::EndDisabled();
        }
    }
    if (OvlSec("Fullscreen and vsync", kOvlA, "The game window's mode. Each switch briefly pauses the game.")) {
        ImGui::BeginDisabled(busy);
        bool liveFull = ResLiveFullscreen();
        if (dvr::ovl::checkbox("Fullscreen", &liveFull)) {
            ResLiveSetFullscreen(liveFull, "F10 Display");
            ConfigWriteKey("Screen", "RenderFullscreen", liveFull ? "1" : "0", "F10 Display");
        }
        OvlTip("Applies now. With VirtualMode on (the default) the device stays windowed whatever this asks.");
        bool vsyncOn = !g_forceNoVSync;
        if (dvr::ovl::checkbox("Vsync", &vsyncOn)) {
            ResLiveSetVsync(vsyncOn, "F10 Display");
            ConfigWriteKey("Perf", "ForceNoVSync", vsyncOn ? "0" : "1", "F10 Display");
        }
        OvlTip("Caps the monitor to its refresh. The headset paces itself either way.");
        ImGui::EndDisabled();
        ImGui::TextDisabled("Asked: %s. Device: %s. Present: %s.",
                            ResLiveWantFullscreen() ? "fullscreen" : "windowed",
                            ResLiveDeviceWindowed() ? "windowed" : "fullscreen",
                            g_vsyncWant == 1 ? "vsynced (forced)" : (g_vsyncWant == 0 ? "uncapped (forced)" : "the game's own choice"));
    }
    if (OvlSec("Flat screen (menus and loading)", kOvlA, "The big screen shown for the main menu and loads.")) {
        if (dvr::ovl::slider_float("Screen distance (m)", &g_screenDist, 0.8f, 4.0f, "%.2f"))
            dvr::vr::set_screen(g_screenDist, g_screenWidth);
        OvlSaveOnRelease("Screen", "DistanceMeters", g_screenDist, "%.2f");
        if (dvr::ovl::slider_float("Screen width (m)", &g_screenWidth, 0.5f, 6.0f, "%.2f"))
            dvr::vr::set_screen(g_screenDist, g_screenWidth);
        OvlSaveOnRelease("Screen", "WidthMeters", g_screenWidth, "%.2f");
    }
    if (OvlSec("Stereo and capture (debug)", kOvlD, "How the game's frame becomes the headset image.")) {
        const bool monoWanted = !_stricmp(dvr::stereo::wanted_name(), "mono");
        bool armed = dvr::stereo::armed();
        ImGui::BeginDisabled(monoWanted);
        if (dvr::ovl::checkbox("Stereo armed", &armed)) dvr::stereo::set_armed(armed);
        OvlTip("Off parks the stereo method on the flat screen without forgetting it.");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextDisabled("%s: active %s, %s", dvr::stereo::wanted_name(), dvr::stereo::active_name(),
                            dvr::stereo::wants_projection() ? "projection layer" : "head-locked screen");
        if (monoWanted) { ImGui::SameLine(); if (dvr::ovl::button("select reentry")) dvr::stereo::choose("reentry"); OvlTip("Switch to the shipped stereo method."); }
        static const char* kModes[] = { "sync (readback, waits)", "deferred (readback, one present late)",
                                        "shared (VRAM, needs the D3D9Ex device)", "off (frozen image: the A/B control)" };
        int mode = (int)dvr::capture::mode();
        if (ImGui::Combo("Capture mode", &mode, kModes, 4))
            dvr::capture::set_mode(mode == 0 ? "sync" : mode == 1 ? "deferred" : mode == 2 ? "shared" : "off");
        OvlTip("How the frame is copied to the headset. Off freezes the image on purpose, as an A/B control.");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", !dvr::capture::probed() ? "probe pending"
                                  : dvr::capture::shared_available() ? "shared AVAILABLE" : "shared refused (plain device)");
        static int exAsk = -1;
        if (exAsk < 0) exAsk = dvr::d3d9ex::ex_wanted() ? 1 : 0;
        bool ex = exAsk != 0;
        if (dvr::ovl::checkbox("D3D9Ex device at the next launch", &ex)) { exAsk = ex ? 1 : 0; DeviceSetEx(ex, "F10 Display"); }
        OvlTip("Needed for the shared capture mode. Takes effect next launch.");
        ImGui::SameLine();
        ImGui::TextDisabled("this run: %s", dvr::d3d9ex::device_is_ex() ? "9Ex" : "plain D3D9");
        bool c5 = dvr::stereo::reentry_c5_pair();
        if (dvr::ovl::checkbox("c5 pairing", &c5)) {
            dvr::stereo::set_reentry_c5_pair(c5);
            ConfigWriteKey("Stereo", "C5Pair", c5 ? "1" : "0", "F10 Display");
        }
        OvlTip("Pairs each eye by its camera, not by order. Off is the old A/B. Leave on.");
        bool singleTag = dvr::stereo::reentry_single_tag();
        if (dvr::ovl::checkbox("Repair right-eye flicker after reload", &singleTag)) {
            dvr::stereo::set_reentry_single_tag(singleTag);
            ConfigWriteKey("Stereo", "SingleTagRepair", singleTag ? "1" : "0", "F10 Display");
        }
        OvlTip("Fix for a flickering right eye after loading a save. Leave on.");
        bool lateTag = dvr::stereo::reentry_late_tag();
        if (dvr::ovl::checkbox("Repair late eye tags after notes", &lateTag)) {
            dvr::stereo::set_reentry_late_tag(lateTag);
            ConfigWriteKey("Stereo", "LateTagRepair", lateTag ? "1" : "0", "F10 Display");
        }
        OvlTip("Fix for swapped eyes after closing a note (VR-80). Leave on.");
        bool fid = dvr::frameid::enabled();
        if (dvr::ovl::checkbox("Frame-identity trace", &fid)) {
            dvr::frameid::set_enabled(fid);
            ConfigWriteKey("Perf", "FrameId", fid ? "1" : "0", "F10 Display");
        }
        OvlTip("Samples one eye pair every 8 ticks into the log. Off costs nothing.");
        const dvr::frameid::Last fl = dvr::frameid::last();
        ImGui::TextDisabled("EYES: %s | pairs %u | L-R diff bb %.1f slot %.1f out %.1f sc %.1f (floor %.1f)",
                            fl.pairs == 0 ? "no pairs yet" : fl.onePicture ? "ONE PICTURE" : "two pictures",
                            fl.pairs, fl.diff[0], fl.diff[1], fl.diff[2], fl.diff[3], fl.floorBb);
        ImGui::TextDisabled("side %s | picture shift %+d px | swapped pairs %u", fl.side, fl.shift, fl.swapped);
        if (dvr::ovl::button("Dump eyes")) FrameDumpRequest("eyes");
        ImGui::SameLine();
        if (dvr::ovl::button("Rearm 2")) SceneDrawCommand("rearm 2");
        OvlTip("Two single ticks, capture untouched.");
        ImGui::SameLine();
        if (dvr::ovl::button("Capture reinit")) dvr::capture::request_reinit();
        if (dvr::ovl::button("Projection off (the quad)")) dvr::stereo::set_projection_override(0);
        ImGui::SameLine();
        if (dvr::ovl::button("Projection auto")) dvr::stereo::set_projection_override(-1);
    }
    if (OvlSec("Performance (debug)", kOvlD, "Timing instruments and benchmarks.")) {
        const dvr::perf::Window pw = dvr::perf::last_window();
        ImGui::TextDisabled("tick %.1f ms (%.1f/s) = in %.1f + out %.1f (idle %.1f R %.1f) | capture %.1f [lock %.1f] wait %.1f%s",
                            pw.tickMs, pw.ticksPerS, pw.inMs, pw.outMs, pw.idleMs, pw.rMs, pw.captureMs, pw.lockMs,
                            pw.waitMs, pw.paceBound ? "  PACE-BOUND" : "");
        ImGui::TextDisabled("gpu %s: span %.1f ms (dma %.1f) idle %.1f | marker %s | capture mode %s",
                            pw.gpu[0] ? pw.gpu : "-", pw.gpuSpanMs, pw.gpuDmaMs, pw.gpuIdleMs,
                            pw.marker[0] ? pw.marker : "-", dvr::capture::mode_name());
        if (dvr::ovl::button("MARK (stamp a freeze now)", ImVec2(-1, 0))) dvr::perf::mark("F10", "F10");
        OvlTip("Stamps the log with what just happened, so a freeze you saw can be found.");
        ImGui::TextDisabled("marks so far: %u", pw.marks);
        bool renderProfile = dvr::render_profile::enabled();
        if (dvr::ovl::checkbox("Sample render-thread CPU costs", &renderProfile)) {
            dvr::render_profile::set_enabled(renderProfile);
            ConfigWriteKey("Perf", "RenderProfile", renderProfile ? "1" : "0", "F10 Display");
        }
        OvlTip("Logs where the render thread spends its time.");
        bool nativeProfile = dvr::native_profile::enabled();
        if (dvr::ovl::checkbox("Native draw CPU timing", &nativeProfile)) dvr::native_profile::set_enabled(nativeProfile);
        OvlTip("Logs the game's own draw costs.");
        bool bridgeProfile = dvr::bridge_profile::enabled();
        if (dvr::ovl::checkbox("Bridge GPU timing", &bridgeProfile)) dvr::bridge_profile::set_enabled(bridgeProfile);
        OvlTip("Logs the GPU cost of copying frames to the headset.");
        bool diagnosticAb = dvr::diag_ab::enabled();
        if (dvr::ovl::checkbox("Diagnostic overhead A/B/A (about 110 s)", &diagnosticAb)) {
            if (diagnosticAb) dvr::perf::ab_command("off");
            dvr::diag_ab::set_enabled(diagnosticAb);
        }
        OvlTip("Measures what the mod's own logging costs. Collection now: see below.");
        ImGui::TextDisabled("Collection: %s", dvr::diag_ab::reduced() ? "reduced for test" : "normal");
        if (dvr::ovl::button(dvr::perf::desktop_ab_enabled() ? "Stop desktop benchmark" : "Start desktop benchmark"))
            dvr::perf::desktop_ab_set_enabled(!dvr::perf::desktop_ab_enabled());
        OvlTip("Measures the desktop mirror's cost: 100 s (120 s with pair pacing). Opening a menu aborts it.");
        bool reducedTrial = dvr::perf::desktop_ab_reduced();
        bool pacingTrial = dvr::perf::desktop_ab_pacing();
        ImGui::BeginDisabled(dvr::perf::desktop_ab_enabled());
        if (dvr::ovl::checkbox("Benchmark pair pacing (mirror stays off)", &pacingTrial)) dvr::perf::desktop_ab_set_pacing(pacingTrial);
        OvlTip("Compares unpaced / paced / unpaced instead.");
        ImGui::BeginDisabled(pacingTrial);
        if (dvr::ovl::checkbox("Benchmark Reduced instead of Off", &reducedTrial)) dvr::perf::desktop_ab_set_reduced(reducedTrial);
        OvlTip("The middle phase uses the reduced mirror instead of none.");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
}

// ---- Game options (Advanced) ----------------------------------------------------------------
static void OvlTabGameOptions()
{
    bool defaultsOn = GameOptsStartupEnabled();
    if (dvr::ovl::checkbox("Apply VR defaults to the game's options at startup", &defaultsOn)) {
        GameOptsSetStartup(defaultsOn);
        ConfigWriteKey("GameOptions", "DefaultsAtStartup", defaultsOn ? "1" : "0", "F10 Game options");
    }
    OvlTip("At every launch, sets the game's own options that suit VR: kill cam off, head bob off, "
           "chain climbing not camera-relative, crosshair off, aim assist off, graphics as tested, "
           "gamepad sensitivity 30. Off leaves the game's menu in charge. Takes effect next launch.");
    if (OvlAt(kOvlD)) {
        if (dvr::ovl::button("Read them into the log now")) GameOptsRequest("F10 Game options");
        OvlTip("Writes every game option, as the running game has it, to the log.");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", GameOptsAutoFired() ? "this session's automatic read has run" : "the automatic read has not run yet");
        bool autoOn = GameOptsAutoEnabled();
        if (dvr::ovl::checkbox("Read once automatically when gameplay starts", &autoOn)) {
            GameOptsSetAuto(autoOn, "F10 Game options");
            ConfigWriteKey("Diagnostics", "GameOptsOnStart", autoOn ? "1" : "0", "F10 Game options");
        }
        OvlTip("The same read, once per session, so a bug report carries it.");
        ImGui::TextDisabled("game window %ux%u", dvr::capture::width(), dvr::capture::height());
    }
}

// ---- Log (Debug) ----------------------------------------------------------------------------
static void OvlTabLog()
{
    static int  filterCat = -1;
    static int  minLevel = 2;
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
    ImGui::SameLine(); dvr::ovl::checkbox("follow", &autoScroll);
    ImGui::SameLine();
    if (dvr::ovl::button("copy tail")) {
        size_t n = dvr::log::ring_copy(lines, 512);
        static char clip[512 * 240];
        size_t k = 0;
        for (size_t i = (n > 60 ? n - 60 : 0); i < n && k + 240 < sizeof(clip); i++)
            k += (size_t)snprintf(clip + k, sizeof(clip) - k, "[%lu] [%s] %s\n",
                                  (unsigned long)lines[i].tick, dvr::log::cat_name((dvr::log::Cat)lines[i].cat), lines[i].text);
        ImGui::SetClipboardText(clip);
    }
    ImGui::SameLine();
    if (dvr::ovl::button("status.json")) dvr::status::write_now();
    OvlTip("Writes the status file for the test tools now.");
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
}

// The whole tab bar.
static void OvlTabBody(const char* id, void (*draw)())
{
    dvr::ovl::begin_body(id);
    draw();
    dvr::ovl::end_body();
}

static void OvlTabs()
{
    if (!ImGui::BeginTabBar("vrtabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) return;
    if (dvr::ovl::tab("Hands"))    { OvlTabBody("OvlTabHands-scroll", OvlTabHands);    ImGui::EndTabItem(); }
    if (dvr::ovl::tab("Aim"))      { OvlTabBody("OvlTabAim-scroll", OvlTabAim);      ImGui::EndTabItem(); }
    if (dvr::ovl::tab("Controls")) { OvlTabBody("OvlTabControls-scroll", OvlTabControls); ImGui::EndTabItem(); }
    if (dvr::ovl::tab("Comfort"))  { OvlTabBody("OvlTabComfort-scroll", OvlTabComfort);  ImGui::EndTabItem(); }
    if (dvr::ovl::tab("HUD"))      { OvlTabBody("OvlTabHud-scroll", OvlTabHud);      ImGui::EndTabItem(); }
    if (dvr::ovl::tab("Display"))  { OvlTabBody("OvlTabDisplay-scroll", OvlTabDisplay);  ImGui::EndTabItem(); }
    if (OvlAt(kOvlA) && dvr::ovl::tab("Game options")) { OvlTabBody("OvlTabGameOptions-scroll", OvlTabGameOptions); ImGui::EndTabItem(); }
    if (OvlAt(kOvlD) && dvr::ovl::tab("Runtime")) { OvlTabBody("Runtime-scroll", [] { dvr::vr::draw_debug_ui(); }); ImGui::EndTabItem(); }
    if (OvlAt(kOvlD) && dvr::ovl::tab("Log"))     { OvlTabBody("OvlTabLog-scroll", OvlTabLog); ImGui::EndTabItem(); }
    ImGui::EndTabBar();
}
