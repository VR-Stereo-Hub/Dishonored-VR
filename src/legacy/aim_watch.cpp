// legacy/aim_watch.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static LONG CALLBACK AimWatchVeh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        CONTEXT* c = ep->ContextRecord;
        if (c->Dr6 & 0x1) {                       // our DR0
            uint32_t eip = (uint32_t)c->Eip;
            c->Dr6 = 0;
            InterlockedIncrement(&g_awTotal);
            if (eip >= g_awSelfLo && eip < g_awSelfHi)
                return EXCEPTION_CONTINUE_EXECUTION;   // our own writes
            LONG n = g_awRecN; if (n > 8) n = 8;
            for (LONG i = 0; i < n; i++)
                if (g_awRecs[i].eip == eip) { g_awRecs[i].n++; return EXCEPTION_CONTINUE_EXECUTION; }
            LONG idx = InterlockedIncrement(&g_awRecN) - 1;
            if (idx < 8) {
                AwRec* r = &g_awRecs[idx];
                r->eip = eip; r->n = 1;
                r->ret[0] = r->ret[1] = r->ret[2] = 0;
                uint32_t* sp = (uint32_t*)c->Esp;
                int got = 0;
                for (int k = 0; k < 24 && got < 3; k++) {
                    uint32_t v;
                    if (!RangeReadable(sp + k, 4)) break;
                    v = sp[k];
                    if (v >= 0x401000 && v < 0xf40000) r->ret[got++] = v;
                }
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


static void AimWatchApply(bool enable)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    if (Thread32First(snap, &te)) do {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE th = (te.th32ThreadID == self) ? GetCurrentThread()
            : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT |
                         THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!th) continue;
        bool other = te.th32ThreadID != self;
        if (other) SuspendThread(th);
        CONTEXT c; memset(&c, 0, sizeof(c));
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext(th, &c)) {
            if (enable) {
                c.Dr0 = (DWORD)g_awAddr;
                // L0 on, DR0 write-only (RW=01), 4-byte (LEN=11)
                c.Dr7 = (c.Dr7 & ~(0x3u | (0xFu << 16))) | 0x1u | (0x1u << 16) | (0x3u << 18);
            } else {
                c.Dr0 = 0;
                c.Dr7 &= ~(0x3u | (0xFu << 16));
            }
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            SetThreadContext(th, &c);
        }
        if (other) ResumeThread(th);
        if (other) CloseHandle(th);
    } while (Thread32Next(snap, &te));
    CloseHandle(snap);
}


static void AimWatchArm(uint8_t* obj)
{
    if (!obj) return;
    if (!g_awSelfLo) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)&AimWatchArm, &mbi, sizeof(mbi))) {
            g_awSelfLo = (uintptr_t)mbi.AllocationBase;
            g_awSelfHi = g_awSelfLo + 0x100000;
        }
    }
    if (!g_awVeh) g_awVeh = AddVectoredExceptionHandler(1, AimWatchVeh);
    g_awObj  = obj;
    g_awAddr = (uintptr_t)obj + 0xa0;       // rotation yaw int32
    g_awRecN = 0; g_awTotal = 0;
    AimWatchApply(true);
    Log("aimtrace: write-watch ARMED on pooled projectile %p rot-yaw", (void*)obj);
}


static void AimWatchReport(const char* why)
{
    if (!g_awAddr) return;
    AimWatchApply(false);
    g_awAddr = 0;
    LONG n = g_awRecN; if (n > 8) n = 8;
    Log("aimtrace: write-watch report (%s) - %ld writer(s):", why, (long)n);
    for (LONG i = 0; i < n; i++)
        Log("aimtrace:   eip=0x%08x hits=%u  ret=0x%08x 0x%08x 0x%08x",
            g_awRecs[i].eip, g_awRecs[i].n,
            g_awRecs[i].ret[0], g_awRecs[i].ret[1], g_awRecs[i].ret[2]);
    if (!n) Log("aimtrace:   (no non-self writers - rotation NOT rewritten at fire?)");
}
