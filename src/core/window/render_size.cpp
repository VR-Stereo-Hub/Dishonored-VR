// core/window/render_size.cpp - the render-resolution picker's mechanism
// (41.1). Included by src/mod/dishonoredvr.cpp (unity build).
//
// WHAT THE GAME WILL TAKE, measured (docs/dishonored/ENGINE_NOTES.md, "The
// render size"): the console's `setres` reaches the engine on 41.1 and does
// nothing (empty reply, no Reset, no size change - measured 2026-09-03 with a
// real fullscreen mode and a windowed size); a WINDOWED size is clamped to
// the desktop's rows; a FULLSCREEN size is taken verbatim from the game's own
// ini ([SystemSettings] ResX/ResY, which UE3's AppCompat overwrites from an
// [AppCompatBucketN] at startup) as long as it is a display mode the adapter
// lists, and falls back to a real mode otherwise (2560x2688 -> 2560x1440).
//
// So the picker has two routes, both at the NEXT LAUNCH:
//   1. write the size into the game's ini (both files, all four buckets) - a
//      size the display lists (`res modes`) is honoured as-is;
//   2. [Screen] VirtualMode=1 (default off): for a size the display does NOT
//      list (the eye's recommended 2496x2688, say), the proxy - which IS the
//      game's IDirect3D9 - advertises it in the mode list the game validates
//      against (GetAdapterModeCount / EnumAdapterModes, the list the removed
//      41.0 machinery also fed) and, when the game then creates a FULLSCREEN
//      device at that size, turns the present parameters WINDOWED with the
//      backbuffer kept: a windowed D3D9 backbuffer may be any size, and the
//      fullscreen path has no desktop clamp. Every one of the 41.0-era dead
//      ends ran the game windowed; this uses the fullscreen path. Measured
//      before it ships; the bbox line (capture) is the crop detector.
//
// The verdict is never the requested number: `res: HONOURED` reads the
// capture (the backbuffer the game really renders into), and prints the
// centre density in px/deg so a sharpness complaint is arithmetic.

#include "core/hooks/vtable.h"
#include <intrin.h>
#include <shlobj.h>

typedef UINT    (__stdcall* PFN_GetAdapterModeCount)(IDirect3D9*, UINT, D3DFORMAT);
typedef HRESULT (__stdcall* PFN_EnumAdapterModes)(IDirect3D9*, UINT, D3DFORMAT, UINT, D3DDISPLAYMODE*);
typedef HRESULT (__stdcall* PFN_GetAdapterDisplayMode)(IDirect3D9*, UINT, D3DDISPLAYMODE*);
static PFN_GetAdapterModeCount   g_resOrigGAMC = NULL;
static PFN_EnumAdapterModes      g_resOrigEAM = NULL;
static PFN_GetAdapterDisplayMode g_resOrigGADM = NULL;
static UINT     g_resRealRefresh = 0;
static uint32_t g_resAskedW = 0, g_resAskedH = 0;      // the last ask this session
static bool     g_resAskedFull = true;
static uint32_t g_resVerdictW = 0, g_resVerdictH = 0;  // the capture size the verdict was printed for
static bool     g_resHooked = false;

static bool ResVirtualActive() { return g_resVirtual && g_resWantW >= 640 && g_resWantH >= 480; }

static float ResDensityPxPerDeg(uint32_t w, float hfovDeg)
{
    if (!w || hfovDeg <= 1.0f) return 0.0f;
    return (float)w / (2.0f * tanf(hfovDeg * 0.5f * 0.0174533f)) / 57.29578f;
}

// The display modes of the monitor the game window sits on (unique WxH, at
// least 1280x720, ascending). Logged once per enumeration with whether the
// runtime's recommended eye size is one of them.
static void ResEnumModes(const char* who)
{
    g_resModeN = 0;
    char dev[CCHDEVICENAME] = "";
    HMONITOR mon = MonitorFromWindow(g_gameWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXA mi; ZeroMemory(&mi, sizeof(mi)); mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoA(mon, &mi)) strncpy(dev, mi.szDevice, sizeof(dev) - 1);
    DEVMODEA dm; ZeroMemory(&dm, sizeof(dm)); dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsExA(dev[0] ? dev : NULL, i, &dm, 0); ++i) {
        if (dm.dmPelsWidth < 1280 || dm.dmPelsHeight < 720) continue;
        bool seen = false;
        for (int k = 0; k < g_resModeN; ++k)
            if (g_resModes[k][0] == dm.dmPelsWidth && g_resModes[k][1] == dm.dmPelsHeight) { seen = true; break; }
        if (seen || g_resModeN >= 64) continue;
        g_resModes[g_resModeN][0] = dm.dmPelsWidth;
        g_resModes[g_resModeN][1] = dm.dmPelsHeight;
        ++g_resModeN;
    }
    // ascending by height, then width
    for (int a = 0; a < g_resModeN; ++a)
        for (int b = a + 1; b < g_resModeN; ++b)
            if (g_resModes[b][1] < g_resModes[a][1] || (g_resModes[b][1] == g_resModes[a][1] && g_resModes[b][0] < g_resModes[a][0])) {
                uint32_t t0 = g_resModes[a][0], t1 = g_resModes[a][1];
                g_resModes[a][0] = g_resModes[b][0]; g_resModes[a][1] = g_resModes[b][1];
                g_resModes[b][0] = t0; g_resModes[b][1] = t1;
            }
    char list[512] = "";
    for (int k = 0; k < g_resModeN; ++k) {
        char one[24];
        _snprintf(one, sizeof(one), "%s%ux%u", k ? " " : "", g_resModes[k][0], g_resModes[k][1]);
        strncat(list, one, sizeof(list) - strlen(list) - 1);
    }
    uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh);
    bool eyeIsMode = false;
    for (int k = 0; k < g_resModeN; ++k) if (g_resModes[k][0] == ew && g_resModes[k][1] == eh) eyeIsMode = true;
    DEVMODEA cur; ZeroMemory(&cur, sizeof(cur)); cur.dmSize = sizeof(cur);
    EnumDisplaySettingsExA(dev[0] ? dev : NULL, ENUM_CURRENT_SETTINGS, &cur, 0);
    Log("res: monitor %s (desktop %ux%u, %s): %d display modes >= 1280x720: %s | eye recommended %ux%u %s",
        dev[0] ? dev : "(primary)", cur.dmPelsWidth, cur.dmPelsHeight, who, g_resModeN, list, ew, eh,
        !ew ? "(no runtime yet)" : eyeIsMode ? "IS one of them" : "is NOT a display mode here (VirtualMode=1 provides it)");
}

static bool ResIsMode(uint32_t w, uint32_t h)
{
    for (int k = 0; k < g_resModeN; ++k) if (g_resModes[k][0] == w && g_resModes[k][1] == h) return true;
    return false;
}

static void ResGameIniPaths(char* engine, char* compat, size_t cap)
{
    char docs[MAX_PATH] = "";
    SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs);
    _snprintf(engine, cap, "%s\\My Games\\Dishonored\\DishonoredGame\\Config\\DishonoredEngine.ini", docs);
    _snprintf(compat, cap, "%s\\My Games\\Dishonored\\DishonoredGame\\Config\\DishonoredCompat.ini", docs);
}

// The relaunch route: the size into the game's own ini. Backed up once
// (`.pre-dvr` beside each file), every write logged with the old value, and
// the AppCompat buckets set too (session 2 measured: [SystemSettings] alone
// does not hold, AppCompat writes a bucket's ResX/ResY over it at startup).
static bool ResWriteGameIni(uint32_t w, uint32_t h, bool full)
{
    char engine[MAX_PATH], compat[MAX_PATH];
    ResGameIniPaths(engine, compat, MAX_PATH);
    if (GetFileAttributesA(engine) == INVALID_FILE_ATTRIBUTES) {
        Log("res: the game's ini is not at %s - nothing written (run the game once so it exists)", engine);
        return false;
    }
    char bak[MAX_PATH];
    _snprintf(bak, MAX_PATH, "%s.pre-dvr", engine);
    if (CopyFileA(engine, bak, TRUE)) Log("res: backed up %s -> .pre-dvr (once)", engine);
    _snprintf(bak, MAX_PATH, "%s.pre-dvr", compat);
    if (CopyFileA(compat, bak, TRUE)) Log("res: backed up %s -> .pre-dvr (once)", compat);
    char sw[16], sh[16], oldX[32] = "", oldY[32] = "", oldF[32] = "";
    _snprintf(sw, sizeof(sw), "%u", w);
    _snprintf(sh, sizeof(sh), "%u", h);
    GetPrivateProfileStringA("SystemSettings", "ResX", "?", oldX, sizeof(oldX), engine);
    GetPrivateProfileStringA("SystemSettings", "ResY", "?", oldY, sizeof(oldY), engine);
    GetPrivateProfileStringA("SystemSettings", "Fullscreen", "?", oldF, sizeof(oldF), engine);
    bool ok = WritePrivateProfileStringA("SystemSettings", "ResX", sw, engine) != 0;
    ok = WritePrivateProfileStringA("SystemSettings", "ResY", sh, engine) && ok;
    ok = WritePrivateProfileStringA("SystemSettings", "Fullscreen", full ? "True" : "False", engine) && ok;
    Log("res: DishonoredEngine.ini [SystemSettings] ResX %s -> %s, ResY %s -> %s, Fullscreen %s -> %s (%s)",
        oldX, sw, oldY, sh, oldF, full ? "True" : "False", ok ? "written" : "WRITE FAILED");
    int buckets = 0;
    if (GetFileAttributesA(compat) != INVALID_FILE_ATTRIBUTES) {
        for (int b = 1; b <= 4; ++b) {
            char sec[32];
            _snprintf(sec, sizeof(sec), "AppCompatBucket%d", b);
            char ox[32] = "";
            GetPrivateProfileStringA(sec, "ResX", "", ox, sizeof(ox), compat);
            if (!ox[0]) continue;   // no such bucket key: leave the section alone
            if (WritePrivateProfileStringA(sec, "ResX", sw, compat) && WritePrivateProfileStringA(sec, "ResY", sh, compat)) ++buckets;
        }
        Log("res: DishonoredCompat.ini AppCompat buckets set to %ux%u (%d of 4; the bucket AppCompat picks at startup "
            "overwrites [SystemSettings], so all of them carry the size)", w, h, buckets);
    } else {
        Log("res: DishonoredCompat.ini not found at %s - if the game starts at 1600x900 again, AppCompat overwrote "
            "the size from a bucket", compat);
    }
    return ok;
}

// The ask. Validates, records it in the mod's ini keys (SAVE AS DEFAULTS
// persists them), writes the game's ini, says what to expect at the next
// launch.
static void ResRequest(uint32_t w, uint32_t h, bool full, const char* who)
{
    if (w < 640 || h < 480 || w > 16384 || h > 16384) {
        Log("res: %ux%u refused (640x480..16384x16384)", w, h);
        return;
    }
    if (!g_resModeN) ResEnumModes("ask");
    g_resWantW = w; g_resWantH = h; g_resWantFull = full;
    g_resAskedW = w; g_resAskedH = h; g_resAskedFull = full;
    g_resVerdictW = g_resVerdictH = 0;
    const bool isMode = ResIsMode(w, h);
    const bool written = ResWriteGameIni(w, h, full);
    LaunchArgsWrite(w, h, full);   // the route the engine honours (see LaunchArgsInstall)
    {   // the ask must survive the relaunch it needs: into the mod's ini now, not
        // only on SAVE AS DEFAULTS (the verdict at the next boot reads it back)
        char ini[MAX_PATH], v[32];
        _snprintf(ini, MAX_PATH, "%s\dishonored_vr.ini", g_dir);
        _snprintf(v, sizeof(v), "%u", w); WritePrivateProfileStringA("Screen", "RenderWidth", v, ini);
        _snprintf(v, sizeof(v), "%u", h); WritePrivateProfileStringA("Screen", "RenderHeight", v, ini);
        WritePrivateProfileStringA("Screen", "RenderFullscreen", full ? "1" : "0", ini);
        WritePrivateProfileStringA("Screen", "VirtualMode", g_resVirtual ? "1" : "0", ini);
    }
    _snprintf(g_resLastLine, sizeof(g_resLastLine), "asked %ux%u %s (%s) - takes effect at the next launch (-ResX/-ResY on the command line)%s",
              w, h, full ? "fullscreen" : "windowed", who,
              !full ? "; a windowed size is clamped to the desktop's rows"
              : isMode ? "" : g_resVirtual ? "; not a display mode: VirtualMode provides it"
                                          : "; NOT a display mode here: the game will fall back - set VirtualMode=1");
    g_resLastLine[sizeof(g_resLastLine) - 1] = 0;
    Log("res: %s%s", g_resLastLine, written ? "" : " (the game's ini was NOT written)");
}

// Once the capture knows the size (and again after every size change): the
// verdict against the ask, with the density the picture will have.
static void ResVerdictTick()
{
    const uint32_t cw = dvr::capture::width(), ch = dvr::capture::height();
    if (!cw || !ch) return;
    if (cw == g_resVerdictW && ch == g_resVerdictH) return;
    g_resVerdictW = cw; g_resVerdictH = ch;
    const float claim = dvr::camera::fov_deg();
    const float dens = ResDensityPxPerDeg(cw, claim);
    uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh);
    float hh = 0.0f, hv = 0.0f; dvr::vr::headset_half_fov_deg(&hh, &hv);
    const float eyeDens = (ew && hh > 0.0f) ? ResDensityPxPerDeg(ew, 2.0f * hh) : 0.0f;
    if (!g_resWantW || !g_resWantH) {
        Log("res: the game renders %ux%u (its own size; no [Screen] RenderWidth/Height ask) | claim %.1f deg -> centre "
            "%.1f px/deg (the eye %ux%u wants %.1f) | `res modes`, `res <W>x<H>[f|w]` to ask, F10 Display",
            cw, ch, claim, dens, ew, eh, eyeDens);
        return;
    }
    const bool honoured = cw == g_resWantW && ch == g_resWantH;
    _snprintf(g_resLastLine, sizeof(g_resLastLine), "%s: asked %ux%u %s, the game renders %ux%u",
              honoured ? "HONOURED" : "NOT HONOURED", g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed", cw, ch);
    g_resLastLine[sizeof(g_resLastLine) - 1] = 0;
    if (honoured)
        Log("res: HONOURED - the game renders %ux%u as asked | claim %.1f deg -> centre density %.1f px/deg (the eye "
            "%ux%u wants %.1f)", cw, ch, claim, dens, ew, eh, eyeDens);
    else
        DVR_WARN("res: NOT HONOURED - asked %ux%u %s, the game renders %ux%u (claim %.1f deg -> %.1f px/deg). A "
                 "windowed size is clamped to the desktop's rows; a fullscreen size must be a display mode the "
                 "adapter lists (`res modes`) or VirtualMode=1 must provide it; the ask lives in the game's own "
                 "ini and takes a relaunch",
                 g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed", cw, ch, claim, dens);
}

static void ResStatusLine()
{
    const uint32_t cw = dvr::capture::width(), ch = dvr::capture::height();
    uint32_t ew = 0, eh = 0; dvr::vr::recommended_eye_size(&ew, &eh);
    Log("res: renders %ux%u | ask %ux%u %s | eye %ux%u | virtual mode %s (hooks %s) | %s",
        cw, ch, g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed", ew, eh,
        g_resVirtual ? "ON" : "off", g_resHooked ? "installed" : "not installed", g_resLastLine);
}

// `res modes | status | virtual on|off | <W>x<H>[f|w]`
static bool ResCommand(const char* args)
{
    char a[32] = "", b[16] = "";
    const int n = sscanf(args, "%31s %15s", a, b);
    if (n < 1 || !strcmp(a, "status")) { ResStatusLine(); return true; }
    if (!strcmp(a, "modes")) { ResEnumModes("seam"); return true; }
    if (!strcmp(a, "virtual")) {
        bool on;
        if (n >= 2 && DvrOnOff(b, &on)) {
            g_resVirtual = on;
            char ini[MAX_PATH];
            _snprintf(ini, MAX_PATH, "%s\dishonored_vr.ini", g_dir);
            WritePrivateProfileStringA("Screen", "VirtualMode", on ? "1" : "0", ini);   // survives the relaunch
            if (g_resWantW && g_resWantH) LaunchArgsWrite(g_resWantW, g_resWantH, g_resWantFull);   // the file carries it too
            Log("res: virtual mode %s - the proxy %s the asked size in the adapter's mode list and turns a fullscreen "
                "device at that size windowed with the backbuffer kept (takes effect at the next launch)",
                on ? "ON" : "off", on ? "advertises" : "no longer advertises");
        } else Log("res: virtual on|off (now %s)", g_resVirtual ? "ON" : "off");
        return true;
    }
    unsigned w = 0, h = 0; char tail = 'f';
    if (sscanf(a, "%ux%u%c", &w, &h, &tail) >= 2) {
        if (!w && !h) {   // res 0x0: the game's own size again
            g_resWantW = g_resWantH = 0;
            LaunchArgsWrite(0, 0, true);
            char ini[MAX_PATH];
            _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
            WritePrivateProfileStringA("Screen", "RenderWidth", "0", ini);
            WritePrivateProfileStringA("Screen", "RenderHeight", "0", ini);
            return true;
        }
        ResRequest(w, h, tail != 'w', "seam");
        return true;
    }
    Log("res: modes | status | virtual on|off | <W>x<H>[f|w]  (e.g. res 2496x2688f)");
    return true;
}

// ---- the proxy's own IDirect3D9: the mode list the game validates against ----
static bool ResExtraModeFormat(D3DFORMAT fmt)
{
    return fmt == D3DFMT_X8R8G8B8 || fmt == D3DFMT_A8R8G8B8 || fmt == D3DFMT_R5G6B5 || fmt == D3DFMT_X1R5G5B5;
}

static UINT __stdcall hkResGetAdapterModeCount(IDirect3D9* self, UINT adapter, D3DFORMAT fmt)
{
    const UINT n = g_resOrigGAMC ? g_resOrigGAMC(self, adapter, fmt) : 0;
    DVR_LOG_FIRST_N(dvr::log::Cat::res, dvr::log::Level::Info, 6,
                    "res: the game asked the mode list (fmt %d): %u modes%s - from %p", (int)fmt, n,
                    ResVirtualActive() && ResExtraModeFormat(fmt) ? " + ours" : "", _ReturnAddress());
    if (!ResVirtualActive() || !ResExtraModeFormat(fmt)) return n;
    return n + 1;
}

static HRESULT __stdcall hkResEnumAdapterModes(IDirect3D9* self, UINT adapter, D3DFORMAT fmt, UINT mode, D3DDISPLAYMODE* out)
{
    if (ResVirtualActive() && ResExtraModeFormat(fmt)) {
        const UINT real = g_resOrigGAMC ? g_resOrigGAMC(self, adapter, fmt) : 0;
        if (mode == real && out) {
            out->Width = g_resWantW;
            out->Height = g_resWantH;
            out->RefreshRate = g_resRealRefresh ? g_resRealRefresh : 60;   // a list filtered by refresh must keep it
            out->Format = fmt;
            DVR_LOG_FIRST_N(dvr::log::Cat::res, dvr::log::Level::Info, 4,
                            "res: handed the game our %ux%u@%u mode (slot %u) - from %p", g_resWantW, g_resWantH,
                            out->RefreshRate, mode, _ReturnAddress());
            return D3D_OK;
        }
    }
    return g_resOrigEAM ? g_resOrigEAM(self, adapter, fmt, mode, out) : D3DERR_INVALIDCALL;
}

static HRESULT __stdcall hkResGetAdapterDisplayMode(IDirect3D9* self, UINT adapter, D3DDISPLAYMODE* mode)
{
    const HRESULT hr = g_resOrigGADM ? g_resOrigGADM(self, adapter, mode) : E_FAIL;
    if (SUCCEEDED(hr) && mode) {
        if (mode->RefreshRate) g_resRealRefresh = mode->RefreshRate;
        DVR_LOG_FIRST_N(dvr::log::Cat::res, dvr::log::Level::Info, 4,
                        "res: the game read the adapter's display mode: %ux%u@%u (NOT spoofed) - from %p",
                        mode->Width, mode->Height, mode->RefreshRate, _ReturnAddress());
    }
    return hr;
}

static void ResHookD3D9(IDirect3D9* d3d)
{
    if (!d3d || g_resHooked) return;
    void* o6 = PatchVtable(d3d, 6, (void*)hkResGetAdapterModeCount);
    void* o7 = PatchVtable(d3d, 7, (void*)hkResEnumAdapterModes);
    void* o8 = PatchVtable(d3d, 8, (void*)hkResGetAdapterDisplayMode);
    if (o6 && !g_resOrigGAMC) g_resOrigGAMC = (PFN_GetAdapterModeCount)o6;
    if (o7 && !g_resOrigEAM) g_resOrigEAM = (PFN_EnumAdapterModes)o7;
    if (o8 && !g_resOrigGADM) g_resOrigGADM = (PFN_GetAdapterDisplayMode)o8;
    g_resHooked = g_resOrigGAMC && g_resOrigEAM && g_resOrigGADM;
    Log("res: adapter mode hooks %s (count/enum/display mode; the list is logged, and fed only with VirtualMode=1: %s%s)",
        g_resHooked ? "installed" : "FAILED", g_resVirtual ? "ON, " : "off",
        ResVirtualActive() ? "the asked size is advertised" : g_resVirtual ? "no size asked" : "");
}

// ---- the launch arguments: the route the engine honours ----------------------
// Measured 2026-09-03 (runs 07-08): with 2560x1440 fullscreen in every place
// of the game's own ini (both files, all four buckets, rewritten by the game
// itself at launch) the game still created a 1920x1080 WINDOWED device, and
// never Reset out of it. The one route measured to be taken verbatim is the
// command line: UE3 parses -ResX= -ResY= -FullScreen / -Windowed before it
// reads anything else (the research branch's probes: fullscreen 5120x1440
// honoured exactly, a windowed size clamped to the desktop's rows). The proxy
// is loaded before the engine's entry point, so it can hand the engine an
// extended command line: the exe's (and the CRT's) import slots for
// GetCommandLineA/W are pointed at copies with the picker's arguments
// appended. kernel32 only, so it is loader-lock safe like the pad hook.
// The ask lives in <gamedir>\dishonored_vr_launch.txt (one line, written by
// ResRequest, deleted by `res 0x0`) - DllMain must not touch the ini.
typedef LPSTR  (WINAPI* PFN_GetCommandLineA)();
typedef LPWSTR (WINAPI* PFN_GetCommandLineW)();
static PFN_GetCommandLineA g_origGetCmdA = NULL;
static PFN_GetCommandLineW g_origGetCmdW = NULL;
static char    g_launchExtra[128] = "";
static char    g_launchCmdA[2048] = "";
static wchar_t g_launchCmdW[2048] = L"";
static int     g_launchSlots = 0;

static LPSTR WINAPI hkLaunchGetCommandLineA() { return g_launchCmdA[0] ? g_launchCmdA : (g_origGetCmdA ? g_origGetCmdA() : NULL); }
static LPWSTR WINAPI hkLaunchGetCommandLineW() { return g_launchCmdW[0] ? g_launchCmdW : (g_origGetCmdW ? g_origGetCmdW() : NULL); }

static void LaunchArgsPath(char* out, size_t cap) { _snprintf(out, cap, "%s\\dishonored_vr_launch.txt", g_dir); }

// ResRequest writes the ask here; DllMain reads it at the next launch.
static void LaunchArgsWrite(uint32_t w, uint32_t h, bool full)
{
    char path[MAX_PATH];
    LaunchArgsPath(path, MAX_PATH);
    if (!w || !h) {
        DeleteFileA(path);
        Log("res: launch arguments cleared (%s) - the game picks its own size at the next launch", path);
        return;
    }
    char line[128];
    _snprintf(line, sizeof(line), "-ResX=%u -ResY=%u %s%s", w, h, full ? "-FullScreen" : "-Windowed",
              g_resVirtual ? " -DvrVirtualMode" : "");   // our own token, stripped before the engine sees it
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { Log("res: could not write %s (err %lu)", path, GetLastError()); return; }
    DWORD wr = 0;
    WriteFile(f, line, (DWORD)strlen(line), &wr, NULL);
    CloseHandle(f);
    Log("res: launch arguments for the next launch: \"%s\" (%s; the proxy appends them to the command line the "
        "engine reads, before its entry point)", line, path);
}

// DllMain: read the ask, extend the command line the engine will read.
static void LaunchArgsInstall()
{
    char path[MAX_PATH];
    LaunchArgsPath(path, MAX_PATH);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD rd = 0;
    char buf[128] = "";
    ReadFile(f, buf, sizeof(buf) - 1, &rd, NULL);
    CloseHandle(f);
    buf[rd < sizeof(buf) ? rd : sizeof(buf) - 1] = 0;
    for (char* p = buf; *p; ++p) if (*p == '\r' || *p == '\n') { *p = 0; break; }
    if (!buf[0] || strncmp(buf, "-ResX=", 6) != 0) {
        DVR_LOG(dvr::log::Cat::res, dvr::log::Level::Warn, "launch: %s holds \"%s\", not a -ResX= line - ignored", path, buf);
        return;
    }
    strncpy(g_launchExtra, buf, sizeof(g_launchExtra) - 1);
    {   // the ask travels with the file, so the verdict knows it even when the
        // harness restores the mod's ini around a launch
        unsigned w = 0, h = 0;
        if (sscanf(buf, "-ResX=%u -ResY=%u", &w, &h) == 2) { g_launchW = w; g_launchH = h; }
        g_launchFull = strstr(buf, "-Windowed") == NULL;
        char* tok = strstr(buf, " -DvrVirtualMode");
        if (tok) { g_launchVirtual = true; *tok = 0; }   // ours, not the engine's
        g_resVirtual = g_resVirtual || g_launchVirtual;
        if (g_launchVirtual) { g_resWantW = g_launchW; g_resWantH = g_launchH; g_resWantFull = g_launchFull; }
    }
    const char* origA = GetCommandLineA();
    const wchar_t* origW = GetCommandLineW();
    _snprintf(g_launchCmdA, sizeof(g_launchCmdA), "%s %s", origA ? origA : "", g_launchExtra);
    wchar_t extraW[128];
    MultiByteToWideChar(CP_ACP, 0, g_launchExtra, -1, extraW, 128);
    _snwprintf(g_launchCmdW, 2048, L"%s %s", origW ? origW : L"", extraW);
    g_launchCmdW[2047] = 0;
    // the exe's own import slots, and the CRT's if the CRT is a DLL (its
    // WinMain glue is where UE3 gets its lpCmdLine from)
    static const char* kMods[] = { NULL, "msvcr100.dll", "msvcr90.dll", "msvcr110.dll", "msvcr120.dll" };
    for (size_t i = 0; i < sizeof(kMods) / sizeof(kMods[0]); ++i) {
        HMODULE m = kMods[i] ? GetModuleHandleA(kMods[i]) : GetModuleHandleA(NULL);
        if (!m) continue;
        void** sa = dvr::hooks::find_iat_slot_in(m, "kernel32.dll", "GetCommandLineA");
        void** sw = dvr::hooks::find_iat_slot_in(m, "kernel32.dll", "GetCommandLineW");
        if (sa) { void* o = dvr::hooks::patch_iat_slot(sa, (void*)hkLaunchGetCommandLineA); if (o && !g_origGetCmdA) g_origGetCmdA = (PFN_GetCommandLineA)o; if (o) ++g_launchSlots; }
        if (sw) { void* o = dvr::hooks::patch_iat_slot(sw, (void*)hkLaunchGetCommandLineW); if (o && !g_origGetCmdW) g_origGetCmdW = (PFN_GetCommandLineW)o; if (o) ++g_launchSlots; }
    }
    DVR_LOG(dvr::log::Cat::res, dvr::log::Level::Info,
            "launch: command line extended for the engine: \"%s\" + \" %s\" (%d import slot(s) patched; the "
            "CreateDevice line says whether the engine took it)", origA ? origA : "", g_launchExtra, g_launchSlots);
    if (!g_launchSlots)
        DVR_LOG(dvr::log::Cat::res, dvr::log::Level::Warn,
                "launch: no GetCommandLine import slot found - the engine reads its command line another way; the "
                "ask in %s is inert", path);
}

// Before CreateDevice / Reset: log what the game asked for; under VirtualMode,
// a FULLSCREEN device at the asked size becomes a windowed one with the
// backbuffer kept (the fullscreen path took the size verbatim; a windowed
// backbuffer may be any size; nothing else is touched).
static void ResBeforePresentParams(D3DPRESENT_PARAMETERS* pp, const char* where)
{
    if (!pp) return;
    DVR_LOG_FIRST_N(dvr::log::Cat::res, dvr::log::Level::Info, 12,
                    "res: %s - the game asked for %ux%u windowed=%d fmt=%d refresh=%u (ask %ux%u %s, virtual %s)",
                    where, pp->BackBufferWidth, pp->BackBufferHeight, (int)pp->Windowed, (int)pp->BackBufferFormat,
                    pp->FullScreen_RefreshRateInHz, g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed",
                    g_resVirtual ? "ON" : "off");
    if (!ResVirtualActive() || pp->Windowed) return;
    if (pp->BackBufferWidth != g_resWantW || pp->BackBufferHeight != g_resWantH) {
        DVR_LOG_FIRST_N(dvr::log::Cat::res, dvr::log::Level::Warn, 4,
                        "res: %s - VirtualMode is on but the game asked fullscreen %ux%u, not the %ux%u it was handed: "
                        "the game's ini did not carry the size, or the mode list was not consulted (the lines above say)",
                        where, pp->BackBufferWidth, pp->BackBufferHeight, g_resWantW, g_resWantH);
        return;
    }
    DVR_WARN("res: %s - VirtualMode: the game asked FULLSCREEN %ux%u (our advertised mode); creating it WINDOWED with "
             "the backbuffer kept (a windowed backbuffer may be any size; the headset takes the capture). Expect "
             "`capture: %ux%u` and a full bbox; a smaller bbox is the crop signature",
             where, pp->BackBufferWidth, pp->BackBufferHeight, g_resWantW, g_resWantH);
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;
    if (pp->SwapEffect == D3DSWAPEFFECT_COPY) pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
}
