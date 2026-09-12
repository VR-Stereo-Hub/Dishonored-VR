// Explicit XR-space points. Present-thread host seam, no pose reconstruction.
#pragma once
#include <stdint.h>
namespace dvr::vr {
constexpr int kAimVisualPoints = 10; // one ray: endpoint + four markers; two when
                                     // [Crosshair] BothPoses compares aim against grip
struct AimVisualPoint { float pos[3] = {}; float sizeDeg = 0.5f; bool dot = false; };
struct AimVisualConfig {
    bool enabled = false;
    bool valid = false;
    uint32_t generation = 0;
    uint64_t sampleMs = 0; // original input sample age, not publication age
    int count = 0;
    AimVisualPoint points[kAimVisualPoints];
};
enum class AimVisualResult {
    Off, Invalid, Stale, NoFrame, PairPending, NotProjection, NoViews,
    NoTexture, Budget, NearHead, ImageFailed, EndFailed, Submitted, Count
};
struct AimVisualStats {
    uint32_t publishes = 0, submitted = 0, dotFrames = 0, beamFrames = 0;
    uint32_t outcomes[(int)AimVisualResult::Count] = {};
    AimVisualResult last = AimVisualResult::Off;
    uint32_t generation = 0, layerLimit = 0;
};
// All three APIs run on the present thread (including the F10 draw callback).
void set_aim_visual(const AimVisualConfig& cfg);
AimVisualStats aim_visual_stats();
inline const char* aim_visual_result_name(AimVisualResult result) {
    const char* names[] = {"off", "invalid ray", "stale sample/publish", "no XR frame",
        "pair awaiting sibling (expected)", "no projection layer", "no valid views",
        "dot texture unavailable", "layer budget", "point near head/invalid",
        "dot image upload failed", "xrEndFrame failed", "submitted"};
    const int i = (int)result;
    return i >= 0 && i < (int)AimVisualResult::Count ? names[i] : "unknown";
}
// Pure gates shared by the runtime and host regression tests.
inline bool aim_visual_fresh(uint64_t sample, uint64_t published, uint64_t now) {
    return sample && published && now >= sample && now >= published &&
           now - sample <= 250 && now - published <= 250;
}
inline int aim_visual_budget(int used, int runtimeLimit, int arrayLimit) {
    const int cap = runtimeLimit < arrayLimit ? runtimeLimit : arrayLimit;
    return cap > used ? cap - used : 0;
}
} // namespace dvr::vr
