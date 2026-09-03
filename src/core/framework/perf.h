// core/framework/perf.h - the tick budget (41.1, session 8).
//
// The headset run at the Quest 3 size ticks at 26-28/s and nothing on disk
// says WHERE the tick goes: the capture's cost line counts the LockRect wait
// as "capture", but that wait is also the GPU finishing the frame, so the
// same numbers fit "the render thread is bound by the readback" (a GPU-
// resident capture fixes it) and "the GPU is bound by two 2496x2688 draws"
// (nothing about the capture fixes it). This module is the instrument that
// tells them apart, per present, on the thread that presents:
//
//   the render-thread split   eight QPC stamps in hkPresent: our phases (the
//                             seam poll, the runtime's present-head with its
//                             xrWaitFrame, the game tick, the method with its
//                             capture, the runtime's present-tail) plus the
//                             game's own Present, and OUT = Present return
//                             to the next Present entry (the render thread
//                             executing the frame's commands, or waiting for
//                             the game thread)
//   the GPU timestamps        (commit 3) a D3D9 timestamp ring: the GPU's
//                             frame span and the readback's own GPU cost
//   the marker                (commit 2) the first BeginScene after Present
//                             splits OUT into idle and execution
//   mark / gap                (commits 5-6) the freeze marker and the richer
//                             frame-gap line, from the same ring
//
// One line every 3 s, attributed by eye tag under a two-presents-per-tick
// method: `perf: tick 39.2 ms (25.5/s) = P1[-1] in .. + out .. | P2[+1] ..`.
// It prepends PACE-BOUND when the runtime's wait owns the present (the split
// is then a budget, not a bottleneck), which is the sentence the 1080p
// simulator runs lacked. Every call runs on the present thread; the module
// keeps no lock. `perf on|off|status` on the seam; [Perf] Instruments=.
#pragma once
#include <stdint.h>

namespace dvr::status { class Writer; }

namespace dvr::perf {

// The stamps, in the order hkPresent takes them.
enum Point {
    kEntry = 0,          // hkPresent entry (closes the previous record: its OUT)
    kAfterPre,           // after pre_tick and the FpsCap wait
    kAfterBegin,         // after vr::on_present_begin (xrWaitFrame lives here)
    kAfterTick,          // after game_tick
    kAfterEnd,           // after stereo::end_frame (the capture lives here)
    kAfterPresentEnd,    // after vr::on_present_end (CopyResource, xrEndFrame)
    kBeforeGamePresent,  // before the game's own Present
    kAfterGamePresent,   // after it
    kPointCount
};
void stamp(Point p);

// The lever ([Perf] Instruments=, `perf on|off`): off = no stamps are kept
// and no line prints. Default on: eight QPC reads per present.
void set_enabled(bool on);
bool enabled();

// Print the last 3 s line again now (`perf status`), then the ring's tail.
void log_status();

// status.json "perf" object.
void status(dvr::status::Writer& w);

// The last closed 3 s window, ms per present (0 before the first window).
struct Window {
    float tickMs = 0, ticksPerS = 0, presentsPerS = 0;
    float inMs = 0, outMs = 0, waitMs = 0, captureMs = 0, lockMs = 0;
    bool  paceBound = false;
    bool  stereo = false;     // two presents per tick this window
};
Window last_window();

} // namespace dvr::perf
