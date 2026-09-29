// Host tests for the motion sword's decision core (VR-37). Never launches the game.
// Every check names the player-visible behaviour it pins, and each scenario is a
// hand moving through space at a headset's sample rate, not a list of speeds: the
// core takes positions, so the differencing is under test too.
#include "game/dishonored/swing_core.h"
#include <cstdio>
#include <cstdlib>
#include <functional>
using namespace dvr::swing;

static int checks = 0;
static void check(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::printf("FAIL %s\n", why); std::exit(1); }
}

struct Tally { int fires = 0, blocks = 0, gate = 0, rearm = 0, cooldown = 0, flicks = 0;
               float maxSpeed = 0, lastRunMs = 0; double firstFireMs = -1; };

// Drive `core` for `ms` at `hz`, with speed(t) m/s along +X (the hand) and
// headSpeed(t) along +X (the head). Positions are integrated, so what reaches
// the core is what a runtime would hand it.
static Tally drive(Core& core, const Config& c, double ms, double hz,
                   const std::function<float(double)>& speed,
                   const std::function<float(double)>& headSpeed = nullptr,
                   uint32_t closed = 0, double t0 = 0.0, float* handX = nullptr) {
    Tally t; const double dt = 1000.0 / hz;
    float x = handX ? *handX : 0.0f, hx = 0.0f;
    for (double at = 0.0; at <= ms; at += dt) {
        Sample s; s.hand[0] = x; s.hand[1] = 1.2f; s.hand[2] = -0.4f;
        s.head[0] = hx; s.head[1] = 1.6f; s.handValid = s.headValid = true;
        s.tMs = t0 + at; s.closed = closed;
        const Verdict v = core.feed(s, c);
        if (v.fired) { ++t.fires; if (t.firstFireMs < 0) t.firstFireMs = s.tMs; t.lastRunMs = v.runMs; }
        if (v.block) { ++t.blocks; if (v.block == kBlockGate) ++t.gate;
                       if (v.block == kBlockRearm) ++t.rearm; if (v.block == kBlockCooldown) ++t.cooldown; }
        if (v.flick) ++t.flicks;
        if (v.speed > t.maxSpeed) t.maxSpeed = v.speed;
        x  += speed(at) * (float)(dt * 0.001);
        if (headSpeed) hx += headSpeed(at) * (float)(dt * 0.001);
    }
    if (handX) *handX = x;
    return t;
}
static std::function<float(double)> humps(float peak, double humpMs) {
    return [=](double t) { const double p = std::fmod(t, humpMs * 2.0);
        return p < humpMs ? peak * (float)std::sin(3.14159265358979 * p / humpMs) : 0.0f; };
}

int main() {
    Config edge; edge.detector = kEdge;

    { Core k; check(drive(k, edge, 400, 90, humps(3.5f, 200)).fires == 0, "a 3.5 m/s swing stays under a 3.6 threshold"); }
    { Core k; const Tally t = drive(k, edge, 400, 90, humps(3.7f, 200));
      check(t.fires == 1, "a 3.7 m/s swing crosses a 3.6 threshold exactly once"); }
    { Core k; const Tally t = drive(k, edge, 400, 90, humps(6.0f, 200));
      check(t.fires == 1 && t.blocks == 0, "one swing is one attack, up-stroke and down-stroke together");
      Core r; Config rc = edge; rc.median = false; const Tally u = drive(r, rc, 400, 90, humps(6.0f, 200));
      check(t.firstFireMs < 100.0, "the attack fires on the rising edge, before the swing even peaks");
      check(u.fires == 1 && t.firstFireMs - u.firstFireMs <= 1000.0 / 90.0 + 0.01,
            "and the median costs at most one sample over raw speed"); }
    { Core k; const Tally t = drive(k, edge, 800, 90,
          [](double ms) { return 2.0f + 4.0f * (float)std::fabs(std::sin(3.14159265358979 * ms / 200.0)); });
      check(t.fires == 1, "a hand that never slows below the re-arm level cannot fire twice"); }
    { Core k; const Tally t = drive(k, edge, 1199, 90, humps(6.0f, 200));
      check(t.fires == 3, "three swings 400 ms apart pass a 300 ms cooldown"); }
    { Core k; Config c = edge; c.cooldownMs = 600;
      const Tally t = drive(k, c, 1199, 90, humps(6.0f, 200));
      check(t.fires == 2 && t.cooldown == 1, "a 600 ms cooldown blocks the middle swing of three, once"); }
    { Core k; const Tally shut = drive(k, edge, 399, 90, humps(6.0f, 200), nullptr, kGateSword);
      check(shut.fires == 0 && shut.gate == 1 && shut.blocks == 1, "a closed gate blocks once per swing, not once per sample");
      const Tally open = drive(k, edge, 399, 90, humps(6.0f, 200), nullptr, 0, 400.0);
      check(open.fires == 1, "a swing blocked by a gate still re-arms the next one"); }
    { // A runtime that repeats a pose: every third sample is the previous position again.
      Core k; Tally t; const double dt = 1000.0 / 90.0; float x = 0; int i = 0;
      for (double at = 0; at < 400.0; at += dt, ++i) {
          Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.closed = kGateSword;
          const Verdict v = k.feed(s, edge); if (v.fired) ++t.fires; if (v.block) ++t.blocks;
          if (i % 3 != 2) x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
      check(t.blocks == 1 && t.fires == 0, "a repeated pose inside a swing does not re-arm it: still one verdict per swing");
      Core j; Tally u; x = 0; i = 0;
      for (double at = 0; at < 400.0; at += dt, ++i) {
          Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x;
          if (j.feed(s, edge).fired) ++u.fires;
          if (i % 3 != 2) x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
      check(u.fires == 1, "and with the gates open that same swing is exactly one attack"); }
    { Core k; const Tally t = drive(k, edge, 250, 90, humps(8.0f, 200), humps(8.0f, 200));
      check(t.fires == 0, "turning the whole body moves head and hand together and is not a swing"); }
    { Core k; Config c = edge; c.headRel = false;
      check(drive(k, c, 250, 90, humps(8.0f, 200), humps(8.0f, 200)).fires == 1, "the head-relative lever is what rejected it"); }
    { Core k; Config c = edge; c.rearmSpeed = 5.0f;
      check(std::fabs(effective_rearm(c) - 3.24f) < 1e-4f, "the re-arm level is capped at 0.9 x the threshold");
      check(drive(k, c, 1199, 90, humps(6.0f, 200)).fires == 3, "a re-arm level typed above the threshold still re-arms"); }

    // The median of three: one lying sample, in either direction, decides nothing.
    { // A 3.0 m/s swing delivered the way the simulator delivered it: a repeated
      // pose, then a sample carrying two frames of travel in one frame's time.
      auto run = [](bool median) { Core k; Config c; c.detector = kEdge; c.median = median; int fires = 0;
          const double dt = 1000.0 / 90.0; float x = 0, owed = 0; int i = 0;
          for (double at = 0; at < 400.0; at += dt, ++i) {
              Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x;
              if (k.feed(s, c).fired) ++fires;
              const float step = humps(3.0f, 200)(at) * (float)(dt * 0.001);
              if (i % 5 == 4) owed = step; else { x += step + owed; owed = 0; } }
          return fires; };
      check(run(true) == 0, "a 3.0 m/s swing with doubled samples (raw peaks of 6) does not fire through the median");
      check(run(false) == 1, "and does fire on raw speed, so the median lever is what stopped it"); }
    { Core k; Config c = edge; Sample s; s.handValid = true; s.tMs = 0; k.feed(s, c);
      s.hand[0] = 0.1f; s.tMs = 11.0;
      check(!k.feed(s, c).fired, "the first reading after a seed cannot fire by itself");
      s.hand[0] = 0.2f; s.tMs = 22.0;
      check(k.feed(s, c).fired == kFiredSlash, "the second one confirms it: one sample of latency"); }

    // Sample hygiene: what the differencing must refuse to turn into a speed.
    Config raw = edge; raw.median = false;   // these pin the differencing, one sample at a time
    { Core k; Sample s; s.handValid = s.headValid = true; s.tMs = 0; k.feed(s, raw);
      s.hand[0] = 0.5f; s.tMs = 1.0;
      check(!k.feed(s, raw).sampled, "two poses 1 ms apart produce no speed");
      s.tMs = 11.0; const Verdict v = k.feed(s, raw);
      check(v.jump && !v.sampled && !v.fired && std::fabs(v.roomSpeed - 0.5f / 0.011f) < 0.5f,
            "and the older seed is kept, so the travel is timed over the full gap: 45 m/s, a tracking jump, discarded");
      s.hand[0] = 0.55f; s.tMs = 22.0; const Verdict w = k.feed(s, raw);
      check(w.sampled && w.fired == kFiredSlash && std::fabs(w.speed - 0.05f / 0.011f) < 0.2f,
            "the jump re-seeded, so the next real sample is timed from where the hand reappeared"); }
    { Core k; Sample s; s.handValid = s.headValid = true; s.tMs = 0; k.feed(s, raw);
      s.hand[0] = 0.2f; s.tMs = 11.0;
      check(k.feed(s, raw).fired == kFiredSlash, "18 m/s is a very fast arm and still a swing"); }
    { Core k; Sample s; s.handValid = s.headValid = true; s.tMs = 0; k.feed(s, raw);
      s.hand[0] = 2.0f; s.tMs = 500.0;
      check(!k.feed(s, raw).sampled, "a 500 ms gap (an alt-tab, a load) re-seeds instead of reading 4 m/s");
      s.handValid = false; s.tMs = 511.0; k.feed(s, raw);
      s.handValid = true; s.hand[0] = 0.0f; s.tMs = 522.0;
      check(!k.feed(s, raw).sampled, "a hand that lost tracking re-seeds where it reappears"); }
    { Core k; Sample s; s.handValid = true; s.tMs = 0; k.feed(s, raw);
      s.hand[1] = NAN; s.tMs = 11.0; k.feed(s, raw);
      s.hand[1] = 0.0f; s.hand[0] = 1.0f; s.tMs = 22.0;
      check(!k.feed(s, raw).sampled, "a non-finite pose is treated as lost tracking"); }
    { Core k; Sample s; s.handValid = true; s.headValid = false; s.tMs = 0; k.feed(s, raw);
      s.hand[0] = 0.1f; s.tMs = 11.0;
      check(k.feed(s, raw).fired == kFiredSlash, "with no head pose the hand's own speed still counts"); }


    // ---- The hump census and the travel guard (VR-170) -------------------------
    // One line per hand movement: what a threshold is set from. `census` drives a
    // speed profile and collects every hump that ended.
    struct Hump { int n = 0, fired = 0, block = 0, cut = 0; float peak = 0, travel = 0, ms = 0, atFire = 0; double fireMs = -1; };
    // `stallAt`/`stallMs`: the samples stop for that long (a game-thread hitch) while the hand keeps moving.
    auto census = [](const Config& c, double ms, const std::function<float(double)>& speed, uint32_t closed = 0,
                     double stallAt = -1.0, double stallMs = 0.0) {
        Core k; Hump h; const double dt = 1000.0 / 90.0; float x = 0;
        for (double at = 0.0; at <= ms; at += dt) {
            const bool stalled = stallAt >= 0.0 && at >= stallAt && at < stallAt + stallMs;
            if (!stalled) {
                Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.closed = closed;
                const Verdict v = k.feed(s, c);
                if (v.fired) { h.atFire = v.travelAtFire; if (h.fireMs < 0) h.fireMs = at; }
                if (v.humpEnd) { ++h.n; h.fired = v.humpFired; h.block = v.humpBlock; h.peak = v.humpPeak;
                                 h.travel = v.humpTravel; h.ms = v.humpMs; if (v.humpCut) ++h.cut; }
            }
            x += speed(at) * (float)(dt * 0.001);
        }
        return h; };
    { const Hump h = census(edge, 500, humps(6.0f, 200));
      check(h.n == 1 && h.fired == kFiredSlash, "a swing is one hump, and the hump says it fired");
      check(h.peak > 5.7f && h.peak <= 6.0f, "the hump's peak is the swing's peak, through the median");
      check(h.travel > 0.70f && h.travel < 0.78f, "and its travel is the distance the hand covered: 0.76 m for this one");
      check(h.ms > 150.0f && h.ms < 260.0f, "and it lasted about as long as the swing did");
      std::printf("census: a 6.0 m/s, 200 ms swing had travelled %.3f m when it fired at 3.6 (of %.3f m in all)\n", h.atFire, h.travel);
      check(h.atFire > 0.04f && h.atFire < 0.16f, "a real swing has covered well under 16 cm when it crosses the threshold: why a small travel guard decides nothing"); }
    { const Hump h = census(edge, 500, humps(2.5f, 200));
      check(h.n == 1 && h.fired == kFiredNone && h.block == kBlockNone && h.peak > 2.3f && h.peak < 2.51f,
            "a 2.5 m/s movement that did not fire is still counted, with its peak: the half of the distribution a FIRE line never showed"); }
    { const Hump h = census(edge, 500, humps(0.8f, 200));
      check(h.n == 0, "a movement that never reaches the re-arm level is not a hump at all"); }
    { const Hump h = census(edge, 500, humps(6.0f, 200), kGateSword);
      check(h.n == 1 && h.fired == kFiredNone && h.block == kBlockGate, "a swing against a closed gate is a hump that names the block"); }
    { const Hump h = census(edge, 1199, humps(6.0f, 200));
      check(h.n == 3 && h.cut == 0, "three swings are three humps"); }
    { // A 135 ms hitch inside a soft swing, as the simulator delivered one: the samples stop at 2.3 m/s on the way up.
      const Hump h = census(edge, 800, humps(3.3f, 330), 0, 80.0, 135.0);
      check(h.n >= 1 && h.cut == 1, "a swing with a hitch in the middle is still counted: the hump is cut short, not lost");
      const Hump g = census(edge, 800, humps(3.3f, 330));
      check(g.n == 1 && g.cut == 0 && g.peak > 3.1f, "and without the hitch the same swing is one whole hump"); }
    { Config g = edge; g.edgeTravelM = 0.30f;
      const Hump off = census(edge, 500, humps(6.0f, 200)), on = census(g, 500, humps(6.0f, 200));
      check(on.n == 1 && on.fired == kFiredSlash && on.atFire >= 0.30f, "under a 0.30 m travel guard a real swing still fires, once it has the distance");
      check(on.fireMs - off.fireMs > 0.0 && on.fireMs - off.fireMs <= 4000.0 / 90.0 + 0.01, "and the guard cost it at most four samples: it delays, it does not refuse"); }
    { // A sharp 5 cm jolt from rest, on raw speed: 4.5 m/s for one sample.
      auto jolt = [](float guard) { Core k; Config c; c.detector = kEdge; c.median = false; c.edgeTravelM = guard;
          Sample s; s.handValid = true; s.tMs = 0; k.feed(s, c);
          s.hand[0] = 0.05f; s.tMs = 11.0; const bool fired = k.feed(s, c).fired != kFiredNone;
          int humpsSeen = 0, blocks = 0;
          for (int i = 2; i < 8; ++i) { s.tMs = 11.0 * i; const Verdict v = k.feed(s, c); if (v.humpEnd) ++humpsSeen; if (v.block) ++blocks; }
          return fired ? 1 : (humpsSeen == 1 && blocks == 0) ? 0 : -1; };
      check(jolt(0.0f) == 1, "a 5 cm jolt at 4.5 m/s fires on raw speed with the guard off");
      check(jolt(0.15f) == 0, "and does not with a 0.15 m guard - silently, no block line, but the census still counts it: the guard lever is what decided it"); }

    // ---- The sneak-kill thrust (VR-155) ---------------------------------------
    // A body in space: the head at (0, 1.6, 0) facing -Z, so the modelled right
    // shoulder is at (0.17, 1.38, 0.04), and the sword hand starts a forearm in
    // front of it. Velocities are vectors here because DIRECTION is the test.
    struct V3 { float x, y, z; };
    struct Stab { int slash = 0, stab = 0, blocks = 0, rejects = 0, why = 0; float travel = 0, ratio = 0, fwd = 0; };
    auto body = [](const Config& c, double ms, bool armed, const std::function<V3(double)>& hand,
                   const std::function<V3(double)>& head = nullptr,
                   const std::function<float(double)>& yawDeg = nullptr, uint32_t closed = 0) {
        Core k; Stab r; const double dt = 1000.0 / 90.0;
        float h[3] = { 0.20f, 1.25f, -0.25f }, hd[3] = { 0.0f, 1.6f, 0.0f };
        for (double at = 0.0; at <= ms; at += dt) {
            Sample s; s.handValid = s.headValid = true; s.tMs = at; s.closed = closed; s.stabArmed = armed;
            for (int i = 0; i < 3; ++i) { s.hand[i] = h[i]; s.head[i] = hd[i]; }
            const float yaw = yawDeg ? yawDeg(at) * 0.0174533f : 0.0f;
            s.headFwd[0] = -std::sin(yaw); s.headFwd[1] = -std::cos(yaw);
            const Verdict v = k.feed(s, c);
            if (v.fired == kFiredSlash) ++r.slash;
            if (v.fired == kFiredStab) { ++r.stab; r.travel = v.stabTravel; r.ratio = v.stabRatio; r.fwd = v.stabForward; }
            if (v.block) ++r.blocks;
            if (v.stabReject) { ++r.rejects; r.why = v.stabReject; }
            const V3 a = hand(at); const float sec = (float)(dt * 0.001);
            h[0] += a.x * sec; h[1] += a.y * sec; h[2] += a.z * sec;
            if (head) { const V3 b = head(at); hd[0] += b.x * sec; hd[1] += b.y * sec; hd[2] += b.z * sec; }
        }
        return r; };
    auto hump = [](float peak, double humpMs, double t) {
        return t < humpMs ? peak * (float)std::sin(3.14159265358979 * t / humpMs) : 0.0f; };
    Config sc = edge; sc.stab = true; sc.stabStyle = kThrust;   // the forward-grip stab; the plunge follows

    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; });
      check(r.stab == 1 && r.slash == 0, "a 0.28 m thrust forward at 2.2 m/s is one stab and no slash");
      check(r.travel >= 0.20f && r.ratio > 0.85f && r.fwd > 0.95f, "and it was judged on extension, straightness and direction"); }
    { const Stab r = body(sc, 500, false, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; });
      check(r.stab == 0 && r.rejects == 0 && r.blocks == 0, "the same thrust standing up in a fight (not armed) is nothing, and says nothing"); }
    { Config off = sc; off.stab = false;
      const Stab r = body(off, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; });
      check(r.stab == 0 && r.slash == 0, "with the lever off a thrust never fires: it is under the slash threshold by design"); }
    { const Stab r = body(sc, 400, true, [&](double t) { return V3{ 0, 0, -hump(2.0f, 100, t) }; });
      check(r.stab == 0 && r.rejects == 1 && r.why == kStabTravel, "a 0.13 m jab is rejected on travel, once"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ hump(2.2f, 250, t), 0, 0 }; });
      check(r.stab == 0, "a sweep out to the side is not a stab"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ 0, -hump(2.2f, 250, t), 0 }; });
      check(r.stab == 0, "reaching down to the floor is not a stab"); }
    { const Stab r = body(sc, 900, true, [&](double t) { return V3{ 0, 0, t < 400 ? -0.8f : 0.0f }; });
      check(r.stab == 0 && r.rejects == 0, "a slow 0.3 m drift forward never starts a thrust at all"); }
    { const Stab r = body(sc, 900, true, [&](double t) {
          return V3{ 0, 0, t < 200 ? -hump(2.2f, 200, t) : t < 500 ? 0.0f : hump(2.2f, 200, t - 500) }; });
      check(r.stab == 1, "pulling the hand back after a stab is not a second one"); }
    { const Stab r = body(sc, 600, true, [&](double) { return V3{ 0, 0, 0 }; }, nullptr,
          [&](double t) { return t < 300 ? (float)(t * 0.3) : 90.0f; });
      check(r.stab == 0 && r.rejects == 0, "turning the head 90 degrees with the hand still moves the modelled shoulder and extends nothing"); }
    { const Stab r = body(sc, 500, true, [&](double) { return V3{ 0, 0, 0 }; },
          [&](double t) { return V3{ 0, 0, hump(1.2f, 200, t) }; });
      check(r.stab == 0, "leaning the head 0.15 m back from a still hand is not a stab"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) - hump(1.5f, 300, t) }; },
          [&](double t) { return V3{ 0, 0, -hump(1.5f, 300, t) }; });
      check(r.stab == 1, "stepping into the stab still counts: the step is subtracted, the extension is what is left"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ hump(6.0f, 200, t), 0, 0 }; });
      check(r.slash == 1 && r.stab == 0, "a real slash while sneaking is a slash, once"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ 0, 0, -hump(6.0f, 200, t) }; });
      check(r.slash + r.stab == 1, "a hard punch forward is ONE attack, whichever detector took it"); }
    { const Stab r = body(sc, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; }, nullptr, nullptr, kGateSword);
      check(r.stab == 0 && r.blocks == 1, "a thrust against a closed gate is blocked once and says so"); }
    { Config yc = sc; // the body turned 90 degrees to the left: forward is -X now
      Core k; int stabs = 0; const double dt = 1000.0 / 90.0; float x = -0.25f;
      for (double at = 0; at <= 500; at += dt) { Sample s; s.handValid = s.headValid = true; s.tMs = at; s.stabArmed = true;
          s.head[1] = 1.6f; s.headFwd[0] = -1.0f; s.headFwd[1] = 0.0f; s.hand[0] = x; s.hand[1] = 1.25f; s.hand[2] = -0.20f;
          if (k.feed(s, yc).fired == kFiredStab) ++stabs; x -= hump(2.2f, 200, at) * (float)(dt * 0.001); }
      check(stabs == 1, "forward is where the HEAD faces: turned left, a thrust along -X is the stab"); }

    // ---- The plunge: the same kill with the blade in a reverse grip -------------
    // body() starts the hand at (0.20, 1.25, -0.25), 13 cm below the shoulder line
    // (1.38). `lift` raises it first, slowly, the way a player winds up.
    { Config pc = edge; pc.stab = true; pc.stabStyle = kPlunge;
      auto plunge = [&](double t0, float peak) { return [=](double t) {
          if (t < 300) return V3{ 0, 0.30f / 0.3f, 0 };                 // lift 30 cm in 300 ms, 1 m/s
          const float v = t < t0 ? 0.0f : hump(peak, 200, t - t0); return V3{ 0, -0.94f * v, -0.342f * v }; }; };
      Stab r = body(pc, 900, true, plunge(400, 2.2f));
      check(r.stab == 1 && r.slash == 0, "a raised fist driven down and a little forward is one stab");
      check(r.fwd > 0.9f && r.ratio > 0.85f, "judged along the down-and-forward axis");
      r = body(pc, 900, false, plunge(400, 2.2f));
      check(r.stab == 0 && r.rejects == 0, "standing up it is nothing, and says nothing");
      r = body(pc, 600, true, [&](double t) { const float v = hump(2.2f, 200, t); return V3{ 0, -0.94f * v, -0.342f * v }; });
      check(r.stab == 0 && r.rejects == 1 && r.why == kStabStart, "the same plunge from waist height is reaching for loot: rejected on where it started");
      r = body(pc, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; });
      check(r.stab == 0, "a forward thrust is not a plunge");
      r = body(pc, 900, true, plunge(400, 2.2f), [&](double t) { const float v = t < 400 ? 0.0f : hump(2.2f, 200, t - 400);
          return V3{ 0, -0.94f * v, -0.342f * v }; });
      check(r.stab == 0, "ducking with the fist raised moves head and hand together and is not a plunge");
      r = body(pc, 900, true, plunge(400, 6.0f));
      check(r.slash + r.stab == 1, "a hard plunge is ONE attack, whichever detector took it");
      Config tc = pc; tc.stabStyle = kThrust;
      r = body(tc, 900, true, plunge(400, 2.2f));
      check(r.stab == 0, "and under the thrust style a plunge is not a thrust: the style lever is a real A/B"); }

    // ---- The contact-timed sword (VR-173) ---------------------------------------
    // A hand AND a blade. The hand moves along +X; the blade is 0.62 m long, lies in
    // the horizontal plane and points away from the player (-Z) turned by `angle`
    // towards +X, so the tip is hand + L (sin a, 0, -cos a) and its velocity is the
    // hand's plus the turning carried out along the blade. `touch` is the engine's
    // answer, given the tip: the adapter's trace, stood in for by a slab of space.
    struct Blade { int fires = 0, blocks = 0, gate = 0, rearm = 0, cooldown = 0, humps = 0, touched = 0, tipHumps = 0, cuts = 0;
                   int owner = -1, known = 0, fallback = 0, jumps = 0; float tipPeak = 0, handPeak = 0, humpPeak = 0;
                   double fireMs = -1, touchMs = -1; };
    const float L = 0.62f;
    auto blade = [L](const Config& c, double ms, const std::function<float(double)>& handSpeed,
                     const std::function<float(double)>& angle, const std::function<bool(const float*, double)>& touch,
                     bool valid = true, float ageMs = 11.0f, uint32_t closed = 0, bool tipValid = true,
                     const std::function<float(double)>& tipJump = nullptr) {
        Core k; Blade r; const double dt = 1000.0 / 90.0; float x = 0.0f;
        for (double at = 0.0; at <= ms; at += dt) {
            Sample s; s.handValid = s.headValid = true; s.tMs = at; s.closed = closed;
            s.hand[0] = x; s.hand[1] = 1.2f; s.hand[2] = -0.4f; s.head[1] = 1.6f;
            const float a = angle ? angle(at) : 0.0f;
            s.tip[0] = x + L * std::sin(a) + (tipJump ? tipJump(at) : 0.0f); s.tip[1] = 1.2f; s.tip[2] = -0.4f - L * std::cos(a);
            s.tipValid = tipValid; s.contactValid = valid; s.contactAgeMs = ageMs;
            s.contact = touch ? touch(s.tip, at) : false;
            if (s.contact && r.touchMs < 0) r.touchMs = at;
            const Verdict v = k.feed(s, c);
            if (v.fired) { ++r.fires; r.owner = v.owner; if (r.fireMs < 0) r.fireMs = at; }
            if (v.block) { ++r.blocks; if (v.block == kBlockGate) ++r.gate; if (v.block == kBlockRearm) ++r.rearm;
                           if (v.block == kBlockCooldown) ++r.cooldown; }
            if (v.known) ++r.known;
            if (v.fallback) ++r.fallback;
            if (v.jump) ++r.jumps;
            if (v.tipSpeed > r.tipPeak) r.tipPeak = v.tipSpeed;
            if (v.handSpeed > r.handPeak) r.handPeak = v.handSpeed;
            if (v.humpEnd) { ++r.humps; r.humpPeak = v.humpPeak; if (v.humpTouched) ++r.touched; if (v.humpTip) ++r.tipHumps;
                             if (v.humpCut) ++r.cuts; }
            x += handSpeed(at) * (float)(dt * 0.001);
        }
        return r; };
    auto still = [](double) { return 0.0f; };
    auto slab = [](float x0, float x1) { return [=](const float* tip, double) { return tip[0] >= x0 && tip[0] <= x1; }; };
    auto always = [](const float*, double) { return true; };
    Config con; con.detector = kContact;

    { const Blade r = blade(con, 399, humps(6.0f, 200), nullptr, nullptr);
      check(r.fires == 0 && r.blocks == 0, "contact: a 6 m/s swing through empty air is nothing: no attack, no block");
      check(r.humps == 1 && r.tipHumps == 1 && r.touched == 0 && r.humpPeak > 5.7f,
            "and it is still counted, as a blade movement that touched nothing");
      Core k; check(drive(k, edge, 399, 90, humps(6.0f, 200)).fires == 1, "the same swing under edge attacks: the detector is what decided it"); }
    { const Blade r = blade(con, 399, humps(6.0f, 200), nullptr, slab(0.30f, 0.50f));
      check(r.fires == 1 && r.owner == kOwnerContact, "contact: the same swing with something in its path is one attack, owned by the contact");
      check(r.touchMs > 80.0 && r.fireMs >= r.touchMs && r.fireMs - r.touchMs <= 1000.0 / 90.0 + 0.01,
            "pressed when the blade ARRIVES (86 ms into this swing), at most one sample after the touch");
      check(r.humps == 1 && r.touched == 1, "and its hump says the blade touched"); }
    { const Blade r = blade(con, 1500, [](double) { return 0.5f; }, nullptr, always);
      check(r.fires == 0 && r.blocks == 0 && r.known > 100, "contact: a blade resting against a guard while the hand drifts at 0.5 m/s is not an attack"); }
    { // The wrist: the hand does not move at all, the blade turns 6 rad/s for 150 ms.
      auto flick = [](double t) { return t < 100 ? -0.45f : t < 250 ? -0.45f + 6.0f * (float)(t - 100) * 0.001f : 0.45f; };
      const Blade r = blade(con, 600, still, flick, always);
      check(r.fires == 1 && r.owner == kOwnerContact, "contact: a flick of the wrist with the hand still is an attack: the speed is the TIP's");
      check(r.tipPeak > 3.5f && r.tipPeak < 3.9f && r.handPeak < 0.01f, "6 rad/s along 0.62 m of blade is 3.7 m/s at the tip and 0 at the hand");
      Config e = edge; e.edgeSpeed = 1.0f;
      Core k; check(drive(k, e, 600, 90, still).fires == 0, "edge cannot see it at any threshold: the hand never moved"); }
    { // The opposite: the hand sweeps at 3 m/s while the wrist lets the blade trail.
      auto sweep = [](double t) { return t < 200 ? 3.0f : 0.0f; };
      auto trail = [](double t) { return t < 200 ? 0.4f - 4.0f * (float)t * 0.001f : -0.4f; };
      const Blade r = blade(con, 500, sweep, trail, always);
      check(r.fires == 0 && r.tipPeak < 1.3f && r.handPeak > 2.9f, "contact: a fast hand with a trailing blade is not a cut: 3 m/s at the hand, under 1.3 at the tip");
      Config e = edge; e.edgeSpeed = 2.5f;
      Core k; check(drive(k, e, 500, 90, sweep).fires == 1, "edge at 2.5 attacks on that hand: tip speed is not hand speed"); }
    { const Blade r = blade(con, 1199, humps(6.0f, 200), nullptr, always);
      check(r.fires == 3 && r.blocks == 0, "contact: a blade that stays inside the target is one attack per swing, not one per sample"); }
    { Config c = con; c.cooldownMs = 600;
      const Blade r = blade(c, 1199, humps(6.0f, 200), nullptr, always);
      check(r.fires == 2 && r.cooldown == 1, "contact: the cooldown is the one edge has"); }
    { const Blade r = blade(con, 399, humps(6.0f, 200), nullptr, slab(0.30f, 0.50f), true, 11.0f, kGateSword);
      check(r.fires == 0 && r.gate == 1 && r.blocks == 1, "contact: a closed gate blocks once per swing and is named"); }
    { // A hand held still while the wrist shakes the blade inside a target, 6.7 times a second.
      auto shake = [](double t) { return 0.4f * (float)std::sin(2.0 * 3.14159265358979 * t / 150.0); };
      const Blade r = blade(con, 2000, still, shake, always);
      check(r.fires == 1 && r.blocks <= 1, "contact: a shaking blade that never slows is one attack and at most one line, not one per sample");
      auto slow = [](double t) { return 0.4f * (float)std::sin(2.0 * 3.14159265358979 * t / 600.0); };
      const Blade q = blade(con, 3000, still, slow, always);
      std::printf("contact: a blade rocked 10 times in 3 s inside a target: %d attack(s), %d block line(s), tip peak %.2f m/s\n", q.fires, q.blocks, q.tipPeak);
      check(q.fires >= 1 && q.fires <= 10 && q.blocks <= 10,
            "contact: rocked slowly, at most one attack and one line per rock (a rock that crosses inside the cooldown is named, then attacks when it ends, as under edge)"); }

    // Who owns the decision when the blade's answer is missing: the hand, as edge.
    { const Blade old = blade(con, 399, humps(6.0f, 200), nullptr, nullptr, true, 150.0f);
      check(old.fires == 1 && old.owner == kOwnerFallback && old.known == 0 && old.fallback > 0,
            "contact: an answer 150 ms old (the limit is 100) is no answer: the hand's speed decides, as edge, and says so");
      const Blade none = blade(con, 399, humps(6.0f, 200), nullptr, always, false);
      check(none.fires == 1 && none.owner == kOwnerFallback, "contact: with no answer at all, the same");
      const Blade noTip = blade(con, 399, humps(6.0f, 200), nullptr, always, true, 11.0f, 0, false);
      check(noTip.fires == 1 && noTip.owner == kOwnerFallback, "contact: with no blade measured, the same");
      Config c = con; c.edgeSpeed = 7.0f;
      const Blade under = blade(c, 399, humps(6.0f, 200), nullptr, always, false);
      check(under.fires == 0, "and the fallback obeys EdgeSpeed, not ContactSpeed: 6 m/s under a 7.0 threshold is nothing");
      Config m = con; m.contactMaxAgeMs = 200.0f;
      const Blade lever = blade(m, 399, humps(6.0f, 200), nullptr, nullptr, true, 150.0f);
      check(lever.fires == 0 && lever.known > 0, "ContactMaxAgeMs is the lever: at 200 the 150 ms answer counts, and empty air is nothing again"); }
    { // The answer arrives halfway through the swing.
      Core k; int fires = 0, cuts = 0, owner = -1; const double dt = 1000.0 / 90.0; float x = 0;
      for (double at = 0; at <= 500; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
          s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = at > 60.0; s.contactAgeMs = 11.0f;
          const Verdict v = k.feed(s, con); if (v.fired) { ++fires; owner = v.owner; } if (v.humpEnd && v.humpCut) ++cuts;
          x += humps(3.2f, 200)(at) * (float)(dt * 0.001); }
      check(fires == 0 && cuts == 1, "contact: a hump is cut where the owner changes, and a 3.2 m/s swing in the air under both owners attacks under neither");
      (void)owner; }

    // A dropped sample is not a change of owner.
    { auto dropout = [&](float graceMs, double from, double to) { Core k; int fires = 0, holds = 0, owner = -1; const double dt = 1000.0 / 90.0; float x = 0;
          Config c = con; c.contactGraceMs = graceMs;
          for (double at = 0; at <= 399; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
              s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = !(at >= from && at < to); s.contactAgeMs = 11.0f;
              const Verdict v = k.feed(s, c); if (v.fired) { ++fires; owner = v.owner; } if (v.hold) ++holds;
              x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
          return fires * 100 + holds; };
      { Core k; Config c = con; int holdBlocks = 0, blocks = 0; const double dt = 1000.0 / 90.0; float x = 0;
        for (double at = 0; at <= 399; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
            s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = at < 60.0; s.contactAgeMs = 11.0f;
            const Verdict v = k.feed(s, c); if (v.block) { ++blocks; if (v.block == kBlockHold) ++holdBlocks; }
            x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
        check(holdBlocks == 1 && blocks == 1, "contact: a swing that would have attacked under edge while nobody decides is named once, as held"); }
      check(dropout(250.0f, 95.0, 106.0) == 1, "contact: one missing answer in the middle of a swing through the air attacks nothing: nobody decides for that sample");
      check(dropout(0.0f, 95.0, 106.0) / 100 == 1, "and with no grace that one sample hands the swing to the hand, which attacks: the grace is what decided it");
      check(dropout(250.0f, 0.0, 1.0e9) / 100 == 1, "an answer that was NEVER there is not held for: the hand decides from the first sample"); }
    { // The answer goes missing for good, mid-session: the hand takes over after the grace.
      Core k; int fires = 0, owner = -1, holds = 0; const double dt = 1000.0 / 90.0; float x = 0;
      for (double at = 0; at <= 1199; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
          s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = at < 300.0; s.contactAgeMs = 11.0f;
          const Verdict v = k.feed(s, con); if (v.fired) { ++fires; owner = v.owner; } if (v.hold) ++holds;
          x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
      check(fires == 1 && owner == kOwnerFallback && holds > 15 && holds < 30,
            "contact: three swings, the answer lost at 300 ms: the first touches nothing, the second falls in the 250 ms nobody decides, the third is the hand's"); }

    // A hold lasts as long as the adapter says, and the hand never takes it over.
    { Core k; int fires = 0, holds = 0, fallbacks = 0; const double dt = 1000.0 / 90.0; float x = 0;
      for (double at = 0; at <= 1999; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
          s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = true; s.contactAgeMs = 11.0f;
          s.contact = true; s.contactHold = at >= 100.0;
          const Verdict v = k.feed(s, con); if (v.fired) ++fires; if (v.hold) ++holds; if (v.fallback) ++fallbacks;
          x += (at < 200.0 ? 0.0f : humps(6.0f, 200)(at)) * (float)(dt * 0.001); }
      check(fires == 0 && fallbacks == 0 && holds > 160,
            "contact: while the adapter holds (the game is re-posing the sword) five 6 m/s swings in two seconds attack nothing, and the hand never takes over"); }
    { Core k; int fires = 0, owner = -1; const double dt = 1000.0 / 90.0; float x = 0;
      for (double at = 0; at <= 1199; at += dt) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = x; s.hand[1] = 1.2f;
          s.tip[0] = x; s.tip[1] = 1.2f; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = true; s.contactAgeMs = 11.0f;
          s.contact = at >= 800.0; s.contactHold = at >= 100.0 && at < 700.0;
          const Verdict v = k.feed(s, con); if (v.fired) { ++fires; owner = v.owner; }
          x += humps(6.0f, 200)(at) * (float)(dt * 0.001); }
      check(fires == 1 && owner == kOwnerContact, "contact: when the hold ends the blade decides again, and the next cut that reaches something is the contact's"); }

    // The air swing lever.
    { Config c = con; c.contactAirSpeed = 8.0f;
      const Blade fast = blade(c, 399, humps(10.0f, 200), nullptr, nullptr);
      check(fast.fires == 1 && fast.owner == kOwnerAir, "ContactAirSpeed=8: a 10 m/s swing in the air attacks, owned by the air rule");
      const Blade soft = blade(c, 399, humps(6.0f, 200), nullptr, nullptr);
      check(soft.fires == 0, "and a 6 m/s one does not");
      const Blade off = blade(con, 399, humps(10.0f, 200), nullptr, nullptr);
      check(off.fires == 0, "at 0, which is what ships, no swing in the air ever attacks"); }

    // Sample hygiene for the tip.
    { // The blade is re-measured between two samples: the tip alone moves 0.5 m in one frame.
      const Blade r = blade(con, 500, still, nullptr, always, true, 11.0f, 0, true, [](double t) { return t > 200 ? 0.5f : 0.0f; });
      check(r.fires == 0 && r.tipPeak < 0.01f, "contact: a tip that moves 45 m/s in one sample is a re-measured blade, not a cut"); }
    { // The controller is re-acquired somewhere else while the blade touches.
      Core k; int fires = 0, jumps = 0; const double dt = 1000.0 / 90.0; int i = 0;
      for (double at = 0; at <= 300; at += dt, ++i) { Sample s; s.handValid = true; s.tMs = at; s.hand[0] = i >= 10 ? 1.0f : 0.0f;
          s.tip[0] = s.hand[0]; s.tip[2] = -0.62f; s.tipValid = true; s.contactValid = true; s.contact = true; s.contactAgeMs = 11.0f;
          const Verdict v = k.feed(s, con); if (v.fired) ++fires; if (v.jump) ++jumps; }
      check(fires == 0 && jumps == 1, "contact: a tracking jump re-seeds the tip with the hand, and the blade it carried attacks nothing"); }
    { Config c = con; c.rearmSpeed = 5.0f;
      check(std::fabs(effective_contact_rearm(c) - 1.8f) < 1e-4f, "contact: the re-arm level is capped at 0.9 x ContactSpeed");
      check(blade(c, 1199, humps(6.0f, 200), nullptr, always).fires == 3, "so a re-arm level typed above it still re-arms"); }

    // The sneak kill runs under contact as it does under edge.
    { Config c = con; c.stab = true; c.stabStyle = kThrust;
      const Stab r = body(c, 500, true, [&](double t) { return V3{ 0, 0, -hump(2.2f, 200, t) }; });
      check(r.stab == 1 && r.slash == 0, "contact: a thrust while sneaking is still the sneak kill"); }

    // A detector value nobody defined is the detector that ships.
    { Config c; c.detector = 7; Core k;
      const Tally t = drive(k, c, 400, 90, [](double ms) { return ms < 50.0 ? 5.0f : 0.0f; });
      check(t.fires == 1 && t.flicks == 0, "an unknown detector value decides as edge does (a 50 ms flick at 5 m/s attacks), never as the retired sustain"); }

    // The pre-VR-37 detector, kept as the live A/B.
    Config sus; sus.detector = kSustain;
    { Core k; const Tally t = drive(k, sus, 600, 90, humps(4.0f, 300));
      check(t.fires == 1 && t.lastRunMs >= 120.0f, "sustain: a real swing fires once the run has lasted and travelled"); }
    { Core k; const Tally t = drive(k, sus, 400, 90,
          [](double ms) { return ms < 50.0 ? 3.0f : 0.0f; });
      check(t.fires == 0 && t.flicks == 1, "sustain: a 50 ms flick is rejected and reported once"); }
    { Core k; const Tally t = drive(k, sus, 400, 90,
          [](double ms) { return ms < 90.0 ? 3.0f : ms < 110.0 ? 1.5f : ms < 250.0 ? 3.0f : 0.0f; });
      check(t.fires == 1 && t.flicks == 0, "sustain: a brief dip inside a swing does not end the run"); }
    { Core k; const Tally t = drive(k, sus, 600, 90, humps(4.0f, 300), nullptr, kGateTracking);
      check(t.fires == 0 && t.gate == 1, "sustain: a closed gate is named once per run"); }
    { Core k; Config c = sus; c.sustainSpeed = 1.8f;
      check(drive(k, c, 2000, 90, [](double) { return 1.2f; }).fires == 0, "sustain: a steady reach below the gate never starts a run"); }

    // The simulated swing is a position train whose speed is the hump it claims.
    { float maxV = 0; const double dt = 1000.0 / 90.0;
      for (double t = dt; t < 800.0; t += dt) {
          const float v = std::fabs(sim_offset(5.0f, 200.0f, t) - sim_offset(5.0f, 200.0f, t - dt)) / (float)(dt * 0.001);
          if (v > maxV) maxV = v; }
      check(maxV > 4.9f && maxV < 5.01f, "swing sim: the synthesised hand peaks at the speed asked for");
      check(std::fabs(sim_offset(5.0f, 200.0f, 800.0)) < 1e-3f, "swing sim: two humps bring the hand back where it started");
      Core k; Tally t; Config c = edge;
      for (double at = 0; at < 1200.0; at += dt) { Sample s; s.handValid = true; s.tMs = at;
          s.hand[0] = sim_offset(5.0f, 200.0f, at); if (k.feed(s, c).fired) ++t.fires; }
      check(t.fires == 3, "swing sim: three humps through the real core are three attacks"); }

    std::printf("swing-core: %d checks passed\n", checks);
    return 0;
}
