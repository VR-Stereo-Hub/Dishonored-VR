// Exercise the production handler without launching Dishonored or raising a
// real fault. The final capture uses Windows MiniDumpWriteDump on this host.
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "core/util/log.h"
#include "core/util/paths.h"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT] = {};
void write(Cat, Level, const char*, ...) {}
void flush() {}
}
namespace dvr::paths {
const char* dumps_dir() { return "."; }
const char* in_game_dir(char* out, const char* name) { strcpy_s(out, MAX_PATH, name); return out; }
}
#include "core/util/crash.cpp"
#include "core/util/mem.cpp"
static int failures = 0, checks = 0, calls = 0;
static MINIDUMP_TYPE lastFlags;
static DWORD lastTid;
static EXCEPTION_POINTERS* lastEp;
static BOOL WINAPI fakeDump(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE flags,
    PMINIDUMP_EXCEPTION_INFORMATION mei, PMINIDUMP_USER_STREAM_INFORMATION,
    PMINIDUMP_CALLBACK_INFORMATION) {
    ++calls; lastFlags=flags; lastTid=mei->ThreadId; lastEp=mei->ExceptionPointers;
    return TRUE;
}
static void check(bool ok, const char* name) {
    ++checks; if (!ok) { ++failures; printf("FAIL %s\n", name); }
}
int main() {
    using namespace dvr::crash;
    uint8_t code[] = {0x8b,0x50,0x08,0x8b,0x48,0x0c};
    uintptr_t pc = (uintptr_t)code;
    CONTEXT ctx = {}; ctx.Eip=(DWORD)pc;
    EXCEPTION_RECORD er = {}; er.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    er.ExceptionAddress=(void*)pc; er.NumberParameters=2; er.ExceptionInformation[1]=0x3f800008;
    EXCEPTION_POINTERS ep = {&er, &ctx};
    g_faults=100; // ordinary diagnostic budget already exhausted
    g_miniDump=fakeDump;
    check(fingerprint(&ep)==EXCEPTION_CONTINUE_SEARCH && calls==0,"off by default");
    check(!configure_read_fault_dump(1,code,sizeof(code)),"unreadable target refused");
    uint8_t bad[6] = {};
    check(!configure_read_fault_dump(pc,bad,sizeof(bad)) && !g_readFaultDumpAddress,"mismatch disarms");
    check(!configure_read_fault_dump(pc,code,17),"oversize signature refused");
    check(configure_read_fault_dump(pc,code,sizeof(code)),"matching bytes arm");
    er.ExceptionCode=EXCEPTION_ILLEGAL_INSTRUCTION; fingerprint(&ep);
    check(calls==0,"other exception ignored"); er.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    er.ExceptionInformation[0]=1; fingerprint(&ep); check(calls==0,"write ignored");
    er.ExceptionInformation[0]=8; fingerprint(&ep); check(calls==0,"execute ignored");
    er.ExceptionInformation[0]=0; er.NumberParameters=1; fingerprint(&ep);
    check(calls==0,"short exception ignored"); er.NumberParameters=2;
    ++ctx.Eip; fingerprint(&ep); check(calls==0,"wrong context ignored"); --ctx.Eip;
    er.ExceptionAddress=code+1; fingerprint(&ep); check(calls==0,"wrong fault site ignored");
    er.ExceptionAddress=code; ep.ContextRecord=nullptr; fingerprint(&ep);
    check(calls==0,"missing context ignored"); ep.ContextRecord=&ctx;
    g_teardown=1; fingerprint(&ep); check(calls==0,"teardown ignored"); g_teardown=0;
    check(fingerprint(&ep)==EXCEPTION_CONTINUE_SEARCH && calls==1,"target captures despite exhausted budget, propagates fault");
    check((lastFlags & MiniDumpWithFullMemory)!=0 && lastTid==GetCurrentThreadId() && lastEp==&ep,"full heap and original context supplied");
    fingerprint(&ep); check(calls==1,"only one dump per run");
    configure_read_fault_dump(0,nullptr,0); g_dumpDone=0; fingerprint(&ep);
    check(calls==1,"explicit off prevents capture");
    // A real OS dump must retain a heap page not otherwise passed to the writer.
    auto marker=(char*)VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    strcpy_s(marker,4096,"DVR_INITIAL_FAULT_HEAP_SENTINEL_20260913");
    RtlCaptureContext(&ctx); ctx.Eip=(DWORD)pc; ctx.Eax=0x3f800000; ctx.Edi=(DWORD)marker;
    configure_read_fault_dump(pc,code,sizeof(code));
    HMODULE dbg=LoadLibraryA("dbghelp.dll");
    g_miniDump=(PFN_MiniDumpWriteDump)GetProcAddress(dbg,"MiniDumpWriteDump");
    check(g_miniDump!=nullptr,"real dump writer available");
    if (g_miniDump) fingerprint(&ep);
    printf("sentinel_address=%08lx fault_site=%08lx thread=%lu\n",(DWORD)marker,(DWORD)pc,GetCurrentThreadId());
    printf("crash capture: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
