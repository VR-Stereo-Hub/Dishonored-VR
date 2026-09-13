// game/dishonored/blink.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).
#include "game/dishonored/fire_aim_math.h"   // VR-36: the shared XR-to-world converter


static inline bool BpAlive(int i)       // 32.6 lesson: GObjects is the liveness test
{
    if (i < 0 || i >= g_bpN) return false;
    uint8_t* o = g_bpObj[i];
    if (!o || !g_bpIdx[i]) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || g_bpIdx[i] >= num) return false;
    if ((uint8_t*)objs[g_bpIdx[i]] != o) return false;
    return *(void**)(o + kClassOff) == g_bpCls[i];
}


// A live instance sits under a Level/World. A template sits under a Package.
// Nothing on a template ever moves, so watching one is a wasted slot.
static bool BpIsLive(uint8_t* o)
{
    uint8_t* cur = o;
    for (int lvl = 0; lvl < 6; lvl++) {
        if (!RangeReadable(cur + kOuterOff, 4)) return false;
        uint8_t* out = *(uint8_t**)(cur + kOuterOff);
        if (!out || ((uintptr_t)out & 3) || !RangeReadable(out, kClassOff + 4)) return false;
        const char* cn = ObjClassName(out);
        if (!cn) return false;
        if (strstr(cn, "Level") || strstr(cn, "World")) return true;
        cur = out;
    }
    return false;
}


static void BpAdd(uint8_t* o, uint32_t idx, const char* why)
{
    if (g_bpN >= kBpMax || !o || ((uintptr_t)o & 3)) return;
    if (!RangeReadable(o, kClassOff + 4)) return;
    for (int q = 0; q < g_bpN; q++) if (g_bpObj[q] == o) return;
    int s = g_bpN++;
    g_bpObj[s] = o; g_bpIdx[s] = idx;
    g_bpCls[s] = *(void**)(o + kClassOff);
    // resolve the readable extent ONCE - a VirtualQuery per offset per sample
    // would be 15k of them a frame
    g_bpLimit[s] = 0;
    static const uint32_t steps[4] = { 0x800, 0x400, 0x200, 0x100 };
    for (int t = 0; t < 4; t++)
        if (RangeReadable(o, steps[t] + 12)) { g_bpLimit[s] = steps[t]; break; }
    const char* cn = ObjClassName(o);
    const char* on = RangeReadable(o + kNameOff, 4)
                   ? RealName(*(uint32_t*)(o + kNameOff)) : "?";
    _snprintf(g_bpLabel[s], sizeof(g_bpLabel[s]) - 1, "%s '%s'",
              cn ? cn : "?", on ? on : "?");
    g_bpLabel[s][sizeof(g_bpLabel[s]) - 1] = 0;
    Log("blink:   [%d] %-52s (%s, scan to +0x%03x)", s, g_bpLabel[s], why,
        (unsigned)g_bpLimit[s]);
}


// ============================================================================
// 32.14 - BLINK WRITE TEST: is +0x060 an INPUT or just a readout?
//
// MEASURED (32.13, four consecutive progress lines, one run):
//   DishonoredActivePowerComponent_Blink 'PowerBlink'
//     +0x060  hits=363  best dot=1.000   dist 795 -> 747 -> 900 -> 985 uu
//     +0x0d0  hits=340  best dot=1.000   dist 812 -> 762 -> 913 -> 1002 uu
// Both track the aim perfectly. +0x0d0 sits a consistent ~15-17 uu FURTHER
// out than +0x060 at every distance, which reads like raw trace hit (0x0d0)
// versus the landing spot pulled back off the surface (0x060).
//
// That is where the aim LIVES. It is not yet where the aim COMES FROM, and
// that distinction is the one I flagged before writing any of this: if the
// destination is retraced from the camera when you release, writing these
// moves a decal and nothing else.
//
// So: one decisive test before building the feature. Take the game's own
// point, rotate it a fixed 25 degrees about world Z, write it back at
// ProcessEvent rate (the FOV-lever trick - no aim-shaped script events exist,
// so the trace is native and something will restamp a once-per-frame write).
// Then two questions get answered by looking:
//   1. does the MARKER swing 25 deg right?      -> the field feeds the display
//   2. does the BLINK land there?               -> the field feeds the teleport
// Only if both are yes is this feature a matter of arithmetic.

static inline bool BlkAlive()            // 32.6 lesson, again
{
    if (!g_blkObj || !g_blkIdx) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || g_blkIdx >= num) return false;
    if ((uint8_t*)objs[g_blkIdx] != g_blkObj) return false;
    return *(void**)(g_blkObj + kClassOff) == g_blkCls;
}


static void BlinkLatch()
{
    if (BlkAlive()) return;
    // 32.99: SLICED. This walk - every object in GObjects, a readability
    // probe and a class-name compare each - used to run in ONE gulp on the
    // game thread whenever the latch was stale. Millions of objects, tens of
    // milliseconds, one hitched frame: the "little hitch when arming Blink".
    // Now it walks 16k objects per call and resumes where it left off; the
    // latch lands a few frames later instead of stalling any single frame.
    // After a full fruitless sweep it rests 2 s before sweeping again.
    double now = MaimNowMs();
    if (now < g_blkNextFind) return;
    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    static uint32_t cur = 1;
    if (cur >= num) cur = 1;
    uint32_t end = cur + 16384;
    if (end > num) end = num;
    for (; cur < end; cur++) {
        uint8_t* o = (uint8_t*)objs[cur];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x100)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || !strstr(cn, "ActivePowerComponent_Blink")) continue;
        if (!BpIsLive(o)) continue;
        g_blkObj = o; g_blkIdx = cur; g_blkCls = *(void**)(o + kClassOff);
        Log("blink: latched the live PowerBlink @ %p (GObjects[%u])",
            (void*)o, cur);
        return;
    }
    if (end == num) {          // sweep wrapped without a hit: rest, restart
        cur = 1;
        g_blkNextFind = now + 2000.0;
        g_blkObj = NULL; g_blkIdx = 0; g_blkCls = NULL;
    }
}


// Runs in the ProcessEvent lane, beside FovLeverApply - the only write rate
// that has ever beaten a native recompute in this game.
static inline void BlinkTestApply()
{
    if (!g_blkTest || !BlkAlive()) return;
    if (!CamStillValid() || !RangeReadable(g_camObj + 0x80, 12)) return;
    {
        double nw = MaimNowMs();
        if (nw >= g_blkNextTell) {
            g_blkNextTell = nw + 2000.0;
            LONG ap = InterlockedExchange(&g_blkApplies, 0);
            LONG en = InterlockedExchange(&g_blkEngine, 0);
            const float* cur = (const float*)(g_blkObj + 0x060);
            Log("blink/race: %ld applies, %ld of them found the ENGINE had "
                "rewritten it (%.0f%%). src=(%.0f,%.0f,%.0f) ours=(%.0f,%.0f,%.0f) "
                "now=(%.0f,%.0f,%.0f)",
                (long)ap, (long)en, ap ? 100.0 * (double)en / (double)ap : 0.0,
                g_blkSrc[0][0], g_blkSrc[0][1], g_blkSrc[0][2],
                g_blkWrote[0][0], g_blkWrote[0][1], g_blkWrote[0][2],
                cur[0], cur[1], cur[2]);
        }
    }
    const float* cp = (const float*)(g_camObj + 0x80);
    const float  a  = g_blkTestDeg * 3.14159265f / 180.0f;
    const float  ca = cosf(a), sa = sinf(a);
    const uint32_t offs[2] = { 0x060, 0x0d0 };
    const bool     want[2] = { g_blkWrite060, g_blkWrite0d0 };
    for (int q = 0; q < 2; q++) {
        if (!want[q]) continue;
        if (!RangeReadable(g_blkObj + offs[q], 12)) continue;
        float* v = (float*)(g_blkObj + offs[q]);
        // did the ENGINE author this since our last write? if so it is the
        // source of truth; if it still holds our own value, keep the source we
        // already have and do NOT feed our output back in
        bool engineWrote = !g_blkHaveWrote[q];
        if (!engineWrote)
            for (int c = 0; c < 3; c++)
                if (fabsf(v[c] - g_blkWrote[q][c]) > 0.05f) { engineWrote = true; break; }
        if (q == 0) {
            InterlockedIncrement(&g_blkApplies);
            if (engineWrote) InterlockedIncrement(&g_blkEngine);
        }
        if (engineWrote) { memcpy(g_blkSrc[q], v, 12); g_blkHaveSrc[q] = true; }
        if (!g_blkHaveSrc[q]) continue;
        const float* sv = g_blkSrc[q];
        float r[3] = { sv[0] - cp[0], sv[1] - cp[1], sv[2] - cp[2] };
        float d2 = r[0]*r[0] + r[1]*r[1] + r[2]*r[2];
        if (!(d2 > 400.0f && d2 < 3.6e7f)) continue;      // 20 .. 6000 uu
        float nx = r[0]*ca - r[1]*sa;                      // yaw about world Z
        float ny = r[0]*sa + r[1]*ca;
        if (nx != nx || ny != ny) continue;
        v[0] = cp[0] + nx; v[1] = cp[1] + ny; v[2] = cp[2] + r[2];
        memcpy(g_blkWrote[q], v, 12);
        g_blkHaveWrote[q] = true;
        InterlockedIncrement(&g_blkWrites);
    }
}


// The controller ray, in world axes. Same path that measured dot(hand)=+1.00
// against live projectiles in 7.3 - the one piece of hand maths here that was
// verified against the game rather than reasoned about.
// 32.86: like HandRelFull, but the head frame is rebuilt from the SNAPSHOT
// taken at the last camera write instead of the live pose. The controller ray
// is world-anchored (tracking space); hmd-at-write and camera-at-write are a
// matched pair; so head rotation between camera writes cancels out exactly.
// Blink-only on purpose - the weapon aim paths work and are not touched.
static bool HandRelSnap(int hand, float* rel)
{
    if (!g_injSnapOk) return false;
    int devHand = (hand >= 0 && hand <= 1) ? g_ctrlIdx[hand] : -1;
    if (devHand < 0 || devHand >= 16) return false;
    if (!g_devPoseOk[devHand]) return false;

    float (*h)[4] = g_devPose[devHand];
    float fwdH[3]  = { -h[0][2], -h[1][2], -h[2][2] };
    float downH[3] = { -h[0][1], -h[1][1], -h[2][1] };
    float a = g_maimPitchOff * 3.14159265f / 180.0f;
    float ca = cosf(a), sa = sinf(a);
    float ray[3] = { fwdH[0]*ca + downH[0]*sa,
                     fwdH[1]*ca + downH[1]*sa,
                     fwdH[2]*ca + downH[2]*sa };
    V3Norm(ray);

    // head frame from the snapshot, tracking axes (y up, fwd = -Z at yaw 0),
    // roll-free - the same conventions g_hmdYaw/g_hmdPitch are extracted with
    float cp = cosf(g_injHmdPitchSnap), sp = sinf(g_injHmdPitchSnap);
    float cy = cosf(g_injHmdYawSnap),   sy = sinf(g_injHmdYawSnap);
    float fwdT[3] = { cp * sy, sp, -cp * cy };
    float upW[3] = { 0, 1, 0 };
    float rightT[3]; V3Cross(fwdT, upW, rightT);
    if (V3Norm(rightT) < 0.2f) return false;   // looking straight up/down
    float upT[3]; V3Cross(rightT, fwdT, upT); V3Norm(upT);

    rel[0] = V3Dot(ray, rightT);
    rel[1] = V3Dot(ray, upT);
    rel[2] = V3Dot(ray, fwdT);
    if (g_maimFlipR) rel[0] = -rel[0];
    if (g_maimFlipU) rel[1] = -rel[1];
    return true;
}


// VR-36: THE PUBLISHED RAY, IN GAME WORLD UNITS. The only direction source.
//
// dvr::aim::fire_frame() is the one publication the dot, the laser and the
// crossbow/pistol launch hooks all consume; dvr::fireaim::solve is VR-57/VR-82's
// converter from XR LOCAL metres into world units, through the head basis and
// the view yaw/pitch at [PosTrack] Scale. Nothing is derived here that is not
// already derived for the shot, which is what makes this one ray rather than
// two rays that happen to agree.
//
// The spawn argument is the camera, so solve()'s reach guard compares the
// controller against the camera rather than against a point already displaced
// by a muzzle standoff - Blink has no standoff.
//
// A refusal is a refusal. The caller leaves the engine's own vector alone and
// Blink is head-aimed, unchanged, because the thing being aimed here is where
// the player's body ends up.
static bool BlinkAimRayWorld(float* outOrigin, float* outDir)
{
    const auto aim = dvr::aim::fire_frame();
    float camera[3];
    if (!dvr::camera::render_pos_world(camera)) {
        g_blkRayWhy = "no world camera position";
        InterlockedIncrement(&g_blkRayRefused);
        return false;
    }
    dvr::fireaim::Solution sol;
    if (!dvr::fireaim::solve(aim, GetTickCount64(), g_viewYawRad, g_viewPitchRad,
                             camera, g_posScaleUU, camera, sol)) {
        g_blkRayWhy = aim.ray.ok ? (aim.headValid ? "launch geometry refused the ray"
                                                  : "no head pose with the ray")
                                 : aim.ray.why;
        InterlockedIncrement(&g_blkRayRefused);
        return false;
    }
    float d[3] = { sol.target[0] - sol.origin[0],
                   sol.target[1] - sol.origin[1],
                   sol.target[2] - sol.origin[2] };
    if (!dvr::fireaim::normalize(d)) {
        g_blkRayWhy = "degenerate ray (origin and target coincide)";
        InterlockedIncrement(&g_blkRayRefused);
        return false;
    }
    float gap[3] = { sol.origin[0] - camera[0], sol.origin[1] - camera[1],
                     sol.origin[2] - camera[2] };
    g_blkRayGapUU = sqrtf(dvr::fireaim::dot(gap, gap));
    memcpy(g_blkRayOrigin, sol.origin, 12);
    memcpy(g_blkRayDir, d, 12);
    memcpy(outOrigin, sol.origin, 12);
    memcpy(outDir, d, 12);
    g_blkRayWhy = "ready";
    InterlockedIncrement(&g_blkRayUsed);
    return true;
}


// 32.90: ROLLED BACK to the 32.86 aim, byte for byte in behaviour.
// The user's timeline is the authority here: 32.86 was tested and called
// fixed. What broke the two builds after it was the 32.87 crouch pulse
// pressing B mid-play (fixed by the 32.88 gates, which stay), and then the
// 32.89 rewrite of THIS function - a "stronger" formulation that changed the
// yaw conventions and introduced the very coupling it claimed to prevent.
// Lesson pinned here so it survives me: when a tested-good build exists,
// restore it EXACTLY; do not improve it on the way back.
static bool BlinkLegacyControllerDir(float* out)
{
    float rel[3];
    int hand = (g_maimHand == 1) ? 1 : 0;
    // snapshot frame first; live frame only as the fallback before the first
    // camera write, so blink never goes dead
    if (!HandRelSnap(hand, rel)) {
        if (!HandRelFull(hand, rel, NULL)) return false;
    }
    MaimDirFromView(g_viewYawRad, g_viewPitchRad, rel, out);
    float n = sqrtf(out[0]*out[0] + out[1]*out[1] + out[2]*out[2]);
    if (!(n > 0.5f && n < 2.0f)) return false;
    out[0] /= n; out[1] /= n; out[2] /= n;
    // 32.93: the probe goes SILENT, the tripwire stays armed.
    // The root cause is fixed (32.92: both camera writers publish the frame),
    // 3 consecutive launches verified, and the user reports the 2 Hz logging
    // costs frames. But the failure mode this probe caught - the aim frame
    // freezing while the head moves - is exactly the kind that comes back
    // quietly. So: no logging in the healthy case at all. The frozen-frame
    // condition itself is watched with two float compares per aim tick, and
    // if it EVER recurs, three loud lines name it and the old 2 Hz probe
    // re-arms for the rest of the session. Zero cost when healthy, full
    // evidence when not. BlinkProbe=1 in [Blink] forces the 2 Hz line back
    // on for a debugging session.
    {
        static float  lastViewYaw = -999.0f, lastHmdYaw = -999.0f;
        static float  hmdMoved = 0.0f;
        static int    froze = 0;
        static bool   probeArmed = false;
        static double tell = 0.0;
        if (lastViewYaw != -999.0f) {
            float dh = g_hmdYaw - lastHmdYaw;
            while (dh >  3.14159265f) dh -= 6.2831853f;
            while (dh < -3.14159265f) dh += 6.2831853f;
            if (g_viewYawRad == lastViewYaw) {
                hmdMoved += (dh < 0 ? -dh : dh);
                froze++;
            } else {
                hmdMoved = 0.0f; froze = 0;
            }
            // the head turned >20 deg across many aim ticks and the camera
            // frame never moved once: the 32.91 bug shape, exactly
            if (!probeArmed && froze > 30 && hmdMoved > 0.35f) {
                probeArmed = true;
                Log("blink/TRIPWIRE: aim frame FROZEN (viewYaw=%.1f) while the "
                    "head moved %.0f deg - the camera writer stopped "
                    "publishing. Probe re-armed; see "
                    "claude/blink-head-coupling-root-cause.md",
                    g_viewYawRad * 57.2958f, hmdMoved * 57.2958f);
            }
        }
        lastViewYaw = g_viewYawRad; lastHmdYaw = g_hmdYaw;
        if ((probeArmed || g_blkProbeForce) && MaimNowMs() >= tell) {
            tell = MaimNowMs() + 500.0;
            Log("blink/probe: outYaw=%+.1f | viewYaw=%+.1f snapYaw=%+.1f "
                "A=%+.1f | liveHmdYaw=%+.1f",
                atan2f(out[1], out[0]) * 57.2958f,
                g_viewYawRad * 57.2958f, g_injHmdYawSnap * 57.2958f,
                (g_viewYawRad - g_injHmdYawSnap) * 57.2958f,
                g_hmdYaw * 57.2958f);
        }
    }
    return true;
}


// VR-36: every seam in this module goes through here, and only here.
//
// UseAimRay=1 (default) is the published ray. UseAimRay=0 is the legacy
// MotionAim ray, kept as a NAMED A/B so the two can be compared in one headset
// run - it is never reached by accident, and a failure of the published ray
// does not quietly hand aiming back to it. `blink ray aim|legacy` switches live.
static bool BlinkControllerDir(float* out)
{
    if (!g_blkUseAimRay) return BlinkLegacyControllerDir(out);
    float origin[3];
    return BlinkAimRayWorld(origin, out);
}


// 32.38: how far along the controller ray the landing point goes.
// The direction was solved in 32.26; the LENGTH was still the engine's, taken
// from a trace along the VIEW, which is why tilting the head slid the marker
// in and out. 32.37 nailed it to a constant, which killed the coupling and the
// control together. This gives the length back to the hand: pitch the
// controller down and the point walks in toward your feet, level it out and it
// runs to the power's full range. The curve is squared so most of the pitch
// range is spent on the near half, where a metre matters.
// VR-36: THE ENGINE'S OWN VECTOR ALREADY CARRIES THE REACH RULE, INCLUDING A
// VERTICAL CAP, AND THIS CURVE MAY ONLY EVER SHORTEN IT.
//
// Measured in one run (2026-09-13). The engine's aim vector at the source seam
// is 1100 uu long while the aim is level, and when the aim rises its Z
// component pins at exactly +500.00 and the LENGTH drops to match:
//
//   engine aim (-1087.11,-157.00,-59.45) len 1100    level, full reach
//   engine aim (  811.29,-136.61, 500.00) len  963
//   engine aim (  400.12,-178.28, 500.00) len  665    steeply up
//
// So "you cannot blink far upwards" is not a downstream refusal to be
// preserved by luck - it is built into the magnitude the engine hands us, and
// keeping that magnitude keeps the rule. ReachMode 1 and 2 replace the
// magnitude, which threw the cap away and let a blink climb into the sky.
//
// Hence the clamp below. Every mode is now bounded by the engine's own reach
// FOR THIS ACTIVATION, so no reach setting, present or future, can make the
// blink go further than the power offered - only nearer. A curve that can only
// subtract cannot reintroduce this fault.
static float BlinkReach(const float* d, float engineDist)
{
    float far_ = (g_blkReachUU > 20.0f) ? g_blkReachUU : g_blkReachSeen;
    if (!(far_ > 20.0f)) far_ = engineDist;        // nothing learned yet
    if (far_ > engineDist) far_ = engineDist;      // never longer than the engine's
    if (g_blkReachMode == 0) return engineDist;    // original behaviour
    if (g_blkReachMode == 1) return (far_ > 20.0f) ? far_ : engineDist;

    float up = d[2] < -1.0f ? -1.0f : (d[2] > 1.0f ? 1.0f : d[2]);
    float pitchDeg = asinf(up) * 57.29577951f;
    g_blkPitchNow = pitchDeg;

    float lo = g_blkPitchNear, hi = g_blkPitchFar;
    if (hi - lo < 5.0f) hi = lo + 5.0f;            // never divide by ~0
    float t = (pitchDeg - lo) / (hi - lo);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    t = t * t;

    float near_ = g_blkNearUU;
    if (near_ < 40.0f)  near_ = 40.0f;
    if (near_ > far_)   near_ = far_;
    float dist = near_ + t * (far_ - near_);
    bool capped = false;
    if (dist > engineDist) { dist = engineDist; capped = true; }

    // Say it out loud once a second. "I cannot get the blink closer" has to be
    // answerable from the log - pitch in, distance out, both visible - and so
    // does "why will it not go further", which is the engine's own cap biting.
    static double next = 0.0;
    double now = MaimNowMs();
    if (now >= next) {
        next = now + 1000.0;
        Log("blink/reach: hand pitch %+.0f deg -> %.0f uu (near %.0f far %.0f, "
            "the engine offered %.0f uu this activation%s)",
            pitchDeg, dist, near_, far_, engineDist,
            capped ? " and CAPPED us to it - aiming up shortens the engine's own "
                     "vector, which is how the game limits an upward blink"
                   : "");
    }
    return dist;
}


extern "C" void __cdecl BlinkAimHook(void* self, uint8_t* framePtr)
{
    InterlockedIncrement(&g_blkHookHits);
    if (!framePtr) return;
    float* dirLocal = (float*)(framePtr - 0xc);      // -0xc,-0x8,-0x4 contiguous
    memcpy(g_blkGameDir, dirLocal, 12);
    bool mine = (self == (void*)g_blkObj);
    // 38.52: any other live power component is "mine" too - identified by
    // its class name at the moment of use (the hook only fires on power
    // activation, so the name lookup costs nothing that matters).
    if (!mine && g_aimAllPowers && self && !((uintptr_t)self & 3) &&
        RangeReadable(self, 0x100)) {
        const char* pcn = ObjClassName((uint8_t*)self);
        if (pcn && strstr(pcn, "ActivePowerComponent") &&
            !strstr(pcn, "DarkVision") && !strstr(pcn, "BendTime")) {
            mine = true;
            static uint32_t toldCls[8]; static int toldN = 0;
            uint32_t ch = 2166136261u;
            for (const char* q = pcn; *q; q++) { ch ^= (uint8_t)*q; ch *= 16777619u; }
            bool told = false;
            for (int qi = 0; qi < toldN; qi++) if (toldCls[qi] == ch) told = true;
            if (!told && toldN < 8) { toldCls[toldN++] = ch;
                Log("magic-aim: %s now aims with the controller", pcn); }
        }
    }
    if (mine) InterlockedIncrement(&g_blkHookMine);
    float d[3];
    if (!BlinkControllerDir(d)) return;
    memcpy(g_blkOurDir, d, 12);
    if (!g_blkAimDrive) return;                      // phase 1: observe only
    if (!mine) return;                               // never redirect NPCs
    memcpy(dirLocal, d, 12);
}


extern "C" void __cdecl BlinkDirHook(void* self, uint8_t* framePtr, float* eng)
{
    (void)framePtr;
    InterlockedIncrement(&g_blkDirHits);
    g_blkDirUse = eng;                         // default: leave the engine alone
    if (!eng || ((uintptr_t)eng & 3)) return;
    if (!RangeReadable(eng, 12)) return;
    memcpy(g_blkDirEng, eng, 12);
    float len = sqrtf(eng[0]*eng[0] + eng[1]*eng[1] + eng[2]*eng[2]);
    g_blkDirLen = len;
    if (!(len > 0.001f)) return;
    uint8_t* o = (uint8_t*)self;
    if (!o || ((uintptr_t)o & 3) || o != g_blkObj) return;   // player's Blink only
    InterlockedIncrement(&g_blkDirMine);
    if (!g_blkDirAim || !g_blkAimDrive) return;              // observe-only
    float d[3];
    if (!BlinkControllerDir(d)) return;
    // 32.69: THE LENGTH WAS STILL THE HEAD'S.
    // 32.50 kept the engine's magnitude and called it "the power's reach", on
    // the assumption it was a constant max range. The log says otherwise - it
    // reads 1434, 1600, 1001, 1600 across a few seconds, varying with where
    // the view points. So the vector handed to us is already shortened by the
    // VIEW, and preserving its magnitude re-imported exactly the head-coupled
    // distance that 32.37-32.38 removed: turn your head and the landing point
    // slides in and out.
    // The fix is the one already written for the destination patch, applied at
    // the source instead - which is where it belonged once the source redirect
    // took over, and which keeps marker and landing locked together because
    // the engine traces the vector we hand it.
    if (len > g_blkReachSeen) g_blkReachSeen = len;   // learn the true max
    float useLen = BlinkReach(d, len);
    if (!(useLen > 20.0f)) useLen = len;
    g_blkAimDistUU = useLen;
    g_blkDirBuf[0] = d[0] * useLen;            // direction AND length ours
    g_blkDirBuf[1] = d[1] * useLen;
    g_blkDirBuf[2] = d[2] * useLen;
    g_blkDirUse = g_blkDirBuf;
    g_blkDirLastMine = MaimNowMs();
}


static void BlinkDirInstall()
{
    if (g_blkDirOn) return;
    uint8_t* at = (uint8_t*)kBlkDirHook;
    if (memcmp(at, kBlkDirOrig, 5) != 0) {
        Log("blinkdir: REFUSING to patch - bytes at 0x%08x are %02x %02x %02x "
            "%02x %02x, expected 8b 08 89 4d b4. Wrong exe build?",
            (unsigned)kBlkDirHook, at[0], at[1], at[2], at[3], at[4]);
        return;
    }
    g_blkDirRet = (uint32_t)kBlkDirBack;
    g_blkDirUse = NULL;
    DWORD op = 0;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        Log("blinkdir: VirtualProtect failed"); return;
    }
    int32_t rel = (int32_t)((uintptr_t)&BlinkDirStub - (kBlkDirHook + 5));
    at[0] = 0xE9;
    memcpy(at + 1, &rel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    g_blkDirOn = true;
    Log("blinkdir: INSTALLED at 0x%08x -> stub %p (aim the trace at its source)",
        (unsigned)kBlkDirHook, (void*)&BlinkDirStub);
}


// VR-36: THE DESTINATION SEAM IS AN INSTRUMENT NOW, NOT A REDIRECT.
//
// This runs AFTER the engine has traced, collided and validated. A destination
// substituted here was never checked against geometry, which is 32.51's
// teleport through walls, and it is precisely what VR-36 rules out:
// unreachable targets must be refused by the engine, not by us. The whole
// write path is gone. The source seam at 0xbf55a3 is the only redirect and it
// hands the engine a ray to do its own work on - so the marker, the collision
// pull-back and the landing point are one trace, by construction.
//
// What it still does is measure. It is the only place that sees the engine's
// settled destination beside the vector the engine was handed.
extern "C" void __cdecl BlinkDestHook(void* self, uint8_t* framePtr)
{
    (void)framePtr;
    InterlockedIncrement(&g_blkDstHits);
    InterlockedExchange(&g_blkAutoDump, 0);   // 41.0: the fork's marker dump is gone
    uint8_t* o = (uint8_t*)self;
    if (!o || ((uintptr_t)o & 3)) return;
    if (o != g_blkObj) return;                    // only the player's Blink
    InterlockedIncrement(&g_blkDstMine);
    if (!CamStillValid() || !RangeReadable(g_camObj + 0x80, 12)) return;
    const float* cp = (const float*)(g_camObj + 0x80);
    const float* dst = (const float*)(o + 0x60);
    if (!RangeReadable((void*)dst, 12)) return;
    float r[3] = { dst[0] - cp[0], dst[1] - cp[1], dst[2] - cp[2] };
    float dist = sqrtf(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    if (!(dist > 20.0f && dist < 6000.0f)) return;
    memcpy(g_blkDstWas, dst, 12);
    memcpy(g_blkDstNow, dst, 12);
    // VR-36: 32.37 LEARNED THE MAXIMUM REACH FROM THIS POINT. IT NO LONGER CAN.
    //
    // Once the source seam drives the aim, this destination is the result of
    // OUR OWN vector, so feeding its distance back into g_blkReachSeen made the
    // input a function of the previous output. It ratcheted, measured in one
    // run: reach 1100 -> 1839 -> 2007 -> 2062 -> 2610 -> 4698 -> 5606 uu across
    // a few minutes of play, monotonically, with the blink getting longer every
    // time - and it could not come back down, because nothing here ever lowered
    // it. The maximum is learned in the dir seam instead, from the engine's own
    // untouched vector length, which is an input we do not author.
    g_blkAimSeen = MaimNowMs();

    // WHERE DOES THE ENGINE'S TRACE START?
    //
    // The source seam cannot see it (END = offset + [ebp-0x24], and the seam
    // that would show that local sits on a branch that never executes). This
    // measures it from the far end: the settled destination taken FROM THE
    // CAMERA, against the vector the engine was handed. Near zero says the
    // trace starts at the camera, and a convergence correction for the
    // controller-to-eye offset is then worth adding; a large angle says it
    // starts somewhere else and such a correction would be a constant chosen
    // by eye. The line is free to print either answer, which is why it is
    // evidence.
    float eng[3] = { g_blkDirEng[0], g_blkDirEng[1], g_blkDirEng[2] };
    float to[3]  = { r[0], r[1], r[2] };
    if (dvr::fireaim::normalize(eng) && dvr::fireaim::normalize(to)) {
        const float c = dvr::fireaim::dot(eng, to);
        g_blkDstAngleDeg = acosf(c < -1 ? -1 : (c > 1 ? 1 : c)) * 57.2957795f;
    }
}

extern "C" void __cdecl BlinkTraceHook(void* self, uint8_t* framePtr)
{
    InterlockedIncrement(&g_blkTrcHits);
    if (!g_blkTraceAim || !framePtr) return;
    if ((uint8_t*)self != g_blkObj) return;        // only the player's Blink
    float* start = (float*)(framePtr - 0x24);      // -0x24,-0x20,-0x1c
    float* end   = (float*)(framePtr - 0x30);      // -0x30,-0x2c,-0x28
    float r[3] = { end[0]-start[0], end[1]-start[1], end[2]-start[2] };
    float len = sqrtf(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    if (!(len > 20.0f && len < 20000.0f)) return;
    float d[3];
    if (!BlinkControllerDir(d)) return;
    end[0] = start[0] + d[0]*len;
    end[1] = start[1] + d[1]*len;
    end[2] = start[2] + d[2]*len;
    g_blkTrcLen = len;
    g_blkTrcLastMine = MaimNowMs();
    InterlockedIncrement(&g_blkTrcMine);
}


static void BlinkTraceInstall()
{
    if (g_blkTrcOn) return;
    uint8_t* at = (uint8_t*)kBlkTrcHook;
    if (memcmp(at, kBlkTrcOrig, 5) != 0) {
        Log("blinktrc: REFUSING to patch - bytes at 0x%08x are not "
            "f3 0f 11 55 d8", (unsigned)kBlkTrcHook);
        return;
    }
    g_blkTrcRet = (uint32_t)kBlkTrcBack;
    DWORD op = 0;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) return;
    int32_t rel = (int32_t)((uintptr_t)&BlinkTraceStub - (kBlkTrcHook + 5));
    at[0] = 0xE9; memcpy(at + 1, &rel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    g_blkTrcOn = true;
    g_blkTrcHits = 0; g_blkTrcMine = 0;
    Log("blinktrc: INSTALLED at 0x%08x - the TRACE now runs down the "
        "controller ray", (unsigned)kBlkTrcHook);
}


static void BlinkTraceRemove()
{
    if (!g_blkTrcOn) return;
    uint8_t* at = (uint8_t*)kBlkTrcHook;
    DWORD op = 0;
    if (VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(at, kBlkTrcOrig, 5);
        VirtualProtect(at, 5, op, &op);
        FlushInstructionCache(GetCurrentProcess(), at, 5);
    }
    g_blkTrcOn = false;
    Log("blinktrc: removed");
}


static void BlinkTraceTick()
{
    if (g_blkTraceAim && !g_blkTrcOn && BlkAlive()) BlinkTraceInstall();
    if (!g_blkTraceAim && g_blkTrcOn) BlinkTraceRemove();
    if (!g_blkTrcOn) return;
    double now = MaimNowMs();
    if (now < g_blkTrcTell) return;
    g_blkTrcTell = now + 3000.0;
    LONG h = InterlockedExchange(&g_blkTrcHits, 0);
    LONG m = InterlockedExchange(&g_blkTrcMine, 0);
    if (h) Log("blinktrc: %ld traces (%ld redirected), ray length %.0f uu",
               (long)h, (long)m, g_blkTrcLen);
}


static void BlinkDestInstall()
{
    if (g_blkDstOn) return;
    uint8_t* at = (uint8_t*)kBlkDstHook;
    if (memcmp(at, kBlkDstOrig, 6) != 0) {
        Log("blinkdst: REFUSING to patch - bytes at 0x%08x are not "
            "8d 85 30 ff ff ff", (unsigned)kBlkDstHook);
        return;
    }
    g_blkDstRet = (uint32_t)kBlkDstBack;
    DWORD op = 0;
    if (!VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &op)) return;
    int32_t rel = (int32_t)((uintptr_t)&BlinkDestStub - (kBlkDstHook + 5));
    at[0] = 0xE9; memcpy(at + 1, &rel, 4); at[5] = 0x90;
    VirtualProtect(at, 6, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 6);
    g_blkDstOn = true;
    g_blkDstHits = 0; g_blkDstMine = 0;
    Log("blinkdst: INSTALLED at 0x%08x (proven-executed site). Tick DRIVE to "
        "redirect; leave it off to just watch.", (unsigned)kBlkDstHook);
}


static void BlinkDestRemove()
{
    if (!g_blkDstOn) return;
    uint8_t* at = (uint8_t*)kBlkDstHook;
    DWORD op = 0;
    if (VirtualProtect(at, 6, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(at, kBlkDstOrig, 6);
        VirtualProtect(at, 6, op, &op);
        FlushInstructionCache(GetCurrentProcess(), at, 6);
    }
    g_blkDstOn = false;
    Log("blinkdst: removed");
}


static void BlinkDestTick()
{
    if (InterlockedExchange(&g_blkDstReq, 0)) {
        if (g_blkDstOn) BlinkDestRemove(); else BlinkDestInstall();
    }
    // 32.27: DRIVE belongs to THIS detour, not the dead 0xbf595f one. It was
    // gated behind that hook's install, so the working feature was hiding
    // under a checkbox that belonged to a detour which never fires. The user
    // found it anyway; nobody else would have.
    g_blkAimDrive = g_blkDriveUI;
    g_blkDstOnUI  = g_blkDstOn;
    // VR-36: Blink is a consumer of the published ray in its own right. Without
    // this it would be aiming off whatever the crosshair and the fire hook
    // happened to leave armed, so turning the dot off would silently return
    // Blink to head aim - the shape of fault this project has paid for twice.
    dvr::aim::request_blink_ray(g_blkAimDrive && g_blkUseAimRay);
    // auto-arm: this is a shipping feature now, not an experiment
    if (g_blkAimOnCfg && !g_blkDstOn && BlkAlive()) BlinkDestInstall();
    if (g_blkDirAim && !g_blkDirOn && BlkAlive()) BlinkDirInstall();
    if (g_blkDirOn) {
        double dn = MaimNowMs();
        static double dirTell = 0.0;
        if (dn >= dirTell) {
            dirTell = dn + 2000.0;
            LONG h = InterlockedExchange(&g_blkDirHits, 0);
            LONG m = InterlockedExchange(&g_blkDirMine, 0);
            LONG ru = InterlockedExchange(&g_blkRayUsed, 0);
            LONG rr = InterlockedExchange(&g_blkRayRefused, 0);
            if (h) {
                // VR-36: name the OWNER before the result. A healthy run and a
                // dead one used to produce identical text; now the line says
                // which ray drove, how often it refused and why, and what the
                // angle between the engine's own aim and ours actually was.
                float eng[3] = { g_blkDirEng[0], g_blkDirEng[1], g_blkDirEng[2] };
                float ours[3] = { g_blkRayDir[0], g_blkRayDir[1], g_blkRayDir[2] };
                float sep = -1.0f;
                if (dvr::fireaim::normalize(eng) && dvr::fireaim::normalize(ours)) {
                    const float c = dvr::fireaim::dot(eng, ours);
                    sep = acosf(c < -1 ? -1 : (c > 1 ? 1 : c)) * 57.2957795f;
                }
                Log("blinkdir: %ld calls (%ld ours) | ray=%s driving=%d | engine "
                    "aim (%.2f,%.2f,%.2f) len %.0f | ours (%.3f,%.3f,%.3f) "
                    "%.1f deg from the engine's, controller %.0f uu from the "
                    "camera | ray ready %ld, refused %ld (%s) | reach %.0f uu "
                    "at hand pitch %+.0f deg",
                    (long)h, (long)m, g_blkUseAimRay ? "published (VR-36)"
                                                     : "legacy MotionAim",
                    (int)(g_blkDirAim && g_blkAimDrive),
                    g_blkDirEng[0], g_blkDirEng[1], g_blkDirEng[2], g_blkDirLen,
                    g_blkRayDir[0], g_blkRayDir[1], g_blkRayDir[2], sep,
                    g_blkRayGapUU, (long)ru, (long)rr, g_blkRayWhy,
                    g_blkAimDistUU, g_blkPitchNow);
            }
        }
    }
    if (!g_blkDstOn) return;
    double now = MaimNowMs();
    if (now < g_blkDstTell) return;
    g_blkDstTell = now + 2000.0;
    LONG h = InterlockedExchange(&g_blkDstHits, 0);
    LONG m = InterlockedExchange(&g_blkDstMine, 0);
    if (!h) return;
    // VR-36: READ-ONLY. The destination is the engine's own, after its trace
    // and its validation, and this seam no longer writes it. The angle is the
    // trace-start measurement: it compares the destination taken from the
    // camera against the vector the engine was handed, so a value near zero
    // means the trace starts at the camera and a larger one means it does not.
    // -1 means no Blink aim has completed since the counters were last read.
    Log("blinkdst: %ld stores (%ld ours), READ-ONLY | engine settled on "
        "(%.0f,%.0f,%.0f) | that point from the camera sits %.2f deg off the "
        "vector the engine was handed (near 0 = the trace starts at the "
        "camera; -1 = not measured yet)",
        (long)h, (long)m, g_blkDstWas[0], g_blkDstWas[1], g_blkDstWas[2],
        g_blkDstAngleDeg);
}


static void BlinkHookInstall()
{
    if (g_blkHookOn) return;
    uint8_t* at = (uint8_t*)kBlkAimHook;
    if (memcmp(at, kBlkAimOrig, 5) != 0) {
        Log("blinkhook: REFUSING to patch - bytes at 0x%08x are %02x %02x %02x "
            "%02x %02x, expected f3 0f 10 45 f4. Wrong exe build?",
            (unsigned)kBlkAimHook, at[0], at[1], at[2], at[3], at[4]);
        return;
    }
    g_blkRet = (uint32_t)kBlkAimBack;
    DWORD op = 0;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        Log("blinkhook: VirtualProtect failed"); return;
    }
    int32_t rel = (int32_t)((uintptr_t)&BlinkAimStub - (kBlkAimHook + 5));
    at[0] = 0xE9;
    memcpy(at + 1, &rel, 4);
    VirtualProtect(at, 5, op, &op);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    g_blkHookOn = true;
    g_blkHookHits = 0; g_blkHookMine = 0;
    Log("blinkhook: INSTALLED at 0x%08x -> stub %p (read-only; tick 'drive' "
        "to substitute)", (unsigned)kBlkAimHook, (void*)&BlinkAimStub);
}


static void BlinkHookRemove()
{
    if (!g_blkHookOn) return;
    uint8_t* at = (uint8_t*)kBlkAimHook;
    DWORD op = 0;
    if (VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(at, kBlkAimOrig, 5);
        VirtualProtect(at, 5, op, &op);
        FlushInstructionCache(GetCurrentProcess(), at, 5);
    }
    g_blkHookOn = false;
    g_blkAimDrive = false;
    Log("blinkhook: removed");
}


static void BlinkHookTick()
{
    if (InterlockedExchange(&g_blkHookReq, 0)) {
        if (g_blkHookOn) BlinkHookRemove(); else BlinkHookInstall();
    }
    g_blkHookOnUI = g_blkHookOn;
    if (!g_blkHookOn) return;
    double now = MaimNowMs();
    if (now < g_blkHookTell) return;
    g_blkHookTell = now + 2000.0;
    LONG h = InterlockedExchange(&g_blkHookHits, 0);
    LONG m = InterlockedExchange(&g_blkHookMine, 0);
    if (!h) return;
    Log("blinkhook: %ld calls (%ld with esi==our PowerBlink) | game dir "
        "(%.3f,%.3f,%.3f)  ours (%.3f,%.3f,%.3f) | drive=%d",
        (long)h, (long)m,
        g_blkGameDir[0], g_blkGameDir[1], g_blkGameDir[2],
        g_blkOurDir[0], g_blkOurDir[1], g_blkOurDir[2], (int)g_blkAimDrive);
}


static void BlinkProbeArm()
{
    g_bpN = 0; g_bpEvtN = 0;
    memset(g_bpHave, 0, sizeof(g_bpHave));
    memset(g_bpMaxMove, 0, sizeof(g_bpMaxMove));
    memset(g_bpDotSum, 0, sizeof(g_bpDotSum));
    memset(g_bpDistSum, 0, sizeof(g_bpDistSum));
    memset(g_bpSamples, 0, sizeof(g_bpSamples));
    memset(g_bpHits, 0, sizeof(g_bpHits));
    memset(g_bpBestDot, 0, sizeof(g_bpBestDot));

    if (!RangeReadable((void*)kGObjHdr, 12)) { Log("blink: GObjects unreadable"); return; }
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) {
        Log("blink: GObjects looks wrong (num=%u)", num); return;
    }
    Log("blink: ==== arming over %u objects (LIVE instances only) ====", num);

    // the player himself first - a power's aim usually lands on the pawn or
    // the controller, not on the power component
    if (g_pePawn && LooksLikeObj(g_pePawn)) BpAdd(g_pePawn, 0, "player pawn");
    if (g_peCtrl && LooksLikeObj(g_peCtrl)) BpAdd(g_peCtrl, 0, "player controller");
    if (g_pePawn && LooksLikeObj(g_pePawn)) {
        for (uint32_t o2 = 0x20; o2 + 4 <= 0x600 && g_bpN < 20; o2 += 4) {
            if (!RangeReadable(g_pePawn + o2, 4)) break;
            uint8_t* c = *(uint8_t**)(g_pePawn + o2);
            if (!LooksLikeObj(c)) continue;
            const char* cc = ObjClassName(c);
            if (!cc) continue;
            if (!strstr(cc, "Power") && !strstr(cc, "Blink") &&
                !strstr(cc, "Aim")   && !strstr(cc, "Target") &&
                !strstr(cc, "Reach") && !strstr(cc, "Traversal")) continue;
            BpAdd(c, 0, "pawn component");
        }
    }
    for (int pass = 0; pass < 2 && g_bpN < kBpMax; pass++) {
        for (uint32_t i = 1; i < num && g_bpN < kBpMax; i++) {
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
            const char* cn = ObjClassName(o);
            if (!cn) continue;
            bool want = pass == 0
                ? (strstr(cn, "Blink") != NULL)
                : (strstr(cn, "Teleport") || strstr(cn, "Reach") ||
                   strstr(cn, "Traversal") || strstr(cn, "Marker"));
            if (!want) continue;
            if (strstr(cn, "SeqAct")) continue;         // Kismet nodes, not state
            if (!BpIsLive(o)) continue;                 // template, not an instance
            BpAdd(o, i, pass == 0 ? "live Blink object" : "live teleport object");
        }
    }
    if (!g_bpN) {
        Log("blink: nothing live to watch. Stand in gameplay with Blink equipped");
        Log("blink:   and arm this again.");
        return;
    }
    g_bpGo = true;
    g_bpUntil = MaimNowMs() + 30000.0;
    g_bpNextSample = 0.0;
    g_bpNextTell = MaimNowMs() + 5000.0;
    Log("blink: ==== %d LIVE candidate(s). WINDOW OPEN FOR 30 s ====", g_bpN);
    Log("blink:   HOLD the Blink aim and sweep it around - near, far, up, down.");
    Log("blink:   A progress line every 5 s says whether anything is tracking.");
}


static int BpBestNow(float* dotOut)          // for the live progress line
{
    int n = 0; float best = 0.0f;
    for (int i = 0; i < g_bpN; i++)
        for (int k = 0; k < kBpOffN; k++) {
            if (g_bpMaxMove[i][k] < 10.0f) continue;
            if (g_bpHits[i][k] < 5) continue;
            n++;
            if (g_bpBestDot[i][k] > best) best = g_bpBestDot[i][k];
        }
    if (dotOut) *dotOut = best;
    return n;
}


static void BlinkProbeSample()
{
    if (!g_bpGo) return;
    double now = MaimNowMs();
    if (now > g_bpUntil) { g_bpGo = false; g_bpReq = 2; return; }
    if (now < g_bpNextSample) return;
    g_bpNextSample = now + 50.0;                       // 20 Hz

    if (!CamStillValid() || !RangeReadable(g_camObj + 0x50, 0x40)) return;
    const float* cf = (const float*)(g_camObj + 0x50);  // view forward
    const float* cp = (const float*)(g_camObj + 0x80);  // camera world pos

    for (int i = 0; i < g_bpN; i++) {
        if (!BpAlive(i) || !g_bpLimit[i]) continue;
        uint8_t* o = g_bpObj[i];
        int kmax = (int)(g_bpLimit[i] / 4);
        if (kmax > kBpOffN) kmax = kBpOffN;
        for (int k = 0; k < kmax; k++) {
            const float* v = (const float*)(o + 0x20 + (uint32_t)k * 4);
            bool sane = true;
            for (int c = 0; c < 3; c++) {
                float a = v[c] < 0 ? -v[c] : v[c];
                if (v[c] != v[c] || a > 1.0e6f) { sane = false; break; }
            }
            if (!sane) continue;
            if (!g_bpHave[i][k]) { g_bpHave[i][k] = true; memcpy(g_bpFirst[i][k], v, 12); }
            float d[3] = { v[0] - g_bpFirst[i][k][0],
                           v[1] - g_bpFirst[i][k][1],
                           v[2] - g_bpFirst[i][k][2] };
            float mv = sqrtf(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
            if (mv > g_bpMaxMove[i][k]) g_bpMaxMove[i][k] = mv;

            float r[3] = { v[0] - cp[0], v[1] - cp[1], v[2] - cp[2] };
            float dist = sqrtf(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
            if (dist < 20.0f || dist > 6000.0f) continue;
            float dot = (r[0]*cf[0] + r[1]*cf[1] + r[2]*cf[2]) / dist;
            g_bpDotSum[i][k]  += dot;
            g_bpDistSum[i][k] += dist;
            g_bpSamples[i][k]++;
            if (dot > g_bpBestDot[i][k]) g_bpBestDot[i][k] = dot;
            if (dot > 0.97f) g_bpHits[i][k]++;
        }
    }
    if (now >= g_bpNextTell) {
        g_bpNextTell = now + 5000.0;
        float best = 0.0f;
        int n = BpBestNow(&best);
        Log("blink: %.0f s left - %d field(s) moving AND on the view ray "
            "(best dot %.3f), %d event(s) seen", (g_bpUntil - now) / 1000.0,
            n, best, g_bpEvtN);
        // 32.13: and NAME them, every 5 s, not only in the final report. The
        // last run locked on at dot=1.000 and then told us nothing, because
        // the user quit 17 s into a 30 s window and the report never ran. A
        // diagnostic that only pays out if the operator waits for the timer is
        // a diagnostic that throws away good runs. Now any 5 seconds of
        // holding the aim is enough.
        int shown = 0;
        for (int i = 0; i < g_bpN && shown < 3; i++)
            for (int k = 0; k < kBpOffN && shown < 3; k++) {
                if (g_bpHits[i][k] < 5 || g_bpMaxMove[i][k] < 10.0f) continue;
                Log("blink:     %-40s +0x%03x  hits=%-5u best=%.3f  dist=%.0fuu",
                    g_bpLabel[i], (unsigned)(0x20 + k * 4), g_bpHits[i][k],
                    g_bpBestDot[i][k],
                    g_bpDistSum[i][k] / (float)(g_bpSamples[i][k] ? g_bpSamples[i][k] : 1));
                shown++;
            }
    }
}


static void BlinkProbeReport()
{
    Log("blink: ==== WINDOW CLOSED - results ====");
    if (g_bpEvtN) {
        Log("blink: script events seen on the player/power objects:");
        for (int e = 0; e < g_bpEvtN; e++) {
            const char* nm = RealName(g_bpEvt[e]);
            Log("blink:   %-44s x%u", nm ? nm : "?", g_bpEvtC[e]);
        }
    } else {
        Log("blink: no aim-shaped script events - Blink's trace is likely native.");
    }
    // 32.11: rank by HITS (samples essentially dead on the ray), and ALWAYS
    // print the top rows even when none clear the bar. The 32.10 report said
    // "NOTHING" while its own progress line had already reported dot=0.931 a
    // few seconds earlier; a verdict that hides its runners-up throws away the
    // run. There is no such thing as a null result worth printing alone.
    struct Hit { int obj; int k; uint32_t hits; float best; float dist; float move; };
    Hit top[16]; int tn = 0;
    for (int i = 0; i < g_bpN; i++)
        for (int k = 0; k < kBpOffN; k++) {
            if (g_bpMaxMove[i][k] < 10.0f) continue;      // never moved
            if (!g_bpSamples[i][k]) continue;
            Hit h;
            h.obj = i; h.k = k; h.hits = g_bpHits[i][k];
            h.best = g_bpBestDot[i][k];
            h.dist = g_bpDistSum[i][k] / (float)g_bpSamples[i][k];
            h.move = g_bpMaxMove[i][k];
            if (!h.hits && h.best < 0.80f) continue;      // not even close
            int p = tn;
            while (p > 0 && (top[p-1].hits < h.hits ||
                            (top[p-1].hits == h.hits && top[p-1].best < h.best))) {
                if (p < 16) top[p] = top[p-1];
                p--;
            }
            if (p < 16) { top[p] = h; if (tn < 16) tn++; }
        }
    if (!tn) {
        Log("blink: not one field on ANY of those objects moved and pointed");
        Log("blink:   down the view ray, even loosely. If the progress lines all");
        Log("blink:   read 0, the aim was never held and this run says nothing.");
        Log("blink:   If they read >0, tell me - that means my ranking is wrong,");
        Log("blink:   not the game.");
    } else {
        Log("blink: fields that MOVE and point down the view ray, best first.");
        Log("blink:   hits = samples dead on the ray (20/s). A real aim target");
        Log("blink:   scores hundreds of hits with best=1.000.");
        for (int q = 0; q < tn; q++) {
            Log("blink:   %-44s +0x%03x  hits=%-5u best=%.3f  dist=%.0fuu  moved=%.0fuu",
                g_bpLabel[top[q].obj], (unsigned)(0x20 + top[q].k * 4),
                top[q].hits, top[q].best, top[q].dist, top[q].move);
        }
        if (top[0].hits >= 20)
            Log("blink: STRONG - +0x%03x on %s is the aim target. Next build "
                "writes it.", (unsigned)(0x20 + top[0].k * 4), g_bpLabel[top[0].obj]);
        else
            Log("blink: WEAK - nothing locked for even a second. Hold the aim "
                "longer and steadier, or the target is not on these objects.");
    }
    Log("blink: ==== end ====");
}
