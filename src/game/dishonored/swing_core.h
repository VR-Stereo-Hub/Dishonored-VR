// game/dishonored/swing_core.h - the motion sword's decision core (VR-37).
//
// What it is for: the player swings the right controller and Corvo swings the
// sword. This header decides WHEN a hand movement counts as a swing. It is pure:
// no engine objects, no globals, no clock of its own. Hand and head positions and
// a timestamp go in, a verdict comes out, so tools/swing-core-host.ps1 drives
// exactly the code the headset does. Everything that knows the game (the gates,
// the trigger pulse, the haptic, the log) lives in the adapter, melee.cpp.
//
// Two detectors share one arm latch and one cooldown:
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
//
// POSITION in, not speed in. The simulated swing (`swing sim`) synthesises
// positions too, so the differencing, the dt window and the head-relative
// subtraction are covered flat and in the host tests rather than skipped.
#pragma once
#include <cmath>
#include <cstdint>

namespace dvr::swing {

enum Detector : int { kSustain = 0, kEdge = 1 };
enum Fired : int { kFiredNone = 0, kFiredSlash = 1 };

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
    int   detector     = kSustain;
    float edgeSpeed    = 3.6f;     // m/s, fire on the crossing
    float rearmSpeed   = 1.0f;     // m/s, the hand must slow below this to re-arm
    float cooldownMs   = 300.0f;   // between fires, both detectors
    bool  headRel      = true;     // edge: subtract the head's own movement
    float sustainSpeed = 1.8f;     // m/s, the sustain gate
    float sustainMs    = 120.0f;
    float sustainDistM = 0.25f;
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
};

struct Verdict {
    bool     sampled = false;    // a speed was produced from this sample
    bool     jump = false;       // the sample was a tracking jump and was discarded
    int      fired = kFiredNone;
    int      block = kBlockNone; // set ONCE per gesture, not once per sample
    uint32_t closed = 0;         // the closed gates at the block
    float    speed = 0.0f;       // the speed the decision used
    float    roomSpeed = 0.0f;   // the raw room-space speed, for comparison
    float    cooldownLeftMs = 0.0f;
    bool     flick = false;      // sustain: a run ended without qualifying
    float    runMs = 0.0f, runDistM = 0.0f, runPeak = 0.0f;
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
            have_ = false;                        // lost tracking: re-seed
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
                if (c.detector == kEdge) { v.speed = len(d) / sec; edge(s, c, v); }
                else                     sustain(s, c, roomStep, v);
            }
            // a gap beyond kMaxDtMs, or time running backwards: re-seed below
        }
        seed(s, headOk);
        return v;
    }

private:
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

    void edge(const Sample& s, const Config& c, Verdict& v) {
        // Re-arm on the way down, UNCONDITIONALLY: a gate that closes mid-swing
        // must not leave the latch stuck and eat the next swing too.
        if (v.speed < effective_rearm(c)) {
            if (++slow_ >= kRearmSamples) { armed_ = true; blockLatched_ = false; }
        } else slow_ = 0;
        if (v.speed < c.edgeSpeed) return;
        v.cooldownLeftMs = cooldown_left(s, c);
        if (s.closed)                      block(kBlockGate, s, v);
        else if (!armed_)                  block(kBlockRearm, s, v);
        else if (v.cooldownLeftMs > 0.0f)  block(kBlockCooldown, s, v);
        else                               fire(s, v);
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
