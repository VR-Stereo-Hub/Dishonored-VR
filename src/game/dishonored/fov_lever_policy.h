#pragma once
#include <cmath>
#include "cinematic_fov_policy.h"

namespace dvr::fov_lever {
// VR-227: the draw-only lock cannot repair persistent base fields. Reuse the
// bounded exit policy without allowing ordinary zoom to prime recovery.
struct CinematicRecovery {
    dvr::cine_fov::ExitBridge bridge;
    float update(bool eligible, bool cinematic, bool walking, float sensor,
                 float requested, unsigned long long now) {
        if (!eligible) { bridge = {}; return 0; }
        return bridge.update(cinematic, walking, sensor, requested, now) ? requested : 0;
    }
};
// The sensor is rendered output, including interpolated echoes of our writes.
// A contraction ratio repeatedly applied to it has no positive fixed point.
// Cap the scaling baseline at the requested target: below that baseline retain
// native zoom; above it converge to the target. Existing expansion is unchanged.
inline float target(float sensor, float natural, float requested) {
    if (!std::isfinite(sensor) || !std::isfinite(natural) ||
        !std::isfinite(requested) || sensor <= 10 || sensor >= 175 ||
        natural <= 30 || natural >= 140 || requested < 40 || requested > 160)
        return 0;
    const float base = natural < requested ? natural : requested;
    const float bounded = sensor < base ? sensor : base;
    const float scaled = bounded * (requested / base);
    return scaled < 20 ? 20 : (scaled > 160 ? 160 : scaled);
}

// The natural base is re-read after every load that changes the owners, from the
// same sensor the lever has been driving. That reading is usually our own output:
// the logs show "natural base 108.1 deg ... ratio 1.000" after loads. A ratio of
// exactly one has no restoring force, so a narrowing the game makes for a moment
// (a death, a store, an objective) is copied into the controller's DefaultFOV and
// becomes the new resting FOV; the headset window then stays a small box. With the
// real base (75) the ratio is 1.44 and any narrow value is widened back each pass.
// So a re-armed reading is accepted only when it cannot be our echo or a transient.
enum class Rearm { Fresh, Accepted, KeptEcho, KeptAtTarget, KeptNarrower, Invalid };
inline const char* rearm_name(Rearm r) {
    switch (r) {
    case Rearm::Fresh: return "first capture";
    case Rearm::Accepted: return "accepted (wider than the kept base, not our output)";
    case Rearm::KeptEcho: return "KEPT the old base: the reading is our own last write";
    case Rearm::KeptAtTarget: return "KEPT the old base: the reading is at the target, indistinguishable from our output";
    case Rearm::KeptNarrower: return "KEPT the old base: the reading is narrower, a death/store/menu zoom";
    default: return "refused: reading out of range";
    }
}
// reading: the sensor now; kept: the base this session already trusted (0 = none);
// lastWrite: the last value the lever wrote (0 = none); requested: the target.
inline float rearm_natural(float reading, float kept, float lastWrite, float requested, Rearm* why) {
    Rearm r = Rearm::Invalid;
    float out = 0;
    if (std::isfinite(reading) && reading > 30 && reading < 140) {
        if (!(kept > 30 && kept < 140)) { r = Rearm::Fresh; out = reading; }
        else if (lastWrite > 0 && std::fabs(reading - lastWrite) <= 0.5f) { r = Rearm::KeptEcho; out = kept; }
        else if (reading >= requested - 0.5f) { r = Rearm::KeptAtTarget; out = kept; }
        else if (reading < kept - 0.5f) { r = Rearm::KeptNarrower; out = kept; }
        else { r = Rearm::Accepted; out = reading; }
    }
    if (why) *why = r;
    return out;
}
}
