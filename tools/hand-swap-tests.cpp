// The hand swap ([Hands] SwapHands, docs/dishonored/HAND_SWAP.md), checked offline against the
// PRODUCTION headers. hand_frame_test.h's swap_mirror_* cases (frame_test, and every proxy start)
// pin the placement itself. This file checks the claims the OTHER consumers rest on, each against
// a reference built from the RENDER side (mirrored world, then the reflection), each with a
// negative control that must miss:
//   1. the aim ray: palm_ray_to_xr fed R_C * X (trim_snapshot) lies on the barrel the draw shows
//   2. the view-aligned trim step: a step through base = R_C * X * G moves the drawn palm by it
//   3. the weapons: a member drawn with its own conjugated reflection stays rigid on the hand,
//      by the rebased route and by the native bridge
//   4. full-arm IK: the plane through the shoulder pair's centre maps one shoulder onto the other
//   5. the effects: a mirrored frame with Y turned back is a proper rotation keeping X and Z
#include <cstdio>
#include <cmath>
#include <cstdarg>
#include "game/dishonored/hands/hand_frame.h"
#include "game/dishonored/hands/bolt_axis.h"
#include "game/dishonored/hands/weapon_frame.h"
#include "game/dishonored/hands/arm_ik.h"
#include "core/util/xr_math.h"

using namespace dvr::hf;

static int g_pass = 0, g_fail = 0;
static void rec(const char* what, bool ok, const char* fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    if (ok) ++g_pass; else ++g_fail;
    std::printf("%-36s %s  %s\n", what, ok ? "PASS" : "**FAIL**", buf);
}

static Mat3 rotAxis(int axis, float deg)
{
    float r[3] = {0, 0, 0};
    r[axis] = deg;
    return euler_xyz_deg_to_mat(r[0], r[1], r[2]);
}
static float dist3(const float* a, const float* b)
{
    return sqrtf((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]) + (a[2]-b[2])*(a[2]-b[2]));
}

// One coherent scene. XR -> camera-relative world is x_cam = M (x - head) + e, M = B * F * R_H^T,
// exactly the position path (k = 1 here, so XR metres and camera units agree); e is the eye offset,
// so the head's centre sits at e and the mirror plane passes through it with the head's right axis.
struct Scene {
    Mat3 B, R_H, R_C, G, M;
    float head[3], p[3], e[3], trimDeg[3], trimM[3];
    Plane plane;
};
static Scene make_scene()
{
    Scene s;
    s.B   = rotAxis(2, 7.0f);                                   // right-handed, as measured
    s.R_H = mul3(rotAxis(1, 31.0f), rotAxis(0, -12.0f));
    s.R_C = mul3(rotAxis(0, 28.0f), mul3(rotAxis(1, -35.0f), rotAxis(2, 14.0f)));
    s.M   = mul3(mul3(s.B, xr_back_to_forward()), transpose3(s.R_H));
    const Mat3 O_C = mul3(s.M, s.R_C);
    s.G = mul3(parity_factor(parity_of(O_C)), mul3(rotAxis(2, 17.0f), rotAxis(0, -40.0f)));
    const float head[3] = { 0.12f, 1.62f, -0.30f }, p[3] = { -0.21f, 1.18f, -0.62f };
    const float e[3] = { 0.031f * s.B.m[0], 0.031f * s.B.m[3], 0.031f * s.B.m[6] };   // half an IPD along right
    const float tD[3] = { 6.0f, -9.0f, 4.0f }, tM[3] = { 0.012f, -0.004f, 0.021f };
    for (int i = 0; i < 3; i++) {
        s.head[i] = head[i]; s.p[i] = p[i]; s.e[i] = e[i]; s.trimDeg[i] = tD[i]; s.trimM[i] = tM[i];
    }
    const float right[3] = { s.B.m[0], s.B.m[3], s.B.m[6] };
    plane_from(right, s.e, &s.plane);
    return s;
}
static void to_cam(const Scene& s, const float* x, float* out)
{
    const float d[3] = { x[0]-s.head[0], x[1]-s.head[1], x[2]-s.head[2] };
    mulv3(s.M, d, out);
    for (int i = 0; i < 3; i++) out[i] += s.e[i];
}
static void to_xr(const Scene& s, const float* x, float* out)
{
    const float d[3] = { x[0]-s.e[0], x[1]-s.e[1], x[2]-s.e[2] };
    mulv3(transpose3(s.M), d, out);
    for (int i = 0; i < 3; i++) out[i] += s.head[i];
}
// THE RENDER REFERENCE: the palm frame the draw SHOWS, camera-relative - the controller's target
// taken into the mirrored world, the hand placed there, the result reflected back.
static Xform drawn_palm(const Scene& s, const float* trimM)
{
    float d[3]; to_cam(s, s.p, d);
    Mat3 O = mul3(s.M, s.R_C);
    mirror_target(s.plane, d, &O);
    const Mat3 Tr = euler_xyz_deg_to_mat(s.trimDeg[0], s.trimDeg[1], s.trimDeg[2]);
    return xform_mul(reflection_xform(s.plane), palm_target(O, s.G, d, Tr, trimM));
}

int main()
{
    std::printf("hand swap - the consumers of the mirrored hand\n");
    std::printf("---------------------------------------------------------------\n");
    const Scene s = make_scene();

    // ---- 1. the aim ray -------------------------------------------------------------------
    {
        const float oPalm[3] = { 0.03f, -0.11f, 0.24f }, dPalm[3] = { 0.10f, 0.15f, 0.98f };
        const Xform palm = drawn_palm(s, s.trimM);
        float oc[3], dc[3]; apply_point(palm, oPalm, oc); mulv3(palm.r, dPalm, dc);
        float refO[3], refD[3]; to_xr(s, oc, refO); mulv3(transpose3(s.M), dc, refD);
        const float n = sqrtf(refD[0]*refD[0] + refD[1]*refD[1] + refD[2]*refD[2]);
        for (int i = 0; i < 3; i++) refD[i] /= n;
        const Mat3 RCX = mul3(s.R_C, controller_mirror());     // what trim_snapshot publishes
        float o[3], d[3], o0[3], d0[3];
        const bool ok = palm_ray_to_xr(RCX, s.G, s.p, s.trimDeg, s.trimM, oPalm, dPalm, o, d);
        palm_ray_to_xr(s.R_C, s.G, s.p, s.trimDeg, s.trimM, oPalm, dPalm, o0, d0);
        const float eo = dist3(o, refO), ed = dist3(d, refD), bo = dist3(o0, refO), bd = dist3(d0, refD);
        rec("aim_ray_on_the_drawn_barrel", ok && eo < 1e-4f && ed < 1e-4f,
            "R_C*X puts the ray %.6f m / %.6f off the drawn barrel", eo, ed);
        rec("aim_ray_needs_x", bo > 1e-3f || bd > 1e-2f,
            "the bare R_C misses it by %.4f m / %.4f, which the case above rejects", bo, bd);
    }

    // ---- 2. the view-aligned trim step ------------------------------------------------------
    {
        const float step[3] = { 0.01f, 0.0f, -0.02f };          // XR metres, as the view step builds it
        auto drawnXr = [&](const float* t, float* out) { const Xform x = drawn_palm(s, t); to_xr(s, x.t, out); };
        auto stepped = [&](const Mat3& base, float* out) {
            float dp[3]; mulv3(transpose3(base), step, dp);
            float t[3]; for (int i = 0; i < 3; i++) t[i] = s.trimM[i] + dp[i];
            drawnXr(t, out);
        };
        float p0[3], p1[3], pb[3];
        drawnXr(s.trimM, p0);
        stepped(mul3(mul3(s.R_C, controller_mirror()), s.G), p1);   // MpTrimViewStep's base while swapped
        stepped(mul3(s.R_C, s.G), pb);
        float moved[3], bad[3];
        for (int i = 0; i < 3; i++) { moved[i] = p1[i] - p0[i]; bad[i] = pb[i] - p0[i]; }
        rec("trim_step_moves_the_drawn_palm", dist3(moved, step) < 1e-5f,
            "a %.3f m step through R_C*X*G moves the drawn palm by it to %.7f m",
            sqrtf(step[0]*step[0] + step[1]*step[1] + step[2]*step[2]), dist3(moved, step));
        rec("trim_step_needs_x", dist3(bad, step) > 1e-3f,
            "through the bare R_C*G the palm goes %.4f m off the step", dist3(bad, step));
    }

    // ---- 3. the weapons ---------------------------------------------------------------------
    {
        Xform L;  L.r  = rotAxis(0, 90.0f);  const float tL[3] = { 7.0f, -3.0f, 11.0f };
        for (int i = 0; i < 3; i++) L.t[i] = tL[i];
        const Mat3 Rsrc = rotAxis(1, 51.0f); const float q[3] = { 24.5f, -142.5f, 58.0f };
        float d[3]; to_cam(s, s.p, d);
        Mat3 O = mul3(s.M, s.R_C);
        mirror_target(s.plane, d, &O);
        const Xform tgt = palm_target(O, s.G, d, euler_xyz_deg_to_mat(s.trimDeg[0], s.trimDeg[1], s.trimDeg[2]), s.trimM);
        const Xform Dl = delta_from_target(L.r, L.t, tgt, Rsrc, q);
        Xform iL; dvr::wf::inverse(L, &iL);
        const Xform Dc = xform_mul(xform_mul(L, Dl), iL);         // WaPublishCommon's D
        const Xform W  = reflection_xform(s.plane);               // ... and its W
        // A member with its own draw transform, and a point on it.
        Xform Lm; Lm.r = mul3(rotAxis(2, -63.0f), rotAxis(0, 22.0f));
        Lm.t[0] = -4.0f; Lm.t[1] = 18.0f; Lm.t[2] = 3.5f;
        const float g[3] = { 1.5f, -6.0f, 9.25f };
        auto member = [&](const Xform& draw, const Xform& corr, const Xform& mirror, bool useMirror, float* out) {
            Xform id; dvr::wf::inverse(draw, &id);
            const Xform dm  = xform_mul(xform_mul(id, corr), draw);        // the member's delta
            const Xform dmW = xform_mul(xform_mul(id, mirror), draw);      // WaMirrorLocal
            const Xform drawn = useMirror ? xform_mul(dmW, dm) : dm;       // MpBuild(drawDelta)
            apply_point(xform_mul(draw, drawn), g, out);
        };
        // Rigid on the hand: relative to the DRAWN palm the member sits where it sat relative to the
        // native palm before any correction.
        Xform srcF; srcF.r = Rsrc; for (int i = 0; i < 3; i++) srcF.t[i] = q[i];
        const Xform nativePalm = xform_mul(L, srcF), shownPalm = xform_mul(W, tgt);
        Xform iN, iS; dvr::wf::inverse(nativePalm, &iN); dvr::wf::inverse(shownPalm, &iS);
        float lm[3], want[3]; apply_point(Lm, g, lm); apply_point(iN, lm, want);
        float got[3], rel[3]; member(Lm, Dc, W, true, got); apply_point(iS, got, rel);
        // The native route: draws in a non-rebased world, reached through a bridge K (rebased = K * native).
        Xform K; K.r = rotAxis(1, 12.0f); K.t[0] = -350.0f; K.t[1] = 120.0f; K.t[2] = 64.0f;
        Xform iK; dvr::wf::inverse(K, &iK);
        const Xform Ln = xform_mul(iK, Lm);
        const Xform nativeD = xform_mul(xform_mul(iK, Dc), K), nativeW = xform_mul(xform_mul(iK, W), K);
        float gotN[3], gotNr[3], relN[3]; member(Ln, nativeD, nativeW, true, gotN);
        apply_point(K, gotN, gotNr); apply_point(iS, gotNr, relN);
        float bad[3], relB[3]; member(Lm, Dc, W, false, bad); apply_point(iS, bad, relB);
        rec("weapon_rides_the_mirrored_hand", dist3(rel, want) < 1e-2f && dist3(relN, want) < 1e-2f,
            "a member sits %.5f (rebased) / %.5f (native bridge) from its place on the drawn palm",
            dist3(rel, want), dist3(relN, want));
        rec("weapon_needs_its_reflection", dist3(relB, want) > 1.0f,
            "drawn with its delta alone it is %.2f units off the hand", dist3(relB, want));
    }

    // ---- 4. full-arm IK: the plane through the shoulder pair ---------------------------------
    {
        using namespace dvr::ik;
        Body b;
        b.origin = { 3.0f, -2.0f, 40.0f };
        b.right = { 0.8f, 0.0f, 0.6f }; b.up = { 0.0f, 1.0f, 0.0f }; b.forward = cross(b.right, b.up);
        Settings st; st.rightCm = 4.0f; st.widthCm = 38.1f; st.forwardCm = -16.0f; st.upCm = -25.0f;
        const float upc = 1.07f;                                  // units per cm
        const Vec R = shoulder(b, st, 1, upc), Lsh = shoulder(b, st, 0, upc);
        float n[3]; put(b.right, n);
        float c[3]; put(b.origin + b.right * (st.rightCm * upc), c);   // arm_ik_draw.inc's pairShift
        float c0[3]; put(b.origin, c0);
        Plane pair, headOnly; plane_from(n, c, &pair); plane_from(n, c0, &headOnly);
        float r[3]; put(R, r);
        float m[3] = { r[0], r[1], r[2] }, m0[3] = { r[0], r[1], r[2] };
        mirror_target(pair, m, nullptr); mirror_target(headOnly, m0, nullptr);
        float l[3]; put(Lsh, l);
        rec("ik_mirror_maps_shoulder_to_shoulder", dist3(m, l) < 1e-3f,
            "the right shoulder's mirror image lands %.5f units from the left shoulder", dist3(m, l));
        rec("ik_mirror_needs_the_pair_centre", fabsf(dist3(m0, l) - 2.0f * st.rightCm * upc) < 1e-2f,
            "a plane through the head alone misses by %.3f units (2 x ArmShoulderRightCm)", dist3(m0, l));
    }

    // ---- 5. the effects: a proper frame from a mirrored one ------------------------------------
    {
        const Mat3 F = mul3(rotAxis(2, 33.0f), rotAxis(0, -18.0f));     // an effect's X, Y, Z (columns)
        const Mat3 S = reflection3(s.plane.n);
        Mat3 T = mul3(S, F);
        const float detMirrored = det3(T);
        for (int r = 0; r < 3; r++) T.m[r*3+1] = -T.m[r*3+1];       // fx_follow.cpp: TY turned back
        float ex = 0.0f, ez = 0.0f;
        const Mat3 SF = mul3(S, F);
        for (int r = 0; r < 3; r++) { ex += fabsf(T.m[r*3+0] - SF.m[r*3+0]); ez += fabsf(T.m[r*3+2] - SF.m[r*3+2]); }
        rec("fx_frame_made_proper", detMirrored < 0.0f && is_rotation(T, 1e-4f) && ex < 1e-6f && ez < 1e-6f,
            "mirrored det %+.3f -> with Y turned back det %+.3f, X and Z kept to %.1e / %.1e",
            detMirrored, det3(T), ex, ez);
    }

    // 6. (2026-10-21) PointGrab's second dot: the weapon hand's ray carried to the other controller by
    //    dvr::xrmath::mirror_dir_between (aim_ray.cpp). Held symmetrically about the body's centre plane
    //    (the right grip the mirror image of the left: quaternion (x, -y, -z, w)), the carried ray must be
    //    the world mirror of the weapon ray (x negated), at any orientation and any ray direction in the hand.
    //    Negative controls: the local direction placed on the other grip WITHOUT the reflection, and the first
    //    build's ray (the other controller's forward turned by the -48 deg other-items pitch).
    {
        const float poses[4][3] = { {0, 0, 0}, {25, -30, 10}, {-40, 60, -20}, {10, 120, 45} };   // pitch, yaw, roll deg
        const float dirsLocal[3][3] = { {0, 0, -1}, {0.2f, -0.5f, -0.84f}, {-0.3f, 0.1f, -0.95f} };
        float worst = 0.0f, worstNoMirror = 0.0f;
        for (const auto& pz : poses) for (const auto& dl : dirsLocal) {
            float qL[4], qR[4], d[3], want[3], got[3], raw[3];
            dvr::xrmath::xr_local_trim_quat(pz[0] * 0.0174533f, pz[1] * 0.0174533f, pz[2] * 0.0174533f, qL);
            qR[0] = qL[0]; qR[1] = -qL[1]; qR[2] = -qL[2]; qR[3] = qL[3];
            dvr::xrmath::quat_rotate(qL[0], qL[1], qL[2], qL[3], dl, d);
            want[0] = -d[0]; want[1] = d[1]; want[2] = d[2];
            const bool ok = dvr::xrmath::mirror_dir_between(qL, d, qR, got);
            float l[3]; dvr::xrmath::quat_rotate(-qL[0], -qL[1], -qL[2], qL[3], d, l);
            dvr::xrmath::quat_rotate(qR[0], qR[1], qR[2], qR[3], l, raw);       // no reflection
            const float nd = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
            for (int i = 0; i < 3; ++i) { want[i] /= nd; raw[i] /= nd; }
            const float e = ok ? dist3(got, want) : 9.0f, eRaw = dist3(raw, want);
            if (e > worst) worst = e;
            if (eRaw > worstNoMirror) worstNoMirror = eRaw;
        }
        rec("pointgrab_dot_mirrors_weapon_ray", worst < 1e-5f && worstNoMirror > 0.1f,
            "worst %.1e over 12 poses x rays (no reflection misses by %.3f)", worst, worstNoMirror);
        // The headset report: the first build pointed the sword hand along its own forward turned -48 deg.
        float qL[4], qR[4], fwd[3] = {0, 0, -1}, d[3], got[3], old[3], tilt[4], qOld[4];
        dvr::xrmath::xr_local_trim_quat(0.0f, 20.0f * 0.0174533f, 0.0f, qL);
        qR[0] = qL[0]; qR[1] = -qL[1]; qR[2] = -qL[2]; qR[3] = qL[3];
        dvr::xrmath::quat_rotate(qL[0], qL[1], qL[2], qL[3], fwd, d);
        dvr::xrmath::mirror_dir_between(qL, d, qR, got);
        dvr::xrmath::xr_local_trim_quat(-48.0f * 0.0174533f, 0.0f, 0.0f, tilt);
        dvr::xrmath::quat_mul(qR, tilt, qOld);
        dvr::xrmath::quat_rotate(qOld[0], qOld[1], qOld[2], qOld[3], fwd, old);
        const float dotGot = -got[0] * d[0] + got[1] * d[1] + got[2] * d[2];   // vs the weapon ray's mirror
        const float dotOld = -old[0] * d[0] + old[1] * d[1] + old[2] * d[2];
        const float angOld = acosf(dotOld > 1 ? 1 : dotOld) * 57.29578f;
        rec("pointgrab_old_dot_was_48_down", dotGot > 0.99999f && fabsf(angOld - 48.0f) < 0.1f && old[1] < -0.7f,
            "carried ray on the mirror (dot %.6f); the first build's ray sat %.1f deg off it, pointing %.0f%% down",
            dotGot, angOld, -old[1] * 100.0f);
    }

    // 7. (2026-10-29) The arms' lens undo (lens_undo_rows): the shader draws L2W * P with L2W = R * diag(n). After the
    //    undo, L2W * P' must equal R * P exactly, for the plain palette and after the hand swap's reflection; the
    //    negative control applies diag(1/n) BEFORE the reflection, which misses because they do not commute.
    {
        const float n[3] = { 1.026f, 1.022f, 1.0f };                 // the dialogue's column norms (headset log)
        const Mat3 R = mul3(rotAxis(1, 37.0f), rotAxis(0, -21.0f));
        float L2W[12], Rx[12];
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) { L2W[r*4+c] = R.m[r*3+c] * n[c]; Rx[r*4+c] = R.m[r*3+c]; }
        L2W[3] = 5.0f; L2W[7] = -2.0f; L2W[11] = 1.5f; Rx[3] = 5.0f; Rx[7] = -2.0f; Rx[11] = 1.5f;
        float P[24];                                                  // two bones, each a rigid 3x4
        for (int b = 0; b < 2; b++) {
            const Mat3 M = mul3(rotAxis(2, 15.0f + 30.0f * b), rotAxis(0, 10.0f * b));
            for (int r = 0; r < 3; r++) { for (int c = 0; c < 3; c++) P[b*12 + r*4 + c] = M.m[r*3+c]; P[b*12 + r*4 + 3] = 3.0f * (r + 1) - b; }
        }
        auto err = [&](const float* Pa, const float* L, const float* Pb, const float* Lb) {
            float worst = 0.0f;
            for (int b = 0; b < 2; b++) {
                float A[12], B[12]; mul_3x4(L, Pa + b*12, A); mul_3x4(Lb, Pb + b*12, B);
                for (int i = 0; i < 12; i++) worst = fmaxf(worst, fabsf(A[i] - B[i]));
            }
            return worst;
        };
        float U[24]; for (int i = 0; i < 24; i++) U[i] = P[i];
        lens_undo_rows(n, U, 6);
        const float ePlain = err(U, L2W, P, Rx), eNone = err(P, L2W, P, Rx);
        Plane pl; pl.n[0] = 0.8f; pl.n[1] = 0.0f; pl.n[2] = 0.6f; pl.c[0] = 1.25f; pl.c[1] = -0.5f; pl.c[2] = 2.0f;
        float M1[24], M2[24];
        for (int i = 0; i < 24; i++) { M1[i] = P[i]; M2[i] = P[i]; }
        reflect_palette_left(pl, M1, 6); lens_undo_rows(n, M1, 6);   // production order: reflect, then undo
        float Ref[24]; for (int i = 0; i < 24; i++) Ref[i] = P[i];
        reflect_palette_left(pl, Ref, 6);                             // the reference: R * S * P
        lens_undo_rows(n, M2, 6); reflect_palette_left(pl, M2, 6);   // the wrong order
        const float eMirror = err(M1, L2W, Ref, Rx), eWrong = err(M2, L2W, Ref, Rx);
        rec("lens_undo_places_through_scale", ePlain < 1e-5f && eNone > 0.01f && eMirror < 1e-5f && eWrong > 0.001f,
            "plain %.1e (no undo %.3f), after the mirror %.1e (undo before the mirror %.4f)", ePlain, eNone, eMirror, eWrong);
        const float one[3] = { 1.0f, 1.0f, 1.0f }; float I[24]; for (int i = 0; i < 24; i++) I[i] = P[i];
        lens_undo_rows(one, I, 6); bool same = true; for (int i = 0; i < 24; i++) same = same && I[i] == P[i];
        rec("lens_undo_identity_without_lens", same, "a unit scale leaves the palette bit for bit");
    }

    std::printf("---------------------------------------------------------------\n");
    std::printf("hand-swap: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
