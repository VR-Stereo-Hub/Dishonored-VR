// game/dishonored/choke_core.h - the choke assist's decisions, pure (2026-11-01).
//
// The game chokes on RB (its Block) only while its interaction focus is on a guard it lets you choke;
// otherwise RB blocks. In VR that focus follows the hand's ray (VR-166), so a guard you stand behind is
// rarely focused when the grip goes down: one choke in 58 grip presses in one headset run, none in 15
// in the next. The assist picks the guard (judge/pick), turns the interaction trace onto it, and
// presses RB once the game focuses it (Sequencer). Nothing here touches the engine;
// tools/choke-tests.cpp fails it on the host.
//
// World: UE3, X forward at yaw 0, Y right, Z up, game units; a rotator turns 65536 units a circle.
#pragma once
#include <cmath>
#include <cstdint>

namespace dvr::choke {

constexpr float kPi = 3.14159265358979f;
constexpr float kYawUnitsToRad = 2.0f * kPi / 65536.0f;

struct Guard {
    float loc[3] = {};
    int   yaw = 0;           // Actor.Rotation.Yaw, rotator units
    int   health = 0;        // Pawn.Health; -9999 unreadable
    int   controller = -1;   // 1 has one, 0 none, -1 unreadable
};
struct Config {
    float reachUU = 150.0f;    // horizontal distance, player to guard
    float heightUU = 90.0f;    // how far above or below the player the guard may stand
    float behindDeg = 90.0f;   // the player within this of the guard's back (90 = anywhere behind its shoulders)
    float frontDeg = 60.0f;    // the guard within this of where the player looks
};
enum Refusal : int { kTaken = 0, kNotLiving, kTooFar, kHeight, kNotBehind, kNotAhead };
inline const char* refusal_text(int r) {
    switch (r) {
    case kTaken:     return "taken";
    case kNotLiving: return "not a living guard (Health <= 0 or no controller)";
    case kTooFar:    return "out of reach";
    case kHeight:    return "above or below you";
    case kNotBehind: return "you are not behind it (it faces you)";
    case kNotAhead:  return "not where you look";
    default:         return "?";
    }
}
struct Reading { float distUU = 0, dzUU = 0, offBackDeg = 0, offViewDeg = 0; int refusal = kTaken; };

inline float wrap_deg(float d) {
    d = std::fmod(d + 180.0f, 360.0f);
    if (d < 0) d += 360.0f;
    return d - 180.0f;
}

// One guard, the player standing at `player` and looking along view yaw `viewYawRad`.
inline Reading judge(const float player[3], float viewYawRad, const Guard& g, const Config& c) {
    Reading r;
    const float dx = g.loc[0] - player[0], dy = g.loc[1] - player[1];
    r.dzUU = g.loc[2] - player[2];
    r.distUU = std::sqrt(dx * dx + dy * dy);
    // Where the player stands seen from the guard, against the guard's back (its facing turned round).
    const float back = g.yaw * kYawUnitsToRad + kPi;
    r.offBackDeg = std::fabs(wrap_deg((std::atan2(-dy, -dx) - back) * 57.29578f));
    r.offViewDeg = std::fabs(wrap_deg((std::atan2(dy, dx) - viewYawRad) * 57.29578f));
    if (!(g.health > 0) || g.controller != 1) r.refusal = kNotLiving;
    else if (!(r.distUU <= c.reachUU))         r.refusal = kTooFar;
    else if (!(std::fabs(r.dzUU) <= c.heightUU)) r.refusal = kHeight;
    else if (!(r.offBackDeg <= c.behindDeg))   r.refusal = kNotBehind;
    else if (r.distUU > 1.0f && !(r.offViewDeg <= c.frontDeg)) r.refusal = kNotAhead;
    return r;
}

// The nearest guard that qualifies, and for the log the nearest living one that did not.
struct Pick { int index = -1; Reading taken; int nearestRefused = -1; Reading refused; };
inline Pick pick(const float player[3], float viewYawRad, const Guard* gs, int n, const Config& c) {
    Pick p;
    for (int i = 0; i < n; ++i) {
        const Reading r = judge(player, viewYawRad, gs[i], c);
        if (r.refusal == kTaken) {
            if (p.index < 0 || r.distUU < p.taken.distUU) { p.index = i; p.taken = r; }
        } else if (r.refusal != kNotLiving) {
            if (p.nearestRefused < 0 || r.distUU < p.refused.distUU) { p.nearestRefused = i; p.refused = r; }
        }
    }
    return p;
}

// The press, present lane. A grip press or a ranged-hand swing with a guard ready STEERS the
// interaction focus onto it with RB held back; the focus arriving PRESSES RB (at least pulseMs, longer
// while the grip stays down) and keeps steering a little after, so the game reads the press with the
// focus still on the guard. No focus within focusWaitMs: a held grip becomes the block it always was,
// a swing does nothing. A grip pressed with no guard ready is passed straight through.
// The game's choke is a HOLD of Block (its own tutorial says so; two chokes released at 94 and 400 ms
// both broke free about 560 ms in, as the opening move ended). So once the game is choking (`choking`:
// its choke state, not yet in a Win, Lose or Cancel move) RB stays down by itself, up to maxHoldMs:
// a quick squeeze or a swing finishes the takedown. It lets go at the ending, so a choke does not
// run on into the game's hoist of the body unless the grip itself is still held.
struct Timing { double focusWaitMs = 250.0, pulseMs = 120.0, tailMs = 150.0, maxHoldMs = 8000.0; };
enum Phase : int { kIdle = 0, kSteering, kPressing, kPassthrough };
enum Source : int { kSrcNone = 0, kSrcGrip, kSrcSwing };
enum Event : int { kEvNone = 0, kEvSteer, kEvFocused, kEvTimeoutBlock, kEvTimeoutDrop, kEvPassthrough, kEvSwingNoGuard, kEvDone };
inline const char* phase_text(int p) {
    return p == kSteering ? "steering" : p == kPressing ? "pressing" : p == kPassthrough ? "passthrough" : "idle";
}
struct Step { bool rb = false; bool steer = false; int event = kEvNone; bool held = false; };   // held: RB down for the choke alone

class Sequencer {
public:
    // grip: the Choke action held; swing: a ranged swing fired this step; gripReady/swingReady: a guard is
    // ready for that source (its switch on and a candidate fresh); focused: the game focuses the steered guard;
    // choking: the game is in its choke and has not reached the ending.
    Step step(double now, bool grip, bool swing, bool gripReady, bool swingReady, bool focused, const Timing& t,
              bool choking = false) {
        Step s;
        const bool edge = grip && !gripWas_;
        gripWas_ = grip;
        if (phase_ == kIdle) {
            if (edge) {
                src_ = kSrcGrip;
                if (gripReady) { phase_ = kSteering; since_ = now; sawChoke_ = false; s.event = kEvSteer; }
                else { phase_ = kPassthrough; s.event = kEvPassthrough; }
            } else if (swing) {
                if (swingReady) { phase_ = kSteering; src_ = kSrcSwing; since_ = now; sawChoke_ = false; s.event = kEvSteer; }
                else s.event = kEvSwingNoGuard;
            }
        }
        if (phase_ == kPassthrough) {
            s.rb = grip;
            if (!grip) { phase_ = kIdle; src_ = kSrcNone; }
            return s;
        }
        if (phase_ == kSteering) {
            if (focused) { phase_ = kPressing; pressAt_ = now; s.event = kEvFocused; }
            else if (now - since_ >= t.focusWaitMs) {
                if (grip) { phase_ = kPassthrough; s.rb = true; s.event = kEvTimeoutBlock; }
                else { phase_ = kIdle; src_ = kSrcNone; s.event = kEvTimeoutDrop; }
                return s;
            } else { s.steer = true; return s; }
        }
        if (phase_ == kPressing) {
            const bool hold = choking && now - pressAt_ < t.maxHoldMs;
            if (hold) sawChoke_ = true;
            s.held = hold && !grip && now - pressAt_ >= t.pulseMs;
            s.rb = now - pressAt_ < t.pulseMs || grip || hold;
            if (s.rb) releasedAt_ = now;
            if (!s.rb && now - releasedAt_ >= t.tailMs) {
                phase_ = kIdle; src_ = kSrcNone;
                if (s.event == kEvNone) s.event = kEvDone;
                return s;
            }
            s.steer = true;
        }
        return s;
    }
    int phase() const { return phase_; }
    int source() const { return src_; }
    double since() const { return since_; }
    double pressed_at() const { return pressAt_; }
    bool saw_choke() const { return sawChoke_; }
    void reset() { *this = Sequencer{}; }
private:
    int phase_ = kIdle, src_ = kSrcNone;
    bool gripWas_ = false, sawChoke_ = false;
    double since_ = 0.0, pressAt_ = 0.0, releasedAt_ = 0.0;
};

// A grip with two jobs: something its pickup would take AND a guard to choke. The game's choke is a
// hold, so a TAP takes and a HOLD chokes. Nothing is pressed while it is undecided: the take is made on
// a release before holdMs, the choke when holdMs is reached with the grip still down.
class TapHold {
public:
    enum Verdict : int { kUndecided = 0, kTake, kChoke };
    // edge: the grip went down this step (starts a decision); down: it is down now.
    // Returns the verdict made THIS step, kUndecided otherwise.
    int step(double now, bool edge, bool down, double holdMs) {
        if (edge) { pending_ = true; since_ = now; if (!down) { pending_ = false; return kTake; } return kUndecided; }
        if (!pending_) return kUndecided;
        if (!down) { pending_ = false; return kTake; }
        if (now - since_ >= holdMs) { pending_ = false; return kChoke; }
        return kUndecided;
    }
    bool pending() const { return pending_; }
    double since() const { return since_; }
    void cancel() { pending_ = false; }
private:
    bool pending_ = false;
    double since_ = 0.0;
};

} // namespace dvr::choke
