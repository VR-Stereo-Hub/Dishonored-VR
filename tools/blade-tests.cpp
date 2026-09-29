// tools/blade-tests.cpp - the held sword's blade maths (VR-173), on the host.
// Compiles src/game/dishonored/blade_math.h alone: what runs here is what the proxy runs.
// Run: tools\blade-host.ps1. Never launches the game.
#include "game/dishonored/blade_math.h"
#include "game/dishonored/hands/weapon_frame.h"
#include "game/dishonored/fire_aim_math.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static int checks = 0, failed = 0;
static void check(bool ok, const char* what) {
    ++checks;
    if (!ok) { ++failed; std::printf("FAIL %d: %s\n", checks, what); }
}
static bool near_f(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

using namespace dvr::blade;
using namespace dvr::hf;

// Twelve floats per bone, row-major 3x4, translation in the fourth column.
static void put_bone(float* palette, int bone, const Mat3& r, const float* t) {
    float* m = palette + bone * 12;
    for (int row = 0; row < 3; ++row) {
        for (int c = 0; c < 3; ++c) m[row*4+c] = r.m[row*3+c];
        m[row*4+3] = t[row];
    }
}

// A sword the way the game's is built: a hilt and guard on one bone, a blade on another,
// 2481 vertices, the blade along +X from the guard. Units are the mesh's own (uu).
static std::vector<Vertex> make_sword(int hiltBone, int bladeBone, float bladeLen, int n = 2481) {
    std::vector<Vertex> v;
    for (int i = 0; i < n; ++i) {
        Vertex p = {};
        p.w[0] = 1; p.bone[0] = (uint8_t)bladeBone;
        const float u = (float)(i % 97) / 96.0f, a = (float)i * 0.61803f;
        if (i % 3 == 0) {            // hilt and pommel: 18 uu behind the guard, 1.6 thick
            p.bone[0] = (uint8_t)hiltBone;
            p.pos[0] = -18.0f * u; p.pos[1] = 1.6f * std::cos(a); p.pos[2] = 1.6f * std::sin(a);
        } else if (i % 3 == 1 && i % 30 == 1) {   // the guard: across the blade, 7 wide
            p.bone[0] = (uint8_t)hiltBone;
            p.pos[0] = 0.5f; p.pos[1] = 7.0f * (u - 0.5f); p.pos[2] = 0.6f * std::sin(a);
        } else {                      // the blade: flat, tapering to the tip
            const float taper = 1.0f - u;
            p.pos[0] = bladeLen * u; p.pos[1] = 2.2f * taper * std::cos(a); p.pos[2] = 0.3f * std::sin(a);
        }
        v.push_back(p);
    }
    // The tip is ONE vertex, at an index the even subsample steps over.
    Vertex tip = {}; tip.w[0] = 1; tip.bone[0] = (uint8_t)bladeBone;
    tip.pos[0] = bladeLen + 1.5f;
    v[1001] = tip;
    return v;
}

int main() {
    // ---- skinning -------------------------------------------------------------------
    {
        float pal[4 * 12] = {};
        const float zero[3] = {0, 0, 0}, shift[3] = {10, 0, 0};
        put_bone(pal, 0, identity3(), zero);
        put_bone(pal, 1, identity3(), shift);
        put_bone(pal, 2, euler_xyz_deg_to_mat(0, 0, 90), zero);
        Vertex v = {}; v.pos[0] = 1; v.pos[1] = 2; v.pos[2] = 3; v.w[0] = 1; v.bone[0] = 0;
        float o[3];
        check(skin(v, pal, 12, o) && near_f(o[0], 1, 1e-6f) && near_f(o[1], 2, 1e-6f) && near_f(o[2], 3, 1e-6f), "identity bone returns the position");
        v.w[0] = 0.5f; v.w[1] = 0.5f; v.bone[1] = 1;
        check(skin(v, pal, 12, o) && near_f(o[0], 6, 1e-5f), "two bones blend by weight");
        v.bone[1] = 3;
        check(skin(v, pal, 12, o), "bone 3 is inside a 12 register palette");
        check(!skin(v, pal, 9, o), "a bone the palette does not carry refuses");
        v.w[0] = 0.2f; v.w[1] = 0.1f;
        check(!skin(v, pal, 12, o), "weights that sum to under a half refuse");
        v.w[0] = -1;
        check(!skin(v, pal, 12, o), "a negative weight refuses");
        Vertex d = {}; d.w[0] = 0.2f; d.w[2] = 0.7f; d.w[3] = 0.1f; d.bone[0] = 4; d.bone[2] = 9; d.bone[3] = 1;
        check(dominant_bone(d) == 9, "the dominant bone is the heaviest");
    }

    // ---- the fit, on a posed two-bone sword -----------------------------------------
    const float bladeLen = 70.0f;
    const std::vector<Vertex> sword = make_sword(2, 5, bladeLen);
    const Mat3 pose = euler_xyz_deg_to_mat(23, -41, 67);
    const float poseT[3] = {12, -7, 30};
    float pal[8 * 12] = {};
    for (int b = 0; b < 8; ++b) put_bone(pal, b, pose, poseT);
    std::vector<float> posed(sword.size() * 3);
    bool skinned = true;
    for (size_t i = 0; i < sword.size(); ++i) skinned = skinned && skin(sword[i], pal, 24, &posed[i*3]);
    check(skinned, "every vertex of the sword skins");
    Axis ax;
    check(fit((const float (*)[3])posed.data(), (int)sword.size(), 2.0f, ax), "a sword has a long axis");
    {
        const float bx[3] = {1, 0, 0}; float want[3];
        mulv3(pose, bx, want);
        const float dot = std::fabs(ax.fit.dir[0]*want[0] + ax.fit.dir[1]*want[1] + ax.fit.dir[2]*want[2]);
        check(dot > 0.9998f, "the fitted axis is the blade's direction within a degree, hilt and guard included");
        check(ax.fitted <= 1024 && ax.fitted >= 512, "the fit saw an even subsample, not every vertex");
        check(ax.highIndex == 1001 || ax.lowIndex == 1001, "the tip vertex is found although the subsample stepped over it");
        check(near_f(ax.high - ax.low, bladeLen + 1.5f + 18.0f, 0.6f), "the extent is pommel to tip");
        std::printf("synthetic sword: variance ratio %.1f to 1, %.1f uu long, fitted over %d of %d\n",
                    ax.fit.ratio, ax.high - ax.low, ax.fitted, (int)sword.size());
        // A sword with a hilt and a guard is long, and still nowhere near as one-dimensional
        // as a bolt: this one reads about 11 to 1, under the 16 to 1 the bolt reader demands.
        // That strictness cannot be borrowed for a blade.
        check(ax.fit.ratio > 5.0f, "the variance ratio is a sword's, not a box's");
        Axis strict;
        check(!fit((const float (*)[3])posed.data(), (int)sword.size(), 16.0f, strict), "and the bolt's 16 to 1 would refuse it");
    }
    {
        float cube[512][3];
        for (int i = 0; i < 512; ++i) { cube[i][0] = (float)(i % 8); cube[i][1] = (float)((i / 8) % 8); cube[i][2] = (float)(i / 64); }
        Axis c;
        check(!fit(cube, 512, 2.0f, c), "a cube has no long axis and refuses");
        check(!fit(cube, 8, 2.0f, c), "too few points refuse");
    }

    // ---- the fold: the SAME mesh with the blade bone turned back along the hilt --------
    {
        float folded[8 * 12];
        for (int b = 0; b < 8; ++b) put_bone(folded, b, pose, poseT);
        const Mat3 turned = mul3(pose, euler_xyz_deg_to_mat(0, 0, 178));   // the blade lies back over the grip
        put_bone(folded, 5, turned, poseT);
        std::vector<float> fp(sword.size() * 3);
        for (size_t i = 0; i < sword.size(); ++i) skin(sword[i], folded, 24, &fp[i*3]);
        Axis fa;
        const bool has = fit((const float (*)[3])fp.data(), (int)sword.size(), 2.0f, fa);
        // palm at the grip's middle, 9 uu behind the guard, 100 uu per metre
        float palmLocal[3] = {-9, 0, 0}, palmW[3]; mulv3(pose, palmLocal, palmW);
        for (int i = 0; i < 3; ++i) palmW[i] += poseT[i];
        auto to_palm = [&](const float* w, float* o) { for (int i = 0; i < 3; ++i) o[i] = (w[i] - palmW[i]) / 100.0f; };
        if (has) {
            float lo[3], hi[3]; to_palm(&fp[fa.lowIndex*3], lo); to_palm(&fp[fa.highIndex*3], hi);
            Bounds b; b.minReachM = 0.65f;      // a bound sized for THIS 0.8 m test blade
            const Segment s = segment(lo, hi, b);
            check(!s.ok, "a folded sword does not pass for an open one");
        } else check(true, "a folded sword has no long axis at all");
    }

    // ---- the segment in the palm frame ----------------------------------------------
    {
        Bounds b;
        const float pommel[3] = {-0.09f, 0.01f, 0.0f}, tip[3] = {0.80f, 0.02f, 0.0f};
        Segment s = segment(pommel, tip, b);
        check(s.ok && s.tipIsHigh == 1, "the far end is the tip");
        check(near_f(s.tip[0], 0.80f, 1e-6f), "the tip is the end vertex itself");
        check(near_f(s.base[0], 0.0f, 0.002f) && s.offPalmM < 0.03f, "the base is the blade line's nearest point to the palm");
        check(near_f(s.lengthM, 0.80f, 0.005f), "the blade is what lies ahead of the palm, not pommel to tip");
        Segment r = segment(tip, pommel, b);
        check(r.ok && r.tipIsHigh == -1 && near_f(r.tip[0], 0.80f, 1e-6f), "and it does not depend on which end the fit called high");
        check(apart(s, r).deg < 0.01f && apart(s, r).tipM < 1e-6f, "the two orders give one segment");
        // the palm beyond the pommel: the base is held at the end, never outside the sword
        const float p2[3] = {0.10f, 0, 0}, t2[3] = {0.90f, 0, 0};
        Segment o = segment(p2, t2, b);
        check(o.ok && near_f(o.base[0], 0.10f, 1e-6f), "the base never leaves the segment");
        const float a1[3] = {-0.40f, 0, 0}, a2[3] = {0.45f, 0, 0};
        check(segment(a1, a2, b).refuse == kEndsAlike, "two ends about as far away as each other refuse");
        const float s1[3] = {-0.05f, 0, 0}, s2[3] = {0.30f, 0, 0};
        check(segment(s1, s2, b).refuse == kTooShort, "a tip 0.30 m away is not an open blade");
        const float l1[3] = {-0.10f, 0, 0}, l2[3] = {0.95f, 0.95f, 0.0f};
        check(segment(l1, l2, b).refuse == kTooLong, "reach is a DISTANCE: 0.95 on two axes is 1.34 m and refuses");
        const float f1[3] = {-0.10f, 0.30f, 0}, f2[3] = {0.80f, 0.30f, 0};
        check(segment(f1, f2, b).refuse == kOffThePalm, "a blade line 0.30 m from the palm is not in the hand");
        const float n1[3] = {0, 0, 0}, n2[3] = {NAN, 0, 0};
        check(segment(n1, n2, b).refuse == kNotFinite, "a point that is not finite refuses");
    }

    // ---- the latch: a count AND a time ----------------------------------------------
    {
        Bounds b;
        const float pommel[3] = {-0.09f, 0.0f, 0.0f}, tip[3] = {0.80f, 0.0f, 0.0f};
        const Segment s = segment(pommel, tip, b);
        Latch l;
        bool got = false;
        for (int i = 0; i < 5; ++i) got = got || l.feed(s, 1000 + i * 10);
        check(!got && !l.have, "five agreeing draws in 50 ms do not latch");
        check(l.feed(s, 1400) && l.have, "the same candidate latches once 300 ms have passed");
        check(!l.feed(s, 1500), "a latched blade takes no more candidates");
        Latch m;
        const float moved[3] = {0.80f, 0.05f, 0.0f};
        const Segment t = segment(pommel, moved, b);
        m.feed(s, 0); m.feed(s, 100); m.feed(s, 200); m.feed(s, 300);
        check(!m.feed(t, 400) && m.votes == 1, "a candidate 5 cm away restarts the count");
        bool late = false;
        for (int i = 0; i < 4; ++i) late = m.feed(t, 500 + i * 100);
        check(late && near_f(m.kept.tip[1], 0.05f, 1e-6f), "and the blade that latches is the one that settled");
        Latch r; Segment bad = s; bad.ok = false; bad.refuse = kTooShort;
        for (int i = 0; i < 10; ++i) r.feed(bad, i * 100);
        check(!r.have && r.votes == 0, "a refused candidate never votes");
        l.forget();
        check(!l.have && l.votes == 0, "forget clears it");
    }

    // ---- the palm transport is the aim ray's ----------------------------------------
    for (int parity = -1; parity <= 1; parity += 2) for (int angle = -170; angle < 180; angle += 37) {
        const Mat3 rc = euler_xyz_deg_to_mat((float)angle, angle * 0.3f, -angle * 0.7f);
        const Mat3 grip = mul3(euler_xyz_deg_to_mat(31, 53, -21), parity_factor(parity));
        const float trim[3] = {(float)angle, -83, 62}, tm[3] = {-0.08f, 0.12f, -0.03f}, p0[3] = {0.2f, 1.2f, -0.4f};
        const float point[3] = {0.71f, -0.04f, 0.12f}, anyDir[3] = {0, 1, 0};
        float a[3], b2[3], d[3];
        check(palm_point_to_xr(rc, grip, p0, trim, tm, point, a), "a palm point carries");
        check(palm_ray_to_xr(rc, grip, p0, trim, tm, point, anyDir, b2, d), "and so does the ray it shares the arithmetic with");
        check(a[0] == b2[0] && a[1] == b2[1] && a[2] == b2[2], "bit for bit the same point");
        // a rigid transport: the distance between two palm points is kept
        const float q[3] = {0.0f, 0.0f, 0.0f}; float o[3];
        palm_point_to_xr(rc, grip, p0, trim, tm, q, o);
        const float e[3] = {a[0]-o[0], a[1]-o[1], a[2]-o[2]};
        check(near_f(len3(e), len3(point), 1e-5f), "the transport keeps the blade's length");
    }
    {
        const float head[3] = {0.1f, 1.6f, 0.0f};
        float p[3] = {0.3f, 1.2f, -0.5f};
        scale_about_head(head, 100.0f / 108.0f, p);
        check(near_f(p[0], 0.1f + 0.2f * 100.0f / 108.0f, 1e-6f) && near_f(p[2], -0.5f * 100.0f / 108.0f, 1e-6f), "hand travel against the camera's scale, about the head");
        float same[3] = {0.3f, 1.2f, -0.5f};
        scale_about_head(head, 1.0f, same);
        check(same[0] == 0.3f && same[2] == -0.5f, "a scale of one moves nothing");
    }

    // ---- prerequisite 3: a point takes the arithmetic a ray's origin takes ------------
    {
        using namespace dvr::fireaim;
        int n = 0;
        for (int yawD = -170; yawD < 180; yawD += 53) for (int pitchD = -60; pitchD <= 60; pitchD += 30)
        for (int rollD = -20; rollD <= 20; rollD += 20) {
            // the head: yaw about Y, pitch about X, ROLL about Z, as a quaternion
            const float y = yawD * 0.0174532925f * 0.5f, p = pitchD * 0.0174532925f * 0.5f, r = rollD * 0.0174532925f * 0.5f;
            const float qy[4] = {0, std::sin(y), 0, std::cos(y)}, qx[4] = {std::sin(p), 0, 0, std::cos(p)}, qz[4] = {0, 0, std::sin(r), std::cos(r)};
            auto mulq = [](const float* a, const float* b, float* o) {
                o[0] = a[3]*b[0] + a[0]*b[3] + a[1]*b[2] - a[2]*b[1];
                o[1] = a[3]*b[1] - a[0]*b[2] + a[1]*b[3] + a[2]*b[0];
                o[2] = a[3]*b[2] + a[0]*b[1] - a[1]*b[0] + a[2]*b[3];
                o[3] = a[3]*b[3] - a[0]*b[0] - a[1]*b[1] - a[2]*b[2];
            };
            float t[4], q[4]; mulq(qy, qx, t); mulq(t, qz, q);
            dvr::aim::FireFrame f; f.ray.ok = true; f.ray.gen = 7; f.ray.sampleMs = 1000; f.headValid = true; f.distanceM = 8;
            for (int i = 0; i < 4; ++i) f.headQuat[i] = q[i];
            const float head[3] = {0.1f, 1.6f, -0.2f}, point[3] = {0.35f, 1.25f, -0.75f};
            for (int i = 0; i < 3; ++i) { f.headPos[i] = head[i]; f.ray.originXr[i] = point[i]; }
            f.ray.dirXr[0] = 0.2f; f.ray.dirXr[1] = -0.3f; f.ray.dirXr[2] = -0.9f;
            const float viewYaw = 1.3f, viewPitch = pitchD * 0.0174532925f * 0.5f;
            const float camera[3] = {-2670.0f, -26290.0f, 386.0f};
            Solution sol; Frames fr;
            if (!solve(f, 1010, viewYaw, viewPitch, camera, 108, camera, sol)) continue;   // straight up or down refuses
            ++n;
            check(frames(q, viewYaw, viewPitch, fr), "the frames build wherever the ray solver accepts");
            float w[3]; point_to_world(fr, head, point, camera, 108, w);
            check(w[0] == sol.origin[0] && w[1] == sol.origin[1] && w[2] == sol.origin[2], "a point is the ray's origin, bit for bit");
            float back[3]; point_to_xr(fr, head, w, camera, 108, back);
            check(near_f(back[0], point[0], 2e-4f) && near_f(back[1], point[1], 2e-4f) && near_f(back[2], point[2], 2e-4f), "and comes back to where it started");
            // distances survive: 1 m of XR is `scale` uu of world, in any direction, rolled or not
            const float other[3] = {point[0] + 0.3f, point[1] - 0.4f, point[2] + 0.5f};
            float w2[3]; point_to_world(fr, head, other, camera, 108, w2);
            const float e[3] = {w2[0]-w[0], w2[1]-w[1], w2[2]-w[2]}, x[3] = {0.3f, -0.4f, 0.5f};
            check(near_f(len3(e), 108.0f * len3(x), 0.02f), "a length in metres is that many times the scale in uu, head rolled or not");
        }
        check(n >= 60, "the bridge was exercised at many head poses, rolled ones included");
        Frames fr; const float up[4] = {0.7071068f, 0, 0, 0.7071068f};
        check(!frames(up, 0, 0, fr), "a head looking straight up has no horizon and refuses");
    }

    // ---- the acceptance check can fail ----------------------------------------------
    {
        Bounds b;
        const float pommel[3] = {-0.09f, 0.0f, 0.0f}, tip[3] = {0.80f, 0.0f, 0.0f}, off[3] = {0.80f, 0.10f, 0.0f};
        const Segment s = segment(pommel, tip, b), t = segment(pommel, off, b);
        const Apart a = apart(s, t);
        check(near_f(a.tipM, 0.10f, 1e-5f) && a.deg > 5.0f, "a marker 10 cm off the blade reads as 10 cm and 6 degrees off");
    }

    std::printf("%s: %d check(s), %d failed\n", failed ? "blade maths FAILED" : "blade maths ok", checks, failed);
    return failed ? 1 : 0;
}
