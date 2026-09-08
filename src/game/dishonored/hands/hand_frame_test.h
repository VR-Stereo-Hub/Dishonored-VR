// game/dishonored/hands/hand_frame_test.h - the deterministic checks for the
// VR-33 rotation/grip frame math.
//
// ONE suite, TWO callers: `src/tools/frame_test` runs it on the desk and fails
// the build machine's console with a non-zero exit; the proxy runs it once at
// init and writes the result to the log. So the tester's log always carries
// proof that the maths in THAT build passed, and nothing here needs the tester
// to run anything.
//
// Every case is written to be able to FAIL. `head_turn` in particular asserts
// that the REJECTED formula produces the wrong answer, so a future edit that
// quietly reintroduces it cannot pass by accident.

#ifndef DVR_HAND_FRAME_TEST_H
#define DVR_HAND_FRAME_TEST_H

#include "hand_frame.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

namespace dvr {
namespace hf {
namespace test {

typedef void (*ReportFn)(void* ctx, const char* name, bool pass, const char* detail);

struct Rec {
    ReportFn fn; void* ctx; int run, failed;
};

static inline void rec(Rec* r, const char* name, bool pass, const char* fmt, ...)
{
    char buf[256];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    r->run++; if (!pass) r->failed++;
    if (r->fn) r->fn(r->ctx, name, pass, buf);
}

static inline Mat3 rot_axis_deg(int axis, float deg)
{
    const float k = 0.01745329252f, c = cosf(deg*k), s = sinf(deg*k);
    Mat3 o = identity3();
    if (axis == 0)      { o.m[4]=c; o.m[5]=-s; o.m[7]=s;  o.m[8]=c; }
    else if (axis == 1) { o.m[0]=c; o.m[2]=s;  o.m[6]=-s; o.m[8]=c; }
    else                { o.m[0]=c; o.m[1]=-s; o.m[3]=s;  o.m[4]=c; }
    return o;
}

// The formula this lane REJECTED, kept only so a test can prove it is wrong.
static inline Mat3 rejected_similarity(const Mat3& B, const Mat3& R_H, const Mat3& R_C)
{
    return mul3(mul3(B, mul3(transpose3(R_H), R_C)), transpose3(B));
}

static inline int run_all(ReportFn fn, void* ctx)
{
    Rec r; r.fn = fn; r.ctx = ctx; r.run = 0; r.failed = 0;
    const float TOL = 1e-4f;

    // ---- 1. the head-turn counterexample -----------------------------------
    // Stationary controller, turning head, in a common frame consistent with
    // the working position conversion (B = R_H * F). The controller's
    // orientation in that frame must not move.
    {
        bool ok = true, rejectedFails = false;
        float worst = 0.0f, worstRejected = 0.0f;
        for (int i = 0; i <= 4; i++) {
            const float yaw = (float)i * 45.0f;
            const Mat3 R_H = rot_axis_deg(1, yaw);
            const Mat3 B   = mul3(R_H, xr_back_to_forward());
            const Mat3 R_C = identity3();
            const float a = rotation_angle_deg(controller_orient_camera(B, R_H, R_C));
            if (a > worst) worst = a;
            if (a > 0.01f) ok = false;
            const float ar = rotation_angle_deg(rejected_similarity(B, R_H, R_C));
            if (ar > worstRejected) worstRejected = ar;
            if (yaw > 1.0f && ar > 1.0f) rejectedFails = true;
        }
        rec(&r, "head_turn", ok,
            "stationary controller, head yaw 0..180: correct formula moves the hand "
            "%.4f deg (want 0); the rejected similarity moves it %.1f deg", worst,
            worstRejected);
        rec(&r, "head_turn_can_fail", rejectedFails,
            "the rejected similarity DOES produce head-driven rotation (%.1f deg), so "
            "this suite would catch its return", worstRejected);
    }

    // ---- 2. each axis, both signs, checked as basis vectors ----------------
    // A wrong orientation can be a perfectly good rotation, so compare the
    // MAPPED AXES against what the physical mapping predicts, per axis.
    {
        const Mat3 R_H = rot_axis_deg(1, 31.0f);
        const Mat3 B   = mul3(rot_axis_deg(0, 12.0f), mul3(R_H, xr_back_to_forward()));
        float worst = 0.0f;
        for (int axis = 0; axis < 3; axis++)
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                const Mat3 R_C = rot_axis_deg(axis, 90.0f * (float)sgn);
                const Mat3 got = controller_orient_camera(B, R_H, R_C);
                const Mat3 want = mul3(mul3(mul3(B, xr_back_to_forward()),
                                            transpose3(R_H)), R_C);
                for (int c = 0; c < 3; c++) {
                    float g[3], w[3], e = 0.0f;
                    for (int k = 0; k < 3; k++) { g[k] = got.m[k*3+c]; w[k] = want.m[k*3+c]; }
                    for (int k = 0; k < 3; k++) e += fabsf(g[k] - w[k]);
                    if (e > worst) worst = e;
                }
            }
        rec(&r, "axis_pm90", worst < TOL,
            "each controller axis at +/-90 deg, compared column by column: worst "
            "column error %.6f", worst);
    }

    // ---- 3. non-commuting combinations -------------------------------------
    {
        const Mat3 R_H = rot_axis_deg(2, 17.0f);
        const Mat3 B   = mul3(R_H, xr_back_to_forward());
        const Mat3 a   = mul3(rot_axis_deg(0, 70.0f), rot_axis_deg(1, 40.0f));
        const Mat3 b   = mul3(rot_axis_deg(1, 40.0f), rot_axis_deg(0, 70.0f));
        const float sep = rotation_diff_deg(controller_orient_camera(B, R_H, a),
                                            controller_orient_camera(B, R_H, b));
        // The two orders are genuinely different rotations, and the conversion
        // must preserve that difference exactly rather than average it away.
        const float want = rotation_diff_deg(a, b);
        rec(&r, "noncommuting", fabsf(sep - want) < 1e-2f && want > 1.0f,
            "XY vs YX ordering differs by %.3f deg at the controller and %.3f deg "
            "after conversion", want, sep);
    }

    // ---- 4. the published split is an identity ------------------------------
    {
        const Mat3 R_H = rot_axis_deg(1, 23.0f);
        const Mat3 R_C = mul3(rot_axis_deg(0, 55.0f), rot_axis_deg(2, -33.0f));
        const Mat3 B   = mul3(rot_axis_deg(2, 9.0f), mul3(R_H, xr_back_to_forward()));
        const float d = rotation_diff_deg(
            controller_orient_camera(B, R_H, R_C),
            head_orient_to_camera(B, controller_orient_in_head(R_H, R_C)));
        rec(&r, "split_matches_whole", d < 1e-3f,
            "publishing F*R_H^T*R_C from the pose tick and multiplying by B at the "
            "draw equals the whole conversion to %.6f deg", d);
    }

    // ---- 5. handedness -----------------------------------------------------
    {
        const Mat3 left  = mul3(rot_axis_deg(1, 40.0f), xr_back_to_forward());
        const Mat3 right = rot_axis_deg(1, 40.0f);
        rec(&r, "basis_proper", basis_is_proper(left, 1e-3f) &&
                                !basis_is_proper(right, 1e-3f),
            "B*F is a proper rotation for a left-handed B (det %.3f) and is REFUSED "
            "for a right-handed one (det %.3f)",
            det3(mul3(left, xr_back_to_forward())),
            det3(mul3(right, xr_back_to_forward())));
    }

    // ---- 6. the grip capture round trip ------------------------------------
    // Solving G on a frozen sample and immediately using it must produce NO
    // rotational correction: the calibration press snaps to the native pose.
    {
        const Mat3 R_H  = rot_axis_deg(1, 12.0f);
        const Mat3 R_C  = mul3(rot_axis_deg(0, 40.0f), rot_axis_deg(1, -20.0f));
        const Mat3 B    = mul3(rot_axis_deg(2, 5.0f), mul3(R_H, xr_back_to_forward()));
        const Mat3 R_L  = rot_axis_deg(0, 90.0f);
        const Mat3 Rsrc = mul3(rot_axis_deg(2, 33.0f), rot_axis_deg(1, 15.0f));
        const float tL[3] = { 3.0f, -4.0f, 5.0f };
        const float q[3]  = { 24.5f, -142.5f, 58.0f };
        float dcam[3];
        {   // put the target exactly where the source already is, so only the
            // rotational half is under test
            float qc[3]; mulv3(R_L, q, qc);
            for (int i = 0; i < 3; i++) dcam[i] = qc[i] + tL[i];
        }
        const Mat3 O_C = controller_orient_camera(B, R_H, R_C);
        const Mat3 G   = grip_solve(O_C, R_L, Rsrc);
        const Xform D  = delta_local(R_L, tL, O_C, G, dcam, Rsrc, q, true);
        const float ang = rotation_angle_deg(D.r);
        float tmag = 0.0f; for (int i = 0; i < 3; i++) tmag += fabsf(D.t[i]);
        rec(&r, "grip_roundtrip", ang < 1e-2f && tmag < 1e-2f,
            "capture then apply on frozen input: correction is %.5f deg and "
            "%.5f uu, i.e. identity", ang, tmag);

        const Mat3 G2 = grip_solve(O_C, R_L, Rsrc);
        rec(&r, "grip_idempotent", rotation_diff_deg(G, G2) < 1e-4f,
            "repeating the capture on the same input reproduces G to %.6f deg",
            rotation_diff_deg(G, G2));

        // The SAME physical situation described in a differently oriented view
        // must solve the same G. Rotate the head and the draw basis together.
        const Mat3 Q    = rot_axis_deg(1, 65.0f);
        const Mat3 R_H2 = mul3(Q, R_H);
        const Mat3 R_C2 = mul3(Q, R_C);
        const Mat3 B2   = B;                       // same view, head+hand rotated
        const Mat3 O2   = controller_orient_camera(B2, R_H2, R_C2);
        const Mat3 G3   = grip_solve(O2, R_L, Rsrc);
        rec(&r, "grip_basis_invariance", rotation_diff_deg(G, G3) < 1e-3f,
            "rotating head and controller together by 65 deg leaves G unchanged to "
            "%.6f deg", rotation_diff_deg(G, G3));
    }

    // ---- 7. the pivot ------------------------------------------------------
    // D must carry the palm anchor exactly onto the target point. If the pivot
    // is wrong the hand swings about the component origin, which is the fault
    // that was reverted once already.
    {
        const Mat3 R_H  = rot_axis_deg(1, 8.0f);
        const Mat3 R_C  = rot_axis_deg(0, 65.0f);
        const Mat3 B    = mul3(R_H, xr_back_to_forward());
        const Mat3 R_L  = rot_axis_deg(2, 25.0f);
        const Mat3 Rsrc = rot_axis_deg(1, 44.0f);
        const float tL[3] = { 10.0f, 20.0f, -30.0f };
        const float q[3]  = { 24.5f, -142.5f, 58.0f };
        const float dcam[3] = { -46.2f, 11.3f, -24.6f };
        const Mat3 O_C = controller_orient_camera(B, R_H, R_C);
        const Mat3 G   = identity3();
        for (int rot = 0; rot < 2; rot++) {
            const Xform D = delta_local(R_L, tL, O_C, G, dcam, Rsrc, q, rot != 0);
            float moved[3]; apply_point(D, q, moved);
            float want[3], d[3];
            for (int i = 0; i < 3; i++) d[i] = dcam[i] - tL[i];
            mulv3(transpose3(R_L), d, want);
            float e = 0.0f; for (int i = 0; i < 3; i++) e += fabsf(moved[i] - want[i]);
            rec(&r, rot ? "pivot_rotating" : "pivot_translating", e < 1e-2f,
                "the palm anchor lands on the target to %.5f uu", e);
        }
    }

    // ---- 8. rotation OFF is bit-for-bit the shipped behaviour ---------------
    {
        const Mat3 R_L  = rot_axis_deg(2, 25.0f);
        const Mat3 Rsrc = rot_axis_deg(1, 44.0f);
        const float tL[3] = { 10.0f, 20.0f, -30.0f };
        const float q[3]  = { 24.5f, -142.5f, 58.0f };
        const float dcam[3] = { -46.2f, 11.3f, -24.6f };
        const Xform D = delta_local(R_L, tL, identity3(), identity3(), dcam,
                                    Rsrc, q, false);
        // the shipped expression, written out
        float d[3], tgt[3];
        for (int i = 0; i < 3; i++) d[i] = dcam[i] - tL[i];
        mulv3(transpose3(R_L), d, tgt);
        float e = 0.0f;
        for (int i = 0; i < 3; i++) e += fabsf(D.t[i] - (tgt[i] - q[i]));
        e += rotation_angle_deg(D.r);
        rec(&r, "rot_off_matches_legacy", e < 1e-4f,
            "with rotation off the delta is exactly target_local - q, error %.7f", e);
    }

    // ---- 9. the compose ----------------------------------------------------
    {
        float M[12] = { 0.540228f, -0.149257f, -0.827587f, -10.384630f,
                       -0.634541f,  0.573143f, -0.517580f, -46.606033f,
                        0.551848f,  0.805142f,  0.215024f, 103.612244f };
        float out[12];
        Xform I; I.r = identity3(); I.t[0]=I.t[1]=I.t[2]=0.0f;
        compose_3x4(I, M, out);
        float e = 0.0f; for (int i = 0; i < 12; i++) e += fabsf(out[i] - M[i]);
        rec(&r, "compose_identity", e < 1e-5f,
            "an identity D leaves the palette untouched, error %.7f", e);

        Xform T; T.r = identity3(); T.t[0]=1.5f; T.t[1]=-2.5f; T.t[2]=3.5f;
        compose_3x4(T, M, out);
        e = 0.0f;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                e += fabsf(out[i*4+j] - (M[i*4+j] + (j == 3 ? T.t[i] : 0.0f)));
        rec(&r, "compose_translation", e < 1e-5f,
            "a pure translation D reproduces the shipped add-to-.w, error %.7f", e);

        // THE PROPERTY THAT KEEPS THE ANIMATION: a common rigid D commutes with
        // the skinning blend, so skinning with the composed palette equals
        // applying D to the vertex skinned with the original one.
        Xform D; D.r = mul3(rot_axis_deg(0, 37.0f), rot_axis_deg(2, -12.0f));
        D.t[0] = 4.0f; D.t[1] = -9.0f; D.t[2] = 2.0f;
        compose_3x4(D, M, out);
        const float v[3] = { 53.25f, -97.29f, 11.12f };
        float a[3], b[3];
        for (int i = 0; i < 3; i++)
            a[i] = out[i*4+0]*v[0] + out[i*4+1]*v[1] + out[i*4+2]*v[2] + out[i*4+3];
        float s[3];
        for (int i = 0; i < 3; i++)
            s[i] = M[i*4+0]*v[0] + M[i*4+1]*v[1] + M[i*4+2]*v[2] + M[i*4+3];
        apply_point(D, s, b);
        e = 0.0f; for (int i = 0; i < 3; i++) e += fabsf(a[i] - b[i]);
        rec(&r, "compose_commutes", e < 1e-2f,
            "skinning with the composed palette equals D applied after skinning, "
            "error %.5f uu - this is why finger animation survives", e);
    }

    // ---- 10. the scaled-rotation decomposition -----------------------------
    {
        const float s = 0.999512f;
        Mat3 a = mul3(rot_axis_deg(0, 33.0f), rot_axis_deg(1, -17.0f));
        for (int i = 0; i < 9; i++) a.m[i] *= s;
        ScaledRot sr;
        const bool ok = decompose_scaled_rotation(a, 1e-3f, 1e-3f, &sr);
        rec(&r, "scaled_rotation", ok && fabsf(sr.scale - s) < 1e-5f &&
                                   is_rotation(sr.r, 1e-4f),
            "a rotation times %.6f decomposes to scale %.6f, orthonormality "
            "residual %.7f", s, ok ? sr.scale : 0.0f, ok ? sr.ortho : 9.9f);

        Mat3 aniso = a;                            // stretch one column only
        for (int rw = 0; rw < 3; rw++) aniso.m[rw*3+1] *= 1.05f;
        rec(&r, "scaled_rotation_rejects_aniso",
            !decompose_scaled_rotation(aniso, 1e-3f, 1e-3f, &sr),
            "a 5%% anisotropic scale is REFUSED rather than normalised into a "
            "plausible frame");

        Mat3 shear = a; shear.m[1] += 0.20f;       // break orthogonality
        rec(&r, "scaled_rotation_rejects_shear",
            !decompose_scaled_rotation(shear, 1e-3f, 1e-3f, &sr),
            "a sheared matrix is REFUSED (unit columns alone are not a rotation)");
    }

    // ---- 11. Euler serialisation ------------------------------------------
    {
        float worst = 0.0f;
        const float ang[5] = { 0.0f, 30.0f, -75.0f, 150.0f, -179.0f };
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 5; j++)
                for (int k = 0; k < 5; k++) {
                    const Mat3 m = euler_xyz_deg_to_mat(ang[i], ang[j], ang[k]);
                    float x, y, z;
                    mat_to_euler_xyz_deg(m, &x, &y, &z);
                    const float d = rotation_diff_deg(m, euler_xyz_deg_to_mat(x, y, z));
                    if (d > worst) worst = d;
                }
        rec(&r, "euler_roundtrip", worst < 0.05f,
            "125 orientations through degrees and back (extrinsic X,Y,Z; R = "
            "Rz*Ry*Rx): worst reconstruction error %.4f deg", worst);
    }

    // ---- 12. the reported error metric -------------------------------------
    {
        const float a = rotation_angle_deg(rot_axis_deg(1, 90.0f));
        const float b = rotation_diff_deg(rot_axis_deg(0, 20.0f),
                                          rot_axis_deg(0, 50.0f));
        rec(&r, "rotation_metric", fabsf(a - 90.0f) < 1e-2f && fabsf(b - 30.0f) < 1e-2f,
            "angle of a 90 deg rotation reads %.3f, and 20 deg vs 50 deg reads "
            "%.3f apart", a, b);
    }

    return r.failed;
}

} // namespace test
} // namespace hf
} // namespace dvr

#endif // DVR_HAND_FRAME_TEST_H
