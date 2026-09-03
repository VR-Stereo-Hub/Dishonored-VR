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
    _snprintf(g_resLastLine, sizeof(g_resLastLine), "asked %ux%u %s (%s) - takes effect at the next launch%s",
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
            Log("res: virtual mode %s - the proxy %s the asked size in the adapter's mode list and turns a fullscreen "
                "device at that size windowed with the backbuffer kept (takes effect at the next launch)",
                on ? "ON" : "off", on ? "advertises" : "no longer advertises");
        } else Log("res: virtual on|off (now %s)", g_resVirtual ? "ON" : "off");
        return true;
    }
    unsigned w = 0, h = 0; char tail = 'f';
    if (sscanf(a, "%ux%u%c", &w, &h, &tail) >= 2) {
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
