// game/dishonored/blade_math.h - the held sword's blade as a line segment (VR-173).
//
// What it is for: the motion sword presses the game's attack when the HAND is fast.
// To time that press from the blade reaching something, the mod first has to know
// where the blade is: a base near the palm and a tip, in the palm frame, so the segment
// follows the tracked hand exactly as the drawn sword does.
//
// PURE ARITHMETIC. No engine types, no D3D, no logging, no globals, so
// tools/blade-host.ps1 compiles exactly what the proxy runs. The reader that feeds it
// (hands/blade_axis.cpp) is the only part that knows a vertex buffer.
//
// WHY NOT THE BOLT'S MEASUREMENT (hands/bolt_model_ray.cpp). Three of its gates refuse
// the sword before an axis is fitted, and each for a good reason where a BARREL is
// wanted: 1024 vertices at most (the sword has 2481), every vertex rigid to one bone
// (the sword has several), and a candidate list that accepts a loaded bolt only. The
// blade is not an aim ray and does not enter that path; it shares the fit
// (bolt_axis_ratio) and the palm transport (palm_ray_to_xr), and nothing else.
#pragma once
#include "game/dishonored/hands/bolt_axis.h"
#include <cmath>
#include <cstdint>

namespace dvr::blade {

struct Vertex { float pos[3]; float w[4]; uint8_t bone[4]; };

// One vertex through the draw's OWN palette: three registers (twelve floats) per bone,
// row-major 3x4 with the translation in the fourth column, which is the layout MpBuild
// composes onto. Skinning with the palette of the draw being measured is what makes a
// folded or animated sword read as it is drawn, not as it was modelled.
inline bool skin(const Vertex& v, const float* palette, unsigned regs, float* out) {
    float acc[3] = {0, 0, 0}, sum = 0;
    for (int j = 0; j < 4; ++j) {
        const float w = v.w[j];
        if (!std::isfinite(w) || w < 0) return false;
        if (w <= 0.0001f) continue;
        const unsigned b = v.bone[j];
        if (b * 3 + 3 > regs) return false;          // a bone this palette does not carry
        const float* m = palette + b * 12;
        for (int r = 0; r < 3; ++r)
            acc[r] += w * (m[r*4+0]*v.pos[0] + m[r*4+1]*v.pos[1] + m[r*4+2]*v.pos[2] + m[r*4+3]);
        sum += w;
    }
    if (!(sum > 0.5f)) return false;
    for (int r = 0; r < 3; ++r) { out[r] = acc[r] / sum; if (!std::isfinite(out[r])) return false; }
    return true;
}
inline int dominant_bone(const Vertex& v) {
    int best = 0;
    for (int j = 1; j < 4; ++j) if (v.w[j] > v.w[best]) best = j;
    return v.bone[best];
}

// The long axis of ALL the points and the two vertices at its ends. The direction comes
// from an even subsample (the fit takes 1024 points at most); the ENDS are searched over
// every point, because a blade's tip is one vertex and a subsample can step over it.
struct Axis {
    hf::BoltAxis fit;            // centre, direction, ratio: from the subsample
    float low = 0, high = 0;     // extent along the direction, over every point
    int lowIndex = -1, highIndex = -1;
    int fitted = 0;              // how many points the fit saw
};
inline bool fit(const float (*points)[3], int n, float minRatio, Axis& out) {
    if (n < 16 || !points) return false;
    static const int kMax = 1024;
    const int stride = (n + kMax - 1) / kMax;
    float sub[kMax][3]; int m = 0;
    for (int i = 0; i < n && m < kMax; i += stride) {
        for (int a = 0; a < 3; ++a) sub[m][a] = points[i][a];
        ++m;
    }
    Axis ax;
    if (!hf::bolt_axis_ratio(sub, m, minRatio, ax.fit)) return false;
    ax.fitted = m; ax.low = 1e20f; ax.high = -1e20f;
    for (int i = 0; i < n; ++i) {
        float t = 0;
        for (int a = 0; a < 3; ++a) {
            if (!std::isfinite(points[i][a])) return false;
            t += (points[i][a] - ax.fit.center[a]) * ax.fit.dir[a];
        }
        if (t < ax.low)  { ax.low = t;  ax.lowIndex = i; }
        if (t > ax.high) { ax.high = t; ax.highIndex = i; }
    }
    if (ax.lowIndex < 0 || ax.highIndex < 0 || ax.high - ax.low < 0.01f) return false;
    out = ax; return true;
}

// The blade in the palm frame, metres. `endA` and `endB` are the two end vertices
// already carried into the palm frame; the palm's origin is (0, 0, 0).
//
// TIP: the end farther from the palm. The bolt's rule (agree with the camera's forward)
// cannot be used: a sword is not pointed where the player looks.
// BASE: the point of the blade line nearest the palm, held inside the segment. The
// pommel reaches behind the hand, and a trace begun at the pommel would find the
// player's own arm.
enum Refuse : int { kOk = 0, kNotFinite, kEndsAlike, kTooShort, kTooLong, kOffThePalm, kCount };
inline const char* refuse_text(int r) {
    static const char* t[] = {
        "ok", "a point is not finite",
        "the two ends are about as far from the palm as each other, so which is the tip cannot be decided",
        "the far end is closer to the palm than a blade reaches (a folded sword reads like this)",
        "the far end is further from the palm than a held blade can reach",
        "the blade line passes further from the palm than a held sword's can" };
    return r >= 0 && r < kCount ? t[r] : "?";
}
struct Bounds {
    float minReachM = 0.35f;     // a folded or sheathed sword must refuse
    float maxReachM = 1.30f;     // as a DISTANCE: a per-axis bound admits a corner (TRAPS, VR-57)
    float maxOffPalmM = 0.20f;   // how far the blade line may pass from the palm
    float minEndRatio = 1.5f;    // far end / near end
};
struct Segment {
    float base[3] = {}, tip[3] = {}, dir[3] = {};
    float lengthM = 0;           // base to tip
    float reachM = 0;            // palm to tip
    float offPalmM = 0;          // palm to the blade line
    float endRatio = 0;          // far end / near end, from the palm
    int   tipIsHigh = 0;         // which end of the axis the tip was: +1 high, -1 low
    int   refuse = kOk;
    bool  ok = false;
};
inline float len3(const float* v) { return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]); }
inline Segment segment(const float* endLow, const float* endHigh, const Bounds& b) {
    Segment s;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(endLow[i]) || !std::isfinite(endHigh[i])) { s.refuse = kNotFinite; return s; }
    const float rl = len3(endLow), rh = len3(endHigh);
    const bool high = rh >= rl;
    const float* tip = high ? endHigh : endLow;
    const float* other = high ? endLow : endHigh;     // the pommel side (`far` is a windows.h macro)
    s.tipIsHigh = high ? 1 : -1;
    s.reachM = high ? rh : rl;
    const float otherM = high ? rl : rh;
    s.endRatio = s.reachM / (otherM > 1e-4f ? otherM : 1e-4f);
    float d[3] = { tip[0]-other[0], tip[1]-other[1], tip[2]-other[2] };
    const float whole = len3(d);
    if (!(whole > 1e-4f)) { s.refuse = kNotFinite; return s; }
    for (int i = 0; i < 3; ++i) { s.dir[i] = d[i] / whole; s.tip[i] = tip[i]; }
    // the palm's projection onto the line, as a distance back from the tip
    float back = tip[0]*s.dir[0] + tip[1]*s.dir[1] + tip[2]*s.dir[2];
    if (back < 0) back = 0;
    if (back > whole) back = whole;
    float foot[3];
    for (int i = 0; i < 3; ++i) { s.base[i] = tip[i] - s.dir[i] * back; foot[i] = s.base[i]; }
    s.offPalmM = len3(foot);
    s.lengthM = back;
    if (s.endRatio < b.minEndRatio)      s.refuse = kEndsAlike;
    else if (s.reachM < b.minReachM)     s.refuse = kTooShort;
    else if (s.reachM > b.maxReachM)     s.refuse = kTooLong;
    else if (s.offPalmM > b.maxOffPalmM) s.refuse = kOffThePalm;
    s.ok = s.refuse == kOk;
    return s;
}

// How far two segments are apart: the angle between their directions and the distance
// between their tips. Used for the latch's agreement and for constant against live.
struct Apart { float deg = 0, tipM = 0; };
inline Apart apart(const Segment& a, const Segment& b) {
    Apart r;
    float dot = a.dir[0]*b.dir[0] + a.dir[1]*b.dir[1] + a.dir[2]*b.dir[2];
    dot = dot > 1 ? 1 : dot < -1 ? -1 : dot;
    r.deg = std::acos(dot) * 57.2957795f;
    const float e[3] = { a.tip[0]-b.tip[0], a.tip[1]-b.tip[1], a.tip[2]-b.tip[2] };
    r.tipM = len3(e);
    return r;
}

// A latched blade is a CONSTANT of the hand, not a sample (TRAPS, VR-57): it must come
// from a sword at rest, so candidates have to agree with each other over a COUNT and
// over a TIME before one is kept. Frames are not time: five can pass in 50 ms.
struct Latch {
    Segment pending, kept;
    int votes = 0;
    uint64_t firstMs = 0;
    bool have = false;
    float agreeDeg = 1.0f, agreeM = 0.01f;
    int needVotes = 5;
    uint64_t needMs = 300;
    // true on the feed that latches
    bool feed(const Segment& s, uint64_t nowMs) {
        if (have || !s.ok) return false;
        const Apart a = votes > 0 ? apart(s, pending) : Apart{};
        if (votes > 0 && a.deg <= agreeDeg && a.tipM <= agreeM) ++votes;
        else { pending = s; votes = 1; firstMs = nowMs; }
        if (votes < needVotes || nowMs - firstMs < needMs) return false;
        kept = pending; have = true;
        return true;
    }
    void forget() { *this = Latch{}; }
};

// A palm-frame point in XR LOCAL metres, through the SAME arithmetic the aim ray's
// origin takes (palm_ray_to_xr), so the blade and the guide cannot be carried two ways.
inline bool palm_point_to_xr(const hf::Mat3& controller, const hf::Mat3& grip, const float* palm,
                             const float* trimDeg, const float* trimM, const float* pointPalm,
                             float* out) {
    const float anyDir[3] = {1, 0, 0}; float unused[3];
    return hf::palm_ray_to_xr(controller, grip, palm, trimDeg, trimM, pointPalm, anyDir, out, unused);
}
// The hands are drawn at hand travel (WorldScaleUU x PaletteDriveGain), the camera at
// [PosTrack] Scale, so a hand-frame point is seen at this scale about the head. The aim
// ray's origin takes the same step (aim_ray.cpp).
inline void scale_about_head(const float* head, float handToWorldScale, float* p) {
    for (int i = 0; i < 3; ++i) p[i] = head[i] + handToWorldScale * (p[i] - head[i]);
}

} // namespace dvr::blade
