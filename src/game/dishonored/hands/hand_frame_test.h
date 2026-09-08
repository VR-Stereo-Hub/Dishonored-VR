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
    // THE NOISE FLOOR FOR A NEAR-IDENTITY ANGLE. rotation_angle_deg goes
    // through acos, which is ill-conditioned near zero: for a small angle
    // theta, cos(theta) ~ 1 - theta^2/2, so a trace error of eps reads as
    // an angle of about sqrt(2*eps). At float32 epsilon that is ~7e-4 rad,
    // i.e. about 0.04 degrees, and a chain of six matrix products reaches
    // it. Anything below this is float noise, not a rotation; asserting
    // tighter than it would be asserting against the arithmetic rather
    // than against the maths. 0.05 deg is three orders below visible.
    const float ANG_EPS = 0.05f;

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
    // BOTH parities are accepted; only a non-orthogonal basis is refused. The
    // game measures right-handed, so B*F is a reflection - see hand_frame.h.
    {
        const Mat3 left  = mul3(rot_axis_deg(1, 40.0f), xr_back_to_forward());
        const Mat3 right = rot_axis_deg(1, 40.0f);
        Mat3 broken = right; broken.m[1] += 0.5f;
        rec(&r, "basis_orthonormal",
            basis_is_orthonormal(left, 1e-3f) && basis_is_orthonormal(right, 1e-3f) &&
            !basis_is_orthonormal(broken, 1e-3f) &&
            basis_parity(left) == +1 && basis_parity(right) == -1,
            "both parities accepted (left-handed B gives parity %+d, right-handed "
            "gives %+d) and a non-orthogonal basis is REFUSED",
            basis_parity(left), basis_parity(right));
    }

    // ---- 5b. THE REFLECTION CARRIES THROUGH AND CANCELS ---------------------
    // The measured case: a right-handed B, so the pose mapping B*F*R_H^T is a
    // mirror. The grip round trip must still be exact, the transform composed
    // onto the palette must still be a PROPER rotation, and a controller
    // rotation must still produce a hand rotation of the same angle.
    {
        const Mat3 R_H  = rot_axis_deg(1, 22.0f);
        const Mat3 B    = rot_axis_deg(2, 8.0f);       // right-handed: B*F improper
        const Mat3 R_C0 = mul3(rot_axis_deg(0, 15.0f), rot_axis_deg(2, 40.0f));
        const Mat3 R_L  = rot_axis_deg(0, 90.0f);
        const Mat3 Rsrc = rot_axis_deg(1, 27.0f);
        const float tL[3] = { 1.0f, 2.0f, 3.0f };
        const float q[3]  = { 24.5f, -142.5f, 58.0f };
        float dcam[3];
        { float qc[3]; mulv3(R_L, q, qc);
          for (int i = 0; i < 3; i++) dcam[i] = qc[i] + tL[i]; }

        const int parity = basis_parity(B);
        const Mat3 O0 = controller_orient_camera(B, R_H, R_C0);
        const Mat3 G  = grip_solve(O0, R_L, Rsrc);
        const Xform D0 = delta_local(R_L, tL, O0, G, dcam, Rsrc, q, true);
        const bool capOk = rotation_angle_deg(D0.r) < ANG_EPS;

        // now turn the controller by a known angle and check what the hand does
        const float turn = 37.0f;
        const Mat3 R_C1 = mul3(R_C0, rot_axis_deg(1, turn));
        const Mat3 O1 = controller_orient_camera(B, R_H, R_C1);
        const Xform D1 = delta_local(R_L, tL, O1, G, dcam, Rsrc, q, true);
        const float ang  = rotation_angle_deg(D1.r);
        const bool proper = is_rotation(D1.r, 1e-3f);   // det > 0 as well

        rec(&r, "improper_basis_roundtrip",
            parity == -1 && capOk && proper && fabsf(ang - turn) < 0.05f,
            "with a MIRRORED pose mapping (parity %+d): capture is still exact "
            "(%.5f deg), the composed transform is still a proper rotation "
            "(det %+.4f), and a %.0f deg controller turn gives a %.2f deg hand "
            "turn. The reflection cancels between O_C and G",
            parity, rotation_angle_deg(D0.r), det3(D1.r), turn, ang);
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
        rec(&r, "grip_roundtrip", ang < ANG_EPS && tmag < 1e-2f,
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

    // ---- 12b. THE CALIBRATION SURVIVES A RESTART, AT BOTH PARITIES ---------
    // The bug: G is improper here, and three Euler angles rebuilt as Rz*Ry*Rx
    // can never be improper, so the saved calibration came back mirrored.
    // Compared as MATRICES, not as an angle - rotation_diff_deg is meaningless
    // between a proper and an improper matrix and would have hidden this.
    {
        float worstEl = 0.0f; bool parityKept = true;
        for (int sgn = -1; sgn <= 1; sgn += 2) {
            // an improper G of the kind the game actually produces
            Mat3 G = mul3(rot_axis_deg(0, 41.0f), rot_axis_deg(2, -63.0f));
            if (sgn < 0) G = mul3(parity_factor(-1), G);
            int p = 0; Mat3 proper;
            split_parity(G, &p, &proper);
            if (p != sgn) parityKept = false;
            if (!is_rotation(proper, 1e-4f)) parityKept = false;
            // through the ini's own format and back
            float ex, ey, ez;
            mat_to_euler_xyz_deg(proper, &ex, &ey, &ez);
            const Mat3 back = join_parity(p, euler_xyz_deg_to_mat(ex, ey, ez));
            for (int i = 0; i < 9; i++) {
                const float e = fabsf(back.m[i] - G.m[i]);
                if (e > worstEl) worstEl = e;
            }
        }
        rec(&r, "grip_persists_with_parity", parityKept && worstEl < 2e-3f,
            "capture, split to parity + proper rotation, write as degrees, read "
            "back and rejoin: worst matrix element error %.6f at BOTH parities",
            worstEl);

        // and the naive route this replaced, so the suite can still catch it
        Mat3 G = mul3(parity_factor(-1), rot_axis_deg(0, 41.0f));
        float ex, ey, ez;
        mat_to_euler_xyz_deg(G, &ex, &ey, &ez);
        const Mat3 naive = euler_xyz_deg_to_mat(ex, ey, ez);
        rec(&r, "grip_naive_euler_loses_parity",
            det3(G) < 0.0f && det3(naive) > 0.0f,
            "writing an improper G straight out as Euler angles comes back with "
            "determinant %+.3f instead of %+.3f - the reflection is gone, which "
            "is the restart bug this format replaced", det3(naive), det3(G));

        // an UNCALIBRATED start must not be mirrored either
        const Mat3 O_C = mul3(parity_factor(-1), rot_axis_deg(1, 20.0f));
        rec(&r, "uncalibrated_default_is_not_mirrored",
            det3(mul3(O_C, parity_factor(parity_of(O_C)))) > 0.0f &&
            det3(mul3(O_C, identity3())) < 0.0f,
            "with an improper pose mapping, the parity-matched default G gives a "
            "proper orientation (det %+.3f) while identity gives a MIRRORED one "
            "(det %+.3f) - which is the inside-out startup",
            det3(mul3(O_C, parity_factor(parity_of(O_C)))),
            det3(mul3(O_C, identity3())));
    }

    // ---- 12c. the shared palm target, and the trims -------------------------
    // The weapon path needs the target palm BEFORE any hand has drawn, so it
    // must come from the pose and the calibration alone. It has to agree
    // exactly with what the hand path already computes.
    {
        const Mat3 R_H  = rot_axis_deg(1, 19.0f);
        const Mat3 R_C  = mul3(rot_axis_deg(0, 33.0f), rot_axis_deg(2, 12.0f));
        const Mat3 B    = rot_axis_deg(2, 6.0f);         // right-handed, as measured
        const Mat3 R_L  = rot_axis_deg(0, 90.0f);
        const Mat3 Rsrc = rot_axis_deg(1, 51.0f);
        const float tL[3] = { 7.0f, -3.0f, 11.0f };
        const float q[3]  = { 24.5f, -142.5f, 58.0f };
        const float dcam[3] = { -46.2f, 11.3f, -24.6f };
        const Mat3 O_C = controller_orient_camera(B, R_H, R_C);
        const Mat3 G   = grip_solve(O_C, R_L, Rsrc);
        const float noT[3] = { 0.0f, 0.0f, 0.0f };

        const Xform tgt = palm_target(O_C, G, dcam, identity3(), noT);
        const Xform viaTarget = delta_from_target(R_L, tL, tgt, Rsrc, q);
        const Xform direct    = delta_local(R_L, tL, O_C, G, dcam, Rsrc, q, true);
        float e = rotation_diff_deg(direct.r, viaTarget.r);
        for (int i = 0; i < 3; i++) e += fabsf(direct.t[i] - viaTarget.t[i]);
        rec(&r, "palm_target_matches_delta", e < 0.05f,
            "the shared target helper reproduces the hand path's own correction "
            "to %.5f - so a weapon can be placed before either hand draws", e);

        // A trim translation is in the PALM frame, so it rotates with the palm.
        const float trimT[3] = { 0.5f, 0.0f, 0.0f };
        const Xform t1 = palm_target(O_C, G, dcam, identity3(), trimT);
        float moved = 0.0f;
        for (int i = 0; i < 3; i++) moved += (t1.t[i]-tgt.t[i])*(t1.t[i]-tgt.t[i]);
        moved = sqrtf(moved);
        const Mat3 O2 = controller_orient_camera(B, R_H, mul3(R_C, rot_axis_deg(1, 90.0f)));
        const Xform t2 = palm_target(O2, G, dcam, identity3(), trimT);
        float moved2 = 0.0f;
        for (int i = 0; i < 3; i++) moved2 += (t2.t[i]-dcam[i])*(t2.t[i]-dcam[i]);
        moved2 = sqrtf(moved2);
        bool dirChanged = false;
        for (int i = 0; i < 3; i++)
            if (fabsf((t1.t[i]-dcam[i]) - (t2.t[i]-dcam[i])) > 0.05f) dirChanged = true;
        rec(&r, "hand_trim_is_in_the_palm_frame",
            fabsf(moved - 0.5f) < 1e-3f && fabsf(moved2 - 0.5f) < 1e-3f && dirChanged,
            "a 0.5 unit trim moves the target 0.5 units (%.4f), still 0.5 after a "
            "90 deg wrist turn (%.4f), and in a DIFFERENT direction - so it rides "
            "the palm rather than the world", moved, moved2);

        // A hand trim must move a held weapon by the SAME common transform.
        Xform H; H.r = rot_axis_deg(2, 25.0f); H.t[0]=1.0f; H.t[1]=-2.0f; H.t[2]=0.5f;
        const Xform w0 = xform_mul(tgt, H);
        const Xform w1 = xform_mul(t1,  H);
        float dh = 0.0f, dw = 0.0f;
        for (int i = 0; i < 3; i++) {
            dh += (t1.t[i]-tgt.t[i])*(t1.t[i]-tgt.t[i]);
            dw += (w1.t[i]-w0.t[i])*(w1.t[i]-w0.t[i]);
        }
        rec(&r, "hand_trim_carries_the_weapon",
            fabsf(sqrtf(dh) - sqrtf(dw)) < 1e-3f &&
            rotation_diff_deg(w0.r, w1.r) < 1e-2f,
            "a hand trim moves the palm and the held weapon by the same %.4f "
            "units and adds no relative rotation - so weapon alignment inherits "
            "hand alignment instead of needing recalibration", sqrtf(dh));
    }

    // ---- 11b. the model scale ----------------------------------------------
    {
        Mat3 R_L = rot_axis_deg(1, 40.0f);
        float tL[3] = { 3.0f, -1.0f, 2.0f };
        Mat3 Rsrc = rot_axis_deg(0, 15.0f);
        float q[3] = { 0.2f, -0.1f, 0.4f };
        Xform tgt; tgt.r = rot_axis_deg(2, 20.0f);
        tgt.t[0] = 5.0f; tgt.t[1] = 1.0f; tgt.t[2] = -2.0f;

        float palm[3];
        const Xform D = delta_from_target(R_L, tL, tgt, Rsrc, q, palm);

        // The palm this returns must be the target, carried into local space.
        float back[3];
        mulv3(R_L, palm, back);
        float ep = 0.0f;
        for (int i = 0; i < 3; i++) {
            const float w = back[i] + tL[i];
            ep += (w - tgt.t[i]) * (w - tgt.t[i]);
        }

        // THE PALM IS A POINT IN THE OUTPUT SPACE, not an input vertex: D is
        // built so that the SOURCE ANCHOR lands exactly on it. So the fixed
        // point to check is "the anchor still maps to the palm", and distances
        // are measured from the palm itself. Measuring them from D(palm)
        // instead is what this case caught on its first run.
        const Xform H = scale_about(D, palm, 0.5f);
        float movedPalm = 0.0f;
        for (int i = 0; i < 3; i++) {
            const float b = H.r.m[i*3+0]*q[0] + H.r.m[i*3+1]*q[1] +
                            H.r.m[i*3+2]*q[2] + H.t[i];
            movedPalm += (b - palm[i]) * (b - palm[i]);
        }
        float v[3] = { 1.3f, -0.7f, 2.1f }, d0 = 0.0f, d1 = 0.0f;
        for (int i = 0; i < 3; i++) {
            const float a = D.r.m[i*3+0]*v[0] + D.r.m[i*3+1]*v[1] +
                            D.r.m[i*3+2]*v[2] + D.t[i];
            const float b = H.r.m[i*3+0]*v[0] + H.r.m[i*3+1]*v[1] +
                            H.r.m[i*3+2]*v[2] + H.t[i];
            d0 += (a - palm[i]) * (a - palm[i]);
            d1 += (b - palm[i]) * (b - palm[i]);
        }
        d0 = sqrtf(d0); d1 = sqrtf(d1);
        // A uniform scale leaves the rotation's DIRECTION alone: normalising
        // the scaled basis must give the original back.
        Mat3 Hn = H.r;
        for (int c = 0; c < 3; c++) {
            float n = 0.0f;
            for (int rr = 0; rr < 3; rr++) n += Hn.m[rr*3+c] * Hn.m[rr*3+c];
            n = sqrtf(n);
            for (int rr = 0; rr < 3; rr++) Hn.m[rr*3+c] /= n;
        }
        rec(&r, "model_scale_about_the_palm",
            sqrtf(ep) < 1e-3f && sqrtf(movedPalm) < 1e-4f &&
            fabsf(d1 - 0.5f * d0) < 1e-3f &&
            rotation_diff_deg(Hn, D.r) < 1e-2f,
            "the palm comes back exactly (%.5f), the source anchor still lands "
            "on it under a half scale (%.5f off) and a %.4f distance from it "
            "halves to %.4f, and the basis still points the same way (%.4f "
            "deg) - so the grip stays where tracking put it and the tangent "
            "frame is not sheared",
            sqrtf(ep), sqrtf(movedPalm), d0, d1, rotation_diff_deg(Hn, D.r));
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
