// core/gfx/reentry.cpp - rung 3 of the stereo ladder: SequentialReentry.
//
// THE IDEA. Call the engine's viewport draw a SECOND time per tick with the
// other eye's camera, so every present carries a fresh eye and every per-eye
// effect (reflections, screen-space post, the viewmodel) is native to its
// eye. Present-rate is 2x the tick rate; the beat line reads L/s == R/s ==
// out/s / 2 and draws/s == 2nd/s (the Infinite acceptance numbers).
//
// THE SPLIT. The game side (game/dishonored/scene_draw.cpp) owns the root, the
// call-site patch, the gates, the SEH guard and the second call; it registers
// through set_reentry_hooks and pushes one eye tag per draw into the ring
// below. This side is the method: per present it pops the tag the draw
// pushed, captures the game's frame into an RGBA texture (the same capture
// and blit the mono screen uses), attaches the eye to the output and hands
// the runtime its tag (dvr::vr::sr_push_eye, right before the runtime pops it
// in on_present_end - one tag per present, so the runtime's ring never
// skews). The runtime's pair pacing (a LEFT present holds the XR frame open,
// the RIGHT completes it) does the rest.
//
// THE PAIRING PROOF. Each tag carries the camera position the writer
// produced; the present compares it with the c5 the constant hook captured
// for the frame it is about to show and DROPS a tag that does not match
// (counted as tagMismatch) - a present from another draw caller, a movie or a
// Reset would otherwise eat a tag and swap the eyes.
//
// FAIL SOFT. select("reentry") is accepted only when the game side verifies
// the root's bytes; a fault in the second draw poisons the game side, and the
// next present here drops the method to mono. The eyetest refuses to run
// while this method is active (two presents per tick with different eyes
// would destroy its verdict); `stereo mono` restores the call site.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"
#include "core/gfx/desktop_eye.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/gfx/capture.h"
#include "core/gfx/frame_id.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"
#include "game/dishonored/camera.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::stereo {
namespace {

ReentryHooks g_hooks;

// The tag ring, pop/peek and the c5 pairing live in reentry_pair.inc so the VR-80 host
// model compiles the same code (tools/reentry-pair-host.ps1).
#include "core/gfx/reentry_pair.inc"

// ---- VR-80: the ring ledger ([Stereo] RingLedger, default off) ---------------------------
//
// One record per present that reached the pop: the ring before the pop, what the pop
// returned, what the c5 arms and the drain did (every removed draw id), the eye and the
// record that went out, the popped tag's age, the c5-to-position distance for the popped
// tag and for the next one (corroboration, not identity), and why end_frame returned.
// Records go into a 64-entry buffer; lines are printed only in bounded windows: after an
// arm (a return to gameplay, a re-arm) and around an override, a drain or an empty pop in
// a tagged stream. Plus a 10 s reconcile line: every tag that entered or left the ring.
volatile LONG g_ledgerOn = 0;
volatile LONG g_ledgerArmReq = 0;
const char*   g_ledgerArmWhy = "";
volatile LONG g_ledgerStance = 0;   // 0 unknown, 1 standing, 2 crouched (set by the game side)
uint32_t g_endFrames = 0, g_exitPoisoned = 0, g_exitDevices = 0, g_exitBlit = 0;
bool g_singleTagRepair = false;
uint32_t g_singleTagObserved = 0, g_singleTagFixed = 0, g_singleTagRefused = 0;
uint32_t g_lateRelabeled = 0, g_lateRelabelRefused = 0;   // F-late: capture slots relabelled / refused
enum LedgerOut : uint8_t { OUT_OK = 0, OUT_MONO, OUT_HOLD, OUT_NOSRC, OUT_TARGET };
const char* kLedgerOut[] = {"stereo", "mono", "HOLD", "NOSRC", "TARGET"};
struct LedgerRec {
    uint32_t id = 0, frame = 0;
    double   ms = 0.0;
    int      stance = 0;
    ArbTrace tr;
    int      ringEye = 0, inv = 0;
    float    along = 0.0f, other = 0.0f;
    bool     tagged = false;
    int      finalEye = 0;
    uint32_t draw = 0, rec = 0, popDraw = 0;
    double   ageMs = -1.0;
    bool     w2cSelfOk = false, w2cNextOk = false;
    float    w2cSelf = 0.0f, w2cNext = 0.0f;
    int      nextEye = 0;
    uint32_t nextDraw = 0;
    uint8_t  out = OUT_OK;
    int      delivered = 0;
    uint32_t delivSerial = 0;
    bool     fresh = false;
};
constexpr int      kLedN = 64, kLedBack = 12, kLedAfterArm = 24, kLedAfterEvent = 16;
constexpr uint32_t kLedMaxDumps = 40;
constexpr double   kLedEventGapMs = 2000.0;
LedgerRec g_led[kLedN];
uint32_t  g_ledCount = 0, g_ledDumps = 0, g_ledLastPrinted = 0;
int       g_ledLeft = 0;
double    g_ledNextEventMs = 0.0;
bool      g_ledPrevTagged = false;

void ledger_print(const LedgerRec& r) {
    char pop[40], act[64], rem[80], w2c[64];
    if (r.tr.popResult == POPR_TAG) _snprintf(pop, sizeof(pop), "D%u(%+d)", r.popDraw, r.ringEye);
    else if (r.tr.popResult == POPR_CLEAR) _snprintf(pop, sizeof(pop), "CLEAR %u", r.tr.clearRemoved);
    else _snprintf(pop, sizeof(pop), "EMPTY");
    int a = 0; act[0] = 0;
    if (r.tr.action & ACT_AGREE)   a += _snprintf(act + a, sizeof(act) - a, " agree");
    if (r.tr.action & ACT_TOOK)    a += _snprintf(act + a, sizeof(act) - a, " TOOK");
    if (r.tr.action & ACT_HELD)    a += _snprintf(act + a, sizeof(act) - a, " HELD");
    if (r.tr.action & ACT_REALIGN) a += _snprintf(act + a, sizeof(act) - a, " REALIGN");
    if (r.tr.action & ACT_INVENT)  a += _snprintf(act + a, sizeof(act) - a, " INVENT");
    if (r.tr.action & ACT_REFUSE)  a += _snprintf(act + a, sizeof(act) - a, " REFUSE");
    if (r.tr.action & ACT_UNKNOWN) a += _snprintf(act + a, sizeof(act) - a, " unknown");
    if (r.tr.action & ACT_OWE)     a += _snprintf(act + a, sizeof(act) - a, " OWE");
    if (r.tr.action & ACT_LATE)    a += _snprintf(act + a, sizeof(act) - a, " LATE-REPAIR");
    if (!act[0]) _snprintf(act, sizeof(act), " -");
    int k = 0; rem[0] = 0;
    if (r.tr.removedN) {
        k += _snprintf(rem, sizeof(rem), " removed %d [", r.tr.removedN);
        for (int i = 0; i < r.tr.removedN && i < 6; ++i) k += _snprintf(rem + k, sizeof(rem) - k, "%sD%u", i ? " " : "", r.tr.removed[i]);
        _snprintf(rem + k, sizeof(rem) - k, "] stop %s", r.tr.drainStop == 1 ? "next-other-eye" : r.tr.drainStop == 2 ? "empty" : r.tr.drainStop == 3 ? "pop-failed" : r.tr.drainStop == 4 ? "late-tag" : "?");
    } else if (r.tr.action & ACT_REALIGN) {
        _snprintf(rem, sizeof(rem), " removed 0 stop %s", r.tr.drainStop == 1 ? "next-other-eye" : r.tr.drainStop == 2 ? "empty" : "?");
    }
    int m = 0; w2c[0] = 0;
    m += r.w2cSelfOk ? _snprintf(w2c, sizeof(w2c), "self %.2f", r.w2cSelf) : _snprintf(w2c, sizeof(w2c), "self ?");
    if (r.w2cNextOk) _snprintf(w2c + m, sizeof(w2c) - m, " next D%u(%+d) %.2f", r.nextDraw, r.nextEye, r.w2cNext);
    else _snprintf(w2c + m, sizeof(w2c) - m, " next none");
    pop[sizeof(pop) - 1] = act[sizeof(act) - 1] = rem[sizeof(rem) - 1] = w2c[sizeof(w2c) - 1] = 0;
    DVR_INFO("ledger: P%u f%u %s | ring %ld..%ld d%ld newest D%u | pop %s age %.1f ms | c5 along %+.2f other %.2f inv %+d | "
             "streak %u->%u%s%s | out %+d D%u rec %u %s | w2c %s | deliv %+d ser %u%s",
             r.id, r.frame, r.stance == 2 ? "CROUCH" : r.stance == 1 ? "stand" : "?",
             (long)r.tr.tailBefore, (long)r.tr.headBefore, (long)(r.tr.headBefore - r.tr.tailBefore), r.tr.newestDraw,
             pop, r.ageMs, (double)r.along, (double)r.other, r.inv, r.tr.streakBefore, r.tr.streakAfter, act, rem,
             r.finalEye, r.tagged ? r.draw : 0u, r.rec, kLedgerOut[r.out < 5 ? r.out : 0], w2c,
             r.delivered, r.delivSerial, r.fresh ? "" : " (no fresh grab)");
    g_ledLastPrinted = r.id;
}

void ledger_open(const char* why) {
    ++g_ledDumps;
    DVR_INFO("ledger: WINDOW %u/%u (%s) - one line per present. pop D<n>(eye) is the draw whose tag this present "
             "took; out is the eye and draw that went out; w2c is the distance from this present's c5 to the popped "
             "tag's written position and to the next tag's (corroboration only: the camera can move after a write); "
             "a draw id missing between consecutive pops was removed by a drain, a clear, or never pushed.",
             g_ledDumps, kLedMaxDumps, why);
}

void ledger_commit(LedgerRec& r) {
    r.id = ++g_ledCount;
    g_led[(r.id - 1) % kLedN] = r;
    const double now = r.ms;
    if (InterlockedExchange(&g_ledgerArmReq, 0) && g_ledDumps < kLedMaxDumps) {
        ledger_open(g_ledgerArmWhy);
        for (uint32_t id = (r.id > (uint32_t)kLedBack ? r.id - kLedBack : 1); id < r.id; ++id)
            if (id > g_ledLastPrinted) ledger_print(g_led[(id - 1) % kLedN]);
        g_ledLeft = kLedAfterArm;
    }
    const bool event = (r.tr.action & (ACT_TOOK | ACT_HELD | ACT_REALIGN | ACT_INVENT | ACT_REFUSE | ACT_LATE)) != 0 ||
                       (r.tr.popResult != POPR_TAG && g_ledPrevTagged);
    if (event && g_ledLeft == 0 && now >= g_ledNextEventMs && g_ledDumps < kLedMaxDumps) {
        g_ledNextEventMs = now + kLedEventGapMs;
        ledger_open(r.tr.action & ACT_LATE ? "a late-tag repair" : r.tr.action & ACT_REALIGN ? "a drain" : r.tr.action & (ACT_TOOK | ACT_HELD) ? "an override"
                    : r.tr.popResult == POPR_CLEAR ? "a depth clear" : r.tr.popResult == POPR_EMPTY ? "an empty pop in a tagged stream"
                    : "an invented or refused eye");
        for (uint32_t id = (r.id > (uint32_t)kLedBack ? r.id - kLedBack : 1); id < r.id; ++id)
            if (id > g_ledLastPrinted) ledger_print(g_led[(id - 1) % kLedN]);
        g_ledLeft = kLedAfterEvent;
    }
    if (g_ledLeft > 0) { ledger_print(r); --g_ledLeft; }
    g_ledPrevTagged = r.tagged && r.finalEye != 0;
}

// The reconcile line: every tag that entered or left the ring must account for how far
// the ring's two ends moved. The tail is written only by this (present) thread, so its side
// is exact; the head is the game thread's, and a push between its head write and its
// counter can show as one push in flight, which the line allows and names.
void ledger_reconcile() {
    static double next = 0.0;
    static uint32_t a0 = 0, rj0 = 0, n0 = 0, rp0 = 0, cl0 = 0, lc0 = 0, em0 = 0, ef0 = 0, fr0 = 0, xp0 = 0, xd0 = 0, xb0 = 0;
    static LONG head0 = 0, tail0 = 0;
    static uint32_t lo0 = 0, lr0 = 0, le0 = 0, rl0 = 0, rr0 = 0;
    const double now = pair_now_ms();
    if (now < next) return;
    const LONG tail = g_ringTail;
    const uint32_t acc = g_pushAccepted;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    const uint32_t frames = dvr::frame::count();
    if (next != 0.0) {
        const long tailMoved = (long)(tail - tail0);
        const long removed = (long)(g_popNormal - n0) + (long)(g_popRepair - rp0) + (long)(g_popClearRemoved - cl0) +
                             (long)(g_lifecycleRemoved - lc0);
        const long headMoved = (long)(head - head0), pushed = (long)(acc - a0);
        const bool tailOk = tailMoved == removed;
        const long headGap = headMoved - pushed;
        DVR_INFO("ledger/reconcile: 10 s | presents %u, end_frame %u (pre-pop exits: poisoned %u devices %u blit %u) | "
                 "pushes accepted %u rejected %u (last rejected D%u) | removed: normal %u repair %u clear %u lifecycle %u; "
                 "empty pops %u | tail moved %ld vs removals %ld (%s) | head moved %ld vs accepted %ld (%s) | depth now %ld | "
                 "late tags ([Stereo] LateTagRepair %s): owed %u repaired %u expired %u, slot relabelled %u refused %u",
                 frames - fr0, g_endFrames - ef0, g_exitPoisoned - xp0, g_exitDevices - xd0, g_exitBlit - xb0,
                 (unsigned)pushed, g_pushRejected - rj0, g_lastRejectedDraw, g_popNormal - n0, g_popRepair - rp0,
                 g_popClearRemoved - cl0, g_lifecycleRemoved - lc0, g_popEmpty - em0,
                 tailMoved, removed, tailOk ? "reconciles" : "DOES NOT RECONCILE - a removal path is uncounted",
                 headMoved, pushed, headGap == 0 ? "reconciles" : (headGap == 1 || headGap == -1) ? "one push in flight"
                                                              : "DOES NOT RECONCILE - an insertion path is uncounted",
                 (long)(head - tail), g_lateTagRepair ? "on" : "off", g_lateOwed - lo0, g_lateRepaired - lr0, g_lateExpired - le0,
                 g_lateRelabeled - rl0, g_lateRelabelRefused - rr0);
    }
    next = now + 10000.0;
    a0 = acc; rj0 = g_pushRejected; n0 = g_popNormal; rp0 = g_popRepair; cl0 = g_popClearRemoved;
    lc0 = g_lifecycleRemoved; em0 = g_popEmpty; ef0 = g_endFrames; fr0 = frames;
    xp0 = g_exitPoisoned; xd0 = g_exitDevices; xb0 = g_exitBlit; head0 = head; tail0 = tail;
    lo0 = g_lateOwed; lr0 = g_lateRepaired; le0 = g_lateExpired; rl0 = g_lateRelabeled; rr0 = g_lateRelabelRefused;
}

class SequentialReentry : public IStereo {

public:
    const char* name() const override { return "reentry"; }
    bool implemented() const override {
        char why[160] = "";
        if (!g_hooks.available) { strncpy(note_, "reentry: the game side has not registered (no scene_draw hooks)", sizeof(note_) - 1); return false; }
        if (!g_hooks.available(why, sizeof(why))) {
            _snprintf(note_, sizeof(note_), "reentry: the scene-draw root does not verify on this exe - %s", why);
            note_[sizeof(note_) - 1] = 0;
            return false;
        }
        return true;
    }
    const char* note() const override { return note_; }
    bool wants_projection() const override { return true; }
    int  presents_per_tick() const override { return 2; }
    int  eye_for_next_frame() const override { return -1; }   // pass 1 is always the left eye

    void begin_frame(const FrameInput& in) override {
        if (!armed_) {
            armed_ = true;
            if (g_hooks.set_armed) g_hooks.set_armed(true);
            DVR_INFO("stereo: reentry ARMED - two draws per tick, the second under an SEH guard; the beat "
                     "line must read L/s == R/s == out/s / 2 (%ux%u eye recommended)", in.eyeW, in.eyeH);
        }
    }

    // The runtime submits at the tail of the PREVIOUS present, after this
    // method returned; a stale eye in that submit is noticed here, one present
    // later, and named with the owner first: the deltas of the game side's
    // pass-2 skip counters (a one-sided -1 stream), the runtime's swapchain
    // failures, or a tag eaten by a present that opened no frame.
    void stale_check() {
        const uint32_t stale = dvr::vr::pair_stale_submits();
        uint32_t gates[kReentryGateCount] = {};
        if (g_hooks.gates) g_hooks.gates(gates);
        dvr::vr::PairProbe pp;
        dvr::vr::pair_probe_peek(&pp);
        if (stale != lastStale_ && lastStaleInit_) {
            uint32_t dg[kReentryGateCount];
            uint32_t gameSkips = 0;
            for (int i = 0; i < kReentryGateCount; ++i) { dg[i] = gates[i] - lastGates_[i]; gameSkips += dg[i]; }
            const uint32_t dAcq = pp.acqFail - lastProbe_.acqFail, dWait = pp.waitFail - lastProbe_.waitFail;
            const uint32_t dUntagged = g_tagUntagged - lastUntagged_;
            const uint32_t dNoFrame = g_tagNoFrame - lastNoFrame_;
            const uint32_t dAbortLeft = pp.abortLeft - lastProbe_.abortLeft;
            const uint32_t dEaten = pp.eatenNoFrame - lastProbe_.eatenNoFrame;
            // 41.1 (session 10): the method's OWN actors. Until now the line
            // printed only the game side's gates and the runtime's failures,
            // so the one actor that can manufacture a second -1 - the c5
            // override in end_frame - was invisible, and every field on the
            // line read a truthful zero while the fault ran.
            const uint32_t dSame = g_pushSameEye - lastSame_;
            const uint32_t dTook = g_c5Took - lastTook_, dHeld = g_c5Held - lastHeld_;
            const uint32_t dRefused = g_c5Refused - lastRefused_;
            const uint32_t dRealign = g_c5Realigned - lastRealign_;
            const uint32_t dDis = g_c5Disagree - lastDis_;
            // The game thread's skip counters can lag this present by a tick;
            // a second LEFT tag closing a pair (abortLeft) is the runtime's own
            // evidence of the same one-sided stream and needs no lag.
            const char* owner =
                gameSkips ? "the game side's pass-2 gates (a one-sided -1 stream: pass 1 tagged, pass 2 skipped)"
                : dSame ? "THIS METHOD pushed the same eye twice (see the c5 fields: took/held/realigned are the "
                          "override acting on the ring; the game side pushed both tags)"
                : dAbortLeft ? "a second -1 tag closed the pair - WHO made it is the c5 fields below, not necessarily the "
                               "game side: pass 1 pushes its -1 only when pass 2 will run, so a zero in every gate means "
                               "the +1 WAS pushed and something downstream replaced or dropped it (game counters lag a tick)"
                : (dAcq || dWait) ? "the runtime's swapchain path (acquire/wait failed, the release still ran)"
                : dEaten ? "a frame-less present ate the tag (the runtime opened no XR frame for it: the pace guard "
                           "while not FOCUSED, a session hold, or a pace-thread timeout; the sibling stood alone)"
                : dUntagged ? "a present that delivered no tag (a frame-less present ate its sibling's tag)"
                : dNoFrame ? "a present whose grab delivered no frame went out untagged (a capture mode switch, a "
                             "Reset, capture off) and its sibling stood alone"
                : "unknown - a lone +1 (arming mid-tick?) or a tag eaten by the pace guard";
            const char eye = pp.stalePresR != lastProbe_.stalePresR ? 'R' : 'L';
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 1000,
                             "stereo: STALE %c EYE in a stereo submit - owner: %s | ages L=%u R=%u presents (the runtime "
                             "shows each eye swapchain's last released image; healthy = 1/0) | pass-2 skips since the "
                             "last line: foreign=%u state=%u silent=%u stall=%u session=%u test=%u exit=%u forced=%u | runtime: "
                             "acqFail=%u waitFail=%u untaggedProj=%u abortLeft=%u eatenNoFrame=%u | c5: sameEyePushed=%u "
                             "disagree=%u took=%u held=%u realigned=%u refusedInvent=%u (pairing %s) | method untagged "
                             "presents=%u noFrame=%u | stale submits so far L=%u R=%u | strict=%s",
                             eye, owner, pp.agePresL, pp.agePresR, dg[0], dg[1], dg[2], dg[3], dg[4], dg[5], dg[6], dg[7],
                             dAcq, dWait, pp.untaggedProj - lastProbe_.untaggedProj,
                             pp.abortLeft - lastProbe_.abortLeft, dEaten,
                             dSame, dDis, dTook, dHeld, dRealign, dRefused, g_c5Pair ? "on" : "off",
                             dUntagged, dNoFrame, pp.stalePresL, pp.stalePresR,
                             dvr::vr::pair_strict() ? "on (the held eye was replaced by the fresh one)" : "off");
        }
        lastStale_ = stale; lastStaleInit_ = true;
        memcpy(lastGates_, gates, sizeof(lastGates_));
        lastProbe_ = pp;
        lastUntagged_ = g_tagUntagged;
        lastNoFrame_ = g_tagNoFrame;
        lastSame_ = g_pushSameEye; lastTook_ = g_c5Took; lastHeld_ = g_c5Held;
        lastRefused_ = g_c5Refused; lastRealign_ = g_c5Realigned; lastDis_ = g_c5Disagree;
    }

    bool end_frame(const FrameDevices& d, FrameOutput& out) override {
        ++g_endFrames;
        const bool led = InterlockedCompareExchange(&g_ledgerOn, 0, 0) != 0;
        if (led) ledger_reconcile();
        if (g_hooks.poisoned && g_hooks.poisoned()) {
            ++g_exitPoisoned;
            DVR_ERROR("stereo: reentry POISONED by a second-draw fault - dropping to mono");
            armed_ = false;
            select("mono");
            return false;
        }
        stale_check();
        dvr::frameid::begin_present();   // 41.1 (session 9): the previous present's trace closes, its pairs are judged
        if (!d.dev9 || !d.dev11 || !d.ctx11) { ++g_exitDevices; return false; }
        if (!blit_.init(d.dev11)) { ++g_exitBlit; return false; }
        // The tag for the frame the game just drew, checked against its c5.
        Tag t = {0, false, {0.0f, 0.0f, 0.0f}, 0, 0};   // 41.1: the c5 arm can tag a present the ring never filled
        int eye = 0;
        float c5now[3];
        const bool haveC5 = dvr::camera::render_pos(c5now);
        ArbView view;
        view.haveC5 = haveC5;
        if (haveC5) memcpy(view.c5now, c5now, sizeof(view.c5now));
        {   float vbf[3], vbu[3]; view.basisOk = dvr::camera::last_basis(vbf, view.br, vbu); }
        view.ipd = dvr::camera::ipd_m() * dvr::camera::world_scale();
        int ringEye = 0, inv = 0;
        float along = 0.0f, other = 0.0f;
        ArbTrace arbTrace;
        bool tagged = pop_and_arbitrate(arb_, view, t, ringEye, inv, along, other, &arbTrace);
        if (arbTrace.action & ACT_LATE) {
            // F-late: the removed tag was the previous present's image, still waiting in the capture
            // slot untagged; label it so it reaches its eye instead of being held.
            if (dvr::capture::relabel_last_grab(arbTrace.lateEye, arbTrace.lateRec)) ++g_lateRelabeled;
            else {
                ++g_lateRelabelRefused;
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                                 "reentry: late tag repaired but the capture slot could not be relabelled (mode %s: "
                                 "only shared with SharedWait=0 or deferred still hold it; refused %u, relabelled %u) - "
                                 "that image goes out held", dvr::capture::mode_name(), g_lateRelabelRefused, g_lateRelabeled);
            }
        }
        const uint32_t poppedDraw = t.draw;
        Tag recovered = {};
        if (observe_single_tag(single_, view, t, ringEye, tagged ? t.eye : 0,
                               inv, along, other, arbTrace.action, recovered)) {
            ++g_singleTagObserved;
            bool fixed = false;
            if (g_singleTagRepair) {
                // SharedWait=0/deferred still own the preceding capture. A
                // mismatched/failed/already-delivered grab leaves both labels alone.
                fixed = dvr::capture::retire_last_right_grab(recovered.rec);
                if (fixed) { t = recovered; tagged = true; ++g_singleTagFixed; }
                else ++g_singleTagRefused;
            }
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
                "reentry/single-tag: confirmed adjacent R/0 camera displacement; candidate R D%u rec %u; "
                "observed %u fixed %u refused %u; this %s ([Stereo] SingleTagRepair=%d)",
                recovered.draw, recovered.rec, g_singleTagObserved, g_singleTagFixed,
                g_singleTagRefused, fixed ? "retired buffered wrong-right image, restored right record" : "unchanged",
                g_singleTagRepair ? 1 : 0);
        }
        LedgerRec lr;   // VR-80: filled only while the ledger is on, committed at every return below
        if (led) {
            lr.frame = dvr::frame::count();
            lr.ms = pair_now_ms();
            lr.stance = (int)InterlockedCompareExchange(&g_ledgerStance, 0, 0);
            lr.tr = arbTrace;
            lr.ringEye = ringEye; lr.inv = inv; lr.along = along; lr.other = other;
            lr.tagged = tagged; lr.finalEye = tagged ? t.eye : 0;
            lr.popDraw = poppedDraw;
            lr.draw = t.draw; lr.rec = tagged ? t.rec : 0u;
            lr.ageMs = (tagged && t.pushMs > 0.0) ? lr.ms - t.pushMs : -1.0;
            if (tagged && t.posOk && haveC5) {
                const float e0 = c5now[0] - t.pos[0], e1 = c5now[1] - t.pos[1], e2 = c5now[2] - t.pos[2];
                lr.w2cSelf = sqrtf(e0 * e0 + e1 * e1 + e2 * e2); lr.w2cSelfOk = true;
            }
            Tag nt;
            if (peek_tag_full(nt)) {
                lr.nextEye = nt.eye; lr.nextDraw = nt.draw;
                if (nt.posOk && haveC5) {
                    const float e0 = c5now[0] - nt.pos[0], e1 = c5now[1] - nt.pos[1], e2 = c5now[2] - nt.pos[2];
                    lr.w2cNext = sqrtf(e0 * e0 + e1 * e1 + e2 * e2); lr.w2cNextOk = true;
                }
            }
        }
        auto commit = [&](uint8_t why, int deliv, bool fr) {
            if (!led) return;
            lr.out = why; lr.delivered = deliv; lr.fresh = fr;
            lr.delivSerial = fr ? dvr::capture::delivered_serial() : 0u;
            ledger_commit(lr);
        };
        if (tagged) {
            eye = t.eye;
            ++g_tagOk;
            // TELEMETRY ONLY: the engine moves the camera by up to a tick of
            // travel after the tick's last write, so a walking player's -1
            // present sits a few uu from the written position with its eye
            // offset intact. A large distance is a teleport or a present from
            // another draw - counted and named, never a dropped tag.
            if (t.posOk && haveC5) {
                const float d0 = c5now[0] - t.pos[0], d1 = c5now[1] - t.pos[1], d2 = c5now[2] - t.pos[2];
                const float dist = sqrtf(d0 * d0 + d1 * d1 + d2 * d2);
                if (dist > 40.0f) {
                    ++g_tagMismatch;
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                                     "reentry: tag %+d kept - this present's c5 (%.1f %.1f %.1f) is %.1f uu from the "
                                     "position the draw wrote (%.1f %.1f %.1f): a teleport, or a present from another "
                                     "draw (counted, not dropped)",
                                     t.eye, c5now[0], c5now[1], c5now[2], dist, t.pos[0], t.pos[1], t.pos[2]);
                }
            }
        } else {
            ++g_tagUntagged;
        }
        if (g_hooks.present_tag)
            g_hooks.present_tag(ringEye, eye, tagged, tagged ? t.acct : 0u, haveC5, c5now,
                                dvr::camera::render_pos_serial());
        dvr::desktop_eye::note_drawn_eye(eye); // VR-76: current pixels, before capture delivery
        dvr::capture::set_pending_tag(eye);
        // VR-65: and the record the draw was rendered with, onto the same slot
        // the pixels land in. An untagged present carries 0, which the audit
        // reports as MISSING rather than silently joining to nothing.
        dvr::capture::set_pending_rec(tagged ? t.rec : 0u);
        {   // 41.1 (session 9): the camera of the draw the grab will take, and its right row
            float bf[3], br[3], bu[3];
            const bool basisOk = dvr::camera::last_basis(bf, br, bu);
            dvr::frameid::note_c5(c5now, haveC5, br, basisOk);
        }

        const bool fresh = dvr::capture::grab(d.dev9, d.dev11, d.ctx11);
        ID3D11ShaderResourceView* src = dvr::capture::srv();
        if (!src) { commit(OUT_NOSRC, 0, fresh); return false; }
        const uint32_t w = dvr::capture::width(), h = dvr::capture::height();
        if (!ensure_target(d.dev11, w, h)) { commit(OUT_TARGET, 0, fresh); return false; }
        if (fresh || !drawnOnce_) {
            blit_.draw(d.ctx11, src, rtv_, w, h);
            // 41.2 (VR-31): our own hands, over the game image and under the
            // F10 panel. The eye is the tag of the pixels JUST blitted, which
            // is NOT `eye` (the eye of the current D3D9 backbuffer) - one line
            // apart, and confusing them is the stale-eye fault in miniature.
            if (HandDrawFn hd = hand_draw())
                hd(d.dev11, d.ctx11, rtv_, w, h,
                   fresh ? dvr::capture::delivered_tag() : 0);
            if (OverlayDrawFn ov = overlay_draw()) ov(d.ctx11, rtv_, w, h);
            // 41.1 (session 9): the frame-identity trace's stages slot and out,
            // inside the read fence (the slot thumbnail is a read of the slot).
            if (fresh) {
                dvr::frameid::note_delivery(dvr::capture::delivered_serial(), dvr::capture::delivered_tag(),
                                            dvr::capture::delivered_slot(), dvr::capture::mode_name());
                dvr::frameid::stage_slot(d.dev11, d.ctx11, src);
                dvr::frameid::stage_out(d.dev11, d.ctx11, srv_);
            }
            dvr::capture::read_done(d.ctx11);   // shared: the slot may be blitted into again only after this read
            drawnOnce_ = true;
        }

        // 41.1 (session 8): a grab that delivered NOTHING (a mode switch's first
        // present, a Reset, capture off) leaves texture() re-showing the last
        // frame; its tag is the previous present's and must not be pushed
        // again, or the runtime pairs a stale duplicate (the STALE EYE at
        // every capture-mode switch, and the STALE spam under capture off).
        // The present goes out untagged: the mono path, honest.
        const int delivered = fresh ? dvr::capture::delivered_tag() : 0;   // the tag of the pixels texture() holds
        if (!fresh && eye != 0) ++g_tagNoFrame;
        // The pulse instrument's readout: the c5 of consecutive tagged presents.
        if (delivered != 0) {
            float c5[3];
            if (dvr::camera::render_pos(c5)) {
                if (delivered > 0 && lastLeftOk_) {
                    const float dx = c5[0] - lastLeft_[0], dy = c5[1] - lastLeft_[1], dz = c5[2] - lastLeft_[2];
                    const float want = dvr::camera::ipd_m() * dvr::camera::world_scale();
                    const float mag  = sqrtf(dx * dx + dy * dy + dz * dz);
                    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 6,
                                    "reentry: pair - the +1 present's c5 sits (%.2f %.2f %.2f) uu from the -1 present's "
                                    "(|d| %.2f; ipd*scale = %.2f expected along right)", dx, dy, dz,
                                    mag, want);
                    // 41.1 (Dishonored): the pair GEOMETRY, continuously. The first-6
                    // line above only ever sampled the opening second, so a
                    // misalignment that LATCHES minutes in (reported on the dev rig:
                    // proper or wrong for ~30 s at a time, standing still, with every
                    // pairing counter clean) had no measurement at all. The derived
                    // numbers are the ones that can fail the hypothesis: |d| against
                    // the ipd*scale it should be, and the ANGLE between the pair's
                    // separation and the camera right row it is supposed to lie along.
                    // A healthy pair reads err ~0 % and off-right ~0 deg; a swapped or
                    // stale eye reads a sign flip or a large angle, and says so here.
                    float fwd[3], rgt[3], up[3];
                    if (mag > 0.001f && dvr::camera::last_basis(fwd, rgt, up)) {
                        const float dot = (dx * rgt[0] + dy * rgt[1] + dz * rgt[2]) / mag;
                        const float c   = dot > 1.0f ? 1.0f : dot < -1.0f ? -1.0f : dot;
                        const float deg = acosf(c) * 57.2957795f;
                        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
                                         "reentry: pair geom |d|=%.2f uu (want %.2f, err %+.0f%%) off-right %.1f deg "
                                         "(dot %+.3f; NEGATIVE dot = the eyes are SWAPPED) sep=(%.2f %.2f %.2f) "
                                         "right=(%.2f %.2f %.2f)",
                                         mag, want, want > 0.001f ? (mag - want) * 100.0f / want : 0.0f,
                                         deg, dot, dx, dy, dz, rgt[0], rgt[1], rgt[2]);
                    }
                } else if (delivered < 0) {
                    memcpy(lastLeft_, c5, sizeof(c5)); lastLeftOk_ = true;
                }
            }
        }
        // 41.1 (Dishonored): the untagged HOLD. `delivered == 0` means this
        // present carries no eye, and an untagged present is the mono path -
        // the same image in BOTH eyes. Inside a healthy stereo stream that is
        // a one-frame flicker (the present-stall guard in scene_draw.cpp fires
        // on a tick that outran the present thread). When the lever is on and
        // the stream was tagged recently, keep this present OFF THE WIRE:
        // returning false leaves out.tex NULL, the frame path submits nothing,
        // and the compositor holds the previous pair. Bounded by N so a real
        // transition (menu, load, cinematic) still reaches mono within N.
        if (delivered == 0) {
            const int lim = dvr::stereo::hold_untagged();
            if (lim > 0 && taggedRecently_ && heldRun_ < lim) {
                ++heldRun_;
                dvr::stereo::note_hold();
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                                 "reentry: untagged present HELD (%d of %d in this run; the compositor keeps the "
                                 "previous pair instead of flipping both eyes to mono). The (N+1)th in a row goes "
                                 "out as mono - `stereo hold 0` restores that for every one.",
                                 heldRun_, lim);
                commit(OUT_HOLD, delivered, fresh);
                return false;
            }
            // Not held: the mono path, as before. A run that reaches here has
            // either spent the lever or is a genuine transition.
            taggedRecently_ = false;
        } else {
            taggedRecently_ = true;
            heldRun_ = 0;
        }
        out.tex = tex_;
        out.eyeSign = delivered;
        out.w = w; out.h = h;
        // The runtime pops exactly one tag per present in on_present_end,
        // right after this returns; a 0 pushes nothing (mono path).
        if (delivered != 0) {
            // The fault, named where it happens: the runtime pairs a LEFT with
            // the next present, so two of the same eye in a row leave the other
            // eye's swapchain untouched for two presents - the stale eye.
            if (delivered == g_lastPushedEye) {
                ++g_pushSameEye;
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 1000,
                                 "reentry: pushed eye %+d TWICE in a row (%u so far) - the %s eye's swapchain gets no "
                                 "copy this present and goes stale; the runtime counts it as abortLeft one stage later. "
                                 "c5 pairing %s: agree=%u disagree=%u took=%u held=%u realigned=%u refused=%u untagged=%u",
                                 delivered, g_pushSameEye, delivered < 0 ? "RIGHT" : "LEFT",
                                 g_c5Pair ? "on" : "off", g_c5Agree, g_c5Disagree, g_c5Took, g_c5Held,
                                 g_c5Realigned, g_c5Refused, g_c5Untagged);
            }
            g_lastPushedEye = delivered;
            dvr::vr::sr_push_eye(delivered);
        }
        commit(delivered != 0 ? OUT_OK : OUT_MONO, delivered, fresh);
        return true;
    }

    void on_reset() override { single_ = SingleTagState{}; dvr::capture::on_reset(); }

    void shutdown() override {
        if (armed_) {
            armed_ = false;
            if (g_hooks.set_armed) g_hooks.set_armed(false);
            DVR_INFO("stereo: reentry disarmed - the call site is restored at the next script dispatch");
        }
        {   // VR-80: a lifecycle clear is a removal the ledger must account for
            const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
            g_lifecycleRemoved += (uint32_t)(head - g_ringTail);
            InterlockedExchange(&g_ringTail, head);
        }
        release_target();
        blit_.shutdown();
        drawnOnce_ = false;
        lastLeftOk_ = false;
        single_ = SingleTagState{};
        arb_ = ArbState{};   // the c5 history and the disagreement streak
        g_lastPushedEye = 0;   // a re-select must not read as a repeat
        taggedRecently_ = false;
        heldRun_ = 0;
    }

    void status(dvr::status::Writer& w) override {
        w.kv("drawArmed", armed_);   // the second draw's own arm (the seam's `armed` is the tickbox)
        w.kv("tagOk", (unsigned long)g_tagOk);
        w.kv("tagMismatch", (unsigned long)g_tagMismatch);
        w.kv("tagResynced", (unsigned long)g_tagResynced);
        w.kv("tagUntagged", (unsigned long)g_tagUntagged);
        w.kv("tagNoFrame", (unsigned long)g_tagNoFrame);
        w.kv("ringDropped", (unsigned long)g_ringDropped);
        w.kv("ringCleared", (unsigned long)g_ringCleared);
        w.kv("c5Pair", g_c5Pair);
        w.kv("lateTagRepair", g_lateTagRepair);
        w.kv("lateRepaired", (unsigned long)g_lateRepaired);
        w.kv("c5Agree", (unsigned long)g_c5Agree);
        w.kv("c5Disagree", (unsigned long)g_c5Disagree);
        w.kv("c5Realigned", (unsigned long)g_c5Realigned);
        w.kv("c5Took", (unsigned long)g_c5Took);
        w.kv("c5Held", (unsigned long)g_c5Held);
        w.kv("c5Refused", (unsigned long)g_c5Refused);
        w.kv("pushSameEye", (unsigned long)g_pushSameEye);
        w.kv("c5Unknown", (unsigned long)g_c5Unknown);
        w.kv("c5Untagged", (unsigned long)g_c5Untagged);

        if (g_hooks.status) { w.obj("draw"); g_hooks.status(w); w.end_obj(); }
        // 41.1 (session 8): the capture cost under the shipped method too
        // (only the mono screen wrote it, so a default run had none).
        const dvr::capture::Cost c = dvr::capture::cost();
        w.obj("captureCost");
        w.kv("rtdUs", (int)c.rtdUs); w.kv("lockUs", (int)c.lockUs); w.kv("copyUs", (int)c.copyUs);
        w.kv("uploadUs", (int)c.uploadUs); w.kv("blitUs", (int)c.blitUs); w.kv("totalUs", (int)c.totalUs);
        w.kv("grabs", (int)c.grabsInWindow);
        w.end_obj();
    }

private:
    bool ensure_target(ID3D11Device* dev, uint32_t w, uint32_t h) {
        if (tex_ && w_ == w && h_ == h) return true;
        release_target();
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w; td.Height = h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // the runtime swapchain's family
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex_)) ||
            FAILED(dev->CreateRenderTargetView(tex_, nullptr, &rtv_)) ||
            FAILED(dev->CreateShaderResourceView(tex_, nullptr, &srv_))) {   // the trace's stage out reads it
            DVR_ERROR("reentry: output texture %ux%u failed", w, h);
            release_target();
            return false;
        }
        w_ = w; h_ = h;
        drawnOnce_ = false;
        DVR_INFO("reentry: eye texture %ux%u (RGBA) - each present's eye is captured into it", w, h);
        return true;
    }
    void release_target() {
        if (srv_) { srv_->Release(); srv_ = nullptr; }
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
        if (tex_) { tex_->Release(); tex_ = nullptr; }
        w_ = h_ = 0;
    }

    mutable char            note_[240] = "";
    dvr::gfx::BlitQuad      blit_;
    ID3D11Texture2D*        tex_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    ID3D11ShaderResourceView* srv_ = nullptr;
    uint32_t w_ = 0, h_ = 0;

    bool     drawnOnce_ = false;
    bool     taggedRecently_ = false;   // 41.1: the stream was stereo just now (the hold's precondition)
    int      heldRun_ = 0;              // consecutive untagged presents suppressed
    bool     armed_ = false;
    float    lastLeft_[3] = {0, 0, 0};
    bool     lastLeftOk_ = false;
    uint32_t lastSame_ = 0, lastTook_ = 0, lastHeld_ = 0, lastRefused_ = 0, lastRealign_ = 0, lastDis_ = 0;
    ArbState arb_;
    SingleTagState single_;                     // the previous present's c5 and the disagreement streak (reentry_pair.inc)
    // the stale-eye line's previous snapshot
    uint32_t lastStale_ = 0;
    bool     lastStaleInit_ = false;
    uint32_t lastGates_[kReentryGateCount] = {};
    uint32_t lastUntagged_ = 0;
    uint32_t lastNoFrame_ = 0;
    dvr::vr::PairProbe lastProbe_;
};

SequentialReentry g_reentry;

} // namespace

void set_reentry_hooks(const ReentryHooks& h) { g_hooks = h; }

void set_reentry_c5_pair(bool on) {
    if (on == g_c5Pair) return;
    g_c5Pair = on;
    DVR_INFO("reentry: c5 pairing %s - %s ([Stereo] C5Pair=%d for the next launch)", on ? "ON" : "off",
             on ? "each present's eye is checked against its c5 step from the previous present (the within-tick invariant), "
                  "the ring realigned on a disagreement streak"
                : "the ring's order is the only claim (the pre-41.1 behaviour: a single draw's present can eat the next tag)",
             on ? 1 : 0);
}
bool reentry_c5_pair() { return g_c5Pair; }

void set_reentry_single_tag(bool on) {
    g_singleTagRepair = on;
    DVR_INFO("reentry/single-tag: %s ([Stereo] SingleTagRepair=%d); confirmed adjacent R/0 only, pipelined capture required",
             on ? "ON" : "off", on ? 1 : 0);
}
bool reentry_single_tag() { return g_singleTagRepair; }

// VR-80 candidate F-late (reentry_pair.inc), default off.
void set_reentry_late_tag(bool on) {
    if (on == g_lateTagRepair) return;
    g_lateTagRepair = on;
    DVR_INFO("reentry: late-tag repair %s - %s ([Stereo] LateTagRepair=%d)", on ? "ON" : "off",
             on ? "an empty pop whose image the c5 step names owes that eye one tag; the next present removes it when its own "
                  "c5 confirms the other eye, instead of three presents of disagreement and a drain"
                : "an empty pop is refused and a late tag is left to the disagreement streak (the shipped behaviour)",
             on ? 1 : 0);
}
bool reentry_late_tag() { return g_lateTagRepair; }

void reentry_push_tag(int eyeSign, const float pos[3]) { reentry_push_tag_acct(eyeSign, pos, 0, 0); }

void reentry_push_tag_rec(int eyeSign, const float pos[3], uint32_t rec) { reentry_push_tag_acct(eyeSign, pos, rec, 0); }

void reentry_push_tag_acct(int eyeSign, const float pos[3], uint32_t rec, uint32_t acct) {
    push_tag(eyeSign, pos, rec, acct);   // reentry_pair.inc
}

// VR-80: the same push, carrying the game side's draw attempt id for the ledger.
void reentry_push_tag_draw(int eyeSign, const float pos[3], uint32_t rec, uint32_t acct, uint32_t draw) {
    push_tag(eyeSign, pos, rec, acct, draw);
}

void set_reentry_ledger(bool on) {
    const bool was = InterlockedExchange(&g_ledgerOn, on ? 1 : 0) != 0;
    if (on != was)
        DVR_INFO("ledger: %s - %s", on ? "ON" : "off",
                 on ? "one record per present, printed in bounded windows after a return to gameplay and around "
                      "overrides, drains and empty pops (%d back / %d after, %u windows at most), and a 10 s reconcile of "
                      "every tag that entered or left the ring" : "no records", kLedBack, kLedAfterEvent, kLedMaxDumps);
}
void reentry_ledger_arm(const char* why) {
    if (!InterlockedCompareExchange(&g_ledgerOn, 0, 0)) return;
    g_ledgerArmWhy = why ? why : "?";
    InterlockedExchange(&g_ledgerArmReq, 1);
}
void reentry_ledger_stance(int stance) { InterlockedExchange(&g_ledgerStance, stance); }

IStereo* create_reentry() { return &g_reentry; }

} // namespace dvr::stereo
