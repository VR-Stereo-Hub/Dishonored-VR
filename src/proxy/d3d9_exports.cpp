// proxy/d3d9_exports.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ----------------------------------------------------------------------------
// Real d3d9
// ----------------------------------------------------------------------------
static bool EnsureRealD3D9()
{
    if (g_realD3D9) return true;
    // Phase 1 of VR 2.0 (build 30.27): chain into DXVK when it is present.
    // DXVK implements the same IDirect3D9 COM surface, so every hook below
    // works unchanged - but the game then renders through Vulkan on hardware
    // that flat-tested at 250 fps. Drop dxvk_d3d9.dll next to the game exe
    // to enable; delete or rename it to fall back to system DX9. (Config
    // cannot be read this early on all paths, so presence-of-file IS the
    // switch - visible, and revertable without a rebuild.)
    {
        char dpath[MAX_PATH];
        _snprintf(dpath, MAX_PATH, "%s\\dxvk_d3d9.dll", g_dir);
        g_realD3D9 = LoadLibraryA(dpath);
        if (g_realD3D9)
            Log("backend: DXVK loaded from dxvk_d3d9.dll - DX9 -> Vulkan");
    }
    if (!g_realD3D9) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat(path, "\\d3d9.dll");
        g_realD3D9 = LoadLibraryA(path);
    }
    if (!g_realD3D9) {
        Log("FATAL: could not load any d3d9 backend (err %lu)", GetLastError());
        return false;
    }
    g_realCreate9    = (PFN_Direct3DCreate9)  GetProcAddress(g_realD3D9, "Direct3DCreate9");
    g_realCreate9Ex  = (PFN_Direct3DCreate9Ex)GetProcAddress(g_realD3D9, "Direct3DCreate9Ex");
    g_realBeginEvent = (PFN_D3DPERF_BeginEvent)GetProcAddress(g_realD3D9, "D3DPERF_BeginEvent");
    g_realEndEvent   = (PFN_D3DPERF_EndEvent) GetProcAddress(g_realD3D9, "D3DPERF_EndEvent");
    g_realSetOptions = (PFN_D3DPERF_SetOptions)GetProcAddress(g_realD3D9, "D3DPERF_SetOptions");
    g_realGetStatus  = (PFN_D3DPERF_GetStatus)GetProcAddress(g_realD3D9, "D3DPERF_GetStatus");
    g_realSetMarker  = (PFN_D3DPERF_SetMarker)GetProcAddress(g_realD3D9, "D3DPERF_SetMarker");
    g_realSetRegion  = (PFN_D3DPERF_SetRegion)GetProcAddress(g_realD3D9, "D3DPERF_SetRegion");
    g_realQueryRepeatFrame = (PFN_D3DPERF_QueryRepeatFrame)GetProcAddress(g_realD3D9, "D3DPERF_QueryRepeatFrame");
    Log("d3d9 backend ready (Create9=%p Create9Ex=%p)",
        (void*)g_realCreate9, (void*)g_realCreate9Ex);
    return true;
}


// ----------------------------------------------------------------------------
// Exports
// ----------------------------------------------------------------------------
extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT sdkVersion)
{
    EnsureConfig(); // safe here (post loader-lock); not in DllMain
    dvr::crash::install();   // fingerprint VEH + minidump filter, before any hook
    DvrDebugInit();          // command seam + status provider
#if DVR_WITH_LEGACY
    {   // 37.0: XR-1 bench, armed only by the env var (xr_bench.bat)
        char xb[8] = "";
        if (GetEnvironmentVariableA("DISHONORED_VR_XR_BENCH", xb, sizeof(xb))
            && xb[0] == '1') {
            static bool xrOnce = false;
            if (!xrOnce) {
                xrOnce = true;
                // 37.2: the game cannot run flat with the stereo pipeline
                // armed and no headset (0 fps then death - both bench runs).
                // The bench does not need our rendering at all: master
                // disable, bone-stock game, bench thread beside it.
                g_disabled = true;
                Log("xrb: bench mode - proxy DISABLED for this run (the "
                    "game runs stock; only the bench thread is ours)");
                HANDLE h = CreateThread(NULL, 0, XrBenchThread, NULL, 0, NULL);
                if (h) CloseHandle(h);
            }
        }
    }
#endif
    if (!EnsureRealD3D9() || !g_realCreate9) return NULL;
    IDirect3D9* d3d = g_realCreate9(sdkVersion);
    Log("Direct3DCreate9(sdk=%u) -> %p", sdkVersion, (void*)d3d);
    if (d3d && !g_disabled) {
        void* old = PatchVtable(d3d, 16, (void*)hkCreateDevice);
        if (old && !g_origCreateDevice) g_origCreateDevice = (PFN_CreateDevice)old;
        // 30.23: the game sizes its windowed mode from D3D's idea of the
        // desktop (GetAdapterDisplayMode, vtable 8) - not from user32. We ARE
        // its D3D, so answer with the spoofed size when armed.
        void* old8 = PatchVtable(d3d, 8, (void*)hkGetAdapterDisplayMode);
        if (old8 && !g_origGADM) g_origGADM = (PFN_GetAdapterDisplayMode)old8;
        // 6 = GetAdapterModeCount, 7 = EnumAdapterModes: the list the game
        // validates its saved resolution against before it will ask for it.
        void* old6 = PatchVtable(d3d, 6, (void*)hkGetAdapterModeCount);
        if (old6 && !g_origGAMC) g_origGAMC = (PFN_GetAdapterModeCount)old6;
        void* old7 = PatchVtable(d3d, 7, (void*)hkEnumAdapterModes);
        if (old7 && !g_origEAM) g_origEAM = (PFN_EnumAdapterModes)old7;
        Log("res: adapter mode list hooked (count=%p enum=%p)", old6, old7);
    }
    return d3d;
}


extern "C" HRESULT WINAPI Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out)
{
    if (!EnsureRealD3D9() || !g_realCreate9Ex) return E_FAIL;
    return g_realCreate9Ex(sdkVersion, out);
}


extern "C" int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR name)
{ return (EnsureRealD3D9() && g_realBeginEvent) ? g_realBeginEvent(col, name) : 0; }


extern "C" int WINAPI D3DPERF_EndEvent(void)
{ return (EnsureRealD3D9() && g_realEndEvent) ? g_realEndEvent() : 0; }


extern "C" void WINAPI D3DPERF_SetOptions(DWORD options)
{ if (EnsureRealD3D9() && g_realSetOptions) g_realSetOptions(options); }


extern "C" DWORD WINAPI D3DPERF_GetStatus(void)
{ return (EnsureRealD3D9() && g_realGetStatus) ? g_realGetStatus() : 0; }


extern "C" void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR name)
{ if (EnsureRealD3D9() && g_realSetMarker) g_realSetMarker(col, name); }


extern "C" void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR name)
{ if (EnsureRealD3D9() && g_realSetRegion) g_realSetRegion(col, name); }


extern "C" BOOL WINAPI D3DPERF_QueryRepeatFrame(void)
{ return (EnsureRealD3D9() && g_realQueryRepeatFrame) ? g_realQueryRepeatFrame() : FALSE; }
