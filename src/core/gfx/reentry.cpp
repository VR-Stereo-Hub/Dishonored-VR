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
uint32_t      g_parityFlips = 0;   // 41.1: measured parity transitions (dot sign changes)
uint32_t      g_parityFixed = 0;   // 41.1: presents whose eye sign the guard inverted

uint32_t g_tagResynced = 0;

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
            // The game thread's skip counters can lag this present by a tick;
            // a second LEFT tag closing a pair (abortLeft) is the runtime's own
            // evidence of the same one-sided stream and needs no lag.
            const char* owner =
                gameSkips ? "the game side's pass-2 gates (a one-sided -1 stream: pass 1 tagged, pass 2 skipped)"
                : dAbortLeft ? "a second -1 tag closed the pair (the game side skipped pass 2; its counters lag a "
                               "tick, read forced/stall/state on the next line)"
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
                             "acqFail=%u waitFail=%u untaggedProj=%u abortLeft=%u eatenNoFrame=%u | method untagged "
                             "presents=%u noFrame=%u | stale submits so far L=%u R=%u | strict=%s",
                             eye, owner, pp.agePresL, pp.agePresR, dg[0], dg[1], dg[2], dg[3], dg[4], dg[5], dg[6], dg[7],
                             dAcq, dWait, pp.untaggedProj - lastProbe_.untaggedProj,
                             pp.abortLeft - lastProbe_.abortLeft, dEaten, dUntagged, dNoFrame, pp.stalePresL, pp.stalePresR,
                             dvr::vr::pair_strict() ? "on (the held eye was replaced by the fresh one)" : "off");
        }
        lastStale_ = stale; lastStaleInit_ = true;
        memcpy(lastGates_, gates, sizeof(lastGates_));
        lastProbe_ = pp;
        lastUntagged_ = g_tagUntagged;
        lastNoFrame_ = g_tagNoFrame;
    }

    bool end_frame(const FrameDevices& d, FrameOutput& out) override {
        if (g_hooks.poisoned && g_hooks.poisoned()) {
            DVR_ERROR("stereo: reentry POISONED by a second-draw fault - dropping to mono");
            armed_ = false;
            select("mono");
            return false;
        }
        stale_check();
        if (!d.dev9 || !d.dev11 || !d.ctx11) return false;
        if (!blit_.init(d.dev11)) return false;
        // The tag for the frame the game just drew, checked against its c5.
        Tag t;
        int eye = 0;
        float c5now[3];
        const bool haveC5 = dvr::camera::render_pos(c5now);
        if (pop_tag(t, haveC5 ? c5now : nullptr)) {
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
        const bool fresh = dvr::capture::grab(d.dev9, d.dev11, d.ctx11);
        ID3D11ShaderResourceView* src = dvr::capture::srv();
        if (!src) return false;
        const uint32_t w = dvr::capture::width(), h = dvr::capture::height();
        if (!ensure_target(d.dev11, w, h)) return false;
        if (fresh || !drawnOnce_) {
            blit_.draw(d.ctx11, src, rtv_, w, h);
            dvr::capture::read_done(d.ctx11);   // shared: the slot may be blitted into again only after this read
            if (OverlayDrawFn ov = overlay_draw()) ov(d.ctx11, rtv_, w, h);
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
                        // 41.1 (Dishonored): the PARITY LATCH. dot is measured at
                        // exactly +1 or -1 on this game (63 samples, 15 transitions
                        // over 124 s: |d| always 6.18 uu as wanted, off-right only
                        // ever 0 or 180 deg), so a negative dot is not noise - the
                        // present tagged +1 is carrying the frame drawn from the -1
                        // camera and the eyes are reversed until something shifts the
                        // ring back. Every pairing counter reads clean through it
                        // because they count TAGS, and the tags are all there and
                        // correctly paired; only the geometry can tell.
                        // The measurement is the authority: latch what it says and
                        // let the output sign follow. The latch is read below, per
                        // present, so a flip costs one pair (~12 ms) before it is
                        // corrected, against spells measured at 2-24 s.
                        if (dot < -0.5f || dot > 0.5f) {
                            const bool swapped = (dvr::stereo::parity_polarity() > 0) ? (dot < 0.0f) : (dot > 0.0f);
                            if (swapped != parityFlip_) {
                                parityFlip_ = swapped;
                                ++g_parityFlips;
                                if (dvr::stereo::parity_guard())
                                    DVR_INFO("reentry: parity %s (dot %+.3f) - the guard is ON, the eye sign "
                                             "handed to the runtime is %s from here; %lu flip(s) this run",
                                             swapped ? "SWAPPED" : "restored", dot,
                                             swapped ? "INVERTED" : "passed through",
                                             (unsigned long)g_parityFlips);
                                else
                                    DVR_WARN("reentry: parity %s (dot %+.3f) - the eyes are %s and the guard "
                                             "is OFF ([Stereo] ParityGuard=1 or `stereo parity on` corrects "
                                             "it); %lu flip(s) this run",
                                             swapped ? "SWAPPED" : "restored", dot,
                                             swapped ? "REVERSED" : "correct again",
                                             (unsigned long)g_parityFlips);
                            }
                        }
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
        // The parity latch corrects the sign the runtime is handed; the
        // MEASUREMENT above always runs on the raw delivered tag, so the
        // latch keeps tracking ground truth instead of chasing its own tail.
        int outEye = (dvr::stereo::parity_guard() && parityFlip_) ? -delivered : delivered;
        if (dvr::stereo::eye_swap()) outEye = -outEye;   // the unconditional A/B, applied last
        if (outEye != delivered) ++g_parityFixed;
        out.tex = tex_;
        out.eyeSign = outEye;
        out.w = w; out.h = h;
        // The runtime pops exactly one tag per present in on_present_end,
        // right after this returns; a 0 pushes nothing (mono path).
        if (outEye != 0) dvr::vr::sr_push_eye(outEye);
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
        w.kv("parityGuard", dvr::stereo::parity_guard());
        w.kv("parityFlips", (unsigned long)g_parityFlips);
        w.kv("paritySwappedNow", parityFlip_);
        w.kv("parityFixed", (unsigned long)g_parityFixed);
        w.kv("ringDropped", (unsigned long)g_ringDropped);
        w.kv("ringCleared", (unsigned long)g_ringCleared);
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
            FAILED(dev->CreateRenderTargetView(tex_, nullptr, &rtv_))) {
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
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
        if (tex_) { tex_->Release(); tex_ = nullptr; }
        w_ = h_ = 0;
    }

    mutable char            note_[240] = "";
    dvr::gfx::BlitQuad      blit_;
    ID3D11Texture2D*        tex_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    uint32_t w_ = 0, h_ = 0;
    bool     drawnOnce_ = false;
    bool     parityFlip_ = false;        // 41.1: the measured parity (true = the raw tags are reversed)
    bool     taggedRecently_ = false;   // 41.1: the stream was stereo just now (the hold's precondition)
    int      heldRun_ = 0;              // consecutive untagged presents suppressed
    bool     armed_ = false;
    float    lastLeft_[3] = {0, 0, 0};
    bool     lastLeftOk_ = false;
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
