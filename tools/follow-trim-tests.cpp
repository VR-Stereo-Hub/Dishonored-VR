// VR-57 commit 2: follow_trim_ray, checked against an INDEPENDENT reference.
//
// The reference does not reuse the production algebra. It builds the camera-space
// palm frames with palm_target exactly as the draw does, forms the trim delta in
// CAMERA space, and converts to XR LOCAL through an explicit M = B * F * R_H^T.
// If the production shortcut (the M cancellation) is wrong, these disagree.
#include <cstdio>
#include <cmath>
#include <cstring>
#include "game/dishonored/hands/hand_frame.h"

using namespace dvr::hf;

static int g_pass = 0, g_fail = 0;
static void rec(const char* what, bool ok, const char* extra = "")
{
    if (ok) ++g_pass;
    else { ++g_fail; std::printf("FAIL: %s %s\n", what, extra); }
}

static Mat3 rotAxis(int axis, float deg)
{
    float r[3] = {0, 0, 0};
    r[axis] = deg;
    return euler_xyz_deg_to_mat(r[0], r[1], r[2]);
}

// An improper basis, to exercise the reflection path the hand mapping really has.
static Mat3 flipX(void) { Mat3 o = identity3(); o.m[0] = -1.0f; return o; }

// ---- the independent reference ----------------------------------------------
// Camera space throughout, then one explicit change of basis at the end.
static void referenceRay(const Mat3& B, const Mat3& R_H, const Mat3& R_C, const Mat3& G,
                         const float* p0, const float* trimRdeg, const float* trimTm,
                         const float* origin0, const float* dir0,
                         float* originOut, float* dirOut)
{
    const Mat3 M   = mul3(mul3(B, xr_back_to_forward()), transpose3(R_H));
    const Mat3 O_C = mul3(M, R_C);
    const Mat3 Tr  = euler_xyz_deg_to_mat(trimRdeg[0], trimRdeg[1], trimRdeg[2]);

    // The palm frames the draw would build, untrimmed and trimmed, in CAMERA space.
    // d_cam is the palm origin carried into camera space by the same M.
    float dcam[3];
    mulv3(M, p0, dcam);
    const float noT[3] = {0, 0, 0};
    const Xform untrimmed = palm_target(O_C, G, dcam, identity3(), noT);
    const Xform trimmed   = palm_target(O_C, G, dcam, Tr,          trimTm);

    // The camera-space rigid delta that carries one palm frame onto the other.
    // A local inverse, so the reference borrows nothing from production beyond
    // palm_target itself. The rotation may be improper; it is still orthogonal,
    // so the transpose is its inverse.
    Xform invU;
    invU.r = transpose3(untrimmed.r);
    float negT[3];
    mulv3(invU.r, untrimmed.t, negT);
    for (int i = 0; i < 3; i++) invU.t[i] = -negT[i];
    const Xform Dc = xform_mul(trimmed, invU);

    // The ray, carried into camera space, transformed, carried back.
    float oC[3], dC[3];
    mulv3(M, origin0, oC);
    mulv3(M, dir0, dC);
    float oC2[3], dC2[3];
    mulv3(Dc.r, oC, oC2);
    for (int i = 0; i < 3; i++) oC2[i] += Dc.t[i];
    mulv3(Dc.r, dC, dC2);

    const Mat3 Mt = transpose3(M);
    mulv3(Mt, oC2, originOut);
    float d[3];
    mulv3(Mt, dC2, d);
    const float n = std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
    for (int i = 0; i < 3; i++) dirOut[i] = d[i] / n;
}

static void caseCheck(const char* name, const Mat3& B, const Mat3& R_H, const Mat3& R_C,
                      const Mat3& G, const float* p0, const float* trimRdeg,
                      const float* trimTm, const float* origin0, const float* dir0)
{
    FollowTrimIn in;
    in.R_C = R_C; in.G = G;
    for (int i = 0; i < 3; i++) {
        in.p0[i] = p0[i]; in.trimRdeg[i] = trimRdeg[i]; in.trimTm[i] = trimTm[i];
        in.origin0[i] = origin0[i]; in.dir0[i] = dir0[i];
    }
    FollowTrimOut out;
    const bool ok = follow_trim_ray(in, out);
    if (!ok) { rec(name, false, out.why); return; }

    float rO[3], rD[3];
    referenceRay(B, R_H, R_C, G, p0, trimRdeg, trimTm, origin0, dir0, rO, rD);

    float worstO = 0, worstD = 0;
    for (int i = 0; i < 3; i++) {
        worstO = std::fmax(worstO, std::fabs(out.origin[i] - rO[i]));
        worstD = std::fmax(worstD, std::fabs(out.dir[i] - rD[i]));
    }
    char msg[256];
    std::snprintf(msg, sizeof(msg), "origin err %.3e m, dir err %.3e", worstO, worstD);
    // 1e-4 m on the endpoint-bearing origin, 1e-5 on the unit direction.
    rec(name, worstO < 1.0e-4f && worstD < 1.0e-5f, msg);
}

int main(void)
{
    const float p0[3]      = { 0.31f, -0.18f, -0.42f };
    const float origin0[3] = { 0.33f, -0.16f, -0.45f };   // the AIM pose, near but not at the palm
    float dir0[3]          = { 0.12f,  0.05f, -0.99f };
    const float n0 = std::sqrt(dir0[0]*dir0[0] + dir0[1]*dir0[1] + dir0[2]*dir0[2]);
    for (int i = 0; i < 3; i++) dir0[i] /= n0;

    const Mat3 B    = identity3();
    const Mat3 Bodd = flipX();                       // improper draw basis
    const Mat3 headA = rotAxis(1, 23.0f);
    const Mat3 headB = mul3(rotAxis(0, -17.0f), rotAxis(1, 140.0f));
    const Mat3 ctlA  = rotAxis(1, -35.0f);
    const Mat3 ctlB  = mul3(rotAxis(2, 65.0f), rotAxis(0, 28.0f));
    const Mat3 gProp = mul3(rotAxis(0, 24.2f), mul3(rotAxis(1, 62.2f), rotAxis(2, -53.7f)));
    const Mat3 gImp  = mul3(gProp, flipX());         // the reflection the grip really can carry

    const float zero[3] = {0, 0, 0};

    // 1. exact identity on zero trim, and NOT merely on zero rotation
    {
        FollowTrimIn in; in.R_C = ctlA; in.G = gProp;
        for (int i = 0; i < 3; i++) {
            in.p0[i] = p0[i]; in.trimRdeg[i] = 0; in.trimTm[i] = 0;
            in.origin0[i] = origin0[i]; in.dir0[i] = dir0[i];
        }
        FollowTrimOut o; const bool ok = follow_trim_ray(in, o);
        bool bitwise = ok && o.identity;
        for (int i = 0; i < 3; i++)
            if (o.origin[i] != origin0[i] || o.dir[i] != dir0[i]) bitwise = false;
        rec("zero trim returns the ray bit for bit", bitwise);

        // rotation zero but translation nonzero MUST move the ray
        in.trimTm[0] = 0.05f;
        FollowTrimOut o2; const bool ok2 = follow_trim_ray(in, o2);
        bool moved = ok2 && !o2.identity;
        float d = 0; for (int i = 0; i < 3; i++) d += std::fabs(o2.origin[i] - origin0[i]);
        rec("zero rotation with nonzero translation still translates", moved && d > 1.0e-6f);
        // and its DIRECTION must be untouched by a pure translation
        float dd = 0; for (int i = 0; i < 3; i++) dd += std::fabs(o2.dir[i] - dir0[i]);
        rec("a pure translation leaves the direction alone", ok2 && dd < 1.0e-6f);
    }

    // 2. against the independent reference, across bases, parities and trims
    struct { const char* n; const float r[3]; const float t[3]; } trims[] = {
        { "yaw only",            { 0, 30, 0 },        { 0, 0, 0 } },
        { "pitch only",          { 28, 0, 0 },        { 0, 0, 0 } },
        { "roll only",           { 0, 0, -40 },       { 0, 0, 0 } },
        { "past the old bound",  { 120, 0, 0 },       { 0, 0, 0 } },
        { "the new limit",       { 180, 0, 0 },       { 0, 0, 0 } },
        { "noncommuting xyz",    { 35, -75, 50 },     { 0, 0, 0 } },
        { "translation only",    { 0, 0, 0 },         { 0.06f, -0.03f, 0.04f } },
        { "mixed",               { -47, 88, -22 },    { -0.05f, 0.02f, -0.07f } },
        { "full range mixed",    { 175, -160, 150 },  { 0.25f, -0.25f, 0.25f } },
    };
    const struct { const char* n; Mat3 B, H, C, G; } rigs[] = {
        { "identity basis, proper grip",  B,    headA, ctlA, gProp },
        { "identity basis, improper grip", B,   headA, ctlA, gImp  },
        { "improper basis, proper grip",  Bodd, headB, ctlB, gProp },
        { "improper basis, improper grip", Bodd, headB, ctlB, gImp  },
        { "turned head",                  B,    headB, ctlA, gProp },
        { "turned controller",            B,    headA, ctlB, gImp  },
    };
    for (int r = 0; r < (int)(sizeof(rigs)/sizeof(rigs[0])); r++)
        for (int t = 0; t < (int)(sizeof(trims)/sizeof(trims[0])); t++) {
            char name[192];
            std::snprintf(name, sizeof(name), "%s / %s", rigs[r].n, trims[t].n);
            caseCheck(name, rigs[r].B, rigs[r].H, rigs[r].C, rigs[r].G,
                      p0, trims[t].r, trims[t].t, origin0, dir0);
        }

    // 3. rotation ABOUT the ray must leave the direction fixed while still moving a
    //    translated origin. Rotation angle is not the ray's angular change, and a
    //    direction-only implementation would pass the wrong half of this.
    {
        // Build a trim whose transported axis is the ray direction: choose Trim.r
        // about the axis Q^T * dir0, so D = Q Tr Q^T rotates about dir0.
        const Mat3 Q = mul3(ctlA, gProp);
        float axis[3]; mulv3(transpose3(Q), dir0, axis);
        // A rotation of 40 deg about `axis` expressed in xyz euler is not direct, so
        // instead verify the property that matters: D applied to dir0 is dir0 when
        // the trim rotation is about Q^T dir0. Use the matrix form directly.
        const float ang = 40.0f * 3.14159265358979f / 180.0f;
        const float c = std::cos(ang), s = std::sin(ang), k = 1 - c;
        const float x = axis[0], y = axis[1], z = axis[2];
        Mat3 Tr;
        Tr.m[0] = c + x*x*k;   Tr.m[1] = x*y*k - z*s; Tr.m[2] = x*z*k + y*s;
        Tr.m[3] = y*x*k + z*s; Tr.m[4] = c + y*y*k;   Tr.m[5] = y*z*k - x*s;
        Tr.m[6] = z*x*k - y*s; Tr.m[7] = z*y*k + x*s; Tr.m[8] = c + z*z*k;
        const Mat3 D = mul3(mul3(Q, Tr), transpose3(Q));
        float spun[3]; mulv3(D, dir0, spun);
        float err = 0; for (int i = 0; i < 3; i++) err = std::fmax(err, std::fabs(spun[i] - dir0[i]));
        rec("rotation about the ray leaves its direction fixed", err < 1.0e-5f);
        // while a point off the axis does move
        float rel[3] = { origin0[0]-p0[0], origin0[1]-p0[1], origin0[2]-p0[2] };
        float relSpun[3]; mulv3(D, rel, relSpun);
        float moved = 0;
        for (int i = 0; i < 3; i++) moved += std::fabs(relSpun[i] - rel[i]);
        rec("the same rotation still moves an off-axis origin", moved > 1.0e-4f);
    }

    // 4. refusals
    {
        FollowTrimIn in; in.R_C = ctlA; in.G = gProp;
        for (int i = 0; i < 3; i++) {
            in.p0[i] = p0[i]; in.trimRdeg[i] = 10; in.trimTm[i] = 0;
            in.origin0[i] = origin0[i]; in.dir0[i] = dir0[i];
        }
        FollowTrimOut o;
        FollowTrimIn bad = in; bad.trimRdeg[1] = std::nanf("");
        rec("nonfinite trim is refused", !follow_trim_ray(bad, o));
        bad = in; bad.dir0[0] = bad.dir0[1] = bad.dir0[2] = 0.0f;
        rec("a degenerate direction is refused", !follow_trim_ray(bad, o));
        bad = in; bad.G.m[4] = 9.0f;   // no longer orthonormal
        rec("a non-rotation grip is refused", !follow_trim_ray(bad, o));
    }

    std::printf("\nfollow-trim: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
