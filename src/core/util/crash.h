// core/util/crash.h - crash fingerprinting and minidumps.
//
// A vectored handler names the faulting module, address, thread and the first
// return addresses on the stack for every fatal exception code, into the log
// AND into dishonored_vr_crash.txt (written with WriteFile, no CRT, because the
// heap may be the thing that broke). Every run that faults gets a header line
// naming the build, pid and VR runtime, because the simulator and VDXR produce
// byte-identical fingerprint text.
//
// The minidump goes to <data_dir>\dumps from EITHER handler, once per run.
// 40.2: the unhandled-exception filter is no longer the only path to it.
// Measured 2026-09-01: three fingerprints, zero dumps, because UE3's own filter
// or an SEH frame consumes the fault before SetUnhandledExceptionFilter can
// fire. The vectored handler always runs, so it takes the dump too - gated on
// the instruction pointer resolving to no loaded module, a condition no
// recoverable exception can satisfy.
#pragma once
#include <windows.h>
#include <stdint.h>

namespace dvr::crash {
// Opt-in read-only diagnostic. Verify instruction bytes before publishing a
// first-chance AV/read target; address=0 disables. A match captures full memory
// once, then continues normal exception handling. Does not repair engine state.
bool configure_read_fault_dump(uintptr_t address, const uint8_t* expected, size_t count);
void install();                                   // idempotent; first Direct3DCreate9, not DllMain
void rearm();                                     // the Steam overlay and the game displace filters; call from Present
void register_thread(const char* name, DWORD tid); // "present", "xr-pace": named in the fingerprint
void set_context(const char* text);                // "openxr/VirtualDesktopXR": named in the crash file's run header
void note_teardown(const char* why);              // game announced exit: faults after this get one line, no dump
bool teardown_seen();
// VR-177: a fault THIS thread raises inside a guarded probe of our own (a __try that
// expects to fault) is not a crash. Between probe_begin and probe_end the fingerprinter
// ignores this thread's faults entirely: no line, no dump, and none of its 3-fault budget.
// Build 533 spent that budget on the runtime watchdog's stack scans at every startup, so no
// real fault after them could ever be recorded. Nests; thread-local; costs nothing else.
void probe_begin();
void probe_end();
} // namespace dvr::crash
