// Host tests for game/dishonored/hands/grab_pose.h: the screw blend and the grab's timing.
// Built and run by tools\grab-pose-host.ps1. Never launches the game.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "game/dishonored/hands/grab_pose.h"

static int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); std::exit(1); } } while (0)
static bool near(float a, float b, float tol = 1e-3f) { return std::fabs(a - b) <= tol; }

// A rotation about Z by `deg` around the pivot (px, py, 0), scaled by k, as a 3x4.
static void rot_z_about(float deg, float px, float py, float k, float* M)
{
    const float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a);
    const float R[9] = { c, -s, 0, s, c, 0, 0, 0, 1 };
    for (int r = 0; r < 3; r++) { M[r * 4 + 0] = k * R[r * 3 + 0]; M[r * 4 + 1] = k * R[r * 3 + 1]; M[r * 4 + 2] = k * R[r * 3 + 2]; }
    // x' = R (x - p) + p
    M[3] = px - (c * px - s * py); M[7] = py - (s * px + c * py); M[11] = 0;
}
static void apply(const float* M, const float* p, float* o)
{
    for (int r = 0; r < 3; r++) o[r] = M[r * 4 + 0] * p[0] + M[r * 4 + 1] * p[1] + M[r * 4 + 2] * p[2] + M[r * 4 + 3];
}

int main()
{
    using namespace dvr::grab;
    // min-jerk: ends, middle, monotone, symmetric
    CHECK(min_jerk(0) == 0 && min_jerk(1) == 1 && min_jerk(-1) == 0 && min_jerk(2) == 1);
    CHECK(near(min_jerk(0.5f), 0.5f));
    CHECK(near(min_jerk(0.25f) + min_jerk(0.75f), 1.0f));
    for (int i = 1; i <= 100; i++) CHECK(min_jerk(i / 100.0f) >= min_jerk((i - 1) / 100.0f));
    CHECK(min_jerk(0.02f) < 0.001f);   // starts at rest

    // the blend reproduces both ends exactly
    float A[12], B[12], O[12];
    rot_z_about(10, 3, 1, 1, A); rot_z_about(95, 3, 1, 1, B);
    screw_blend_3x4(A, B, 0, O); for (int i = 0; i < 12; i++) CHECK(near(O[i], A[i]));
    screw_blend_3x4(A, B, 1, O); for (int i = 0; i < 12; i++) CHECK(near(O[i], B[i]));

    // A bone turning about a fixed knuckle sweeps an ARC: halfway through, a fingertip point is
    // still the same distance from the knuckle, and sits at the halfway angle. A straight blend
    // of the translations would bring it closer (the chord).
    rot_z_about(0, 3, 1, 1, A); rot_z_about(90, 3, 1, 1, B);
    const float tip[3] = { 3 + 4, 1, 0 };   // 4 units out from the knuckle
    for (int k = 1; k < 10; k++) {
        const float s = k / 10.0f;
        CHECK(screw_blend_3x4(A, B, s, O));
        float p[3]; apply(O, tip, p);
        const float dx = p[0] - 3, dy = p[1] - 1;
        CHECK(near(std::sqrt(dx * dx + dy * dy), 4.0f, 1e-3f));
        CHECK(near(std::atan2(dy, dx) * 180.0f / 3.14159265f, 90.0f * s, 0.05f));
        CHECK(near(p[2], 0));
    }
    // ...and the straight blend really would have shrunk it (the instrument can fail)
    {
        float lin[12]; for (int i = 0; i < 12; i++) lin[i] = 0.5f * (A[i] + B[i]);
        float p[3]; apply(lin, tip, p);
        CHECK(std::sqrt((p[0] - 3) * (p[0] - 3) + (p[1] - 1) * (p[1] - 1)) < 3.0f);
    }

    // a pure slide blends linearly; a slide along the turning axis (a screw) too
    {
        float S0[12] = { 1, 0, 0, 1, 0, 1, 0, 2, 0, 0, 1, 3 }, S1[12] = { 1, 0, 0, 5, 0, 1, 0, -2, 0, 0, 1, 7 };
        screw_blend_3x4(S0, S1, 0.25f, O);
        CHECK(near(O[3], 2) && near(O[7], 1) && near(O[11], 4) && near(O[0], 1) && near(O[5], 1));
        rot_z_about(0, 0, 0, 1, A); rot_z_about(60, 0, 0, 1, B); B[11] = 6;
        screw_blend_3x4(A, B, 0.5f, O);
        CHECK(near(O[11], 3));
        CHECK(near(std::atan2(O[4], O[0]) * 180.0f / 3.14159265f, 30.0f, 0.05f));
    }
    // uniform scale is carried and blended
    {
        rot_z_about(0, 0, 0, 1.0f, A); rot_z_about(40, 0, 0, 1.5f, B);
        screw_blend_3x4(A, B, 0.5f, O);
        const float k = std::sqrt(O[0] * O[0] + O[4] * O[4] + O[8] * O[8]);
        CHECK(near(k, 1.25f));
    }
    // a reflection or a degenerate matrix is refused, and the nearer end is kept
    {
        float Rf[12] = { -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
        rot_z_about(30, 0, 0, 1, A);
        CHECK(!screw_blend_3x4(A, Rf, 0.3f, O)); for (int i = 0; i < 12; i++) CHECK(O[i] == A[i]);
        CHECK(!screw_blend_3x4(A, Rf, 0.7f, O)); for (int i = 0; i < 12; i++) CHECK(O[i] == Rf[i]);
    }
    // the long way round is never taken (q and -q are the same rotation)
    {
        rot_z_about(170, 0, 0, 1, A); rot_z_about(-170, 0, 0, 1, B);
        screw_blend_3x4(A, B, 0.5f, O);
        CHECK(near(std::fabs(std::atan2(O[3 * 0 + 4], O[0])) * 180.0f / 3.14159265f, 180.0f, 0.1f));
    }

    // timing
    Timing T = { 70, 220, 180, 260, 40 };
    CHECK(phase_at(T, -5, -1).phase == kIdle);
    CHECK(phase_at(T, 0, -1).phase == kShape && phase_at(T, 0, -1).s == 0);
    CHECK(phase_at(T, 35, -1).phase == kShape && near(phase_at(T, 35, -1).s, 0.5f));
    CHECK(phase_at(T, 70, -1).phase == kClose);
    CHECK(phase_at(T, 70 + 259, -1).phase == kClose && phase_at(T, 70 + 260, -1).phase == kHold);
    CHECK(phase_at(T, 5000, -1).phase == kHold);                       // held while the grip is
    CHECK(near(earliest_release(T), 70 + 220 + 40 + 180));
    CHECK(phase_at(T, 600, 600).phase == kRelease && phase_at(T, 600, 600).s == 0);
    CHECK(near(phase_at(T, 730, 600).s, 0.5f));
    CHECK(phase_at(T, 860, 600).phase == kDone);
    // the lag: the knuckle leads, the fingertip follows, both arrive
    CHECK(per_bone_close(T, 110, 0) > per_bone_close(T, 110, 1));
    CHECK(near(per_bone_close(T, 110, 0), 0.5f));
    CHECK(near(per_bone_close(T, 150, 1), 0.5f));
    CHECK(per_bone_close(T, 260, 1) == 1 && per_bone_close(T, 220, 0) == 1);
    CHECK(per_bone_close(T, 10, -3) == per_bone_close(T, 10, 0) && per_bone_close(T, 10, 9) == per_bone_close(T, 10, 1));

    // the parent test: a turn about a joint between the centroids leaves that joint in place
    {
        rot_z_about(80, 5, 0, 1, A);                      // the joint at (5,0,0)
        const float cp[3] = { 2, 0, 0 }, cc[3] = { 8, 0, 0 }, far1[3] = { 9, 0, 0 }, far2[3] = { 14, 0, 0 };
        CHECK(joint_residual(A, cp, cc) < 1e-4f);         // the joint lies between: found
        CHECK(joint_residual(A, far1, far2) > 3.0f);      // a segment off the joint moves a lot
        float I[12] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0 };
        CHECK(joint_residual(I, cp, cc) < 1e-6f);         // a rigid follower: every point stays
        float Sl[12] = { 1, 0, 0, 0, 0, 1, 0, 2, 0, 0, 1, 0 };
        CHECK(near(joint_residual(Sl, cp, cc), 2.0f));    // a slide has no fixed point
        CHECK(parent_score(A, cp, cc, 0.3f) < parent_score(A, far1, far2, 0.3f));
        // between two candidates that both fit, the nearer centroid wins
        const float cpFar[3] = { -4, 0, 0 };
        CHECK(parent_score(A, cp, cc, 0.3f) < parent_score(A, cpFar, cc, 0.3f));
        // the ratio: a joint between the ends reads well under 1
        CHECK(joint_residual_ratio(A, cp, cc, 0.2f) < 0.01f);
        CHECK(joint_residual_ratio(I, cp, cc, 0.2f) == 0.0f);   // a rigid follower: a perfect fit
        // two sibling knuckles: a small turn about an axis PARALLEL to the segment between them,
        // off to one side. Every point drifts alike: the raw residual is small, the ratio is not.
        {
            const float a5 = 5.0f * 3.14159265f / 180.0f, c5 = std::cos(a5), s5 = std::sin(a5);
            // rotation about the X axis line through (0, 0, -3): y' = c y - s (z+3), z' = s y + c (z+3) - 3
            float Sib[12] = { 1, 0, 0, 0,  0, c5, -s5, -3 * s5,  0, s5, c5, 3 * c5 - 3 };
            const float k0[3] = { 0, 0, 0 }, k1[3] = { 2.3f, 0, 0 };
            CHECK(joint_residual(Sib, k0, k1) < 0.3f);             // small raw: what fooled the first score
            CHECK(joint_residual_ratio(Sib, k0, k1, 0.2f) > 0.95f);
            CHECK(parent_score(Sib, k0, k1, 0.02f) > parent_score(A, cpFar, cc, 0.02f));
        }
    }

    {   // the pose latch (2026-10-05): a sample is committed only if the state held for holdMs after it
        PoseLatch<4> L;
        float s[4][12];
        auto fill = [&](float v) { for (int b = 0; b < 4; ++b) for (int k = 0; k < 12; ++k) s[b][k] = v; };
        fill(1); CHECK(!L.tick(0, true, s, 4, 1500));                  // candidate taken at 0
        fill(2); CHECK(!L.tick(1000, true, s, 4, 1500) && !L.haveGood);  // too young: nothing committed
        fill(3); CHECK(L.tick(1500, true, s, 4, 1500));                  // the sample from t=0 commits
        CHECK(L.haveGood && L.good[2][5] == 1.0f && L.commits == 1);
        // the field case: empty, the item draw begins while the read still says empty, then the read flips
        fill(9); CHECK(!L.tick(2000, true, s, 4, 1500));                 // the candidate from 1500 is still young
        CHECK(!L.tick(2600, false, s, 4, 1500));                         // the read flips: the candidate is dropped
        CHECK(L.good[0][0] == 1.0f && L.dropped == 1);                   // the committed pose is still the old one
        fill(7); CHECK(!L.tick(5000, true, s, 4, 1500));                 // empty again: a new candidate...
        CHECK(!L.tick(6000, false, s, 4, 1500));                         // ...interrupted before its hold: never committed
        CHECK(L.good[0][0] == 1.0f);
        // negative control: with no hold the grip the read was late about is committed on the next tick
        PoseLatch<4> naive; fill(1); naive.tick(0, true, s, 4, 0); fill(9); naive.tick(1, true, s, 4, 0);
        CHECK(naive.good[0][0] == 1.0f);
        naive.tick(2, true, s, 4, 0); CHECK(naive.good[0][0] == 9.0f);
    }
    std::printf("grab-pose: %d checks passed\n", checks);
    return 0;
}
