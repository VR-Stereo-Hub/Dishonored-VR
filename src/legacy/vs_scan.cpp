// legacy/vs_scan.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ----------------------------------------------------------------------------
// SetVertexShaderConstantF hook + scanner
// ----------------------------------------------------------------------------
static inline bool IsAffineRowMajor(const float* m)
{
    // last column (m[3],m[7],m[11],m[15]) ~ (0,0,0,1)
    const float e = 0.0005f;
    return fabsf(m[3]) < e && fabsf(m[7]) < e && fabsf(m[11]) < e &&
           fabsf(m[15] - 1.0f) < e;
}

static inline bool IsAffineColMajor(const float* m)
{
    // last row (m[12],m[13],m[14],m[15]) ~ (0,0,0,1)
    const float e = 0.0005f;
    return fabsf(m[12]) < e && fabsf(m[13]) < e && fabsf(m[14]) < e &&
           fabsf(m[15] - 1.0f) < e;
}


static inline bool Finite16(const float* m)
{
    for (int i = 0; i < 16; i++) {
        float v = m[i];
        if (v != v || v > 3.0e38f || v < -3.0e38f) return false;
    }
    return true;
}


// Reflection/mirror passes (water, planar mirrors) upload a matrix whose
// upper-3x3 has a NEGATIVE determinant. Shearing those warps the water, so we
// detect and skip them. Works for both row- and column-major (det sign is the
// same under transpose).
static inline bool IsMirrored(const float* m)
{
    float a=m[0], b=m[1], c=m[2];
    float d=m[4], e=m[5], f=m[6];
    float g=m[8], h=m[9], i=m[10];
    float det = a*(e*i - f*h) - b*(d*i - f*g) + c*(d*h - e*g);
    return det < 0.0f;
}


// Are we currently drawing the MAIN scene pass? Shadow maps are square, small
// reflection/UI targets are small - the main view is a wide (16:9-ish) target
// near the game resolution. Only shear that one; shearing shadow/reflection
// passes produces artifacts and was the source of the hotkey crash.
static inline bool IsMainScenePass()
{
    if (g_curRTw == 0 || g_curRTh == 0) return true;      // unknown early on -> allow
    if (g_curRTw < 640) return false;                      // too small (UI/reflection)
    float aspect = (float)g_curRTw / (float)g_curRTh;
    return aspect > 1.4f && aspect < 2.4f;                 // wide -> scene, not square shadow
}


// Apply the 3D-Vision separation+convergence shear to a row-major MVP/VP so
// that clip.x += eyeSign*sep*(clip.w - convergence). After the perspective
// divide this yields ndc.x += eyeSign*sep*(1 - convergence/w): a horizontal
// disparity that grows/shrinks with depth = real stereo parallax.
static inline void ShearVP(float* m, float eyeSign)
{
    float s = eyeSign * g_sepClip;
    if (!g_stereoTranspose) {
        // row-major (translation in row 3): clip.x = pos . col0, clip.w = pos . col3
        m[0]  += s * m[3];    // r0: x += s*w
        m[4]  += s * m[7];    // r1
        m[8]  += s * m[11];   // r2
        m[12] += s * (m[15] - g_converge); // r3 (pos.w=1): +s*w - s*convergence
    } else {
        // column-major fallback
        m[0]  += s * m[12];
        m[1]  += s * m[13];
        m[2]  += s * m[14];
        m[3]  += s * (m[15] - g_converge);
    }
}


// Stage 5.0: fold the positional head offset (lean/peek/crouch) into the VP as
// a pure view-space translation. A camera moved right by t game-units changes
// clip.x by -t*P00 for every world vertex (w=1); after the perspective divide
// that's ndc -t*P00/w - near objects shift more than far ones = true positional
// parallax. P00/P11 (the projection scales) are recovered from the matrix's own
// column norms, so no FOV assumption is needed. Unlike the stereo shear there
// is NO uniform screen-shift term: at infinity the world stays put, which is
// exactly how real head translation behaves. No rotation math anywhere.
static inline void LeanVP(float* m)
{
    float rx = g_leanRightUU * (g_posFlipX ? -1.0f : 1.0f);
    float uy = g_leanUpUU;
    float fz = g_leanFwdUU;
    if (!g_stereoTranspose) {
        // row-major: col0 = (m0,m4,m8 | m12), col1 = (m1,m5,m9 | m13)
        float p00 = sqrtf(m[0]*m[0] + m[4]*m[4] + m[8]*m[8]);
        float p11 = sqrtf(m[1]*m[1] + m[5]*m[5] + m[9]*m[9]);
        m[12] -= rx * p00;   // head right -> world left on screen
        m[13] -= uy * p11;   // head up    -> world down on screen
        // 30.35: forward axis. Camera-relative VP: moving the camera forward
        // by fz shrinks every point's view depth. col3 (w) is the unit view-
        // forward, col2 (z) is forward scaled by the depth-range factor -
        // subtract fz times each column's own scale from the constant row.
        float p22 = sqrtf(m[2]*m[2] + m[6]*m[6] + m[10]*m[10]);
        m[14] -= fz * p22;
        m[15] -= fz;
    } else {
        // column-major: col0 = (m0,m1,m2 | m3), col1 = (m4,m5,m6 | m7)
        float p00 = sqrtf(m[0]*m[0] + m[1]*m[1] + m[2]*m[2]);
        float p11 = sqrtf(m[4]*m[4] + m[5]*m[5] + m[6]*m[6]);
        m[3] -= rx * p00;
        m[7] -= uy * p11;
        float p22 = sqrtf(m[8]*m[8] + m[9]*m[9] + m[10]*m[10]);
        m[11] -= fz * p22;
        m[15] -= fz;
    }
}


static void DumpVSConstScan()
{
    g_scanDumps++;
    Log("scan: ---- VS-constant scan dump %d (frame %lu) ----", g_scanDumps,
        (unsigned long)g_frame);
    // rank registers by non-affine count (VP-matrix candidates)
    for (int pick = 0; pick < 8; pick++) {
        uint32_t best = 0, bestR = 0xffffffff;
        for (uint32_t r = 0; r < VS_REGS - 3; r++) {
            if (g_vsNonAffine[r] > best) { best = g_vsNonAffine[r]; bestR = r; }
        }
        if (bestR == 0xffffffff || best == 0) break;
        float* m = g_vsLast[bestR];
        Log("scan: c%-3u nonAffine=%u/%u changes=%u", bestR,
            g_vsNonAffine[bestR], g_vsCallCount[bestR], g_vsChanged[bestR]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[0], m[1], m[2], m[3]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[4], m[5], m[6], m[7]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[8], m[9], m[10], m[11]);
        Log("scan:      [% .3f % .3f % .3f % .3f]", m[12], m[13], m[14], m[15]);
        g_vsNonAffine[bestR] = 0; // so the next pick finds the next register
    }
    // reset tallies for the next window
    memset(g_vsCallCount, 0, sizeof(g_vsCallCount));
    memset(g_vsNonAffine, 0, sizeof(g_vsNonAffine));
    memset(g_vsChanged, 0, sizeof(g_vsChanged));
    Log("scan: ---- end dump ----");
}
