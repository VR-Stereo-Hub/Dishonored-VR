// Host tests for controller bind remapping (src/core/input/controller_binds.h). Never launches
// the game. The regression that matters: the shipped layout must hand the pad bridge the
// physical snapshot unchanged, bit for bit, for every button combination.
#include "core/input/controller_binds.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace dvr::binds;
using dvr::vr::InputSnapshot;
static int checks = 0;
static void check(bool ok, const char* why) { ++checks; if (!ok) { std::printf("FAIL %s\n", why); std::exit(1); } }
static bool same(const InputSnapshot& a, const InputSnapshot& b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
static InputSnapshot combo(unsigned m) {
    InputSnapshot s; s.active = (m & 1) != 0;
    s.a = m & 2; s.b = m & 4; s.x = m & 8; s.y = m & 16; s.clkL = m & 32; s.clkR = m & 64; s.menu = m & 128;
    s.restL = m & 256; s.restR = m & 512;
    s.gripL = (m & 1024) ? .95f : .1f; s.gripR = (m & 2048) ? .8f : 0.f;
    s.trigL = (m & 4096) ? .3f : 0.f; s.trigR = (m & 8192) ? 1.f : .45f;
    s.mv[0] = .25f; s.mv[1] = -.5f; s.lk[0] = -.75f; s.lk[1] = .125f;
    return s;
}
int main() {
    static_assert(ActionCount * 4 + 1 <= 64, "layout packs into 64 bits");
    const Layout def;
    check(def.is_default(), "a fresh layout is the shipped one");
    for (unsigned m = 0; m < (1u << 14); ++m) {
        const InputSnapshot p = combo(m);
        check(same(apply(p, def), p), "shipped layout passes every combo through untouched");
    }
    // An edit undone is the shipped layout again, and passes through again.
    for (unsigned m = 0; m < (1u << 14); ++m) {
        const InputSnapshot p = combo(m);
        Layout l; l.src[Jump] = B; l.src[Jump] = A;   // edited back: still default
        check(l.is_default() && same(apply(p, l), p), "an edit undone is the shipped layout");
    }
    // Pack/unpack round-trip for every source in every slot, and the swap bit.
    for (int a = 0; a < ActionCount; ++a) for (int s = 0; s < SourceCount; ++s) {
        Layout l; l.src[a] = (Source)s; l.swapSticks = (a + s) & 1;
        const Layout r = unpack(pack(l));
        check(std::memcmp(r.src, l.src, sizeof(l.src)) == 0 && r.swapSticks == l.swapSticks, "pack round-trip");
    }
    // Parsing: every key name and the short forms, case-insensitive; junk refused.
    for (int s = 0; s < SourceCount; ++s) { Source o; check(parse_source(source_key(s), &o) && o == s, "source key parses"); }
    { Source o; check(parse_source("r3", &o) && o == RightStickClick, "R3 alias");
      check(parse_source(" lefttrigger ; c", &o) && o == LeftTrigger, "case and trailing comment");
      check(!parse_source("Grip", &o) && !parse_source("AA", &o), "junk refused"); }
    for (int a = 0; a < ActionCount; ++a) { int o = -1; check(parse_action(info(a).key, &o) && o == a, "action key parses"); }
    // A remap moves the ACTION: Jump on B, Stealth on A (a swap).
    { Layout l; l.src[Jump] = B; l.src[Stealth] = A;
      InputSnapshot p = combo(1 | 4); auto o = apply(p, l); check(o.a && !o.b, "B press -> Jump");
      p = combo(1 | 2); o = apply(p, l); check(!o.a && o.b, "A press -> Stealth"); }
    // Button action on an analog source presses past half travel; analog action on a button is 0/1.
    { Layout l; l.src[Jump] = RightTrigger; l.src[Attack] = A;
      InputSnapshot p = combo(1); p.trigR = .49f; check(!apply(p, l).a, "trigger under half: no jump");
      p.trigR = .51f; check(apply(p, l).a, "trigger past half: jump");
      p = combo(1 | 2); check(apply(p, l).trigR == 1.f, "button drives the attack trigger fully");
      p = combo(1); check(apply(p, l).trigR == 0.f, "button up: no attack"); }
    // The grip keeps its analog value, so the pad bridge's 0.9 / 0.7 hysteresis still applies.
    { Layout l; l.src[PowerWheel] = RightGrip; l.src[Choke] = LeftGrip;
      InputSnapshot p = combo(1); p.gripL = .2f; p.gripR = .83f; auto o = apply(p, l);
      check(o.gripL == .83f && o.gripR == .2f, "grips swap with their values"); }
    // Unbound: never pressed.
    { Layout l; l.src[Lean] = None; auto o = apply(combo(1 | 16), l); check(!o.y, "unbound Lean never presses"); }
    // Sticks swap; thumbrests and `active` are never remapped.
    { Layout l; l.swapSticks = true; const auto p = combo(1 | 256); const auto o = apply(p, l);
      check(o.mv[0] == p.lk[0] && o.mv[1] == p.lk[1] && o.lk[0] == p.mv[0] && o.lk[1] == p.mv[1], "sticks swap");
      check(o.restL == p.restL && o.restR == p.restR && o.active == p.active, "rests and active pass through"); }
    // The F10 pointer mute silences its trigger for EVERY action bound to it.
    { Layout l; l.src[Jump] = RightTrigger; InputSnapshot p = combo(1 | 8192);
      auto o = apply(p, l, RightTrigger); check(!o.a && o.trigR == 0.f, "muted trigger drives nothing");
      o = apply(p, l); check(o.a && o.trigR == 1.f, "unmuted it drives both"); }
    // Press-to-bind: the first NEW press, never one already held.
    { InputSnapshot a = combo(1), b = combo(1 | 2);
      check(first_press(a, b) == A, "new A press captured");
      check(first_press(b, b) == None, "a held press is not a new one");
      a.trigL = 0; b = a; b.trigL = .7f; check(first_press(a, b) == LeftTrigger, "trigger past 0.6 captured");
      b.trigL = .5f; check(first_press(a, b) == None, "trigger at 0.5 is not a press yet");
      InputSnapshot z; check(!any_down(z), "idle: nothing down"); z.trigR = .45f; check(any_down(z), "a trigger resting at 0.45 is down"); }
    // Conflicts: both ways, and None never conflicts.
    { Layout l; l.src[Jump] = X; check(conflicts(l, Jump) == (1u << Interact) && conflicts(l, Interact) == (1u << Jump), "shared X");
      l.src[Jump] = None; l.src[Lean] = None; check(!conflicts(l, Jump) && !conflicts(l, Lean), "None is no conflict");
      check(!conflicts(Layout{}, Attack), "the shipped layout has no conflicts"); }
    std::printf("controller-binds: %d checks passed\n", checks);
    return 0;
}
