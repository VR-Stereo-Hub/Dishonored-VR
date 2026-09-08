    // VR-33 phase 1. CTRL + Numpad2 steps the hand-move experiment through
    // off -> additive zero -> plus -> minus -> off. CTRL, because the bare
    // numpad belongs to the mesh split and the draw census.
    {
        static bool hmWas = false;
        const bool hmk = kCtrl && (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0;
        if (hmk && !hmWas) g_hmStepReq = 1;
        hmWas = hmk;
    }

