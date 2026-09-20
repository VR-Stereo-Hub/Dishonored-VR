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
    Config sc = edge; sc.stab = true;

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
