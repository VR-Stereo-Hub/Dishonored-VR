// game/dishonored/hands/grab_pose.h - the grab: a flat hand closing into a fist.
//
// Pure maths for the grab animation (mesh_split.cpp, "THE GRAB"). No game reads, no D3D, so a
// host test (tools/grab-pose-tests.cpp) checks it without the game.
//
// What moves. Each finger bone is a 3x4 skinning matrix in the drawn palette. The animation
// blends each finger bone's matrix RELATIVE TO ITS PARENT BONE between two poses (flat, fist) and
// rebuilds the chain from the wrist out (mesh_split.cpp), so each JOINT turns by its own curl.
// The palette carries no hierarchy; the parents are found from the mesh and the pose (bottom of
// this file). The blend is a SCREW: the rigid motion from one pose to the other is a rotation
// about some axis in space plus a slide along it, and the blend takes that fraction of the
// rotation about that same axis. A bone turning about its knuckle therefore sweeps an ARC about
// the knuckle, as a real finger does. A straight blend of the translations would cut across the
// chord, and the finger would shrink in the middle of the motion.
//
// When it moves. A grab is a minimum-jerk motion (the profile reach-to-grasp closure follows in
// the motor-control literature: smooth start, smooth stop, no velocity step at either end), in
// four phases, all measured from the grip press:
//     shape    the hand opens flat from whatever pose it had          (default 150 ms, scaled
//              by how far from flat it is: none for a hand already open)
//     close    flat -> fist, the bones nearer the knuckles leading     (default 220 ms + the lag)
//     hold     the fist stays while the grip is held (at least)       (default 180 ms)
//     release  fist -> the pose the game (or the open hand) gives it   (default 260 ms)
// The close lags each bone by its distance from the wrist (0 at the wrist, `lagMs` at the
// fingertips), so a finger rolls shut from the knuckle out instead of folding as one plank.

#pragma once
#include <cmath>
#include <cstring>
#include <cstdint>

namespace dvr {
namespace grab {

// 10t^3 - 15t^4 + 6t^5 on [0,1]: zero velocity and acceleration at both ends.
static inline float min_jerk(float t)
{
    if (!(t > 0.0f)) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return t * t * t * (10.0f + t * (-15.0f + 6.0f * t));
}

struct Timing { float shapeMs, closeMs, holdMs, releaseMs, lagMs; };

enum Phase { kIdle = 0, kShape, kClose, kHold, kRelease, kDone };

// Where a grab is at `elapsed` ms after the press. `releaseAt` is the time (ms after the press)
// the release began, or a negative number while it has not. For kClose `s` is the progress of
// a bone at the WRIST; per_bone_close() applies the lag.
struct State { Phase phase; float s; };
static inline State phase_at(const Timing& T, float elapsed, float releaseAt)
{
    State st; st.phase = kIdle; st.s = 0.0f;
    if (elapsed < 0.0f) return st;
    if (releaseAt >= 0.0f && elapsed >= releaseAt) {
        const float r = T.releaseMs > 1.0f ? (elapsed - releaseAt) / T.releaseMs : 1.0f;
        st.phase = r >= 1.0f ? kDone : kRelease; st.s = min_jerk(r);
        return st;
    }
    if (elapsed < T.shapeMs) { st.phase = kShape; st.s = min_jerk(elapsed / T.shapeMs); return st; }
    const float c = elapsed - T.shapeMs;
    if (c < T.closeMs + T.lagMs) { st.phase = kClose; st.s = c; return st; }   // raw ms: per bone below
    st.phase = kHold; st.s = 1.0f;
    return st;
}
// The close progress of one bone, `along` in [0,1] from the wrist to the fingertips.
static inline float per_bone_close(const Timing& T, float closeElapsedMs, float along)
{
    const float a = along < 0.0f ? 0.0f : along > 1.0f ? 1.0f : along;
    const float t = T.closeMs > 1.0f ? (closeElapsedMs - a * T.lagMs) / T.closeMs : 1.0f;
    return min_jerk(t);
}
// The earliest moment (ms after the press) the release may begin: the fist has formed and held.
static inline float earliest_release(const Timing& T) { return T.shapeMs + T.closeMs + T.lagMs + T.holdMs; }

// ---- rigid blending of 3x4 skinning matrices (row-major, translation in column 3) ----------

struct Quat { float w, x, y, z; };

static inline Quat quat_from_rot(const float* R)   // R: 3x3 row-major, proper rotation
{
    Quat q; const float tr = R[0] + R[4] + R[8];
    if (tr > 0.0f) {
        const float s = sqrtf(tr + 1.0f) * 2.0f;
        q.w = 0.25f * s; q.x = (R[7] - R[5]) / s; q.y = (R[2] - R[6]) / s; q.z = (R[3] - R[1]) / s;
    } else if (R[0] > R[4] && R[0] > R[8]) {
        const float s = sqrtf(1.0f + R[0] - R[4] - R[8]) * 2.0f;
        q.w = (R[7] - R[5]) / s; q.x = 0.25f * s; q.y = (R[1] + R[3]) / s; q.z = (R[2] + R[6]) / s;
    } else if (R[4] > R[8]) {
        const float s = sqrtf(1.0f + R[4] - R[0] - R[8]) * 2.0f;
        q.w = (R[2] - R[6]) / s; q.x = (R[1] + R[3]) / s; q.y = 0.25f * s; q.z = (R[5] + R[7]) / s;
    } else {
        const float s = sqrtf(1.0f + R[8] - R[0] - R[4]) * 2.0f;
        q.w = (R[3] - R[1]) / s; q.x = (R[2] + R[6]) / s; q.y = (R[5] + R[7]) / s; q.z = 0.25f * s;
    }
    const float n = sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    if (n > 1e-12f) { q.w /= n; q.x /= n; q.y /= n; q.z /= n; } else { q.w = 1; q.x = q.y = q.z = 0; }
    return q;
}
static inline void rot_from_quat(const Quat& q, float* R)
{
    const float w = q.w, x = q.x, y = q.y, z = q.z;
    R[0] = 1 - 2 * (y * y + z * z); R[1] = 2 * (x * y - w * z);     R[2] = 2 * (x * z + w * y);
    R[3] = 2 * (x * y + w * z);     R[4] = 1 - 2 * (x * x + z * z); R[5] = 2 * (y * z - w * x);
    R[6] = 2 * (x * z - w * y);     R[7] = 2 * (y * z + w * x);     R[8] = 1 - 2 * (x * x + y * y);
}
static inline Quat quat_mul(const Quat& a, const Quat& b)
{
    Quat r;
    r.w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z;
    r.x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y;
    r.y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x;
    r.z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w;
    return r;
}
static inline void quat_rotate(const Quat& q, const float* v, float* o)
{
    float R[9]; rot_from_quat(q, R);
    o[0] = R[0] * v[0] + R[1] * v[1] + R[2] * v[2];
    o[1] = R[3] * v[0] + R[4] * v[1] + R[5] * v[2];
    o[2] = R[6] * v[0] + R[7] * v[1] + R[8] * v[2];
}

// Split a 3x4 into a uniform scale, a proper rotation and a translation. False when the 3x3
// is degenerate or a reflection (then the caller keeps its input).
static inline bool split_3x4(const float* M, float* scale, Quat* q, float* t)
{
    const float det = M[0] * (M[5] * M[10] - M[6] * M[9]) - M[1] * (M[4] * M[10] - M[6] * M[8]) +
                      M[2] * (M[4] * M[9] - M[5] * M[8]);
    if (!(det > 1e-12f)) return false;
    const float k = cbrtf(det);
    const float R[9] = { M[0] / k, M[1] / k, M[2] / k, M[4] / k, M[5] / k, M[6] / k, M[8] / k, M[9] / k, M[10] / k };
    *scale = k; *q = quat_from_rot(R); t[0] = M[3]; t[1] = M[7]; t[2] = M[11];
    return true;
}

// out = the screw blend of A (s = 0) and B (s = 1). The motion D = B * inv(A) (scale aside) is a
// rotation by theta about an axis u through a point c, plus a slide d along u; the blend applies
// that rotation by s*theta about the same axis and s*d of the slide, then A. Uniform scale is
// blended linearly. Falls back to a straight translation blend when theta is tiny. Returns false
// (and copies A when s < 0.5, else B) when either matrix is not a scaled rotation.
static inline bool screw_blend_3x4(const float* A, const float* B, float s, float* out)
{
    float ka, kb, ta[3], tb[3]; Quat qa, qb;
    if (!split_3x4(A, &ka, &qa, ta) || !split_3x4(B, &kb, &qb, tb)) {
        memcpy(out, s < 0.5f ? A : B, sizeof(float) * 12);
        return false;
    }
    // qd = qb * conj(qa), the shorter way round
    const Quat qac = { qa.w, -qa.x, -qa.y, -qa.z };
    Quat qd = quat_mul(qb, qac);
    if (qd.w < 0) { qd.w = -qd.w; qd.x = -qd.x; qd.y = -qd.y; qd.z = -qd.z; }
    const float vn = sqrtf(qd.x * qd.x + qd.y * qd.y + qd.z * qd.z);
    const float theta = 2.0f * atan2f(vn, qd.w);
    // the motion's translation: tb = Rd * ta + td
    float rta[3]; quat_rotate(qd, ta, rta);
    const float td[3] = { tb[0] - rta[0], tb[1] - rta[1], tb[2] - rta[2] };
    Quat qs; float t[3];
    if (theta < 1e-4f) {
        qs = { 1, 0, 0, 0 };
        for (int i = 0; i < 3; i++) t[i] = ta[i] + s * (tb[i] - ta[i]);
    } else {
        const float u[3] = { qd.x / vn, qd.y / vn, qd.z / vn };
        const float d = td[0] * u[0] + td[1] * u[1] + td[2] * u[2];
        const float tp[3] = { td[0] - d * u[0], td[1] - d * u[1], td[2] - d * u[2] };
        const float ut[3] = { u[1] * tp[2] - u[2] * tp[1], u[2] * tp[0] - u[0] * tp[2], u[0] * tp[1] - u[1] * tp[0] };
        const float cot = cosf(0.5f * theta) / sinf(0.5f * theta);
        const float c[3] = { 0.5f * (tp[0] + cot * ut[0]), 0.5f * (tp[1] + cot * ut[1]), 0.5f * (tp[2] + cot * ut[2]) };
        const float h = 0.5f * s * theta;
        qs = { cosf(h), u[0] * sinf(h), u[1] * sinf(h), u[2] * sinf(h) };
        const float rel[3] = { ta[0] - c[0], ta[1] - c[1], ta[2] - c[2] };
        float rr[3]; quat_rotate(qs, rel, rr);
        for (int i = 0; i < 3; i++) t[i] = rr[i] + c[i] + s * d * u[i];
    }
    const Quat q = quat_mul(qs, qa);
    float R[9]; rot_from_quat(q, R);
    const float k = ka + s * (kb - ka);
    for (int r = 0; r < 3; r++) {
        out[r * 4 + 0] = k * R[r * 3 + 0]; out[r * 4 + 1] = k * R[r * 3 + 1]; out[r * 4 + 2] = k * R[r * 3 + 2];
        out[r * 4 + 3] = t[r];
    }
    return true;
}

// ---- which bone is a finger bone's parent ----------------------------------------------------
//
// WHY THE BLEND NEEDS THE HIERARCHY (found by tools/blender/grab_verify.py, 2026-10-05). Blended
// against the WRIST, a fingertip bone has to turn by the sum of its three joints' curls: about
// 240 degrees for a fist. A rotation blend takes the shorter way round, so the tip went about
// 120 degrees BACKWARDS, and the joints opened gaps of 8.6 units on a 10.8 unit finger. Blended
// against its PARENT bone, each joint turns by its own curl (under 110 degrees) and the chain
// stays joined by construction.
//
// The palette does not carry the hierarchy, and its order is not the skeleton's. The parent is
// found from the pose itself: in any pose, a child moves against its true parent by a pure turn
// about their shared joint, which lies between the two bones' vertex centroids. So for a
// candidate parent p, Q = inv(P[p]) * P[c] (palette matrices: Q is in reference-pose space) has a
// point on the segment between the centroids that it leaves where it is. joint_residual() is how
// far the best point on that segment moves; the true parent reads about zero. Candidates are
// limited to the bones the mesh itself joins to the child (a vertex weighted to both) and that
// sit nearer the wrist; ties are broken toward the nearer centroid.

// min over t in [0,1] of |(Q - I) (a + t (b - a))|: how far the best point between a and b moves.
static inline float joint_residual(const float* Q, const float* a, const float* b)
{
    // (Q - I) x = M x + q, with M = Q's 3x3 minus I and q its translation
    float Ma[3], Md[3];
    const float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    for (int r = 0; r < 3; r++) {
        Ma[r] = Q[r * 4 + 0] * a[0] + Q[r * 4 + 1] * a[1] + Q[r * 4 + 2] * a[2] + Q[r * 4 + 3] - a[r];
        Md[r] = Q[r * 4 + 0] * d[0] + Q[r * 4 + 1] * d[1] + Q[r * 4 + 2] * d[2] - d[r];
    }
    const float dd = Md[0] * Md[0] + Md[1] * Md[1] + Md[2] * Md[2];
    float t = dd > 1e-12f ? -(Ma[0] * Md[0] + Ma[1] * Md[1] + Ma[2] * Md[2]) / dd : 0.0f;
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    const float e[3] = { Ma[0] + t * Md[0], Ma[1] + t * Md[1], Ma[2] + t * Md[2] };
    return sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
}
// The residual as a fraction of how far the segment's ends move. A true joint between the two
// centroids: the ends swing, the joint stays (well under 1). Two SIBLING knuckles curl almost
// alike, so every point between them drifts by about the same small amount (near 1): measured
// raw, that drift is as small as a real joint's and the Blender check picked a neighbour knuckle
// as the parent twice. A bone that rides its parent rigidly (an end bone) barely moves at all:
// below `still` units it reads 0, a perfect fit.
static inline float joint_residual_ratio(const float* Q, const float* a, const float* b, float still)
{
    float ma = 0, mb = 0;
    for (int r = 0; r < 3; r++) {
        const float da = Q[r * 4 + 0] * a[0] + Q[r * 4 + 1] * a[1] + Q[r * 4 + 2] * a[2] + Q[r * 4 + 3] - a[r];
        const float db = Q[r * 4 + 0] * b[0] + Q[r * 4 + 1] * b[1] + Q[r * 4 + 2] * b[2] + Q[r * 4 + 3] - b[r];
        ma += da * da; mb += db * db;
    }
    const float m = sqrtf(ma > mb ? ma : mb);
    if (m < still) return 0.0f;
    return joint_residual(Q, a, b) / m;
}
// The score of p as c's parent (lower is better): the residual ratio plus a light pull toward
// the nearer centroid, `distWeight` per unit.
static inline float parent_score(const float* Q, const float* cenP, const float* cenC, float distWeight)
{
    const float dx = cenC[0] - cenP[0], dy = cenC[1] - cenP[1], dz = cenC[2] - cenP[2];
    return joint_residual_ratio(Q, cenP, cenC, 0.2f) + distWeight * sqrtf(dx * dx + dy * dy + dz * dz);
}

// A pose sampled from the game is trusted only if the state that makes it the RIGHT pose (both
// hands empty for the open hand, the sword in the right hand for the fist) still holds `holdMs`
// after the sample was taken. The inventory read that says "empty" lags the hand animation: the
// first frames of drawing the Heart, or of a crossbow, still read empty while the fingers already
// close on the item, and sampling every frame copied that grip into the "open" pose (2026-10-05).
// tick() once per frame: `inState` is the state, `sample` this frame's pose (n matrices, 3x4).
// Any frame out of state drops the candidate. Returns true on the frame a candidate is committed
// to `good`.
template <int N>
struct PoseLatch {
    float cand[N][12], good[N][12];
    double candAt = -1.0;
    bool candOk = false, haveGood = false;
    uint32_t commits = 0, dropped = 0;
    bool tick(double nowMs, bool inState, const float (*sample)[12], int n, double holdMs)
    {
        if (n > N) n = N;
        if (!inState) { if (candOk) ++dropped; candOk = false; return false; }
        bool committed = false;
        if (candOk && nowMs - candAt >= holdMs) {
            std::memcpy(good, cand, sizeof(float) * 12 * (size_t)n);
            haveGood = true; ++commits; committed = true; candOk = false;
        }
        if (!candOk) { std::memcpy(cand, sample, sizeof(float) * 12 * (size_t)n); candAt = nowMs; candOk = true; }
        return committed;
    }
};

}  // namespace grab
}  // namespace dvr
