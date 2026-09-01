// core/util/crash.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).



// 37.4: crash fingerprinter. The first XR flight died silently ~6 s in -
// backend live, frames flowing, then nothing in the log. This names the
// faulting module and address for real faults (codes >= 0xC0000000) before
// the process goes down, then lets the crash proceed. XR mode only.
static LONG WINAPI XrVeh(EXCEPTION_POINTERS* ep)
{
    static LONG logged = 0;
    if (ep && ep->ExceptionRecord &&
        (ep->ExceptionRecord->ExceptionCode & 0xF0000000u) == 0xC0000000u &&
        InterlockedIncrement(&logged) <= 3) {
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        char mod[MAX_PATH] = "?";
        HMODULE hm = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)addr, &hm) && hm)
            GetModuleFileNameA(hm, mod, MAX_PATH);
        // 38.11: thread identity - is it the pace thread, the present
        // thread, or someone else's worker?
        Log("xr: EXCEPTION 0x%08lx at %p [%s] tid=%lu (pace=%lu present=%lu)",
            (unsigned long)ep->ExceptionRecord->ExceptionCode, addr, mod,
            (unsigned long)GetCurrentThreadId(),
            (unsigned long)g_xrPaceTid, (unsigned long)g_presentTid);
        if (ep->ExceptionRecord->ExceptionCode == 0xC0000005 &&
            ep->ExceptionRecord->NumberParameters >= 2)
            Log("xr:   access violation %s %p",
                ep->ExceptionRecord->ExceptionInformation[0] ? "writing"
                                                             : "reading",
                (void*)ep->ExceptionRecord->ExceptionInformation[1]);
        if (ep->ContextRecord) {
            // 38.11: registers - the shape of the dead state
            Log("xr:   eax=%08lx ecx=%08lx edx=%08lx ebx=%08lx esi=%08lx "
                "edi=%08lx ebp=%08lx esp=%08lx",
                ep->ContextRecord->Eax, ep->ContextRecord->Ecx,
                ep->ContextRecord->Edx, ep->ContextRecord->Ebx,
                ep->ContextRecord->Esi, ep->ContextRecord->Edi,
                ep->ContextRecord->Ebp, ep->ContextRecord->Esp);
            uint32_t* sp = (uint32_t*)ep->ContextRecord->Esp;
            // 38.11: NAME THE CALLER. For a CALL to a null pointer the
            // return address is AT [esp]. The old scanner only recognized
            // Dishonored.exe addresses, so a caller inside ANY DLL (VDXR,
            // dxvk, d3d11, driver) was invisible - that blind spot cost
            // two theories. Resolve the module of every pointer-looking
            // value in the top 24 slots, whatever module it lives in.
            for (int i = 0; i < 24; i++) {
                if (!RangeReadable(sp + i, 4)) break;
                uint32_t v = sp[i];
                if (v < 0x10000) continue;
                HMODULE vm = NULL;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        (LPCSTR)(uintptr_t)v, &vm) && vm) {
                    char vmn[MAX_PATH] = "?";
                    GetModuleFileNameA(vm, vmn, MAX_PATH);
                    const char* bs = strrchr(vmn, '\\');
                    Log("xr:   esp[%2d] = 0x%08lx  [%s+0x%lx]", i,
                        (unsigned long)v, bs ? bs + 1 : vmn,
                        (unsigned long)(v - (uint32_t)(uintptr_t)vm));
                }
            }
            // the original exe-range deep scan stays (return-address trail)
            int shown = 0;
            for (int i = 0; i < 512 && shown < 10; i++) {
                if (!RangeReadable(sp + i, 4)) break;
                uint32_t v = sp[i];
                if (v >= 0x401000 && v < 0x1800000) {
                    Log("xr:   stack[%d] = 0x%08lx", i, (unsigned long)v);
                    shown++;
                }
            }
        }
        LogFlush();   // 38.90: the log is buffered now - never lose a crash
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
