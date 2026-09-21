// game/dishonored/swing_core.h - the motion sword's decision core (VR-37).
//
// What it is for: the player swings the right controller and Corvo swings the
// sword. This header decides WHEN a hand movement counts as a swing. It is pure:
// no engine objects, no globals, no clock of its own. Hand and head positions and
// a timestamp go in, a verdict comes out, so tools/swing-core-host.ps1 drives
// exactly the code the headset does. Everything that knows the game (the gates,
// the trigger pulse, the haptic, the log) lives in the adapter, melee.cpp.
//
// Three detectors share one arm latch and one cooldown:
//
//   kEdge     the feel of the sibling BioShock mod's wrench swing. Raw 2-sample
//             speed, the head's own movement subtracted, and the attack fires the
//             instant the speed crosses EdgeSpeed - the game's own wind-up then
//             lands the hit where the arm is going. One physical swing crosses the
//             threshold on the way up AND on the way down, so a fire clears the
//             arm latch and only a sample below RearmSpeed sets it again; the
//             cooldown separately bounds a shake.
//   kSustain  the detector this mod shipped until VR-37, moved here verbatim and
//             kept as the live A/B: EMA-smoothed room-space speed, and a fire only
//             once a run has lasted SustainMs AND covered SustainDistM.
//   thrust    the sneak kill (VR-155), alongside kEdge. A player creeping up behind
//             a guard stabs, and a stab is short, slow and straight: about 1.5 to
//             2 m/s over 20 to 30 cm, which never crosses a slash threshold, while
//             lowering that threshold would turn every reach into an attack in the
//             middle of a stealth approach. So it is its own shape: the hand
//             EXTENDING away from the shoulder, mostly in a straight line, mostly
//             forward, and only while the adapter says it is armed (sneaking).
//             The game has no separate input for the kill; it is the attack in
//             context, so a thrust presses what a slash presses.
//
// POSITION in, not speed in. The simulated swing (`swing sim`) synthesises
// positions too, so the differencing, the dt window and the head-relative
// subtraction are covered flat and in the host tests rather than skipped.
#pragma once
#include <cmath>
#include <cstdint>

namespace dvr::swing {

enum Detector : int { kSustain = 0, kEdge = 1 };
enum Fired : int { kFiredNone = 0, kFiredSlash = 1, kFiredStab = 2 };
enum StabReject : int { kStabOk = 0, kStabTravel, kStabRatio, kStabForward, kStabStart };
// The sneak kill has two shapes, because it depends how the blade sits in the hand.
//   kPlunge  the sword is held in a REVERSE (ice-pick) grip, so the kill is a fist
//            raised to about the shoulder and driven DOWN, a little forward - which
//            is also what the game's own from-behind animation does. It must START
//            high: reaching down for loot starts low, and that one fact rejects it.
//   kThrust  a forward-grip stab straight out from the shoulder, kept as the A/B.
enum StabStyle : int { kThrust = 0, kPlunge = 1 };

// Closed gates, filled by the adapter per sample. The core needs to know only
// that something is closed; the adapter owns what each bit means and its text.
enum Gate : uint32_t {
    kGateOff      = 1u << 0,   // the gesture is switched off
    kGateGameplay = 1u << 1,   // not a gameplay view (menu, load, cutscene)
    kGateSword    = 1u << 2,   // the sword is not the item in the right hand
    kGateBumper   = 1u << 3,   // a grip is held (the power wheel, block)
    kGateOverlay  = 1u << 4,   // the F10 overlay is up
    kGateBody     = 1u << 5,   // the game owns the body (takedown, climb, ...)
    kGateTracking = 1u << 6,   // sustain only: the weapon is not following the hand
    kGateUiMute   = 1u << 7,   // sustain only: a UI event in the last 3 s
};

enum Block : int { kBlockNone = 0, kBlockGate, kBlockRearm, kBlockCooldown };

// A sample pair closer than this amplifies millimetre jitter into metres per
// second; one further apart than this belongs to two different situations (an
// alt-tab, a level load, a session hiccup) and must not produce a speed at all.
constexpr double kMinDtMs = 4.0;
constexpr double kMaxDtMs = 100.0;
// No arm moves a controller this fast. A hand that comes back from behind the
// body is re-acquired somewhere else in one sample, which reads as tens of
// metres per second: that is a tracking jump, and it re-seeds instead of firing.
constexpr float kMaxSpeed = 20.0f;
// The hand has slowed down when this many samples IN A ROW are below the re-arm
// level. One is not enough: a runtime can hand over the same pose twice (the
// simulator does, measured 2026-09-20: a new hand generation with an unchanged
// position about every third sample), and that lone zero in the middle of a
// swing re-armed the latch and logged one BLOCKED line per repeat. A real hand
// that has slowed stays slow for many samples, so the cost is one frame.
constexpr int kRearmSamples = 2;

struct Config {
    int   detector     = kEdge;
    float edgeSpeed    = 3.6f;     // m/s, fire on the crossing
    float rearmSpeed   = 1.0f;     // m/s, the hand must slow below this to re-arm
    float cooldownMs   = 300.0f;   // between fires, both detectors
    bool  headRel      = true;     // edge: subtract the head's own movement
    bool  median       = true;     // edge: decide on the median of the last 3 speeds
    // edge: how far the hand must have travelled in this hump before a crossing may
    // fire (VR-170). 0 = off, and off is what ships: a real swing has covered only
    // about 7 cm when it crosses the threshold, so a value here is a measurement to
    // take from the hump census, not a number to guess. A shortfall DELAYS the fire
    // while the speed stays up; it never blocks and never latches.
    float edgeTravelM  = 0.0f;
    float sustainSpeed = 1.8f;     // m/s, the sustain gate
    float sustainMs    = 120.0f;
    float sustainDistM = 0.25f;
    bool  stab         = false;    // the thrust detector (edge mode only)
    float stabSpeed    = 1.5f;     // m/s of EXTENSION that starts a thrust
    float stabTravelM  = 0.20f;    // extension a thrust must gain...
    float stabWindowMs = 400.0f;   // ...within this long
    float stabRatio    = 0.75f;    // extension gained / path travelled: how straight
    float stabForward  = 0.5f;     // dot(thrust direction, where the head faces, flattened)
    float shoulder[3]  = { 0.17f, 0.22f, 0.04f };   // right, down, back of the head, metres
    int   stabStyle    = kPlunge;
    float stabStartBelowM = 0.05f; // plunge: how far BELOW the shoulder line the hand may start
};

// The two levels can be typed in either order without the latch becoming
// unreachable: the re-arm level is never above 0.9 x the threshold.
inline float effective_rearm(const Config& c) {
    const float cap = 0.9f * c.edgeSpeed;
    return c.rearmSpeed < cap ? c.rearmSpeed : cap;
}

struct Sample {
    float    hand[3] = {};       // metres, tracking space
    float    head[3] = {};
    double   tMs = 0.0;          // the sample's own time, not the wall clock
    bool     handValid = false;
    bool     headValid = false;
    uint32_t closed = 0;         // Gate bits that are closed right now
    float    headFwd[2] = { 0.0f, -1.0f };   // where the head faces, flattened: (x, z), unit
    bool     stabArmed = false;  // the adapter's verdict that a thrust may count (sneaking)
};

struct Verdict {
    bool     sampled = false;    // a speed was produced from this sample
    bool     jump = false;       // the sample was a tracking jump and was discarded
    int      fired = kFiredNone;
    int      block = kBlockNone; // set ONCE per gesture, not once per sample
    uint32_t closed = 0;         // the closed gates at the block
    float    speed = 0.0f;       // the speed the decision used
    float    rawSpeed = 0.0f;    // edge: this sample's own 2-sample speed, before the median
    float    roomSpeed = 0.0f;   // the raw room-space speed, for comparison
    float    cooldownLeftMs = 0.0f;
    bool     flick = false;      // sustain: a run ended without qualifying
    float    runMs = 0.0f, runDistM = 0.0f, runPeak = 0.0f;
    // thrust: radial is this sample's extension speed (median of three). The rest
    // describe a run at the moment it fired or was rejected.
    float    radial = 0.0f;
    int      stabReject = kStabOk;
    float    stabTravel = 0.0f, stabRatio = 0.0f, stabForward = 0.0f, stabMs = 0.0f, stabPeak = 0.0f;
    float    stabStartDy = 0.0f; // plunge: where the hand started, metres above (+) the shoulder line
    // edge, the hump census (VR-170). A hump is one excursion of the decision speed
    // above the re-arm level: it starts at the first sample at or above it and ends
    // where the latch re-arms. humpEnd is set on that ONE sample, so a player's
    // whole session reads as one line per hand movement: the swings that fired, and
    // - the half nobody could see before - the movements that did not, with how
    // fast and how far they were. That distribution is what a threshold is set from.
    bool     humpEnd = false;
    float    humpPeak = 0.0f;    // the highest decision speed inside it, m/s
    float    humpTravel = 0.0f;  // path covered inside it, head movement subtracted when HeadRel is on
    float    humpMs = 0.0f;
    int      humpFired = kFiredNone;
    int      humpBlock = kBlockNone;
    uint32_t humpClosed = 0;     // the closed gates at that block
    float    travelAtFire = 0.0f;   // edge: the hump's travel at the moment it fired
    // The hump did not end by slowing down: tracking was lost, jumped, or the samples
    // stopped for longer than kMaxDtMs (a hitch). What it had is still reported -
    // measured on the simulator 2026-09-21, a 135 ms game-thread stall landed inside
    // a swing and the movement vanished from the census without a line.
    bool     humpCut = false;
};

inline bool finite3(const float* v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

class Core {
public:
    void reset() { *this = Core{}; }
    bool armed() const { return armed_; }
    double last_fire_ms() const { return haveFire_ ? lastFireMs_ : -1.0; }

    Verdict feed(const Sample& s, const Config& c) {
        Verdict v{};
        if (!s.handValid || !finite3(s.hand) || !std::isfinite(s.tMs)) {
            cut_hump(v);
            have_ = false; forget(); stabRun_ = false;   // lost tracking: re-seed
            return v;
        }
        const bool headOk = s.headValid && finite3(s.head);
        if (have_) {
            const double dt = s.tMs - lastMs_;
            if (dt >= 0.0 && dt < kMinDtMs) return v;   // keep the older seed
            if (dt >= kMinDtMs && dt <= kMaxDtMs) {
                float d[3] = { s.hand[0] - lastHand_[0], s.hand[1] - lastHand_[1],
                               s.hand[2] - lastHand_[2] };
                const float roomStep = len(d);
                if (roomStep / (float)(dt * 0.001) > kMaxSpeed) {
                    v.jump = true; v.roomSpeed = roomStep / (float)(dt * 0.001);
                    cut_hump(v);
                    forget();
                    seed(s, headOk);
                    return v;
                }
                if (c.headRel && headOk && lastHeadOk_) {
                    d[0] -= s.head[0] - lastHead_[0];
                    d[1] -= s.head[1] - lastHead_[1];
                    d[2] -= s.head[2] - lastHead_[2];
                }
                const float sec = (float)(dt * 0.001);
                v.sampled = true;
                v.roomSpeed = roomStep / sec;
                if (c.detector == kEdge) {
                    v.rawSpeed = len(d) / sec;
                    v.speed = c.median ? median3(v.rawSpeed) : v.rawSpeed;
                    edge(s, c, len(d), v);
                    if (c.stab && headOk) thrust(s, c, d, v); else stabRun_ = false;
                    if (hump_ && v.fired) humpFired_ = v.fired;   // a thrust inside the hump counts as its fire
                    if (hump_ && v.block) { humpBlock_ = v.block; humpClosed_ = v.closed; }
                }
                else                     sustain(s, c, roomStep, v);
            }
            else { cut_hump(v); forget(); }   // a gap beyond kMaxDtMs, or time running backwards: re-seed below
        }
        seed(s, headOk);
        return v;
    }

private:
    // One sample can lie in either direction. Measured 2026-09-20: a repeated pose
    // reads 0 and the sample after it carries two frames of travel in one frame's
    // time, so a 1.6 m/s reach showed single samples of 2.9 and 3.2 - and a lone
    // tracking-noise spike on a headset has the same shape. The median of the last
    // three speeds ignores any single outlier and costs one sample of latency
    // (11 ms at 90 Hz). A re-seed zeroes the history, so the first reading after
    // one can never fire by itself.
    void forget() { ring_[0] = ring_[1] = ring_[2] = 0.0f; sring_[0] = sring_[1] = sring_[2] = 0.0f; }
    // A re-seed ends the hump in progress where the hand was last SEEN: its travel
    // must not be measured across a gap, and it must not vanish either.
    void cut_hump(Verdict& v) {
        if (!hump_) return;
        hump_ = false;
        v.humpEnd = true; v.humpCut = true; v.humpPeak = humpPeak_; v.humpTravel = humpPath_;
        v.humpMs = (float)(lastMs_ - humpStartMs_);
        v.humpFired = humpFired_; v.humpBlock = humpBlock_; v.humpClosed = humpClosed_;
    }
    float median3(float x) {
        ring_[ringI_] = x; ringI_ = (ringI_ + 1) % 3;
        const float a = ring_[0], b = ring_[1], c = ring_[2];
        return a > b ? (b > c ? b : a > c ? c : a) : (a > c ? a : b > c ? c : b);
    }
    void seed(const Sample& s, bool headOk) {
        lastHand_[0] = s.hand[0]; lastHand_[1] = s.hand[1]; lastHand_[2] = s.hand[2];
        lastHeadOk_ = headOk;
        if (headOk) { lastHead_[0] = s.head[0]; lastHead_[1] = s.head[1]; lastHead_[2] = s.head[2]; }
        lastMs_ = s.tMs;
        have_ = true;
    }
    static float len(const float* d) { return std::sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]); }

    float cooldown_left(const Sample& s, const Config& c) const {
        if (!haveFire_) return 0.0f;
        const double left = lastFireMs_ + (double)c.cooldownMs - s.tMs;
        return left > 0.0 ? (float)left : 0.0f;
    }
    void fire(const Sample& s, Verdict& v) {
        armed_ = false;
        blockLatched_ = true;                     // the rest of this swing is silent
        lastFireMs_ = s.tMs; haveFire_ = true;
        v.fired = kFiredSlash;
    }
    void block(int why, const Sample& s, Verdict& v) {
        if (blockLatched_) return;                // one verdict per gesture
        blockLatched_ = true;
        v.block = why; v.closed = s.closed;
    }

    void edge(const Sample& s, const Config& c, float step, Verdict& v) {
        // Re-arm on the way down, UNCONDITIONALLY: a gate that closes mid-swing
        // must not leave the latch stuck and eat the next swing too.
        if (v.speed < effective_rearm(c)) {
            if (++slow_ >= kRearmSamples) {
                armed_ = true; blockLatched_ = false;
                if (hump_) {                          // the hump ends where the latch re-arms
                    hump_ = false;
                    v.humpEnd = true; v.humpPeak = humpPeak_; v.humpTravel = humpPath_;
                    v.humpMs = (float)(s.tMs - humpStartMs_);
                    v.humpFired = humpFired_; v.humpBlock = humpBlock_; v.humpClosed = humpClosed_;
                }
            }
        } else {
            slow_ = 0;
            if (!hump_) {
                hump_ = true; humpStartMs_ = lastMs_; humpPath_ = 0.0f; humpPeak_ = 0.0f;
                humpFired_ = kFiredNone; humpBlock_ = kBlockNone; humpClosed_ = 0;
            }
        }
        if (hump_) { humpPath_ += step; if (v.speed > humpPeak_) humpPeak_ = v.speed; }
        if (v.speed < c.edgeSpeed) return;
        // The travel guard: fast enough, but not far enough YET. No verdict and no
        // latch - the same swing fires a sample or two later once it has the distance.
        if (c.edgeTravelM > 0.0f && humpPath_ < c.edgeTravelM) return;
        v.cooldownLeftMs = cooldown_left(s, c);
        if (s.closed)                      block(kBlockGate, s, v);
        else if (!armed_)                  block(kBlockRearm, s, v);
        else if (v.cooldownLeftMs > 0.0f)  block(kBlockCooldown, s, v);
        else                             { fire(s, v); v.travelAtFire = humpPath_; }
    }

    // The thrust. `d` is this sample's hand displacement, head movement already
    // subtracted when HeadRel is on - which is what makes a step forward WITH the
    // stab count as arm extension only, and a lean back not count as a stab.
    //
    // The shoulder is modelled from the head, YAW ONLY (a nod or a tilt must not
    // swing it), and is used for ONE thing: the direction "away from the body".
    // The extension itself is measured from the hand's own displacement along
    // that direction, never as a change in hand-to-shoulder distance - that way a
    // head that turns with the hand held still moves the modelled shoulder and
    // produces no extension at all.
    void thrust(const Sample& s, const Config& c, const float* d, Verdict& v) {
        const float fx = s.headFwd[0], fz = s.headFwd[1];
        const float sh[3] = { s.head[0] + (-fz) * c.shoulder[0] - fx * c.shoulder[2],
                              s.head[1] - c.shoulder[1],
                              s.head[2] + ( fx) * c.shoulder[0] - fz * c.shoulder[2] };
        float u[3];
        if (c.stabStyle == kPlunge) {
            // Down, tilted 20 degrees the way the head faces: a fixed axis, because
            // a plunge is aimed by gravity and the body, not by where the hand is.
            u[0] = fx * 0.342f; u[1] = -0.940f; u[2] = fz * 0.342f;
        } else {
            u[0] = s.hand[0] - sh[0]; u[1] = s.hand[1] - sh[1]; u[2] = s.hand[2] - sh[2];
            const float ul = len(u);
            if (ul < 0.05f) { stabRun_ = false; return; }  // the hand is AT the shoulder: no direction
            u[0] /= ul; u[1] /= ul; u[2] /= ul;
        }
        const float step = d[0]*u[0] + d[1]*u[1] + d[2]*u[2];
        const double dt = s.tMs - lastMs_;
        const float raw = step / (float)(dt * 0.001);
        // the same one-bad-sample argument as the slash, on its own history
        sring_[sringI_] = raw; sringI_ = (sringI_ + 1) % 3;
        const float a = sring_[0], b = sring_[1], e = sring_[2];
        v.radial = c.median ? (a > b ? (b > e ? b : a > e ? e : a) : (a > e ? a : b > e ? e : b)) : raw;

        if (v.fired) { stabRun_ = false; return; }          // a slash took this gesture
        if (!s.stabArmed) { stabRun_ = false; return; }     // standing up in a fight: silent
        if (!stabRun_) {
            // Not while the latch is down: the hand is still finishing a gesture that
            // already attacked, and a run begun here only ends as a stray REJECTED line.
            if (!armed_ || v.radial < c.stabSpeed) return;
            stabRun_ = true; stabDone_ = false; stabStartMs_ = s.tMs - dt;
            // where the hand was BEFORE this sample moved it, against the shoulder line
            stabStartDy_ = (s.hand[1] - d[1]) - sh[1];
            stabTravel_ = 0.0f; stabPath_ = 0.0f; stabPeak_ = 0.0f;
            stabNet_[0] = stabNet_[1] = stabNet_[2] = 0.0f;
        }
        stabTravel_ += step; stabPath_ += len(d);
        stabNet_[0] += d[0]; stabNet_[1] += d[1]; stabNet_[2] += d[2];
        if (v.radial > stabPeak_) stabPeak_ = v.radial;
        const float ms = (float)(s.tMs - stabStartMs_);
        const bool over = v.radial < 0.5f * c.stabSpeed || ms > c.stabWindowMs;
        if (stabDone_) { if (over) stabRun_ = false; return; }
        auto describe = [&](int why) {
            const float nl = len(stabNet_);
            v.stabReject = why; v.stabTravel = stabTravel_; v.stabMs = ms; v.stabPeak = stabPeak_;
            v.stabRatio = stabPath_ > 1e-4f ? stabTravel_ / stabPath_ : 0.0f;
            // how well the whole movement followed its axis: the head's heading for a
            // thrust, the down-and-forward axis for a plunge
            v.stabForward = nl <= 1e-4f ? 0.0f : c.stabStyle == kPlunge
                ? (stabNet_[0] * u[0] + stabNet_[1] * u[1] + stabNet_[2] * u[2]) / nl
                : (stabNet_[0] * fx + stabNet_[2] * fz) / nl;
            v.stabStartDy = stabStartDy_;
        };
        if (stabTravel_ >= c.stabTravelM) {
            describe(kStabOk);
            stabDone_ = true;
            if (c.stabStyle == kPlunge && stabStartDy_ < -c.stabStartBelowM) v.stabReject = kStabStart;
            else if (v.stabRatio < c.stabRatio)     v.stabReject = kStabRatio;
            else if (v.stabForward < c.stabForward) v.stabReject = kStabForward;
            else if (!armed_) { /* the tail of a gesture that already attacked: silent */ }
            else {
                v.cooldownLeftMs = cooldown_left(s, c);
                blockLatched_ = false;               // a thrust is its own gesture, with its own one verdict
                if (s.closed)                      block(kBlockGate, s, v);
                else if (v.cooldownLeftMs > 0.0f)  block(kBlockCooldown, s, v);
                else { fire(s, v); v.fired = kFiredStab; }
            }
        } else if (over) {
            describe(kStabTravel);
            stabRun_ = false;
        }
    }

    // The pre-VR-37 detector, verbatim apart from reporting why it declined.
    // A run starts when the smoothed speed crosses the gate, accumulates travel
    // while above it, survives a dip to 70 % of the gate, and fires only once it
    // has lasted SustainMs AND covered SustainDistM. A closed gate does not end
    // the run: it fires later in the same run if the gate opens.
    void sustain(const Sample& s, const Config& c, float roomStep, Verdict& v) {
        sm_ = 0.5f * sm_ + 0.5f * v.roomSpeed;
        v.speed = sm_;
        if (sm_ > c.sustainSpeed) {
            if (!run_) { run_ = true; runStartMs_ = s.tMs; runDist_ = 0.0f; runPeak_ = 0.0f;
                         runFired_ = false; blockLatched_ = false; }
            runDist_ += roomStep;
            if (sm_ > runPeak_) runPeak_ = sm_;
            if (!runFired_ && (s.tMs - runStartMs_) >= (double)c.sustainMs &&
                runDist_ >= c.sustainDistM) {
                v.cooldownLeftMs = cooldown_left(s, c);
                if (s.closed)                      block(kBlockGate, s, v);
                else if (v.cooldownLeftMs > 0.0f)  block(kBlockCooldown, s, v);
                else { fire(s, v); runFired_ = true; }
                v.runMs = (float)(s.tMs - runStartMs_); v.runDistM = runDist_; v.runPeak = runPeak_;
            }
        } else if (sm_ < c.sustainSpeed * 0.7f) {
            if (run_ && !runFired_ && runPeak_ > c.sustainSpeed) {
                v.flick = true;
                v.runMs = (float)(s.tMs - runStartMs_); v.runDistM = runDist_; v.runPeak = runPeak_;
            }
            run_ = false;
            armed_ = true;
        }
    }

    bool   have_ = false, lastHeadOk_ = false;
    float  lastHand_[3] = {}, lastHead_[3] = {};
    double lastMs_ = 0.0;
    bool   armed_ = true, blockLatched_ = false;
    int    slow_ = 0;
    bool   hump_ = false;
    double humpStartMs_ = 0.0;
    float  humpPath_ = 0.0f, humpPeak_ = 0.0f;
    int    humpFired_ = kFiredNone, humpBlock_ = kBlockNone;
    uint32_t humpClosed_ = 0;
    float  ring_[3] = {};
    int    ringI_ = 0;
    float  sring_[3] = {};
    int    sringI_ = 0;
    bool   stabRun_ = false, stabDone_ = false;
    double stabStartMs_ = 0.0;
    float  stabTravel_ = 0.0f, stabPath_ = 0.0f, stabPeak_ = 0.0f, stabNet_[3] = {}, stabStartDy_ = 0.0f;
    bool   haveFire_ = false;
    double lastFireMs_ = 0.0;
    float  sm_ = 0.0f;
    bool   run_ = false, runFired_ = false;
    double runStartMs_ = 0.0;
    float  runDist_ = 0.0f, runPeak_ = 0.0f;
};

// The simulated swing: where a hand is `phaseMs` into a train of half-sine speed
// humps of `peak` m/s lasting `humpMs`, each followed by an equal rest (the rest
// is what re-arms the latch). Humps alternate direction so the hand stays in
// reach. Returns the offset along one axis, in metres.
inline float sim_offset(float peak, float humpMs, double phaseMs) {
    if (humpMs < 1.0f) humpMs = 1.0f;
    const double period = 2.0 * (double)humpMs;
    const long   n = (long)(phaseMs / period);
    const double t = phaseMs - (double)n * period;
    const double T = (double)humpMs * 0.001;
    const double full = 2.0 * (double)peak * T / 3.14159265358979;   // one hump's travel
    double x = t < (double)humpMs
        ? (double)peak * T / 3.14159265358979 * (1.0 - std::cos(3.14159265358979 * t / (double)humpMs))
        : full;
    if (n & 1) x = full - x;                      // odd humps come back
    return (float)x;
}

} // namespace dvr::swing
