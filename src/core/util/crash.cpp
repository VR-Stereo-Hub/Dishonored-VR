#define DVR_CAT ::dvr::log::Cat::crash
#include "core/util/crash.h"
#include "core/util/log.h"
#include "core/util/mem.h"
#include "core/util/paths.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dbghelp.h>

namespace dvr::crash {
namespace {

bool   g_installed = false;
LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
LPTOP_LEVEL_EXCEPTION_FILTER g_ours = nullptr;
volatile LONG g_teardown = 0;
volatile LONG g_faults = 0;
HANDLE g_crashFile = INVALID_HANDLE_VALUE;

struct NamedThread { char name[16]; DWORD tid; };
NamedThread g_threads[8];
int g_threadCount = 0;

// Both destinations, no heap: a static line buffer, WriteFile for the crash
// text and the regular log for context (best effort - it takes a lock and
// uses stdio, which is what the original fingerprinter did too).
char g_line[512];

void emit(const char* fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(g_line, sizeof(g_line) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    if (n > (int)sizeof(g_line) - 2) n = (int)sizeof(g_line) - 2;
    if (g_crashFile == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH];
        dvr::paths::in_game_dir(path, "dishonored_vr_crash.txt");
        g_crashFile = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                  OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (g_crashFile != INVALID_HANDLE_VALUE) {
        g_line[n] = '\r'; g_line[n + 1] = '\n';
        DWORD wrote = 0;
        WriteFile(g_crashFile, g_line, (DWORD)n + 2, &wrote, nullptr);
        g_line[n] = 0;
    }
    dvr::log::write(dvr::log::Cat::crash, dvr::log::Level::Error, "%s", g_line);
}

const char* thread_name(DWORD tid)
{
    for (int i = 0; i < g_threadCount; i++)
        if (g_threads[i].tid == tid) return g_threads[i].name;
    return "other";
}

void module_of(const void* addr, char* name, size_t n, uintptr_t* base)
{
    name[0] = '?'; name[1] = 0; *base = 0;
    HMODULE hm = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &hm) && hm) {
        char full[MAX_PATH] = "?";
        GetModuleFileNameA(hm, full, MAX_PATH);
        const char* bs = strrchr(full, '\\');
        strncpy(name, bs ? bs + 1 : full, n - 1); name[n - 1] = 0;
        *base = (uintptr_t)hm;
    }
}

// 37.4 / 38.11 / 38.17: the fingerprinter. Names the faulting module and
// address for real faults (codes >= 0xC0000000), the thread (pace, present or
// someone else's worker), the registers, and every pointer-looking value in
// the top stack slots resolved to its module - a caller inside ANY DLL (VDXR,
// dxvk, d3d11, the driver) is visible, not just Dishonored.exe.
LONG WINAPI fingerprint(EXCEPTION_POINTERS* ep)
{
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if ((code & 0xF0000000u) != 0xC0000000u) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_faults) > 3) return EXCEPTION_CONTINUE_SEARCH;

    if (g_teardown) {
        emit("fault 0x%08lx during teardown at %p - the game's own exit path; ignoring",
             (unsigned long)code, ep->ExceptionRecord->ExceptionAddress);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    void* addr = ep->ExceptionRecord->ExceptionAddress;
    char mod[64]; uintptr_t base;
    module_of(addr, mod, sizeof(mod), &base);
    DWORD tid = GetCurrentThreadId();
    emit("EXCEPTION 0x%08lx at %p [%s+0x%lx] tid=%lu (%s)",
         (unsigned long)code, addr, mod, (unsigned long)((uintptr_t)addr - base),
         (unsigned long)tid, thread_name(tid));
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        emit("  access violation %s %p",
             ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
             (void*)ep->ExceptionRecord->ExceptionInformation[1]);
    if (ep->ContextRecord) {
        const CONTEXT* c = ep->ContextRecord;
        emit("  eax=%08lx ecx=%08lx edx=%08lx ebx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx",
             c->Eax, c->Ecx, c->Edx, c->Ebx, c->Esi, c->Edi, c->Ebp, c->Esp);
        const uint32_t* sp = (const uint32_t*)c->Esp;
        for (int i = 0; i < 24; i++) {
            if (!dvr::mem::range_readable(sp + i, 4)) break;
            uint32_t v = sp[i];
            if (v < 0x10000) continue;
            char vm[64]; uintptr_t vb;
            module_of((const void*)(uintptr_t)v, vm, sizeof(vm), &vb);
            if (vb) emit("  esp[%2d] = 0x%08lx  [%s+0x%lx]", i, (unsigned long)v, vm, (unsigned long)(v - vb));
        }
        int shown = 0;
        for (int i = 0; i < 512 && shown < 10; i++) {
            if (!dvr::mem::range_readable(sp + i, 4)) break;
            uint32_t v = sp[i];
            if (v >= 0x401000 && v < 0x1800000) { emit("  stack[%d] = 0x%08lx", i, (unsigned long)v); shown++; }
        }
    }
    dvr::log::flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

typedef BOOL (WINAPI *PFN_MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

LONG WINAPI unhandled(EXCEPTION_POINTERS* ep)
{
    if (!g_teardown) {
        HMODULE dbg = LoadLibraryA("dbghelp.dll");
        PFN_MiniDumpWriteDump write = dbg ? (PFN_MiniDumpWriteDump)GetProcAddress(dbg, "MiniDumpWriteDump") : nullptr;
        if (write) {
            SYSTEMTIME st; GetLocalTime(&st);
            char path[MAX_PATH];
            snprintf(path, sizeof(path), "%s\\dvr_%04u%02u%02u_%02u%02u%02u.dmp", dvr::paths::dumps_dir(),
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
            HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), ep, FALSE };
                BOOL ok = write(GetCurrentProcess(), GetCurrentProcessId(), f,
                                (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
                                ep ? &mei : nullptr, nullptr, nullptr);
                CloseHandle(f);
                emit("minidump %s: %s", ok ? "written" : "FAILED", path);
            }
        }
        dvr::log::flush();
    }
    if (g_previous && g_previous != g_ours) return g_previous(ep);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void install()
{
    if (g_installed) return;
    g_installed = true;
    AddVectoredExceptionHandler(1, fingerprint);
    g_ours = unhandled;
    g_previous = SetUnhandledExceptionFilter(g_ours);
    DVR_INFO("crash handler installed (fingerprint VEH + minidump filter; dumps in %s)", dvr::paths::dumps_dir());
}

void rearm()
{
    if (!g_installed) return;
    LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(g_ours);
    if (cur != g_ours) {
        if (cur) g_previous = cur;   // chain to whoever displaced us
        DVR_LOG_ONCE(DVR_CAT, dvr::log::Level::Info, "crash filter re-armed (something displaced it)");
    }
}

void register_thread(const char* name, DWORD tid)
{
    for (int i = 0; i < g_threadCount; i++)
        if (!strcmp(g_threads[i].name, name)) { g_threads[i].tid = tid; return; }
    if (g_threadCount < 8) {
        strncpy(g_threads[g_threadCount].name, name, 15);
        g_threads[g_threadCount].name[15] = 0;
        g_threads[g_threadCount].tid = tid;
        g_threadCount++;
    }
}

void note_teardown(const char* why)
{
    if (InterlockedExchange(&g_teardown, 1) == 0)
        DVR_INFO("teardown noted (%s): later faults get one line and no dump", why);
}

bool teardown_seen() { return g_teardown != 0; }

} // namespace dvr::crash
