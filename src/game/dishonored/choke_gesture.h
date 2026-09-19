// VR-145: the physical choke gesture, pure (host-tested by tools/choke-gesture-tests.cpp).
// A QUICK right-hand move into a zone at the left shoulder starts it; it stays
// held while the hand stays near the shoulder and releases when the hand leaves.
// The shoulder is modelled from the head: yaw-only (a head tilt must not swing
// the shoulder), a fixed offset left, down and back. XR LOCAL space, metres,
// right +X, up +Y, forward -Z.
#pragma once
#include <cmath>
namespace dvr::choke {

struct Config {
    bool  enabled = false;       // [Choke] Gesture
    float enterM = 0.14f;        // start: the hand within this of the shoulder point
    float exitM = 0.22f;         // hold: until the hand is farther than this
    float minSpeed = 0.9f;       // m/s: the hand's peak speed in the last `windowMs` before entering
    float windowMs = 400.0f;
    float leftM = 0.17f, downM = 0.22f, backM = 0.04f;   // the left shoulder from the head
};

inline void shoulder_point(const Config& c, const float head[3], const float headQ[4], float out[3]) {
    // Yaw-only basis from the head's forward (-Z rotated), flattened.
    const float x = headQ[0], y = headQ[1], z = headQ[2], w = headQ[3];
    float f[3] = { -(2*(x*z + w*y)), -(2*(y*z - w*x)), -(1 - 2*(x*x + y*y)) };
    f[1] = 0;
    float n = std::sqrt(f[0]*f[0] + f[2]*f[2]);
    if (!(n > 1e-4f)) { f[0] = 0; f[2] = -1; n = 1; }
    f[0] /= n; f[2] /= n;
    const float r[3] = { -f[2], 0.0f, f[0] };   // right = forward x up
    for (int i = 0; i < 3; ++i) out[i] = head[i] - r[i] * c.leftM - f[i] * c.backM;
    out[1] -= c.downM;
}

struct State {
    bool  held = false;
    bool  havePrev = false;
    float prev[3] = {};
    double prevMs = 0;
    double peakMs = -1e30;       // when the hand last moved at >= minSpeed
    float lastSpeed = 0, lastDist = 0;
    // One step. `allowed` false (menu, wheel, untracked) releases immediately.
    // Returns the held state; `edge` is +1 on start, -1 on release, else 0.
    bool update(const Config& c, double nowMs, const float hand[3], const float head[3],
                const float headQ[4], bool allowed, int* edge) {
        *edge = 0;
        if (havePrev && nowMs > prevMs) {
            const float dx = hand[0]-prev[0], dy = hand[1]-prev[1], dz = hand[2]-prev[2];
            lastSpeed = std::sqrt(dx*dx + dy*dy + dz*dz) / (float)((nowMs - prevMs) / 1000.0);
            if (lastSpeed >= c.minSpeed) peakMs = nowMs;
        }
        for (int i = 0; i < 3; ++i) prev[i] = hand[i];
        prevMs = nowMs; havePrev = true;
        float s[3]; shoulder_point(c, head, headQ, s);
        const float ex = hand[0]-s[0], ey = hand[1]-s[1], ez = hand[2]-s[2];
        lastDist = std::sqrt(ex*ex + ey*ey + ez*ez);
        const bool was = held;
        if (!c.enabled || !allowed) held = false;
        else if (held) held = lastDist <= c.exitM;
        else held = lastDist <= c.enterM && nowMs - peakMs <= c.windowMs;
        if (held != was) *edge = held ? 1 : -1;
        return held;
    }
};

} // namespace dvr::choke
