// core/gfx/clarity_math.h - the arithmetic behind the clarity passes (clarity.h),
// kept free of D3D and of the mod so the host test can hold it to numbers.
//
// Three questions live here:
//   * how big the resolved image is (the runtime's recommended pixel count at
//     the rendered aspect, or no resolve at all when the render is not larger);
//   * where a pixel of the current eye image was in the previous image of the
//     same eye, from the two camera rotations the engine rendered them with;
//   * whether a history can be kept at all (a snap turn, a Blink or a load is
//     not something to blend across).
#pragma once
#include <math.h>
#include <stdint.h>

namespace dvr::clarity {

// ---- the resolve size ---------------------------------------------------------
// The target is the runtime's recommended pixel count (its "1.0") at the
// rendered aspect. A render within `slack` of it (2 % by default) is not
// resolved: a 1.01x filter would only blur. Returns true when a resolve applies.
inline bool resolve_size(uint32_t w, uint32_t h, uint32_t recW, uint32_t recH,
                         uint32_t* ow, uint32_t* oh, float slack = 1.02f) {
    *ow = w; *oh = h;
    if (!w || !h || !recW || !recH) return false;
    const double target = (double)recW * (double)recH;
    const double have = (double)w * (double)h;
    if (have <= target * slack) return false;
    const double aspect = (double)w / (double)h;
    uint32_t tw = (uint32_t)(sqrt(target * aspect) + 0.5);
    uint32_t th = (uint32_t)((double)tw / aspect + 0.5);
    if (tw < 16 || th < 16 || tw >= w || th >= h) return false;
    *ow = tw; *oh = th;
    return true;
}

// ---- the camera --------------------------------------------------------------
// UE3's FRotationMatrix: rows are the camera's forward (X), right (Y) and up
// (Z) in world space, from pitch/yaw/roll in DEGREES (the pose record stores
// the written rotator converted with the engine's own 65536 units per turn).
struct Basis { float f[3], r[3], u[3]; };

inline Basis basis_from_rotator(float pitchDeg, float yawDeg, float rollDeg) {
    const float k = 3.14159265358979f / 180.0f;
    const float sp = sinf(pitchDeg * k), cp = cosf(pitchDeg * k);
    const float sy = sinf(yawDeg * k), cy = cosf(yawDeg * k);
    const float sr = sinf(rollDeg * k), cr = cosf(rollDeg * k);
    Basis b;
    b.f[0] = cp * cy;                 b.f[1] = cp * sy;                 b.f[2] = sp;
    b.r[0] = sr * sp * cy - cr * sy;  b.r[1] = sr * sp * sy + cr * cy;  b.r[2] = -sr * cp;
    b.u[0] = -(cr * sp * cy + sr * sy); b.u[1] = cy * sr - cr * sp * sy; b.u[2] = cr * cp;
    return b;
}

inline float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// M maps a direction in the CURRENT camera's (forward, right, up) coordinates
// to the PREVIOUS camera's: d_prev = M * d_cur. Row i = previous axis i dotted
// with each current axis.
struct Mat3 { float m[3][3]; };
inline Mat3 prev_from_cur(const Basis& prev, const Basis& cur) {
    const float* p[3] = { prev.f, prev.r, prev.u };
    const float* c[3] = { cur.f, cur.r, cur.u };
    Mat3 o;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) o.m[i][j] = dot3(p[i], c[j]);
    return o;
}

// The angle between two camera bases (degrees), for the history reset.
inline float basis_angle_deg(const Basis& a, const Basis& b) {
    const Mat3 m = prev_from_cur(a, b);
    float t = (m.m[0][0] + m.m[1][1] + m.m[2][2] - 1.0f) * 0.5f;
    if (t > 1.0f) t = 1.0f;
    if (t < -1.0f) t = -1.0f;
    return acosf(t) * 57.2957795f;
}

// Where the pixel at image uv (0..1, v down) of the current view lands in the
// previous view, for a symmetric projection with tangents tanH/tanV. False when
// the direction points behind the previous camera. The shader does exactly this.
inline bool reproject_uv(const Mat3& m, float tanH, float tanV, float u, float v,
                         float* pu, float* pv) {
    const float x = u * 2.0f - 1.0f, y = 1.0f - v * 2.0f;
    const float d[3] = { 1.0f, x * tanH, y * tanV };
    float q[3];
    for (int i = 0; i < 3; ++i) q[i] = m.m[i][0] * d[0] + m.m[i][1] * d[1] + m.m[i][2] * d[2];
    if (q[0] <= 1e-4f) return false;
    *pu = 0.5f + 0.5f * (q[1] / (q[0] * tanH));
    *pv = 0.5f - 0.5f * (q[2] / (q[0] * tanV));
    return true;
}

// ---- the history --------------------------------------------------------------
// Why a history is dropped instead of blended. Kept as a reason so the status
// line can say which one is firing: a counter of resets with no cause cannot
// tell a snap turn from a broken record.
enum class Reset { None = 0, First, Size, Record, Turn, Move, Fov, Off, Count };
inline const char* reset_name(Reset r) {
    switch (r) {
    case Reset::None:   return "kept";
    case Reset::First:  return "first";
    case Reset::Size:   return "size";
    case Reset::Record: return "record";
    case Reset::Turn:   return "turn";
    case Reset::Move:   return "move";
    case Reset::Fov:    return "fov";
    case Reset::Off:    return "off";
    default:            return "?";
    }
}

struct View {
    bool     ok = false;
    float    pitch = 0, yaw = 0, roll = 0;   // degrees
    float    pos[3] = {0, 0, 0};             // engine units
    bool     posOk = false;
    float    tanH = 0, tanV = 0;
    uint32_t w = 0, h = 0;
};

// A snap turn is 30-45 degrees in one step and a Blink covers metres in one
// frame; the limits sit well above a fast head turn at 90 frames per eye per
// second (about 7 deg) and a sprint (about 5 uu per frame).
inline Reset keep_history(const View& prev, const View& cur, float maxTurnDeg = 20.0f,
                          float maxMoveUu = 60.0f) {
    if (!cur.ok) return Reset::Record;
    if (!prev.ok) return Reset::First;
    if (prev.w != cur.w || prev.h != cur.h) return Reset::Size;
    if (fabsf(prev.tanH - cur.tanH) > 0.002f * cur.tanH || fabsf(prev.tanV - cur.tanV) > 0.002f * cur.tanV)
        return Reset::Fov;
    const Basis a = basis_from_rotator(prev.pitch, prev.yaw, prev.roll);
    const Basis b = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
    if (basis_angle_deg(a, b) > maxTurnDeg) return Reset::Turn;
    if (prev.posOk && cur.posOk) {
        const float dx = cur.pos[0] - prev.pos[0], dy = cur.pos[1] - prev.pos[1], dz = cur.pos[2] - prev.pos[2];
        if (sqrtf(dx * dx + dy * dy + dz * dz) > maxMoveUu) return Reset::Move;
    }
    return Reset::None;
}

// How much of the temporal accumulation to give up for camera motion between two
// frames of one eye: 0 still (a walk starts to count at ~1 uu, full at ~6 uu per
// eye frame, about a brisk walk; a turn at 0.25..1.5 degrees per frame).
inline float motion_weight(float moveUu, float turnDeg) {
    float a = (moveUu - 1.0f) / 5.0f, b = (turnDeg - 0.25f) / 1.25f;
    float m = a > b ? a : b;
    return m < 0.0f ? 0.0f : (m > 1.0f ? 1.0f : m);
}

// ---- the resolve filter --------------------------------------------------------
// Mitchell-Netravali with B = C = 1/3, support 2 in OUTPUT pixels. The shader
// scales it by the downscale factor so every source pixel under an output
// pixel's footprint contributes; a single bilinear tap at a 1.7x step reads
// about a third of them and aliases. Returned unnormalised; the shader divides
// by the sum of the weights it used.
inline float mitchell(float x) {
    const float B = 1.0f / 3.0f, C = 1.0f / 3.0f;
    x = fabsf(x);
    if (x < 1.0f) return ((12 - 9 * B - 6 * C) * x * x * x + (-18 + 12 * B + 6 * C) * x * x + (6 - 2 * B)) / 6.0f;
    if (x < 2.0f) return ((-B - 6 * C) * x * x * x + (6 * B + 30 * C) * x * x + (-12 * B - 48 * C) * x + (8 * B + 24 * C)) / 6.0f;
    return 0.0f;
}

} // namespace dvr::clarity
