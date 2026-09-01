// core/util/crash.h - crash fingerprinting and minidumps.
//
// A vectored handler names the faulting module, address, thread and the first
// return addresses on the stack for every fatal exception code, into the log
// AND into dishonored_vr_crash.txt (written with WriteFile, no CRT, because the
// heap may be the thing that broke). The unhandled-exception filter then
// writes a minidump to <data_dir>\dumps and chains to whoever was installed
// before us (the game's own handler).
#pragma once
#include <windows.h>

namespace dvr::crash {
void install();                                   // idempotent; first Direct3DCreate9, not DllMain
void rearm();                                     // the Steam overlay and the game displace filters; call from Present
void register_thread(const char* name, DWORD tid); // "present", "xr-pace": named in the fingerprint
void note_teardown(const char* why);              // game announced exit: faults after this get one line, no dump
bool teardown_seen();
} // namespace dvr::crash
