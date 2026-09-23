// game/dishonored/drop_assist.cpp - drop takedowns from above (VR-203). Included by the
// unity build after anim_state.cpp (it is sampled from the anim tick) and uses the
// name-keyed resolver, the live-object table and the right-hand item read.
//
// WHAT THE PLAYER SEES. Dropping onto a guard and attacking gives an ordinary slash
// instead of the drop kill, often enough that the move feels random. It was like
// that before the motion sword and the sword makes it worse, because a swing fires
// on the crossing of a speed threshold, early in the arm's motion, and a falling
// player swings early.
//
// WHY, FROM THE ENGINE (ENGINE_NOTES "Drop takedown timing"). The player's
// DisItemContext_DropAssassinate re-decides every tick while airborne and caches the
// answer: 0 no target, 1 target found but too high (the game will WAIT for it),
// 2 do it now. An attack press is routed by the cached answer at that moment. On 0
// the sword's next context, the ordinary attack, takes the press and the chance is
// gone for this fall. The archived VR-111 run shows exactly that: the attack
// started at type 0 and the same fall read type 2 about 30 ms later, then flickered
// 0/2 as the player landed on the target and was pushed off it.
//
// THE ASSIST. While the player is airborne with the sword in the right hand, an
// attack press that meets "no target" is HELD (not sent) for up to HoldMs. The
// first tick the game reports a target (type 1 or 2) the attack is pressed and the
// game's own drop kill runs, including its wait-while-too-high. If the player lands
// or HoldMs runs out first, the held attack is delivered as an ordinary one
// (Fallback=1) or dropped (Fallback=0). Nothing about the engine's decision changes:
// only WHEN the press arrives. An attack that meets type 1 or 2 is never touched.
//
// THE REACH LEVER (default 2.00; 1.00 = the shipped game). The trajectory the game traces
// for a target is the player's velocity plus gravity over the tweak's
// m_fHitWindowInSeconds. ReachScale multiplies that one tweak value, so the target
// is found earlier and further along the fall. The original value is kept per
// tweak object and written back when the scale returns to 1.
//
// LANES. sample() on the script lane publishes one record under a lock; gate() on the
// present lane reads it. Only sample() touches engine memory.
#include "game/dishonored/drop_assist.h"

namespace dvr::drop {
namespace {

struct Cfg { bool assist = true, fallback = true; float holdMs = 420.0f, reach = 2.0f; } cfg;   // the headset-tuned values (VR-203)
bool dropWatch = true;   // [Anim] DropWatch: the read-only decision trace (VR-111)

// ---- the published decision (script lane writes, present lane reads) ----
struct Pub {
    unsigned long long stamp = 0;
    bool known = false, airborne = false, targetLive = false;
    unsigned char status = 255, type = 255;
    float vz = 0.0f;
};
SRWLOCK pubLock = SRWLOCK_INIT;
Pub pub;
Pub published() { AcquireSRWLockShared(&pubLock); Pub p = pub; ReleaseSRWLockShared(&pubLock); return p; }

// ---- the engine side (script lane only) ----
struct Offs { bool tried = false; uint32_t owner = 0, status = 0, type = 0, target = 0, tag = 0, wait = 0,
              tweaks = 0, velocity = 0, hit = 0, minDrop = 0, maxDrop = 0, maxJump = 0, minDown = 0, ray = 0; } off;
uint8_t* context = nullptr;
uint32_t scan = 0, slot = 0;
struct Tweak { uint8_t* obj = nullptr; float original = 0.0f; } tweakRecs[8];
volatile LONG reachDirty = 1;   // re-apply after a config or F10 change

bool rd(uint8_t* obj, uint32_t o, void* dst, size_t n) {
    if (!obj || !o || !RangeReadable(obj + o, n)) return false;
    memcpy(dst, obj + o, n); return true;
}
uint8_t* live_ptr(uint8_t* obj, uint32_t o) {
    uint8_t* p = nullptr;
    return rd(obj, o, &p, sizeof(p)) && p && IsLiveObject(p) ? p : nullptr;
}
void resolve() {
    if (off.tried) return;
    off.tried = true;
    off.owner    = RflOffsetOf("DisItemContext_DropAssassinate", "m_pPlayerOwner");
    off.status   = RflOffsetOf("DisItemContext", "m_ContextStatus");
    off.type     = RflOffsetOf("DisItemContext_DropAssassinate", "m_CachedDropType");
    off.target   = RflOffsetOf("DisItemContext_DropAssassinate", "m_pCachedTarget");
    off.tag      = RflOffsetOf("DisItemContext_DropAssassinate", "m_TickTagAtWhichCacheIsValid");
    off.wait     = RflOffsetOf("DisItemContext_DropAssassinate", "m_pPawnToWaitFor");
    off.tweaks   = RflOffsetOf("DisItemContext", "m_pContextTweaks");
    off.velocity = RflOffsetOf("Actor", "Velocity");
    off.hit      = RflOffsetOf("DisTweaks_DropAssassinate", "m_fHitWindowInSeconds");
    off.minDrop  = RflOffsetOf("DisTweaks_DropAssassinate", "m_fMinDropDistToTarget");
    off.maxDrop  = RflOffsetOf("DisTweaks_DropAssassinate", "m_fMaxDropDistToTarget");
    off.maxJump  = RflOffsetOf("DisTweaks_DropAssassinate", "m_fMaxAllowedDropJumpVel");
    off.minDown  = RflOffsetOf("DisTweaks_DropAssassinate", "m_fMinDropDownVel");
    off.ray      = RflOffsetOf("DisTweaks_DropAssassinate", "m_fRayScalePercent");
    // The disassembly of the decision (0x00C09810/0x00C09900) reads the tweak
    // floats at +0x528..+0x538 and the cached type at context+0xC0. Say whether the
    // names landed on the same fields; a mismatch means one of the two routes is wrong.
    const bool layout = off.hit == 0x528 && off.minDrop == 0x52C && off.maxDrop == 0x530 &&
                        off.maxJump == 0x534 && off.minDown == 0x538 && off.type == 0xC0;
    Log("drop: resolved owner=%x status=%x type=%x target=%x tag=%x wait=%x tweaks=%x velocity=%x | tweak hit=%x "
        "minDrop=%x maxDrop=%x maxJump=%x minDown=%x ray=%x -> %s", off.owner, off.status, off.type, off.target,
        off.tag, off.wait, off.tweaks, off.velocity, off.hit, off.minDrop, off.maxDrop, off.maxJump, off.minDown,
        off.ray, layout ? "matches the disassembly (tweaks +0x528.., type +0xC0)"
                        : "DIFFERS from the disassembly - the reach lever is refused, the assist still reads by name");
    if (!layout) off.hit = 0;   // never scale a field we cannot tie to the traced one
}
// Shipped values once per tweak object, and the reach lever's write.
void tweak(uint8_t* t) {
    if (!t || !off.hit) return;
    const char* cls = ObjClassName(t);
    if (!cls || !strstr(cls, "DropAssassinate")) return;
    Tweak* rec = nullptr;
    for (Tweak& r : tweakRecs) if (r.obj == t) { rec = &r; break; }
    if (!rec) {
        float v[6] = {};
        const uint32_t o[6] = { off.hit, off.minDrop, off.maxDrop, off.maxJump, off.minDown, off.ray };
        for (int i = 0; i < 6; ++i) rd(t, o[i], &v[i], 4);
        Log("drop: shipped tweaks (%s) HitWindow=%.3f s MinDropDist=%.1f MaxDropDist=%.1f MaxAllowedDropJumpVel=%.1f "
            "MinDropDownVel=%.1f RayScalePercent=%.3f", cls, v[0], v[1], v[2], v[3], v[4], v[5]);
        if (!(v[0] > 0.0f && v[0] < 5.0f)) {
            Log("drop: reach lever refused: HitWindow %.3f is outside 0..5 s, so it is not the field the lever was "
                "derived for", v[0]);
            return;
        }
        static unsigned next = 0;
        rec = &tweakRecs[next++ % 8];
        rec->obj = t; rec->original = v[0];
        InterlockedExchange(&reachDirty, 1);
    }
    if (!InterlockedExchange(&reachDirty, 0)) return;
    const float want = rec->original * cfg.reach;
    float now = 0.0f;
    if (!rd(t, off.hit, &now, 4) || fabsf(now - want) < 1e-5f) return;
    if (!RangeReadable(t + off.hit, 4)) return;
    *(float*)(t + off.hit) = want;
    Log("drop: reach x%.2f -> HitWindow %.3f s (shipped %.3f s)%s", cfg.reach, want, rec->original,
        cfg.reach == 1.0f ? " - restored" : "");
}
bool airborne(const char* master) {
    return !strcmp(master, "StatePlayerMasterFalling") || !strcmp(master, "StatePlayerMasterJump");
}

// ---- the gate (present lane only) ----
enum Phase { Idle, Holding, Pressing, WaitLow, Swallow };
struct GateState { Phase phase = Idle; bool prev = false; double holdMs = 0, pressMs = 0; long polls0 = 0;
                   bool heldSwing = false; } g;
struct Counts { unsigned held = 0, intoDrop = 0, fallback = 0, discarded = 0, native = 0, refused = 0; } n;
char lastEvent[160] = "none yet";
const char* phase_name() {
    return g.phase == Idle ? "idle" : g.phase == Holding ? "holding" : g.phase == Pressing ? "pressing"
         : g.phase == WaitLow ? "wait-release" : "swallow";
}
const char* type_name(unsigned t) {
    return t == 0 ? "no target" : t == 1 ? "too high, the game waits" : t == 2 ? "do it now" : "unknown";
}
constexpr double kPressMs = 100.0;   // at least this long, and at least two pad polls
constexpr unsigned long long kFreshMs = 150;
void event(const char* fmt, ...) {
    va_list a; va_start(a, fmt); _vsnprintf_s(lastEvent, sizeof(lastEvent), _TRUNCATE, fmt, a); va_end(a);
    Log("drop/assist: %s", lastEvent);
}
bool sword_in_hand() {
    const LONG tick = InterlockedCompareExchange(&g_rflPrimaryKindTick, 0, 0);
    const unsigned age = tick ? (unsigned)(GetTickCount() - (DWORD)tick) : 0xffffffffu;
    return InterlockedCompareExchange(&g_rflPrimaryKind, 0, 0) == 1 && age <= 1000u;
}
float clampf(float v, float lo, float hi) { return !(v == v) ? lo : v < lo ? lo : v > hi ? hi : v; }

} // namespace

void sample(uint8_t* pawn, const dvr::anim::Snapshot& s) {
    if (!(cfg.assist || dropWatch || cfg.reach != 1.0f) || !s.valid || !pawn) return;
    resolve();
    const auto now = GetTickCount64();
    if (!off.owner || !off.status || !off.type || !off.target || !off.tag || !off.velocity ||
        !RangeReadable((void*)kGObjHdr, 12)) return;
    auto** objects = *(uint8_t***)(kGObjHdr);
    const uint32_t count = *(uint32_t*)(kGObjHdr + 4);
    if (!objects || !count || count > 4000000) return;
    uint8_t* owner = nullptr;
    if (context && (slot >= count || !RangeReadable(objects + slot, sizeof(void*)) || objects[slot] != context ||
        !IsLiveObject(context) || !rd(context, off.owner, &owner, sizeof(owner)) || owner != pawn)) context = nullptr;
    // Discovery is bounded: 1024 slots per tick, resumed where it stopped.
    for (unsigned budget = 0; !context && budget < 1024; ++budget) {
        if (scan >= count) { scan = 0; break; }
        const uint32_t i = scan++;
        if (!RangeReadable(objects + i, sizeof(void*))) break;
        auto* c = objects[i];
        if (!IsLiveObject(c)) continue;
        const char* cls = ObjClassName(c);
        if (!cls || !strstr(cls, "ItemContext") || !strstr(cls, "DropAssassinate")) continue;
        if (!rd(c, off.owner, &owner, sizeof(owner)) || owner != pawn) continue;
        context = c; slot = i;
        Log("drop/watch: current player context discovered slot=%u class=%s", slot, cls);
    }
    Pub p; p.stamp = now; p.airborne = airborne(s.state[0]);
    int tag = -1; uint8_t* target = nullptr; float velocity[3] = {};
    p.known = context && rd(context, off.status, &p.status, 1) && p.status < 4 &&
              rd(context, off.type, &p.type, 1) && p.type < 3 && rd(context, off.tag, &tag, 4) &&
              rd(context, off.target, &target, sizeof(target)) && rd(pawn, off.velocity, velocity, sizeof(velocity));
    p.targetLive = p.known && target && IsLiveObject(target);
    p.vz = velocity[2];
    AcquireSRWLockExclusive(&pubLock); pub = p; ReleaseSRWLockExclusive(&pubLock);
    if (context && off.tweaks) tweak(live_ptr(context, off.tweaks));

    // The VR-111 trace, unchanged in meaning: every change, and a beat.
    static int previous = -1;
    static unsigned long long beat = 0, nextLog = 0;
    if (!dropWatch || now < nextLog) return;
    const int key = p.known ? p.status * 8 + p.type : -1;
    if (key != previous || now >= beat) {
        nextLog = now + 20;
        Log("drop/watch: known=%d status=%u (0 idle 1 failed 2 active 3 finished) type=%u "
            "(0 no-drop 1 too-high 2 do-now) cacheTick=%d targetLive=%d velocity=%.1f/%.1f/%.1f "
            "master=%s upper=%s; cached native decision, not a forced attack",
            p.known, (unsigned)p.status, (unsigned)p.type, tag, p.targetLive,
            velocity[0], velocity[1], velocity[2], s.state[0], s.state[1]);
        previous = key; beat = now + (p.airborne ? 100 : 1000);
    }
}

Gate gate(bool attack, bool swingPulse, long padPolls) {
    const double now = MaimNowMs();
    const bool rise = attack && !g.prev;
    g.prev = attack;
    if (!cfg.assist) { g.phase = Idle; return Pass; }
    const Pub d = published();
    const bool fresh = d.stamp && GetTickCount64() - d.stamp <= kFreshMs;
    switch (g.phase) {
    case Idle: {
        if (!rise) return Pass;
        if (!fresh || !d.known || !d.airborne) return Pass;   // on the ground: not ours
        if (!sword_in_hand()) {
            ++n.refused;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
                "drop/assist: airborne attack passed untouched - the sword is not in the right hand");
            return Pass;
        }
        if (d.type != 0) {
            ++n.native;
            event("attack while airborne met type %u (%s) - the game already has the kill, passed untouched",
                  (unsigned)d.type, type_name(d.type));
            return Pass;
        }
        g.phase = Holding; g.holdMs = now; g.heldSwing = swingPulse; ++n.held;
        event("HELD a %s: airborne and the game has no drop target yet (vz=%.0f) - waiting up to %.0f ms for one",
              swingPulse ? "swing" : "trigger attack", d.vz, cfg.holdMs);
        return Hold;
    }
    case Holding: {
        const double waited = now - g.holdMs;
        if (fresh && d.known && (d.type == 1 || d.type == 2) && d.targetLive) {
            g.phase = Pressing; g.pressMs = now; g.polls0 = padPolls; ++n.intoDrop;
            event("RELEASED into the drop kill after %.0f ms: type %u (%s), vz=%.0f", waited, (unsigned)d.type,
                  type_name(d.type), d.vz);
            return Press;
        }
        const bool landed = fresh && d.known && !d.airborne;
        if (!landed && fresh && waited < cfg.holdMs) return Hold;
        const char* why = landed ? "landed" : !fresh ? "the decision stopped updating" : "hold time ran out";
        if (cfg.fallback) {
            g.phase = Pressing; g.pressMs = now; g.polls0 = padPolls; ++n.fallback;
            event("no drop target (%s after %.0f ms) - delivering the held attack as an ordinary one", why, waited);
            return Press;
        }
        g.phase = attack ? Swallow : Idle; ++n.discarded;
        event("no drop target (%s after %.0f ms) - held attack dropped (Fallback=0)", why, waited);
        return attack ? Hold : Pass;
    }
    case Pressing:
        if (now - g.pressMs < kPressMs || (padPolls - g.polls0 < 2 && now - g.pressMs < 500.0)) return Press;
        g.phase = attack ? WaitLow : Idle;
        return Pass;
    case WaitLow:   // our press ended with the player's own input still down: one continuous press
        if (!attack) g.phase = Idle;
        return Pass;
    case Swallow:   // a dropped attack stays unsent until the input is let go
        if (!attack) { g.phase = Idle; return Pass; }
        return Hold;
    }
    return Pass;
}

void configure(const char* ini) {
    cfg.assist   = GetPrivateProfileIntA("DropTakedown", "Assist", 1, ini) != 0;
    cfg.fallback = GetPrivateProfileIntA("DropTakedown", "Fallback", 1, ini) != 0;
    cfg.holdMs   = clampf(IniFloat(ini, "DropTakedown", "HoldMs", 420.0f), 100.0f, 3000.0f);
    const float reach = clampf(IniFloat(ini, "DropTakedown", "ReachScale", 2.0f), 0.5f, 3.0f);
    if (reach != cfg.reach) { cfg.reach = reach; InterlockedExchange(&reachDirty, 1); }
    dropWatch = GetPrivateProfileIntA("Anim", "DropWatch", 1, ini) != 0;
    Log("config: [DropTakedown] Assist=%d HoldMs=%.0f Fallback=%d ReachScale=%.2f (1.00 = the shipped game); "
        "[Anim] DropWatch=%d (read-only native drop decision trace)", cfg.assist, cfg.holdMs, cfg.fallback,
        cfg.reach, dropWatch);
}
void save(const char* ini) {
    char v[32];
    WritePrivateProfileStringA("DropTakedown", "Assist", cfg.assist ? "1" : "0", ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", cfg.holdMs); WritePrivateProfileStringA("DropTakedown", "HoldMs", v, ini);
    WritePrivateProfileStringA("DropTakedown", "Fallback", cfg.fallback ? "1" : "0", ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", cfg.reach); WritePrivateProfileStringA("DropTakedown", "ReachScale", v, ini);
}
bool command(const char* args) {
    char sub[32] = {}, a[32] = {};
    if (args) sscanf_s(args, "%31s %31s", sub, (unsigned)sizeof(sub), a, (unsigned)sizeof(a));
    const float f = (float)atof(a);
    if (!*sub || !strcmp(sub, "status")) {
        const Pub d = published();
        Log("drop: assist=%d hold=%.0f ms fallback=%d reach x%.2f | phase=%s | held %u -> into drop %u, fallback %u, "
            "dropped %u; passed with a target %u; refused (no sword) %u | now: known=%d airborne=%d type=%u (%s) "
            "target=%d vz=%.0f | last: %s", cfg.assist, cfg.holdMs, cfg.fallback, cfg.reach, phase_name(), n.held,
            n.intoDrop, n.fallback, n.discarded, n.native, n.refused, d.known, d.airborne, (unsigned)d.type,
            type_name(d.type), d.targetLive, d.vz, lastEvent);
    }
    else if (!strcmp(sub, "on") || !strcmp(sub, "off")) { cfg.assist = !strcmp(sub, "on"); Log("drop: assist %s (live)", cfg.assist ? "ON" : "off"); }
    else if (!strcmp(sub, "hold") && *a) { cfg.holdMs = clampf(f, 100.0f, 3000.0f); Log("drop: HoldMs=%.0f (live)", cfg.holdMs); }
    else if (!strcmp(sub, "fallback") && *a) { cfg.fallback = f != 0.0f; Log("drop: Fallback=%d (live)", cfg.fallback); }
    else if (!strcmp(sub, "reach") && *a) {
        cfg.reach = clampf(f, 0.5f, 3.0f); InterlockedExchange(&reachDirty, 1);
        Log("drop: ReachScale=%.2f (live; applied on the next script tick)", cfg.reach);
    }
    else Log("drop: usage - drop status | on | off | hold <ms> | fallback 0|1 | reach <x, 1 = shipped>");
    return true;
}
void status(dvr::status::Writer& w) {
    const Pub d = published();
    w.obj("drop");
    w.kv("assist", cfg.assist); w.kv("holdMs", (double)cfg.holdMs); w.kv("fallback", cfg.fallback);
    w.kv("reach", (double)cfg.reach); w.kv("phase", phase_name());
    w.kv("known", d.known); w.kv("airborne", d.airborne); w.kv("type", (int)d.type); w.kv("targetLive", d.targetLive);
    w.kv("held", (unsigned long)n.held); w.kv("intoDrop", (unsigned long)n.intoDrop);
    w.kv("fallbacks", (unsigned long)n.fallback); w.kv("dropped", (unsigned long)n.discarded);
    w.kv("passedWithTarget", (unsigned long)n.native); w.kv("last", lastEvent);
    w.end_obj();
}
void draw_ui() {
    namespace ov = dvr::ovl;
    if (!ov::section("Drop takedowns", ov::Basic, "Killing a guard by dropping onto them from above.")) return;
    char v[32];
    if (ImGui::Checkbox("Help time the drop takedown", &cfg.assist))
        ConfigWriteKey("DropTakedown", "Assist", cfg.assist ? "1" : "0", "F10 Controls");
    ov::tip("Attacking in the air before the game has found the guard below waits a moment for it, "
            "so the attack becomes the takedown instead of a slash.");
    if (!cfg.assist) return;
    if (ov::show(ov::Advanced)) {
        ImGui::SliderFloat("Wait for a target (ms)", &cfg.holdMs, 100.0f, 2000.0f, "%.0f");
        ov::tip("How long an early attack waits for the game to find a guard below.");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", cfg.holdMs); ConfigWriteKey("DropTakedown", "HoldMs", v, "F10 Controls"); }
        if (ImGui::Checkbox("Attack anyway if no guard is found", &cfg.fallback))
            ConfigWriteKey("DropTakedown", "Fallback", cfg.fallback ? "1" : "0", "F10 Controls");
        ov::tip("On: the attack still happens when you land. Off: it is dropped.");
        ImGui::SliderFloat("Takedown reach", &cfg.reach, 0.5f, 2.5f, "x%.2f");
        ov::tip("Looks further ahead along your fall for a guard. 1.00 is the original game.");
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            InterlockedExchange(&reachDirty, 1);
            _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", cfg.reach); ConfigWriteKey("DropTakedown", "ReachScale", v, "F10 Controls");
        }
    }
    if (ov::show(ov::Debug)) {
        const Pub d = published();
        ImGui::TextDisabled("now: %s, game says: %s", d.airborne ? "airborne" : "on the ground", d.known ? type_name(d.type) : "unknown");
        ImGui::TextDisabled("held %u -> takedown %u, ordinary %u, dropped %u", n.held, n.intoDrop, n.fallback, n.discarded);
        ImGui::TextDisabled("last: %s", lastEvent);
    }
}
} // namespace dvr::drop
