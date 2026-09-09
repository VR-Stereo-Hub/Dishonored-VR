// game/dishonored/ue3/reflect.cpp - gameplay state, read by name (VR-61).
//
// A thin layer over the resolver this repo already has. FindPropOffset in
// uobject.cpp does the resolving; this adds a cache and a TArray reader, and uses
// them to answer one question the mod has been guessing at: which item is in
// which hand. Read-only - nothing here hooks, patches or writes engine memory.
//
// The rationale and the rules are docs/dishonored/GAMEPLAY_STATE.md. The state
// block carries why this is a thin layer rather than a second resolver.
//
// LANE: the script lane, on a 250 ms cadence, behind a cache. Never per frame,
// and never a name scan on a cadence.


// GNames sane? The same gate arm_follow uses, and the reason nothing here runs at
// init: our DLL loads from DllMain during the exe's import resolution, before the
// exe's CRT static initializers, so the name pool is empty then. A module that
// resolved at startup would fail every boot and look broken.
static bool RflNamesReady(void)
{
    if (!RangeReadable((void*)kGNamesData, 8)) return false;
    const char* n0 = NameFromIndex(0);
    return n0 && !strcmp(n0, "None");
}


// A property offset by (declaring class, property), memoised for the process
// lifetime. Offsets are stable per boot and the underlying lookup is a full
// GObjects scan, so repeating one is pure waste and doing it on a cadence is the
// documented way to stutter the game.
static uint32_t RflOffsetOf(const char* cls, const char* prop)
{
    if (!cls || !prop) return 0;
    for (int i = 0; i < g_rflPropN; ++i)
        if (!strcmp(g_rflProp[i].cls, cls) && !strcmp(g_rflProp[i].prop, prop)) {
            InterlockedIncrement(&g_rflHit);
            return g_rflProp[i].off;
        }
    if (!RflNamesReady()) return 0;   // not cached: a retry later can succeed

    InterlockedIncrement(&g_rflScan);
    const uint32_t off = FindPropOffset(cls, prop);
    if (!off) {
        InterlockedIncrement(&g_rflMiss);
        Log("rfl: '%s' is not a property declared on '%s'. Not an error by itself "
            "- it is how a name that moved, or a wrong DECLARING class, reports "
            "itself instead of reading a neighbouring field. FindPropOffset "
            "matches on the OUTER's name, so for a struct member that name is the "
            "ScriptStruct and not the class holding it.", prop, cls);
    }
    // Cached either way, the miss included: a miss costs a full scan, and
    // repeating it every tick is exactly the cadence trap this cache prevents.
    if (g_rflPropN < RFL_PROP_MAX) {
        RflProp* e = &g_rflProp[g_rflPropN++];
        _snprintf(e->cls,  sizeof(e->cls),  "%s", cls);
        _snprintf(e->prop, sizeof(e->prop), "%s", prop);
        e->cls[sizeof(e->cls) - 1] = 0;
        e->prop[sizeof(e->prop) - 1] = 0;
        e->off = off;
        e->done = true;
    } else {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
            "rfl: the property cache is full at %d - further lookups re-scan "
            "GObjects every time they are asked, which is the cadence this cache "
            "exists to prevent.", (int)RFL_PROP_MAX);
    }
    if (off) Log("rfl: '%s::%s' resolved to +0x%04x", cls, prop, off);
    return off;
}


// Try several declaring classes for one property and report which answered. A
// property declared on a base class is not visible under a subclass's name
// through this lookup, and guessing a single name is how a resolve silently fails.
static uint32_t RflOffsetOfAny(const char* const* classes, int n,
                               const char* prop, const char** whichOut)
{
    for (int i = 0; i < n; ++i) {
        const uint32_t off = RflOffsetOf(classes[i], prop);
        if (off) { if (whichOut) *whichOut = classes[i]; return off; }
    }
    if (whichOut) *whichOut = NULL;
    return 0;
}


// ---- the TArray reader ------------------------------------------------------
//
// THE CAPABILITY VR-60 NEEDS. A TArray is { void* Data; int32 Num; int32 Max },
// and it is exactly what the component walk cannot follow: that walk reads each
// 4-byte field as a UObject pointer, and here the first four bytes are a heap
// buffer. Every inventory item in this game sits behind one of these.
static bool RflArrayAt(uint8_t* obj, uint32_t off, uint8_t** outData,
                       int32_t* outNum)
{
    if (!obj || !off || !RangeReadable(obj + off, 12)) return false;
    uint8_t* data = *(uint8_t**)(obj + off);
    const int32_t num = *(int32_t*)(obj + off + 4);
    const int32_t max = *(int32_t*)(obj + off + 8);
    // An EMPTY array is a legitimate answer and must not read as a failure.
    if (num == 0) { if (outData) *outData = NULL; if (outNum) *outNum = 0; return true; }
    if (num < 0 || num > 65536 || max < num) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "rfl: +0x%04x on '%s' does not read as a TArray - Num %d, Max %d. "
            "Refusing rather than walking a buffer of unknown length.",
            off, ObjClassName(obj) ? ObjClassName(obj) : "?", (int)num, (int)max);
        return false;
    }
    if (!data || ((uintptr_t)data & 3)) return false;
    if (outData) *outData = data;
    if (outNum) *outNum = num;
    return true;
}


// ---- which item is in which hand --------------------------------------------
//
// pawn -> m_pInventory -> m_Slots, and each slot carries the item AND its
// EDisEquipUsage. Every offset resolved by name; only the element stride is not a
// property, and that is validated rather than trusted.
//
// This exists to make the whole idea FALSIFIABLE. A resolver that resolves and
// reads nothing has proved only that it did not crash. This reads the one thing
// the existing instrument provably cannot, so it either produces the equipment
// per hand or it names the step that failed.
static void RflStateTick(void)
{
    if (!g_rflStateOn) return;
    const double now = MaimNowMs();
    if (now - g_rflStateMs < 250.0) return;   // four times a second, not per frame
    g_rflStateMs = now;

    if (!RflNamesReady()) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "GNames is not populated yet - nothing can be resolved by name. "
                  "Expected before the engine's static initializers run, which is "
                  "why nothing here happens at init.");
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }
    uint8_t* pawn = FpPawn();
    if (!pawn) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy), "no pawn latched yet");
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }

    // m_pInventory is declared on DishonoredPawn and inherited by the player
    // pawn, so both names are offered and the log says which one answered.
    static const char* const kInvClasses[] = { "DishonoredPawn",
                                               "DishonoredPlayerPawn" };
    const char* which = NULL;
    const uint32_t invOff = RflOffsetOfAny(kInvClasses, 2, "m_pInventory", &which);
    if (!invOff) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "m_pInventory did not resolve on any candidate declaring class");
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }
    if (!RangeReadable(pawn + invOff, sizeof(void*))) return;
    uint8_t* inv = *(uint8_t**)(pawn + invOff);
    if (!LooksLikeObj(inv)) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "m_pInventory at +0x%04x on the pawn does not read as a UObject",
                  invOff);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }

    const uint32_t slotsOff = RflOffsetOf("DishonoredInventory", "m_Slots");
    if (!slotsOff) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "m_Slots did not resolve on DishonoredInventory");
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }
    uint8_t* data = NULL; int32_t num = 0;
    if (!RflArrayAt(inv, slotsOff, &data, &num)) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "m_Slots at +0x%04x did not read as a TArray", slotsOff);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return;
    }

    // VALIDATE THE STRIDE, ONCE. Whichever candidate yields readable item
    // pointers is the layout; if none does, that is a wrong stride and the log
    // must not let it look like an empty inventory.
    char found[3][64];
    for (int i = 0; i < 3; ++i) found[i][0] = 0;
    int usable = 0, stride = g_rflStride;
    const int nCand = (int)(sizeof(kRflStrideCandidates) /
                            sizeof(kRflStrideCandidates[0]));

    for (int attempt = 0; attempt < (stride ? 1 : nCand); ++attempt) {
        const int s = stride ? stride : kRflStrideCandidates[attempt];
        int hits = 0;
        for (int i = 0; i < 3; ++i) found[i][0] = 0;
        for (int i = 0; i < num && i < 64; ++i) {
            uint8_t* slot = data + (size_t)i * (size_t)s;
            if (!RangeReadable(slot, (size_t)s)) break;
            uint8_t* item = *(uint8_t**)slot;
            if (!item) continue;                 // an empty slot is normal
            if (!LooksLikeObj(item)) continue;
            ++hits;
            const int usage = (int)*(uint8_t*)(slot + kRflUsageOff);
            if (usage < 0 || usage > 2) continue;
            const char* cn = ObjClassName(item);
            // AN EMPTY PLACEHOLDER MUST NOT MASK A REAL ITEM. The first run
            // reported DishonoredItemEmpty for both hands because this loop
            // overwrote per usage and the placeholders came last. The engine
            // keeps a slot per usage whether or not something occupies it, so
            // the empty one is a legitimate row and simply not the answer.
            if (cn && strstr(cn, "ItemEmpty") && found[usage][0]) continue;
            const char* nm = RealName(RangeReadable(item + kNameOff, 4)
                                      ? *(uint32_t*)(item + kNameOff) : 0);
            if (found[usage][0] && cn && strstr(cn, "ItemEmpty")) continue;
            _snprintf(found[usage], sizeof(found[usage]), "%s (%s)",
                      cn ? cn : "?", nm ? nm : "?");
            found[usage][sizeof(found[usage]) - 1] = 0;
        }
        if (hits > 0) {
            if (!g_rflStride) {
                g_rflStride = s;
                Log("rfl/state: PawnInventorySlot stride validated at %d byte(s) "
                    "- %d of %d slot(s) yielded a readable item. The stride is the "
                    "only number in this path that reflection cannot answer, so it "
                    "is validated against the data instead of assumed.",
                    s, hits, (int)num);
                // DUMP THE WHOLE ARRAY ONCE. The per-hand summary above collapses
                // the array to two strings, and the first run showed why that is
                // not enough on its own: every usage read DishonoredItemEmpty,
                // which could equally mean empty placeholders, a wrong usage byte,
                // or a loadout template rather than live equipment. One dump
                // distinguishes all three and costs nothing after it.
                for (int k = 0; k < num && k < 64; ++k) {
                    uint8_t* sl = data + (size_t)k * (size_t)s;
                    if (!RangeReadable(sl, (size_t)s)) break;
                    uint8_t* it = *(uint8_t**)sl;
                    const char* c2 = (it && LooksLikeObj(it)) ? ObjClassName(it) : NULL;
                    const char* n2 = (it && LooksLikeObj(it) &&
                                      RangeReadable(it + kNameOff, 4))
                                     ? RealName(*(uint32_t*)(it + kNameOff)) : NULL;
                    Log("rfl/state:   slot[%d] usage %d  %s (%s)", k,
                        (int)*(uint8_t*)(sl + kRflUsageOff),
                        c2 ? c2 : (it ? "not a UObject" : "empty"), n2 ? n2 : "-");
                }
            }
            usable = hits; stride = s;
            break;
        }
    }
    ++g_rflState.gen;
    g_rflState.slots = num;
    g_rflState.usable = usable;
    g_rflState.readMs = now;
    InterlockedIncrement(&g_rflReads);

    // A STRIDE THAT PRODUCES NOTHING IS A WRONG STRIDE, AND IT MUST SAY SO. This
    // is the difference between "the player has no items" and "we are reading the
    // array with the wrong element size", which are indistinguishable otherwise.
    if (num > 0 && usable == 0) {
        g_rflState.ok = false;
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "m_Slots holds %d element(s) but no candidate stride yielded a "
                  "readable item pointer - the stride is wrong, not the inventory "
                  "empty", (int)num);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "rfl/state: %s. Tried %d candidate(s); PawnInventorySlot is "
            "{item, requiredType, requiredUsage}.", g_rflWhy, nCand);
        return;
    }

    g_rflState.ok = true;
    _snprintf(g_rflWhy, sizeof(g_rflWhy),
              "reading %d slot(s), %d usable, stride %d, m_pInventory via '%s'",
              (int)num, usable, stride, which ? which : "?");
    g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
    for (int i = 0; i < 3; ++i) {
        _snprintf(g_rflState.equip[i], sizeof(g_rflState.equip[i]), "%s",
                  found[i][0] ? found[i] : "none");
        g_rflState.equip[i][sizeof(g_rflState.equip[i]) - 1] = 0;
    }

    // LOG THE CHANGE, NEVER THE STATE.
    if (strcmp(g_rflState.equip[1], g_rflEquipPrev[1]) ||
        strcmp(g_rflState.equip[2], g_rflEquipPrev[2])) {
        Log("rfl/state: equipment CHANGED - Primary %s -> %s | Secondary %s -> %s "
            "(%d slot(s), %d usable). Read BY NAME out of the TArray the component "
            "walk cannot traverse, which is why the pistol never appears in a "
            "component snapshot.",
            g_rflEquipPrev[1][0] ? g_rflEquipPrev[1] : "?", g_rflState.equip[1],
            g_rflEquipPrev[2][0] ? g_rflEquipPrev[2] : "?", g_rflState.equip[2],
            (int)num, usable);
        for (int i = 1; i < 3; ++i) strcpy_s(g_rflEquipPrev[i], g_rflState.equip[i]);
    }
}


// Script lane. Nothing to derive and nothing to retry: the cache handles a lookup
// that could not answer yet, so this is only the state read.
static void RflTick(void)
{
    RflStateTick();
}


// ---- the command seam -------------------------------------------------------
//
// `rfl status`                  the cache and the state, and why if refused
// `rfl get <Class> <Property>`  resolve one property by name
static bool RflCommand(const char* args)
{
    if (!args) args = "";
    if (!args[0] || !strcmp(args, "status") || !strcmp(args, "state")) {
        Log("rfl: %s - %s | Primary %s | Secondary %s | %d slot(s), %d usable, "
            "stride %d | snapshot %u, %.0f ms old | cache %d/%d, hits %ld, scans "
            "%ld, misses %ld | StateFlags=%d",
            g_rflState.ok ? "OK" : "REFUSED", g_rflWhy,
            g_rflState.equip[1][0] ? g_rflState.equip[1] : "none",
            g_rflState.equip[2][0] ? g_rflState.equip[2] : "none",
            g_rflState.slots, g_rflState.usable, g_rflStride, g_rflState.gen,
            g_rflState.readMs > 0.0 ? MaimNowMs() - g_rflState.readMs : -1.0,
            g_rflPropN, (int)RFL_PROP_MAX, g_rflHit, g_rflScan, g_rflMiss,
            (int)g_rflStateOn);
        for (int i = 0; i < g_rflPropN; ++i)
            Log("rfl:   %s::%s -> %s+0x%04x", g_rflProp[i].cls, g_rflProp[i].prop,
                g_rflProp[i].off ? "" : "NOT FOUND ", g_rflProp[i].off);
        return true;
    }
    if (!_strnicmp(args, "get ", 4)) {
        char buf[160];
        _snprintf(buf, sizeof(buf), "%s", args + 4);
        buf[sizeof(buf) - 1] = 0;
        char* sp = strchr(buf, ' ');
        if (!sp) { Log("rfl: get <Class> <Property>"); return true; }
        *sp = 0;
        const uint32_t off = RflOffsetOf(buf, sp + 1);
        Log("rfl: '%s::%s' -> %s+0x%04x", buf, sp + 1,
            off ? "" : "NOT FOUND ", off);
        return true;
    }
    Log("rfl: status | get <Class> <Property>");
    return true;
}
