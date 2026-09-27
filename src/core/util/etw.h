// core/util/etw.h - the proxy's own ETW markers (TraceLogging provider "DishonoredVR").
//
// WHY THIS EXISTS. The frame is three stages (game thread, render thread, GPU) and the log can only
// print their window MEANS. A GPU timeline (WPR/GPUView, tools/perf-gpu-trace.ps1) shows every GPU
// packet and every thread switch but not which of OUR phases a thread was in. These events put the
// present path's phases (the xrWaitFrame, the capture fence wait, the method, the HUD, xrEndFrame,
// the flush that stands in for the desktop Present) and the game thread's scene draws on the same
// clock as the GPU packets, so a gap on the game's 3D queue can be named by owner.
//
// COST. Nothing unless a trace session enables the provider: every call first tests the provider's
// enabled flag (one load and a branch). With a session listening, one event is ~1 us. There is
// no ini key on purpose: the session IS the switch (`[Perf] Etw=0` exists only as a kill switch).
//
// Provider GUID {6b3c1f4e-2d6a-4f7c-9a51-0d2e8c7b4a19}; tools/wpr/dvr-gpu.wprp enables it.
#pragma once
#include <stdint.h>

namespace dvr::etw {

// Phase ids. Stable numbers: the analysis script (tools/perf-gpu-timeline.py) keys on them.
enum Phase : int32_t {
    kPresent = 1,        // the whole hkPresent (a = present count, b = the eye this present rendered)
    kXrBegin = 2,        // dvr::vr::on_present_begin (xrWaitFrame lives here)
    kGameTick = 3,       // the game side's per-present tick (seam, head, hands)
    kMethod = 4,         // stereo end_frame: capture + the D3D11 conversion
    kCapFence = 5,       // capture: waiting for the D3D9 blit fence of the slot being delivered
    kCapRead = 6,        // capture: waiting for the D3D11 read of the slot about to be blitted
    kHud = 7,            // hudcap end_frame
    kXrEnd = 8,          // dvr::vr::on_present_end (xrEndFrame lives here)
    kDeskPresent = 9,    // the game's Present or the flush that replaces it
    kSceneDraw = 10,     // game thread: one eye's viewport draw (a = eye)
    kHudFence = 11,      // hudcap: waiting on its own blit fence
};

bool enabled();                       // a session is listening
void begin(Phase p, int64_t a = 0, int64_t b = 0);
void end(Phase p, int64_t a = 0, int64_t b = 0);
// A named instant: seam commands, A/B segment boundaries, a lever switching.
void mark(const char* text, int64_t a = 0);
// The render thread's frame-start marker (first BeginScene after a present).
void frame_start(uint32_t present);
void set_killed(bool killed);         // [Perf] Etw=0

struct Scope {
    Phase p; int64_t a, b; bool on;
    Scope(Phase ph, int64_t aa = 0, int64_t bb = 0) : p(ph), a(aa), b(bb), on(enabled()) { if (on) begin(p, a, b); }
    ~Scope() { if (on) end(p, a, b); }
};

void init();       // DllMain: registers the provider (never fails the load)
void shutdown();

} // namespace dvr::etw
