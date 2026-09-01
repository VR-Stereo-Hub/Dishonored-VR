// game/dishonored/game_state.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

static bool CineActive()
{
    if (!g_cineNow) return false;
    // 38.66: the MAIN MENU's 3D background fires the same cinematic toggle,
    // which parked the pad at the menu ("can't move my joystick in the main
    // menu"). No live pawn = no gameplay cutscene to protect - clear the
    // latch. This also self-heals toggle parity across menus and loads.
    if (!CylTruthLive()) {
        g_cineNow = false;
        Log("cine: latch cleared (no live pawn - menu/load, not a cutscene)");
        return false;
    }
    if (MaimNowMs() - g_cineOnMs > 480000.0) {   // missed off-toggle escape
        g_cineNow = false;
        Log("cine: latch expired after 8 min - inputs live again");
    }
    return g_cineNow;
}

static bool SprintBit(bool clickNow)
{
    if (!g_sprintHoldCfg) return clickNow;
    double now = MaimNowMs();
    if (clickNow != g_spWas) {
        g_spWas = clickNow;
        g_spPulseUntil = now + (double)g_sprintPulseMs;
        Log("sprint: %s -> toggle pulse (hold-to-sprint; a latch is now "
            "impossible)", clickNow ? "PRESS" : "RELEASE");
        g_spTold = true;
    }
    return now < g_spPulseUntil;
}

static WORD SlideAssist(WORD b, bool userB, float mx, float my)
{
    static double until = 0.0, phase0 = 0.0;
    static bool   bWas = false;
    double now = MaimNowMs();
    float mag = sqrtf(mx*mx + my*my);
    if (g_slideAssist && userB && !bWas && mag > 0.85f && my > 0.4f) {
        phase0 = now; until = now + 240.0;
        Log("slide-assist: B at full run -> sprint+crouch (slide)");
    }
    bWas = userB;
    if (now < until) {
        b |= XINPUT_GAMEPAD_LEFT_THUMB;                    // sprint held
        if (now - phase0 < 90.0) b &= ~(WORD)XINPUT_GAMEPAD_B; // engage sprint
        else b |= XINPUT_GAMEPAD_B;                        // then crouch=slide
    }
    return b;
}

static inline bool CylTruthLive() {
    return g_cylOkMs != 0.0 && (MaimNowMs() - g_cylOkMs) < 3000.0;
}


// ----------------------------------------------------------------------------
// Head tracking -> mouse injection
// ----------------------------------------------------------------------------
// 30.59: analog axis -> discrete menu steps (see the call site). Returns a
// full-scale pulse on a fresh push and on each repeat tick, neutral otherwise,
// so the game sees clean press/release edges instead of a wandering axis.
static SHORT MenuStep(SHORT v, int axis)
{
    static double next[2] = {0.0, 0.0};
    static int    last[2] = {0, 0};
    if (axis < 0 || axis > 1) return 0;
    int dir = (v > 11000) ? 1 : (v < -11000 ? -1 : 0);
    if (dir == 0) { last[axis] = 0; next[axis] = 0.0; return 0; }
    double now = MaimNowMs();
    if (dir != last[axis]) {                 // fresh push: step at once
        last[axis] = dir;
        next[axis] = now + 380.0;            // delay before auto-repeat
        return (SHORT)(dir * 32000);
    }
    if (now >= next[axis]) {                 // held: steady repeat
        next[axis] = now + 170.0;
        return (SHORT)(dir * 32000);
    }
    return 0;                                // between steps: neutral
}
