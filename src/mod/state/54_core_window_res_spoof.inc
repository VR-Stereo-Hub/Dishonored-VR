// State chunk 54 for core/window/res_spoof - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// Desktop-size spoof. Hooks are installed at load and pass through untouched
// until config arms g_spoofW/H, so DllMain stays loader-lock-clean.
typedef int  (WINAPI *GetSystemMetrics_t)(int);
typedef BOOL (WINAPI *GetMonitorInfoA_t)(HMONITOR, LPMONITORINFO);
typedef BOOL (WINAPI *GetMonitorInfoW_t)(HMONITOR, LPMONITORINFO);
static GetSystemMetrics_t g_realGSM  = NULL;
static GetMonitorInfoA_t  g_realGMIA = NULL;
static GetMonitorInfoW_t  g_realGMIW = NULL;

// 32.70: WHY THE GAME NEVER RENDERS AT THE RESOLUTION IT IS TOLD TO.
// The log finally named it. DishonoredEngine.ini says ResX=1920 ResY=1080,
// yet CreateDevice arrives asking for 1783x1003 - the game shrank its own
// request by 0.9287 with the aspect held, which is a "fit the window inside
// the usable desktop" clamp, not anything we or DXVK did. 1003 tall is then
// stretched into a ~2240-tall eye texture, and THAT is the blur. It was never
// the side-by-side split: the split only costs horizontal pixels, and the
// vertical shortfall is just as large.
// So the clamp has to go. We already spoof the screen size and the monitor
// rect; the miss is every OTHER way Windows can be asked "how much room is
// there", and until one launch says which one the game actually calls, the
// honest move is to answer them all consistently and log what gets asked.
static bool g_gsmTrace = true;

// SPI_GETWORKAREA is the classic UE3 windowed clamp and we have never hooked
// it. If the game asks it for room, it currently hears the real 1920x1080
// minus the taskbar - which is almost exactly the 1003 we are stuck at.
typedef BOOL (WINAPI *SystemParametersInfoA_t)(UINT, UINT, PVOID, UINT);
typedef BOOL (WINAPI *SystemParametersInfoW_t)(UINT, UINT, PVOID, UINT);
static SystemParametersInfoA_t g_realSPIA = NULL;
static SystemParametersInfoW_t g_realSPIW = NULL;

// 32.75: SOMETHING PUTS THE WINDOW BACK, AND IT IS NOT US.
// With every re-assert of mine removed, the log is clean and damning:
//   res: startup window client -> 3200x1800   (GetClientRect agrees: 1800)
//   device Reset (3200x1800 windowed=1)       (the game accepted it)
//   res: after Reset ... window client is 3200x1071
// Sixty milliseconds, nothing of ours in between, and the window is short
// again. The width sticks every time; only the height is pulled back, and it
// is pulled back to exactly the monitor. So the game re-sizes its own window
// after a resize, using a height it clamped against the desktop somewhere we
// have not found - and we have already spent launches guessing which API that
// is (GetSystemMetrics, GetMonitorInfo, SPI_GETWORKAREA are all spoofed and
// it still happens).
//
// So stop guessing who, and veto what. Every window-resizing call the game
// makes goes through one of these three USER32 entry points. If a call would
// make the window shorter than the size we are driving, the size is replaced
// with ours. No loop is possible: we always substitute the SAME size, and
// Windows sends no WM_SIZE when the size does not change.
// The caller's return address is logged once. The exe has no ASLR, so that
// address names the exact instruction doing this if we ever want to know.
typedef BOOL (WINAPI *SetWindowPos_t)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL (WINAPI *MoveWindow_t)(HWND, int, int, int, int, BOOL);
typedef BOOL (WINAPI *SetWindowPlacement_t)(HWND, const WINDOWPLACEMENT*);
static SetWindowPos_t       g_realSWP = NULL;
static MoveWindow_t         g_realMW  = NULL;
static SetWindowPlacement_t g_realSWPl = NULL;

// The one hook that matters now. UE3 sizes its viewport from the client rect;
// give it the size we want rendered. Hooked ONLY in the exe - the fork has to
// keep seeing the real window so its swapchain matches what it presents into.
typedef BOOL (WINAPI *GetClientRect_t)(HWND, LPRECT);
static GetClientRect_t g_realGCR = NULL;

static void InstallForkWindowHooks();
