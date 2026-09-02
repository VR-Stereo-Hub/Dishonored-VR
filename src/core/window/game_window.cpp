// core/window/game_window.cpp - the game window: the WndProc subclass (the
// 38.78 focus keep-alive and the overlay's input), the focus guard and the
// vsync uncap. The window is an ordinary window at whatever size the player
// picked; the 4032x2268 spoof that used to live next to this went in 41.0.
// Included by src/mod/dishonoredvr.cpp (unity build).

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    // 38.78 THE QUEST "BAM, BROKEN" STATE, from an affected user's log: the
    // game window lost desktop focus (Virtual Desktop's overlay takes the
    // foreground; the player is in the headset and nobody is at the desk to
    // click the game back). UE3 then fires OnLostFocusPause, auto-pauses,
    // and stops BOTH input polling and ProcessViewRotation - measured as
    // pad polls=0 and headwrites=0 for the rest of the session while the
    // render kept flowing at 66 fps: frozen head-locked view, dead
    // controllers, "hands not working". The old focus guard only reclaims
    // focus from SteamVR-titled windows on purpose (a desktop user who
    // alt-tabs must be left alone), so instead of fighting over the
    // foreground, the game simply never learns it lost it: while a headset
    // is being served, deactivation messages are rewritten to "active".
    // Alt-tab still works on the desktop - the game just keeps running.
    if (g_vrKeepAlive && (g_vrReady || g_xrOn)) {
        if (msg == WM_ACTIVATEAPP && wp == FALSE)
            wp = TRUE;
        else if (msg == WM_ACTIVATE && LOWORD(wp) == WA_INACTIVE)
            wp = MAKEWPARAM(WA_ACTIVE, HIWORD(wp));
        else if (msg == WM_KILLFOCUS)
            return 0;                     // the game never hears it
    }
    if (g_ovlVisible && g_ovlInit) {
        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp);
        ImGuiIO& io = ImGui::GetIO();
        bool mouseMsg = msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        bool keyMsg   = msg >= WM_KEYFIRST   && msg <= WM_KEYLAST;
        if ((io.WantCaptureMouse && mouseMsg) || (io.WantCaptureKeyboard && keyMsg))
            return 0;
    }
    return CallWindowProcA(g_ovlOldWndProc, hwnd, msg, wp, lp);
}


// 38.92: install the game-window subclass exactly once, as early as we know
// the window. Safe before ImGui exists - the ImGui branch inside is gated on
// g_ovlInit, and everything we do not handle is forwarded to the game.
static void InstallWindowSubclass(const char* who)
{
    if (g_ovlOldWndProc || !g_gameWnd) return;
    g_ovlOldWndProc = (WNDPROC)SetWindowLongPtrA(g_gameWnd, GWLP_WNDPROC,
                                                 (LONG_PTR)OverlayWndProc);
    if (g_ovlOldWndProc)
        Log("window: subclass armed at %s (focus keep-alive + overlay input)", who);
}


// 30.54: see the note at g_autoFocus. SetForegroundWindow is refused for a
// background process, so borrow the foreground thread's input queue for the
// call - the standard AttachThreadInput dance.
static void FocusGuardTick()
{
    static double focusNext = 0.0;
    bool manual = g_focusNow;
    if (!manual && !g_autoFocus) return;
    if (!g_gameWnd) return;
    double now = MaimNowMs();
    if (!manual && now < focusNext) return;
    focusNext = now + 2000.0;
    HWND fg = GetForegroundWindow();
    if (!manual) {
        if (!fg || fg == g_gameWnd) return;
        char title[160] = {0};
        GetWindowTextA(fg, title, sizeof(title) - 1);
        if (!strstr(title, "SteamVR") && !strstr(title, "Steam VR") &&
            !strstr(title, "vrmonitor") && !strstr(title, "VR Settings"))
            return;                       // not SteamVR - leave the user alone
        Log("focus: SteamVR window '%s' had focus - returning it to the game", title);
    }
    g_focusNow = false;
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, NULL) : 0;
    DWORD myTid = GetCurrentThreadId();
    bool attached = (fgTid && fgTid != myTid && AttachThreadInput(myTid, fgTid, TRUE));
    if (IsIconic(g_gameWnd)) ShowWindow(g_gameWnd, SW_RESTORE);
    BringWindowToTop(g_gameWnd);
    SetForegroundWindow(g_gameWnd);
    SetFocus(g_gameWnd);
    if (attached) AttachThreadInput(myTid, fgTid, FALSE);
}


// The game syncs Present to the monitor, which pins it to the display's
// refresh. In VR the desktop refresh is irrelevant - the headset is fed from
// our own 90 Hz clock - and since each game frame renders ONE eye, that cap
// halves straight into the per-eye rate. Presenting immediately lets the GPU
// run as fast as it can, and every extra frame is a fresher eye.
static void UncapPresent(D3DPRESENT_PARAMETERS* pp, const char* where)
{
    if (!g_forceNoVSync || !pp) return;
    if (pp->PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE) return;
    Log("perf: %s - present interval 0x%08x -> IMMEDIATE (vsync off, uncapped)",
        where, (unsigned)pp->PresentationInterval);
    pp->PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp->FullScreen_RefreshRateInHz = pp->Windowed ? 0 : pp->FullScreen_RefreshRateInHz;
    if (pp->SwapEffect == D3DSWAPEFFECT_COPY) pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
}
