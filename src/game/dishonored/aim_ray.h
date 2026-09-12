// VR-34/VR-57: the sole owner of the new controller aim ray, in XR LOCAL metres.
#pragma once
#include <stdint.h>
#include <cmath>
#include "core/util/xr_math.h"
#include "core/vr/aim_visual.h"
namespace dvr::status { class Writer; }
namespace dvr::aim {
struct Ray {
    bool ok = false;
    int hand = 0;
    float originXr[3] = {}, dirXr[3] = {};
    uint32_t gen = 0;
    uint64_t sampleMs = 0;
    const char* why = "no sample";
};
// Consumers: fixed-distance crosshair endpoint and beam points, by shared DATA.
// Future consumers: projectile, Blink and powers. None is connected in VR-57.
// Game-space conversion and a measured barrel axis do not exist here yet.
inline Ray from_pose(int hand, bool valid, const float pos[3], const float quat[4],
                     uint32_t gen, uint64_t sampleMs, uint64_t now) {
    Ray r; r.hand = hand; r.gen = gen; r.sampleMs = sampleMs;
    if (hand < 0 || hand > 1) { r.why = "invalid hand"; return r; }
    if (!valid) { r.why = "aim pose unavailable"; return r; }
    if (!gen || !sampleMs || now < sampleMs || now - sampleMs > 250) {
        r.why = "input sample stale"; return r;
    }
    float norm = 0;
    for (int i = 0; i < 3; ++i) if (!std::isfinite(pos[i])) {
        r.why = "nonfinite position"; return r;
    }
    for (int i = 0; i < 4; ++i) {
        if (!std::isfinite(quat[i])) { r.why = "nonfinite orientation"; return r; }
        norm += quat[i] * quat[i];
    }
    if (!std::isfinite(norm) || norm < 0.25f || norm > 4.0f) {
        r.why = "invalid quaternion norm"; return r;
    }
    const float inv = 1.0f / std::sqrt(norm), fwd[3] = {0, 0, -1};
    dvr::xrmath::quat_rotate(quat[0]*inv, quat[1]*inv, quat[2]*inv, quat[3]*inv, fwd, r.dirXr);
    for (int i = 0; i < 3; ++i) r.originXr[i] = pos[i];
    r.ok = true; r.why = "ready"; return r;
}
inline void visual_append(dvr::vr::AimVisualConfig& out, const Ray& ray, bool dot,
                          bool beam, float distance, float size);

inline dvr::vr::AimVisualConfig visual(const Ray& ray, bool dot, bool beam,
                                      float distance, float size) {
    dvr::vr::AimVisualConfig out;
    out.enabled = dot || beam; out.valid = ray.ok;
    out.generation = ray.gen; out.sampleMs = ray.sampleMs;
    if (!out.enabled || !out.valid) return out;
    if (!std::isfinite(distance) || distance < 0.5f || distance > 50.0f ||
        !std::isfinite(size) || size < 0.05f || size > 2.0f) { out.valid = false; return out; }
    auto point = [&](float along, float angular, bool endpoint) {
        auto& p = out.points[out.count++]; p.dot = endpoint; p.sizeDeg = angular;
        for (int a = 0; a < 3; ++a) p.pos[a] = ray.originXr[a] + along * ray.dirXr[a];
    };
    if (dot) point(distance, size, true);
    if (beam) for (int i = 0; i < 4; ++i) {
        // End before the endpoint so the two don't overlap. No second pose read.
        const float along = 0.25f * std::pow((distance * 0.8f) / 0.25f, i / 3.0f);
        point(along, size * 0.5f, false);
    }
    return out;
}
// The SECOND ray, for the pose A/B. Same geometry, appended into the same
// publication so both beams are one layer budget and one freshness stamp.
// The tester reports the beam pointing nowhere near the controller while the
// shot derived from the same pose lands roughly right, and reading the code
// has not settled it: drawing the runtime's OTHER pose beside it lets the
// headset name the correct one in one look.
inline void visual_append(dvr::vr::AimVisualConfig& out, const Ray& ray, bool dot,
                          bool beam, float distance, float size) {
    if (!ray.ok || !(dot || beam)) return;
    if (!std::isfinite(distance) || distance < 0.5f || distance > 50.0f ||
        !std::isfinite(size) || size < 0.05f || size > 2.0f) return;
    auto point = [&](float along, float angular, bool endpoint) {
        if (out.count >= dvr::vr::kAimVisualPoints) return;
        auto& p = out.points[out.count++]; p.dot = endpoint; p.sizeDeg = angular;
        for (int a = 0; a < 3; ++a) p.pos[a] = ray.originXr[a] + along * ray.dirXr[a];
    };
    if (dot) point(distance, size, true);
    if (beam) for (int i = 0; i < 4; ++i) {
        const float along = 0.25f * std::pow((distance * 0.8f) / 0.25f, i / 3.0f);
        point(along, size * 0.5f, false);
    }
}

struct Config { bool dot = false, laser = false; int hand = 0; float distanceM = 8, sizeDeg = 0.5f;
                bool bothPoses = false; };   // draw the GRIP ray too, at half size
Config config();
void configure(const Config& cfg, const char* origin);
Ray ray(); // most recent present-thread snapshot, no recomputation
void tick(bool gameplay, bool projectionWanted);
void command(const char* args);
void draw_ui();
void status(dvr::status::Writer& w);
} // namespace dvr::aim
