// game/dishonored/hands/hand_pose.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static inline void V3Cross(const float* a, const float* b, float* o)
{
    o[0] = a[1]*b[2] - a[2]*b[1];
    o[1] = a[2]*b[0] - a[0]*b[2];
    o[2] = a[0]*b[1] - a[1]*b[0];
}

static inline float V3Dot(const float* a, const float* b)
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

static inline float V3Norm(float* v)
{
    float l = sqrtf(V3Dot(v, v));
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
    return l;
}


// Controller ray AND offset relative to the (roll-free) head, in tracking
// space. rel/relPos = {right, up, forward}. Pure pose math - no game reads.
static bool HandRelFull(int hand, float* rel, float* relPos)
{
    int devHand = (hand >= 0 && hand <= 1) ? g_ctrlIdx[hand] : -1;
    if (devHand < 0 || devHand >= 16) return false;
    if (!g_devPoseOk[devHand] || !g_devPoseOk[0]) return false;

    // controller ray: raw fwd = -Z column, tilted PitchOffsetDeg toward the
    // grip (local -Y) for a natural point
    float (*h)[4] = g_devPose[devHand];
    float fwdH[3]  = { -h[0][2], -h[1][2], -h[2][2] };
    float downH[3] = { -h[0][1], -h[1][1], -h[2][1] };
    float a = g_maimPitchOff * 3.14159265f / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    float ray[3] = { fwdH[0]*ca + downH[0]*sa,
                     fwdH[1]*ca + downH[1]*sa,
                     fwdH[2]*ca + downH[2]*sa };
    V3Norm(ray);

    // head frame WITHOUT roll (the game camera has no roll either)
    float (*hm)[4] = g_devPose[0];
    float fwdT[3] = { -hm[0][2], -hm[1][2], -hm[2][2] };
    if (V3Norm(fwdT) < 0.5f) return false;
    float upW[3] = { 0, 1, 0 };
    float rightT[3]; V3Cross(fwdT, upW, rightT);
    if (V3Norm(rightT) < 0.2f) return false;   // looking straight up/down
    float upT[3]; V3Cross(rightT, fwdT, upT); V3Norm(upT);

    rel[0] = V3Dot(ray, rightT);
    rel[1] = V3Dot(ray, upT);
    rel[2] = V3Dot(ray, fwdT);
    if (g_maimFlipR) rel[0] = -rel[0];
    if (g_maimFlipU) rel[1] = -rel[1];
    if (relPos) {                       // hand position relative to the head
        float dp[3] = { h[0][3] - hm[0][3], h[1][3] - hm[1][3], h[2][3] - hm[2][3] };
        relPos[0] = V3Dot(dp, rightT);
        relPos[1] = V3Dot(dp, upT);
        relPos[2] = V3Dot(dp, fwdT);
    }
    return true;
}


// Raw controller position in TRACKING (room) space. The room is the one
// frame the head cannot move: anchoring the weapon's position offset here is
// what stops head motion from dragging the weapons around (build 30.2). The
// old head-relative offset made every lean and turn read as hand movement.
static bool HandRoomPos(int hand, float* out)
{
    int dev = (hand >= 0 && hand <= 1) ? g_ctrlIdx[hand] : -1;
    if (dev < 0 || dev >= 16) return false;
    if (!g_devPoseOk[dev]) return false;
    float (*h)[4] = g_devPose[dev];
    out[0] = h[0][3]; out[1] = h[1][3]; out[2] = h[2][3];
    return true;
}


static bool MaimHandRel(float* rel) { return HandRelFull(g_maimHand, rel, NULL); }


// Head motion was steering the weapon, and the reason is in the frame we
// measured against. HandRelFull reports the controller ray relative to your
// HEAD, pitch included - so looking up 30 degrees drops the reported hand
// pitch by 30 degrees even though your hand never moved.
//
// The component's rotation is relative to the PAWN, and the pawn's world
// matrix is yaw-only (row1 was a flat (0,0,-1) in both dumps): it turns with
// you but never pitches. So the two angles need different frames. Yaw must
// stay head-relative, because the pawn's yaw already follows your view and the
// two cancel. Pitch must be ABSOLUTE, because the pawn has no pitch to cancel
// against - measure it straight off the controller and head movement stops
// mattering.
static bool HandAnglesPos(int hand, float* yawRel, float* pitchAbs, float* posFlat)
{
    int devHand = (hand >= 0 && hand <= 1) ? g_ctrlIdx[hand] : -1;
    if (devHand < 0 || devHand >= 16) return false;
    if (!g_devPoseOk[devHand] || !g_devPoseOk[0]) return false;

    float (*h)[4] = g_devPose[devHand];
    float fwdH[3]  = { -h[0][2], -h[1][2], -h[2][2] };
    float downH[3] = { -h[0][1], -h[1][1], -h[2][1] };
    float a = g_maimPitchOff * 3.14159265f / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    float ray[3] = { fwdH[0]*ca + downH[0]*sa,
                     fwdH[1]*ca + downH[1]*sa,
                     fwdH[2]*ca + downH[2]*sa };
    if (V3Norm(ray) < 0.5f) return false;

    // head frame flattened to the horizon - no pitch, no roll
    float (*hm)[4] = g_devPose[0];
    float hf[3] = { -hm[0][2], 0.0f, -hm[2][2] };
    if (V3Norm(hf) < 0.1f) return false;
    float hr[3] = {  hm[0][0], 0.0f,  hm[2][0] };
    if (V3Norm(hr) < 0.1f) return false;

    float rf[3] = { ray[0], 0.0f, ray[2] };
    float yr = 0.0f;
    if (V3Norm(rf) > 0.05f) yr = atan2f(V3Dot(rf, hr), V3Dot(rf, hf));
    float y = ray[1];
    if (y >  1.0f) y =  1.0f;
    if (y < -1.0f) y = -1.0f;
    float pa = asinf(y);

    if (g_maimFlipR) yr = -yr;
    if (g_maimFlipU) pa = -pa;
    *yawRel = yr; *pitchAbs = pa;

    if (posFlat) {                       // hand offset from the head, same flat frame
        float dp[3] = { h[0][3] - hm[0][3], h[1][3] - hm[1][3], h[2][3] - hm[2][3] };
        posFlat[0] = V3Dot(dp, hr);
        posFlat[1] = dp[1];
        posFlat[2] = V3Dot(dp, hf);
    }
    return true;
}
