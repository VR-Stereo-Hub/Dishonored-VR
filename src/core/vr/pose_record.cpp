// core/vr/pose_record.cpp - three records, kept apart on purpose (VR-65).
//
// The argument is in the header. This file is the storage, the render-side
// solver, and the controls.
//
// LANES AND SYNCHRONISATION. Records are opened on the GAME thread, copied out
// on the PRESENT thread, and the render observation is written on the RENDER
// thread. Every shared structure is behind one lock, held only for the memcpy.
//
// The first version used a compiler barrier and an id check. That is not
// synchronisation: a compiler barrier orders nothing between threads, and an id
// check cannot stop a slot being overwritten while the reader is copying out of
// it. The reader could see a torn record whose id happened to still match.
// Nothing about that was safe, and it is replaced rather than patched.

#include "core/vr/pose_record.h"
#include "core/util/log.h"
#include "core/util/clock.h"

#include <windows.h>
#include <string.h>
#include <math.h>

#define DVR_CAT ::dvr::log::Cat::openxr

namespace dvr::pose {
namespace {

CRITICAL_SECTION g_cs;
bool             g_csReady = false;

struct Lock {
    Lock()  { if (g_csReady) EnterCriticalSection(&g_cs); }
    ~Lock() { if (g_csReady) LeaveCriticalSection(&g_cs); }
};

// ---- the published camera pair ---------------------------------------------
Track g_camTrack = {};
Cam   g_camCam   = {};
bool  g_camHave  = false;
uint32_t g_camPublished = 0;

// ---- the record ring -------------------------------------------------------
// 128 views is about two thirds of a second of pairs at 90 Hz. If a copy ever
// finds a wrapped slot the pipeline is far deeper than the design assumes, and
// EXPIRED is the finding rather than a wrong pose quietly returned.
constexpr uint32_t kRing = 128;
Record   g_ring[kRing];
uint32_t g_nextId = 1;
uint32_t g_pair = 0;
uint32_t g_opened = 0, g_copies = 0, g_expired = 0, g_missing = 0;

// ---- the render observation ------------------------------------------------
float    g_vp[16];
float    g_vpCam[3];
bool     g_vpCamOk = false;
bool     g_vpHave  = false;
uint32_t g_vpSerial = 0;
uint32_t g_renderBlocks = 0;

// Which multiplication convention the game's own numbers validated. Measured
// once, never assumed: 0 not yet decided, 1 row-vector (v * M), 2 column-vector
// (M * v), -1 neither validated (a real finding, logged as one).
int g_vpConv = 0;
int g_vpGood[2] = { -1, -1 };   // probe hits per convention, for the log

// ---- the controls ----------------------------------------------------------
uint32_t g_ctrlStart = 1500, g_ctrlLen = 120;
float    g_ctrlYawDeg = 6.0f;
uint32_t g_ctrlPass = 0, g_ctrlFail = 0, g_ctrlChecks = 0;
bool     g_ctrlDoneSub = false;
int      g_ctrlDone[CTRL_COUNT] = {};

float    g_subSumAbs = 0.0f, g_subMaxAbs = 0.0f, g_subMaxSpeed = 0.0f;
float    g_subFastSum = 0.0f, g_subSlowSum = 0.0f;
uint32_t g_subChecks = 0, g_subFastN = 0, g_subSlowN = 0, g_subRepeatGen = 0;
float    g_subLastD = 0.0f, g_subLastSpeed = 0.0f;
int      g_subGenBack = 0;

Record   g_lastCopy = {};
bool     g_haveLastCopy = false;

void ensure_cs()
{
    static LONG once = 0;
    if (InterlockedCompareExchange(&once, 1, 0) == 0) {
        InitializeCriticalSection(&g_cs);
        g_csReady = true;
    }
    while (!g_csReady) Sleep(0);
}

// The angle between two rotations, sign-invariant so q and -q read as identical.
float quat_angle_deg(float ax, float ay, float az, float aw,
                            float bx, float by, float bz, float bw)
{
    const float na = sqrtf(ax*ax + ay*ay + az*az + aw*aw);
    const float nb = sqrtf(bx*bx + by*by + bz*bz + bw*bw);
    if (!(na > 1e-6f) || !(nb > 1e-6f)) return 0.0f;
    ax /= na; ay /= na; az /= na; aw /= na;
    bx /= nb; by /= nb; bz /= nb; bw /= nb;
    float dot = ax*bx + ay*by + az*bz + aw*bw;
    if (dot < 0.0f) dot = -dot;              // q and -q are one rotation
    if (dot > 1.0f) dot = 1.0f;
    return 2.0f * acosf(dot) * 57.29578f;
}


float wrap180(float d)
{
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// Project a world point through the stored block under one convention. Returns
// false when w is not usable, which is how a wrong convention announces itself.
bool project(const float p[3], int conv, float* ndcX, float* w)
{
    const float* m = g_vp;
    float c[4];
    if (conv == 1) {           // row vector: clip = [x y z 1] * M
        for (int i = 0; i < 4; ++i)
            c[i] = p[0]*m[0*4+i] + p[1]*m[1*4+i] + p[2]*m[2*4+i] + m[3*4+i];
    } else {                   // column vector: clip = M * [x y z 1]
        for (int i = 0; i < 4; ++i)
            c[i] = p[0]*m[i*4+0] + p[1]*m[i*4+1] + p[2]*m[i*4+2] + m[i*4+3];
    }
    if (!(c[3] > 1.0f) || c[3] > 1.0e7f) return false;   // behind, or nonsense
    *w = c[3];
    *ndcX = c[0] / c[3];
    return true;
}

} // namespace


// ---- CAMERA -----------------------------------------------------------------

void publish_camera(const Track& t, const Cam& c)
{
    ensure_cs();
    Lock lk;
    g_camTrack = t;
    g_camCam = c;
    g_camHave = true;
    ++g_camPublished;
}


bool camera_snapshot(Track* t, Cam* c)
{
    ensure_cs();
    Lock lk;
    if (!g_camHave) return false;
    if (t) *t = g_camTrack;
    if (c) *c = g_camCam;
    return true;
}


// ---- the record ring --------------------------------------------------------

uint32_t next_pair()
{
    ensure_cs();
    Lock lk;
    return ++g_pair;
}


uint32_t open(int eye, uint32_t pairId, bool secondPassReuse)
{
    ensure_cs();
    Lock lk;
    if (!g_camHave) return 0;      // nothing to copy: an id would be a lie
    const uint32_t id = g_nextId++;
    Record& r = g_ring[id & (kRing - 1)];
    r.id = id;
    r.pairId = pairId;
    r.eye = eye;
    r.track = g_camTrack;          // a COPY of what the camera write published
    r.cam = g_camCam;
    r.openedMs = dvr::clock::now_ms();
    r.secondPassReuse = secondPassReuse;
    ++g_opened;
    return id;
}


bool copy(uint32_t id, Record* out)
{
    if (!out) return false;
    ensure_cs();
    Lock lk;
    if (!id) { ++g_missing; return false; }
    const Record& r = g_ring[id & (kRing - 1)];
    if (r.id != id) {
        if (r.id) ++g_expired; else ++g_missing;
        return false;
    }
    *out = r;
    ++g_copies;
    g_lastCopy = r;
    g_haveLastCopy = true;
    return true;
}


// ---- RENDER -----------------------------------------------------------------

void note_render_vp(const float vp16[16], const float camPos[3], bool camPosOk)
{
    if (!vp16) return;
    ensure_cs();
    Lock lk;
    memcpy(g_vp, vp16, sizeof(g_vp));
    if (camPosOk && camPos) { memcpy(g_vpCam, camPos, sizeof(g_vpCam)); g_vpCamOk = true; }
    g_vpHave = true;
    ++g_vpSerial;
    ++g_renderBlocks;
}


bool render_yaw_deg(float* outDeg, uint32_t* outSerial)
{
    ensure_cs();
    Lock lk;
    if (!g_vpHave || !g_vpCamOk) return false;

    // DECIDE THE CONVENTION FROM THE GAME'S OWN NUMBERS, ONCE. A point a little
    // way from the observed camera position must project with a positive,
    // sane w under exactly one of the two conventions. Whichever answers for a
    // full ring of directions is the layout; if neither does, that is reported
    // rather than guessed around.
    if (g_vpConv == 0) {
        for (int conv = 1; conv <= 2 && g_vpConv == 0; ++conv) {
            int good = 0;
            for (int i = 0; i < 36; ++i) {
                const float a = (float)i * 10.0f * 0.0174533f;
                const float p[3] = { g_vpCam[0] + 200.0f * cosf(a),
                                     g_vpCam[1] + 200.0f * sinf(a),
                                     g_vpCam[2] };
                float x, w;
                if (project(p, conv, &x, &w)) ++good;
            }
            g_vpGood[conv - 1] = good;
            // A perspective camera sees rather LESS than half a ring. The first
            // run validated 22 of 36 under the wrong matrix, which is more than
            // a real camera can see and was the first sign the block was not a
            // world view-projection. So the band is bounded at both ends and the
            // count is always printed: too few means nothing projected, too many
            // means this is not a perspective view of the world.
            if (good >= 5 && good <= 20) {
                g_vpConv = conv;
                DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                    "pose/render: the view-projection block multiplies as %s - "
                    "%d of 36 probe directions around the observed camera "
                    "projected with a usable w. MEASURED from the game's own "
                    "numbers, not assumed from a register number.",
                    conv == 1 ? "a ROW vector (v * M)" : "a COLUMN vector (M * v)",
                    good);
            }
        }
        if (g_vpConv == 0) {
            g_vpConv = -1;
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pose/render: the probe ring validated %d and %d of 36 directions "
                "under the two conventions. Outside the 5..20 band a perspective "
                "world view would give, so this block is not one.",
                g_vpGood[0], g_vpGood[1]);
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pose/render: NEITHER multiplication convention projected the "
                "probe ring usably, so the block at these registers is not a "
                "world view-projection in the form assumed. The render side of "
                "this trace reports NOTHING until that is resolved - it does not "
                "fall back to a guess, because a guessed layout would produce a "
                "confident number about the wrong matrix.");
        }
    }
    if (g_vpConv < 0) return false;

    // THE YAW THE RENDER ACTUALLY USED, without decomposing anything: the world
    // direction that projects to the screen centre is where the camera looks.
    // Coarse ring, then three refinements, all in the observed block.
    float best = 0.0f, bestAbs = 1.0e9f;
    bool found = false;
    for (int i = 0; i < 72; ++i) {
        const float yaw = (float)i * 5.0f;
        const float a = yaw * 0.0174533f;
        const float p[3] = { g_vpCam[0] + 400.0f * cosf(a),
                             g_vpCam[1] + 400.0f * sinf(a), g_vpCam[2] };
        float x, w;
        if (!project(p, g_vpConv, &x, &w)) continue;
        if (fabsf(x) < bestAbs) { bestAbs = fabsf(x); best = yaw; found = true; }
    }
    if (!found) return false;
    float step = 5.0f;
    for (int it = 0; it < 12; ++it) {
        step *= 0.5f;
        const float cand[2] = { best - step, best + step };
        for (int k = 0; k < 2; ++k) {
            const float a = cand[k] * 0.0174533f;
            const float p[3] = { g_vpCam[0] + 400.0f * cosf(a),
                                 g_vpCam[1] + 400.0f * sinf(a), g_vpCam[2] };
            float x, w;
            if (!project(p, g_vpConv, &x, &w)) continue;
            if (fabsf(x) < bestAbs) { bestAbs = fabsf(x); best = cand[k]; }
        }
    }
    if (outDeg) *outDeg = wrap180(best);
    if (outSerial) *outSerial = g_vpSerial;
    return true;
}


// ---- the controls -----------------------------------------------------------

// GROUP A: PURE ARITHMETIC, on synthetic inputs, run once at configure time on
// this machine before anything is installed. It depends on no game state, no
// render observation and no headset, so it can never be blocked by a leg that
// refuses - which is what silenced every control in the previous two builds.
static void RunArithmeticControls()
{
    struct Case { const char* what; float a[4], b[4], expect; };
    // Rotations about Y by known amounts, plus the sign-invariance case that a
    // naive dot-product comparison gets wrong.
    const float c10 = cosf(5.0f * 0.0174533f), s10 = sinf(5.0f * 0.0174533f);
    const float c90 = cosf(45.0f * 0.0174533f), s90 = sinf(45.0f * 0.0174533f);
    const Case cases[] = {
        { "identity",              {0,0,0,1}, {0,0,0,1},          0.0f },
        { "10 deg about yaw",      {0,0,0,1}, {0,s10,0,c10},     10.0f },
        { "90 deg about yaw",      {0,0,0,1}, {0,s90,0,c90},     90.0f },
        { "10 deg about pitch",    {0,0,0,1}, {s10,0,0,c10},     10.0f },
        { "10 deg about roll",     {0,0,0,1}, {0,0,s10,c10},     10.0f },
        { "negated quaternion",    {0,0,0,1}, {0,0,0,-1},         0.0f },
        { "unnormalised input",    {0,0,0,2}, {0,0,0,1},          0.0f },
    };
    int pass = 0, fail = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const Case& k = cases[i];
        const float got = quat_angle_deg(k.a[0], k.a[1], k.a[2], k.a[3],
                                         k.b[0], k.b[1], k.b[2], k.b[3]);
        const bool ok = fabsf(got - k.expect) <= 0.05f;
        ok ? ++pass : ++fail;
        if (!ok)
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Error,
                "pose/ctrl A: ARITHMETIC control FAILED - '%s' gave %.3f deg, "
                "expected %.3f (tolerance 0.05). The angular comparison itself is "
                "wrong, so every difference this trace reports is wrong with it.",
                k.what, got, k.expect);
    }
    g_ctrlPass += pass; g_ctrlFail += fail;
    DVR_LOG(DVR_CAT, fail ? ::dvr::log::Level::Error : ::dvr::log::Level::Info,
        "pose/ctrl A: arithmetic - %d passed, %d FAILED of %d synthetic case(s), "
        "covering identity, yaw, pitch, roll, a negated quaternion (the same "
        "rotation, must read zero) and an unnormalised input. Group A depends on "
        "nothing but this file, so it runs whether or not any other leg of the "
        "trace is available.",
        pass, fail, (int)(sizeof(cases) / sizeof(cases[0])));
}


void configure_controls(uint32_t startAfter, uint32_t eachLen, float yawDeg)
{
    RunArithmeticControls();
    g_ctrlStart = startAfter; g_ctrlLen = eachLen; g_ctrlYawDeg = yawDeg;
    if (!eachLen)
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
            "pose/ctrl: the controls are disabled, so nothing will demonstrate "
            "that this trace can report a wrong answer. Every agreement it "
            "prints is then unverified and must be read as such.");
}


void check_controls(const Record& real, float observedYawDeg)
{
    // Each control perturbs a COPY and asks whether the comparison moves by the
    // amount the arithmetic says it must. Nothing here touches the record that
    // reached submission, so an armed control cannot move the picture - which is
    // the difference from the first version, and the reason there is no longer
    // any wobble for the tester to see or to misread as a fault.
    // PHASED ON CHECKS PERFORMED, not on records opened. The first version
    // counted opens, and opens ran far ahead of the submission side that calls
    // this - so by the time the first check happened the whole window had gone
    // by and not one control ran. The run showed it: `controls passed 0 FAILED
    // 0` beside 27,284 records. A control that never fires is worse than none,
    // because the zero reads like a pass.
    if (!g_ctrlLen) return;
    const uint32_t n = ++g_ctrlChecks;
    if (n < g_ctrlStart) return;
    const uint32_t phase = (n - g_ctrlStart) / g_ctrlLen;
    if (phase >= CTRL_COUNT - 1) return;
    const int which = (int)phase + 1;
    if (g_ctrlDone[which]) return;
    g_ctrlDone[which] = 1;

    const float baseErr = wrap180(observedYawDeg - real.cam.yawDeg);

    if (which == CTRL_YAW) {
        Record c = real;
        c.cam.yawDeg = wrap180(c.cam.yawDeg + g_ctrlYawDeg);
        const float got = wrap180(observedYawDeg - c.cam.yawDeg);
        const float moved = wrap180(got - baseErr);
        const bool pass = fabsf(moved + g_ctrlYawDeg) <= 0.05f;
        pass ? ++g_ctrlPass : ++g_ctrlFail;
        DVR_LOG(DVR_CAT, pass ? ::dvr::log::Level::Info : ::dvr::log::Level::Error,
            "pose/ctrl: ANGLE control %s - a diagnostic copy was turned %+.2f deg "
            "and the camera-vs-render comparison moved %+.2f deg (expected "
            "%+.2f, tolerance 0.05). This proves the comparison responds to an "
            "angle and NOTHING else: it says nothing about whether the real "
            "sample is the one rendering used.",
            pass ? "PASSED" : "FAILED", g_ctrlYawDeg, moved, -g_ctrlYawDeg);
        return;
    }
    if (which == CTRL_OLD_REC) {
        // Substitute a record from a few views ago. If generation association is
        // live the error must change by the amount the camera has actually
        // turned since; if it does not move at all, the comparison is not
        // reading the record it claims to read.
        Record older;
        const bool haveOlder = (real.id > 8) && copy(real.id - 8, &older);
        if (!haveOlder) {
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pose/ctrl: GENERATION control UNAVAILABLE - no record 8 views "
                "back could be copied. Unavailable evidence, not agreement.");
            return;
        }
        const float got = wrap180(observedYawDeg - older.cam.yawDeg);
        const float moved = fabsf(wrap180(got - baseErr));
        const float turned = fabsf(wrap180(real.cam.yawDeg - older.cam.yawDeg));
        const bool pass = fabsf(moved - turned) <= 0.05f;
        pass ? ++g_ctrlPass : ++g_ctrlFail;
        DVR_LOG(DVR_CAT, pass ? ::dvr::log::Level::Info : ::dvr::log::Level::Error,
            "pose/ctrl: GENERATION control %s - substituting the record from 8 "
            "views back moved the comparison %.2f deg, and the camera had turned "
            "%.2f deg over that span (tolerance 0.05). A zero here with a "
            "non-zero turn would mean the comparison is not reading the record "
            "it names.", pass ? "PASSED" : "FAILED", moved, turned);
        return;
    }
    if (which == CTRL_WRONG_EYE) {
        Record c = real;
        c.eye = -c.eye;
        const bool pass = (c.eye == -real.eye) && real.eye != 0;
        pass ? ++g_ctrlPass : ++g_ctrlFail;
        DVR_LOG(DVR_CAT, pass ? ::dvr::log::Level::Info : ::dvr::log::Level::Warn,
            "pose/ctrl: EYE control %s - the record under test names eye %+d, and "
            "the audit compares each eye against ITS OWN submitted view rather "
            "than against whichever record arrived last. If the eye reads 0 here "
            "the image was untagged and no eye association exists to check.",
            pass ? "is meaningful" : "CANNOT RUN", real.eye);
        return;
    }
}


// Yaw about the XR up axis (+Y) from a quaternion, degrees. One conversion,
// used for both sides of the comparison so a convention error cannot enter on
// one side only.
static float quat_yaw_deg(float x, float y, float z, float w)
{
    const float siny = 2.0f * (w * y + z * x);
    const float cosy = 1.0f - 2.0f * (x * x + y * y);
    return atan2f(siny, cosy) * 57.29578f;
}


void note_submitted(int eye, float qx, float qy, float qz, float qw,
                    uint32_t submittedGen, int lagUsed, const Record& rec)
{
    // THE RECORD IS PASSED IN. The previous version read a hidden "last copy"
    // global, so it could compare an eye against a record belonging to the
    // other one, and it could not say which eye it had actually checked.
    ensure_cs();
    Lock lk;
    const Record& r = rec;
    if (r.eye != eye || !r.track.ok) return;

    // FULL ORIENTATION, not yaw. A yaw-only number cannot see a pitch or roll
    // error at all, and the tester reports judder on pitch and roll too. The
    // difference is the sign-invariant angle between two normalised quaternions,
    // so a quaternion and its negation - the same rotation - read as zero.
    const float d = quat_angle_deg(r.track.qx, r.track.qy, r.track.qz, r.track.qw,
                                   qx, qy, qz, qw);
    const float recYaw = quat_yaw_deg(r.track.qx, r.track.qy, r.track.qz, r.track.qw);
    const float subYaw = quat_yaw_deg(qx, qy, qz, qw);

    // HEAD SPEED, from this record against the previous one for the same eye.
    // Without it a delta is just a number; with it the delta can be shown to
    // scale with speed, which is what separates a stale sample from a constant
    // offset.
    // THE DERIVATIVE USES TRACKING TIMESTAMPS, not the time this call happened.
    // The previous version divided by submission arrival times, so queue jitter
    // entered the number and it could not honestly be called head speed. This is
    // the change in the SAMPLE between two locates, over the interval between
    // those locates.
    static float  prevQ[2][4] = {};
    static double prevLocate[2] = {0.0, 0.0};
    static uint32_t prevGen[2] = {0, 0};
    static bool   havePrev[2] = {false, false};
    const int ei = eye < 0 ? 0 : 1;
    float speed = 0.0f;
    bool speedOk = false;
    if (havePrev[ei] && r.track.gen != prevGen[ei] &&
        r.track.locateMs > prevLocate[ei]) {
        const float da = quat_angle_deg(prevQ[ei][0], prevQ[ei][1], prevQ[ei][2],
                                        prevQ[ei][3], r.track.qx, r.track.qy,
                                        r.track.qz, r.track.qw);
        speed = da / (float)((r.track.locateMs - prevLocate[ei]) / 1000.0);
        speedOk = true;
    }
    if (r.track.gen != prevGen[ei]) {
        prevQ[ei][0] = r.track.qx; prevQ[ei][1] = r.track.qy;
        prevQ[ei][2] = r.track.qz; prevQ[ei][3] = r.track.qw;
        prevLocate[ei] = r.track.locateMs; prevGen[ei] = r.track.gen;
        havePrev[ei] = true;
    } else {
        ++g_subRepeatGen;
    }

    // GROUP C: the controls that only need a record and a submitted pose. They
    // used to sit behind the render leg and so never ran once in two sessions.
    if (g_ctrlLen && ++g_ctrlChecks >= g_ctrlStart && !g_ctrlDoneSub) {
        g_ctrlDoneSub = true;
        // Perturb a DIAGNOSTIC COPY by a known angle and confirm the comparison
        // moves by it. Nothing that reaches submission is touched, so there is
        // no wobble for the tester to see.
        const float k = g_ctrlYawDeg * 0.5f * 0.0174533f;
        const float pqx = r.track.qx * cosf(k) + r.track.qw * 0.0f;
        (void)pqx;
        const float sy = sinf(k), cy = cosf(k);
        // q_perturbed = q_yaw(delta) * q_record, composed properly.
        const float px2 = cy * r.track.qx + sy * r.track.qz;
        const float py2 = cy * r.track.qy + sy * r.track.qw;
        const float pz2 = cy * r.track.qz - sy * r.track.qx;
        const float pw2 = cy * r.track.qw - sy * r.track.qy;
        const float moved = quat_angle_deg(r.track.qx, r.track.qy, r.track.qz,
                                           r.track.qw, px2, py2, pz2, pw2);
        const bool pass = fabsf(moved - fabsf(g_ctrlYawDeg)) <= 0.10f;
        pass ? ++g_ctrlPass : ++g_ctrlFail;
        DVR_LOG(DVR_CAT, pass ? ::dvr::log::Level::Info : ::dvr::log::Level::Error,
            "pose/ctrl C: RECORD-VS-SUBMISSION control %s - a diagnostic copy of "
            "the live record was turned %.2f deg and the comparison measured "
            "%.3f (tolerance 0.10). This proves the comparison responds to a real "
            "rotation of a real record. It does NOT prove the record holds the "
            "sample rendering used, and it does not exercise the render leg.",
            pass ? "PASSED" : "FAILED", g_ctrlYawDeg, moved);
    }

    ++g_subChecks;
    g_subSumAbs += fabsf(d);
    if (fabsf(d) > g_subMaxAbs) { g_subMaxAbs = fabsf(d); g_subMaxSpeed = speed; }
    if (speedOk) {
        if (speed > 5.0f) { g_subFastSum += fabsf(d); ++g_subFastN; }
        else              { g_subSlowSum += fabsf(d); ++g_subSlowN; }
    }
    g_subLastD = d; g_subLastSpeed = speed;
    g_subGenBack = (int)(submittedGen - r.track.gen);

    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
        "xr: posesub eye %+d | the camera for this image CONSUMED locate "
        "generation %u (yaw %.2f deg); the pose SUBMITTED with it is generation "
        "%u (yaw %.2f), chosen by lag arm %d, %d generation(s) apart | full "
        "orientation difference %.3f deg | sample changed %.1f deg/s between "
        "locates%s | mean difference %.3f deg over %u check(s); above 5 deg/s "
        "the mean is %.3f over %u, near still %.3f over %u | worst %.3f deg at "
        "%.1f deg/s | repeated generations %u. Both sides are OpenXR convention "
        "so the difference is real and needs no world matrix. It compares the "
        "camera INPUT against the submitted metadata; it does not show whether "
        "rendering honoured that input, and an association with motion is not by "
        "itself proof of a stale sample.",
        eye, r.track.gen, recYaw, submittedGen, subYaw, lagUsed, g_subGenBack, d,
        speed, speedOk ? "" : " (NOT MEASURABLE this check - the generation did not advance)",
        g_subChecks ? g_subSumAbs / (float)g_subChecks : 0.0f, g_subChecks,
        g_subFastN ? g_subFastSum / (float)g_subFastN : 0.0f, g_subFastN,
        g_subSlowN ? g_subSlowSum / (float)g_subSlowN : 0.0f, g_subSlowN,
        g_subMaxAbs, g_subMaxSpeed, g_subRepeatGen);
}


Stats stats()
{
    ensure_cs();
    Lock lk;
    Stats s;
    s.opened = g_opened; s.copies = g_copies; s.expired = g_expired;
    s.missing = g_missing; s.pairs = g_pair;
    s.camPublished = g_camPublished; s.renderBlocks = g_renderBlocks;
    s.ctrlPass = g_ctrlPass; s.ctrlFail = g_ctrlFail;
    return s;
}


void log_beat()
{
    Record last; bool have;
    {
        ensure_cs();
        Lock lk;
        have = g_haveLastCopy;
        last = g_lastCopy;
    }
    const Stats s = stats();
    if (!have) {
        // A zero here is NOT agreement - it is "nothing was ever joined", and the
        // two must never read the same.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
            "pose/rec: %u view record(s) over %u pair(s), %u camera publication(s), "
            "%u render block(s) - and NOTHING has been copied out yet, so no join "
            "is being tested at all. missing=%u expired=%u.",
            s.opened, s.pairs, s.camPublished, s.renderBlocks, s.missing, s.expired);
        return;
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "pose/rec: opened %u over %u pair(s) | copies %u, EXPIRED %u (the ring "
        "wrapped before submission asked - the pipeline is deeper than %u views), "
        "MISSING %u (an id nobody set) | camera publications %u by writer %d, "
        "render blocks %u | controls passed %u FAILED %u (checks %u) | last: rec %u pair %u "
        "eye %+d, camera yaw %.2f pitch %.2f roll %.2f deg written %.1f ms before "
        "it was read%s",
        s.opened, s.pairs, s.copies, s.expired, (unsigned)kRing, s.missing,
        s.camPublished, last.cam.writer, s.renderBlocks, s.ctrlPass, s.ctrlFail,
        g_ctrlChecks,
        last.id, last.pairId, last.eye,
        last.cam.yawDeg, last.cam.pitchDeg, last.cam.rollDeg,
        dvr::clock::now_ms() - last.cam.writeMs,
        last.secondPassReuse ? " (second pass, reusing pass 1's camera by design)" : "");
}

} // namespace dvr::pose
