// core/vr/hud_anchor.h - the pure placement math behind the HUD anchors (VR-117).
//
// Two anchors show the game's HUD in the headset: a WINDOW in front of the
// player (a VIEW-space quad, or a LOCAL-space one parked where the player
// recentred) and the tracked HAND (a LOCAL-space quad placed from the grip
// pose, the way build 38.92's wrist panel was placed from the controller). The
// runtime layer submits the quads; the decisions about WHERE live here, with
// no OpenXR handle in sight, so tools/hud-anchor-tests.cpp can fail them on
// the host before a headset is asked for.
//
// Conventions (XR): right +X, up +Y, forward -Z, metres, quaternions xyzw.
// A quad layer's face looks along its own +Z (quat_facing in the runtime aims
// +Z at the eyes), so "the panel's normal" below means the pose's +Z axis.
//
// The 38.92 rules this keeps, verbatim in intent (ENGINE_NOTES, "The HUD and
// the cinematics: what the original did", section 4):
//   - the panel takes the controller's POSITION; billboarding gives it its
//     orientation (FollowGrip is the new option, off by default);
//   - it keeps world up as its up axis, so it never rolls with the head;
//   - hide when it is at or behind the face, when the hand is at the eye, and
//     when the billboard's right vector degenerates (the hand nearly overhead).
// One deliberate difference: the lift is along WORLD up here. 38.92 lifted
// along HEAD up, which differs only with the head pitched, and the headset is
// the judge of which reads better.
#pragma once
#include "core/util/xr_math.h"
#include <cmath>
#include <cstdint>

namespace dvr::hudanchor {
// Reading panels follow grip position, with depth along the camera normal.
// Grip rotation and wrist-panel calibration deliberately do not enter this path.
inline void camera_panel_position(const float grip[3],const float camera[4],float distance,float out[3]) {
    const float unit[3]={0,0,-1};float forward[3];
    dvr::xrmath::quat_rotate(camera[0],camera[1],camera[2],camera[3],unit,forward);
    for(int k=0;k<3;++k) out[k]=grip[k]+forward[k]*distance;
}


// A private element texture can show the whole image without stretching or
// clipping at its initial identification rectangle. Preserve every reference
// pixel's world location while expanding the transparent surrounding panel.
inline void expand_reference_panel(const float r[4],float aspect,float& width,float offset[2]) {
    const float rw=r[2]-r[0];
    if(!(rw>0)) return;
    width/=rw;
    offset[0]-=((r[0]+r[2])*.5f-.5f)*width;
    offset[1]-=(.5f-(r[1]+r[3])*.5f)*aspect*width;
}

// Watch-face tilt. The grip pose per the OpenXR specification (6.3, "grip"):
// +X is the ray normal to the open palm, AWAY from the palm on the left hand
// and INTO the palm on the right; -Z runs through the tube the closed fingers
// make, little finger toward thumb; +Y completes the right-handed frame. With
// the hand held palm-down in front of you, +Y therefore points along the
// knuckles, away from the body, and the back of the hand faces grip +X on the
// LEFT hand and grip -X on the RIGHT.
// A panel lying on the back of the hand like a watch face has its normal (+Z)
// along that back-of-hand axis and its up (+Y) along grip +Y, so it reads like
// a page on a table. That is a +90 degree turn about grip Y for the left hand
// and -90 for the right (R_y maps +Z to (sin, 0, cos)). tools/hud-anchor-tests
// asserts both mappings; the headset judges the sign once.
inline void tilt_right(float out[4]) { out[0] = 0.0f; out[1] = -0.70710678f; out[2] = 0.0f; out[3] = 0.70710678f; }
inline void tilt_left(float out[4])  { out[0] = 0.0f; out[1] = 0.70710678f;  out[2] = 0.0f; out[3] = 0.70710678f; }

// FollowGrip orientation: the grip quaternion times the watch-face tilt, then
// the user's own tilt about the panel's right axis (a nod toward or away from
// the eyes). Applied in the panel's own frame, so it behaves at every grip
// orientation, not only the tuning pose (the "pivot that breaks everything"
// lesson in xr_math.h).
inline void follow_grip_orientation(const float grip[4], int hand, float userTiltDeg, float out[4]) {
    float tilt[4];
    if (hand == 1) tilt_right(tilt); else tilt_left(tilt);
    float base[4];
    dvr::xrmath::quat_mul(grip, tilt, base);
    float nod[4];
    dvr::xrmath::quat_axis_angle(1.0f, 0.0f, 0.0f, userTiltDeg * 3.14159265f / 180.0f, nod);
    dvr::xrmath::quat_mul(base, nod, out);
}

// The panel's centre: grip + R(grip) * offset (the user's x/y/z in the hand's
// own frame) + lift along world up.
inline void wrist_position(const float grip[3], const float gripQ[4], const float offset[3],
                           float liftM, float out[3]) {
    float r[3];
    dvr::xrmath::quat_rotate(gripQ[0], gripQ[1], gripQ[2], gripQ[3], offset, r);
    out[0] = grip[0] + r[0];
    out[1] = grip[1] + r[1] + liftM;
    out[2] = grip[2] + r[2];
}

// The three hide rules, each answered separately so the log can name which one
// bit. `toHead` is head - panel (unnormalised); `headFwd` the head's forward.
inline bool too_near(const float toHead[3], float minM = 0.05f) {
    const float d2 = toHead[0] * toHead[0] + toHead[1] * toHead[1] + toHead[2] * toHead[2];
    return d2 < minM * minM;
}
// At or behind the face: the panel sits less than `minAheadM` in front of the
// eyes along the head's forward (38.92: pr.z > -0.06 in head space).
inline bool behind_face(const float toHead[3], const float headFwd[3], float minAheadM = 0.06f) {
    // toHead = head - panel, so panel - head = -toHead; its component along
    // headFwd is how far AHEAD of the eyes the panel is.
    const float ahead = -(toHead[0] * headFwd[0] + toHead[1] * headFwd[1] + toHead[2] * headFwd[2]);
    return ahead < minAheadM;
}
// The billboard keeps world up; when the panel is nearly overhead the right
// vector (up x normal) collapses and the panel would spin. quat_facing clamps
// that case instead of refusing, so the caller tests it here first.
inline bool billboard_degenerate(const float toHead[3], float minRight = 0.2f) {
    const float len = std::sqrt(toHead[0] * toHead[0] + toHead[1] * toHead[1] + toHead[2] * toHead[2]);
    if (len < 1e-6f) return true;
    const float n[3] = {toHead[0] / len, toHead[1] / len, toHead[2] / len};
    // |up x n| with up = (0,1,0): (n.z, 0, -n.x)
    const float r = std::sqrt(n[2] * n[2] + n[0] * n[0]);
    return r < minRight;
}

// A quad's pixel rectangle inside a texture of texW x texH, from a normalised
// sub-rectangle (u0,v0,u1,v1) and the wanted metres (width, height). height 0
// means "the texture's own aspect": the whole sub-rectangle is shown and the
// height follows. A given height crops the sub-rectangle CENTRED to the asked
// aspect so pixels keep their shape; stretching text is never an option.
// Opening orientation belongs to the panel, not each new head pose.
struct OpeningOrientation {
    bool valid=false;
    float q[4]={0,0,0,1};
    void reset() {valid=false;}
    bool capture(const float* camera) {
        if(valid) return true;
        float norm=0;
        for(int i=0;i<4;++i) {if(!std::isfinite(camera[i])) return false;norm+=camera[i]*camera[i];}
        if(norm<.0001f) return false;
        norm=std::sqrt(norm);
        for(int i=0;i<4;++i) q[i]=camera[i]/norm;
        valid=true;return true;
    }
};

struct Crop { int32_t x, y, w, h; float widthM, heightM; };
inline Crop crop_rect(uint32_t texW, uint32_t texH, const float sub[4], float widthM, float heightM) {
    Crop c{};
    float u0 = sub[0], v0 = sub[1], u1 = sub[2], v1 = sub[3];
    if (!(u1 > u0) || !(v1 > v0)) { u0 = 0; v0 = 0; u1 = 1; v1 = 1; }
    float px = u0 * (float)texW, py = v0 * (float)texH;
    float pw = (u1 - u0) * (float)texW, ph = (v1 - v0) * (float)texH;
    if (pw < 1.0f) pw = 1.0f;
    if (ph < 1.0f) ph = 1.0f;
    c.widthM = widthM;
    if (heightM > 0.0f) {
        const float want = widthM / heightM;          // wanted aspect (w/h)
        const float have = pw / ph;
        if (have > want) {                            // too wide: trim the sides
            const float nw = ph * want;
            px += (pw - nw) * 0.5f; pw = nw;
        } else if (have < want) {                     // too tall: trim top and bottom
            const float nh = pw / want;
            py += (ph - nh) * 0.5f; ph = nh;
        }
        c.heightM = heightM;
    } else {
        c.heightM = widthM * ph / pw;
    }
    c.x = (int32_t)(px + 0.5f); c.y = (int32_t)(py + 0.5f);
    c.w = (int32_t)(pw + 0.5f); c.h = (int32_t)(ph + 0.5f);
    if (c.w < 1) c.w = 1;
    if (c.h < 1) c.h = 1;
    return c;
}

} // namespace dvr::hudanchor
