// game/dishonored/hands/bone_query.cpp - VR-33 step 1b. See state chunk 57.
//
// Asks the engine for bone names, parents and ancestry, through the same
// ProcessEvent path hands/mat_hide.cpp already uses. The one question that
// decides the architecture: is handAttachment_L_jnt a descendant of
// hand_L_jnt, or a sibling?

// UStruct::SuperField, derived. Try each candidate offset and keep the one
// where DishonoredPlayerPawn's chain reaches Pawn, then Actor, then Object -
// three correctly named links. One plausible pointer is not evidence; three
// in the order the class hierarchy actually has is.
static uint32_t BqDeriveSuperOffset(uint8_t* cls)
{
    if (!cls) return 0;
    static const char* const kWant[] = { "Pawn", "Actor", "Object" };
    for (uint32_t off = 0x28; off <= 0x60; off += 4) {
        uint8_t* c = cls;
        int hit = 0;
        for (int step = 0; step < 12 && hit < 3; step++) {
            if (!RangeReadable(c + off, 4)) break;
            uint8_t* sup = *(uint8_t**)(c + off);
            if (!sup || ((uintptr_t)sup & 3) || !RangeReadable(sup, kNameOff + 8)) break;
            const char* nm = RealName(*(uint32_t*)(sup + kNameOff));
            if (!nm) break;
            if (!strcmp(nm, kWant[hit])) hit++;
            c = sup;
        }
        if (hit == 3) {
            Log("bq: UStruct::SuperField derived at +0x%X - "
                "DishonoredPlayerPawn -> Pawn -> Actor -> Object all resolve "
                "by name at that offset. Three named links in the right order, "
                "not one plausible pointer.", off);
            return off;
        }
    }
    Log("bq: REFUSED - could not derive the class Super offset. Without it a "
        "receiver's ancestry cannot be checked, and calling a function on an "
        "object that does not derive from its declaring class is the shape of "
        "a crash. No engine call will be made.");
    return 0;
}


// Does `obj`'s class derive from a class named `cls`?
static bool BqReceiverIsA(uint8_t* obj, const char* cls)
{
    if (!obj || !cls || !g_bqSuperOff) return false;
    if (!RangeReadable(obj + kClassOff, 4)) return false;
    uint8_t* c = *(uint8_t**)(obj + kClassOff);
    for (int step = 0; step < 16; step++) {
        if (!c || ((uintptr_t)c & 3) || !RangeReadable(c, kNameOff + 8)) return false;
        const char* nm = RealName(*(uint32_t*)(c + kNameOff));
        if (nm && !strcmp(nm, cls)) return true;
        if (!RangeReadable(c + g_bqSuperOff, 4)) return false;
        c = *(uint8_t**)(c + g_bqSuperOff);
    }
    return false;
}


// A UFunction by name AND declaring class. Same shape as FindPropOffset's
// owner match, which is the pattern this codebase already trusts.
static uint8_t* BqFindFunc(const char* cls, const char* fname)
{
    const uint32_t fi = FindNameIdx(fname);
    const uint32_t ci = FindNameIdx(cls);
    if (fi == 0xffffffffu || ci == 0xffffffffu) return NULL;
    if (!RangeReadable((void*)kGObjHdr, 12)) return NULL;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return NULL;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (*(uint32_t*)(o + kNameOff) != fi) continue;
        const char* on = ObjClassName(o);
        if (!on || strcmp(on, "Function")) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        if (*(uint32_t*)(ou + kNameOff) != ci) continue;   // the DECLARING class
        return o;
    }
    return NULL;
}


// One call, with our own scoped depth guard. Ours re-enters ProcessEvent and
// therefore re-enters PeHandler, and g_peReentry does not guard the whole
// hook - so the depth is what PeHandler checks to keep mod-originated calls
// out of both its side effects and its evidence.
static void BqCall(uint8_t* obj, uint8_t* fn, void* parms)
{
    g_bqDepth++;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(obj, fn, parms, NULL);
    g_peReentry = false;
    g_bqDepth--;
}


static int BqMatchRefBone(uint8_t* comp, BqName n)
{
    if (!g_bqFnMatchRefBone) return -1;
    struct { BqName BoneName; int32_t ReturnValue; } p;
    p.BoneName = n; p.ReturnValue = -1;
    BqCall(comp, g_bqFnMatchRefBone, &p);
    return p.ReturnValue;
}


static BqName BqGetBoneName(uint8_t* comp, int idx)
{
    BqName r; r.idx = 0xffffffffu; r.num = 0;
    if (!g_bqFnGetBoneName) return r;
    struct { int32_t BoneIndex; BqName ReturnValue; } p;
    p.BoneIndex = idx; p.ReturnValue.idx = 0xffffffffu; p.ReturnValue.num = 0;
    BqCall(comp, g_bqFnGetBoneName, &p);
    return p.ReturnValue;
}


static BqName BqGetParentBone(uint8_t* comp, BqName n)
{
    BqName r; r.idx = 0xffffffffu; r.num = 0;
    if (!g_bqFnGetParentBone) return r;
    // name -> name. NOT int -> int; the first proposal had this wrong and
    // would have passed a two-integer frame to a two-name function.
    struct { BqName BoneName; BqName ReturnValue; } p;
    p.BoneName = n; p.ReturnValue.idx = 0xffffffffu; p.ReturnValue.num = 0;
    BqCall(comp, g_bqFnGetParentBone, &p);
    return p.ReturnValue;
}


static const char* BqStr(BqName n)
{
    const char* s = RealName(n.idx);
    return s ? s : "?";
}


// Walk one bone to the root, by NAME, printing the chain. A visited set and a
// hard bound, because a cycle in a chain we do not control would otherwise
// hang the game inside our own diagnostic.
static void BqWalk(uint8_t* comp, const char* startName, BqName* outChain, int* outN)
{
    *outN = 0;
    const uint32_t si = FindNameIdx(startName);
    if (si == 0xffffffffu) {
        Log("bq/chain: '%s' is not in GNames at all - the name does not exist "
            "on this build, which is a different thing from the bone being "
            "absent from the skeleton", startName);
        return;
    }
    BqName cur; cur.idx = si; cur.num = 0;
    const int idx0 = BqMatchRefBone(comp, cur);
    if (idx0 < 0) {
        Log("bq/chain: '%s' - MatchRefBone returned %d, so this component's "
            "skeleton has no such bone. Reported, not worked around.",
            startName, idx0);
        return;
    }
    // Round-trip before trusting anything: both FName words must come back.
    const BqName back = BqGetBoneName(comp, idx0);
    if (back.idx != cur.idx || back.num != cur.num) {
        Log("bq/chain: '%s' -> index %d, but GetBoneName(%d) returned '%s' "
            "(idx %u num %u vs %u/%u). The round trip FAILED, so the call "
            "contract is wrong and nothing further is trustworthy.",
            startName, idx0, idx0, BqStr(back), back.idx, back.num,
            cur.idx, cur.num);
        return;
    }

    char line[512]; int at = 0;
    at += _snprintf(line + at, sizeof(line) - at, "%s[%d]", startName, idx0);
    outChain[(*outN)++] = cur;
    for (int step = 0; step < BQ_MAX_CHAIN; step++) {
        const BqName par = BqGetParentBone(comp, cur);
        if (par.idx == 0xffffffffu) { at += _snprintf(line + at, sizeof(line) - at, " -> <call failed>"); break; }
        const char* pn = RealName(par.idx);
        if (!pn || !strcmp(pn, "None") || par.idx == cur.idx) {
            at += _snprintf(line + at, sizeof(line) - at, " -> ROOT");
            break;
        }
        int seen = 0;
        for (int k = 0; k < *outN; k++)
            if (outChain[k].idx == par.idx && outChain[k].num == par.num) seen = 1;
        if (seen) {
            at += _snprintf(line + at, sizeof(line) - at, " -> %s (CYCLE)", pn);
            break;
        }
        if (*outN < BQ_MAX_CHAIN) outChain[(*outN)++] = par;
        if (at < (int)sizeof(line) - 64)
            at += _snprintf(line + at, sizeof(line) - at, " -> %s", pn);
        cur = par;
    }
    Log("bq/chain: %s   (%d link(s))", line, *outN);
}


static void BqRun(void)
{
    if (!g_bqOn || g_bqDone) return;
    if (!g_pePawn) return;
    g_bqThread = GetCurrentThreadId();

    const uint32_t meshOff = PrOff("Pawn", "Mesh");
    if (!meshOff || !RangeReadable(g_pePawn + meshOff, 4)) return;
    uint8_t* comp = *(uint8_t**)(g_pePawn + meshOff);
    if (!comp || ((uintptr_t)comp & 3) || !RangeReadable(comp, 0x80)) return;

    g_bqDone = true;
    Log("bq: ==== VR-33 step 1b, the semantic bone queries ==== (thread %u)",
        g_bqThread);

    // The receiver's ancestry, before any call. Calling a SkeletalMeshComponent
    // function on something that is not one is how this crashes.
    if (!RangeReadable(g_pePawn + kClassOff, 4)) return;
    g_bqSuperOff = BqDeriveSuperOffset(*(uint8_t**)(g_pePawn + kClassOff));
    if (!g_bqSuperOff) return;
    const char* rc = ObjClassName(comp);
    if (!BqReceiverIsA(comp, "SkeletalMeshComponent")) {
        Log("bq: REFUSED - the pawn's Mesh is a '%s', which does not derive "
            "from SkeletalMeshComponent by the class chain. No call made.",
            rc ? rc : "?");
        return;
    }
    Log("bq: receiver %p is a '%s' and derives from SkeletalMeshComponent",
        (void*)comp, rc ? rc : "?");

    // Resolve every function on its DECLARING class.
    g_bqFnNumElements   = BqFindFunc("MeshComponent",         "GetNumElements");
    g_bqFnMatchRefBone  = BqFindFunc("SkeletalMeshComponent", "MatchRefBone");
    g_bqFnGetBoneName   = BqFindFunc("SkeletalMeshComponent", "GetBoneName");
    g_bqFnGetParentBone = BqFindFunc("SkeletalMeshComponent", "GetParentBone");
    Log("bq: functions - GetNumElements %s, MatchRefBone %s, GetBoneName %s, "
        "GetParentBone %s (each resolved on its own declaring class, so a "
        "same-named function elsewhere cannot be called on this receiver)",
        g_bqFnNumElements ? "ok" : "MISSING",
        g_bqFnMatchRefBone ? "ok" : "MISSING",
        g_bqFnGetBoneName ? "ok" : "MISSING",
        g_bqFnGetParentBone ? "ok" : "MISSING");

    // SMOKE CALL FIRST: no inputs, integer return, and an existing tested
    // caller in mat_hide.cpp. If this does not come back sane, nothing that
    // passes a value is going to.
    if (!g_bqFnNumElements) {
        Log("bq: REFUSED - GetNumElements did not resolve, so the call path "
            "itself cannot be smoke-tested. Nothing further attempted.");
        return;
    }
    {
        struct { int32_t ReturnValue; } p; p.ReturnValue = -1;
        BqCall(comp, g_bqFnNumElements, &p);
        if (p.ReturnValue < 0 || p.ReturnValue > 256) {
            Log("bq: REFUSED - the smoke call GetNumElements() returned %d, "
                "which is not a credible element count. The call contract is "
                "wrong and no bone query will be attempted.", p.ReturnValue);
            return;
        }
        Log("bq: smoke call OK - GetNumElements() = %d, the frame was written "
            "and the guard depth is %d", p.ReturnValue, g_bqDepth);
    }

    if (!g_bqFnMatchRefBone || !g_bqFnGetBoneName || !g_bqFnGetParentBone) {
        Log("bq: the bone queries did not all resolve - stopping here rather "
            "than reporting a partial chain as an answer");
        return;
    }

    // THE QUESTION. Every target's chain to the root, by name.
    BqName chain[BQ_MAX_TARGETS][BQ_MAX_CHAIN];
    int    chainN[BQ_MAX_TARGETS] = { 0 };
    for (int t = 0; t < BQ_MAX_TARGETS && kBqTargets[t]; t++)
        BqWalk(comp, kBqTargets[t], chain[t], &chainN[t]);

    // The verdict the architecture turns on, stated from the chains rather
    // than asserted: is each attachment joint anywhere in its hand's chain?
    for (int side = 0; side < 2; side++) {
        const char* handName = kBqTargets[side];        // hand_L_jnt / hand_R_jnt
        const int   ai = 2 + side;                      // handAttachment_L/R_jnt
        if (!chainN[ai]) {
            Log("bq/VERDICT: %s - no chain, so its relationship to %s is "
                "UNKNOWN", kBqTargets[ai], handName);
            continue;
        }
        const uint32_t hi = FindNameIdx(handName);
        int found = 0;
        for (int k = 1; k < chainN[ai]; k++)
            if (chain[ai][k].idx == hi) found = 1;
        Log("bq/VERDICT: %s is %s of %s. %s",
            kBqTargets[ai], found ? "a DESCENDANT" : "NOT a descendant", handName,
            found ? "Moving the wrist can carry the weapon through the engine's "
                    "own attachment path - which is the case for the native "
                    "route and against a GPU palette edit."
                  : "A wrist-only edit CANNOT carry the weapon automatically. "
                    "That does not kill the native route - separate targets can "
                    "take a coordinated transform - but it does kill the "
                    "follow-for-free argument as stated.");
    }
    Log("bq: what this still does NOT establish: whether any later writer "
        "overwrites a pose we set, where the attachment update sits relative "
        "to composition, and whether the item's own mesh is parented where the "
        "socket says. Those are step 2.");
}


static void BqTick(void)
{
    if (g_bqRunReq) { g_bqRunReq = 0; g_bqDone = false; }
    BqRun();
}


static bool BqCommand(const char* args)
{
    if (args) {
        while (*args == ' ') args++;
        // The command seam runs on the PRESENT thread. It must only POST a
        // request; consuming it here would call into the engine off-lane,
        // which is the exact fault the pose report was just corrected for.
        if (!strncmp(args, "run", 3)) { g_bqOn = true; g_bqRunReq = 1; }
        else if (!strncmp(args, "off", 3)) g_bqOn = false;
    }
    Log("bq: status - %s, %s. `bq run` posts a request that the SCRIPT lane "
        "consumes; this command never calls the engine itself.",
        g_bqOn ? "enabled" : "disabled",
        g_bqDone ? "already run" : "not yet run");
    return true;
}
