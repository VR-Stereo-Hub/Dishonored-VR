// Included by the Dishonored unity TU. READ-ONLY: this file never writes a
// single byte of engine memory.
//
// VR-85. FindPropOffset answers "where is the property I can NAME". This is the
// question the other way round: "which property just changed", when the name is
// what we are missing.
//
// Interaction is entirely native - no exec anywhere in the 2554 native
// registrations, so ProcessEvent never sees it - and property names live in the
// packages rather than the image, so the field holding the currently focused
// interactable cannot be found offline. It CAN be found by watching: look at a
// thing, look away, and see which object-typed property on the controller, the
// pawn or the HUD followed.
//
// The instrument has to be able to come back empty and say so, otherwise a
// silent run reads the same as "there is no such field". It prints its
// POPULATION (how many properties on how many owners it is actually watching)
// on arming and in every heartbeat, so a zero is visibly a zero rather than a
// missing line. See TRAPS on counters without populations.

#include <atomic>

static std::atomic<bool> g_pwEnabled{false};

// The owners worth asking. The focused interactable has to be reachable from
// something that lives as long as the player does, and these are the objects
// this project already holds live pointers to or can resolve by class.
static const char* const kPwOwners[] = {
    "DishonoredPlayerController",
    "DishonoredPlayerPawn",
    "DishonoredHUD",
};

struct PwSlot {
    const char* owner;      // the class the property was declared on
    const char* prop;       // its name, from GNames
    uint32_t    offset;     // UProperty::Offset
    uint8_t*    obj;        // the live object it was last read from
    uint32_t    last;       // the last value seen
    bool        seeded;     // has `last` been filled yet
};
static PwSlot g_pwSlot[192];
static int    g_pwSlots = 0;
static int    g_pwScanned = 0;      // UProperty objects examined
static int    g_pwOwnersFound = 0;  // how many of kPwOwners resolved at all
static volatile LONG g_pwChanges = 0;
static double g_pwNextBeatMs = 0;

// Is this UProperty one that can hold a pointer to another object? Anything
// else cannot be the focused interactable, and watching ints would bury the
// answer in noise.
static bool PwIsObjectProp(uint8_t* p) {
    const char* c = ObjClassName(p);
    if (!c) return false;
    return strstr(c, "ObjectProperty") || strstr(c, "InterfaceProperty") ||
           strstr(c, "ComponentProperty") || strstr(c, "ClassProperty");
}

// One pass over GObjects collecting every object-typed property declared on any
// of kPwOwners. Runs once per arming, on the script lane.
static void PwBuild() {
    g_pwSlots = g_pwScanned = g_pwOwnersFound = 0;
    if (!RangeReadable((void*)kGObjHdr, 12)) {
        Log("propwatch: GObjects header unreadable - nothing to watch"); return;
    }
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) {
        Log("propwatch: GObjects count %u is not plausible - nothing to watch", onum);
        return;
    }
    uint32_t want[sizeof(kPwOwners)/sizeof(kPwOwners[0])];
    for (size_t i = 0; i < sizeof(kPwOwners)/sizeof(kPwOwners[0]); ++i) {
        want[i] = FindNameIdx(kPwOwners[i]);
        if (want[i] != 0xffffffffu) g_pwOwnersFound++;
    }
    for (uint32_t i = 0; i < onum && g_pwSlots < (int)(sizeof(g_pwSlot)/sizeof(g_pwSlot[0])); i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (!PwIsObjectProp(o)) continue;
        g_pwScanned++;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        const uint32_t oname = *(uint32_t*)(ou + kNameOff);
        for (size_t w = 0; w < sizeof(kPwOwners)/sizeof(kPwOwners[0]); ++w) {
            if (want[w] == 0xffffffffu || oname != want[w]) continue;
            PwSlot& s = g_pwSlot[g_pwSlots++];
            s.owner  = kPwOwners[w];
            s.prop   = RealName(*(uint32_t*)(o + kNameOff));
            s.offset = *(uint32_t*)(o + kUPropOffset);
            s.obj = NULL; s.last = 0; s.seeded = false;
            break;
        }
    }
    Log("propwatch: watching %d object-typed propert%s across %d of %d owner classes "
        "(%d object properties examined). Look at a thing and look away; the line "
        "that follows names the property that tracked it. If NOTHING prints while "
        "you do that, the focused interactable is not an object property on these "
        "classes - which is a result, not a missing line.",
        g_pwSlots, g_pwSlots == 1 ? "y" : "ies", g_pwOwnersFound,
        (int)(sizeof(kPwOwners)/sizeof(kPwOwners[0])), g_pwScanned);
    for (int i = 0; i < g_pwSlots; ++i)
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug,
                "propwatch:   [%d] %s::%s at +0x%X", i, g_pwSlot[i].owner,
                g_pwSlot[i].prop ? g_pwSlot[i].prop : "?", g_pwSlot[i].offset);
}

static bool PwEnabled() { return g_pwEnabled.load(); }
static void PwSet(bool on, const char* source) {
    const bool was = g_pwEnabled.exchange(on);
    Log("propwatch: %s (%s). Read-only: it never writes engine memory.",
        on ? "ON" : "off", source);
    if (on && !was) { g_pwSlots = 0; g_pwNextBeatMs = 0; }
}

// The live object for an owner class, from the latches ProcessEvent keeps.
// Returns NULL when that owner is not currently resolvable, which is normal in
// a menu and is why the heartbeat reports how many owners were live.
static uint8_t* PwOwnerObj(const char* owner) {
    if (!strcmp(owner, "DishonoredPlayerController")) return g_peCtrl;
    if (!strcmp(owner, "DishonoredPlayerPawn"))       return g_pePawn;
    return NULL;   // the HUD has no latch yet; its slots simply never sample
}

// SCRIPT LANE. Objects are coherent here, which is the same reason PrTick and
// AimSeamTick run from this point.
static void PropWatchTick() {
    if (!PwEnabled()) return;
    if (!g_pwSlots) PwBuild();
    if (!g_pwSlots) { g_pwEnabled.store(false); return; }   // said why in PwBuild

    int live = 0;
    for (int i = 0; i < g_pwSlots; ++i) {
        PwSlot& s = g_pwSlot[i];
        uint8_t* o = PwOwnerObj(s.owner);
        if (!o || !LooksLikeObj(o) || !RangeReadable(o + s.offset, 4)) continue;
        live++;
        const uint32_t now = *(uint32_t*)(o + s.offset);
        if (o != s.obj) { s.obj = o; s.last = now; s.seeded = true; continue; }
        if (!s.seeded) { s.last = now; s.seeded = true; continue; }
        if (now == s.last) continue;
        const uint32_t was = s.last;
        s.last = now;
        InterlockedIncrement(&g_pwChanges);
        // Name what it points AT, not just that it moved: a pointer that changes
        // every frame is noise, one that becomes a bolt when you look at a bolt
        // is the answer.
        uint8_t* nv = (uint8_t*)now;
        const char* ncls = (nv && LooksLikeObj(nv)) ? ObjClassName(nv) : NULL;
        uint8_t* ov = (uint8_t*)was;
        const char* ocls = (ov && LooksLikeObj(ov)) ? ObjClassName(ov) : NULL;
        Log("propwatch: %s::%s (+0x%X) %s -> %s",
            s.owner, s.prop ? s.prop : "?", s.offset,
            was ? (ocls ? ocls : "<not an object>") : "none",
            now ? (ncls ? ncls : "<not an object>") : "none");
    }

    const double nowMs = MaimNowMs();
    if (nowMs >= g_pwNextBeatMs) {
        g_pwNextBeatMs = nowMs + 5000.0;
        Log("propwatch: beat - %d of %d slots sampled this tick, %ld change(s) so far. "
            "A slot is only sampled while its owner is live, so 0 sampled in a menu is "
            "expected; 0 sampled during gameplay means the owner latches are empty.",
            live, g_pwSlots, g_pwChanges);
    }
}
