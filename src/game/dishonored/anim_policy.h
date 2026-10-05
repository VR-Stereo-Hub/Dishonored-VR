#pragma once
#include <cstring>
#include <cctype>
#include <cmath>
#include "hands/hand_frame.h"
namespace dvr::anim {
inline bool fresh(unsigned long long stamp,unsigned long long now) { return stamp && now>=stamp && now-stamp<=150; }
inline bool listed(const char* list, const char* name) {
    if (!name || !*name) return false;
    while (*list) {
        while (*list == ',' || std::isspace((unsigned char)*list)) ++list;
        const char* end = list;
        while (*end && *end != ',') ++end;
        const char* trimmed = end;
        while (trimmed > list && std::isspace((unsigned char)trimmed[-1])) --trimmed;
        if (size_t(trimmed-list) == std::strlen(name) && !std::strncmp(list,name,trimmed-list)) return true;
        list = end;
    }
    return false;
}
// Smooth hand-back ([Anim] SmoothBlend): zero velocity AND acceleration at both ends, so
// neither the start nor the finish of a hand-back has a corner the eye reads as a snap.
inline float smootherstep(float t) {
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t*t*t*(t*(t*6-15)+10);
}
struct Handoff {
    bool game = false, releasing = false;
    unsigned long long releaseAt = 0, blendAt = 0;
    float from = 1, target = 1;
    // SmoothBlend: eased, with its own entry (to the game, target 0) and return (to the
    // controller, target 1) durations; a reversal mid-blend covers only the remaining
    // distance, in that share of the duration. Off: the original linear ramp of `duration`.
    bool smooth = false;
    unsigned inMs = 0, outMs = 0;
    unsigned span() const {
        const unsigned full = target < 0.5f ? inMs : outMs;
        const float part = fabsf(target-from);
        return (unsigned)(full*(part < 1 ? part : 1) + 0.5f);
    }
    float value(unsigned long long now, unsigned duration) const {
        if (smooth) {
            const unsigned d = span();
            if (!d || now >= blendAt + d) return target;
            const float t = now > blendAt ? float(now-blendAt)/d : 0;
            return from + (target-from)*smootherstep(t);
        }
        if (!duration || now >= blendAt + duration) return target;
        const float t = now > blendAt ? float(now-blendAt)/duration : 0;
        return from + (target-from)*t;
    }
    void update(bool valid, bool match, bool enabled, unsigned long long now,
                unsigned releaseMs, unsigned blendMs) {
        if (!valid || !enabled) {   // reset the state, keep the configured shape
            const bool s = smooth; const unsigned i = inMs, o = outMs;
            *this = Handoff{}; smooth = s; inMs = i; outMs = o; return;
        }
        if (match) { game = true; releasing = false; }
        else if (game) {
            if (!releasing) { releasing = true; releaseAt = now; }
            if (now-releaseAt >= releaseMs) { game = false; releasing = false; }
        }
        const float next = game ? 0.0f : 1.0f;
        if (next != target) { from = value(now,blendMs); target = next; blendAt = now; }
    }
};
// Classification can end before the visual return does. With SmoothBlend the same hands
// stay owned until their correction is back at the controller (weight 1); otherwise the
// mask dropped on the tick the return began and weight_for() answered 1 at once: the
// return blend was never seen, the hand snapped back.
inline unsigned char render_hand_mask(bool valid, bool enabled, unsigned char matched,
                                      unsigned char previous, bool game, float weight) {
    if (!valid || !enabled) return 0;
    return matched ? matched : (game || weight < 1.0f) ? previous : 0;
}
// Slerp the proper rotation from identity and linearly interpolate uniform
// scale/translation. Refuse shear/reflection instead of collapsing a limb.
// CinematicArms: "the game is animating the arms" from the motion of the GAME's own arm bones.
// Opens after `startMs` above `start` uu/s, closes after `stopMs` below `stop` uu/s; in between
// it keeps its state. A stale or missing speed reads as still.
// `stamp` names the measurement the speed came from (0 = unknown, every call counts as new) and
// `minSamples` is how many DIFFERENT measurements above `start` an opening needs: a pose snap is
// one measurement, however long the reader keeps seeing it (run 3: one 700..2700 uu/s frame,
// then a stalled sampler, read as 200 ms of motion).
struct MotionGate {
    bool on = false;
    unsigned long long aboveSince = 0, belowSince = 0, aboveStamp = 0;
    unsigned aboveSamples = 0;
    bool update(float speed, unsigned long long now, float start, float stop, unsigned startMs, unsigned stopMs,
                unsigned long long stamp = 0, unsigned minSamples = 1) {
        if (!(speed >= 0) || !std::isfinite(speed)) speed = 0;
        if (speed > start) {
            if (!aboveSince) { aboveSince = now; aboveSamples = 0; aboveStamp = 0; }
            if (!stamp || stamp != aboveStamp) { ++aboveSamples; aboveStamp = stamp; }
        } else { aboveSince = 0; aboveSamples = 0; }
        if (speed < stop) { if (!belowSince) belowSince = now; } else belowSince = 0;
        if (!on && aboveSince && now - aboveSince >= startMs && aboveSamples >= minSamples) on = true;
        else if (on && belowSince && now - belowSince >= stopMs) on = false;
        return on;
    }
};
// The game's own arm motion, blind to the mod's. Run 3 (2026-10-04) measured the old instrument
// (the fastest arm-bone point in the native palette) following the PLAYER's controller: the
// mod's hand control moves palette bones too, so any hand movement above 0.2 m/s opened the gate.
// What the mod writes is ONE rigid move of one bone and everything below it, per arm. That splits
// an arm's bones into at most two groups that stay rigid inside themselves, whichever bone the
// control sits on. An animation bends more than one joint. So the motion is measured BETWEEN
// bones (bone b's points in bone a's frame, which no common rigid move changes), and the answer
// is the largest relative speed that is left after the single fastest split is granted to the
// mod: the second-largest edge of the minimum spanning tree over the pairwise speeds.
// Limit, by construction: a clip that moves exactly one joint reads as still.
constexpr int kArmMotionMaxBones = 10;
struct ArmMotion {
    float joint = 0;      // uu/s between bones, the mod's one rigid write excluded: the gate's input
    float fastest = 0;    // uu/s of the fastest bone point in the palette (the run-3 instrument; log only)
    float refPose = -1;   // uu the pose is from the reference pose, same exclusion; -1 = not measurable
    int bones = 0;        // bones measured; under 3 nothing can be separated and joint stays 0
};
inline float second_split(const float (*w)[kArmMotionMaxBones], int n) {
    if (n < 3) return 0;
    bool in[kArmMotionMaxBones] = {}; float best[kArmMotionMaxBones]; float e1 = 0, e2 = 0;
    in[0] = true; for (int i = 0; i < n; ++i) best[i] = w[0][i];
    for (int k = 1; k < n; ++k) {
        int pick = -1;
        for (int i = 0; i < n; ++i) if (!in[i] && (pick < 0 || best[i] < best[pick])) pick = i;
        const float e = best[pick];
        if (e > e1) { e2 = e1; e1 = e; } else if (e > e2) e2 = e;
        in[pick] = true;
        for (int i = 0; i < n; ++i) if (!in[i] && w[pick][i] < best[i]) best[i] = w[pick][i];
    }
    return e2;
}
// `cur`/`prev`: n skin matrices, 12 floats each (3 rows of 4, translation last), reference to
// draw-local. `probe`: one reference-space point per bone (its vertex centroid). prev may be
// null or dt <= 0: speeds read 0, the reference distance is still measured.
inline ArmMotion arm_motion(const float* cur, const float* prev, const float (*probe)[3], int n, float dt) {
    ArmMotion out;
    if (!cur || !probe || n < 1 || n > kArmMotionMaxBones) return out;
    float invCur[kArmMotionMaxBones][12], invPrev[kArmMotionMaxBones][12];
    const bool timed = prev && dt > 0;
    for (int b = 0; b < n; ++b) {
        if (!hf::invert_3x4(cur + b * 12, invCur[b])) return out;      // collapsed (hidden) palette: nothing to measure
        if (timed && !hf::invert_3x4(prev + b * 12, invPrev[b])) return out;
    }
    auto at = [](const float* m, const float* p, float* q) {
        for (int r = 0; r < 3; ++r) q[r] = m[r * 4] * p[0] + m[r * 4 + 1] * p[1] + m[r * 4 + 2] * p[2] + m[r * 4 + 3];
    };
    float speed[kArmMotionMaxBones][kArmMotionMaxBones] = {}, away[kArmMotionMaxBones][kArmMotionMaxBones] = {};
    for (int b = 0; b < n; ++b) {
        // The centroid and two 10 uu levers: a bone that turns in place moves its levers.
        const float pts[3][3] = { { probe[b][0], probe[b][1], probe[b][2] }, { probe[b][0] + 10, probe[b][1], probe[b][2] },
                                  { probe[b][0], probe[b][1] + 10, probe[b][2] } };
        for (int k = 0; k < 3; ++k) {
            float wc[3], wp[3] = {}; at(cur + b * 12, pts[k], wc);
            if (timed) {
                at(prev + b * 12, pts[k], wp);
                const float v = sqrtf((wc[0]-wp[0])*(wc[0]-wp[0]) + (wc[1]-wp[1])*(wc[1]-wp[1]) + (wc[2]-wp[2])*(wc[2]-wp[2])) / dt;
                if (std::isfinite(v) && v > out.fastest) out.fastest = v;
            }
            for (int a = 0; a < n; ++a) {
                if (a == b) continue;
                float rc[3], rp[3]; at(invCur[a], wc, rc);
                const float d = sqrtf((rc[0]-pts[k][0])*(rc[0]-pts[k][0]) + (rc[1]-pts[k][1])*(rc[1]-pts[k][1]) + (rc[2]-pts[k][2])*(rc[2]-pts[k][2]));
                if (std::isfinite(d)) { if (d > away[a][b]) away[a][b] = d; if (d > away[b][a]) away[b][a] = d; }
                if (!timed) continue;
                at(invPrev[a], wp, rp);
                const float v = sqrtf((rc[0]-rp[0])*(rc[0]-rp[0]) + (rc[1]-rp[1])*(rc[1]-rp[1]) + (rc[2]-rp[2])*(rc[2]-rp[2])) / dt;
                if (std::isfinite(v)) { if (v > speed[a][b]) speed[a][b] = v; if (v > speed[b][a]) speed[b][a] = v; }
            }
        }
    }
    out.bones = n;
    if (n >= 3) { out.joint = second_split(speed, n); out.refPose = second_split(away, n); }
    return out;
}
// A uniformly scaled proper rotation: what blend_transform can interpolate.
inline bool blendable(const hf::Xform& input, float* scaleOut=nullptr, hf::Mat3* rotOut=nullptr) {
    const float scale=sqrtf(input.r.m[0]*input.r.m[0]+input.r.m[3]*input.r.m[3]+input.r.m[6]*input.r.m[6]);
    if (!(scale>0.0001f) || !std::isfinite(scale)) return false;
    hf::Mat3 r=input.r;
    for(int i=0;i<9;++i) r.m[i]/=scale;
    if (!hf::is_rotation(r,0.02f)) return false;
    if (scaleOut) *scaleOut=scale;
    if (rotOut) *rotOut=r;
    return true;
}
inline hf::Xform blend_transform(const hf::Xform& input, float weight) {
    if (weight>=1) return input;
    hf::Xform out={hf::identity3(),{0,0,0}};
    if (weight<=0) return out;
    float scale=1; hf::Mat3 r;
    if (!blendable(input,&scale,&r)) return out;
    float q[4]={};
    const float trace=r.m[0]+r.m[4]+r.m[8];
    if(trace>0) {
        const float s=sqrtf(trace+1)*2; q[3]=s/4;
        q[0]=(r.m[7]-r.m[5])/s; q[1]=(r.m[2]-r.m[6])/s; q[2]=(r.m[3]-r.m[1])/s;
    } else {
        int i=r.m[4]>r.m[0]?1:0; if(r.m[8]>r.m[i*3+i]) i=2;
        const int j=(i+1)%3,k=(i+2)%3;
        const float s=sqrtf(1+r.m[i*3+i]-r.m[j*3+j]-r.m[k*3+k])*2;
        q[i]=s/4; q[j]=(r.m[j*3+i]+r.m[i*3+j])/s;
        q[k]=(r.m[k*3+i]+r.m[i*3+k])/s; q[3]=(r.m[k*3+j]-r.m[j*3+k])/s;
    }
    if(q[3]<0) for(float& v:q) v=-v;
    float norm=0; for(float v:q) norm+=v*v; norm=sqrtf(norm);
    for(float& v:q) v/=norm;
    const float angle=acosf(fminf(1.0f,fmaxf(0.0f,q[3])));
    const float mul=angle>0.00001f?sinf(angle*weight)/sinf(angle):weight;
    const float x=q[0]*mul,y=q[1]*mul,z=q[2]*mul,w=cosf(angle*weight);
    out.r={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),
            2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),
            2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
    for(int i=0;i<9;++i) out.r.m[i]*=1+(scale-1)*weight;
    for(int i=0;i<3;++i) out.t[i]=input.t[i]*weight;
    return out;
}
// SmoothBlend: the same rotation/scale, but the translation is solved so the PALM moves on
// the straight line between its native and its controller position. Interpolating the
// correction's translation instead swings a palm far from the mesh origin around that
// origin and overshoots both ends (measured on the retired HandOrigin branch: median 8.9 uu,
// up to 98 uu off the straight path). `palm` is the palm point in the draw's local space.
inline hf::Xform blend_transform_palm(const hf::Xform& input, float weight, const float* palm) {
    hf::Xform out=blend_transform(input,weight);
    if (!palm || weight>=1 || weight<=0) return out;
    for(int i=0;i<3;++i) if(!std::isfinite(palm[i])) return out;
    if (!blendable(input)) return out;   // refused (sheared/reflected): keep blend_transform's identity
    for(int i=0;i<3;++i) {
        float target=input.t[i], rotated=0;
        for(int j=0;j<3;++j) { target+=input.r.m[i*3+j]*palm[j]; rotated+=out.r.m[i*3+j]*palm[j]; }
        out.t[i]=palm[i]+(target-palm[i])*weight-rotated;
    }
    return out;
}
}


