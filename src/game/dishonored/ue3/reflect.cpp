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
    // A FULL cache REFUSES instead of scanning. It used to scan and not keep the
    // answer, so every later call to a new name was a full GObjects walk (about
    // 100 ms each, measured 2026-09-22): the VR-165 census took the lookups from
    // under 96 to 104 and the game thread sat at 0 fps on the main menu, dozens of
    // scans per sample. An unresolved offset is visible on the caller's own line
    // and every caller already treats 0 as unresolved; a freeze explains nothing.
    if (g_rflPropN >= RFL_PROP_MAX) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
            "rfl: the property cache is full at %d - '%s::%s' and every further NEW "
            "name is REFUSED (reads as unresolved) rather than re-scanning GObjects on "
            "every call, which froze the game at 0 fps (VR-165, 2026-09-22). Raise "
            "RFL_PROP_MAX.", (int)RFL_PROP_MAX, cls, prop);
        return 0;
    }

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
    {   // room is guaranteed by the refusal above
        RflProp* e = &g_rflProp[g_rflPropN++];
        _snprintf(e->cls,  sizeof(e->cls),  "%s", cls);
        _snprintf(e->prop, sizeof(e->prop), "%s", prop);
        e->cls[sizeof(e->cls) - 1] = 0;
        e->prop[sizeof(e->prop) - 1] = 0;
        e->off = off;
        e->done = true;
    }
    if (off) Log("rfl: '%s::%s' resolved to +0x%04x", cls, prop, off);
    return off;
}


// MANY names in ONE GObjects walk (VR-165, 2026-09-22). Every RflOffsetOf miss
// and every FindBoolProp is a full walk of the object table, about 100 ms each on
// this build; the VR-165 census asks for about fifty names and resolving them one
// at a time froze the game thread for 3.6 s on the main menu (measured). This walks
// once, answers every want (a BoolProperty also gets its bit mask), and enters each
// non-bool answer into RflOffsetOf's cache so later single lookups are hits.
// `found` distinguishes a real offset of 0 (a struct's first member) from a miss.
struct RflWant { const char* cls; const char* prop; bool isBool; uint32_t off; uint32_t mask; bool found; };
static int RflResolveBatch(RflWant* w, int n)
{
    if (!w || n <= 0 || n > 128 || !RflNamesReady()) return 0;
    uint32_t ci[128], pi[128];
    for (int k = 0; k < n; ++k) {
        w[k].off = w[k].mask = 0; w[k].found = false;
        ci[k] = FindNameIdx(w[k].cls); pi[k] = FindNameIdx(w[k].prop);
    }
    if (!RangeReadable((void*)kGObjHdr, 12)) return 0;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return 0;
    InterlockedIncrement(&g_rflScan);
    const double t0 = MaimNowMs();
    int got = 0;
    for (uint32_t i = 0; i < onum && got < n; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        const uint32_t nm = *(uint32_t*)(o + kNameOff);
        for (int k = 0; k < n; ++k) {
            if (w[k].found || pi[k] != nm || ci[k] == 0xffffffffu) continue;
            uint8_t* ou = *(uint8_t**)(o + kOuterOff);
            if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) break;
            if (*(uint32_t*)(ou + kNameOff) != ci[k]) continue;
            const char* pc = ObjClassName(o);
            if (!pc || !strstr(pc, "Property")) break;
            if (w[k].isBool) {
                if (strcmp(pc, "BoolProperty")) continue;
                w[k].mask = *(uint32_t*)(o + kUBoolBitMask);
            }
            w[k].off = *(uint32_t*)(o + kUPropOffset);
            w[k].found = true; ++got;
        }
    }
    int cached = 0;
    for (int k = 0; k < n; ++k) {
        if (w[k].isBool) continue;
        bool have = false;
        for (int i = 0; i < g_rflPropN && !have; ++i)
            have = !strcmp(g_rflProp[i].cls, w[k].cls) && !strcmp(g_rflProp[i].prop, w[k].prop);
        if (have || g_rflPropN >= RFL_PROP_MAX) continue;
        RflProp* e = &g_rflProp[g_rflPropN++];
        _snprintf(e->cls,  sizeof(e->cls),  "%s", w[k].cls);
        _snprintf(e->prop, sizeof(e->prop), "%s", w[k].prop);
        e->cls[sizeof(e->cls) - 1] = 0; e->prop[sizeof(e->prop) - 1] = 0;
        e->off = w[k].off; e->done = true; ++cached;
    }
    Log("rfl: batch - %d of %d names resolved in ONE GObjects walk (%.0f ms, %u objects); %d entered "
        "into the cache (now %d/%d)", got, n, MaimNowMs() - t0, onum, cached, g_rflPropN, (int)RFL_PROP_MAX);
    return got;
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
    uint8_t* heldObj[3] = {};
    int heldSock[3] = {};
    for (int i = 0; i < 3; ++i) found[i][0] = 0;
    int usable = 0, stride = g_rflStride;
    LONG powerSocket = 0;   // DisItemPowers in the equipped socket - recorded only, see below
    const int nCand = (int)(sizeof(kRflStrideCandidates) /
                            sizeof(kRflStrideCandidates[0]));

    for (int attempt = 0; attempt < (stride ? 1 : nCand); ++attempt) {
        const int s = stride ? stride : kRflStrideCandidates[attempt];
        int hits = 0;
        for (int i = 0; i < 3; ++i)
            { found[i][0] = 0; heldObj[i] = NULL; heldSock[i] = 0; }
        powerSocket = 0;
        for (int i = 0; i < num && i < 64; ++i) {
            uint8_t* slot = data + (size_t)i * (size_t)s;
            if (!RangeReadable(slot, (size_t)s)) break;
            uint8_t* item = *(uint8_t**)slot;
            if (!item) continue;                 // an empty slot is normal
            if (!LooksLikeObj(item)) continue;
            ++hits;
            // ASK THE ITEM, NOT THE SLOT. m_RequiredUsage on the slot is a
            // constraint on what may occupy it - the first run showed slot 0
            // requiring Primary and slot 1 requiring Secondary, both holding an
            // Empty placeholder, while the real weapons sat in unconstrained
            // slots. The item carries its own equip usage and socket.
            const uint32_t uOff = RflOffsetOf("DishonoredInventoryItem",
                                              "m_EquipUsage");
            const uint32_t sOff = RflOffsetOf("DishonoredInventoryItem",
                                              "m_CurSocket");
            if (!uOff || !sOff) continue;
            if (!RangeReadable(item + uOff, 1) ||
                !RangeReadable(item + sOff, 1)) continue;
            const int usage  = (int)*(uint8_t*)(item + uOff);
            const int socket = (int)*(uint8_t*)(item + sOff);
            // Powers are ONE inventory item (DisItemPowers) whatever power is selected, and the
            // Primary/Secondary report below never names it. Equipped = drawn in the left hand.
            if (socket == RFL_SOCKET_EQUIPPED) {
                const char* pc = ObjClassName(item);
                if (pc && !strcmp(pc, "DisItemPowers")) powerSocket = 1;
            }
            if (usage < 0 || usage > 2) continue;
            const char* cn = ObjClassName(item);
            const char* nm = RealName(RangeReadable(item + kNameOff, 4)
                                      ? *(uint32_t*)(item + kNameOff) : 0);
            // Only an EQUIPPED item is in the hand. A holstered one is on the
            // body and must not be treated as held - that distinction is the
            // whole reason this reads the item rather than counting components.
            if (socket != RFL_SOCKET_EQUIPPED) continue;
            _snprintf(found[usage], sizeof(found[usage]), "%s (%s)",
                      cn ? cn : "?", nm ? nm : "?");
            found[usage][sizeof(found[usage]) - 1] = 0;
            heldObj[usage] = item;
            heldSock[usage] = socket;
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
                    const uint32_t uo = RflOffsetOf("DishonoredInventoryItem",
                                                    "m_EquipUsage");
                    const uint32_t so = RflOffsetOf("DishonoredInventoryItem",
                                                    "m_CurSocket");
                    int iu = -1, is_ = -1;
                    if (it && LooksLikeObj(it) && uo && RangeReadable(it + uo, 1))
                        iu = (int)*(uint8_t*)(it + uo);
                    if (it && LooksLikeObj(it) && so && RangeReadable(it + so, 1))
                        is_ = (int)*(uint8_t*)(it + so);
                    Log("rfl/state:   slot[%d] requires usage %d | item %s (%s) "
                        "-> its own equip usage %d, socket %d (%s)", k,
                        (int)*(uint8_t*)(sl + kRflUsageOff),
                        c2 ? c2 : (it ? "not a UObject" : "empty"), n2 ? n2 : "-",
                        iu, is_, RflSocketName(is_));
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

    // WHICH SLOT EACH HAND HAS EQUIPPED. The socket test above never fires for powers: a run
    // with Blink out for minutes read DisItemPowers at socket 0 throughout (it is not a mesh
    // that is socketed), so the first powers trim never engaged. The inventory itself says it:
    // DishonoredInventory.m_EquipUsageInfo[EDisEquipUsage] is an array of DisEquipUsageInfo
    // {m_iEquippedSlot, m_iReequipSlot, m_iReequipSlotNonEmpty, m_iDropSlot_NextFrame,
    // m_VelocitySupplement, m_RotationSupplement} - 4 ints and 2 FVectors, 40 bytes - indexed
    // by None/Primary/Secondary. [Secondary].m_iEquippedSlot is the left hand's slot.
    // m_iEquippedSlot is at +0 and 0 reads as "unresolved" to the resolver, so the layout is
    // confirmed by its neighbours (+4 and +28), and the Primary entry is checked against the
    // item the socket read found in the right hand: a layout that cannot name the sword is not
    // trusted to name the power.
    LONG powerHeld = -1;
    {
        static uint32_t euOff = 0; static int layout = 0;   // 0 unknown, 1 confirmed, -1 refused
        static LONG agree = 0, disagree = 0;
        if (!layout) {
            euOff = RflOffsetOf("DishonoredInventory", "m_EquipUsageInfo");
            const uint32_t re = RflOffsetOf("DisEquipUsageInfo", "m_iReequipSlot");
            const uint32_t rs = RflOffsetOf("DisEquipUsageInfo", "m_RotationSupplement");
            layout = (euOff && re == 4 && rs == 28) ? 1 : -1;
            Log("rfl/equip: m_EquipUsageInfo +0x%x, m_iReequipSlot +%u, m_RotationSupplement +%u -> %s",
                euOff, re, rs, layout > 0 ? "layout CONFIRMED (40-byte entries, m_iEquippedSlot at +0)"
                                          : "REFUSED - the powers trim stays off");
        }
        const uint32_t kStride = 40;
        if (layout > 0 && RangeReadable(inv + euOff, kStride * 3)) {
            const int32_t pri = *(int32_t*)(inv + euOff + kStride * 1);
            const int32_t sec = *(int32_t*)(inv + euOff + kStride * 2);
            auto itemAt = [&](int32_t k) -> uint8_t* {
                if (k < 0 || k >= num || !g_rflStride) return NULL;
                uint8_t* sl = data + (size_t)k * (size_t)g_rflStride;
                if (!RangeReadable(sl, sizeof(void*))) return NULL;
                uint8_t* it = *(uint8_t**)sl;
                return (it && LooksLikeObj(it)) ? it : NULL;
            };
            uint8_t* priItem = itemAt(pri);
            uint8_t* secItem = itemAt(sec);
            if (heldObj[1]) { if (priItem == heldObj[1]) ++agree; else ++disagree; }
            const char* sc = secItem ? ObjClassName(secItem) : NULL;
            if (disagree > agree) powerHeld = -1;   // the check failed: do not guess
            else powerHeld = (sc && !strcmp(sc, "DisItemPowers")) ? 1 : 0;
            static int32_t lastSec = -2;
            if (sec != lastSec) {
                lastSec = sec;
                Log("rfl/equip: left hand slot %d = %s | right hand slot %d = %s | primary check %ld agree "
                    "%ld disagree | DisItemPowers socket %ld -> %s",
                    sec, sc ? sc : "none", pri, priItem ? (ObjClassName(priItem) ? ObjClassName(priItem) : "?") : "none",
                    agree, disagree, powerSocket,
                    powerHeld == 1 ? "POWER in the left hand" : powerHeld == 0 ? "no power" : "UNTRUSTED");
            }
        }
    }
    if (InterlockedExchange(&g_rflPowerHeld, powerHeld) != powerHeld)
        Log("rfl/state: the left hand %s (m_EquipUsageInfo[Secondary]) - the powers hand trim follows this",
            powerHeld == 1 ? "holds a POWER" : powerHeld == 0 ? "does not hold a power" : "is UNKNOWN");
    _snprintf(g_rflWhy, sizeof(g_rflWhy),
              "reading %d slot(s), %d usable, stride %d, m_pInventory via '%s'",
              (int)num, usable, stride, which ? which : "?");
    g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
    for (int i = 0; i < 3; ++i) {
        _snprintf(g_rflState.equip[i], sizeof(g_rflState.equip[i]), "%s",
                  found[i][0] ? found[i] : "none");
        g_rflState.equip[i][sizeof(g_rflState.equip[i]) - 1] = 0;
        // The OBJECT, not just its name. VR-60 needs it: the equipped item is
        // the owner of the component the weapon attachment should verify
        // against, instead of another asset's.
        g_rflHeldObj[i] = heldObj[i];
        g_rflHeldSocket[i] = heldSock[i];
    }
    // VR-37: the motion sword asks "is the sword in the right hand" from the
    // present lane. found[1] is the EQUIPPED Primary item, class name first.
    for (int u = 1; u <= 2; ++u) {
        char cls[64]; _snprintf(cls, sizeof(cls), "%s", found[u]); cls[63] = 0;
        if (char* sp = strchr(cls, ' ')) *sp = 0;            // the class, not the object name
        const LONG gun = (strstr(cls, "WepCrossbow") || strstr(cls, "WepPistol")) ? 1 : 2;
        if (InterlockedExchange(&g_rflSlotGun[u], gun) != gun)
            Log("crosshair: equip slot %d holds '%s' -> %s for the other-items reticle offset", u,
                cls[0] ? cls : "nothing", gun == 1 ? "a GUN (its aim is kept)" : "another item (offset applies)");
    }
    InterlockedExchange(&g_rflPrimaryKind,
        !found[1][0] ? 0 : !strncmp(found[1], "DishonoredWepSword", 18) ? 1 : 2);
    { const LONG t = (LONG)GetTickCount(); InterlockedExchange(&g_rflPrimaryKindTick, t ? t : 1); }

    // THE EQUIPMENT REVISION, from validated identity. Computed HERE and nowhere
    // else, because this is the only point in the tick where the read is known
    // to have succeeded - every failure above returned early, leaving the
    // revision untouched, which is the "unknown is not empty" rule.
    {
        uint32_t sig = 2166136261u;
        for (int u = 1; u <= 2; ++u) {
            const uint32_t parts[2] = { (uint32_t)(uintptr_t)g_rflHeldObj[u],
                                        (uint32_t)g_rflHeldSocket[u] };
            for (int k = 0; k < 2; ++k) {
                sig ^= parts[k];
                sig *= 16777619u;
            }
        }
        if (!g_rflEquipSigOk || sig != g_rflEquipSig) {
            const bool first = !g_rflEquipSigOk;
            g_rflEquipSig = sig;
            g_rflEquipSigOk = true;
            ++g_rflEquipRev;
            g_rflEquipRevMs = now;
            Log("rfl/state: equipment REVISION %u - Primary %p (%s), Secondary "
                "%p (%s)%s. Keyed on the item OBJECT and its socket, not on the "
                "class name: two instances of one weapon share a name and the "
                "component that has to be re-collected belongs to the instance.",
                g_rflEquipRev, (void*)g_rflHeldObj[1],
                RflSocketName(g_rflHeldSocket[1]), (void*)g_rflHeldObj[2],
                RflSocketName(g_rflHeldSocket[2]),
                first ? " (the first signature this session)" : "");
        }
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
        Log("rfl/state:   sockets - Primary %s, Secondary %s. Only an EQUIPPED "
            "item is in the hand; a holstered one is on the body.",
            RflSocketName(g_rflHeldSocket[1]), RflSocketName(g_rflHeldSocket[2]));
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
