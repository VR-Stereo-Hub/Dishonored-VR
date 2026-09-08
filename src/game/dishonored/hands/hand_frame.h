// game/dishonored/hands/hand_frame.h - the rotation/grip frame math for VR-33.
//
// PURE ARITHMETIC. No engine types, no D3D, no logging, no globals. It is a
// header so that the shipped proxy and the standalone test executable
// (src/tools/frame_test) compile THE SAME FUNCTIONS - a test that re-derives
// the maths in a second language tests the second derivation, not the code.
//
// ---------------------------------------------------------------------------
// THE CONVENTIONS, stated once, because every fault in this lane so far has
// been a frame confusion rather than an arithmetic error.
//
// Mat3 is ROW-MAJOR: m[r * 3 + c]. Vectors are COLUMNS, so `y = M * x`.
//
//   R_H  the head pose's 3x3. Columns are the head's RIGHT, UP and BACK axes
//        expressed in XR tracking space. (XR is right/up/back: forward is -Z.)
//   R_C  the controller grip pose's 3x3, same convention.
//   F    diag(1, 1, -1). Converts XR right/up/BACK to right/up/FORWARD.
//   B    the draw's camera basis, COLUMNS right | up | forward, recovered from
//        the ViewProjection rows. Camera-relative world axes, unreal units.
//   L    the draw's LocalToWorld: component local -> camera-relative world.
//
// THE POSITION PATH, which is already headset-confirmed, is exactly
//
//   p_C = k * B * F * transpose(R_H) * (p_controller - p_head)
//
// (`MpDriveTick` publishes F * transpose(R_H) * w as three scalars and the
// draw multiplies by B.) The ORIENTATION must use the same physical mapping,
// so the controller's orientation in camera-relative world axes is
//
//   O_C = B * F * transpose(R_H) * R_C
//
// and NOT a similarity transform of the head-relative rotation. `B * (R_H^T *
// R_C) * B^T` looks like a basis change and is not one: it changes the basis
// of a rotation OPERATOR, whereas a pose orientation MAPS controller-local axes
// into another frame. With a stationary controller and a turning head that
// error rotates the hand with the head. `frame_test` case `head_turn` is that
// counterexample and it fails on the wrong formula by construction.
//
// HANDEDNESS. B is measured left-handed on this game's verified path and F is
// a reflection, so `B * F` is a proper rotation. That is CHECKED
// (`hf_basis_is_proper`) rather than assumed, and a run where it does not hold
// refuses instead of forcing an improper matrix through a quaternion.
// ---------------------------------------------------------------------------

#ifndef DVR_HAND_FRAME_H
#define DVR_HAND_FRAME_H

#include <math.h>

namespace dvr {
namespace hf {

// ---- small types ------------------------------------------------------------

struct Mat3 { float m[9]; };            // row-major, m[r*3+c]
struct Xform { Mat3 r; float t[3]; };   // y = r*x + t

static inline Mat3 identity3(void)
{
    Mat3 o; o.m[0]=1;o.m[1]=0;o.m[2]=0; o.m[3]=0;o.m[4]=1;o.m[5]=0;
    o.m[6]=0;o.m[7]=0;o.m[8]=1; return o;
}

static inline Mat3 mul3(const Mat3& a, const Mat3& b)
{
    Mat3 o;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            o.m[r*3+c] = a.m[r*3+0]*b.m[0*3+c] + a.m[r*3+1]*b.m[1*3+c] +
                         a.m[r*3+2]*b.m[2*3+c];
    return o;
}

static inline Mat3 transpose3(const Mat3& a)
{
    Mat3 o;
    for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++) o.m[r*3+c] = a.m[c*3+r];
    return o;
}

static inline void mulv3(const Mat3& a, const float* x, float* o)
{
    const float t0 = a.m[0]*x[0] + a.m[1]*x[1] + a.m[2]*x[2];
    const float t1 = a.m[3]*x[0] + a.m[4]*x[1] + a.m[5]*x[2];
    const float t2 = a.m[6]*x[0] + a.m[7]*x[1] + a.m[8]*x[2];
    o[0] = t0; o[1] = t1; o[2] = t2;      // safe when o aliases x
}

static inline float det3(const Mat3& a)
{
    return a.m[0]*(a.m[4]*a.m[8] - a.m[5]*a.m[7])
         - a.m[1]*(a.m[3]*a.m[8] - a.m[5]*a.m[6])
         + a.m[2]*(a.m[3]*a.m[7] - a.m[4]*a.m[6]);
}

static inline bool finite3(const Mat3& a)
{
    for (int i = 0; i < 9; i++) {
        const float v = a.m[i];
        if (!(v == v) || v > 3.4e38f || v < -3.4e38f) return false;
    }
    return true;
}

// Build a basis from three COLUMN vectors.
static inline Mat3 basis_from_cols(const float* c0, const float* c1, const float* c2)
{
    Mat3 o;
    o.m[0]=c0[0]; o.m[1]=c1[0]; o.m[2]=c2[0];
    o.m[3]=c0[1]; o.m[4]=c1[1]; o.m[5]=c2[1];
    o.m[6]=c0[2]; o.m[7]=c1[2]; o.m[8]=c2[2];
    return o;
}

// F = diag(1, 1, -1): XR right/up/BACK -> right/up/FORWARD.
static inline Mat3 xr_back_to_forward(void)
{
    Mat3 o = identity3(); o.m[8] = -1.0f; return o;
}

// ---- validation -------------------------------------------------------------

// How far from orthonormal a matrix is: the largest element of |R^T R - I|.
// A ROTATION CHECK, not a column-length check: unit columns alone do not make
// a rotation, and the transpose is only the inverse if it is one.
static inline float orthonormal_err(const Mat3& a)
{
    float worst = 0.0f;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float g = 0.0f;
            for (int k = 0; k < 3; k++) g += a.m[k*3+i] * a.m[k*3+j];
            const float d = fabsf(g - (i == j ? 1.0f : 0.0f));
            if (d > worst) worst = d;
        }
    return worst;
}

// A PROPER rotation: orthonormal within tol AND det > 0.
static inline bool is_rotation(const Mat3& a, float tol)
{
    return finite3(a) && orthonormal_err(a) <= tol && det3(a) > 0.0f;
}

// The product of the draw basis and F must be a proper rotation, or the pose
// conversion would be composing a reflection into an orientation.
static inline bool basis_is_proper(const Mat3& b, float tol)
{
    return is_rotation(mul3(b, xr_back_to_forward()), tol);
}

// A UNIFORMLY SCALED ROTATION, decomposed. The scale is DERIVED from this
// matrix, per call - never a constant. The three column norms must agree
// (isotropy) or it is not a uniform scale and the frame is refused.
//
// `outR` is the normalised rotation and is the ONLY thing the correction is
// built from; the caller keeps the original matrix for the rendered palette,
// so the palette's own scale is preserved rather than quietly removed.
struct ScaledRot {
    Mat3  r;        // normalised
    float scale;    // the derived uniform scale, > 0
    float aniso;    // max/min column norm - 1, the isotropy residual
    float ortho;    // orthonormality residual of `r`
};

static inline bool decompose_scaled_rotation(const Mat3& a, float tolAniso,
                                             float tolOrtho, ScaledRot* out)
{
    if (!out) return false;
    if (!finite3(a)) return false;
    float n[3];
    for (int c = 0; c < 3; c++) {
        n[c] = sqrtf(a.m[0*3+c]*a.m[0*3+c] + a.m[1*3+c]*a.m[1*3+c] +
                     a.m[2*3+c]*a.m[2*3+c]);
        if (!(n[c] > 1e-6f)) return false;
    }
    float lo = n[0], hi = n[0];
    for (int c = 1; c < 3; c++) { if (n[c] < lo) lo = n[c]; if (n[c] > hi) hi = n[c]; }
    out->aniso = hi / lo - 1.0f;
    out->scale = (n[0] + n[1] + n[2]) / 3.0f;
    if (out->aniso > tolAniso) return false;
    const float inv = 1.0f / out->scale;
    for (int i = 0; i < 9; i++) out->r.m[i] = a.m[i] * inv;
    out->ortho = orthonormal_err(out->r);
    if (out->ortho > tolOrtho) return false;
    if (det3(out->r) <= 0.0f) return false;      // a mirrored slot is not a frame
    return true;
}

// ---- the pose conversion ----------------------------------------------------

// THE controller orientation in camera-relative world axes.
//
//   O_C = B * F * transpose(R_H) * R_C
//
// Same physical mapping as the working position path, which is the whole
// point. See the header comment for why the similarity transform is wrong.
static inline Mat3 controller_orient_camera(const Mat3& B, const Mat3& R_H,
                                            const Mat3& R_C)
{
    return mul3(mul3(mul3(B, xr_back_to_forward()), transpose3(R_H)), R_C);
}

// The head-relative half, which is what the pose tick publishes so the draw
// need not know anything about XR. `hand_local_in_head = F * R_H^T * R_C`.
static inline Mat3 controller_orient_in_head(const Mat3& R_H, const Mat3& R_C)
{
    return mul3(mul3(xr_back_to_forward(), transpose3(R_H)), R_C);
}

// ...and the draw's half. Splitting it this way is an identity, checked by
// frame_test case `split_matches_whole`.
static inline Mat3 head_orient_to_camera(const Mat3& B, const Mat3& inHead)
{
    return mul3(B, inHead);
}

// ---- the grip transform -----------------------------------------------------

// SOLVE G, the fixed controller-to-palm rotation, from one coherent sample.
//
// Both operands must be in the SAME space. The source frame is measured in the
// component's LOCAL space, so it is carried to camera-relative world by this
// draw's own LocalToWorld rotation before being compared with the controller.
// Comparing them directly is a space mix-up and produces a G that is wrong by
// the component's own orientation.
//
//   G = transpose(O_C) * ( R_L * R_src_local )
static inline Mat3 grip_solve(const Mat3& O_C, const Mat3& R_L,
                              const Mat3& R_src_local)
{
    return mul3(transpose3(O_C), mul3(R_L, R_src_local));
}

// ---- the correction ---------------------------------------------------------

// D_local = A_target_local * inverse(A_source_local), the rigid transform that
// carries the palm from where the engine animated it to where the controller
// says it is.
//
//   A_target_cam   = [ O_C * G | d_cam ]
//   A_target_local = inverse(L) * A_target_cam
//   D_local        = A_target_local * inverse(A_source_local)
//
// THE PIVOT IS q. Because A_source carries the palm anchor's position, D
// rotates about the palm and not about the component origin. Rotating without
// the pivot is what made the earlier attempt swing the hand off its wrist.
//
// `rotate` false reproduces the headset-confirmed translation-only behaviour
// EXACTLY - D.r is identity and D.t is `target_local - q`, which is the shipped
// expression. frame_test case `rot_off_matches_legacy` pins that.
static inline Xform delta_local(const Mat3& R_L, const float* t_L,
                                const Mat3& O_C, const Mat3& G,
                                const float* d_cam,
                                const Mat3& R_src_local, const float* q_local,
                                bool rotate)
{
    const Mat3 R_Lt = transpose3(R_L);

    // A_target_local's translation: R_L^T * (d_cam - t_L). The shipped one.
    float d[3];
    for (int i = 0; i < 3; i++) d[i] = d_cam[i] - t_L[i];
    float tgt[3];
    mulv3(R_Lt, d, tgt);

    Xform D;
    if (rotate) {
        const Mat3 tgtR = mul3(R_Lt, mul3(O_C, G));   // A_target_local's rotation
        D.r = mul3(tgtR, transpose3(R_src_local));    // * inverse(A_source)
        float Rq[3];
        mulv3(D.r, q_local, Rq);
        for (int i = 0; i < 3; i++) D.t[i] = tgt[i] - Rq[i];
    } else {
        D.r = identity3();
        for (int i = 0; i < 3; i++) D.t[i] = tgt[i] - q_local[i];
    }
    return D;
}

// D * M for one 3x4 skinning matrix, row-major with the translation in .w.
// `out` may alias `src`.
static inline void compose_3x4(const Xform& D, const float* src, float* out)
{
    float r[3][4];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) {
            float v = D.r.m[i*3+0]*src[0*4+j] + D.r.m[i*3+1]*src[1*4+j] +
                      D.r.m[i*3+2]*src[2*4+j];
            if (j == 3) v += D.t[i];
            r[i][j] = v;
        }
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) out[i*4+j] = r[i][j];
}

// Apply an Xform to a point. Used by the self-test's pivot check and by the
// residual instrument.
static inline void apply_point(const Xform& D, const float* p, float* out)
{
    mulv3(D.r, p, out);
    for (int i = 0; i < 3; i++) out[i] += D.t[i];
}

// ---- Euler serialisation, with the convention stated ------------------------
//
// The ini stores three DEGREES. The convention is EXTRINSIC X, then Y, then Z
// about the fixed camera-relative world axes, i.e. R = Rz * Ry * Rx. It is
// declared here, round-tripped by frame_test, and the internal representation
// stays a matrix - the angles exist only so a solved G can be written into a
// file and read back.

static inline Mat3 euler_xyz_deg_to_mat(float xd, float yd, float zd)
{
    const float k = 0.01745329252f;
    const float cx = cosf(xd*k), sx = sinf(xd*k);
    const float cy = cosf(yd*k), sy = sinf(yd*k);
    const float cz = cosf(zd*k), sz = sinf(zd*k);
    Mat3 Rx = identity3(); Rx.m[4]=cx; Rx.m[5]=-sx; Rx.m[7]=sx; Rx.m[8]=cx;
    Mat3 Ry = identity3(); Ry.m[0]=cy; Ry.m[2]=sy;  Ry.m[6]=-sy; Ry.m[8]=cy;
    Mat3 Rz = identity3(); Rz.m[0]=cz; Rz.m[1]=-sz; Rz.m[3]=sz;  Rz.m[4]=cz;
    return mul3(Rz, mul3(Ry, Rx));
}

static inline void mat_to_euler_xyz_deg(const Mat3& r, float* xd, float* yd, float* zd)
{
    const float k = 57.29577951f;
    // R = Rz*Ry*Rx, written out, gives
    //   row2 = [ -sy , cy*sx , cy*cx ]
    //   col0 = [ cz*cy , sz*cy , -sy ]
    // so sy is -r20, x comes from row 2 and z from column 0. Gimbal lock is
    // cy -> 0, where only (x -/+ z) is determined and z is folded into x.
    float sy = -r.m[6];
    if (sy > 1.0f) sy = 1.0f; else if (sy < -1.0f) sy = -1.0f;
    *yd = asinf(sy) * k;
    const float cy = sqrtf(1.0f - sy * sy);
    if (cy > 1e-4f) {
        *xd = atan2f(r.m[7], r.m[8]) * k;
        *zd = atan2f(r.m[3], r.m[0]) * k;
    } else {
        *zd = 0.0f;
        *xd = (sy > 0.0f ? atan2f(r.m[1], r.m[4])
                         : atan2f(-r.m[1], r.m[4])) * k;
    }
}

// The angle of a rotation, in degrees. The ONLY honest single number for
// "how far apart are these two orientations" - three Euler numbers wrap and
// reorder and can read as an axis failure when nothing is wrong.
static inline float rotation_angle_deg(const Mat3& r)
{
    float tr = r.m[0] + r.m[4] + r.m[8];
    float c = 0.5f * (tr - 1.0f);
    if (c > 1.0f) c = 1.0f; else if (c < -1.0f) c = -1.0f;
    return acosf(c) * 57.29577951f;
}

// The relative rotation error between two orientations, in degrees.
static inline float rotation_diff_deg(const Mat3& a, const Mat3& b)
{
    return rotation_angle_deg(mul3(transpose3(a), b));
}

} // namespace hf
} // namespace dvr

#endif // DVR_HAND_FRAME_H
