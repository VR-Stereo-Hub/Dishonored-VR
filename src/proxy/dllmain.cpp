// proxy/dllmain.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ----------------------------------------------------------------------------
// DllMain
// ----------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        InitializeCriticalSection(&g_logLock);
        InitializeCriticalSection(&g_padLock);
        { LARGE_INTEGER f; if (QueryPerformanceFrequency(&f)) g_qpcFreq = f.QuadPart; }
        GetModuleFileNameA(hinst, g_dir, MAX_PATH);
        char* slash = strrchr(g_dir, '\\');
        if (slash) *slash = 0;

        char path[MAX_PATH];
        _snprintf(path, MAX_PATH, "%s\\disable_vr.txt", g_dir);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            g_disabled = true;

        _snprintf(path, MAX_PATH, "%s\\dishonored_vr.log", g_dir);
        g_log = fopen(path, "w");
        Log("=== Dishonored VR proxy loaded (BUILD %s, built %s %s) ===",
            kBuildTag, __DATE__, __TIME__);
        Log("dir: %s  disabled: %d", g_dir, (int)g_disabled);
        // NOTE: do NOT touch the ini here. DllMain runs under the Windows loader
        // lock; profile/file calls that pull in other DLLs there can abort the
        // whole process (0xc0000142). Config is loaded lazily from the first
        // Direct3DCreate9 call instead — see EnsureConfig().
        //
        // The XInput IAT hook, however, MUST happen here: Dishonored's input
        // system decides "gamepad or not" once, at startup, before the first
        // D3D call. Both xinput1_3.dll and the exe's IAT are already mapped and
        // snapped by the loader at this point, and VirtualProtect/GetProcAddress
        // are loader-lock-safe. From the game's first poll we answer "pad
        // connected" (neutral state until the VR controllers come online).
        // 30.28: arm the desktop spoof EARLY. LoadConfig runs at the first
        // Direct3DCreate9 call, but the game sizes its window from desktop
        // metrics BEFORE that - which is why the spoof never bit in any
        // earlier run. Reading two ints here is kernel32-only (none of the
        // loader-lock DLL pulls the full config path risks).
        if (!g_disabled) {
            char ini2[MAX_PATH];
            _snprintf(ini2, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
            UINT w = GetPrivateProfileIntA("Screen", "SpoofDesktopW", 0, ini2);
            UINT h = GetPrivateProfileIntA("Screen", "SpoofDesktopH", 0, ini2);
            if (w >= 1280 && w <= 4096 && h >= 720 && h <= 2304) {
                g_spoofW = w; g_spoofH = h;
                Log("res: desktop spoof %ux%u armed at load time", w, h);
            }
        }
        if (!g_disabled) InstallPadHook();
        if (!g_disabled) InstallResSpoofHooks();
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_vrReady && g_VR_ShutdownInternal) g_VR_ShutdownInternal();
        if (g_log) { Log("=== proxy unloading ==="); fclose(g_log); g_log = NULL; }
    }
    return TRUE;
}
