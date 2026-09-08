    // VR-33: F6 steps the palette's axis probe. NOT a numpad key - every one
    // of them is already claimed by a bare-key block above - and NOT a CTRL
    // chord: CTRL is the game's block, so holding it to press a diagnostic
    // makes the character do something and reads back as interference.
    // rest -> axis 0 -> axis 1 -> axis 2 -> rest, one press each.
    {
        static bool mpWas = false;
        const bool mpk = (GetAsyncKeyState(VK_F6) & 0x8000) != 0 &&
                         !(GetAsyncKeyState(VK_SHIFT) & 0x8000);
        if (mpk && !mpWas) InterlockedExchange(&g_mpStepReq, 1);
        mpWas = mpk;
    }

