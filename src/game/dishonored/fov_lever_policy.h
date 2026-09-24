#pragma once
#include <cmath>

namespace dvr::fov_lever {
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
}
