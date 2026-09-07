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
#include <stddef.h>

struct IDirect3DDevice9;
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

// The frame-start marker (commit 2): the first BeginScene after the game's
// Present returned splits OUT into idle (the render thread had nothing queued:
// it waited for the game thread) and R (executing the frame's commands). The
// device's BeginScene is patched for it; the first SetRenderTarget after
// Present is the fallback marker when a window sees no BeginScene. `which` is
// "BeginScene" or "SRT". Any thread; the tid is compared with the presenter's.
void frame_start_marker(const char* which);

// The GPU timestamps (commit 3): a ring of D3D9 query sets on the game's
// device - a TIMESTAMPDISJOINT bracket per present, TIMESTAMPFREQ, and four
// TIMESTAMPs: the first BeginScene after Present (the GPU starts the frame),
// hkPresent entry (the GPU's frame is complete when it passes here), and a
// pair around the capture's readback copy (its OWN GPU cost, the number that
// splits the CPU's lock wait into "the GPU finishing the frame" and "the
// readback"). Read back five presents later with GetData(0): never flushed,
// never waited on. The device comes from hkPresent; the queries are released
// on Reset (the hkReset LAW) and recreated lazily.
enum GpuPoint { kGpuRtdA = 0, kGpuRtdB };
void set_device(IDirect3DDevice9* dev);   // hkPresent, before kEntry
void gpu_mark(GpuPoint p);                // the capture, around its readback copy
void on_reset();                          // hkReset: every query goes
void set_gpu_enabled(bool on);            // [Perf] GpuQueries=, `perf gpu on|off`
bool gpu_enabled();

// The freeze marker (commit 5): `mark <text>` on the seam and the F10 MARK
// button stamp the moment a player felt a freeze, at Warn, with everything
// the ring knows: the present and its tag, the pair hold, the last 3 s tick
// and gpu numbers, the pace timeouts, the last closed present's split, the
// game side's context (state, swing age, aim window, the ground-truth test),
// then the last 24 presents as a compact table (in/out, lock, gpu span; a
// star on a present above twice the median). Bounded: three lines.
void mark(const char* text, const char* origin);
// The game side supplies its context string (registered once; null = none).
typedef int (*ContextProvider)(char* buf, size_t cap);
void set_context_provider(ContextProvider fn);

// The frame gap (commit 6): detected at hkPresent entry (before the runtime's
// wait, so a gap that sat in the wait is attributed to it), when the interval
// since the previous entry is 80 ms or more, OR 2.5x the last window's mean
// present interval (never under 40 ms) so a two-tick hitch at 39 ms/tick is
// caught. The game side takes it once and prints its line with the game
// fields; `where` names the phase of the previous present the gap sat in,
// that present's lock and gpu, and the flags since the last gap (a Reset, a
// level load, a pair hold open at the entry, pace-thread timeouts).
enum Flag { kFlagReset = 0, kFlagLevelLoad };
void note(Flag f);
struct Gap {
    float    ms = 0, medianMs = 0;
    uint32_t present = 0;
    char     where[480] = "";
};
bool take_gap(Gap* out);      // true once per detected gap (present thread)
void log_gap_ring();          // the ring's tail at Info, at most once per second

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
    float idleMs = 0, rMs = 0;   // OUT split by the marker (per tick under stereo)
    char  marker[24] = "";       // "BeginScene", "SRT-first" or "none"
    // The GPU, per tick (per present under mono): the frame span (first
    // BeginScene -> Present entry), the readback's own GPU time, the D3D9 idle
    // gap between frames; and the population the means come from.
    float gpuSpanMs = 0, gpuDmaMs = 0, gpuIdleMs = 0;
    uint32_t gpuResolved = 0, gpuLate = 0, gpuDisjoint = 0, gpuUnmarked = 0;
    char  gpu[8] = "";           // "ok", "n/a", "off"
    uint32_t marks = 0;          // `mark` calls so far
    // Session 13: the runtime's own display period (ms; 0 = the runtime does not
    // say, which is UNKNOWN and never "0 Hz"). The budget every number above is
    // being judged against, so it belongs on the same line as them.
    float displayPeriodMs = 0;
    bool  paceBound = false;
    bool  stereo = false;     // two presents per tick this window
};
Window last_window();

} // namespace dvr::perf
