    // VR-33 step 2: SHIFT+F6 arms a batch of draw captures. Same key family as
    // the probe it supports, and shifted so it cannot be hit while stepping.
    {
        static bool pcWas = false;
        const bool pck = (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
                         (GetAsyncKeyState(VK_F6) & 0x8000);
        if (pck && !pcWas) InterlockedExchange(&g_pcArmReq, 1);
        pcWas = pck;
    }

