#pragma once
#include <stdint.h>
namespace dvr::stereo {
// VR-229: rendering inside the previous draw is progress too. Sampling at
// its return discards that progress and can inject a center-eye single draw.
// Game-thread only. One unchanged interval is allowed after observed progress:
// the render thread can lag behind one game tick. A second quiet interval refuses.
struct DrawPresentProgress {
    uint32_t previousEntry = 0, previousReturn = 0;
    bool advanced = false, outsideAdvanced = false;
    bool observedProgress = false, allowed = false;
    unsigned quietEntries = 0;
    void begin(uint32_t present) {
        advanced = present != previousEntry;
        outsideAdvanced = present != previousReturn;
        if (advanced) { observedProgress = true; quietEntries = 0; }
        else if (quietEntries < 2) ++quietEntries;
        allowed = advanced || (observedProgress && quietEntries == 1);
        previousEntry = present;
    }
    void complete(uint32_t present) { previousReturn = present; }
};

// The camera-silent gate (scene_draw.cpp): does this draw entry see a scene being drawn?
// `cameraEntries` is the same progress record fed with the c5 upload serial at each entry.
//
// Grace off (the shipped rule): an upload must have arrived since the previous draw
// RETURNED. That is the baseline VR-229 retired for the present guard: uploads made
// while the previous draw call ran are discarded, and when the render thread spends the
// whole idle interval inside Present the tick goes SINGLE with the scene still drawing.
// Measured 2026-10-05 in gameplay: one such tick after a 36 ms xrEndFrame stall, and one
// after a 1 ms catch-up tick; under afw each costs a held present and a stale right eye.
//
// Grace on ([Stereo] CameraSilentGrace): the entry-to-entry baseline as well, and one
// quiet interval allowed after observed uploads. It only ever ADDS permission: a tick the
// shipped rule passes is passed. A load screen is silent on every interval and is still
// refused from its second one.
inline bool camera_silent(bool grace, bool uploadSinceReturn, const DrawPresentProgress& cameraEntries) {
    if (uploadSinceReturn) return false;
    return !(grace && cameraEntries.allowed);
}
}
