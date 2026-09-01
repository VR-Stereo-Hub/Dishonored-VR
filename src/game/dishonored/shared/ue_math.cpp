// game/dishonored/shared/ue_math.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// rotate vector v about unit axis k by angle a (Rodrigues), in place
static inline void RotAbout(float* v, const float* k, float a)
{
    float ca = cosf(a), sa = sinf(a);
    float vx = v[0], vy = v[1], vz = v[2];
    float kx = k[0], ky = k[1], kz = k[2];
    float cx = ky*vz - kz*vy, cy = kz*vx - kx*vz, cz = kx*vy - ky*vx; // k x v
    float d = kx*vx + ky*vy + kz*vz;                                  // k . v
    v[0] = vx*ca + cx*sa + kx*d*(1 - ca);
    v[1] = vy*ca + cy*sa + ky*d*(1 - ca);
    v[2] = vz*ca + cz*sa + kz*d*(1 - ca);
}


static void M3Mul(const float* A, const float* B, float* out)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            float v = 0.0f;
            for (int k = 0; k < 3; k++) v += A[i*3+k] * B[k*3+j];
            out[i*3+j] = v;
        }
}


static void M3T(const float* A, float* out)
{
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) out[i*3+j] = A[j*3+i];
}


// 3x3 inverse by adjugate. Refuses a determinant far from what a rigid
// response should have - a bad measurement must not become a wild write.
static bool M3Inv(const float* m, float* inv)
{
    float a=m[0], b=m[1], c=m[2], d=m[3], e=m[4], f=m[5], g=m[6], h=m[7], i=m[8];
    float A =  e*i - f*h, B = f*g - d*i, C = d*h - e*g;
    float det = a*A + b*B + c*C;
    float ad = det < 0.0f ? -det : det;
    if (ad < 0.1f || ad > 10.0f) return false;
    float id = 1.0f / det;
    inv[0] = A*id;           inv[1] = (c*h - b*i)*id; inv[2] = (b*f - c*e)*id;
    inv[3] = B*id;           inv[4] = (a*i - c*g)*id; inv[5] = (c*d - a*f)*id;
    inv[6] = C*id;           inv[7] = (b*g - a*h)*id; inv[8] = (a*e - b*d)*id;
    return true;
}


// The controller's full orientation in GAME world axes.
//
// This is where head movement kept leaking in, through every rewrite. I was
// projecting the controller onto the full head frame and then re-expanding it
// through the view frame - and those two frames do NOT cancel. The head frame
// carries real head pitch and roll; the view frame is built from yaw and pitch
// only and has no roll at all. Every difference between them became a phantom
// hand rotation, which on screen is the weapon moving when you move your head.
//
// But the game world and VR space differ by a PURE YAW. g_hmdYaw is
// atan2(fx, -fz), the same convention the view uses, so A = viewYaw - hmdYaw
// is exactly that offset - and because view yaw accumulates head yaw deltas,
// A only changes when you turn with the STICK. Head motion cannot enter.
// Map the axes straight across: VR -Z is game forward, VR +X is game right,
// VR +Y is game up, then spin by A.
// Rows F,R,U from world yaw/pitch, zero roll - identical convention to
// MaimDirFromView (the dot=+1.00 proven aim math).
static void BasisFromYawPitch(float yaw, float pitch, float* b9)
{
    float cp = cosf(pitch), sp = sinf(pitch);
    float cy = cosf(yaw),   sy = sinf(yaw);
    b9[0] =  cp*cy; b9[1] =  cp*sy; b9[2] = sp;     // forward
    b9[3] = -sy;    b9[4] =  cy;    b9[5] = 0;      // right
    b9[6] = -sp*cy; b9[7] = -sp*sy; b9[8] = cp;     // up
}


static bool FpHandBasisWorld(int hand, float* b9)
{
    int dev = (hand >= 0 && hand <= 1) ? g_ctrlIdx[hand] : -1;
    if (dev < 0 || dev >= 16) return false;
    if (!g_devPoseOk[dev]) return false;

    float (*h)[4] = g_devPose[dev];
    float ax[3][3] = {
        { -h[0][2], -h[1][2], -h[2][2] },      // controller forward
        {  h[0][0],  h[1][0],  h[2][0] },      // controller right
        {  h[0][1],  h[1][1],  h[2][1] }       // controller up
    };

    float A = g_viewYawRad - g_hmdYaw;
    float ca = cosf(A), sa = sinf(A);
    for (int r = 0; r < 3; r++) {
        float gx = -ax[r][2];                  // VR -Z  -> game forward
        float gy =  ax[r][0];                  // VR +X  -> game right
        float gz =  ax[r][1];                  // VR +Y  -> game up
        b9[r*3+0] = gx*ca - gy*sa;
        b9[r*3+1] = gx*sa + gy*ca;
        b9[r*3+2] = gz;
    }
    return true;
}


static bool FpBasis(uint8_t* o, float* b9)
{
    if (!LooksLikeObj(o) || !RangeReadable(o + 0x60, 0x24)) return false;
    const float* m = (const float*)(o + 0x60);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) b9[r*3+c] = m[r*4+c];
    return true;
}


// Reading a sign off one matrix row was the wrong measurement. These meshes
// have permuted local axes - pMesh's middle row dumped as a flat (0,0,-1),
// meaning local Y points straight down in world - so row 0 is not "forward"
// and the angle I extracted from it was not the angle I commanded. That is
// why the SAME sword measured pitchSign -1, then 0, then +1 on three runs.
//
// Compare whole bases instead. With our command zeroed the basis IS the
// parent, so B_probe * B_rest^T isolates exactly the rotation the engine
// applied, in the same space as the rotator. Its antisymmetric part gives the
// axis, and the axis tells us the sign no matter how the mesh is oriented.
static void FpDeltaAxis(const float* b1, const float* b0, float* axis)
{
    float R[3][3];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            R[i][j] = b1[i*3+0]*b0[j*3+0] + b1[i*3+1]*b0[j*3+1] + b1[i*3+2]*b0[j*3+2];
    axis[0] = R[2][1] - R[1][2];
    axis[1] = R[0][2] - R[2][0];
    axis[2] = R[1][0] - R[0][1];
}

// Gram-Schmidt on rows. The parent matrices here are right-handed (checked
// against both weapons' rest bases), so rebuilding row2 by cross product is
// safe and keeps a blended rotation a rotation.
static void M3OrthoRows(float* m)
{
    float* r0 = m; float* r1 = m + 3; float* r2 = m + 6;
    V3Norm(r0);
    float d = V3Dot(r1, r0);
    r1[0] -= d*r0[0]; r1[1] -= d*r0[1]; r1[2] -= d*r0[2];
    V3Norm(r1);
    V3Cross(r0, r1, r2);
}
