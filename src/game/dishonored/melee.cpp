// game/dishonored/melee.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// 36.6: the health-elixir long-press. Gameplay only - in menus A is
// confirm and holding it must never drink; a press that starts gated (or
// wanders into a menu/wheel mid-hold) is poisoned until released. Interact
// itself passes through untouched; the potion fires ON TOP at the hold
// threshold, once per press, with a left-hand haptic as the confirmation.
static void HealthElixirTick(bool held)
{
    static double t0 = 0.0;
    static bool   fired = false;
    if (!g_elixirOn) return;
    if (!held) { t0 = 0.0; fired = false; return; }
    if (g_menuOpen || g_inMenu || g_wheelHeld || !g_handMesh) {
        t0 = 0.0; fired = true;          // poison this press
        return;
    }
    double now = MaimNowMs();
    if (t0 == 0.0) { t0 = now; return; }
    if (!fired && now - t0 >= (double)g_elixirHoldMs) {
        fired = true;
        INPUT in[2]; memset(in, 0, sizeof(in));
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = (WORD)g_elixirVk;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = (WORD)g_elixirVk;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        MaimHaptic(0, 0.6f, 0.12f);      // left thump = potion went down
        Log("elixir: health potion (%.0f ms held)", now - t0);
    }
}


// Runs at submit rate (~90 Hz) on the render thread: read both Index
// controllers via the legacy input API and rebuild the shared pad state.
// Swing detector: room-space speed of the RIGHT controller (Corvo's sword
// hand), EMA-smoothed, edge-triggered with a cooldown so one physical swing
// is one attack. Fires the attack input for HoldMs; your trigger still works
// as before - the two OR together.
static void MeleeTick()
{
    if (!g_meleeOn) return;
    int dev = g_ctrlIdx[1];
    if (dev < 0 || dev >= 16 || !g_devPoseOk[dev]) return;
    static float  lastP[3];
    static double lastMs = 0.0;
    static bool   have = false;
    static float  sm = 0.0f;
    double now = MaimNowMs();
    float p[3] = { g_devPose[dev][0][3], g_devPose[dev][1][3], g_devPose[dev][2][3] };
    if (have && now > lastMs && now - lastMs < 200.0) {
        float dt = (float)((now - lastMs) * 0.001);
        float dx = p[0]-lastP[0], dy = p[1]-lastP[1], dz = p[2]-lastP[2];
        float step = sqrtf(dx*dx + dy*dy + dz*dz);
        float v = step / dt;
        sm = 0.5f * sm + 0.5f * v;
        // 30.23: g_inMenu turned out to read TRUE all through VR gameplay
        // (telemetry showed 12 m/s swings blocked by it). Gate on weapon
        // tracking instead - if the sword follows your hand, you are playing.
        // 35.0: SUSTAINED swing, not a one-frame peak. A "run" starts when
        // smoothed speed crosses the gate, accumulates travel while above
        // it, survives a brief dip (hysteresis at 70% of the gate), and the
        // attack fires only once the run has lasted SwingMs AND covered
        // SwingDistM. A flick peaks hard but dies in ~50 ms and ~10 cm, so
        // it never qualifies; a real swing sails past both bars mid-arc.
        {
            static double runStart = 0.0;    // 0 = no active run
            static float  runDist  = 0.0f;
            static float  runPeak  = 0.0f;
            static bool   runFired = false;
            if (sm > g_meleeSpeed) {
                if (runStart == 0.0) {
                    runStart = now; runDist = 0.0f;
                    runPeak = 0.0f; runFired = false;
                }
                runDist += step;
                if (sm > runPeak) runPeak = sm;
                if (!runFired &&
                    (now - runStart) >= (double)g_meleeSwingMs &&
                    runDist >= g_meleeSwingDist &&
                    g_handMesh && !g_wheelHeld && now >= g_meleeNext &&
                    now - g_uiEventMs > 3000.0) {  // 30.26: mute near menus
                    runFired = true;
                    g_meleeUntil = now + g_meleeHoldMs;
                    g_meleeNext  = now + g_meleeCoolMs;
                    g_meleeCount++;
                    g_meleeLastMs = now;
                    double h0 = MaimNowMs();
                    if (g_meleeHaptic) MaimHaptic(1, 0.7f, 0.08f);
                    double h1 = MaimNowMs();
                    Log("melee: swing #%ld (%.1f m/s, %.0f ms, %.2f m) "
                        "haptic=%.1fms", g_meleeCount, sm,
                        now - runStart, runDist, h1 - h0);
                }
            } else if (sm < g_meleeSpeed * 0.7f) {
                // the run is over. If it peaked past the gate but never
                // qualified, say so (rate-limited) - this is the tuning
                // feedback for SwingMs/SwingDistM.
                if (runStart != 0.0 && !runFired && runPeak > g_meleeSpeed) {
                    static double rejMs = 0.0;
                    if (now - rejMs > 1000.0) {
                        rejMs = now;
                        Log("melee: flick rejected (%.0f ms, %.2f m, peak "
                            "%.1f m/s - a swing needs %.0f ms AND %.2f m)",
                            now - runStart, runDist, runPeak,
                            g_meleeSwingMs, g_meleeSwingDist);
                    }
                }
                runStart = 0.0;
            }
        }
        // 30.21: no swing ever registered in the field - telemetry to see
        // why. Peak smoothed speed and every gate, once per 5 s.
        static float  peak = 0.0f;
        static double repMs = 0.0;
        if (sm > peak) peak = sm;
        if (repMs == 0.0) repMs = now + 5000.0;
        else if (now >= repMs) {
            Log("melee: peak=%.2fm/s gate=%.1f track=%d wheel=%d dev=%d",
                peak, g_meleeSpeed, (int)g_handMesh, (int)g_wheelHeld, dev);
            peak = 0.0f;
            repMs = now + 5000.0;
        }
    }
    lastP[0] = p[0]; lastP[1] = p[1]; lastP[2] = p[2];
    lastMs = now; have = true;
}


static bool MeleeActive() { return g_meleeOn && MaimNowMs() < g_meleeUntil; }
