// proxy/dllmain.cpp - DllMain of the d3d9.dll proxy. Included by the unity
// build (src/mod/dishonoredvr.cpp) until the proxy has its own translation unit.
//
// This runs under the Windows loader lock, so it does as little as it can:
// paths, clock, log, the kill switch, two ints of early config, and the two
// hook families that MUST be in place before the game's first D3D call (see
// the comments inside). Everything else waits for Direct3DCreate9.
// Line numbers in comments and docs refer to the original single file
// (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        InitializeCriticalSection(&g_padLock);
        dvr::clock::init();
        { LARGE_INTEGER f; if (QueryPerformanceFrequency(&f)) g_qpcFreq = f.QuadPart; }
        dvr::paths::init(hinst);
        strncpy(g_dir, dvr::paths::game_dir(), MAX_PATH - 1);
        g_dir[MAX_PATH - 1] = 0;

        char path[MAX_PATH];
        _snprintf(path, MAX_PATH, "%s\\disable_vr.txt", g_dir);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            g_disabled = true;

        // dishonored_vr.log next to the game exe (rotates the previous run to
        // dishonored_vr.prev.log). Log levels are read with the config later;
        // the env overrides apply from the first line.
        dvr::log::init(g_dir, "dishonored_vr");
        {
            char lv[32] = "", cats[512] = "";
            GetEnvironmentVariableA("DVR_LOG", lv, sizeof(lv));
            GetEnvironmentVariableA("DVR_LOG_CATS", cats, sizeof(cats));
            dvr::log::configure(lv, cats);
        }
        DVR_LOG(dvr::log::Cat::proxy, dvr::log::Level::Info,
                "=== Dishonored VR proxy loaded (dishonoredvr %s, build %s, built %s %s) ===",
                DVR_VERSION, DVR_BUILD_ID, __DATE__, __TIME__);
        DVR_LOG(dvr::log::Cat::proxy, dvr::log::Level::Info,
                "dir: %s  data: %s  disabled: %d", g_dir, dvr::paths::data_dir(), (int)g_disabled);
        // NOTE: do NOT touch the ini here. DllMain runs under the Windows loader
        // lock; profile/file calls that pull in other DLLs there can abort the
        // whole process (0xc0000142). Config is loaded lazily from the first
        // Direct3DCreate9 call instead - see EnsureConfig().
        //
        // The XInput IAT hook, however, MUST happen here: Dishonored's input
        // system decides "gamepad or not" once, at startup, before the first
        // D3D call. Both xinput1_3.dll and the exe's IAT are already mapped and
        // snapped by the loader at this point, and VirtualProtect/GetProcAddress
        // are loader-lock-safe. From the game's first poll we answer "pad
        // connected" (neutral state until the VR controllers come online).
        if (!g_disabled) InstallPadHook();
        // 41.1: the render-resolution picker's ask as -ResX/-ResY/-FullScreen on
        // the command line the engine reads (kernel32 + a plain file read; see
        // core/window/render_size.cpp, LaunchArgsInstall). The ini is not touched.
        if (!g_disabled) LaunchArgsInstall();
    } else if (reason == DLL_PROCESS_DETACH) {
        DVR_LOG(dvr::log::Cat::proxy, dvr::log::Level::Info, "=== proxy unloading ===");
        dvr::log::shutdown();
    }
    return TRUE;
}
