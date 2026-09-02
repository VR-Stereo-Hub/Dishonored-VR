// core/window/res_spoof.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


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


static int WINAPI hkGetSystemMetrics(int idx)
{
    int real = g_realGSM ? g_realGSM(idx) : 0;
    int out  = real;
    if (g_spoofW) {
        switch (idx) {
            // the screen itself
            case SM_CXSCREEN: case SM_CXFULLSCREEN:
            case SM_CXVIRTUALSCREEN:
            // the largest a window is allowed to be
            case SM_CXMAXIMIZED: case SM_CXMAXTRACK:
                out = (int)g_spoofW; break;
            case SM_CYSCREEN: case SM_CYFULLSCREEN:
            case SM_CYVIRTUALSCREEN:
            case SM_CYMAXIMIZED: case SM_CYMAXTRACK:
                out = (int)g_spoofH; break;
            // 32.61 was lost in the rollback to 32.52: with the real
            // VIRTUALSCREEN of a dual-monitor desktop showing through, a
            // large request looks impossible and the game falls back - once
            // as far as 800x600, which it then wrote to its own ini.
            default: break;
        }
    }
    if (g_gsmTrace) {
        static int seen[64]; static int nseen = 0;
        bool dup = false;
        for (int i = 0; i < nseen; i++) if (seen[i] == idx) { dup = true; break; }
        if (!dup && nseen < 64) {
            seen[nseen++] = idx;
            Log("res/trace: GetSystemMetrics(%d) real=%d -> %d%s",
                idx, real, out, out != real ? "  [SPOOFED]" : "");
        }
    }
    return out;
}


static void SpoofWorkArea(UINT act, PVOID data)
{
    if (act != SPI_GETWORKAREA || !data || !g_spoofW) return;
    RECT* r = (RECT*)data;
    static bool logged = false;
    if (!logged) {
        logged = true;
        Log("res: SPI_GETWORKAREA %ldx%ld -> %ux%u (spoofed)",
            r->right - r->left, r->bottom - r->top, g_spoofW, g_spoofH);
    }
    r->left = 0; r->top = 0;
    r->right = (LONG)g_spoofW; r->bottom = (LONG)g_spoofH;
}


static BOOL WINAPI hkSystemParametersInfoA(UINT a, UINT u, PVOID p, UINT f)
{
    BOOL ok = g_realSPIA ? g_realSPIA(a, u, p, f) : FALSE;
    if (ok) SpoofWorkArea(a, p);
    return ok;
}


static BOOL WINAPI hkSystemParametersInfoW(UINT a, UINT u, PVOID p, UINT f)
{
    BOOL ok = g_realSPIW ? g_realSPIW(a, u, p, f) : FALSE;
    if (ok) SpoofWorkArea(a, p);
    return ok;
}


static void SpoofMonitorRect(LPMONITORINFO mi)
{
    if (!mi || !g_spoofW) return;
    mi->rcMonitor.right  = mi->rcMonitor.left + (LONG)g_spoofW;
    mi->rcMonitor.bottom = mi->rcMonitor.top  + (LONG)g_spoofH;
    mi->rcWork = mi->rcMonitor;
}


static BOOL WINAPI hkGetMonitorInfoA(HMONITOR m, LPMONITORINFO mi)
{
    BOOL ok = g_realGMIA ? g_realGMIA(m, mi) : FALSE;
    if (ok) SpoofMonitorRect(mi);
    return ok;
}


static BOOL WINAPI hkGetMonitorInfoW(HMONITOR m, LPMONITORINFO mi)
{
    BOOL ok = g_realGMIW ? g_realGMIW(m, mi) : FALSE;
    if (ok) SpoofMonitorRect(mi);
    return ok;
}


// outer window size that yields our target client area, for the current style
static void WantOuterSize(HWND h, int* ow, int* oh)
{
    RECT r = { 0, 0, (LONG)g_wantClientW, (LONG)g_wantClientH };
    AdjustWindowRectEx(&r, (DWORD)GetWindowLongA(h, GWL_STYLE), FALSE,
                       (DWORD)GetWindowLongA(h, GWL_EXSTYLE));
    *ow = r.right - r.left;
    *oh = r.bottom - r.top;
}


// The 32.75 veto substituted the right size and STILL lost, because the caller
// had restored the game's framed style first: with a caption and a sizing
// border, Windows clamps the height to the monitor no matter what size is
// asked for. So the veto now strips the frame as well - a borderless popup has
// no tracking size to clamp, and its client rect is its window rect.
static void ForceBorderless(HWND h)
{
    const LONG framed = WS_CAPTION | WS_THICKFRAME | WS_BORDER | WS_DLGFRAME
                      | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    LONG st = GetWindowLongA(h, GWL_STYLE);
    if (st & framed) {
        SetWindowLongA(h, GWL_STYLE, (st & ~framed) | WS_POPUP);
        LONG ex = GetWindowLongA(h, GWL_EXSTYLE);
        SetWindowLongA(h, GWL_EXSTYLE,
            ex & ~(WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME));
    }
}


// 32.77: THE WINDOW WAS THE WRONG THING TO CHANGE.
// Three builds went into making the game's window physically 1800 px tall and
// Windows won every round - it clamps a framed window to the monitor, and the
// borderless giant that got past the clamp rendered a black screen that never
// reached the menu. The whole fight was unnecessary.
//
// The game does not KNOW how big its window is. It asks. And DXVK already
// scales the backbuffer into the window at Present, so a 3200x1800 backbuffer
// inside a 1920x1071 window is perfectly legal - big render, normal desktop
// window, which is what was wanted in the first place.
//
// So: leave the window alone, and answer the question differently. The exe's
// GetClientRect returns our target; the fork's is untouched, because DXVK must
// keep seeing the REAL window to size its swapchain and scale into it. One
// synthetic WM_SIZE tells UE3 to re-derive, and it re-derives from our answer.
// These veto hooks stay, but only to watch: modifying nothing, logging who
// moves the window, because that record is the reason we know what to do here.
static bool VetoShrink(HWND h, int* cx, int* cy, const char* who, void* caller)
{
    if (!g_wantClientW || h != g_gameWnd || !cx || !cy) return false;
    // Name the module the call came from. exe or fork changes what we do next,
    // and guessing that has already cost launches.
    char modName[64] = "?";
    {
        HMODULE m = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                             | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)caller, &m) && m) {
            char full[MAX_PATH] = {};
            GetModuleFileNameA(m, full, MAX_PATH);
            const char* b = strrchr(full, '\\');
            strncpy(modName, b ? b + 1 : full, sizeof(modName) - 1);
        }
    }
    static int told = 0;
    if (told < 24) { told++;
        Log("res: watch %s(%dx%d) from %p [%s] - window left alone",
            who, *cx, *cy, caller, modName); }
    return false;                          // observe only
}


// 32.78: FOUND IT. ONE CALL SITE.
//   CreateDevice -> (3218x1887)      the game rendered 1887 lines tall
//   capture: 3218x1887               and a real frame came out at that size
//   watch SetWindowPos(3224x1100) from 009c3399 [Dishonored.exe]
//   device Reset (3218x1071)         and there it goes
// Then, after the synthetic WM_SIZE, the identical three lines again with
// 3200x1800 in place of 1887. The game ACCEPTS every size it is given. What
// undoes it is one instruction in Dishonored.exe that resizes the game's own
// window to a monitor-clamped height; that fires WM_SIZE, which resets the
// device back down. Not Windows, not the fork, not the ini, not the desktop.
//
// 32.75 tried to out-argue this call by substituting a bigger size, and lost
// to Windows' tracking clamp. The answer is not a better size - it is no size
// at all. SWP_NOSIZE leaves the window exactly as it is; no size change means
// no WM_SIZE, no Reset, and the size the game already accepted simply stays.
// The window on the desktop is never touched, which is the whole point.
static BOOL WINAPI hkSetWindowPos(HWND h, HWND after, int x, int y,
                                  int cx, int cy, UINT flags)
{
    if (!(flags & SWP_NOSIZE)) {
        VetoShrink(h, &cx, &cy, "SetWindowPos", _ReturnAddress());
        // 36.3: the game's window is born small and ONE call site in
        // Dishonored.exe grows it (32.78's discovery). With the desktop-
        // window feature on, cap that growth at the desk size - the window
        // then never spends a single frame desktop-sized. The WM_SIZE this
        // produces reaches the game rewritten to the spoofed client, so the
        // render never follows the real size (32.79's scar: this is a CAP
        // at a usable size, never a block that strands the window tiny).
        if (!g_holdWindow && g_deskWinW && h == g_gameWnd) {
            RECT want = { 0, 0, (LONG)g_deskWinW, (LONG)g_deskWinH };
            DWORD style = (DWORD)GetWindowLongA(h, GWL_STYLE);
            AdjustWindowRect(&want, style, FALSE);
            int maxW = want.right - want.left, maxH = want.bottom - want.top;
            if (cx > maxW || cy > maxH) {
                static int capTold = 0;
                if (capTold < 8) { capTold++;
                    Log("res: capping the game's %dx%d window at %dx%d "
                        "(desktop window cap)", cx, cy, maxW, maxH); }
                if (cx > maxW) cx = maxW;
                if (cy > maxH) cy = maxH;
                InterlockedExchange(&g_ovlRecenter, 1);
            }
        }
        if (g_holdWindow && h == g_gameWnd) {
            static int told = 0;
            if (told < 8) { told++;
                Log("res: HOLD - dropping the game's own resize to %dx%d "
                    "(SWP_NOSIZE). No resize means no WM_SIZE, so nothing "
                    "resets the render back down.", cx, cy); }
            flags |= SWP_NOSIZE;
        }
    }
    return g_realSWP ? g_realSWP(h, after, x, y, cx, cy, flags) : FALSE;
}


static BOOL WINAPI hkMoveWindow(HWND h, int x, int y, int w, int ht, BOOL rp)
{
    VetoShrink(h, &w, &ht, "MoveWindow", _ReturnAddress());
    if (g_holdWindow && h == g_gameWnd) {      // same rule, other entry point
        RECT cur = {};
        if (GetWindowRect(h, &cur)) {
            w  = cur.right - cur.left;
            ht = cur.bottom - cur.top;
        }
    }
    return g_realMW ? g_realMW(h, x, y, w, ht, rp) : FALSE;
}


static BOOL WINAPI hkSetWindowPlacement(HWND h, const WINDOWPLACEMENT* wp)
{
    WINDOWPLACEMENT local;
    if (wp && g_holdWindow && h == g_gameWnd) {
        int cx = wp->rcNormalPosition.right - wp->rcNormalPosition.left;
        int cy = wp->rcNormalPosition.bottom - wp->rcNormalPosition.top;
        VetoShrink(h, &cx, &cy, "SetWindowPlacement",
                   _ReturnAddress());
        RECT cur = {};
        if (GetWindowRect(h, &cur)) {       // keep the window where it is
            local = *wp;
            local.rcNormalPosition = cur;
            wp = &local;
        }
    }
    return g_realSWPl ? g_realSWPl(h, wp) : FALSE;
}


static BOOL WINAPI hkGetClientRect(HWND h, LPRECT r)
{
    BOOL ok = g_realGCR ? g_realGCR(h, r) : FALSE;
    if (ok && r && g_wantClientW && h == g_gameWnd) {
        static int told = 0;
        if (told < 6) { told++;
            Log("res: GetClientRect %ldx%ld -> %ux%u (the game renders at what "
                "it is told the window is; the real window is untouched)",
                r->right - r->left, r->bottom - r->top,
                g_wantClientW, g_wantClientH); }
        r->left = 0; r->top = 0;
        r->right  = (LONG)g_wantClientW;
        r->bottom = (LONG)g_wantClientH;
    }
    return ok;
}


static void InstallResSpoofHooks()
{
    struct { const char* fn; void* hook; void** real; } h[9] = {
        { "GetSystemMetrics",       (void*)hkGetSystemMetrics,       (void**)&g_realGSM  },
        { "GetMonitorInfoA",        (void*)hkGetMonitorInfoA,        (void**)&g_realGMIA },
        { "GetMonitorInfoW",        (void*)hkGetMonitorInfoW,        (void**)&g_realGMIW },
        { "SystemParametersInfoA",  (void*)hkSystemParametersInfoA,  (void**)&g_realSPIA },
        { "SystemParametersInfoW",  (void*)hkSystemParametersInfoW,  (void**)&g_realSPIW },
        { "SetWindowPos",           (void*)hkSetWindowPos,           (void**)&g_realSWP  },
        { "MoveWindow",             (void*)hkMoveWindow,             (void**)&g_realMW   },
        { "SetWindowPlacement",     (void*)hkSetWindowPlacement,     (void**)&g_realSWPl },
        { "GetClientRect",          (void*)hkGetClientRect,          (void**)&g_realGCR  },
    };
    int n = 0;
    for (int i = 0; i < 9; i++) {
        void** slot = FindIatSlot("USER32.dll", h[i].fn);
        // Name each miss. "2/3" told us a hook was absent but not WHICH, and a
        // silent gap in this list is exactly how a clamp survives being fixed.
        if (!slot) { Log("res: %s not in the import table - not hooked", h[i].fn); continue; }
        DWORD op;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &op)) continue;
        *h[i].real = *slot;
        *slot = h[i].hook;
        VirtualProtect(slot, sizeof(void*), op, &op);
        n++;
    }
    Log("res: metrics/window hooks installed (%d/9; inert until SpoofDesktopW/H and RenderWidth/Height are set)", n);

    InstallForkWindowHooks();
}


// Retried from CreateDevice: at load time the fork may not be mapped yet, and
// a hook that silently never installed is exactly how a cause stays hidden.
static void InstallForkWindowHooks()
{
    static bool done = false;
    if (done) return;
    HMODULE fork = GetModuleHandleA("dxvk_d3d9.dll");
    if (!fork) {
        Log("res: dxvk_d3d9.dll not loaded yet - fork window hooks deferred");
    } else {
        struct { const char* fn; void* hook; } f[3] = {
            { "SetWindowPos",       (void*)hkSetWindowPos },
            { "MoveWindow",         (void*)hkMoveWindow },
            { "SetWindowPlacement", (void*)hkSetWindowPlacement },
        };
        int fn2 = 0;
        for (int i = 0; i < 3; i++) {
            void** slot = FindIatSlotIn(fork, "USER32.dll", f[i].fn);
            if (!slot) { Log("res: fork does not import %s", f[i].fn); continue; }
            DWORD op;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &op)) continue;
            // the real pointers were already captured from the exe's table;
            // if the exe did not import it, take the fork's original
            if (i == 0 && !g_realSWP)  g_realSWP  = (SetWindowPos_t)*slot;
            if (i == 1 && !g_realMW)   g_realMW   = (MoveWindow_t)*slot;
            if (i == 2 && !g_realSWPl) g_realSWPl = (SetWindowPlacement_t)*slot;
            *slot = f[i].hook;
            VirtualProtect(slot, sizeof(void*), op, &op);
            fn2++;
        }
        Log("res: fork window hooks installed (%d/3)", fn2);
        done = true;
    }
}


// ----------------------------------------------------------------------------
// Hooks
// ----------------------------------------------------------------------------
// Applied from Present so the game's message loop is running and its viewport
// is live - a WM_SIZE now actually reaches UE3's Resize path, which is what
// makes it rebuild the backbuffer and every scene target at the new size.
// Re-checked periodically because a settings apply or a mode change can put
// the window back; bounded, because resize -> Reset -> resize is a loop.
static void RenderSizeTick()
{
    if (!g_wantClientW || !g_gameWnd) return;
    static int frames = 0;
    static bool done = false;
    frames++;
    if (!done && frames >= 90) {
        done = true;                    // once. see the note in hkReset.
        ApplyRenderWindowSize("startup");
    }
    // 36.1: shrink the REAL window - cosmetic only, the game renders at the
    // spoofed client size and its WM_SIZE arrives rewritten by the subclass.
    // Requires: hold armed (so the game cannot resize back and the subclass
    // rewrite is active) and the subclass actually installed. A few tries,
    // because the game may still be fighting for its size in the first
    // seconds; never again after that (a resize loop is the 32.73 lesson).
    // 36.3: no longer waits for the frame-90 apply - the WM_SIZE rewrite
    // gates on the feature itself now, so the only prerequisite is the
    // subclass that performs the rewrite. Fires within the first second.
    static int shrunk = 0;
    if (frames >= 30 && g_deskWinW && g_ovlOldWndProc && shrunk < 4) {
        RECT rr = {};
        if (g_realGCR) g_realGCR(g_gameWnd, &rr);
        UINT cw = (UINT)(rr.right - rr.left), ch = (UINT)(rr.bottom - rr.top);
        if (cw != g_deskWinW || ch != g_deskWinH) {
            RECT want = { 0, 0, (LONG)g_deskWinW, (LONG)g_deskWinH };
            DWORD style = (DWORD)GetWindowLongA(g_gameWnd, GWL_STYLE);
            AdjustWindowRect(&want, style, FALSE);
            shrunk++;
            SetWindowPos(g_gameWnd, NULL, 16, 16,
                         want.right - want.left, want.bottom - want.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            InterlockedExchange(&g_ovlRecenter, 1);   // 36.2: panel must follow
            Log("res: desktop window shrunk to %ux%u client (try %d) - "
                "cosmetic only, the game still renders %ux%u",
                g_deskWinW, g_deskWinH, shrunk, g_wantClientW, g_wantClientH);
        }
    }
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


// 32.72: RESOLUTION, FOR REAL THIS TIME.
// The measurement that settles it: the ini asks for 2880x1620, the game
// creates a 1920x1080 device, and then Resets to 1920x1071. That 1071 is the
// client height of a window on a 1080-tall monitor. The render follows the
// WINDOW, not the ini - which is why every attempt to raise the resolution
// from the ini, from Steam launch options or by rewriting the backbuffer has
// plateaued at ~1071 lines for weeks.
//
// The old ForceRes rewrote pp->BackBufferWidth/Height. That is exactly the
// wrong lever: the backbuffer changes but the game's own SizeX/SizeY does
// not, so its scene targets and viewports stay small and the frame lands in
// a corner (build 32.57). It is kept here only as a no-op tombstone so the
// idea does not get reinvented.
//
// The right lever is the window itself. Size the game window's CLIENT AREA
// to the resolution we want and the game re-derives everything from it -
// backbuffer, scene targets, viewports, projection - all internally
// consistent, because we never lied to it. The window ends up bigger than
// the monitor and hangs off the screen; that costs nothing, because the
// image that matters is the one we capture out of the backbuffer and hand
// to the headset. Nobody is looking at the desktop.
// 32.79: and now the OTHER half of the log.
//   device Reset (3200x1800 windowed=1)   capture: 3200x1800   129 fps - held.
//   ...27 seconds later...
//   device Reset (1920x1080 windowed=0)
// windowed=0. Entering gameplay, the game switches itself to FULLSCREEN, and a
// fullscreen device takes its size from a display mode, not from a window - so
// every window-side fix we have is bypassed and it lands back on 1920x1080.
// The high resolution only ever existed in the menu.
//
// Rewriting the present parameters is what build 32.57 did, and it produced an
// image in the corner of the screen, because the backbuffer grew while the
// game's own SizeX/SizeY stayed small. That objection is gone: the game reads
// its size through GetClientRect, which now answers with the target, so its
// viewports and scene targets are already that size. Making the backbuffer
// match is what keeps them consistent - it is the correction, not the lie.
// Windowed is forced too, so the game cannot escape into a display mode.
static void ForceRes(D3DPRESENT_PARAMETERS* pp, const char* where)
{
    if (!pp || !g_wantClientW || !g_wantClientH) return;
    // 40.1: log what the game ASKED for, every time, before we touch anything.
    // "the game asked for 3840x2160" is the fact that explains a whole class of
    // reports, and it used to be printed only when we happened to force windowed.
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 12,
        "res: %s the game asked for %ux%u windowed=%d fmt=%d refresh=%u "
        "(our target is %ux%u, PinBackbuffer=%d)",
        where, pp->BackBufferWidth, pp->BackBufferHeight, (int)pp->Windowed,
        (int)pp->BackBufferFormat, pp->FullScreen_RefreshRateInHz,
        g_wantClientW, g_wantClientH, g_pinBackbuffer);

    // 40.1: pin the size at creation when asked. See the note on
    // g_pinBackbuffer: setres was measured returning "(empty reply)" with no
    // Reset, so the backbuffer never reached the target on its own and the
    // engine's client size (GetClientRect, already hooked) disagreed with it
    // for the whole session.
    if (g_pinBackbuffer && (pp->BackBufferWidth != g_wantClientW ||
                            pp->BackBufferHeight != g_wantClientH)) {
        DVR_WARN("res: %s PINNING the backbuffer %ux%u -> %ux%u. GetClientRect already "
                 "reports the target, so this makes the buffer AGREE with the size the "
                 "game thinks it has, instead of rendering into a differently shaped "
                 "one for the whole session.",
                 where, pp->BackBufferWidth, pp->BackBufferHeight,
                 g_wantClientW, g_wantClientH);
        pp->BackBufferWidth  = g_wantClientW;
        pp->BackBufferHeight = g_wantClientH;
    }
    if (pp->Windowed) return;
    static int told = 0;
    if (told < 10) { told++;
        Log("res: %s forcing windowed (was fullscreen %ux%u) - a fullscreen "
            "device sizes itself from a display mode, which is how the render "
            "escaped back to 1920x1080 mid-session. Size left untouched.",
            where, pp->BackBufferWidth, pp->BackBufferHeight); }
    // 32.85: when the game asked for fullscreen, it does not manage its
    // window at all - that is why the desktop has been EMPTY since the
    // fullscreen fallback started (the window sat at its tiny creation size,
    // or hidden). The overlay lives in that window; being unable to reach F10
    // is this bug, not a missing feature. Flag it; fixed up after CreateDevice.
    g_fsRescue = true;
    // 32.81: DO NOT TOUCH THE SIZE. The capture dump settled it - a perfect
    // side-by-side pair drawn into the top-left 1920x1080 of a 3200x1800
    // buffer, black everywhere else. The buffer grew; the game's viewport did
    // not. 32.78 got this right and 32.79 broke it: pinning the size here
    // means the device is BORN at 3200x1800, so when the synthetic WM_SIZE
    // arrives UE3 compares it against a backbuffer that already matches,
    // decides nothing changed, and never updates its own SizeX/SizeY off
    // 1920x1080. The resize is the only thing that moves the game's internal
    // size, and pinning the buffer is what stops the resize from happening.
    // So the game stays the author of its own dimensions. All we take away is
    // the escape into fullscreen, where there is no window to size at all.
    pp->Windowed = TRUE;
    pp->FullScreen_RefreshRateInHz = 0;      // must be 0 when windowed
    if (pp->SwapEffect == D3DSWAPEFFECT_COPY)
        pp->SwapEffect = D3DSWAPEFFECT_DISCARD;
}


static void ApplyRenderWindowSize(const char* where)
{
    if (!g_gameWnd || !g_wantClientW || !g_wantClientH) return;
    // No SetWindowPos. UE3 resizes its viewport when it gets WM_SIZE, and the
    // new client size is carried in lParam - so hand it the size directly.
    // Nothing about the real window changes, so there is nothing for Windows
    // to clamp and nothing on screen to go wrong. Everywhere else the game
    // re-reads its size, hkGetClientRect gives the same answer.
    RECT real = {};
    if (g_realGCR) g_realGCR(g_gameWnd, &real);
    Log("res: %s telling the game its client is %ux%u (real window stays "
        "%ldx%ld on the desktop)", where, g_wantClientW, g_wantClientH,
        real.right - real.left, real.bottom - real.top);
    g_holdWindow = true;               // from here on, the game may not resize
    SendMessageA(g_gameWnd, WM_SIZE, SIZE_RESTORED,
                 (LPARAM)MAKELPARAM(g_wantClientW, g_wantClientH));
}


static HRESULT __stdcall hkGetAdapterDisplayMode(IDirect3D9* self, UINT adapter,
                                                 D3DDISPLAYMODE* mode)
{
    HRESULT hr = g_origGADM ? g_origGADM(self, adapter, mode) : E_FAIL;
    if (SUCCEEDED(hr) && mode && mode->RefreshRate) g_realRefresh = mode->RefreshRate;
    if (SUCCEEDED(hr) && mode && g_spoofW) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            Log("res: GetAdapterDisplayMode %ux%u -> %ux%u (spoofed)",
                mode->Width, mode->Height, g_spoofW, g_spoofH);
        }
        mode->Width  = g_spoofW;
        mode->Height = g_spoofH;
    }
    return hr;
}


static bool VrExtraMode(D3DFORMAT fmt)
{
    // Only advertise into the formats a game actually enumerates for display
    // modes, and only when a render size has been asked for.
    if (!g_forceResW || !g_forceResH) return false;
    return fmt == D3DFMT_X8R8G8B8 || fmt == D3DFMT_A8R8G8B8
        || fmt == D3DFMT_R5G6B5   || fmt == D3DFMT_X1R5G5B5;
}


static UINT __stdcall hkGetAdapterModeCount(IDirect3D9* self, UINT adapter,
                                            D3DFORMAT fmt)
{
    UINT n = g_origGAMC ? g_origGAMC(self, adapter, fmt) : 0;
    if (!VrExtraMode(fmt)) return n;
    static int told = 0;
    if (told < 8) { told++;
        // The return address IS the game's resolution-validation code. That
        // address is where the disassembly starts - no ASLR, so it maps
        // straight onto the exe on disk.
        Log("res: mode list for fmt %d has %u modes -> %u (ours: %ux%u) - "
            "asked from %p", (int)fmt, n, n + 1, g_forceResW, g_forceResH,
            _ReturnAddress()); }
    return n + 1;
}


static HRESULT __stdcall hkEnumAdapterModes(IDirect3D9* self, UINT adapter,
                                            D3DFORMAT fmt, UINT mode,
                                            D3DDISPLAYMODE* out)
{
    if (VrExtraMode(fmt)) {
        const UINT real = g_origGAMC ? g_origGAMC(self, adapter, fmt) : 0;
        if (mode == real && out) {          // the one we added, last in the list
            out->Width  = g_forceResW;
            out->Height = g_forceResH;
            // 32.85: was a hardcoded 60. On a 144/145 Hz desktop a mode list
            // filtered by the current refresh rate would silently drop our
            // entry - one more way to be rejected without an error message.
            out->RefreshRate = g_realRefresh ? g_realRefresh : 60;
            out->Format = fmt;
            static int told = 0;
            if (told < 4) { told++;
                Log("res: handed the game our %ux%u@%u mode (slot %u), "
                    "read from %p", g_forceResW, g_forceResH,
                    out->RefreshRate, mode, _ReturnAddress()); }
            return D3D_OK;
        }
    }
    return g_origEAM ? g_origEAM(self, adapter, fmt, mode, out) : D3DERR_INVALIDCALL;
}
