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
#include "core/gfx/stereo_menu_hold.h"
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
    g_lateOwed = g_lateRepaired = g_lateExpired = 0;
    memset(g_lateExpireReason,0,sizeof(g_lateExpireReason));
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
    bool  exact = false;    // VR-80 run 5: zero lead at DRAW granularity - each tag is pushed just before its own present
    int   lateEvery = 0;    // every N ticks, pass 1's tag is pushed AFTER its present (run 5's onset); 0 = never
    bool  lateRepair = false;   // candidate F-late on
};

struct Result {
    int presents = 0, wrongEye = 0, wrongRecord = 0, emptyPops = 0, untaggedOut = 0;
    int firstWrong = -1, lastWrong = -1, realigns = 0, took = 0, held = 0, wrongLate = 0;
    bool sustained = false;   // still wrong in the last 10% of presents
    int lateEvents = 0, lateRepaired = 0;
    int delivWrong = 0, delivHeld = 0;   // the pipelined capture (SharedWait=0): each present delivers the previous one's pixels and tag
    std::vector<std::string> firstCycle;   // per present from the first late event: pop, action, out vs truth
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
    g_lateTagRepair = s.lateRepair;
    ArbState st;
    Result r;
    const int draws = s.ticks * 2;
    int pushedDraws = 0;   // tags pushed so far (draw index of the next push)
    int slotTag = 0, slotTrue = 0; bool slotValid = false;   // the grabbed, not yet delivered slot
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
        int upto = s.exact ? shownDraw : shownDraw + 2 * lead_for(shownDraw) + (shownDraw % 2 == 0 ? 1 : 0);
        const bool late = s.lateEvery > 0 && shownDraw % 2 == 0 && shownDraw / 2 > 5 && (shownDraw / 2) % s.lateEvery == 0;
        if (late) { upto = shownDraw - 1; ++r.lateEvents; }
        push_through(upto);
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
        ArbTrace tr;
        const bool tagged = pop_and_arbitrate(st, v, t, ringEye, inv, along, other, &tr);
        if (r.lateEvents == 1 && r.firstCycle.size() < 6) {
            char b[96];
            _snprintf(b, sizeof(b), "%s%s%s%s%s%s out %+d true %+d", tr.popResult == POPR_TAG ? "tag" : "EMPTY",
                      tr.action & ACT_LATE ? " LATE" : "", tr.action & ACT_TOOK ? " TOOK" : "", tr.action & ACT_REFUSE ? " REFUSE" : "",
                      tr.action & ACT_AGREE ? " agree" : "", tr.removedN && !(tr.action & ACT_LATE) ? " drain" : "", tagged ? t.eye : 0, trueEye);
            b[sizeof(b) - 1] = 0;
            r.firstCycle.push_back(b);
        }
        if (emptyBefore) ++r.emptyPops;
        r.realigns += (int)(g_c5Realigned - realignBefore);
        const int eye = tagged ? t.eye : 0;
        // the shipped reentry.cpp relabels the waiting slot on a late-tag repair (capture::relabel_last_grab)
        if ((tr.action & ACT_LATE) && slotValid && slotTag == 0) slotTag = tr.lateEye;
        if (slotValid && r.presents > 4) {
            if (slotTag == 0 && slotTrue != 0) ++r.delivHeld;
            else if (slotTag != 0 && slotTag != slotTrue) ++r.delivWrong;
        }
        slotTag = eye; slotTrue = trueEye; slotValid = true;
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
    r.took = (int)g_c5Took; r.held = (int)g_c5Held; r.lateRepaired = (int)g_lateRepaired;
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
    dvr::stereo::MenuGapHold menuHold;
    check(!menuHold.hold(0,true,1000),"menu: no history cannot hold mono");
    check(!menuHold.hold(-1,true,1000),"menu: left image is never held");
    check(!menuHold.hold(+1,true,1005),"menu: right image is never held");
    for(int ms=1006;ms<1155;++ms)
        check(menuHold.hold(0,true,ms),"menu: short center-eye gap retains stereo");
    check(!menuHold.hold(0,true,1155),"menu: 150ms cap prevents indefinite freeze");
    check(!menuHold.hold(0,false,1010),"menu: actual menu exit bypasses extra hold");
    check(!menuHold.hold(0,true,900),"menu: invalid clock does not freeze");
    menuHold.clear();
    check(!menuHold.hold(0,true,1010),"menu: reset clears stereo history");
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
    for (const auto& s0 : table) {
        Scenario s = s0; s.lateRepair = false;
        const Result r = run(s);
        print(s, r);
        if (s.fault == F_NONE && s.leadTicks <= 2 && s.dipPct == 0 && s.risePct == 0)
            check(r.wrongEye == 0 && r.untaggedOut == 0 && r.wrongRecord == 0,
                  "no fault, steady lead within the ring's design depth: every present carries its own draw's eye and record");
        if (!r.reconciles) printf("  ledger accounting: tail %ld head %ld normal %u repair %u clear %u accepted %u\n",
                                  (long)g_ringTail, (long)g_ringHead, g_popNormal, g_popRepair, g_popClearRemoved, g_pushAccepted);
        check(r.reconciles, "the ledger's counters account for every tag that entered or left the ring");
        s.lateRepair = true;
        const Result q = run(s);
        if (q.wrongEye + q.untaggedOut > r.wrongEye + r.untaggedOut) { printf("  lever ON worse: "); print(s, q); }
        check(q.wrongEye + q.untaggedOut <= r.wrongEye + r.untaggedOut && q.wrongRecord <= r.wrongRecord + 2,
              "F-late on is no worse than off on every existing schedule (wrong eyes, untagged, records)");
        check(q.reconciles, "F-late on: the accounting still reconciles");
    }

    // VR-80 run 5: zero lead at draw granularity and a recurring late pass-1 push.
    printf("\nrun 5 schedules (tags pushed just before their presents, pass 1's tag late every N ticks)\n");
    for (float walk : {0.0f, 1.5f})
        for (int every : {40, 17}) {
            Scenario off = {"late pass-1 tag, lever off", 0, walk, F_NONE, 0}; off.exact = true; off.lateEvery = every;
            Scenario on = off; on.name = "late pass-1 tag, F-late ON"; on.lateRepair = true;
            const Result a = run(off), b = run(on);
            print(off, a); print(on, b);
            if (walk == 0.0f && every == 40) {
                printf("  first cycle, lever off:"); for (auto& c : a.firstCycle) printf(" [%s]", c.c_str()); printf("\n");
                printf("  first cycle, lever on: "); for (auto& c : b.firstCycle) printf(" [%s]", c.c_str()); printf("\n");
                // the ledger's cycle: EMPTY/REFUSE, TOOK, a wrong eye, TOOK plus a drain, then agree
                const bool shape = a.firstCycle.size() >= 5 && a.firstCycle[0].find("EMPTY REFUSE") == 0 &&
                                   a.firstCycle[1].find("TOOK") != std::string::npos &&
                                   a.firstCycle[2].find("out +1 true -1") != std::string::npos &&
                                   a.firstCycle[3].find("TOOK drain") != std::string::npos &&
                                   a.firstCycle[4].find("agree") != std::string::npos;
                check(shape, "the model reproduces run 5's four-present cycle (EMPTY/REFUSE, TOOK, left image to the right eye, TOOK + drain)");
            }
            check(a.wrongEye >= a.lateEvents - 1 && a.realigns >= a.lateEvents - 1, "lever off: every late tag costs a wrong eye and a drain");
            check(b.wrongEye == 0 && b.realigns == 0, "F-late on: no wrong eye and no drain on the late-tag schedule");
            check(b.lateRepaired >= b.lateEvents - 1, "F-late on: every late event was repaired");
            printf("  pipelined delivery: lever off held %d wrong %d | lever on held %d wrong %d\n", a.delivHeld, a.delivWrong, b.delivHeld, b.delivWrong);
            check(a.delivHeld >= a.lateEvents - 1, "lever off: every late tag's image is held out of its eye at delivery");
            check(b.delivHeld == 0 && b.delivWrong == 0, "F-late on with the slot relabel: every image reaches its own eye, none held");
            check(a.reconciles && b.reconciles, "run 5 schedules reconcile");
        }

    // the model resets between runs: the same schedule twice gives the same answer
    const Scenario again = {"repeat", 1, 0.0f, F_REPEAT_PRESENT, 201};
    const Result a = run(again), b = run(again);
    check(a.wrongEye == b.wrongEye && a.emptyPops == b.emptyPops && a.realigns == b.realigns,
          "the model is deterministic and resets between runs");

    // VR-229: unresolved debt reasons must distinguish camera ambiguity from
    // publication delay and a contradictory front tag; never infer missing data.
    for (int reason=1; reason<=4; ++reason) {
        reset_model(); g_lateTagRepair=reason!=1;
        ArbState state; state.owedEye=-1; state.inStream=true; state.prevC5Ok=true;
        ArbView view; view.haveC5=true; view.basisOk=true; view.br[0]=1; view.ipd=kIpd;
        view.c5now[0]=reason==2?0:-kIpd;
        if (reason==4) push_tag(+1,nullptr,91,0,91);
        Tag tag={}; int ring=0, measured=0; float along=0, other=0; ArbTrace trace;
        pop_and_arbitrate(state,view,tag,ring,measured,along,other,&trace);
        check(trace.expireReason==reason && trace.expiredEye==-1,"expiry diagnostic identifies actual failed guard");
        check(g_lateExpireReason[reason]==1 && g_lateExpired==1,"expiry population reconciles");
        check(trace.expireFrontDraw==(reason==4?91u:0u),"front identity only claimed when inspected");
    }

    if (g_fail) { printf("reentry-pair host: %d of %d checks FAILED\n", g_fail, g_checks); return 1; }
    printf("reentry-pair host: all %d checks passed\n", g_checks);
    return 0;
}
