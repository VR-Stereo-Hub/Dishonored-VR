// game/dishonored/z_account.cpp - see z_account.h.
#include "game/dishonored/z_account.h"

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef DVR_ZACCT_HOST
// The host harness (tools/zaccount-host.ps1) compiles this file on its own.
#include <stdarg.h>
extern void ZacctHostLine(const char* line);
static void ZaLine(bool warn, const char* fmt, ...) {
    char b[1400];
    va_list ap; va_start(ap, fmt); _vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    b[sizeof(b) - 1] = 0;
    (void)warn;
    ZacctHostLine(b);
}
#define ZA_INFO(...) ZaLine(false, __VA_ARGS__)
#define ZA_WARN(...) ZaLine(true, __VA_ARGS__)
#else
#define DVR_CAT ::dvr::log::Cat::head
#include "core/util/log.h"
#define ZA_INFO(...) DVR_INFO(__VA_ARGS__)
#define ZA_WARN(...) DVR_WARN(__VA_ARGS__)
#endif

namespace dvr::zacct {
namespace {

// ---- thresholds, stated once -------------------------------------------------------
constexpr int    kPin = 32;               // pinned writes awaiting their present
constexpr float  kDownLo = -40.0f, kDownHi = -20.0f;   // DOWN bucket, degrees of camera pitch
constexpr float  kLevelAbs = 6.0f;                       // LEVEL bucket
constexpr float  kUpLo = 20.0f, kUpHi = 40.0f;           // UP bucket
constexpr double kSettleMs = 500.0;       // after a bucket, stance or motion change
constexpr double kHeadMaxAgeMs = 100.0;   // head snapshot older than this at the write: stale
constexpr double kClampMaxAgeMs = 200.0;  // clamp record older than this at the write: stale
constexpr double kCylMaxAgeMs = 1500.0;   // the clamp's own cylinder freshness rule
constexpr float  kStandMin = 76.0f, kCrouchMin = 50.0f;  // stance from the capsule
constexpr float  kRollMax = 12.0f;        // degrees (PITCH mode only: see roll mode below)
// VR-91 roll mode. The head ROLL bins, and the pitch window the samples must sit
// in so that pitch is not a second variable in a lateral measurement.
constexpr float  kRollLeftLo = -45.0f, kRollLeftHi = -15.0f;
constexpr float  kRollRightLo = 15.0f, kRollRightHi = 45.0f;
constexpr float  kRollLevelAbs = 5.0f;
constexpr float  kRollPitchAbs = 20.0f;   // camera pitch must be within this of level
// Below this the lateral residual is not a fault whatever its significance: 2 uu
// is under 2 cm at the shipped world scale, and the instrument exists to find
// something a person can see.
constexpr double kLatFlatUu = 2.0;
constexpr float  kYawRateMax = 25.0f;     // degrees per second
constexpr float  kPawnSpeedMax = 20.0f;   // uu per second
constexpr float  kTeleportUu = 60.0f;
constexpr int    kMinBucket = 60;         // distinct matched samples per bucket
constexpr float  kTarget = 1.0f;          // provisional residual / closure / noise target, uu
constexpr float  kEqTol = 0.05f;          // the writer equation is exact arithmetic
constexpr double kProgressMs = 10000.0;

enum Reason {
    R_MISSING, R_EXPIRED, R_EYE, R_OVERRIDE, R_NOC5, R_STALEC5, R_TAGPOS, R_NOWRITE, R_POSE, R_TORN,
    R_CLAMP, R_CYL, R_LANE, R_BASIS, R_TEST, R_TRANSITION, R_ROLL, R_TURNING, R_MOVING, R_TELEPORT,
    R_CAMERA, R_SETTLING, R_BETWEEN, R_COUNT
};
const char* kReasonName[R_COUNT] = {
    "missing", "expired", "eye", "override", "noC5", "staleC5", "tagPos", "noWrite", "pose", "torn",
    "clamp", "cyl", "lane", "basis", "test", "transition", "roll", "turning", "moving", "teleport",
    "camera", "settling", "between",
};
const char* kBucketName[3] = {"DOWN", "LEVEL", "UP"};
const char* kRollName[3] = {"ROLL_LEFT", "ROLL_LEVEL", "ROLL_RIGHT"};
const char* kStanceName[3] = {"STANCE_UNKNOWN", "STANDING", "CROUCHED"};

struct Acc {
    double n = 0, s = 0, ss = 0;
    void   add(double v) { n += 1; s += v; ss += v * v; }
    double mean() const { return n > 0 ? s / n : 0.0; }
    double sd() const {
        if (n < 2) return 0.0;
        const double m = s / n, v = ss / n - m * m;
        return v > 0 ? sqrt(v * n / (n - 1)) : 0.0;
    }
};

struct Bucket {
    Acc resUp, resFwd, headPitch, camPitch, closure;
    Acc fieldPre, lever, base, raw, neck, cap, render;
    // VR-91, lateral accounting. All along the yaw-only right axis the WRITE
    // used (Write::prAxis), in world uu, positive to the player's right:
    //   latRender  the rendered camera's lateral offset from the pawn
    //   latEye     the eye term's lateral contribution (opposite between eyes)
    //   latPos     the position term's lateral contribution
    //   latHead    the tracked head's own lateral displacement, as published
    //   latRes     latRender - latEye - latHead: what moved sideways that the
    //              head did not do and the eye separation does not explain
    //   latNeck    the NECK term's own lateral contribution, as supplied
    // latRes deliberately does NOT subtract latEye: an eye-offset fault has to
    // stay visible somewhere, and it shows in the eye column, not the residual.
    Acc roll, latRender, latEye, latPos, latHead, latNeck, latRes;
    uint32_t clipped = 0, capped = 0, persisted = 0, noClampField = 0;
    float closureMax = 0.0f, eqMax = 0.0f;
    int n() const { return (int)resUp.n; }
};

struct Fit {
    double ata[4][4] = {}, atb[4] = {}, bb = 0;
    uint32_t n = 0, recovered = 0;
    float pmin = 1e9f, pmax = -1e9f;
};

struct Episode {
    uint32_t index = 0;
    int      stance = 0;             // 1 standing, 2 crouched
    double   startMs = 0.0, lastMs = 0.0;
    Bucket   b[3][2];                // [bucket][eye: 0 left, 1 right]
    Fit      fit;
    uint32_t tagged = 0, matched = 0, accepted = 0;
    uint32_t rej[R_COUNT] = {};
    uint32_t lastProgressAccepted = 0;
    bool     any = false;
};

CRITICAL_SECTION g_cs;
volatile LONG    g_csInit = 0;
volatile LONG    g_on = 0;
const char*      g_source = "compiled default";

void ensure_cs() {
    if (InterlockedCompareExchange(&g_csInit, 1, 0) == 0) InitializeCriticalSection(&g_cs);
}
struct Lock {
    Lock() { ensure_cs(); EnterCriticalSection(&g_cs); }
    ~Lock() { LeaveCriticalSection(&g_cs); }
};

// present thread -> script lane
Head g_head;
bool g_headOk = false;
// script lane only
Clamp    g_clamp;
bool     g_clampOk = false;
Write    g_lastWrite;
bool     g_lastWriteOk = false;
// pinned (locked)
Write    g_pin[kPin];
uint32_t g_nextId = 1;
uint32_t g_pinned = 0, g_pinRefused = 0;
// consumer state (locked)
Episode  g_ep;
uint32_t g_episodes = 0;
uint32_t g_lastC5Serial = 0;
bool     g_lastC5SerialOk = false;
int      g_key = -1;                 // stance*10 + bucket (bucket 3 = between)
bool     g_rollMode = false;         // VR-91: bin by roll, measure laterally
double   g_keySince = 0.0;
bool     g_prevOk = false;
float    g_prevPawn[3] = {0, 0, 0};
float    g_prevYaw = 0.0f;
double   g_prevMs = 0.0;
uint8_t* g_prevCam = nullptr;
double   g_nextProgressMs = 0.0;
uint32_t g_orphans = 0;              // presents seen with no open episode to charge

// ---- VR-80 pair trace (locked, except the two volatiles) ------------------------------
constexpr int    kTraceRing = 16;         // presents kept for an override's look-back
constexpr int    kTraceAfterArm = 24;     // presents printed after a return to gameplay
constexpr int    kTraceBack = 8, kTraceAhead = 8;   // around an override
constexpr double kTraceOverrideGapMs = 2000.0;
constexpr uint32_t kTraceMaxDumps = 40;
struct TraceRec {
    uint32_t n = 0;                       // present counter, trace-local
    int      ring = 0, fin = 0;
    bool     tagged = false;
    bool     haveW = false, p2 = false, wrote = false, tagPosOk = true, sameWrite = false;
    int      weye = 0;
    uint32_t wseq = 0;
    const char* skip = "";
    double   wAgeMs = 0.0;
    bool     c5Ok = false, stepOk = false, tagC5Ok = false;
    float    c5R = 0.0f, stepR = 0.0f, tagC5 = 0.0f;
    char     note[96] = {0};
};
volatile LONG g_trOn = 0;
volatile LONG g_trArmReq = 0;
const char*   g_trArmWhy = "";
TraceRec g_tr[kTraceRing];
int      g_trAt = 0, g_trHave = 0, g_trLeft = 0;
uint32_t g_trN = 0, g_trDumps = 0, g_trPrevSeq = 0;
float    g_trBasis[3] = {0, 0, 0};
bool     g_trBasisOk = false, g_trPrevC5Ok = false;
float    g_trPrevC5R = 0.0f;
double   g_trNextOverrideMs = 0.0;
char     g_trNote[96] = {0};

void trace_print(const TraceRec& r) {
    char w[160], c[64], s[32], t[40];
    if (r.haveW)
        _snprintf(w, sizeof(w), "write seq %u eye %+d%s%s%s%s age %.1f ms", r.wseq, r.weye,
                  r.p2 ? " P2" : "", r.wrote ? "" : " NOWRITE:", r.wrote ? "" : r.skip,
                  r.sameWrite ? " SAME-WRITE" : "", r.wAgeMs);
    else
        _snprintf(w, sizeof(w), "write %s", r.tagged ? "NONE PINNED" : "- (untagged)");
    w[sizeof(w) - 1] = 0;
    if (r.c5Ok) _snprintf(c, sizeof(c), "%.2f", r.c5R); else _snprintf(c, sizeof(c), "?");
    if (r.stepOk) _snprintf(s, sizeof(s), "%+.2f", r.stepR); else _snprintf(s, sizeof(s), "?");
    if (r.tagC5Ok) _snprintf(t, sizeof(t), "%.2f uu%s", r.tagC5, r.tagPosOk ? "" : " TAGPOS-MISMATCH");
    else _snprintf(t, sizeof(t), "?");
    c[sizeof(c) - 1] = 0; s[sizeof(s) - 1] = 0; t[sizeof(t) - 1] = 0;
    ZA_INFO("vr80/trace: #%u ring %+d final %+d%s | %s | c5 along right %s step %s | write-to-c5 %s%s%s",
            r.n, r.ring, r.fin, (r.tagged && r.ring != r.fin) ? " OVERRIDE" : "", w, c, s, t,
            r.note[0] ? " | " : "", r.note);
}

void trace_header(const char* why) {
    ++g_trDumps;
    ZA_INFO("vr80/trace: DUMP %u/%u (%s). One line per present. Healthy stereo alternates ring -1/+1 with "
            "final equal to ring; a -1 carries a write with eye -1 and no P2, a +1 a P2 write. A ring -1 whose "
            "write is P2 or SAME-WRITE drew from the previous pass 2's camera (the WRITER is off); an OVERRIDE "
            "whose writes are clean means the RING is off. c5 is the negated position, so still stereo steps "
            "by one eye separation with alternating sign.", g_trDumps, kTraceMaxDumps, why);
}

void trace_present(int ringEye, int finalEye, bool tagged, uint32_t id, bool haveC5, const float c5[3],
                   double nowMs) {
    TraceRec r;
    r.n = ++g_trN;
    r.ring = tagged ? ringEye : 0;
    r.fin = tagged ? finalEye : 0;
    r.tagged = tagged;
    memcpy(r.note, g_trNote, sizeof(r.note));
    if (tagged && id) {
        const Write& p = g_pin[id & (kPin - 1)];
        if (p.id == id) {
            r.haveW = true; r.weye = p.eye; r.p2 = p.secondPass; r.wrote = p.wrote; r.skip = p.skip ? p.skip : "";
            r.wseq = p.seq; r.wAgeMs = nowMs - p.ms; r.tagPosOk = p.tagPosMatch;
            r.sameWrite = g_trPrevSeq != 0 && p.seq == g_trPrevSeq;
            g_trPrevSeq = p.seq;
            if (p.wrote && haveC5 && c5) {
                float d2 = 0.0f;
                for (int i = 0; i < 3; ++i) { const float d = c5[i] - p.written[i] * p.c5Sign; d2 += d * d; }
                r.tagC5 = sqrtf(d2); r.tagC5Ok = true;
            }
        }
    }
    if (haveC5 && c5 && g_trBasisOk) {
        r.c5R = c5[0] * g_trBasis[0] + c5[1] * g_trBasis[1] + c5[2] * g_trBasis[2];
        r.c5Ok = true;
        if (g_trPrevC5Ok) { r.stepR = r.c5R - g_trPrevC5R; r.stepOk = true; }
        g_trPrevC5R = r.c5R; g_trPrevC5Ok = true;
    }
    if (InterlockedExchange(&g_trArmReq, 0) && g_trDumps < kTraceMaxDumps) {
        trace_header(g_trArmWhy);
        g_trLeft = kTraceAfterArm;
    }
    const bool ovr = tagged && ringEye != 0 && finalEye != ringEye;
    if (ovr && g_trLeft == 0 && nowMs >= g_trNextOverrideMs && g_trDumps < kTraceMaxDumps) {
        g_trNextOverrideMs = nowMs + kTraceOverrideGapMs;
        trace_header("the pairing overrode the ring; the presents before it, then after");
        const int back = g_trHave < kTraceBack ? g_trHave : kTraceBack;
        for (int k = back; k >= 1; --k) trace_print(g_tr[(g_trAt - k + kTraceRing) % kTraceRing]);
        g_trLeft = kTraceAhead;
    }
    g_tr[g_trAt] = r;
    g_trAt = (g_trAt + 1) % kTraceRing;
    if (g_trHave < kTraceRing) ++g_trHave;
    if (g_trLeft > 0) {
        trace_print(r);
        if (--g_trLeft == 0) ZA_INFO("vr80/trace: window closed (%u of %u dumps used)", g_trDumps, kTraceMaxDumps);
    }
}

// VR-91. Returns 3 for "between", the same contract as bucket_of.
int roll_bucket_of(float rollDeg) {
    if (rollDeg >= kRollLeftLo && rollDeg <= kRollLeftHi) return 0;
    if (fabsf(rollDeg) <= kRollLevelAbs) return 1;
    if (rollDeg >= kRollRightLo && rollDeg <= kRollRightHi) return 2;
    return 3;
}

int bucket_of(float pitchDeg) {
    if (pitchDeg >= kDownLo && pitchDeg <= kDownHi) return 0;
    if (fabsf(pitchDeg) <= kLevelAbs) return 1;
    if (pitchDeg >= kUpLo && pitchDeg <= kUpHi) return 2;
    return 3;
}

int stance_of(float cyl) {
    if (cyl > kStandMin) return 1;
    if (cyl > kCrouchMin) return 2;
    return 0;
}

float wrap180(float d) {
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

bool solve4(double a[4][4], double b[4], double x[4]) {
    double m[4][5];
    for (int i = 0; i < 4; ++i) { for (int j = 0; j < 4; ++j) m[i][j] = a[i][j]; m[i][4] = b[i]; }
    for (int c = 0; c < 4; ++c) {
        int p = c;
        for (int r = c + 1; r < 4; ++r) if (fabs(m[r][c]) > fabs(m[p][c])) p = r;
        if (fabs(m[p][c]) < 1e-9) return false;
        if (p != c) for (int j = 0; j < 5; ++j) { const double t = m[c][j]; m[c][j] = m[p][j]; m[p][j] = t; }
        for (int r = 0; r < 4; ++r) {
            if (r == c) continue;
            const double f = m[r][c] / m[c][c];
            for (int j = c; j < 5; ++j) m[r][j] -= f * m[c][j];
        }
    }
    for (int i = 0; i < 4; ++i) x[i] = m[i][4] / m[i][i];
    return true;
}

// The engine pivot from FRESH bases: base_rel = neutral + arc(theta), with
//   up(th)  = nU + below*(cos th - 1) + behind*sin th
//   fwd(th) = nF + behind*(cos th - 1) - below*sin th
// (the pitchtest's model, per SAMPLE, never trig of a bucket mean).
void fit_add(Fit& f, float thetaDeg, float up, float fwd) {
    const double th = thetaDeg * 0.017453292519943295, c1 = cos(th) - 1.0, s = sin(th);
    const double rows[2][4] = {{1, 0, c1, s}, {0, 1, -s, c1}};
    const double ys[2] = {up, fwd};
    for (int k = 0; k < 2; ++k) {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) f.ata[i][j] += rows[k][i] * rows[k][j];
            f.atb[i] += rows[k][i] * ys[k];
        }
        f.bb += ys[k] * ys[k];
    }
    ++f.n;
    if (thetaDeg < f.pmin) f.pmin = thetaDeg;
    if (thetaDeg > f.pmax) f.pmax = thetaDeg;
}

void reset_consumer() {
    g_key = -1; g_keySince = 0.0; g_prevOk = false; g_prevCam = nullptr;
}

double level_se(const Acc& a, const Acc& b) {
    const double sa = a.sd(), sb = b.sd();
    return sqrt((a.n > 0 ? sa * sa / a.n : 0.0) + (b.n > 0 ? sb * sb / b.n : 0.0));
}

// Print one episode. Locked by the caller.
void report(const char* why, bool full) {
    Episode& e = g_ep;
    if (!e.any) return;
    const double secs = (e.lastMs - e.startMs) * 0.001;
    char rej[700] = "";
    for (int i = 0; i < R_COUNT; ++i) {
        char one[32];
        _snprintf(one, sizeof(one), " %s=%lu", kReasonName[i], (unsigned long)e.rej[i]);
        one[sizeof(one) - 1] = 0;
        strncat(rej, one, sizeof(rej) - strlen(rej) - 1);
    }
    // flags
    bool complete = true, closureBad = false, eqBad = false, clipped = false, persisted = false;
    bool residual = false, noisy = false;
    for (int bi = 0; bi < 3; ++bi)
        for (int ei = 0; ei < 2; ++ei) {
            const Bucket& b = e.b[bi][ei];
            if (b.n() < kMinBucket) complete = false;
            if (b.n() > 0 && b.closure.mean() > kTarget) closureBad = true;
            if (b.eqMax > kEqTol) eqBad = true;
            if (b.clipped || b.capped) clipped = true;
            if (b.persisted) persisted = true;
        }
    if (complete)
        for (int ei = 0; ei < 2; ++ei)
            for (int bi = 0; bi < 3; bi += 2) {
                const Bucket& b = e.b[bi][ei];
                const Bucket& l = e.b[1][ei];
                const double dU = b.resUp.mean() - l.resUp.mean(), dF = b.resFwd.mean() - l.resFwd.mean();
                const double sU = level_se(b.resUp, l.resUp), sF = level_se(b.resFwd, l.resFwd);
                if (sU > kTarget || sF > kTarget) noisy = true;
                else if (fabs(dU) > kTarget || fabs(dF) > kTarget) residual = true;
            }
    const bool unmatched = e.tagged > 0 && (e.tagged - e.matched) * 20u > e.tagged;   // > 5 %
    char flags[240] = "";
    auto flag = [&](bool on, const char* name) {
        if (!on) return;
        if (flags[0]) strncat(flags, " ", sizeof(flags) - strlen(flags) - 1);
        strncat(flags, name, sizeof(flags) - strlen(flags) - 1);
    };
    flag(!complete, "INCOMPLETE");
    flag(unmatched, "UNMATCHED");
    flag(closureBad, "CLOSURE");
    flag(eqBad, "EQUATION");
    flag(clipped, "CLIPPED");
    flag(persisted, "PERSISTENT_BASE");
    flag(complete && noisy, "INCONCLUSIVE");
    flag(complete && !noisy && residual, "PITCH_RESIDUAL");
    flag(complete && !noisy && !residual && !closureBad && !eqBad, "NO_MEASURED_CAMERA_RESIDUAL");

    // the pivot fit
    char fitLine[400];
    double x[4] = {0, 0, 0, 0};
    const Fit& f = e.fit;
    double a[4][4]; double bv[4];
    memcpy(a, f.ata, sizeof(a)); memcpy(bv, f.atb, sizeof(bv));
    const bool solved = f.n >= (uint32_t)kMinBucket && (f.pmax - f.pmin) >= 30.0f && solve4(a, bv, x);
    float cfgBelow = 0.0f, cfgBehind = 0.0f, scale = 100.0f;
    if (g_headOk) { cfgBelow = g_head.neckBelowM; cfgBehind = g_head.neckBehindM; if (g_head.scale > 1.0f) scale = g_head.scale; }
    if (!f.n) {
        _snprintf(fitLine, sizeof(fitLine), "NO_ENGINE_SAMPLES: every base in this stance was clipped by the clamp (or the "
                  "clamp reconciled our own write), so no unclipped engine eye was seen to fit");
    } else if (!solved) {
        _snprintf(fitLine, sizeof(fitLine), "LOW_VARIANCE: %lu engine samples (%lu recovered) over pitch %+.0f..%+.0f deg (needs "
                  "%d and a 30 deg span) - not fitted", (unsigned long)f.n, (unsigned long)f.recovered, f.pmin, f.pmax, kMinBucket);
    } else {
        double q = f.bb;
        for (int i = 0; i < 4; ++i) { q -= 2.0 * x[i] * f.atb[i]; for (int j = 0; j < 4; ++j) q += x[i] * f.ata[i][j] * x[j]; }
        const double rms = sqrt(q > 0 ? q / (2.0 * f.n) : 0.0);
        const double below = x[2] / scale, behind = x[3] / scale;
        const bool mismatch = rms < 2.0 && (fabs(below - cfgBelow) > 0.03 || fabs(behind - cfgBehind) > 0.03);
        _snprintf(fitLine, sizeof(fitLine), "%s: %lu engine samples (%lu recovered), pitch %+.0f..%+.0f deg: below %.3f m behind "
                  "%.3f m (neck in use %.3f/%.3f), neutral U%+.1f F%+.1f uu, rms %.2f uu (%s)",
                  mismatch ? "PIVOT_MISMATCH" : rms < 2.0 ? "PIVOT_MATCHES" : "PIVOT_NO_FIT", (unsigned long)f.n,
                  (unsigned long)f.recovered, f.pmin, f.pmax,
                  below, behind, cfgBelow, cfgBehind, x[0], x[1], rms,
                  rms < 2.0 ? "a rigid pivot fits" : "a rigid pivot does NOT fit: the arc is not a rotation about one point");
    }
    fitLine[sizeof(fitLine) - 1] = 0;

    ZA_INFO("zaccount: episode #%lu %s %s %.1f s (%s) | tagged presents %lu, matched %lu, accepted %lu | flags: %s | rejected:%s",
            (unsigned long)e.index, kStanceName[e.stance], full ? "REPORT" : "progress", secs, why, (unsigned long)e.tagged,
            (unsigned long)e.matched, (unsigned long)e.accepted, flags[0] ? flags : "none", rej);
    if (g_rollMode) {
        // VR-91. Everything is along the yaw-only right axis the write used,
        // world uu, positive to the player's right.
        //
        // latRes is the number: what moved sideways that the tracked head did not
        // do and the eye separation does not explain. Zero at every roll is the
        // healthy answer, and this line can print it - which is what makes a
        // non-zero reading evidence rather than a foregone conclusion.
        for (int bi = 0; bi < 3; ++bi)
            for (int ei = 0; ei < 2; ++ei) {
                const Bucket& b = e.b[bi][ei];
                const Bucket& l = e.b[1][ei];
                const char eyeC = ei ? 'R' : 'L';
                if (!full) continue;
                if (b.n() == 0) {
                    ZA_INFO("zaccount/roll: #%lu %s %s %c n=0 (no accepted sample: see the rejected counts)",
                            (unsigned long)e.index, kStanceName[e.stance], kRollName[bi], eyeC);
                    continue;
                }
                const bool lOk = l.n() > 0;
                ZA_INFO("zaccount/roll: #%lu %s %s %c n=%d roll %+.1f deg pitch %+.1f | LATERAL uu (+ = player's "
                        "right): render %+.2f, eye term %+.2f, position term %+.2f, tracked head %+.2f, neck "
                        "term %+.2f | RESIDUAL %+.2f (sd %.2f)%s | closure mean %.2f max %.2f",
                        (unsigned long)e.index, kStanceName[e.stance], kRollName[bi], eyeC, b.n(),
                        b.roll.mean(), b.camPitch.mean(), b.latRender.mean(), b.latEye.mean(),
                        b.latPos.mean(), b.latHead.mean(), b.latNeck.mean(), b.latRes.mean(), b.latRes.sd(),
                        (bi == 1 || !lOk) ? " (absolute)" : "", b.closure.mean(), b.closureMax);
                if (bi != 1 && lOk) {
                    const double dRes = b.latRes.mean() - l.latRes.mean();
                    const double dNeck = b.latNeck.mean() - l.latNeck.mean();
                    const double dEye = b.latEye.mean() - l.latEye.mean();
                    const double dHead = b.latHead.mean() - l.latHead.mean();
                    const double dRoll = b.roll.mean() - l.roll.mean();
                    const double se = level_se(b.latRes, l.latRes);
                    // Name the owner before the verdict, and say what each reading
                    // would mean - including the one that clears us.
                    // VR-91: SIGNIFICANCE IS NOT A FAULT. With 2000 samples a bin
                    // the standard error is 0.02 uu, so a purely statistical
                    // threshold calls a 0.5 uu residual - five millimetres, which
                    // nobody can see - a fault, and the fixed run duly reported an
                    // owner for one. The bound is therefore the larger of the
                    // statistical one and a perceptual floor.
                    const char* owner =
                        fabs(dRes) <= (2.0 * se + 0.05 > kLatFlatUu ? 2.0 * se + 0.05 : kLatFlatUu)
                            ? "FLAT: our writes move the camera sideways no more than the head did. If the "
                              "fault is still visible it is not in this composition - look between the "
                              "submitted view poses and the rendered world"
                        : fabs(dRes - dNeck) <= 0.15 * (fabs(dRes) > 1.0 ? fabs(dRes) : 1.0)
                            ? "THE NECK TERM OWNS IT: the residual equals the neck arc's own lateral "
                              "contribution. The arc is built from the head matrix's up row, which rolls, "
                              "against a yaw-only reference that does not, and [Neck] Mode=cancel negates "
                              "the difference - which is why it moves OPPOSITE to the head"
                        : fabs(dHead) > 0.5 * fabs(dRes)
                            ? "THE TRACKED HEAD: most of the residual is already in the published head "
                              "lateral, so the fault is upstream of this composition, in the pose"
                            : "UNACCOUNTED: the residual is not the neck term and not the head. Suspect the "
                              "position term's axis or the eye field's sign";
                    ZA_INFO("zaccount/roll: #%lu %s %s %c vs ROLL_LEVEL, over %+.1f deg of roll: residual "
                            "%+.2f (se %.2f), neck term %+.2f, eye term %+.2f, position term %+.2f, tracked "
                            "head %+.2f uu | %s. Separately: an eye term that changes with roll is the eye "
                            "offset riding the camera's rolled right row, and it will NOT appear in the "
                            "residual - the residual does not subtract it.",
                            (unsigned long)e.index, kStanceName[e.stance], kRollName[bi], eyeC, dRoll,
                            dRes, se, dNeck, dEye, b.latPos.mean() - l.latPos.mean(), dHead, owner);
                }
            }
        ZA_INFO("zaccount/roll: #%lu %s end of table. The pitch fit is not run in roll mode.",
                (unsigned long)e.index, kStanceName[e.stance]);
        return;
    }
    for (int bi = 0; bi < 3; ++bi)
        for (int ei = 0; ei < 2; ++ei) {
            const Bucket& b = e.b[bi][ei];
            const Bucket& l = e.b[1][ei];
            const char eyeC = ei ? 'R' : 'L';
            if (!full) continue;
            if (b.n() == 0) {
                ZA_INFO("zaccount: #%lu %s %s %c n=0 (no accepted sample: see the rejected counts)", (unsigned long)e.index,
                        kStanceName[e.stance], kBucketName[bi], eyeC);
                continue;
            }
            if (bi == 1) {
                ZA_INFO("zaccount: #%lu %s LEVEL %c n=%d pitch head %+.1f cam %+.1f | residual up %+.2f (sd %.2f) fwd %+.2f (sd %.2f) "
                        "uu = the reference | closure mean %.2f max %.2f, writer equation max %.3f | terms: fieldBeforeClamp %+.2f "
                        "lever %+.2f base %+.2f raw %+.2f neck %+.2f cap %+.2f render %+.2f (Z uu rel pawn, up) | clamp clipped %lu, "
                        "cap %lu, persisted base %lu, no clamp field %lu",
                        (unsigned long)e.index, kStanceName[e.stance], eyeC, b.n(), b.headPitch.mean(), b.camPitch.mean(),
                        b.resUp.mean(), b.resUp.sd(), b.resFwd.mean(), b.resFwd.sd(), b.closure.mean(), b.closureMax, b.eqMax,
                        b.fieldPre.mean(), b.lever.mean(), b.base.mean(), b.raw.mean(), b.neck.mean(), b.cap.mean(),
                        b.render.mean(), (unsigned long)b.clipped, (unsigned long)b.capped, (unsigned long)b.persisted,
                        (unsigned long)b.noClampField);
                continue;
            }
            const bool lOk = l.n() > 0;
            ZA_INFO("zaccount: #%lu %s %s %c n=%d pitch head %+.1f cam %+.1f | vs LEVEL: residual up %+.2f (se %.2f) fwd %+.2f "
                    "(se %.2f) uu%s | closure mean %.2f max %.2f, writer equation max %.3f | terms vs LEVEL: fieldBeforeClamp "
                    "%+.2f lever %+.2f base %+.2f raw %+.2f neck %+.2f cap %+.2f render %+.2f | clamp clipped %lu, cap %lu, "
                    "persisted base %lu, no clamp field %lu",
                    (unsigned long)e.index, kStanceName[e.stance], kBucketName[bi], eyeC, b.n(), b.headPitch.mean(),
                    b.camPitch.mean(), lOk ? b.resUp.mean() - l.resUp.mean() : 0.0, lOk ? level_se(b.resUp, l.resUp) : 0.0,
                    lOk ? b.resFwd.mean() - l.resFwd.mean() : 0.0, lOk ? level_se(b.resFwd, l.resFwd) : 0.0,
                    lOk ? "" : " (NO LEVEL bucket: these are absolute, not relative)", b.closure.mean(), b.closureMax, b.eqMax,
                    b.fieldPre.mean() - (lOk ? l.fieldPre.mean() : 0.0), b.lever.mean() - (lOk ? l.lever.mean() : 0.0),
                    b.base.mean() - (lOk ? l.base.mean() : 0.0), b.raw.mean() - (lOk ? l.raw.mean() : 0.0),
                    b.neck.mean() - (lOk ? l.neck.mean() : 0.0), b.cap.mean() - (lOk ? l.cap.mean() : 0.0),
                    b.render.mean() - (lOk ? l.render.mean() : 0.0), (unsigned long)b.clipped, (unsigned long)b.capped,
                    (unsigned long)b.persisted, (unsigned long)b.noClampField);
        }
    ZA_INFO("zaccount: #%lu %s engine pivot: %s", (unsigned long)e.index, kStanceName[e.stance], fitLine);
}

void close_episode(const char* why) {
    if (g_ep.any) report(why, true);
    g_ep = Episode();
}

void open_episode(int stance, double ms) {
    g_ep = Episode();
    g_ep.index = ++g_episodes;
    g_ep.stance = stance;
    g_ep.startMs = g_ep.lastMs = ms;
    g_ep.any = false;
}

void reject(Reason r) { ++g_ep.rej[r]; }

// The consumer proper. Locked by the caller.
void consume(const Write& w, int finalEye, bool haveC5, const float c5[3], uint32_t c5Serial) {
    // Every check below happens BEFORE a value is used; the order is the order of trust.
    if (!haveC5) { reject(R_NOC5); return; }
    if (g_lastC5SerialOk && c5Serial == g_lastC5Serial) { reject(R_STALEC5); return; }
    g_lastC5Serial = c5Serial; g_lastC5SerialOk = true;
    if (w.eye != finalEye) { reject(R_EYE); return; }
    if (!w.tagPosMatch) { reject(R_TAGPOS); return; }
    if (!w.wrote) { reject(R_NOWRITE); return; }
    if (!w.headOk || !w.head.ok) { reject(R_POSE); return; }
    if (w.torn) { reject(R_TORN); return; }
    if (w.ms - w.head.ms > kHeadMaxAgeMs) { reject(R_POSE); return; }
    if (!w.clampOk || w.ms - w.clamp.ms > kClampMaxAgeMs) { reject(R_CLAMP); return; }
    if (!w.clamp.ran || w.clamp.cylAgeMs > kCylMaxAgeMs) { reject(R_CYL); return; }
    if (!w.projection || !w.laneCamera || !w.head.posTrack || !w.head.projection) { reject(R_LANE); return; }
    if (!w.basisOk || w.posDropped) { reject(R_BASIS); return; }
    if (w.otherTest) { reject(R_TEST); return; }
    ++g_ep.matched;

    const int stance = stance_of(w.clamp.cyl);
    if (stance == 0) { reject(R_TRANSITION); g_key = -1; return; }
    if (stance != g_ep.stance) {
        if (g_ep.stance != 0) {
            close_episode("stance changed");
            reset_consumer();
            open_episode(stance, w.ms);
            g_ep.tagged = 1; g_ep.matched = 1;   // this present opens the new episode
        } else {
            if (!g_ep.index) g_ep.index = ++g_episodes;
            g_ep.stance = stance;
            g_ep.startMs = w.ms;
        }
    }
    g_ep.lastMs = w.ms;
    g_ep.any = true;
    // VR-91: in pitch mode roll is noise and is rejected. In ROLL mode it is the
    // signal, and this very rejection is why the existing probe could say nothing
    // about a roll fault - it threw away every sample that carried one.
    if (!g_rollMode && fabsf(w.head.rollDeg) > kRollMax) { reject(R_ROLL); g_key = -1; return; }
    if (g_rollMode && !w.prAxisOk) { reject(R_BASIS); g_key = -1; return; }

    // motion, across writes at least 5 ms apart (pass 1 and pass 2 share a tick)
    if (g_prevOk && w.cam != g_prevCam) { reject(R_CAMERA); reset_consumer(); }
    if (g_prevOk) {
        const double dt = w.ms - g_prevMs;
        const float dx = w.clamp.pawn[0] - g_prevPawn[0], dy = w.clamp.pawn[1] - g_prevPawn[1];
        const float dxy = sqrtf(dx * dx + dy * dy);
        if (dxy > kTeleportUu) { reject(R_TELEPORT); reset_consumer(); }
        else if (dt >= 5.0) {
            const float speed = (float)(dxy / dt * 1000.0);
            const float yawRate = (float)(fabsf(wrap180(w.head.yawDeg - g_prevYaw)) / dt * 1000.0);
            g_prevPawn[0] = w.clamp.pawn[0]; g_prevPawn[1] = w.clamp.pawn[1]; g_prevPawn[2] = w.clamp.pawn[2];
            g_prevYaw = w.head.yawDeg; g_prevMs = w.ms;
            if (yawRate > kYawRateMax) { reject(R_TURNING); g_keySince = w.ms; return; }
            if (speed > kPawnSpeedMax) { reject(R_MOVING); g_keySince = w.ms; return; }
        }
    }
    if (!g_prevOk) {
        g_prevOk = true; g_prevCam = w.cam; g_prevMs = w.ms; g_prevYaw = w.head.yawDeg;
        memcpy(g_prevPawn, w.clamp.pawn, sizeof(g_prevPawn));
    }
    g_prevCam = w.cam;

    // the fresh-engine pivot sample: the field was NOT our write, and the clamp did not touch it
    int ci = -1;
    for (int i = 0; i < kClampFields; ++i)
        if (w.clamp.readable[i] && w.clamp.fieldOff[i] == w.fieldOff) { ci = i; break; }
    const bool leverClipped = ci >= 0 && w.clamp.post[ci] < w.clamp.pre[ci] - 0.001f;
    const float relB[3] = {w.sign * w.base[0] - w.clamp.pawn[0], w.sign * w.base[1] - w.clamp.pawn[1],
                           w.sign * w.base[2] - w.clamp.pawn[2]};
    // A RECOVERED base (the field still held our write) is admitted too: the
    // writer re-bases on every script dispatch, so the tick's last call always
    // finds the earlier call's write, and its base is still the engine value that
    // call saw. Launch 1 (2026-09-12) had 100 % recovered bases in both stances
    // and a fit that refused them never ran. The line counts the two apart.
    if (ci >= 0 && !leverClipped && !w.clamp.ours[ci]) {
        fit_add(g_ep.fit, w.camPitchDeg, relB[2], relB[0] * w.heading[0] + relB[1] * w.heading[1]);
        if (w.persisted) ++g_ep.fit.recovered;
    }

    // VR-91: roll mode bins by head ROLL and holds pitch near level, so that a
    // lateral measurement is not also a pitch measurement.
    const int bucket = g_rollMode ? roll_bucket_of(w.head.rollDeg) : bucket_of(w.camPitchDeg);
    const int key = stance * 10 + bucket;
    if (key != g_key) { g_key = key; g_keySince = w.ms; }
    if (bucket == 3) { reject(R_BETWEEN); return; }
    if (g_rollMode && fabsf(w.camPitchDeg) > kRollPitchAbs) { reject(R_BETWEEN); return; }
    if (w.ms - g_keySince < kSettleMs) { reject(R_SETTLING); return; }

    // the measurement
    const float render[3] = {c5[0] * w.c5Sign, c5[1] * w.c5Sign, c5[2] * w.c5Sign};
    const float dc[3] = {render[0] - w.written[0], render[1] - w.written[1], render[2] - w.written[2]};
    const float closure = sqrtf(dc[0] * dc[0] + dc[1] * dc[1] + dc[2] * dc[2]);
    const float eq = fabsf(w.written[2] - (w.sign * w.base[2] + w.eyeW[2] + w.posW[2] + w.capDelta));
    const float rel[3] = {render[0] - w.clamp.pawn[0] - w.eyeW[0], render[1] - w.clamp.pawn[1] - w.eyeW[1],
                          render[2] - w.clamp.pawn[2] - w.eyeW[2]};
    const float up = rel[2], fwd = rel[0] * w.heading[0] + rel[1] * w.heading[1];
    Bucket& b = g_ep.b[bucket][w.eye > 0 ? 1 : 0];
    b.resUp.add(up - w.head.raw[1]);
    b.resFwd.add(fwd - w.head.raw[2]);
    b.headPitch.add(w.head.pitchDeg);
    b.camPitch.add(w.camPitchDeg);
    b.closure.add(closure);
    if (closure > b.closureMax) b.closureMax = closure;
    if (eq > b.eqMax) b.eqMax = eq;
    if (ci >= 0) {
        b.fieldPre.add(w.clamp.pre[ci] - w.clamp.pawn[2]);
        b.lever.add(w.clamp.post[ci] - w.clamp.pre[ci]);
        if (leverClipped) ++b.clipped;
    } else {
        ++b.noClampField;
    }
    b.base.add(relB[2]);
    b.raw.add(w.head.raw[1]);
    b.neck.add(w.head.neck[1]);
    b.cap.add(w.capDelta);
    if (w.capDelta < -0.001f) ++b.capped;
    if (w.persisted) ++b.persisted;
    b.render.add(up);
    // VR-91: the lateral accounting, along the axis the WRITE used.
    if (w.prAxisOk) {
        const float pr0 = w.prAxis[0], pr1 = w.prAxis[1];
        const float latRender = (render[0] - w.clamp.pawn[0]) * pr0 + (render[1] - w.clamp.pawn[1]) * pr1;
        const float latEye = w.eyeW[0] * pr0 + w.eyeW[1] * pr1;
        const float latPos = w.posW[0] * pr0 + w.posW[1] * pr1;
        b.roll.add(w.head.rollDeg);
        b.latRender.add(latRender);
        b.latEye.add(latEye);
        b.latPos.add(latPos);
        b.latHead.add(w.head.raw[0]);
        // The neck arc's lateral term, named separately because it is the one
        // suspect that can be read out of the composition rather than inferred:
        // the arc is built from the head matrix's UP row, which rotates with
        // roll, against a yaw-only reference that does not - and [Neck]
        // Mode=cancel then NEGATES the difference. If latRes equals latNeck the
        // neck owns the whole fault; if it does not, something else does too.
        b.latNeck.add(w.head.neck[0]);
        // What moved sideways that the head did not do and the eye separation
        // does not explain. Zero is the healthy answer at every roll.
        b.latRes.add(latRender - latEye - w.head.raw[0]);
    }
    ++g_ep.accepted;
}

} // namespace

double now_ms() {
    static LARGE_INTEGER freq = {};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

// ---- the lever -----------------------------------------------------------------
void set_enabled(bool on, const char* source) {
    const bool was = InterlockedExchange(&g_on, on ? 1 : 0) != 0;
    g_source = source ? source : "?";
    if (on && !was) {
        reset("armed");
        ZA_INFO("zaccount: ON (%s) - every draw's camera write is pinned to its eye tag and joined to that present's c5; "
                "per stance episode, buckets DOWN %.0f..%.0f / LEVEL +-%.0f / UP %.0f..%.0f deg of CAMERA pitch after %.0f ms "
                "settle, %d samples each; progress every %.0f s, the full report when the stance changes, on `camera "
                "zaccount off|reset` and at game exit. Hold still: moving, turning and roll are rejected and counted.",
                source ? source : "?", kDownLo, kDownHi, kLevelAbs, kUpLo, kUpHi, kSettleMs, kMinBucket, kProgressMs / 1000.0);
    } else if (!on && was) {
        flush("switched off");
        ZA_INFO("zaccount: off (%s)", source ? source : "?");
    }
}

bool enabled() { return InterlockedCompareExchange(&g_on, 0, 0) != 0; }

// ---- VR-80 pair trace lever ------------------------------------------------------
void set_trace(bool on, const char* source) {
    const bool was = InterlockedExchange(&g_trOn, on ? 1 : 0) != 0;
    if (on && !was) {
        Lock lk;
        g_trAt = g_trHave = g_trLeft = 0;
        g_trPrevSeq = 0; g_trPrevC5Ok = false;
        ZA_INFO("vr80/trace: ON (%s) - camera writes are recorded and pinned to their eye tags without the "
                "accounting; up to %u dumps: %d presents after each return to gameplay, and %d before / %d after "
                "any pairing override at most every %.0f s", source ? source : "?", kTraceMaxDumps, kTraceAfterArm,
                kTraceBack, kTraceAhead, kTraceOverrideGapMs / 1000.0);
    } else if (!on && was) {
        ZA_INFO("vr80/trace: off (%s), %u dump(s) printed", source ? source : "?", g_trDumps);
    }
}
bool trace_enabled() { return InterlockedCompareExchange(&g_trOn, 0, 0) != 0; }
bool capturing() { return enabled() || trace_enabled(); }
void trace_arm(const char* why) {
    if (!trace_enabled()) return;
    g_trArmWhy = why ? why : "?";
    InterlockedExchange(&g_trArmReq, 1);
}
void trace_note(const char* text) {
    if (!trace_enabled()) return;
    Lock lk;
    _snprintf(g_trNote, sizeof(g_trNote), "%s", text ? text : "");
    g_trNote[sizeof(g_trNote) - 1] = 0;
}
void trace_basis(const float right[3], bool ok) {
    if (!trace_enabled()) return;
    Lock lk;
    g_trBasisOk = ok && right;
    if (g_trBasisOk) for (int i = 0; i < 3; ++i) g_trBasis[i] = right[i];
}

// VR-91. Switching mode RESETS: the two modes bin on different axes and gate on
// different things, so an episode holding samples from both would be a table of
// two questions averaged together.
void set_roll_mode(bool on, const char* source) {
    ensure_cs();
    Lock lk;
    if (g_rollMode == on) return;
    close_episode("accounting mode changed");
    reset_consumer();
    g_rollMode = on;
    if (on)
        ZA_INFO("zaccount: ROLL mode (%s) - binning by head roll (LEFT %.0f..%.0f, LEVEL +-%.0f, "
                "RIGHT %.0f..%.0f deg) with camera pitch held within %.0f deg of level, and measuring "
                "the LATERAL residual along the yaw-only right axis each write used. The pitch mode "
                "REJECTS anything rolled past %.0f deg, so it could never see a roll fault. Hold a "
                "roll still for a few seconds per bin, both ways, standing.",
                source, kRollLeftLo, kRollLeftHi, kRollLevelAbs, kRollRightLo, kRollRightHi,
                kRollPitchAbs, kRollMax);
    else
        ZA_INFO("zaccount: PITCH mode (%s) - back to the VR-78 vertical accounting.", source);
}
bool roll_mode() { return g_rollMode; }

void reset(const char* why) {
    Lock lk;
    close_episode(why);
    reset_consumer();
    g_lastC5SerialOk = false;
    for (int i = 0; i < kPin; ++i) g_pin[i] = Write();
}

void publish_head(const Head& h) {
    if (!enabled()) return;
    Lock lk;
    g_head = h;
    g_headOk = true;
}

bool head_snapshot(Head* out) {
    if (!out) return false;
    Lock lk;
    if (!g_headOk) return false;
    *out = g_head;
    return true;
}

void note_clamp(const Clamp& c) {
    g_clamp = c;
    g_clampOk = true;
}

bool clamp_latest(Clamp* out) {
    if (!out || !g_clampOk) return false;
    *out = g_clamp;
    return true;
}

void note_write(const Write& w) {
    g_lastWrite = w;
    g_lastWriteOk = true;
}

uint32_t pin_for_tag(const float* tagPos) {
    if (!capturing()) return 0;
    if (!g_lastWriteOk) { ++g_pinRefused; return 0; }
    Write w = g_lastWrite;
    if (tagPos) {
        for (int i = 0; i < 3; ++i)
            if (fabsf(tagPos[i] - w.written[i] * w.c5Sign) > 0.01f) w.tagPosMatch = false;
    } else {
        w.tagPosMatch = !w.wrote;   // a tag without a position must come from a call that did not write
    }
    Lock lk;
    w.id = g_nextId++;
    if (!g_nextId) g_nextId = 1;
    g_pin[w.id & (kPin - 1)] = w;
    ++g_pinned;
    return w.id;
}

void on_present(int ringEye, int finalEye, bool tagged, uint32_t id, bool haveC5, const float c5[3],
                uint32_t c5Serial, double nowMs) {
    if (trace_enabled()) {
        Lock lk;
        trace_present(ringEye, finalEye, tagged, id, haveC5, c5, nowMs);
    }
    if (!enabled()) return;
    if (!tagged || finalEye == 0) return;   // mono presents are not part of the question
    Lock lk;
    // A run whose every present is rejected before its stance is known must still
    // report those rejections: the episode exists from its first tagged present.
    if (!g_ep.any) { g_ep.any = true; g_ep.startMs = nowMs; if (!g_ep.index) g_ep.index = ++g_episodes; }
    g_ep.lastMs = nowMs;
    ++g_ep.tagged;
    if (!id) { reject(R_MISSING); return; }
    const Write& slot = g_pin[id & (kPin - 1)];
    if (slot.id != id) { reject(R_EXPIRED); return; }
    if (ringEye != finalEye) { reject(R_OVERRIDE); return; }
    const Write w = slot;
    consume(w, finalEye, haveC5, c5, c5Serial);
    (void)nowMs;
}

void tick(double nowMs) {
    if (!enabled()) return;
    if (nowMs < g_nextProgressMs) return;
    Lock lk;
    g_nextProgressMs = nowMs + kProgressMs;
    if (g_ep.any && g_ep.accepted != g_ep.lastProgressAccepted) {
        g_ep.lastProgressAccepted = g_ep.accepted;
        report("progress", false);
    }
}

void flush(const char* why) {
    Lock lk;
    close_episode(why);
    reset_consumer();
}

void log_status() {
    Lock lk;
    ZA_INFO("zaccount: %s (%s) | episode #%lu %s, accepted %lu | pinned %lu, refused %lu (no writer call yet) | "
            "`camera zaccount on|off|reset|status`",
            enabled() ? "ON" : "off", g_source, (unsigned long)g_ep.index, kStanceName[g_ep.stance],
            (unsigned long)g_ep.accepted, (unsigned long)g_pinned, (unsigned long)g_pinRefused);
}

} // namespace dvr::zacct
