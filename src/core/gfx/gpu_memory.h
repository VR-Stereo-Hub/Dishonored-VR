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
// Read-only. VR-160: the sampling runs on its OWN low-priority thread at 1 Hz.
// It used to run on the present thread at 4 Hz, and one sample is two kernel
// video-memory queries plus a VirtualQuery walk of the whole 32-bit address
// space: about 21 ms, measured (`perf parts`: 0.8 ms per present at 108
// presents/s). Four 21 ms stalls a second on the thread that presents was the
// `sat in: game_tick` frame gap. tick() now only starts the thread; log_now()
// prints the current sample, the last 16 s and what a sample costs.
// Lever: [Perf] GpuMem=1|0, `gpumem on|off` (on = the thread samples).
#pragma once
#include <windows.h>

namespace dvr::gpu_memory {
void tick();                     // present thread, every present: starts the sampler thread once
void set_enabled(bool on);       // [Perf] GpuMem=, `gpumem on|off`
bool enabled();
void log_now(const char* why);   // current sample + the last 16 samples' range
}
