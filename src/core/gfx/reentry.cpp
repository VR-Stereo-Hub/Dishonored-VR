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

// The tag ring: game thread pushes (per draw), present thread pops (per
// present). Power-of-two, SPSC, self-healing on a skew.
constexpr uint32_t kRing = 8;
struct Tag { int eye; bool posOk; float pos[3]; };
Tag           g_ring[kRing];
volatile LONG g_ringHead = 0, g_ringTail = 0;
uint32_t      g_ringDropped = 0, g_ringCleared = 0, g_tagMismatch = 0, g_tagOk = 0, g_tagUntagged = 0;
uint32_t      g_tagNoFrame = 0;   // 41.1 (session 8): tagged presents whose grab delivered no frame (no tag pushed)

uint32_t g_tagResynced = 0;
// 41.1 (session 9): the within-tick invariant. Between pass 1 and pass 2 the
// world does not tick, so the ONLY thing that moves the camera is the writer's
// eye: c5(pass 2) - c5(pass 1) = -ipd*scale along the camera's right row (the
// field holds the position, c5 negates it), nothing along forward or up. A
// present whose c5 sits exactly there from the previous present's IS a pass-2
// present, whatever the ring says; one whose c5 sits exactly +ipd*scale along
// right is a pass 1 after a still pass 2. The ring's order claim is checked
// against that measurement every present: a disagreement is counted, and
// three in a row drain the ring to the next expected tag (a tag eaten by a present that
// drew nothing, or pushed by a draw that never presented - both happen, the
// menu's draws outnumber its presents). Measured on the simulator: the tags
// swapped across a re-arm and within a second of the first arming (the
// frameid line's side check), the picture agreeing.
bool     g_c5Pair = true;              // [Stereo] C5Pair=1; `reentry c5pair on|off`
uint32_t g_c5Agree = 0, g_c5Disagree = 0, g_c5Realigned = 0, g_c5Verdicts = 0, g_c5Unknown = 0, g_c5Untagged = 0;
// 41.1 (session 10): what the measurement DID with a disagreement, and the
// ground truth it is judged by. g_c5Took/g_c5Held split the override by arm;
// g_c5Refused counts the manufactured tags no longer invented on an empty
// ring; g_pushSameEye is the fault ITSELF - two consecutive presents pushed
// the same eye to the runtime, which is what the runtime reports as abortLeft
// one stage later. It is counted at the push, on the present thread, with no
// lag and no inference: if the stale line's owner is right, this moves with it.
uint32_t g_c5Took = 0, g_c5Held = 0, g_c5Refused = 0, g_pushSameEye = 0;
int      g_lastPushedEye = 0;

// Pop the tag for THIS present, strictly in push order. The game thread runs
// up to a frame ahead of the render thread (UE3's OneFrameThreadLag), so two
// pairs can sit in the ring legitimately; a depth beyond that is a skew and
// the ring is cleared. The ORDER pairs the eyes: one push per draw, one pop
// per present. (The 2026-09-03 headset run: re-aligning by camera position
// mis-paired a walking player - the engine moves the camera after the tick's
// write - and showed both frames in both eyes. Position is telemetry now.)
bool pop_tag(Tag& out, const float* c5) {
    (void)c5;
    const LONG tail = g_ringTail;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    if (tail == head) return false;
    if (head - tail > 6) {
        InterlockedExchange(&g_ringTail, head);
        ++g_ringCleared;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
                         "reentry: tag ring skewed (depth %ld) - cleared, mono until the next pair", head - tail);
        return false;
    }
    out = g_ring[tail & (kRing - 1)];
    InterlockedExchange(&g_ringTail, tail + 1);
    return true;
}

// The eye of the tag the next pop would return (false = empty).
bool peek_tag(int& eye) {
    const LONG tail = g_ringTail;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    if (tail == head) return false;
    eye = g_ring[tail & (kRing - 1)].eye;
    return true;
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
        if (g_hooks.poisoned && g_hooks.poisoned()) {
            DVR_ERROR("stereo: reentry POISONED by a second-draw fault - dropping to mono");
            armed_ = false;
            select("mono");
            return false;
        }
        stale_check();
        dvr::frameid::begin_present();   // 41.1 (session 9): the previous present's trace closes, its pairs are judged
        if (!d.dev9 || !d.dev11 || !d.ctx11) return false;
        if (!blit_.init(d.dev11)) return false;
        // The tag for the frame the game just drew, checked against its c5.
        Tag t = {0, false, {0.0f, 0.0f, 0.0f}};   // 41.1: the c5 arm can tag a present the ring never filled
        int eye = 0;
        float c5now[3];
        const bool haveC5 = dvr::camera::render_pos(c5now);
        bool tagged = pop_tag(t, haveC5 ? c5now : nullptr);
        // The within-tick invariant (see g_c5Pair): what this present's c5
        // says about its pass, before the ring's claim is read.
        int inv = 0;
        float along = 0.0f, other = 0.0f;
        {
            float bf[3], br[3], bu[3];
            const float ipd = dvr::camera::ipd_m() * dvr::camera::world_scale();
            if (haveC5 && prevC5Ok_ && ipd > 1.0f && dvr::camera::last_basis(bf, br, bu)) {
                const float s[3] = {c5now[0] - prevC5_[0], c5now[1] - prevC5_[1], c5now[2] - prevC5_[2]};
                along = s[0] * br[0] + s[1] * br[1] + s[2] * br[2];
                const float o[3] = {s[0] - along * br[0], s[1] - along * br[1], s[2] - along * br[2]};
                other = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
                if (fabsf(along + ipd) < 0.35f * ipd && other < 0.5f * ipd) inv = +1;        // pass 2 after pass 1
                else if (fabsf(along - ipd) < 0.35f * ipd && other < 0.5f * ipd) inv = -1;   // pass 1 after a still pass 2
            }
            if (haveC5) { memcpy(prevC5_, c5now, sizeof(prevC5_)); prevC5Ok_ = true; }
        }
        // The measurement is the pairing; the ring's order is the fallback for
        // the presents the invariant cannot name (a pass 1 after a moving
        // pass 2) and is kept aligned by the measurement (measured on the
        // simulator: the ring skews on its own in plain gameplay, ~2 per 25 s,
        // an extra present popping the next draw's tag; with the order alone
        // each skew swapped the eyes until the next).
        //
        // 41.1 (session 10): THE TWO ARMS ARE NOT EQUALLY TRUSTWORTHY, and
        // trusting them equally is what made the stale RIGHT eye.
        //   inv=+1 ("pass 2 after pass 1") compares two draws with NO world
        //     tick between them. The step is exactly -ipd by construction.
        //     Robust: it may override the ring on its own.
        //   inv=-1 ("pass 1 after a still pass 2") is the ONLY arm that reasons
        //     ACROSS a world tick, and is valid solely while the player is near
        //     still. A gently moving player (turning in place, decelerating,
        //     crouch-walk) parks the tick's travel inside the +-0.35*ipd window
        //     and this arm then names a genuine pass-2 present a pass 1.
        // A wrong -1 adds a LEFT and drops a RIGHT: two -1 reach the runtime
        // (abortLeft), the left swapchain is written twice and the right is not
        // written at all - `STALE R EYE ... ages L=0 R=2`, `stale submits L=0
        // R=20`, and not one game-side gate moves, because the game side DID
        // run pass 2 and DID push its +1. The fragile arm must therefore agree
        // with the ring, or defer to it and let the streak realign.
        if (g_c5Pair && inv != 0) {
            const bool robust = (inv == +1);   // within-tick: no world tick to cross
            if (tagged && t.eye != 0) {
                ++g_c5Verdicts;
                if (t.eye == inv) { ++g_c5Agree; c5Streak_ = 0; t.eye = inv; }
                else {
                    ++g_c5Disagree;
                    if (robust) { ++g_c5Took; t.eye = inv; }
                    else {
                        // The cross-tick arm loses to the ring on its own. The
                        // streak below is what promotes a real skew.
                        ++g_c5Held;
                        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 3000,
                                         "reentry: c5 says pass 1 (step %+.2f along right, %.2f other; ipd*scale %.2f) but "
                                         "the ring says %+d - the cross-tick arm DEFERS to the ring (held %u, took %u, "
                                         "%u agree / %u disagree); three in a row still realign",
                                         along, other, dvr::camera::ipd_m() * dvr::camera::world_scale(), t.eye,
                                         g_c5Held, g_c5Took, g_c5Agree, g_c5Disagree);
                    }
                    if (++c5Streak_ >= 3) {
                        // The ring is off: drain it to the tag the NEXT present
                        // must pop (the other eye of this measured one), so a
                        // backlog of any depth (a method re-select leaves up to
                        // a ring of stale tags) aligns in one present.
                        Tag t2;
                        bool popped = false;
                        for (uint32_t k = 0; k < kRing; ++k) {
                            int next = 0;
                            if (!peek_tag(next) || next == -inv) break;
                            if (pop_tag(t2, nullptr)) popped = true; else break;
                        }
                        ++g_c5Realigned;
                        c5Streak_ = 0;
                        t.eye = inv;   // the streak earned the override, either arm
                        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                                         "reentry: the tag ring skewed against the draws - three presents in a row whose c5 "
                                         "step (%+.2f along right, %.2f other; ipd*scale %.2f) named the other pass; realigned "
                                         "by one pop (%s) - realigned %u times, %u agree / %u disagree so far (the eyes "
                                         "followed the measurement throughout; the order alone would have swapped them)",
                                         along, other, dvr::camera::ipd_m() * dvr::camera::world_scale(),
                                         popped ? "a tag dropped" : "the ring was empty", g_c5Realigned, g_c5Agree, g_c5Disagree);
                    }
                }
                tagged = true;
            } else {
                // Nothing to override: the ring was empty, or it delivered the
                // 0 tag a SINGLE gameplay draw pushes (scene_draw's one push
                // per draw). Only the within-tick arm may name an eye out of
                // nothing - a manufactured -1 here is a LEFT that no draw ever
                // pushed, and it is invisible downstream because setting
                // tagged=true is exactly what keeps `method untagged presents`
                // reading 0. An unnamed present goes out untagged and honest;
                // [Stereo] HoldUntagged covers the one-frame mono flick.
                ++g_c5Untagged;
                if (robust) { t.eye = inv; tagged = true; }
                else { ++g_c5Refused; t.eye = 0; }
            }
        } else if (inv == 0 && tagged && t.eye != 0) ++g_c5Unknown;
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
        dvr::capture::set_pending_tag(eye);
        {   // 41.1 (session 9): the camera of the draw the grab will take, and its right row
            float bf[3], br[3], bu[3];
            const bool basisOk = dvr::camera::last_basis(bf, br, bu);
            dvr::frameid::note_c5(c5now, haveC5, br, basisOk);
        }

        const bool fresh = dvr::capture::grab(d.dev9, d.dev11, d.ctx11);
        ID3D11ShaderResourceView* src = dvr::capture::srv();
        if (!src) return false;
        const uint32_t w = dvr::capture::width(), h = dvr::capture::height();
        if (!ensure_target(d.dev11, w, h)) return false;
        if (fresh || !drawnOnce_) {
            blit_.draw(d.ctx11, src, rtv_, w, h);
            // 41.2 (VR-31): our own hands, over the game image and under the
            // F10 panel. The eye is the tag of the pixels JUST blitted, which
            // is NOT `eye` (the eye the next game draw will render) - one line
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
        return true;
    }

    void on_reset() override { dvr::capture::on_reset(); }

    void shutdown() override {
        if (armed_) {
            armed_ = false;
            if (g_hooks.set_armed) g_hooks.set_armed(false);
            DVR_INFO("stereo: reentry disarmed - the call site is restored at the next script dispatch");
        }
        InterlockedExchange(&g_ringTail, InterlockedCompareExchange(&g_ringHead, 0, 0));
        release_target();
        blit_.shutdown();
        drawnOnce_ = false;
        lastLeftOk_ = false;
        prevC5Ok_ = false;
        c5Streak_ = 0;
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
    float    prevC5_[3] = {0, 0, 0};   // the previous present's c5 (the within-tick invariant)
    uint32_t lastSame_ = 0, lastTook_ = 0, lastHeld_ = 0, lastRefused_ = 0, lastRealign_ = 0, lastDis_ = 0;
    bool     prevC5Ok_ = false;
    uint32_t c5Streak_ = 0;
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

void reentry_push_tag(int eyeSign, const float pos[3]) {

    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0), tail = InterlockedCompareExchange(&g_ringTail, 0, 0);
    if (head - tail >= (LONG)kRing) { ++g_ringDropped; return; }   // no consumer (no present) or stalled
    Tag& t = g_ring[head & (kRing - 1)];
    t.eye = eyeSign;
    t.posOk = pos != nullptr;
    if (pos) memcpy(t.pos, pos, sizeof(t.pos));
    InterlockedExchange(&g_ringHead, head + 1);
}

IStereo* create_reentry() { return &g_reentry; }

} // namespace dvr::stereo
