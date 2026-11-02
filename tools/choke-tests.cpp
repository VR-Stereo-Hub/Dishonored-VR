// tools/choke-tests.cpp - the choke assist's pure core (choke_core.h, 2026-11-01) on the host.
// Run by tools/choke-host.ps1; never launches the game.
#include "game/dishonored/choke_core.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace dvr::choke;
static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
static int yaw_units(float deg) { return (int)std::lround(deg / 360.0f * 65536.0f); }
static Guard guard_at(float x, float y, float facingDeg, int hp = 40, int ctl = 1) {
    Guard g; g.loc[0] = x; g.loc[1] = y; g.loc[2] = 0; g.yaw = yaw_units(facingDeg); g.health = hp; g.controller = ctl; return g;
}
int main() {
    const Config c;   // reach 150, height 90, behind 90, front 60
    const float me[3] = {0, 0, 0};
    // The player at the origin looking along +X; a guard 100 uu ahead.
    {
        const Guard away = guard_at(100, 0, 0);       // faces +X, away from the player: its back is to us
        Reading r = judge(me, 0.0f, away, c);
        check(r.refusal == kTaken, "a guard ahead with its back to you is taken");
        check(std::fabs(r.offBackDeg) < 0.01f, "straight behind reads 0 deg off its back");
        check(std::fabs(r.distUU - 100.0f) < 0.01f, "distance is horizontal");
        const Guard facing = guard_at(100, 0, 180);   // faces -X, toward the player
        r = judge(me, 0.0f, facing, c);
        check(r.refusal == kNotBehind, "a guard that faces you is refused (a front fight keeps its instant block)");
        check(std::fabs(r.offBackDeg - 180.0f) < 0.01f, "face to face reads 180 deg off its back");
        // the rear half: 85 deg off its back is still behind, 95 is not
        Guard side = guard_at(100, 0, 85);
        check(judge(me, 0.0f, side, c).refusal == kTaken, "85 deg off its back: behind its shoulders, taken");
        side = guard_at(100, 0, 95);
        check(judge(me, 0.0f, side, c).refusal == kNotBehind, "95 deg off its back: in front of its shoulders, refused");
    }
    {   // reach, height, view and life
        check(judge(me, 0.0f, guard_at(149, 0, 0), c).refusal == kTaken, "149 uu: in reach");
        check(judge(me, 0.0f, guard_at(151, 0, 0), c).refusal == kTooFar, "151 uu: out of reach");
        Guard up = guard_at(100, 0, 0); up.loc[2] = 120;
        check(judge(me, 0.0f, up, c).refusal == kHeight, "a guard on the floor above is refused");
        check(judge(me, 0.0f, guard_at(-100, 0, 180), c).refusal == kNotAhead, "a guard behind YOU is not where you look");
        check(judge(me, 0.0f, guard_at(0, 100, 90), c).refusal == kNotAhead, "90 deg to your side is outside the 60 deg view cone");
        const float yaw = 90.0f * kPi / 180.0f;
        check(judge(me, yaw, guard_at(0, 100, 90), c).refusal == kTaken, "turn to face it and it is taken");
        check(judge(me, 0.0f, guard_at(100, 0, 0, 0, 1), c).refusal == kNotLiving, "Health 0 is a body, not a choke");
        check(judge(me, 0.0f, guard_at(100, 0, 0, 40, 0), c).refusal == kNotLiving, "no controller is a body, not a choke");
        check(judge(me, 0.0f, guard_at(100, 0, 0, -9999, -1), c).refusal == kNotLiving, "unreadable life is never a choke");
        Guard nan = guard_at(100, 0, 0); nan.loc[0] = NAN;
        check(judge(me, 0.0f, nan, c).refusal != kTaken, "a NaN position is never taken");
    }
    {   // the bearing holds at every yaw (no wrap error at +-180)
        for (int deg = -720; deg <= 720; deg += 15) {
            const float a = deg * kPi / 180.0f;
            const float x = 100.0f * std::cos(a), y = 100.0f * std::sin(a);
            const Guard g = guard_at(x, y, (float)deg);              // faces away from the player, along the bearing
            const Reading r = judge(me, a, g, c);
            check(r.refusal == kTaken, "behind a guard at every bearing, looking at it: taken");
            check(r.offBackDeg < 0.05f && r.offViewDeg < 0.05f, "no wrap error in either angle");
        }
    }
    {   // pick: the nearest qualifying guard; the nearest living refused one named
        Guard gs[4] = { guard_at(140, 0, 0), guard_at(80, 10, 0), guard_at(60, 0, 180), guard_at(30, 0, 0, 0, 0) };
        Pick p = pick(me, 0.0f, gs, 4, c);
        check(p.index == 1, "the nearest guard you are behind is picked");
        check(p.nearestRefused == 2 && p.refused.refusal == kNotBehind, "the nearer guard facing you is the named refusal");
        Guard none[1] = { guard_at(60, 0, 180) };
        p = pick(me, 0.0f, none, 1, c);
        check(p.index < 0 && p.nearestRefused == 0, "no candidate, and why");
        p = pick(me, 0.0f, nullptr, 0, c);
        check(p.index < 0 && p.nearestRefused < 0, "an empty list picks nothing");
    }
    const Timing t;   // wait 250, pulse 120, tail 150
    {   // the grip with a guard ready: RB held back, pressed on focus, steering until the tail ends
        Sequencer s;
        Step st = s.step(0, true, false, true, true, false, t);
        check(st.event == kEvSteer && !st.rb && st.steer, "grip with a guard: steering, RB held back");
        st = s.step(16, true, false, true, true, false, t);
        check(!st.rb && st.steer, "still no focus: still held back");
        st = s.step(32, true, false, true, true, true, t);
        check(st.event == kEvFocused && st.rb && st.steer, "focus arrives: RB pressed, still steering");
        st = s.step(500, true, false, true, true, false, t);
        check(st.rb, "RB stays down while the grip does");
        st = s.step(516, false, false, true, true, false, t);
        check(!st.rb && st.steer, "grip released: RB up, steering for the tail");
        st = s.step(700, false, false, true, true, false, t);
        check(!st.steer && st.event == kEvDone && s.phase() == kIdle, "the tail ends: idle");
    }
    {   // a quick tap that lets go before the focus still chokes, for the whole pulse
        Sequencer s;
        s.step(0, true, false, true, true, false, t);
        Step st = s.step(16, false, false, true, true, false, t);
        check(!st.rb && st.steer, "tap released before the focus: still steering");
        st = s.step(40, false, false, true, true, true, t);
        check(st.rb && st.event == kEvFocused, "the focus arrives after the tap: RB pressed");
        st = s.step(100, false, false, true, true, false, t);
        check(st.rb, "the pulse holds RB its full length");
        st = s.step(170, false, false, true, true, false, t);
        check(!st.rb && st.steer, "pulse over (120 ms): RB up, tail steering");
    }
    {   // no focus in time: a held grip becomes the block, a released one does nothing
        Sequencer s;
        s.step(0, true, false, true, true, false, t);
        Step st = s.step(249, true, false, true, true, false, t);
        check(!st.rb, "249 ms: still waiting");
        st = s.step(250, true, false, true, true, false, t);
        check(st.rb && st.event == kEvTimeoutBlock && !st.steer, "250 ms without focus: the grip's block, steering off");
        st = s.step(400, true, false, true, true, false, t);
        check(st.rb && s.phase() == kPassthrough, "the block holds while the grip does");
        st = s.step(416, false, false, true, true, false, t);
        check(!st.rb && s.phase() == kIdle, "release: idle");
        Sequencer q;
        q.step(0, true, false, true, true, false, t);
        q.step(10, false, false, true, true, false, t);
        st = q.step(260, false, false, true, true, false, t);
        check(!st.rb && st.event == kEvTimeoutDrop && q.phase() == kIdle, "a tap that never got a focus does nothing");
    }
    {   // the grip with no guard ready: passed straight through, from the first step
        Sequencer s;
        Step st = s.step(0, true, false, false, true, false, t);
        check(st.rb && st.event == kEvPassthrough && !st.steer, "no guard: the block, instantly");
        st = s.step(16, true, false, true, true, true, t);
        check(st.rb && !st.steer, "a guard appearing mid-press does not take over the block");
        s.step(32, false, false, true, true, false, t);
        check(s.phase() == kIdle, "release: idle");
    }
    {   // the ranged swing: a pulse on focus, nothing without a guard, nothing on timeout
        Sequencer s;
        Step st = s.step(0, false, true, false, false, false, t);
        check(!st.rb && !st.steer && st.event == kEvSwingNoGuard, "a swing with no guard: nothing at all (no block)");
        st = s.step(10, false, true, true, true, false, t);
        check(st.event == kEvSteer && st.steer && !st.rb && s.source() == kSrcSwing, "a swing with a guard: steering");
        st = s.step(30, false, false, true, true, true, t);
        check(st.rb && st.event == kEvFocused, "focus: the pulse");
        st = s.step(149, false, false, true, true, false, t);
        check(st.rb, "the pulse is still on at 119 ms");
        st = s.step(151, false, false, true, true, false, t);
        check(!st.rb, "the pulse is off at 121 ms");
        Sequencer q;
        q.step(0, false, true, true, true, false, t);
        st = q.step(250, false, false, true, true, false, t);
        check(!st.rb && st.event == kEvTimeoutDrop, "a swing that never got a focus does not block");
        Sequencer r;   // a swing while the grip assist is off still works (separate switches)
        st = r.step(0, false, true, false, true, false, t);
        check(st.event == kEvSteer, "the swing's own readiness is what counts for a swing");
    }
    {   // a second trigger while one is in flight is ignored, never stacked
        Sequencer s;
        s.step(0, false, true, true, true, false, t);
        Step st = s.step(10, false, true, true, true, false, t);
        check(st.event == kEvNone && st.steer, "a second swing mid-steer changes nothing");
        st = s.step(20, true, false, true, true, true, t);
        check(st.rb, "a grip pressed during a swing's steer is honoured on the focus");
    }
    {   // the hold: once the game chokes, RB stays down by itself until the ending, then lets go
        Sequencer s;
        s.step(0, true, false, true, true, false, t);
        Step st = s.step(16, true, false, true, true, true, t);          // focused: pressed
        check(st.rb, "pressed");
        st = s.step(60, false, false, true, true, false, t, true);       // grip let go at 60 ms, the game is choking
        check(st.rb && !st.held, "inside the pulse: down, and not yet the choke's own hold");
        st = s.step(600, false, false, true, true, false, t, true);
        check(st.rb && st.held && st.steer, "600 ms, grip long released: RB still down because the game is choking");
        st = s.step(3000, false, false, true, true, false, t, true);
        check(st.rb && st.held, "3 s into the choke: still held");
        st = s.step(3500, false, false, true, true, false, t, false);    // the Win move: the ending
        check(!st.rb, "the ending lets go (no run-on into the hoist)");
        check(s.saw_choke(), "the hold is recorded for the log");
        s.step(3700, false, false, true, true, false, t, false);
        check(s.phase() == kIdle, "then idle (the tail runs from the last step RB was down)");
        // negative control: without the game's choke state the same release lets go at the pulse
        Sequencer q;
        q.step(0, true, false, true, true, false, t);
        q.step(16, true, false, true, true, true, t);
        st = q.step(600, false, false, true, true, false, t, false);
        check(!st.rb && !q.saw_choke(), "negative control: no choke state, no hold");
        // the cap: a choke state that never ends is let go at maxHoldMs
        Sequencer r;
        r.step(0, true, false, true, true, false, t);
        r.step(16, true, false, true, true, true, t);
        st = r.step(8015, false, false, true, true, false, t, true);
        check(st.rb, "just under the cap: held");
        st = r.step(8017, false, false, true, true, false, t, true);
        check(!st.rb, "past 8 s: let go even if the state never ends");
        // the swing: a pulse alone would lose the choke; the hold carries it
        Sequencer w;
        w.step(0, false, true, true, true, false, t);
        w.step(20, false, false, true, true, true, t);
        st = w.step(2000, false, false, true, true, false, t, true);
        check(st.rb && st.held && w.source() == kSrcSwing, "a swing's choke is held through by itself");
        // the grip held through the ending stays the player's: still down (they may want the hoist)
        Sequencer g;
        g.step(0, true, false, true, true, false, t);
        g.step(16, true, false, true, true, true, t);
        st = g.step(4000, true, false, true, true, false, t, false);
        check(st.rb && !st.held, "a grip still held at the ending keeps RB down: the player's choice");
    }
    {   // a grip with two jobs: a tap takes, a hold chokes, nothing in between
        TapHold th;
        check(th.step(0, true, true, 250) == TapHold::kUndecided && th.pending(), "the press decides nothing");
        check(th.step(100, false, true, 250) == TapHold::kUndecided, "100 ms held: still undecided");
        check(th.step(140, false, false, 250) == TapHold::kTake && !th.pending(), "released at 140 ms: a take");
        check(th.step(400, false, false, 250) == TapHold::kUndecided, "decided once: nothing more");
        TapHold h;
        h.step(0, true, true, 250);
        check(h.step(249, false, true, 250) == TapHold::kUndecided, "249 ms: undecided");
        check(h.step(250, false, true, 250) == TapHold::kChoke, "250 ms still down: a choke");
        check(h.step(900, false, false, 250) == TapHold::kUndecided, "the release after a choke is not also a take");
        TapHold z;
        check(z.step(0, true, false, 250) == TapHold::kTake, "a press and release inside one step is a take");
        TapHold n;
        check(n.step(0, false, true, 250) == TapHold::kUndecided && !n.pending(), "no edge, no decision (a grip already down)");
        TapHold cn;
        cn.step(0, true, true, 250); cn.cancel();
        check(cn.step(300, false, true, 250) == TapHold::kUndecided, "a cancelled press decides nothing");
    }
    std::printf("%u choke checks passed\n", checks);
    return 0;
}
