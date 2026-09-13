// Host model for VR-80: the reentry method's tag ring and c5 pairing, compiled from
// the SAME source the proxy ships (src/core/gfx/reentry_pair.inc), driven by a
// simulated game thread (pushes a -1/+1 tag pair per tick before its draws) and a
// simulated render thread (presents each draw in order). Every draw carries its
// true identity, so each present is judged against the oracle, not against the
// counters the pairing keeps about itself.
//
// This is an INVESTIGATION harness first: it prints a table of what each schedule
// does. The checks below assert only things that must hold whatever VR-80 turns out
// to be (no fault, no skew; the model resets), so the table cannot be fitted to a
// hypothesis by the assertions.
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#define DVR_CAT 0
#define DVR_LOG_EVERY_MS(...) ((void)0)
namespace model {
#include "core/gfx/reentry_pair.inc"
}
using namespace model;

static int g_fail = 0, g_checks = 0;
static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) { ++g_fail; printf("FAIL: %s\n", what); }
}

static void reset_model() {
    g_ringHead = g_ringTail = 0;
    g_ringDropped = g_ringCleared = g_tagMismatch = g_tagOk = g_tagUntagged = g_tagNoFrame = g_tagResynced = 0;
    g_c5Pair = true;
    g_c5Agree = g_c5Disagree = g_c5Realigned = g_c5Verdicts = g_c5Unknown = g_c5Untagged = 0;
    g_c5Took = g_c5Held = g_c5Refused = g_pushSameEye = 0;
    g_lastPushedEye = 0;
    g_pushAccepted = g_pushRejected = g_lastRejectedDraw = 0;
    g_popNormal = g_popRepair = g_popClearRemoved = g_popClears = g_popEmpty = g_lifecycleRemoved = 0;
}

enum Fault { F_NONE, F_REPEAT_PRESENT, F_DROP_PRESENT, F_DROP_PUSH, F_ZERO_TICK };

struct Scenario {
    const char* name;
    int   leadTicks;        // ticks the game thread has pushed beyond the tick being presented
    float walk;             // camera travel per tick along right, uu (0 = still)
    Fault fault;
    int   faultAt;          // the present index (draw index) where the fault is injected
    int   ticks = 600;
    int   dipPct = 0;       // per tick: chance (percent) the game thread's lead drops by one (it stalled)
    int   risePct = 0;      // per tick: chance the lead rises by one (the render thread stalled)
};

struct Result {
    int presents = 0, wrongEye = 0, wrongRecord = 0, emptyPops = 0, untaggedOut = 0;
    int firstWrong = -1, lastWrong = -1, realigns = 0, took = 0, held = 0, wrongLate = 0;
    bool sustained = false;   // still wrong in the last 10% of presents
    bool reconciles = true;   // the ledger's accounting: tail moved == removals, head moved == accepted pushes
};

static const float kIpd = 6.82f;

// c5 along right for draw d = 2*tick + pass. pass 0 is the left eye (-1).
static float c5_of(int draw, float walk) {
    const int tick = draw / 2, pass = draw % 2;
    const float base = walk * (float)tick;
    return pass == 0 ? base + 0.5f * kIpd : base - 0.5f * kIpd;
}

static Result run(const Scenario& s) {
    reset_model();
    ArbState st;
    Result r;
    const int draws = s.ticks * 2;
    int pushedDraws = 0;   // tags pushed so far (draw index of the next push)
    auto push_through = [&](int drawInclusive) {
        while (pushedDraws <= drawInclusive && pushedDraws < draws) {
            const int tick = pushedDraws / 2;
            if (s.fault == F_ZERO_TICK && tick == s.faultAt / 2) {
                // a single-draw tick: ONE 0 tag and one draw (its pass 2 never runs)
                float p[3] = {c5_of(pushedDraws, s.walk), 0, 0};
                push_tag(0, p, (uint32_t)(pushedDraws + 1), 0, (uint32_t)(pushedDraws + 1));
                pushedDraws += 2;
                continue;
            }
            for (int pass = 0; pass < 2; ++pass) {
                const int d = pushedDraws + pass;
                if (s.fault == F_DROP_PUSH && d == s.faultAt) continue;   // this draw's tag never published
                float p[3] = {c5_of(d, s.walk), 0, 0};
                push_tag(pass == 0 ? -1 : +1, p, (uint32_t)(d + 1), 0, (uint32_t)(d + 1));
            }
            pushedDraws += 2;
        }
    };
    auto lead_for = [&](int draw) {
        // deterministic per-tick jitter (no RNG state, so a schedule is reproducible)
        uint32_t h = (uint32_t)(draw / 2) * 2654435761u; h ^= h >> 15; const int roll = (int)(h % 100);
        int lead = s.leadTicks;
        if (roll < s.dipPct) lead -= 1;
        else if (roll < s.dipPct + s.risePct) lead += 1;
        return lead < 0 ? 0 : lead;
    };
    auto present = [&](int shownDraw, int trueEye) {
        push_through(shownDraw + 2 * lead_for(shownDraw) + (shownDraw % 2 == 0 ? 1 : 0));
        ArbView v;
        v.haveC5 = true;
        v.c5now[0] = c5_of(shownDraw, s.walk);
        v.basisOk = true; v.br[0] = 1.0f;
        v.ipd = kIpd;
        const bool emptyBefore = g_ringTail == g_ringHead;
        const uint32_t realignBefore = g_c5Realigned;
        Tag t = {};
        int ringEye = 0, inv = 0;
        float along = 0, other = 0;
        const bool tagged = pop_and_arbitrate(st, v, t, ringEye, inv, along, other);
        if (emptyBefore) ++r.emptyPops;
        r.realigns += (int)(g_c5Realigned - realignBefore);
        const int eye = tagged ? t.eye : 0;
        ++r.presents;
        const int idx = r.presents - 1;
        bool wrong = false;
        if (eye == 0) { ++r.untaggedOut; if (trueEye != 0) wrong = true; }
        else if (eye != trueEye) { ++r.wrongEye; wrong = true; }
        if (tagged && t.rec != 0 && (int)t.rec != shownDraw + 1) ++r.wrongRecord;
        if (wrong) { if (r.firstWrong < 0) r.firstWrong = idx; r.lastWrong = idx; if (idx >= s.faultAt + 200) ++r.wrongLate; }
    };
    for (int d = 0; d < draws; ++d) {
        const int tick = d / 2, pass = d % 2;
        if (s.fault == F_ZERO_TICK && tick == s.faultAt / 2) {
            if (pass == 0) present(d, 0);   // the single draw's present, untagged by design
            continue;
        }
        if (s.fault == F_DROP_PRESENT && d == s.faultAt) continue;   // drew, never presented
        present(d, pass == 0 ? -1 : +1);
        if (s.fault == F_REPEAT_PRESENT && d == s.faultAt) present(d, pass == 0 ? -1 : +1);   // shown again
    }
    r.took = (int)g_c5Took; r.held = (int)g_c5Held;
    r.reconciles = (long)g_ringTail == (long)(g_popNormal + g_popRepair + g_popClearRemoved + g_lifecycleRemoved) &&
                   (long)g_ringHead == (long)g_pushAccepted;
    r.sustained = r.lastWrong >= (r.presents * 9) / 10;
    return r;
}

static void print(const Scenario& s, const Result& r) {
    printf("%-34s lead %d dip %2d%% rise %2d%% walk %4.1f | presents %4d wrong eye %4d (late %4d) untagged %3d wrong record %4d empty %4d "
           "realign %3d took %3d held %3d | first %4d last %4d %s\n",
           s.name, s.leadTicks, s.dipPct, s.risePct, (double)s.walk, r.presents, r.wrongEye, r.wrongLate, r.untaggedOut, r.wrongRecord, r.emptyPops,
           r.realigns, r.took, r.held, r.firstWrong, r.lastWrong, r.sustained ? "SUSTAINED" : "");
}

int main() {
    std::vector<Scenario> table;
    for (int lead = 0; lead <= 3; ++lead)
        for (float walk : {0.0f, 1.5f}) {
            table.push_back({"no fault", lead, walk, F_NONE, 0});
            table.push_back({"repeat present (consumes a tag)", lead, walk, F_REPEAT_PRESENT, 201});
            table.push_back({"repeat present on a pass 2", lead, walk, F_REPEAT_PRESENT, 202});
            table.push_back({"a draw that never presents", lead, walk, F_DROP_PRESENT, 201});
            table.push_back({"a pass 2 that never presents", lead, walk, F_DROP_PRESENT, 202});
            table.push_back({"a tag that never publishes", lead, walk, F_DROP_PUSH, 201});
            table.push_back({"a single-draw tick (0 tag)", lead, walk, F_ZERO_TICK, 200});
        }
    // A varying lead: the game thread stalls (dip) or the render thread stalls (rise).
    for (int lead = 1; lead <= 2; ++lead)
        for (int dip : {10, 30}) {
            Scenario a = {"no fault, varying lead", lead, 0.0f, F_NONE, 201}; a.dipPct = dip; a.risePct = 5; table.push_back(a);
            Scenario b = {"repeat present, varying lead", lead, 0.0f, F_REPEAT_PRESENT, 201}; b.dipPct = dip; b.risePct = 5; table.push_back(b);
            Scenario c = {"never-presenting draw, varying", lead, 0.0f, F_DROP_PRESENT, 201}; c.dipPct = dip; c.risePct = 5; table.push_back(c);
            Scenario d = {"unpublished tag, varying lead", lead, 0.0f, F_DROP_PUSH, 201}; d.dipPct = dip; d.risePct = 5; table.push_back(d);
        }
    printf("VR-80 host model: the shipped ring and c5 pairing against the draw oracle\n");
    printf("(lead = ticks the game thread has pushed beyond the tick being presented; 'late' = wrong eyes\n"
           " more than 200 presents after the fault; lead 3 exceeds the ring's depth-6 clear by design)\n");
    for (const auto& s : table) {
        const Result r = run(s);
        print(s, r);
        if (s.fault == F_NONE && s.leadTicks <= 2 && s.dipPct == 0 && s.risePct == 0)
            check(r.wrongEye == 0 && r.untaggedOut == 0 && r.wrongRecord == 0,
                  "no fault, steady lead within the ring's design depth: every present carries its own draw's eye and record");
        if (!r.reconciles) printf("  ledger accounting: tail %ld head %ld normal %u repair %u clear %u accepted %u\n",
                                  (long)g_ringTail, (long)g_ringHead, g_popNormal, g_popRepair, g_popClearRemoved, g_pushAccepted);
        check(r.reconciles, "the ledger's counters account for every tag that entered or left the ring");
    }
    // the model resets between runs: the same schedule twice gives the same answer
    const Scenario again = {"repeat", 1, 0.0f, F_REPEAT_PRESENT, 201};
    const Result a = run(again), b = run(again);
    check(a.wrongEye == b.wrongEye && a.emptyPops == b.emptyPops && a.realigns == b.realigns,
          "the model is deterministic and resets between runs");

    if (g_fail) { printf("reentry-pair host: %d of %d checks FAILED\n", g_fail, g_checks); return 1; }
    printf("reentry-pair host: all %d checks passed\n", g_checks);
    return 0;
}
