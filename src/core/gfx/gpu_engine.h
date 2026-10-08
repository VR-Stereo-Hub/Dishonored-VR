// core/gfx/gpu_engine.h - who is using the GPU, by process, when a frame stalls (2026-10-07).
//
// A stall that sits in xrEndFrame with the game's own GPU time inflated says the GPU is
// busy, not WHO is busy: this game (its D3D9 frame and the mod's D3D11 work share one
// process), the DLSS helper, the VR streamer's encoder, the compositor. A headset run on
// 2026-10-07 had every xrEndFrame blocking about 85 ms for the rest of the session and
// nothing on disk could tell those apart (FLICKER_REFERENCE, the keyhole stall entry).
//
// This reads Windows' own per-engine utilization counters (`\GPU Engine(*)\Utilization
// Percentage`, what Task Manager's GPU column reads), twice 500 ms apart on its own
// low-priority thread, and logs the busiest processes by engine type with this game's
// share named. Read-only; it runs only when a frame-gap report asks for it, at most once
// every 10 s, and follows [Perf] GpuMem (`gpumem off` stops it too).
#pragma once

namespace dvr::gpu_engine {
// Any thread. Starts one sample in the background unless one ran in the last 10 s.
void request(const char* why);
}
