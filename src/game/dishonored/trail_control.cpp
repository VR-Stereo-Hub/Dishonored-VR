// VR-171: the sword's swing trail, identified and (by default) not drawn.
// Included after rain_control.cpp (it reuses RainFindClassFunction); uses the
// name-keyed resolver and the anim snapshot.
//
// WHAT THE PLAYER SEES. A sword attack draws a swoosh ribbon along the path of the
// game's own attack animation. In the headset the blade is in the player's hand,
// so the ribbon hangs in the air somewhere the blade is not.
//
// WHY IT IS HIDDEN AND NOT MOVED. The ribbon is generated from the ANIMATED mesh,
// and this mod corrects the weapon's DRAW, never its component (VR-33). Making the
// ribbon follow the hand would mean moving the component, which the mod does not
// do for anything.
//
// WHAT IT IS, MEASURED 2026-09-21 (nothing in the repo knew, and the script dump is
// not on the dev PC, so both answers came from runtime instruments):
//   - NOT a stock anim-trail notify. TrailsNotify / TrailsNotifyTick /
//     TrailsNotifyEnd exist in the name table, and across 4 sword attacks the
//     ProcessEvent observer saw 0 of them from anyone. That route is eliminated;
//     ENGINE_NOTES records it so nobody re-walks it.
//   - It IS one ParticleSystemComponent that each attack ADDS to the player pawn's
//     AllComponents, template 'Sword_Trail', first seen between 305 and 604 ms into
//     the attack (27 components before, 28 after, nothing else new or changed).
//     Found by `swordtrail census`, which is kept: it is the instrument that names
//     whatever an attack adds, and the next effect like this is one command away.
//
// HOW IT IS HIDDEN. With [SwordTrail] Hide=1 the pawn's component list is read on
// the script lane - every 8 ms while a sword attack is open, every 250 ms
// otherwise - and a particle component whose template name contains
// [SwordTrail] Template gets the engine's own native PrimitiveComponent.SetHidden.
// The native propagates to the render proxy, which a raw HiddenGame write would not
// (the rain box, VR-136, is the same problem solved the same way). The list is ~28
// pointers and a component already judged is skipped by pointer, so the steady cost
// is one array read.
//
// A PARTICLE COMPONENT MAY BE POOLED. The one we hid can come back later carrying
// another template (blood, sparks). Every scan therefore re-checks the components
// WE hid: one that is still live, still hidden and no longer a sword trail is shown
// again, and one that is gone is dropped without a write. IsLiveObject is the only
// valid liveness test, and nothing is written through a pointer it does not pass.
//
// LANE: the script lane throughout (TrailTick, beside the other per-tick readers).
#include <atomic>

static std::atomic<bool> g_trailHide{true}, g_trailTrace{true};
static char g_trailTemplate[64] = "Sword_Trail";

namespace {

struct TrailOffs { bool resolved = false; uint32_t all = 0, hidOff = 0, hidMask = 0, actOff = 0, actMask = 0, tmpl = 0, skel = 0;
                   uint8_t* fnSetHidden = nullptr; } to;
// What the last scan decided about each component, so a judged pointer costs one compare.
struct Judged { uint8_t* p; uint8_t* tmpl; bool trail; };
struct TrailState {
    Judged seen[96]; int nSeen = 0;
    uint8_t* hid[8] = {}; int nHid = 0;                 // the components WE hid
    unsigned attacks = 0, trailsSeen = 0, hidden = 0, shown = 0, released = 0, scans = 0;
    double attackMs = -1e9, lastSeenMs = 0, firstSeenAfterAttackMs = -1;
    bool inAttack = false;
    char lastTemplate[64] = "none", lastRefusal[160] = "";
} st;

const char* obj_name(uint8_t* o) {
    if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kNameOff + 4)) return "unreadable";
    const char* n = RealName(*(uint32_t*)(o + kNameOff));
    return n ? n : "unnamed";
}
uint8_t* obj_ptr(uint8_t* o, uint32_t off) {
    uint8_t* v = nullptr;
    if (!o || !off || !RangeReadable(o + off, sizeof(v))) return nullptr;
    memcpy(&v, o + off, sizeof(v));
    return (v && !((uintptr_t)v & 3) && RangeReadable(v, kClassOff + 4)) ? v : nullptr;
}
bool contains_nocase(const char* hay, const char* needle) {
    if (!hay || !needle || !*needle) return false;
    const size_t n = strlen(needle);
    for (; *hay; ++hay) if (!_strnicmp(hay, needle, n)) return true;
    return false;
}
int hidden_now(uint8_t* c) {
    if (!c || !to.hidOff || !RangeReadable(c + to.hidOff, 4)) return -1;
    return (*(uint32_t*)(c + to.hidOff) & to.hidMask) ? 1 : 0;
}
void set_hidden(uint8_t* c, bool v) {
    struct { uint32_t NewHidden; } parms = { v ? 1u : 0u };
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(c, to.fnSetHidden, &parms, NULL);
    g_peReentry = false;
}
void resolve() {
    if (to.resolved || !RflNamesReady()) return;
    to.resolved = true;
    to.all  = RflOffsetOf("Actor", "AllComponents");
    to.tmpl = RflOffsetOf("ParticleSystemComponent", "Template");
    to.skel = RflOffsetOf("SkeletalMeshComponent", "SkeletalMesh");
    FindBoolProp("PrimitiveComponent", "HiddenGame", &to.hidOff, &to.hidMask);
    FindBoolProp("ParticleSystemComponent", "bIsActive", &to.actOff, &to.actMask);
    to.fnSetHidden = RainFindClassFunction("PrimitiveComponent", "SetHidden");
    Log("trail: layout by name - Actor.AllComponents +0x%x | ParticleSystemComponent.Template +0x%x bIsActive +0x%x/0x%x | "
        "PrimitiveComponent.HiddenGame +0x%x/0x%x SetHidden=%p%s", to.all, to.tmpl, to.actOff, to.actMask, to.hidOff, to.hidMask,
        (void*)to.fnSetHidden, (to.all && to.tmpl && to.hidOff && to.fnSetHidden) ? "" : "  <-- a hide needs every field; it will refuse");
}
void refuse(const char* why) {
    if (!strcmp(st.lastRefusal, why)) return;           // state changes, not state
    _snprintf_s(st.lastRefusal, sizeof(st.lastRefusal), _TRUNCATE, "%s", why);
    Log("trail: hide armed, nothing hidden: %s", why);
}
bool is_trail(uint8_t* c, uint8_t** tmplOut) {
    const char* cn = ObjClassName(c);
    uint8_t* t = cn && strstr(cn, "ParticleSystemComponent") ? obj_ptr(c, to.tmpl) : nullptr;
    if (tmplOut) *tmplOut = t;
    return t && contains_nocase(obj_name(t), g_trailTemplate);
}

// The components WE hid, re-judged: shown again if they became something else or
// the lever went off, dropped unwritten if they are gone.
void review_hidden(bool hide, double now) {
    for (int i = 0; i < st.nHid; ) {
        uint8_t* c = st.hid[i];
        bool drop = false;
        if (!IsLiveObject(c)) {
            ++st.released; drop = true;
            if (g_trailTrace.load()) DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                "trail: released component %p without a write - it is no longer a live object (hidden %u shown %u released %u)",
                (void*)c, st.hidden, st.shown, st.released);
        } else {
            uint8_t* t = nullptr;
            const bool stillTrail = is_trail(c, &t);
            if ((!stillTrail || !hide) && hidden_now(c) == 1 && to.fnSetHidden) {
                set_hidden(c, false);
                ++st.shown; drop = true;
                Log("trail: SHOWED component %p again (hidden now %d): %s, template now '%s'", (void*)c, hidden_now(c),
                    !hide ? "the lever went off" : "it is no longer a sword trail (a pooled particle component reused)",
                    t ? obj_name(t) : "none");
            } else if (hidden_now(c) == 0) drop = true;  // the engine showed it itself: not ours any more
        }
        if (drop) { st.hid[i] = st.hid[--st.nHid]; } else ++i;
    }
    (void)now;
}

void scan(bool hide, double now) {
    uint8_t* pawn = g_pePawn; uint8_t* data = nullptr; int32_t num = 0;
    if (!pawn || !IsLiveObject(pawn)) return;
    if (!RflArrayAt(pawn, to.all, &data, &num) || num <= 0 || num > 4096 || !RangeReadable(data, num * sizeof(void*))) return;
    ++st.scans;
    static Judged next[96]; int nNext = 0;
    for (int i = 0; i < num; ++i) {
        uint8_t* c = ((uint8_t**)data)[i];
        if (!c || ((uintptr_t)c & 3) || !RangeReadable(c, kClassOff + 4)) continue;
        uint8_t* tNow = to.tmpl && RangeReadable(c + to.tmpl, 4) ? *(uint8_t**)(c + to.tmpl) : nullptr;
        const Judged* was = nullptr;
        for (int k = 0; k < st.nSeen; ++k) if (st.seen[k].p == c) { was = &st.seen[k]; break; }
        bool trail;
        uint8_t* t = nullptr;
        // Judged before, and the template slot holds the same bits: same verdict.
        // (For a non-particle component those bits are whatever lives there; they
        // only have to be STABLE for the skip to be right, and a change re-judges.)
        if (was && was->tmpl == tNow) { trail = was->trail; t = tNow; }
        else trail = is_trail(c, &t);
        if (nNext < 96) next[nNext++] = Judged{ c, tNow, trail };
        if (!trail) continue;
        if (!was || !was->trail) {
            ++st.trailsSeen; st.lastSeenMs = now;
            _snprintf_s(st.lastTemplate, sizeof(st.lastTemplate), _TRUNCATE, "%s", obj_name(t));
            if (st.firstSeenAfterAttackMs < 0 && now - st.attackMs < 2000.0) st.firstSeenAfterAttackMs = now - st.attackMs;
            if (g_trailTrace.load() && st.trailsSeen <= 8)
                Log("trail: sword trail #%u seen - component %p on the pawn, template '%s', %.0f ms after the attack began, hidden=%d, hide=%d",
                    st.trailsSeen, (void*)c, obj_name(t), now - st.attackMs, hidden_now(c), (int)hide);
        }
        if (!hide || hidden_now(c) != 0) continue;
        if (!to.fnSetHidden || !to.hidOff) { refuse("SetHidden/HiddenGame unresolved"); continue; }
        if (st.nHid >= 8) { refuse("eight components already held (none released yet)"); continue; }
        const int before = hidden_now(c);
        set_hidden(c, true);
        const int after = hidden_now(c);
        st.hid[st.nHid++] = c; ++st.hidden; st.lastRefusal[0] = 0;
        if (st.hidden <= 6) {
            // The component stays on the pawn between swings, so a hide that arrives
            // with the lever (not with an attack) has no attack to be timed against.
            char when[64];
            if (now - st.attackMs < 2000.0) _snprintf_s(when, sizeof(when), _TRUNCATE, "%.0f ms after the attack began", now - st.attackMs);
            else strcpy_s(when, "found already attached, between attacks");
            Log("trail: HID the sword trail %p (template '%s') via native SetHidden: HiddenGame %d -> %d, %s "
                "(a verified write, not yet an honoured one: the headset says whether the ribbon is gone)",
                (void*)c, obj_name(t), before, after, when);
        }
    }
    memcpy(st.seen, next, sizeof(Judged) * nNext); st.nSeen = nNext;
}

// ---- The component census: what does an attack ADD to the player, or switch on? ----
// The instrument that found the trail. Armed on demand only (`swordtrail census`),
// bounded to one attack: the pawn's AllComponents snapshotted while idle against
// 100, 300 and 600 ms into the next sword attack. Reported: components that
// APPEARED, and ones whose HiddenGame or particle bIsActive bit CHANGED - an effect
// that always exists and is only switched on would never show up as new.
struct CompRow { uint8_t* p; uint8_t hidden, active; };
struct Census { bool armed = false; int stage = 0; double attackMs = 0; CompRow before[384]; int nBefore = 0; unsigned reported = 0; } cs;

int census_read(CompRow* out, int cap) {
    uint8_t* pawn = g_pePawn; uint8_t* data = nullptr; int32_t num = 0;
    if (!pawn || !to.all || !RflArrayAt(pawn, to.all, &data, &num) || num <= 0 || num > 4096 ||
        !RangeReadable(data, num * sizeof(void*))) return -1;
    int n = 0;
    for (int i = 0; i < num && n < cap; ++i) {
        uint8_t* c = ((uint8_t**)data)[i];
        if (!c || ((uintptr_t)c & 3) || !RangeReadable(c, kClassOff + 4)) continue;
        CompRow r{ c, 2, 2 };                       // 2 = not readable on this component
        const char* cn = ObjClassName(c);
        if (hidden_now(c) >= 0) r.hidden = (uint8_t)hidden_now(c);
        if (to.actOff && cn && strstr(cn, "ParticleSystemComponent") && RangeReadable(c + to.actOff, 4))
            r.active = (*(uint32_t*)(c + to.actOff) & to.actMask) ? 1 : 0;
        out[n++] = r;
    }
    return n;
}
void census_describe(const char* what, const CompRow& r, const CompRow* was, double atMs) {
    const char* cn = ObjClassName(r.p);
    uint8_t* tmpl = cn && strstr(cn, "ParticleSystemComponent") ? obj_ptr(r.p, to.tmpl) : nullptr;
    uint8_t* skel = cn && strstr(cn, "SkeletalMeshComponent") ? obj_ptr(r.p, to.skel) : nullptr;
    ++cs.reported;
    Log("trail/census: +%.0f ms %s %p class=%s name='%s' hidden=%d%s active=%d%s | template='%s' mesh='%s'",
        atMs, what, (void*)r.p, cn ? cn : "unreadable", obj_name(r.p), (int)r.hidden,
        was && was->hidden != r.hidden ? " (CHANGED)" : "", (int)r.active, was && was->active != r.active ? " (CHANGED)" : "",
        tmpl ? obj_name(tmpl) : "-", skel ? obj_name(skel) : "-");
}
void census_diff(double atMs) {
    static CompRow now[384];
    const int n = census_read(now, 384);
    if (n < 0) { Log("trail/census: +%.0f ms the pawn's AllComponents is unreadable (offset +0x%x)", atMs, to.all); return; }
    int added = 0, changed = 0;
    for (int i = 0; i < n; ++i) {
        const CompRow* was = nullptr;
        for (int k = 0; k < cs.nBefore; ++k) if (cs.before[k].p == now[i].p) { was = &cs.before[k]; break; }
        if (!was) { ++added; census_describe("NEW", now[i], nullptr, atMs); }
        else if (was->hidden != now[i].hidden || was->active != now[i].active) { ++changed; census_describe("CHANGED", now[i], was, atMs); }
    }
    Log("trail/census: +%.0f ms into the attack: %d component(s) on the pawn (%d before), %d new, %d changed. Zero new and zero "
        "changed at all three marks means the effect is not a component of the player pawn at all", atMs, n, cs.nBefore, added, changed);
}
void census_tick(double now, bool attacking, bool attackStarted) {
    if (!cs.armed) return;
    if (cs.stage == 0) {
        if (attackStarted) { cs.stage = 1; cs.attackMs = now; Log("trail/census: a sword attack began with %d component(s) on the pawn; sampling at 100, 300 and 600 ms", cs.nBefore); }
        else if (!attacking) { const int n = census_read(cs.before, 384); if (n >= 0) cs.nBefore = n; }   // keep the idle picture fresh
        return;
    }
    const double at = now - cs.attackMs;
    const double marks[3] = { 100.0, 300.0, 600.0 };
    if (cs.stage <= 3 && at >= marks[cs.stage - 1]) { census_diff(at); ++cs.stage; }
    if (cs.stage > 3) { cs.armed = false; cs.stage = 0; Log("trail/census: done (%u row(s) reported). Arm it again with 'swordtrail census'", cs.reported); }
}

} // namespace

static void SwordTrailHideSet(bool on, const char* who) {
    g_trailHide.store(on);
    Log("trail: hide=%d by %s (live; a particle component on the player's pawn whose template name contains '%s' gets the "
        "engine's native SetHidden; enemy trails are on other pawns and are not touched)", on ? 1 : 0, who ? who : "?", g_trailTemplate);
}
static bool SwordTrailHideEnabled() { return g_trailHide.load(); }
static void SwordTrailConfigure(const char* ini) {
    g_trailTrace.store(GetPrivateProfileIntA("SwordTrail", "Trace", 1, ini) != 0);
    GetPrivateProfileStringA("SwordTrail", "Template", "Sword_Trail", g_trailTemplate, sizeof(g_trailTemplate), ini);
    if (!g_trailTemplate[0]) strcpy_s(g_trailTemplate, "Sword_Trail");
    SwordTrailHideSet(GetPrivateProfileIntA("SwordTrail", "Hide", 1, ini) != 0, "ini");
}
static void SwordTrailSave(const char* ini) {
    WritePrivateProfileStringA("SwordTrail", "Hide", g_trailHide.load() ? "1" : "0", ini);
    WritePrivateProfileStringA("SwordTrail", "Trace", g_trailTrace.load() ? "1" : "0", ini);
    WritePrivateProfileStringA("SwordTrail", "Template", g_trailTemplate, ini);
}

static void TrailTick() {
    const bool hide = g_trailHide.load();
    if (!hide && !g_trailTrace.load() && !st.nHid && !cs.armed) return;
    static double nextMs = 0.0, nextLive = 0.0;
    const double now = MaimNowMs();
    if (now < nextMs) return;
    const bool open = now - st.attackMs < 1500.0;        // the trail arrives 300-600 ms in and outlives the swing
    nextMs = now + (open || cs.armed ? 8.0 : 250.0);
    if (!RflNamesReady()) return;
    resolve();
    if (!to.all || !to.tmpl) { if (hide) refuse("Actor.AllComponents or ParticleSystemComponent.Template did not resolve by name"); return; }
    if (now >= nextLive) { nextLive = now + 2000.0; RefreshLiveSet(2000); }   // bounded (VR-160), never a full build per tick

    const dvr::anim::Snapshot s = dvr::anim::snapshot();
    const bool attacking = s.valid && !strcmp(s.state[1], "StatePlayerMeleeAttack");
    const bool attackStarted = attacking && !st.inAttack;
    if (attackStarted) { ++st.attacks; st.attackMs = now; }
    st.inAttack = attacking;
    census_tick(now, attacking, attackStarted);

    review_hidden(hide, now);
    scan(hide, now);

    // The instrument must be able to fail: attacks with no trail ever seen.
    if (g_trailTrace.load() && (st.attacks == 3 || st.attacks == 10) && st.trailsSeen == 0 && now - st.attackMs > 1500.0) {
        static unsigned warnedAt = 0;
        if (warnedAt != st.attacks) {
            warnedAt = st.attacks;
            DVR_WARN("trail: %u sword attack(s) and NO particle component on the pawn whose template contains '%s' (%u scans). Either this "
                     "sword's trail has another name or it is not on the pawn: run 'swordtrail census' and swing once, it names what the attack adds",
                     st.attacks, g_trailTemplate, st.scans);
        }
    }
}

static void SwordTrailReport() {
    Log("trail: hide=%d trace=%d template~'%s' | sword attacks %u, trails seen %u (last '%s', first seen %.0f ms into an attack) | hidden %u "
        "shown-again %u released %u, holding %d | scans %u | last refusal: %s | attacks > 0 with 0 trails seen means the match is wrong, not "
        "that the trail is gone; hidden > 0 is a write, the capture or the headset is the proof",
        (int)g_trailHide.load(), (int)g_trailTrace.load(), g_trailTemplate, st.attacks, st.trailsSeen, st.lastTemplate,
        st.firstSeenAfterAttackMs, st.hidden, st.shown, st.released, st.nHid, st.scans, st.lastRefusal[0] ? st.lastRefusal : "none");
}
static bool SwordTrailCommand(const char* args) {
    bool b = false;
    char sub[24] = {}, a[64] = {};
    sscanf(args ? args : "", "%23s %63s", sub, a);
    if (DvrOnOff(sub, &b)) {
        SwordTrailHideSet(b, "the seam");
        ConfigWriteKey("SwordTrail", "Hide", b ? "1" : "0", "the seam");
        return true;
    }
    if (!strcmp(sub, "census")) {
        cs.armed = true; cs.stage = 0; cs.reported = 0;
        Log("trail/census: ARMED for the next sword attack - the pawn's components before it against 100, 300 and 600 ms into it");
        return true;
    }
    if (!strcmp(sub, "template") && *a) {
        strcpy_s(g_trailTemplate, a); st.nSeen = 0;      // re-judge everything against the new name
        ConfigWriteKey("SwordTrail", "Template", g_trailTemplate, "the seam");
        return true;
    }
    if (!strcmp(sub, "trace") && DvrOnOff(a, &b)) { g_trailTrace.store(b); Log("trail: trace=%d", (int)b); return true; }
    if (*sub && strcmp(sub, "status"))
        Log("swordtrail: on|off (hide the sword's swing trail) | template <part of the particle template's name> | trace on|off | "
            "census (what the next sword attack adds to the pawn) | status");
    SwordTrailReport();
    return true;
}
static void SwordTrailStatus(dvr::status::Writer& w) {
    w.obj("swordTrail");
    w.kv("hide", g_trailHide.load()); w.kv("trace", g_trailTrace.load()); w.kv("template", g_trailTemplate);
    w.kv("attacks", (unsigned long)st.attacks); w.kv("trailsSeen", (unsigned long)st.trailsSeen);
    w.kv("hidden", (unsigned long)st.hidden); w.kv("shownAgain", (unsigned long)st.shown); w.kv("released", (unsigned long)st.released);
    w.kv("holding", st.nHid); w.kv("lastTemplate", st.lastTemplate);
    w.kv("firstSeenMsIntoAttack", st.firstSeenAfterAttackMs);
    w.kv("lastRefusal", st.lastRefusal[0] ? st.lastRefusal : "none");
    w.end_obj();
}
