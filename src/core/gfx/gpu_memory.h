// core/gfx/gpu_memory.h - video and process memory, sampled (PERF 2026-09-18).
//
// The periodic headset stalls sit in xrEndFrame with the game's own GPU time
// normal and its timing queries pending: the GPU is backed up by something
// outside the game's draw work. Memory paging is one candidate that fits, and
// this is its instrument: DXGI's per-process video memory usage against the OS
// budget for the game's adapter (LOCAL = VRAM, NON_LOCAL = system memory the
// GPU reaches over the bus), and the 32-bit process's own private bytes and
// largest free address range.
//
// Read-only. tick() is called once per present on the present thread and
// samples every 250 ms; log_now() prints the current sample and the last 4 s.
#pragma once
#include <windows.h>

namespace dvr::gpu_memory {
void tick();                     // present thread, every present; samples at 4 Hz
void log_now(const char* why);   // current sample + the last 16 samples' range
}
