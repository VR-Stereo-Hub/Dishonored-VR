// game/dishonored/ui_ride_policy.h - may an in-game screen ride the HUD window?
// (VR-117). Pure decisions, no engine reads, so tools/ui-ride-tests.cpp can
// fail them on the host. Shaped like stereo_state_policy.h.
//
// The reflected UI owner (ue3/ui_surface.cpp, PR #58) names the screen on top:
// pause, note, journal, store, mission stats, the wheel, the main menu, a
// load. Today every owned screen forces the runtime onto the mono quad. A
// RIDE keeps the projection up and puts the screen's draws (the same
// Scaleform class as the HUD, measured) on the HUD window instead, with the
// world in stereo behind it. Input permission is NOT part of this: the
// gameplay verdict stays parked while a screen is up, so the head-mouse stays
// off, the pad keeps its menu shaping, and VR-71's stale-flag guard still
// sees the owner as blocking.
#pragma once
#include "core/vr/mono_anchor.h"
#include <cstdint>

namespace dvr::ui_ride {

// Never the main menu (it keeps the screen), never a load, never the wheel
// (held open, drawn over gameplay, the redirect pauses for it), never a
// cinematic or an unknown owner.
inline bool context_can_ride(dvr::mono::Context c) {
    return c == dvr::mono::Pause || c == dvr::mono::Note || c == dvr::mono::Journal ||
           c == dvr::mono::Store || c == dvr::mono::MissionStats;
}

// The decision for one publish. optInMask bit = dvr::mono::Context.
inline bool rides(bool enabled, bool blocked, dvr::mono::Context c, uint32_t optInMask,
                  bool menuInWindow, bool windowOn, bool redirectHealthy) {
    return enabled && blocked && context_can_ride(c) && ((optInMask >> (unsigned)c) & 1u) != 0 &&
           menuInWindow && windowOn && redirectHealthy;
}

// Presentation stand-in while riding: a live pawn, and the scene proven to be
// drawing by either the raw camera-upload clock (the paused world keeps
// uploading its camera, measured on PR #12: L/s=R/s=60 behind the pause
// menu) or a tagged projection present within the last few presents. No
// viewLive, no menu term, no FSM validity: all three go silent on a pause by
// construction, which is exactly why the ordinary verdict drops there.
inline bool ride_eligible(bool pawn, bool sceneFreshRaw, bool gateFresh) {
    return pawn && (sceneFreshRaw || gateFresh);
}

// One decision per blocked interval: made on the context's first publish and
// held until the context changes, so a health flap mid-menu cannot flip the
// picture between the window and the mono quad. Only a latched hard failure
// drops it (to the mono quad: fail soft), and an unblocked publish clears it.
struct RideLatch {
    bool riding = false;
    dvr::mono::Context context = dvr::mono::Other;
    bool update(dvr::mono::Context c, bool blocked, bool want, bool hardFail) {
        if (!blocked || c != context) { context = c; riding = blocked && want; }
        if (hardFail) riding = false;
        if (!blocked) riding = false;
        return riding;
    }
};

// The resume gap: when the screen closes the view pipeline is silent for a few
// presents until its first dispatch, and without a stand-in the projection
// dropped to the screen and came straight back on EVERY resume (headset run
// 47, the "stereo reloading" report). The grace holds the stand-in for
// windowMs after a ride ends; a new blocked owner cancels it.
struct RideGrace {
    double endMs = 0;
    bool was = false;
    bool update(bool riding, bool blocked, double nowMs, double windowMs = 1500.0) {
        if (was && !riding) endMs = nowMs;
        was = riding;
        if (blocked && !riding) endMs = 0;
        return !riding && endMs > 0 && nowMs - endMs < windowMs;
    }
};

} // namespace dvr::ui_ride
