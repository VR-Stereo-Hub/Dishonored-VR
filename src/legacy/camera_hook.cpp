// legacy/camera_hook.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


extern "C" void __cdecl InjectCameraMatrix(void* thisptr)
{
    g_injTotal++;
    if (!g_injectHead) return;
    if (!thisptr || thisptr != (void*)g_camObj) { g_injMiss++; return; }
    // NOTE: the spin test proved the renderer does NOT use this camera-object
    // matrix for the view. Head-look is now injected into the c0 view-projection
    // (which the stereo shear proved DOES drive the render). This routine is
    // left as a no-op; we only READ the camera object (orientation/position)
    // elsewhere to compute the head correction.
    (void)thisptr; (void)&RotAbout;
}


static bool InstallCameraHook()
{
    if (g_camHookInstalled) return true;

    // build an executable stub:
    //   pushfd; pushad; push ecx; call InjectCameraMatrix; add esp,4; popad;
    //   popfd; <original epilogue: pop esi; mov esp,ebp; pop ebp; ret>
    g_camStub = (uint8_t*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE,
                                       PAGE_EXECUTE_READWRITE);
    if (!g_camStub) { Log("camhook: VirtualAlloc failed"); return false; }
    uint8_t s[32]; int n = 0;
    s[n++] = 0x9C;                       // pushfd
    s[n++] = 0x60;                       // pushad
    s[n++] = 0x51;                       // push ecx
    s[n++] = 0xE8;                       // call rel32
    int32_t crel = (int32_t)((uintptr_t)&InjectCameraMatrix - ((uintptr_t)g_camStub + n + 4));
    memcpy(s + n, &crel, 4); n += 4;
    s[n++] = 0x83; s[n++] = 0xC4; s[n++] = 0x04; // add esp,4
    s[n++] = 0x61;                       // popad
    s[n++] = 0x9D;                       // popfd
    s[n++] = 0x5E;                       // pop esi
    s[n++] = 0x8B; s[n++] = 0xE5;        // mov esp,ebp
    s[n++] = 0x5D;                       // pop ebp
    s[n++] = 0xC3;                       // ret
    memcpy(g_camStub, s, n);

    // patch the epilogue with jmp rel32 -> stub
    uint8_t* at = (uint8_t*)kCamHookAt;
    DWORD op;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        Log("camhook: VirtualProtect failed"); return false;
    }
    memcpy(g_camOrigEpi, at, 5);
    at[0] = 0xE9;
    int32_t jrel = (int32_t)((uintptr_t)g_camStub - ((uintptr_t)at + 5));
    memcpy(at + 1, &jrel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);

    g_camHookInstalled = true;
    Log("camhook: INSTALLED detour @ 0x%08x -> stub %p (orig epi %02x %02x %02x %02x %02x)",
        (unsigned)kCamHookAt, (void*)g_camStub,
        g_camOrigEpi[0], g_camOrigEpi[1], g_camOrigEpi[2], g_camOrigEpi[3], g_camOrigEpi[4]);
    return true;
}


// F3 = toggle head-look injection into the c0 view-projection. F5 = recenter.
static void CameraHookTick()
{
    bool f3 = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    static bool f3was = false;
    bool f3edge = f3 && !f3was; f3was = f3;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    static bool f5was = false;
    bool f5edge = f5 && !f5was; f5was = f5;

    // keep the player-camera pointer fresh (we READ it for orientation/position)
    if (g_camRefresh-- <= 0) {
        g_camRefresh = 150;
        if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    }

    if (f3edge) {
        if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
        if (!g_camObj) { Log("headinject: no camera yet - load a level first"); return; }
        g_injectHead = !g_injectHead;
        if (g_injectHead) RecenterHead();
        Log("headinject: %s (F3)", g_injectHead ? "ON" : "off");
    }
    if (f5edge && g_injectHead) RecenterHead();

    // rebuild the head correction each frame (used by next frame's c0 draws)
    UpdateHeadInject();

    if (g_injectHead && (g_frame % 90 == 0))
        Log("headinject: hmdYaw=%.1f refHmd=%.1f refGame=%.1f haveA=%d cam=%p",
            g_hmdYaw*57.2958f, g_refHmdYaw*57.2958f, g_refGameYaw*57.2958f,
            (int)g_haveA, (void*)g_camObj);
}
