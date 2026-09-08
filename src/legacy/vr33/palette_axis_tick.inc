    // The palette's stepped axis probe. Present thread, no D3D touched - the
    // draw detour reads g_mpStepAxis next time it runs.
    if (InterlockedExchange(&g_mpStepReq, 0)) {
        g_mpStepAxis = (g_mpStepAxis >= 2) ? -1 : (g_mpStepAxis + 1);
        if (g_mpStepAxis < 0)
            Log("ms/palette/step: >>> REST <<< - no delta on either hand. Both "
                "hands are where the game put them; this is the reference "
                "position. Press F6 for axis 0.");
        else
            Log("ms/palette/step: >>> AXIS %d <<< - hand class %s now carries "
                "%+.1f uu on palette axis %d, the other class carries nothing. "
                "hmdYaw=%.1f deg. Whichever way THIS hand moved from rest is "
                "what axis %d means. Press F6 for %s.",
                g_mpStepAxis,
                g_mpHand == 0 ? "A (left)" : g_mpHand == 1 ? "B (right)" : "BOTH",
                g_mpAmount, g_mpStepAxis, g_hmdYaw * 57.2958f, g_mpStepAxis,
                g_mpStepAxis >= 2 ? "rest" : "the next axis");
    }
