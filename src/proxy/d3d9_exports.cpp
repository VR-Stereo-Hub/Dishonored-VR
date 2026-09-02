// proxy/d3d9_exports.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ----------------------------------------------------------------------------
// Real d3d9
// ----------------------------------------------------------------------------
static bool EnsureRealD3D9()
{
    if (g_realD3D9) return true;
    // 41.0: the DXVK fork is gone. The game renders through the system D3D9;
    // the proxy only hooks it.
    {
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
    dvr::stereo::register_all();   // 41.0: before the config selects [Stereo] Method
    EnsureConfig(); // safe here (post loader-lock); not in DllMain
    dvr::crash::install();   // fingerprint VEH + minidump filter, before any hook
    DvrDebugInit();          // command seam + status provider
    // 41.0: the runtime layer. The device provider builds the D3D11 device on
    // the adapter the runtime names; the instance comes up here (fail-soft).
    dvr::vr::set_device_provider(DvrProvideD3D11Device);
    dvr::vr::init_instance();
    if (!EnsureRealD3D9() || !g_realCreate9) return NULL;
    IDirect3D9* d3d = g_realCreate9(sdkVersion);
    Log("Direct3DCreate9(sdk=%u) -> %p", sdkVersion, (void*)d3d);
    dvr::frame::set_disabled(g_disabled);
    if (d3d && !g_disabled) {
        DvrInstallFrameHooks();        // the game side of the frame path
        dvr::frame::hook_d3d9(d3d);    // CreateDevice -> Present/Reset/... hooks
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
