// core/window/game_window.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

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
    // Windows asks the window how big it may get, and the default answer is
    // "no bigger than the monitor". We render past the edge of the screen on
    // purpose, so raise the ceiling. The game's own handler runs first and we
    // widen whatever it decided.
    // 36.1: while the hold is armed the game must never see a REAL resize -
    // we shrink the desktop window for cosmetics and the render must not
    // follow it down. Rewrite the size the game's handler receives to the
    // size it already believes. (Minimize passes through untouched.)
    // (36.3: gate widened from g_holdWindow alone - with the desktop-window
    // feature on, the cap in hkSetWindowPos resizes the real window BEFORE
    // the hold arms, and the game must not see those sizes either.)
    if (msg == WM_SIZE && g_wantClientW && (g_holdWindow || g_deskWinW) &&
        wp != SIZE_MINIMIZED)
        lp = (LPARAM)MAKELPARAM(g_wantClientW, g_wantClientH);
    if (msg == WM_GETMINMAXINFO && g_wantClientW && lp) {
        LRESULT r = CallWindowProcA(g_ovlOldWndProc, hwnd, msg, wp, lp);
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        LONG needX = (LONG)g_wantClientW + 128, needY = (LONG)g_wantClientH + 128;
        if (mmi->ptMaxTrackSize.x < needX) mmi->ptMaxTrackSize.x = needX;
        if (mmi->ptMaxTrackSize.y < needY) mmi->ptMaxTrackSize.y = needY;
        if (mmi->ptMaxSize.x < needX) mmi->ptMaxSize.x = needX;
        if (mmi->ptMaxSize.y < needY) mmi->ptMaxSize.y = needY;
        return r;
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
        Log("res: window hook armed at %s - the render size no longer depends "
            "on how big this monitor is", who);
}
