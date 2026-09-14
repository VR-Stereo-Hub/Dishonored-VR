// tools/hud-anchor-tests.cpp - the HUD anchors' placement math on the host
// (VR-117). Run by tools/hud-anchor-host.ps1; never launches the game.
#include "core/vr/hud_anchor.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
static bool near3(const float a[3], float x, float y, float z, float eps = 1e-4f) {
    return std::fabs(a[0] - x) < eps && std::fabs(a[1] - y) < eps && std::fabs(a[2] - z) < eps;
}
static void rot(const float q[4], const float v[3], float out[3]) {
    dvr::xrmath::quat_rotate(q[0], q[1], q[2], q[3], v, out);
}
int main() {
    using namespace dvr::hudanchor;
    const float zAxis[3] = {0, 0, 1}, yAxis[3] = {0, 1, 0}, xAxis[3] = {1, 0, 0};
    float q[4], o[3];
    // The watch-face tilt maps the panel's normal (+Z) onto the back of the
    // hand (grip -X right, +X left) and keeps the panel's up along grip +Y.
    tilt_right(q); rot(q, zAxis, o); check(near3(o, -1, 0, 0), "right tilt: panel normal = grip -X (the back of the right hand)");
    rot(q, yAxis, o);               check(near3(o, 0, 1, 0), "right tilt: panel up = grip +Y");
    rot(q, xAxis, o);               check(near3(o, 0, 0, 1), "right tilt: panel right = grip +Z");
    tilt_left(q);  rot(q, zAxis, o); check(near3(o, 1, 0, 0), "left tilt: panel normal = grip +X (the back of the left hand)");
    rot(q, yAxis, o);               check(near3(o, 0, 1, 0), "left tilt: panel up = grip +Y");
    // Unit quaternions.
    for (int h = 0; h < 2; ++h) {
        if (h) tilt_right(q); else tilt_left(q);
        check(std::fabs(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3] - 1.0f) < 1e-5f, "tilt is a unit quaternion");
    }
    // FollowGrip with an identity grip and no user tilt equals the tilt itself;
    // a +90 nod turns the panel's normal from -X toward... the nod is about the
    // panel's own right axis (grip +Z for the right hand), so the normal moves in
    // the grip X/Y plane.
    {
        const float ident[4] = {0, 0, 0, 1};
        float fq[4];
        follow_grip_orientation(ident, 1, 0.0f, fq);
        rot(fq, zAxis, o); check(near3(o, -1, 0, 0), "follow grip, identity grip, right: normal = -X");
        follow_grip_orientation(ident, 1, 90.0f, fq);
        rot(fq, zAxis, o); check(std::fabs(o[2]) < 1e-4f, "a nod about the panel's right axis keeps the normal in the grip X/Y plane");
        check(std::fabs(std::fabs(o[1]) - 1.0f) < 1e-4f, "a 90 degree nod turns the normal onto grip Y");
        // A yawed grip carries the panel with it.
        float yaw90[4]; dvr::xrmath::quat_axis_angle(0, 1, 0, 3.14159265f * 0.5f, yaw90);
        follow_grip_orientation(yaw90, 1, 0.0f, fq);
        rot(fq, zAxis, o); check(near3(o, 0, 0, 1), "a +90 yaw grip turns the right hand's normal from -X to +Z");
    }
    // Wrist position: grip + R(q)*offset + lift along world up.
    {
        const float grip[3] = {1, 2, 3};
        const float ident[4] = {0, 0, 0, 1};
        const float off[3] = {0.1f, 0.0f, -0.2f};
        wrist_position(grip, ident, off, 0.06f, o);
        check(near3(o, 1.1f, 2.06f, 2.8f), "identity grip: offset adds, lift is +Y");
        float yaw90[4]; dvr::xrmath::quat_axis_angle(0, 1, 0, 3.14159265f * 0.5f, yaw90);
        wrist_position(grip, yaw90, off, 0.0f, o);
        // R_y(90): x' = z, z' = -x  ->  (0.1, 0, -0.2) -> (-0.2, 0, -0.1)
        check(near3(o, 1.0f - 0.2f, 2.0f, 3.0f - 0.1f), "a 90 degree yaw grip rotates the offset with it");
    }
    // The hide rules.
    {
        // toHead = head - panel. The head looks along -Z, so a panel AHEAD of
        // the eyes has a more negative z than the head: toHead.z is POSITIVE.
        const float fwd[3] = {0, 0, -1};
        const float ahead[3] = {0, 0.3f, 0.5f};        // the panel is 0.5 m ahead, 0.3 m below
        check(!behind_face(ahead, fwd), "a panel 0.5 m ahead is not behind the face");
        const float behind[3] = {0, 0, -0.3f};         // the panel is 0.3 m BEHIND the head
        check(behind_face(behind, fwd), "a panel behind the head is hidden");
        const float grazing[3] = {0, 0, 0.04f};        // 4 cm ahead: at the face
        check(behind_face(grazing, fwd), "a panel 4 cm ahead is at the face");
        const float atEye[3] = {0.02f, 0.02f, -0.02f};
        check(too_near(atEye), "a panel at the eye is hidden");
        check(!too_near(ahead), "a panel 0.5 m away is not too near");
        const float overhead[3] = {0, -1.0f, -0.05f};  // the panel almost straight above... head - panel = (0,-1,..): panel above the head
        check(billboard_degenerate(overhead), "a panel nearly overhead degenerates the billboard");
        const float side[3] = {0.5f, -0.3f, -0.5f};
        check(!billboard_degenerate(side), "an ordinary panel does not");
        // Within 11.5 degrees of vertical: sin(11.5 deg) = 0.199 < 0.2 -> degenerate; 12 deg -> not.
        const float deg11[3] = {std::sin(11.0f * 3.14159265f / 180.0f), -std::cos(11.0f * 3.14159265f / 180.0f), 0.0f};
        const float deg12[3] = {std::sin(12.0f * 3.14159265f / 180.0f), -std::cos(12.0f * 3.14159265f / 180.0f), 0.0f};
        check(billboard_degenerate(deg11), "11 degrees off vertical: degenerate");
        check(!billboard_degenerate(deg12), "12 degrees off vertical: fine");
    }
    // Crops.
    {
        const float full[4] = {0, 0, 1, 1};
        Crop c = crop_rect(1920, 1080, full, 1.0f, 0.0f);
        check(c.x == 0 && c.y == 0 && c.w == 1920 && c.h == 1080, "height 0: the whole texture");
        check(std::fabs(c.heightM - 1080.0f / 1920.0f) < 1e-5f, "height 0: the texture's aspect");
        c = crop_rect(1920, 1080, full, 1.0f, 1.0f);
        check(c.w == 1080 && c.h == 1080 && c.x == 420 && c.y == 0, "a 1:1 crop of 16:9 trims the sides to 1080 wide at x=420");
        check(std::fabs(c.widthM - 1.0f) < 1e-6f && std::fabs(c.heightM - 1.0f) < 1e-6f, "the asked metres are kept");
        c = crop_rect(1000, 2000, full, 1.0f, 1.0f);
        check(c.w == 1000 && c.h == 1000 && c.y == 500, "a 1:1 crop of a tall texture trims top and bottom");
        const float sub[4] = {0.25f, 0.5f, 0.75f, 1.0f};
        c = crop_rect(2000, 1000, sub, 0.5f, 0.0f);
        check(c.x == 500 && c.y == 500 && c.w == 1000 && c.h == 500, "a sub-rectangle maps to pixels");
        check(std::fabs(c.heightM - 0.25f) < 1e-6f, "the sub-rectangle's own aspect");
        const float bad[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        c = crop_rect(100, 100, bad, 1.0f, 0.0f);
        check(c.w == 100 && c.h == 100, "an empty sub-rectangle falls back to the whole texture");
    }
    std::printf("%u hud-anchor checks passed\n", checks);
    return 0;
}
