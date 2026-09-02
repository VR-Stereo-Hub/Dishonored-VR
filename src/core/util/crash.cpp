#define DVR_CAT ::dvr::log::Cat::crash
#include "core/util/crash.h"
#include "core/util/log.h"
#include "core/util/mem.h"
#include "core/util/paths.h"
#include "dvr_version.h"

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

// 40.2 A MEASUREMENT CARRIES THE IDENTITY OF WHAT IT MEASURED.
// dishonored_vr_crash.txt is opened FILE_APPEND_DATA / OPEN_ALWAYS, so it
// accumulates across every run forever with nothing separating them. Three
// fingerprints sat in it for a session and could not be attributed to a build,
// a backend or a runtime - the simulator and VDXR produce byte-identical text.
// The file now opens with one header naming the run.
char g_ctx[128] = "backend not up yet";
bool g_headerDone = false;

bool write_dump(EXCEPTION_POINTERS* ep, const char* why);   // defined below

void context(const char* text)
{
    if (!text) return;
    strncpy(g_ctx, text, sizeof(g_ctx) - 1);
    g_ctx[sizeof(g_ctx) - 1] = 0;
}

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
        if (g_crashFile != INVALID_HANDLE_VALUE && !g_headerDone) {
            g_headerDone = true;
            SYSTEMTIME st; GetLocalTime(&st);
            char hd[512];
            int hn = snprintf(hd, sizeof(hd) - 2,
                "\r\n===== run %04u-%02u-%02u %02u:%02u:%02u  "
                "dishonoredvr %s build %s  pid=%lu  %s =====",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
                DVR_VERSION, DVR_BUILD_ID, (unsigned long)GetCurrentProcessId(), g_ctx);
            if (hn > 0) {
                hd[hn] = '\r'; hd[hn + 1] = '\n';
                DWORD hw = 0;
                WriteFile(g_crashFile, hd, (DWORD)hn + 2, &hw, nullptr);
            }
        }
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
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2) {
        // 40.2 THE OPERATION IS THREE-VALUED, NOT TWO. ExceptionInformation[0]
        // is 0 read, 1 write, 8 execute (DEP). The old line tested it for
        // truth, so an EXECUTE fault - the instruction pointer itself landing
        // in freed memory - printed as "writing", and three identical
        // fingerprints reading "access violation writing DEDEDEDE" were taken
        // for a stray store into a freed D3D11 object. They are the opposite
        // fault: a CALL through a poisoned function pointer. Decode all three,
        // and say so on the line when the faulting address IS the instruction
        // pointer, because that combination has exactly one meaning.
        const ULONG_PTR op = ep->ExceptionRecord->ExceptionInformation[0];
        const void* at = (const void*)ep->ExceptionRecord->ExceptionInformation[1];
        const char* what = op == 0 ? "reading" : op == 1 ? "writing"
                         : op == 8 ? "EXECUTING" : "accessing";
        emit("  access violation %s %p%s", what, at,
             (at == ep->ExceptionRecord->ExceptionAddress)
                 ? "  <== this address IS the instruction pointer: control was"
                   " transferred INTO this memory, so a code pointer we called"
                   " through was already freed. Not a data write."
                 : "");
    }
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
    // 40.2 THE WILD-EIP GATE. This handler is the only one that reliably runs
    // (the unhandled filter never fired in three measured crashes), so the
    // dump has to be taken here - but a VEH sees FIRST-CHANCE exceptions,
    // including the ones UE3 raises and handles on purpose, and dumping on
    // those would cost a multi-hundred-MB write mid-gameplay for a fault that
    // was never fatal.
    //
    // base == 0 means module_of() could not place the instruction pointer in
    // ANY loaded image. Nothing recovers from that: no __except frame can
    // resume a thread whose EIP is in freed or unmapped memory, so this
    // condition cannot be true for a handled probe. It is exactly the measured
    // signature (EIP == 0xDEDEDEDE, module "?"), which makes this gate both
    // fatal-only and able to fail its own hypothesis: if the freeze crash ever
    // turns out to be an ordinary in-module fault, no dump appears and the
    // wild-EIP theory is falsified by the silence.
    if (!base) {
        emit("  the instruction pointer is in NO loaded module - unrecoverable by "
             "construction (no handler can resume from it), so this fault is fatal "
             "and gets the dump");
        write_dump(ep, "wild instruction pointer");
    }
    dvr::log::flush();
    return EXCEPTION_CONTINUE_SEARCH;
}

typedef BOOL (WINAPI *PFN_MiniDumpWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

// 40.2 dbghelp is resolved ONCE, at install time, on the game thread. The
// dump can now be taken from the vectored handler, and a VEH must never call
// LoadLibrary: it takes the loader lock, and a fault raised while that lock is
// held would deadlock instead of producing the dump we came for.
PFN_MiniDumpWriteDump g_miniDump = nullptr;
volatile LONG g_dumpDone = 0;

// One dump per run, from whichever handler gets there first. `why` names the
// path that decided this fault was fatal, so the .dmp can be tied back to the
// line in dishonored_vr_crash.txt that produced it.
bool write_dump(EXCEPTION_POINTERS* ep, const char* why)
{
    if (!g_miniDump || g_teardown) return false;
    if (InterlockedExchange(&g_dumpDone, 1) != 0) return false;
    SYSTEMTIME st; GetLocalTime(&st);
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s\\dvr_%04u%02u%02u_%02u%02u%02u.dmp", dvr::paths::dumps_dir(),
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        emit("minidump FAILED to create %s (err %lu) - is %s writable?",
             path, (unsigned long)GetLastError(), dvr::paths::dumps_dir());
        return false;
    }
    MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), ep, FALSE };
    BOOL ok = g_miniDump(GetCurrentProcess(), GetCurrentProcessId(), f,
                         (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
                         ep ? &mei : nullptr, nullptr, nullptr);
    CloseHandle(f);
    emit("minidump %s (%s): %s", ok ? "written" : "FAILED", why, path);
    return ok != FALSE;
}

LONG WINAPI unhandled(EXCEPTION_POINTERS* ep)
{
    if (!g_teardown) {
        write_dump(ep, "unhandled-exception filter");
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
    {   // 40.2: resolve dbghelp here, never inside a handler (loader lock).
        HMODULE dbg = LoadLibraryA("dbghelp.dll");
        if (dbg) g_miniDump = (PFN_MiniDumpWriteDump)GetProcAddress(dbg, "MiniDumpWriteDump");
    }
    AddVectoredExceptionHandler(1, fingerprint);
    g_ours = unhandled;
    g_previous = SetUnhandledExceptionFilter(g_ours);
    // Say which handler can actually produce a dump. Measured 2026-09-01: the
    // filter below never ran - three fingerprints in dishonored_vr_crash.txt,
    // zero minidump lines, dumps\ empty - because UE3's own filter or an SEH
    // frame consumes the fault first. The VEH always runs, so the wild-EIP
    // gate in fingerprint() is the path that will actually fire.
    DVR_INFO("crash handler installed (fingerprint VEH%s + unhandled filter; dumps in %s)",
             g_miniDump ? " with wild-EIP minidump" : " WITHOUT minidump - no dbghelp.dll",
             dvr::paths::dumps_dir());
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

void set_context(const char* text) { context(text); }

void note_teardown(const char* why)
{
    if (InterlockedExchange(&g_teardown, 1) == 0)
        DVR_INFO("teardown noted (%s): later faults get one line and no dump", why);
}

bool teardown_seen() { return g_teardown != 0; }

} // namespace dvr::crash
