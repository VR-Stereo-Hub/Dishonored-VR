// legacy/camera_tracer.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static LONG CALLBACK CameraVeh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        CONTEXT* c = ep->ContextRecord;
        if (c->Dr6 & 0x1) {                       // DR0 (our watch) fired
            // 32.23: DEDUPE ON INSERT. The 24 slots used to fill with repeats
            // of whatever fires most often, and the run stopped at 200 hits -
            // which is 1.4 s of holding an aim. The release, the one moment
            // the trace exists to capture, happened after the buffer was full
            // and the trace had already reported. Distinct EIPs are the whole
            // signal; repeats are noise. Now a slot is only spent on a NEW
            // instruction, so a reader that appears once, late, still lands.
            uint32_t eipNow = (uint32_t)c->Eip;
            LONG have = g_hitN; if (have > 24) have = 24;
            for (LONG q = 0; q < have; q++)
                if (g_hits[q].eip == eipNow) {
                    InterlockedIncrement(&g_traceCount);
                    c->Dr6 = 0;
                    return EXCEPTION_CONTINUE_EXECUTION;
                }
            LONG idx = InterlockedIncrement(&g_hitN) - 1;
            if (idx < 24) {
                g_hits[idx].eip = eipNow;
                // walk EBP chain for a few return addresses
                uintptr_t ebp = c->Ebp;
                for (int k = 0; k < 4; k++) {
                    uint32_t ret = 0, next = 0;
                    if (ebp && SafeRead32(ebp + 4, &ret)) g_hits[idx].ret[k] = ret;
                    else { g_hits[idx].ret[k] = 0; break; }
                    if (!SafeRead32(ebp, &next) || next <= ebp) break;
                    ebp = next;
                }
            }
            InterlockedIncrement(&g_traceCount);
            c->Dr6 = 0;                            // clear status
            return EXCEPTION_CONTINUE_EXECUTION;   // resume; the store already ran
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


static void SetHwBreakAllThreads(uintptr_t addr, bool enable)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    DWORD myPid = GetCurrentProcessId();
    DWORD myTid = GetCurrentThreadId();
    if (Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID != myPid) continue;
            if (te.th32ThreadID == myTid) continue; // NEVER suspend ourselves!
            HANDLE h = OpenThread(THREAD_SET_CONTEXT | THREAD_GET_CONTEXT |
                                  THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
            if (!h) continue;
            SuspendThread(h);
            CONTEXT c; memset(&c, 0, sizeof(c));
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(h, &c)) {
                if (enable) {
                    c.Dr0 = addr;
                    // L0=1; RW0=01 (write); LEN0=11 (4 bytes) -> nibble at bit16 = 0xD
                    c.Dr7 = (c.Dr7 & ~0xFuLL) | 0x1;
                    c.Dr7 = (c.Dr7 & ~(0xFuLL << 16)) | ((uint64_t)g_hwNibble << 16);
                } else {
                    c.Dr0 = 0;
                    c.Dr7 &= ~0x1uLL;
                }
                c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                SetThreadContext(h, &c);
            }
            ResumeThread(h);
            CloseHandle(h);
        } while (Thread32Next(snap, &te));
    }
    CloseHandle(snap);

    // also set on the CURRENT thread (no suspend needed for self) in case the
    // game renders single-threaded and the camera write lands on our thread
    CONTEXT c; memset(&c, 0, sizeof(c));
    c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE me = GetCurrentThread();
    if (GetThreadContext(me, &c)) {
        if (enable) {
            c.Dr0 = addr;
            c.Dr7 = (c.Dr7 & ~0xFuLL) | 0x1;
            c.Dr7 = (c.Dr7 & ~(0xFuLL << 16)) | ((uint64_t)g_hwNibble << 16);
        } else {
            c.Dr0 = 0; c.Dr7 &= ~0x1uLL;
        }
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        SetThreadContext(me, &c);
    }
}


static void ArmCameraTrace()
{
    if (g_traceState != 0) return;
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!g_camObj) { Log("trace: no camera yet - load a level, then press F4"); return; }

    // watch the camera world-position X float (written every frame by the
    // camera update). +0x80 = matrix translation row / POV location.
    g_watchAddr = (uintptr_t)(g_camObj + kCamLoc0);
    g_hitN = 0; g_traceCount = 0;
    if (!g_veh) g_veh = AddVectoredExceptionHandler(1, CameraVeh);
    SetHwBreakAllThreads(g_watchAddr, true);
    g_traceState = 1;
    g_traceArmFrame = g_frame;
    Log("trace: ARMED hw-breakpoint on camera matrix @ 0x%08x (writes -> camera fn)",
        (unsigned)g_watchAddr);
}


static void ReportCameraTrace()
{
    SetHwBreakAllThreads(g_watchAddr, false);
    LONG n = g_hitN; if (n > 24) n = 24;
    Log("trace: ---- %ld accesses, %ld DISTINCT instructions ----",
        (long)g_traceCount, (long)n);
    // de-dup EIPs
    uint32_t seen[24]; int ns = 0;
    for (LONG i = 0; i < n; i++) {
        uint32_t e = g_hits[i].eip;
        bool dup = false;
        for (int k = 0; k < ns; k++) if (seen[k] == e) { dup = true; break; }
        if (dup) continue;
        seen[ns++] = e;
        Log("trace: WRITE eip=0x%08x  callers: %08x %08x %08x %08x",
            e, g_hits[i].ret[0], g_hits[i].ret[1], g_hits[i].ret[2], g_hits[i].ret[3]);
    }
    if (g_traceIsBlink && g_traceCount == 0)
        Log("trace: ZERO writes caught - the aim was never held, or the "
            "PowerBlink object was replaced. Re-arm and hold the aim.");
    Log("trace: ---- end (base 0x400000; subtract for file offset) ----");
    g_traceState = 2;
    g_traceIsBlink = false;
}


// Called each frame from Present. Auto-arms once in a level, or on F4; reports
// after ~1.5s of collecting.
static void CameraTraceTick()
{
    bool f4 = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
    static bool f4was = false;
    bool f4edge = f4 && !f4was; f4was = f4;

    // 32.18: a Blink-aim trace request beats the F4 camera trace to the draw.
    if ((g_traceState == 0 || g_traceState == 2) &&
        InterlockedExchange(&g_blkReadReq, 0)) {
        if (!BlkAlive()) Log("trace: no live PowerBlink yet");
        else {
            g_hwNibble = 0xF;                       // read OR write
            g_watchAddr = (uintptr_t)(g_blkObj + 0x060);
            g_hitN = 0; g_traceCount = 0;
            if (!g_veh) g_veh = AddVectoredExceptionHandler(1, CameraVeh);
            SetHwBreakAllThreads(g_watchAddr, true);
            g_traceState = 1; g_traceArmFrame = g_frame; g_traceIsBlink = true;
            g_hitLogged = 0; g_traceStopAt = MaimNowMs() + 40000.0;
            Log("trace: READ+WRITE watch ARMED on PowerBlink+0x060 - aim a "
                "Blink AND ACTUALLY BLINK. Any instruction that READS the "
                "destination at release will be named.");
        }
    }
    else if ((g_traceState == 0 || g_traceState == 2) &&
        InterlockedExchange(&g_blkTraceReq, 0)) {
        g_hwNibble = 0xD;
        if (!BlkAlive()) {
            Log("trace: no live PowerBlink yet - equip Blink and stand in "
                "gameplay for a few seconds, then ask again");
        } else {
            g_watchAddr = (uintptr_t)(g_blkObj + 0x060);
            g_hitN = 0; g_traceCount = 0;
            if (!g_veh) g_veh = AddVectoredExceptionHandler(1, CameraVeh);
            SetHwBreakAllThreads(g_watchAddr, true);
            g_traceState = 1;
            g_traceArmFrame = g_frame;
            g_traceIsBlink = true;
            Log("trace: ARMED on PowerBlink+0x060 @ 0x%08x - you have ~15 s to "
                "close the overlay and HOLD A BLINK AIM.",
                (unsigned)g_watchAddr);
        }
    }
    else if (g_traceState == 0 && f4edge)   // F4-only: arm when in a level
        ArmCameraTrace();
    // The camera writes every frame, so 150 frames was plenty for it. The
    // Blink aim only writes while you HOLD an aim, and 150 frames is about a
    // second at this framerate - not enough time to close the overlay, let
    // alone equip and hold. It would have reported an empty result and looked
    // like a negative. Give the Blink trace a real window.
    else if (g_traceState == 1 && g_traceIsBlink) {
        // stream new instructions out as they are found
        LONG n = g_hitN; if (n > 24) n = 24;
        while (g_hitLogged < n) {
            const TraceHit& h = g_hits[g_hitLogged];
            Log("trace: [%ld] eip=0x%08x  callers: %08x %08x %08x %08x",
                (long)g_hitLogged, h.eip, h.ret[0], h.ret[1], h.ret[2], h.ret[3]);
            g_hitLogged++;
        }
        if (n >= 24 || MaimNowMs() > g_traceStopAt) ReportCameraTrace();
    }
    else if (g_traceState == 1 &&
             (g_traceCount >= 200 || (g_frame - g_traceArmFrame) > 150u))
        ReportCameraTrace();
    else if (g_traceState == 2 && f4edge) {   // allow re-run on F4
        g_traceState = 0;
    }
}


// remove the VEH cleanly on shutdown
static void CameraTraceShutdown()
{
    if (g_traceState == 1 && g_watchAddr) SetHwBreakAllThreads(g_watchAddr, false);
    if (g_veh) { RemoveVectoredExceptionHandler(g_veh); g_veh = NULL; }
}
