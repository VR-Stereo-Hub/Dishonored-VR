// game/dishonored/ue3/reflect.cpp - the UE3 property resolver (VR-61).
//
// Resolves a property by NAME to its byte offset on this build, by walking the
// UClass property chain. Read-only: nothing here hooks, patches or writes engine
// memory. The rationale, the rules and what this unblocks are in
// docs/dishonored/GAMEPLAY_STATE.md; the state block
// (mod/state/60_game_dishonored_ue3_reflect.inc) carries the derivation
// argument, the oracle and the traps.
//
// LANE: the script lane. Every read is guarded by RangeReadable and every
// refusal logs the values that produced it.


// A UObject that also passes the UClass fixpoint test where relevant. The
// trilogy mod hardened its equivalent this way after raw structs walked as
// plausible objects; LooksLikeObj here already demands a readable class with a
// printable name, which is the same idea one step weaker.
static bool RflReadPtr(uint8_t* base, int off, uint8_t** out)
{
    if (off < 0 || !base || !RangeReadable(base + off, sizeof(void*))) return false;
    uint8_t* p = *(uint8_t**)(base + off);
    if (!p || ((uintptr_t)p & 3)) return false;
    *out = p;
    return true;
}


// The FName text of a UField, via the offset patterns.h already establishes.
static const char* RflFieldName(uint8_t* field)
{
    if (!field || !RangeReadable(field + kNameOff, 4)) return NULL;
    return RealName(*(uint32_t*)(field + kNameOff));
}


// Walk one class's own declared fields under a CANDIDATE layout, looking for a
// named property and reporting its recorded offset. Does not follow the super
// chain - RflWalk does that, because a candidate has to be judged on the whole
// chain and a leaf class often declares almost nothing.
static bool RflFieldsOf(uint8_t* cls, const RflLayout* L, const char* want,
                        uint32_t* outOff, int* fieldsSeen)
{
    uint8_t* f = NULL;
    if (!RflReadPtr(cls, L->children, &f)) return false;
    int n = 0;
    while (f && n < kRflMaxFields) {
        ++n;
        const char* nm = RflFieldName(f);
        if (nm && want && !strcmp(nm, want)) {
            if (L->propOffset < 0 ||
                !RangeReadable(f + L->propOffset, 4)) return false;
            const uint32_t off = *(uint32_t*)(f + L->propOffset);
            // A property offset inside a UE3 object is small. A huge value means
            // the candidate column is not Offset at all, which is the whole
            // reason the oracle exists.
            if (off > 0x8000u) return false;
            if (outOff) *outOff = off;
            if (fieldsSeen) *fieldsSeen += n;
            return true;
        }
        uint8_t* nx = NULL;
        if (!RflReadPtr(f, L->next, &nx)) break;
        if (nx == f) break;                    // a self-link is not a chain
        f = nx;
    }
    if (fieldsSeen) *fieldsSeen += n;
    return false;
}


// The whole chain: this class, then every superclass. Returns the offset of the
// first property with this name, which is correct because a subclass shadowing a
// base property is not a thing UnrealScript allows.
static bool RflWalk(uint8_t* cls, const RflLayout* L, const char* want,
                    uint32_t* outOff, int* fieldsSeen)
{
    int depth = 0;
    uint8_t* c = cls;
    while (c && depth++ < kRflMaxSuper) {
        if (RflFieldsOf(c, L, want, outOff, fieldsSeen)) return true;
        uint8_t* sup = NULL;
        if (!RflReadPtr(c, L->super, &sup)) return false;
        if (sup == c) return false;             // a self-link is not a parent
        c = sup;
    }
    return false;
}


// ---- the oracle -------------------------------------------------------------
//
// A candidate layout is accepted only if it resolves BOTH known component
// property names to BOTH known offsets. One match is a coincidence a wide search
// will find; two independent ones at different offsets is the layout.
static bool RflOracleHolds(uint8_t* cls, const RflLayout* L, int* fieldsSeen)
{
    uint32_t l2w = 0xffffffffu, tr = 0xffffffffu;
    int seen = 0;
    if (!RflWalk(cls, L, "LocalToWorld", &l2w, &seen)) return false;
    if (l2w != kRflOracleL2W) return false;
    if (!RflWalk(cls, L, "Translation", &tr, &seen)) return false;
    if (tr != kRflOracleTrans) return false;
    if (seen < 8) return false;   // a chain this short is not a component class
    if (fieldsSeen) *fieldsSeen = seen;
    return true;
}


// Find a component we can prove the layout against: one of the view-model
// components the hands work already resolves, which is a genuine
// SkeletalMeshComponent with the two known offsets in use every frame.
static uint8_t* RflOracleObject(void)
{
    for (int i = 0; i < g_fpCandN; ++i) {
        uint8_t* o = g_fpCand[i].obj;
        if (!LooksLikeObj(o)) continue;
        // The oracle offsets are the ones FpComputePivots reads on exactly these
        // objects, so this is the same population the constants were derived on.
        if (!RangeReadable(o + kRflOracleTrans, 12)) continue;
        return o;
    }
    return NULL;
}


// ---- derivation, ONCE, LAZILY, on the script lane ---------------------------
static bool RflDerive(void)
{
    if (g_rfl.ok) return true;
    g_rflTried = true;
    g_rflCandidates = 0;
    g_rflRejected = 0;

    if (!RangeReadable((void*)kGNamesData, 8) || !*(void**)kGNamesData) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "GNames is not populated yet - nothing can be resolved by name. "
                  "This is expected before the engine's static initializers run "
                  "and is why derivation is never done at init.");
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return false;
    }

    uint8_t* obj = RflOracleObject();
    if (!obj) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "no oracle object yet - the view-model component list is empty "
                  "(%d candidates), so there is nothing whose property offsets we "
                  "already know. Draw a weapon and retry.", g_fpCandN);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return false;
    }
    uint8_t* cls = NULL;
    if (!RflReadPtr(obj, (int)kClassOff, &cls)) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "the oracle object at %p has no readable class pointer at +0x%x",
                  (void*)obj, (unsigned)kClassOff);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        return false;
    }

    const char* cn = ObjClassName(obj);
    _snprintf(g_rflOracleCls, sizeof(g_rflOracleCls), "%s", cn ? cn : "?");
    g_rflOracleCls[sizeof(g_rflOracleCls) - 1] = 0;
    g_rflOracleObj = obj;

    // ---- THE SEARCH IS STAGED, NOT A PRODUCT --------------------------------
    //
    // A four-way product over these windows is ~810k candidate layouts, each
    // doing two full chain walks. On the game thread that is a hang measured in
    // minutes, and a diagnostic that freezes the game is not a diagnostic. The
    // slots are separable, so they are derived in three cheap stages, and the
    // ORACLE still judges the finished layout at the end - staging changes the
    // cost, not the standard of proof.
    //
    // Field walks during the search are capped far shorter than a real walk: a
    // wrong Next produces an endless plausible chain through unrelated memory,
    // and during a search that cost is paid thousands of times.

    // STAGE 1: Children and Next together. A correct pair yields a run of
    // fields whose FName all resolve to printable names; a wrong pair yields
    // garbage names almost immediately. This is 2 unknowns, not 4.
    int bestCh = -1, bestNx = -1, bestRun = 0;
    for (int ch = kRflMinOff; ch <= kRflMaxOff; ch += kRflStep) {
        uint8_t* f0 = NULL;
        if (!RflReadPtr(cls, ch, &f0)) continue;
        if (!RflFieldName(f0)) continue;   // the first child must be named
        for (int nx = kRflMinOff; nx <= kRflMaxOff; nx += kRflStep) {
            if (nx == ch) continue;
            ++g_rflCandidates;
            int run = 0;
            uint8_t* f = f0;
            while (f && run < kRflSearchFields) {
                if (!RflFieldName(f)) break;   // an unnamed link ends the run
                ++run;
                uint8_t* n2 = NULL;
                if (!RflReadPtr(f, nx, &n2) || n2 == f) break;
                f = n2;
            }
            if (run > bestRun) { bestRun = run; bestCh = ch; bestNx = nx; }
        }
    }
    if (bestRun < 4) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "stage 1 found no Children/Next pair - the longest run of "
                  "printable field names was %d, from %d candidate pair(s) on "
                  "'%s'. Without a field chain nothing below can be derived.",
                  bestRun, g_rflCandidates, g_rflOracleCls);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        Log("rfl: DERIVATION FAILED - %s", g_rflWhy);
        return false;
    }

    // STAGE 2: UProperty::Offset. With the chain known, find the field actually
    // named Translation and ask which column reads back the offset we already
    // know. This is the oracle doing the work rather than a heuristic.
    int bestPo = -1;
    {
        uint8_t* target = NULL;
        uint8_t* f = NULL;
        if (RflReadPtr(cls, bestCh, &f)) {
            int n = 0;
            while (f && n++ < kRflSearchFields) {
                const char* nm = RflFieldName(f);
                if (nm && !strcmp(nm, "Translation")) { target = f; break; }
                uint8_t* n2 = NULL;
                if (!RflReadPtr(f, bestNx, &n2) || n2 == f) break;
                f = n2;
            }
        }
        if (target)
            for (int po = kRflMinOff; po <= kRflMaxOff; po += kRflStep) {
                ++g_rflCandidates;
                if (!RangeReadable(target + po, 4)) continue;
                if (*(uint32_t*)(target + po) == kRflOracleTrans) { bestPo = po; break; }
            }
        if (!target) {
            _snprintf(g_rflWhy, sizeof(g_rflWhy),
                      "stage 2 could not find a field named Translation on "
                      "'%s' (Children +0x%02x, Next +0x%02x, %d field(s) in the "
                      "run). Either the chain is wrong or this class does not "
                      "declare it and the super chain is needed first.",
                      g_rflOracleCls, bestCh, bestNx, bestRun);
            g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
            Log("rfl: DERIVATION FAILED - %s", g_rflWhy);
            return false;
        }
        if (bestPo < 0) {
            _snprintf(g_rflWhy, sizeof(g_rflWhy),
                      "stage 2 found the Translation field but no column in it "
                      "reads 0x%02x. If kWaComponentTranslation is not actually "
                      "that property's offset on '%s', the constant is the bug "
                      "and this search cannot succeed.",
                      (unsigned)kRflOracleTrans, g_rflOracleCls);
            g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
            Log("rfl: DERIVATION FAILED - %s", g_rflWhy);
            return false;
        }
    }

    // STAGE 3: SuperStruct. Accepted only if following it lands on a class whose
    // own field chain reads as fields too - a wrong super slot points at
    // something that does not. ObjectArchetype is excluded explicitly: it is
    // class-classed and SHARED between objects, which is what falsified it as a
    // link in the trilogy mod, and a shared pointer cannot be a parent link.
    int bestSu = -1;
    for (int su = kRflMinOff; su <= kRflMaxOff; su += kRflStep) {
        if (su == (int)kClassOff || su == bestCh || su == bestNx) continue;
        ++g_rflCandidates;
        uint8_t* sup = NULL;
        if (!RflReadPtr(cls, su, &sup) || sup == cls) continue;
        if (!RangeReadable(sup + kNameOff, 4)) continue;
        if (!RealName(*(uint32_t*)(sup + kNameOff))) continue;
        uint8_t* sf = NULL;
        if (!RflReadPtr(sup, bestCh, &sf)) continue;
        if (!RflFieldName(sf)) continue;
        // A parent must not be the class itself dressed up, and its chain must
        // differ from the leaf's or we have followed a self-reference.
        uint8_t* lf = NULL;
        if (RflReadPtr(cls, bestCh, &lf) && lf == sf) continue;
        bestSu = su;
        break;
    }

    // THE ORACLE STILL DECIDES. Staging only narrowed the search; the finished
    // layout has to reproduce BOTH known offsets through the full walk,
    // including whichever of them lives on a superclass.
    RflLayout best = { bestNx, bestSu, bestCh, bestPo, false };
    int bestFields = 0;
    if (!RflOracleHolds(cls, &best, &bestFields)) {
        _snprintf(g_rflWhy, sizeof(g_rflWhy),
                  "the staged layout (Next +0x%02x, Super +0x%02x, Children "
                  "+0x%02x, Offset +0x%02x) did not reproduce BOTH known offsets "
                  "on '%s'. Refusing a layout that fails its own oracle rather "
                  "than shipping a plausible one.",
                  bestNx, bestSu, bestCh, bestPo, g_rflOracleCls);
        g_rflWhy[sizeof(g_rflWhy) - 1] = 0;
        Log("rfl: DERIVATION FAILED - %s", g_rflWhy);
        ++g_rflRejected;
        return false;
    }
    best.ok = true;
    g_rfl = best;
    _snprintf(g_rflWhy, sizeof(g_rflWhy), "derived and validated against '%s'",
              g_rflOracleCls);
    g_rflWhy[sizeof(g_rflWhy) - 1] = 0;

    // SAY WHAT WAS DERIVED, NEXT TO WHAT IT WAS CHECKED AGAINST. A layout printed
    // without its oracle is a number nobody can audit later.
    Log("rfl: UE3 property layout DERIVED on this build - UField::Next +0x%02x, "
        "UStruct::SuperStruct +0x%02x, UStruct::Children +0x%02x, "
        "UProperty::Offset +0x%02x. Validated on '%s' @ %p: LocalToWorld "
        "resolved to 0x%02x (known 0x%02x) and Translation to 0x%02x (known "
        "0x%02x), across %d field(s). %d candidate layout(s) walked, %d rejected. "
        "Anchored on the already-derived UObject::Name +0x%02x / Class +0x%02x, "
        "which is why only four slots were unknown.",
        g_rfl.next, g_rfl.super, g_rfl.children, g_rfl.propOffset,
        g_rflOracleCls, (void*)obj,
        (unsigned)kRflOracleL2W, (unsigned)kRflOracleL2W,
        (unsigned)kRflOracleTrans, (unsigned)kRflOracleTrans,
        bestFields, g_rflCandidates, g_rflRejected);
    return true;
}


// ---- resolve by name, cached ------------------------------------------------
//
// One-shot per (class, name). A full walk is hundreds of guarded reads and the
// trilogy mod measured that doing name work per poll stuttered the whole game,
// so this must never be called on a cadence without the cache in front of it -
// and the cache is not optional, it is this function.
static bool RflResolve(uint8_t* obj, const char* name, uint32_t* outOff)
{
    if (!obj || !name || !name[0]) return false;
    if (!RflDerive()) return false;
    uint8_t* cls = NULL;
    if (!RflReadPtr(obj, (int)kClassOff, &cls)) return false;

    for (int i = 0; i < g_rflCacheN; ++i)
        if (g_rflCache[i].cls == cls && !strcmp(g_rflCache[i].name, name)) {
            InterlockedIncrement(&g_rflCacheHit);
            if (!g_rflCache[i].found) return false;
            if (outOff) *outOff = g_rflCache[i].off;
            return true;
        }

    InterlockedIncrement(&g_rflCacheMiss);
    InterlockedIncrement(&g_rflWalks);
    uint32_t off = 0;
    int seen = 0;
    const bool found = RflWalk(cls, &g_rfl, name, &off, &seen);
    if (!found) {
        InterlockedIncrement(&g_rflRefused);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
            "rfl: '%s' is not a property of '%s' or any of its %d superclass "
            "field(s) walked. Not an error by itself - it is how a name that "
            "moved between builds reports itself instead of reading garbage.",
            name, ObjClassName(obj) ? ObjClassName(obj) : "?", seen);
    }
    if (g_rflCacheN < RFL_CACHE_MAX) {
        RflCacheEntry* e = &g_rflCache[g_rflCacheN++];
        e->cls = cls; e->off = off; e->mask = 0; e->found = found;
        _snprintf(e->name, sizeof(e->name), "%s", name);
        e->name[sizeof(e->name) - 1] = 0;
    }
    if (found && outOff) *outOff = off;
    return found;
}


// ---- typed readers ----------------------------------------------------------
//
// Each one refuses rather than returning a plausible zero, because a flag that
// defaults to a confident wrong answer is worse than no flag (GAMEPLAY_STATE
// rule 7). The caller gets false and the mod does what it did before.
static bool RflReadInt(uint8_t* obj, const char* name, int32_t* out)
{
    uint32_t off = 0;
    if (!RflResolve(obj, name, &off)) return false;
    if (!RangeReadable(obj + off, 4)) return false;
    if (out) *out = *(int32_t*)(obj + off);
    return true;
}

static bool RflReadFloat(uint8_t* obj, const char* name, float* out)
{
    uint32_t off = 0;
    if (!RflResolve(obj, name, &off)) return false;
    if (!RangeReadable(obj + off, 4)) return false;
    const float v = *(float*)(obj + off);
    if (v != v) return false;
    if (out) *out = v;
    return true;
}

static bool RflReadObject(uint8_t* obj, const char* name, uint8_t** out)
{
    uint32_t off = 0;
    if (!RflResolve(obj, name, &off)) return false;
    uint8_t* p = NULL;
    if (!RflReadPtr(obj, (int)off, &p)) return false;
    if (!LooksLikeObj(p)) return false;
    if (out) *out = p;
    return true;
}

// The property's FName as text - for an enum-valued byte this is not it (that
// needs the enum's own table), but for a Name property it is the answer.
static bool RflReadName(uint8_t* obj, const char* name, const char** out)
{
    uint32_t off = 0;
    if (!RflResolve(obj, name, &off)) return false;
    if (!RangeReadable(obj + off, 4)) return false;
    const char* nm = RealName(*(uint32_t*)(obj + off));
    if (!nm) return false;
    if (out) *out = nm;
    return true;
}

// THE ONE THE COMPONENT WALK LACKS. A TArray field is {Data, Num, Max}, and it is
// exactly what FpCollect cannot traverse: it reads the first four bytes as a
// UObject pointer, gets a heap buffer, and stops. Every inventory item in this
// game lives behind one of these (ENGINE_NOTES, VR-60).
static bool RflReadArray(uint8_t* obj, const char* name, uint8_t** outData,
                         int32_t* outNum)
{
    uint32_t off = 0;
    if (!RflResolve(obj, name, &off)) return false;
    if (!RangeReadable(obj + off, 12)) return false;
    uint8_t* data = *(uint8_t**)(obj + off);
    const int32_t num = *(int32_t*)(obj + off + 4);
    const int32_t max = *(int32_t*)(obj + off + 8);
    // An empty array is a legitimate answer and must not read as a failure.
    if (num == 0) { if (outData) *outData = NULL; if (outNum) *outNum = 0; return true; }
    if (num < 0 || num > 65536 || max < num) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "rfl: '%s' on '%s' does not read as a TArray - Num %d, Max %d. "
            "Refusing rather than walking a buffer of unknown length.",
            name, ObjClassName(obj) ? ObjClassName(obj) : "?",
            (int)num, (int)max);
        return false;
    }
    if (!data || ((uintptr_t)data & 3)) return false;
    if (outData) *outData = data;
    if (outNum) *outNum = num;
    return true;
}


// ---- the first consumer: which item is in which hand ------------------------
//
// The chain is pawn -> m_pInventory -> m_Slots (a TArray of PawnInventorySlot),
// and each slot carries the item AND its EDisEquipUsage. Every step is resolved
// BY NAME, so a build that moves any of them refuses with a reason instead of
// reading a neighbouring field.
//
// The struct STRIDE is the one number here that is not resolved, because element
// layout is not in the property chain. It is validated instead of trusted: a
// stride is accepted only if the item pointers it produces read as UObjects.
static void RflStateTick(void)
{
    if (!g_rflStateOn) return;
    const double now = MaimNowMs();
    if (now - g_rflStateMs < 250.0) return;   // four times a second, not per frame
    g_rflStateMs = now;

    if (!RflDerive()) return;   // RflDerive already recorded why

    uint8_t* pawn = FpPawn();
    if (!pawn) {
        _snprintf(g_rflStateWhy, sizeof(g_rflStateWhy),
                  "no pawn latched yet");
        g_rflStateWhy[sizeof(g_rflStateWhy) - 1] = 0;
        return;
    }
    uint8_t* inv = NULL;
    if (!RflReadObject(pawn, "m_pInventory", &inv)) {
        _snprintf(g_rflStateWhy, sizeof(g_rflStateWhy),
                  "m_pInventory did not resolve on the pawn class '%s'",
                  ObjClassName(pawn) ? ObjClassName(pawn) : "?");
        g_rflStateWhy[sizeof(g_rflStateWhy) - 1] = 0;
        return;
    }
    uint8_t* data = NULL; int32_t num = 0;
    if (!RflReadArray(inv, "m_Slots", &data, &num)) {
        _snprintf(g_rflStateWhy, sizeof(g_rflStateWhy),
                  "m_Slots did not resolve or did not read as a TArray on '%s'",
                  ObjClassName(inv) ? ObjClassName(inv) : "?");
        g_rflStateWhy[sizeof(g_rflStateWhy) - 1] = 0;
        return;
    }

    char found[3][64];
    for (int i = 0; i < 3; ++i) found[i][0] = 0;
    int usable = 0;
    for (int i = 0; i < num && i < 64; ++i) {
        uint8_t* slot = data + (size_t)i * (size_t)g_rflSlotStride;
        if (!RangeReadable(slot, (size_t)g_rflSlotStride)) break;
        uint8_t* item = *(uint8_t**)(slot + 0);
        if (!item) continue;                 // an empty slot is normal
        if (!LooksLikeObj(item)) continue;   // and so is a stride that is wrong
        ++usable;
        const int usage = (int)*(uint8_t*)(slot + 8);
        if (usage < 0 || usage > 2) continue;
        const char* cn = ObjClassName(item);
        const char* nm = RealName(RangeReadable(item + kNameOff, 4)
                                  ? *(uint32_t*)(item + kNameOff) : 0);
        _snprintf(found[usage], sizeof(found[usage]), "%s (%s)",
                  cn ? cn : "?", nm ? nm : "?");
        found[usage][sizeof(found[usage]) - 1] = 0;
    }

    g_rflSlotsSeen = num;
    InterlockedIncrement(&g_rflStateReads);

    // A STRIDE THAT PRODUCES NOTHING IS A WRONG STRIDE, AND IT MUST SAY SO. This
    // is the difference between "the player has no items" and "we are reading the
    // array with the wrong element size", which look identical without this line.
    if (num > 0 && usable == 0) {
        g_rflStateOk = false;
        _snprintf(g_rflStateWhy, sizeof(g_rflStateWhy),
                  "m_Slots holds %d element(s) but stride %d yielded no readable "
                  "item pointer - the stride is wrong, not the inventory empty",
                  (int)num, g_rflSlotStride);
        g_rflStateWhy[sizeof(g_rflStateWhy) - 1] = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 10000,
            "rfl/state: %s. PawnInventorySlot is {item, requiredType, "
            "requiredUsage}; if this build pads it differently the stride is the "
            "only unresolved number in this path.", g_rflStateWhy);
        return;
    }

    g_rflStateOk = true;
    _snprintf(g_rflStateWhy, sizeof(g_rflStateWhy), "reading %d slot(s)", (int)num);
    g_rflStateWhy[sizeof(g_rflStateWhy) - 1] = 0;
    for (int i = 0; i < 3; ++i) {
        _snprintf(g_rflEquip[i], sizeof(g_rflEquip[i]), "%s",
                  found[i][0] ? found[i] : "none");
        g_rflEquip[i][sizeof(g_rflEquip[i]) - 1] = 0;
    }

    // LOG THE CHANGE, NEVER THE STATE.
    if (strcmp(g_rflEquip[1], g_rflEquipPrev[1]) ||
        strcmp(g_rflEquip[2], g_rflEquipPrev[2])) {
        Log("rfl/state: equipment CHANGED - Primary %s -> %s | Secondary %s -> "
            "%s (%d slot(s), stride %d). Read by name through the property "
            "resolver, out of the TArray the component walk cannot traverse.",
            g_rflEquipPrev[1][0] ? g_rflEquipPrev[1] : "?", g_rflEquip[1],
            g_rflEquipPrev[2][0] ? g_rflEquipPrev[2] : "?", g_rflEquip[2],
            (int)num, g_rflSlotStride);
        for (int i = 1; i < 3; ++i)
            { strcpy_s(g_rflEquipPrev[i], g_rflEquip[i]); }
    }
}


// Auto-derivation. NOT at init - GNames is empty while our DllMain runs, and the
// oracle needs a view-model component to exist. So this retries on a slow
// cadence from the script lane and goes quiet once it succeeds, which also means
// a run that never derives says so repeatedly instead of once at a moment nobody
// was reading.
static void RflTick(void)
{
    if (!g_rfl.ok) {
        static double lastTry = 0.0;
        const double now = MaimNowMs();
        if (now - lastTry >= 2000.0) {
            lastTry = now;
            if (!RflDerive())
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 10000,
                    "rfl: not derived yet - %s", g_rflWhy);
        }
    }
    RflStateTick();
}

// ---- the command seam -------------------------------------------------------
//
// `rfl status`            what was derived, and against what
// `rfl derive`           force a derivation attempt now
// `rfl props <hex obj>`  dump one object's whole chain
// `rfl get <hex obj> <PropertyName>`
static bool RflCommand(const char* args)
{
    if (!args) args = "";
    if (!strcmp(args, "status") || !args[0]) {
        Log("rfl: %s. UField::Next +0x%02x, UStruct::SuperStruct +0x%02x, "
            "UStruct::Children +0x%02x, UProperty::Offset +0x%02x | oracle '%s' "
            "@ %p wanting LocalToWorld=0x%02x Translation=0x%02x | %d candidate(s) "
            "walked, %d rejected | cache %d/%d, hits %ld misses %ld, walks %ld, "
            "names refused %ld.%s",
            g_rfl.ok ? "DERIVED" : (g_rflTried ? "NOT DERIVED" : "not attempted"),
            g_rfl.next, g_rfl.super, g_rfl.children, g_rfl.propOffset,
            g_rflOracleCls[0] ? g_rflOracleCls : "none", g_rflOracleObj,
            (unsigned)kRflOracleL2W, (unsigned)kRflOracleTrans,
            g_rflCandidates, g_rflRejected, g_rflCacheN, RFL_CACHE_MAX,
            g_rflCacheHit, g_rflCacheMiss, g_rflWalks, g_rflRefused,
            g_rfl.ok ? "" : " REASON: ");
        if (!g_rfl.ok) Log("rfl: %s", g_rflWhy);
        return true;
    }
    if (!strcmp(args, "derive")) {
        const bool ok = RflDerive();
        Log("rfl: derive %s%s%s", ok ? "OK" : "REFUSED",
            ok ? "" : " - ", ok ? "" : g_rflWhy);
        return true;
    }
    if (!_strnicmp(args, "props ", 6)) {
        uint8_t* obj = (uint8_t*)(uintptr_t)strtoul(args + 6, NULL, 16);
        if (!LooksLikeObj(obj)) { Log("rfl: %p does not read as a UObject", obj); return true; }
        if (!RflDerive()) { Log("rfl: %s", g_rflWhy); return true; }
        uint8_t* cls = NULL;
        if (!RflReadPtr(obj, (int)kClassOff, &cls)) { Log("rfl: no class"); return true; }
        Log("rfl: '%s' @ %p - walking the chain", ObjClassName(obj), (void*)obj);
        int depth = 0, total = 0;
        for (uint8_t* c = cls; c && depth < kRflMaxSuper; ++depth) {
            const char* cnm = RealName(RangeReadable(c + kNameOff, 4)
                                       ? *(uint32_t*)(c + kNameOff) : 0);
            Log("rfl:   [class %d] %s", depth, cnm ? cnm : "?");
            uint8_t* f = NULL;
            if (RflReadPtr(c, g_rfl.children, &f)) {
                int n = 0;
                while (f && n++ < kRflMaxFields) {
                    const char* nm = RflFieldName(f);
                    if (nm && RangeReadable(f + g_rfl.propOffset, 4)) {
                        Log("rfl:     +0x%04x %s",
                            *(uint32_t*)(f + g_rfl.propOffset), nm);
                        ++total;
                    }
                    uint8_t* nx = NULL;
                    if (!RflReadPtr(f, g_rfl.next, &nx) || nx == f) break;
                    f = nx;
                }
            }
            uint8_t* sup = NULL;
            if (!RflReadPtr(c, g_rfl.super, &sup) || sup == c) break;
            c = sup;
        }
        Log("rfl: %d field(s) across %d class(es)", total, depth);
        return true;
    }
    if (!_strnicmp(args, "get ", 4)) {
        char buf[128];
        _snprintf(buf, sizeof(buf), "%s", args + 4);
        buf[sizeof(buf) - 1] = 0;
        char* sp = strchr(buf, ' ');
        if (!sp) { Log("rfl: get <hex obj> <PropertyName>"); return true; }
        *sp = 0;
        uint8_t* obj = (uint8_t*)(uintptr_t)strtoul(buf, NULL, 16);
        const char* nm = sp + 1;
        if (!LooksLikeObj(obj)) { Log("rfl: %p does not read as a UObject", obj); return true; }
        uint32_t off = 0;
        if (!RflResolve(obj, nm, &off)) {
            Log("rfl: '%s' did not resolve on '%s'", nm,
                ObjClassName(obj) ? ObjClassName(obj) : "?");
            return true;
        }
        int32_t iv = 0; float fv = 0.0f;
        RflReadInt(obj, nm, &iv); RflReadFloat(obj, nm, &fv);
        Log("rfl: '%s' on '%s' is at +0x%04x - as int %d, as float %.4f, raw "
            "0x%08x. The TYPE is not derived here, so read the one that makes "
            "sense for the property.",
            nm, ObjClassName(obj) ? ObjClassName(obj) : "?", off,
            (int)iv, (double)fv, (unsigned)iv);
        return true;
    }
    if (!strcmp(args, "state")) {
        Log("rfl/state: %s%s | Primary %s | Secondary %s | %d slot(s), stride %d, "
            "%ld read(s)",
            g_rflStateOk ? "OK - " : "REFUSED - ", g_rflStateWhy,
            g_rflEquip[1][0] ? g_rflEquip[1] : "none",
            g_rflEquip[2][0] ? g_rflEquip[2] : "none",
            g_rflSlotsSeen, g_rflSlotStride, g_rflStateReads);
        return true;
    }
    Log("rfl: status | state | derive | props <hex obj> | get <hex obj> <PropertyName>");
    return true;
}
