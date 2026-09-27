#pragma once
// Controller bind remapping. The runtime layer publishes the PHYSICAL controller state
// (core/vr/input_snapshot.h); this turns it into the LOGICAL snapshot the pad bridge and
// every gameplay system reads, where each field means a game action instead of a button:
// `a` is "whatever the player bound Jump to", `gripL` is "the power wheel's source", and so
// on. Moving the snapshot rather than the XInput bits is what makes the mod's own systems
// follow a remap for free: slide assist and physical crouch read Stealth, the sword and the
// carry swap read Attack/Power, the wheel gates read PowerWheel, the health hold reads Health.
//
// With every action on its shipped source and the sticks unswapped, apply() returns its input
// unchanged, bit for bit (tools/controller-binds-tests.cpp proves it over every button combo).
//
// Deliberately NOT remapped, because they are physical gestures rather than game actions:
// the thumbrests (D-pad modifier), the both-stick-clicks chord (recenter / F10 panel, resolved
// in the runtime layer before this), and the F10 pointer (it reads the physical snapshot).
#include "core/vr/input_snapshot.h"
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>

namespace dvr::binds {

enum Source : uint8_t {
    None = 0, A, B, X, Y, LeftStickClick, RightStickClick, Menu,
    LeftGrip, RightGrip, LeftTrigger, RightTrigger, SourceCount
};
enum Action : uint8_t {
    Jump = 0, Stealth, Interact, Lean, Sprint, Health, PauseJournal,
    PowerWheel, Choke, Power, Attack, ActionCount
};

struct ActionInfo { const char* key; const char* label; Source def; bool analog; const char* tip; };
inline const ActionInfo& info(int a) {
    static const ActionInfo k[ActionCount] = {
        { "Jump",         "Jump",                   A,               false, "Jump, mantle, and confirm in menus." },
        { "Stealth",      "Stealth (crouch)",       B,               false, "Crouch toggle; at full run it slides. Physical crouch presses it for you." },
        { "Interact",     "Interact",               X,               false, "Use, pick up, open. With Lean it forms the pause chord when that is on." },
        { "Lean",         "Lean / adrenaline",      Y,               false, "Hold and push the opposite stick to lean." },
        { "Sprint",       "Sprint",                 LeftStickClick,  false, "Sprint toggle." },
        { "Health",       "Health elixir (hold)",   RightStickClick, false, "Hold to drink a health elixir." },
        { "PauseJournal", "Pause / journal",        Menu,            false, "Tap to pause, hold (or D-pad modifier + press) for the journal." },
        { "PowerWheel",   "Power wheel (hold)",     LeftGrip,        true,  "Hold to open the wheel; the hand or a stick picks." },
        { "Choke",        "Choke / block",          RightGrip,       true,  "Choke from behind, or block." },
        { "Power",        "Use power (left hand)",  LeftTrigger,     true,  "The left hand's power or gadget." },
        { "Attack",       "Attack (right hand)",    RightTrigger,    true,  "The right hand's sword, gun or crossbow. The motion sword adds to it." },
    };
    return k[(a >= 0 && a < ActionCount) ? a : 0];
}

inline const char* source_key(int s) {
    static const char* k[SourceCount] = { "None", "A", "B", "X", "Y", "LeftStickClick", "RightStickClick",
        "Menu", "LeftGrip", "RightGrip", "LeftTrigger", "RightTrigger" };
    return (s >= 0 && s < SourceCount) ? k[s] : "None";
}
inline const char* source_label(int s) {
    static const char* k[SourceCount] = { "(unbound)", "A", "B", "X", "Y", "Left stick click", "Right stick click",
        "Menu button", "Left grip", "Right grip", "Left trigger", "Right trigger" };
    return (s >= 0 && s < SourceCount) ? k[s] : "(unbound)";
}
// Case-insensitive; accepts the key names and a few short forms (L3, R3, LT, RT, LG, RG).
inline bool parse_source(const char* v, Source* out) {
    if (!v || !out) return false;
    while (*v == ' ' || *v == '\t') ++v;
    char t[32] = {}; int n = 0;
    for (; v[n] && n < 31 && v[n] != ' ' && v[n] != '\t' && v[n] != ';' && v[n] != '\r' && v[n] != '\n'; ++n)
        t[n] = (char)std::tolower((unsigned char)v[n]);
    if (!n) return false;
    struct Alias { const char* n; Source s; };
    static const Alias aliases[] = { { "l3", LeftStickClick }, { "r3", RightStickClick }, { "lt", LeftTrigger },
        { "rt", RightTrigger }, { "lg", LeftGrip }, { "rg", RightGrip }, { "", None }, { "0", None } };
    for (const auto& a : aliases) if (!std::strcmp(t, a.n)) { *out = a.s; return true; }
    for (int s = 0; s < SourceCount; ++s) {
        const char* k = source_key(s); int i = 0;
        while (k[i] && t[i] && (char)std::tolower((unsigned char)k[i]) == t[i]) ++i;
        if (!k[i] && !t[i]) { *out = (Source)s; return true; }
    }
    return false;
}
inline bool parse_action(const char* v, int* out) {
    if (!v || !out) return false;
    for (int a = 0; a < ActionCount; ++a) {
        const char* k = info(a).key; int i = 0;
        while (k[i] && v[i] && std::tolower((unsigned char)k[i]) == std::tolower((unsigned char)v[i])) ++i;
        if (!k[i] && (!v[i] || v[i] == ' ')) { *out = a; return true; }
    }
    return false;
}

// The whole layout, packed so every thread reads one consistent copy lock-free:
// 4 bits per action, bit 44 = the sticks swapped.
struct Layout {
    Source src[ActionCount];
    bool swapSticks = false;
    Layout() { for (int a = 0; a < ActionCount; ++a) src[a] = info(a).def; }
    bool is_default() const {
        if (swapSticks) return false;
        for (int a = 0; a < ActionCount; ++a) if (src[a] != info(a).def) return false;
        return true;
    }
};
constexpr int kSwapBit = 4 * ActionCount;
inline uint64_t pack(const Layout& l) {
    uint64_t v = 0;
    for (int a = 0; a < ActionCount; ++a) v |= uint64_t(l.src[a] < SourceCount ? l.src[a] : None) << (4 * a);
    if (l.swapSticks) v |= uint64_t(1) << kSwapBit;
    return v;
}
inline Layout unpack(uint64_t v) {
    Layout l;
    for (int a = 0; a < ActionCount; ++a) { const unsigned s = unsigned(v >> (4 * a)) & 15u; l.src[a] = s < SourceCount ? (Source)s : None; }
    l.swapSticks = ((v >> kSwapBit) & 1) != 0;
    return l;
}
inline std::atomic<uint64_t> layoutBits{pack(Layout{})};
inline Layout layout() { return unpack(layoutBits.load(std::memory_order_relaxed)); }
inline void configure(const Layout& l) { layoutBits.store(pack(l), std::memory_order_relaxed); }

// F10 press-to-bind: until this tick (GetTickCount64 ms) the pad bridge sends the game no
// buttons, grips or triggers, so the press being captured does not also jump or fire. A
// deadline rather than a flag: a panel closed or a tab switched mid-capture cannot leave the
// pad muted, it expires on its own.
inline std::atomic<uint64_t> captureUntilMs{0};
inline bool capture_active(uint64_t nowMs) { return nowMs < captureUntilMs.load(std::memory_order_relaxed); }

// A physical source's value, 0..1. Buttons read 1 or 0.
inline float value(const dvr::vr::InputSnapshot& s, int src) {
    switch (src) {
    case A: return s.a ? 1.0f : 0.0f;
    case B: return s.b ? 1.0f : 0.0f;
    case X: return s.x ? 1.0f : 0.0f;
    case Y: return s.y ? 1.0f : 0.0f;
    case LeftStickClick:  return s.clkL ? 1.0f : 0.0f;
    case RightStickClick: return s.clkR ? 1.0f : 0.0f;
    case Menu:        return s.menu ? 1.0f : 0.0f;
    case LeftGrip:    return s.gripL;
    case RightGrip:   return s.gripR;
    case LeftTrigger: return s.trigL;
    case RightTrigger:return s.trigR;
    default: return 0.0f;
    }
}
// A button action on an analog source presses past half travel. Stateless on purpose: two
// threads (the pad bridge, the UI surface's wheel check) apply the same layout.
constexpr float kPressAt = 0.5f;

// Physical -> logical. `muted` is a source whose value reads 0 for every action (the F10
// pointer's trigger while the panel is up), None for none.
inline dvr::vr::InputSnapshot apply(const dvr::vr::InputSnapshot& p, const Layout& l, Source muted = None) {
    if (l.is_default() && muted == None) return p;   // the shipped layout: untouched
    dvr::vr::InputSnapshot o = p;
    if (l.swapSticks) for (int i = 0; i < 2; ++i) { o.mv[i] = p.lk[i]; o.lk[i] = p.mv[i]; }
    auto v = [&](int a) { const Source s = l.src[a]; return s == muted ? 0.0f : value(p, s); };
    auto on = [&](int a) { return v(a) > kPressAt; };
    o.a = on(Jump); o.b = on(Stealth); o.x = on(Interact); o.y = on(Lean);
    o.clkL = on(Sprint); o.clkR = on(Health); o.menu = on(PauseJournal);
    o.gripL = v(PowerWheel); o.gripR = v(Choke); o.trigL = v(Power); o.trigR = v(Attack);
    return o;
}

// The first physical source pressed in `now` that was up in `before` (press-to-bind).
inline Source first_press(const dvr::vr::InputSnapshot& before, const dvr::vr::InputSnapshot& now) {
    for (int s = A; s < SourceCount; ++s)
        if (value(now, s) > 0.6f && value(before, s) <= 0.3f) return (Source)s;
    return None;
}
inline bool any_down(const dvr::vr::InputSnapshot& s) {
    for (int src = A; src < SourceCount; ++src) if (value(s, src) > 0.3f) return true;
    return false;
}

// Actions sharing a source with `a` (bit mask), for the conflict warning. None never conflicts.
inline unsigned conflicts(const Layout& l, int a) {
    unsigned m = 0;
    if (l.src[a] == None) return 0;
    for (int b = 0; b < ActionCount; ++b) if (b != a && l.src[b] == l.src[a]) m |= 1u << b;
    return m;
}

} // namespace dvr::binds
