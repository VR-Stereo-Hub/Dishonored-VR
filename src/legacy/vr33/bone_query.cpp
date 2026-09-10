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
            // Print the WHOLE chain, not the three milestones. The decompiled
            // hierarchy is DishonoredPlayerPawn -> DishonoredPawn -> GamePawn
            // -> Pawn -> Actor -> Object, so these are landmarks passed in
            // order with intermediates between them - calling them three
            // direct links, as the first build's line did, overstates it.
            char chain[384]; int at = 0; uint8_t* c = cls;
            for (int step = 0; step < 12; step++) {
                if (!RangeReadable(c + kNameOff, 4)) break;
                const char* nm = RealName(*(uint32_t*)(c + kNameOff));
                if (at < (int)sizeof(chain) - 48)
                    at += _snprintf(chain + at, sizeof(chain) - at, "%s%s",
                                    step ? " -> " : "", nm ? nm : "?");
                if (!RangeReadable(c + off, 4)) break;
                uint8_t* sup = *(uint8_t**)(c + off);
                if (!sup || ((uintptr_t)sup & 3) || !RangeReadable(sup, kNameOff + 8)) break;
                c = sup;
            }
            Log("bq: UStruct::SuperField derived at +0x%X. The full chain from "
                "DishonoredPlayerPawn reads: %s - Pawn, Actor and Object are "
                "reached in order, with intermediates between them.",
                off, chain);
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
// therefore re-enters PeHandler. g_peReentry IS read - console.cpp:72 and
// commands.cpp:319 both test it - but nothing in PeHandler does, so it does
// not guard the hook. The depth is what PeHandler checks, to keep
// mod-originated calls out of both its side effects and its evidence.
static void BqCall(uint8_t* obj, uint8_t* fn, void* parms)
{
    // RESTORE, do not force. g_peReentry is read by console.cpp:72 and
    // commands.cpp:319 - an earlier note in this file claimed it was read
    // nowhere, from a grep that only covered process_event.cpp. Clearing it
    // unconditionally would hand those two callers a false answer if we were
    // ever called with it already set.
    const bool prevReentry = g_peReentry;
    g_bqDepth++;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(obj, fn, parms, NULL);
    g_peReentry = prevReentry;
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
static BqEnd BqWalk(uint8_t* comp, const char* startName, BqName* outChain, int* outN)
{
    *outN = 0;
    const uint32_t si = FindNameIdx(startName);
    if (si == 0xffffffffu) {
        Log("bq/chain: '%s' is not in GNames at all - the name does not exist "
            "on this build, which is a different thing from the bone being "
            "absent from the skeleton", startName);
        return BQ_NOBONE;
    }
    BqName cur; cur.idx = si; cur.num = 0;
    const int idx0 = BqMatchRefBone(comp, cur);
    if (idx0 < 0) {
        Log("bq/chain: '%s' - MatchRefBone returned %d, so this component's "
            "skeleton has no such bone. Reported, not worked around.",
            startName, idx0);
        return BQ_NOBONE;
    }
    // Round-trip before trusting anything: both FName words must come back.
    const BqName back = BqGetBoneName(comp, idx0);
    if (back.idx != cur.idx || back.num != cur.num) {
        Log("bq/chain: '%s' -> index %d, but GetBoneName(%d) returned '%s' "
            "(idx %u num %u vs %u/%u). The round trip FAILED, so the call "
            "contract is wrong and nothing further is trustworthy.",
            startName, idx0, idx0, BqStr(back), back.idx, back.num,
            cur.idx, cur.num);
        return BQ_ROUNDTRIP;
    }

    char line[512]; int at = 0;
    at += _snprintf(line + at, sizeof(line) - at, "%s[%d]", startName, idx0);
    outChain[(*outN)++] = cur;
    BqEnd end = BQ_TRUNC;
    for (int step = 0; step < BQ_MAX_CHAIN; step++) {
        const BqName par = BqGetParentBone(comp, cur);
        if (par.idx == 0xffffffffu) { end = BQ_CALLFAIL; break; }
        // THE ROOT SENTINEL IS THE FULL FNAME, checked before any display
        // name. Index 0 with number 0 is None, and that - and only that -
        // ends a chain successfully.
        if (par.idx == 0 && par.num == 0) { end = BQ_ROOT; break; }
        if (par.idx == cur.idx && par.num == cur.num) { end = BQ_SELF; break; }
        const char* pn = RealName(par.idx);
        if (!pn) { end = BQ_BADNAME; break; }
        int seen = 0;
        for (int k = 0; k < *outN; k++)
            if (outChain[k].idx == par.idx && outChain[k].num == par.num) seen = 1;
        if (seen) { end = BQ_CYCLE; break; }
        // Every non-terminal parent is round-tripped too, not just the seed.
        // A chain is only as trustworthy as its weakest link, and the first
        // build validated exactly one of them.
        const int pidx = BqMatchRefBone(comp, par);
        if (pidx < 0) { end = BQ_NOBONE; break; }
        const BqName pback = BqGetBoneName(comp, pidx);
        if (pback.idx != par.idx || pback.num != par.num) { end = BQ_ROUNDTRIP; break; }
        if (*outN < BQ_MAX_CHAIN) outChain[(*outN)++] = par;
        if (at < (int)sizeof(line) - 64)
            at += _snprintf(line + at, sizeof(line) - at, " -> %s[%d]", pn, pidx);
        cur = par;
    }
    Log("bq/chain: %s -> %s   (%d validated link(s), ended: %s)",
        line, end == BQ_ROOT ? "ROOT" : "<incomplete>", *outN, kBqEndName[end]);
    return end;
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
    BqEnd  chainEnd[BQ_MAX_TARGETS];
    for (int t = 0; t < BQ_MAX_TARGETS; t++) chainEnd[t] = BQ_NOBONE;
    for (int t = 0; t < BQ_MAX_TARGETS && kBqTargets[t]; t++)
        chainEnd[t] = BqWalk(comp, kBqTargets[t], chain[t], &chainN[t]);

    // The verdict the architecture turns on, stated from the chains rather
    // than asserted: is each attachment joint anywhere in its hand's chain?
    for (int side = 0; side < 2; side++) {
        const char* handName = kBqTargets[side];        // hand_L_jnt / hand_R_jnt
        const int   ai = 2 + side;                      // handAttachment_L/R_jnt
        const uint32_t hi = FindNameIdx(handName);
        int found = 0;
        for (int k = 1; k < chainN[ai]; k++)
            if (chain[ai][k].idx == hi) found = 1;

        // A POSITIVE verdict needs only the edge, and the edge is validated.
        // A NEGATIVE verdict needs the WHOLE chain to have terminated at the
        // root: "I did not see the hand" and "I did not finish looking" are
        // different statements, and the first build could not tell them apart.
        if (found) {
            Log("bq/VERDICT: %s IS a descendant of %s (edge validated at link "
                "%d of a walk that ended: %s). Moving the wrist can carry the "
                "weapon through the engine's own attachment path - the case for "
                "the native route.",
                kBqTargets[ai], handName, 1, kBqEndName[chainEnd[ai]]);
        } else if (chainEnd[ai] == BQ_ROOT) {
            Log("bq/VERDICT: %s is NOT a descendant of %s - the chain was "
                "walked to the root and the hand is not in it. A wrist-only "
                "edit cannot carry the weapon automatically. That does not kill "
                "the native route (separate targets can take a coordinated "
                "transform) but it does kill the follow-for-free argument.",
                kBqTargets[ai], handName);
        } else {
            Log("bq/VERDICT: %s vs %s is UNKNOWN. The walk did not reach the "
                "root - it ended: %s after %d link(s) - so the hand's absence "
                "from a PARTIAL chain proves nothing.",
                kBqTargets[ai], handName, kBqEndName[chainEnd[ai]], chainN[ai]);
        }
    }
    BqControls();
    BqItems(comp);

    Log("bq: what this still does NOT establish: whether any later writer "
        "overwrites a pose we set, where the attachment update sits relative "
        "to composition, and whether the item's own mesh is parented where the "
        "socket says. Those are step 2.");
}


// ---- step 1b.3: the LIVE equipped item and its ACTUAL attachment -----------
//
// Taken from the INVERSE link, not from the inventory's slot array.
//
// DishonoredInventory.m_Slots is an array of native PawnInventorySlot records
// and m_EquipUsageInfo is a fixed array of native structs. Neither is a
// pointer array, and decoding a native element layout is exactly the class of
// guess this ticket has been burned by twice. So the walk starts from the
// other end: every DishonoredItemSkeletalComponent carries m_pItem back to
// the item that owns it, and AttachedToSkelComponent to the component it is
// actually attached to. Those are plain object pointers.
//
// That answers the question the socket defaults cannot: not where an item is
// SUPPOSED to attach, but where this live one IS attached. The pistol's own
// tweaks default it to LeftHandWpn (DisTweaks_WepPistol.uc:28), so a report
// built on defaults would have said the wrong hand for at least one weapon.
//
// The attachment RECORD's layout is derived rather than assumed. UE3's
// Attachment struct is { ActorComponent* Component; name BoneName; Vector
// RelativeLocation; Rotator RelativeRotation; Vector RelativeScale }, but
// rather than trusting that stride, the array bytes are searched for this
// child's exact pointer and the FName that follows it is accepted only if
// MatchRefBone recognises it as a bone on the parent. A name that is a real
// bone on the very component the item hangs off is independent evidence; a
// stride that merely looks plausible is not.
static void BqItems(uint8_t* pawnMesh)
{
    const uint32_t oItem   = PrOff("DishonoredItemSkeletalComponent", "m_pItem");
    const uint32_t oAttTo  = PrOff("SkeletalMeshComponent", "AttachedToSkelComponent");
    const uint32_t oAtts   = PrOff("SkeletalMeshComponent", "Attachments");
    const uint32_t oInv    = PrOff("DishonoredPawn", "m_pInventory");
    const uint32_t oInvOwn = PrOff("DishonoredInventory", "m_pOwner");
    if (!oItem || !oAttTo) {
        Log("bq/item: m_pItem (+0x%X) or AttachedToSkelComponent (+0x%X) did "
            "not resolve - the live attachment path is UNKNOWN on this build",
            oItem, oAttTo);
        return;
    }

    // The forward link, as a cheap ownership cross-check. Pointers only.
    if (oInv && oInvOwn && RangeReadable(g_pePawn + oInv, 4)) {
        uint8_t* inv = *(uint8_t**)(g_pePawn + oInv);
        if (inv && !((uintptr_t)inv & 3) && RangeReadable(inv + oInvOwn, 4)) {
            uint8_t* own = *(uint8_t**)(inv + oInvOwn);
            Log("bq/item: pawn %p -> inventory %p, whose m_pOwner is %p (%s)",
                (void*)g_pePawn, (void*)inv, (void*)own,
                own == g_pePawn ? "AGREES with the pawn"
                                : "DISAGREES - ownership is not what it claims");
        }
    }

    if (!RangeReadable((void*)kGObjHdr, 12)) return;
    void**   objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return;

    int found = 0;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x200)) continue;
        const char* cn = ObjClassName(o);
        if (!cn || strcmp(cn, "DishonoredItemSkeletalComponent")) continue;
        if (!RangeReadable(o + oItem, 4)) continue;
        uint8_t* item = *(uint8_t**)(o + oItem);
        if (!item || ((uintptr_t)item & 3) || !RangeReadable(item, 0x80)) continue;

        uint8_t* parent = NULL;
        if (RangeReadable(o + oAttTo, 4)) parent = *(uint8_t**)(o + oAttTo);
        const char* itemCls = ObjClassName(item);
        const bool ours = (parent == pawnMesh);

        // Where is it ACTUALLY attached? Search the parent's Attachments bytes
        // for this exact child pointer, then validate the FName that follows.
        char where[192];
        _snprintf(where, sizeof(where), "%s",
                  parent ? "parent has no readable Attachments array"
                         : "NOT ATTACHED (AttachedToSkelComponent is null)");
        if (parent && !((uintptr_t)parent & 3) && oAtts &&
            RangeReadable(parent + oAtts, 12)) {
            uint8_t* ad = *(uint8_t**)(parent + oAtts);
            const int  an = *(int*)(parent + oAtts + 4);
            const int  ac = *(int*)(parent + oAtts + 8);
            if (ad && !((uintptr_t)ad & 3) && an > 0 && an <= 256 && ac >= an &&
                RangeReadable(ad, (size_t)an * 64)) {
                int hit = -1;
                // THIS DOES NOT DERIVE A STRIDE, and an earlier comment here
                // said it did. It finds this child's pointer somewhere in the
                // array's bytes and reads the FName four bytes past it. That
                // establishes neither the record size nor that the pointer is
                // a Component field of a real element rather than something in
                // adjacent memory. The bone cross-check makes a false positive
                // unlikely, not impossible, so the result is labelled
                // UNVALIDATED and must not be built on.
                for (int b = 0; b + 12 <= an * 64; b += 4) {
                    if (*(uint8_t**)(ad + b) != o) continue;
                    BqName bn;
                    bn.idx = *(uint32_t*)(ad + b + 4);
                    bn.num = *(uint32_t*)(ad + b + 8);
                    const int bi = BqMatchRefBone(parent, bn);
                    if (bi < 0) continue;          // not a bone: wrong offset
                    const char* nm = RealName(bn.idx);
                    float loc[3] = { 0, 0, 0 };
                    if (RangeReadable(ad + b + 12, 12)) memcpy(loc, ad + b + 12, 12);
                    _snprintf(where, sizeof(where),
                              "bone '%s'[%d] local (%.2f %.2f %.2f) [UNVALIDATED "
                              "LAYOUT: pointer found at byte +%d, name read at "
                              "+4 from it, num %u - the record size is NOT "
                              "established]",
                              nm ? nm : "?", bi, loc[0], loc[1], loc[2], b, bn.num);
                    hit = b;
                    break;
                }
                if (hit < 0)
                    _snprintf(where, sizeof(where),
                              "NOT FOUND in the parent's %d attachment record(s) "
                              "- either the child is attached another way or the "
                              "record layout is not what was searched for", an);
            }
        }

        Log("bq/item: %s component %p, item %p (%s) -> parent %p %s | attached at %s",
            ours ? "OURS:" : "other", (void*)o, (void*)item,
            itemCls ? itemCls : "?", (void*)parent,
            ours ? "= the player's mesh" : "(not the player's mesh)", where);
        found++;
        if (found >= 16) { Log("bq/item: stopping at 16"); break; }
    }
    if (!found) {
        // Nothing matched, and there are two very different reasons for that:
        // nothing is equipped through this component type, or the runtime
        // class is not the one the corpus names. Say which by listing what
        // item-ish component classes DO exist, rather than leaving the reader
        // to assume the first.
        Log("bq/item: no DishonoredItemSkeletalComponent carrying an m_pItem "
            "was found. That is UNKNOWN, not empty: this matched the class name "
            "EXACTLY, so a derived component class would have been missed, and "
            "the report fires once early when the inventory may not be "
            "populated. Listing component classes with 'Item' in the name:");
        uint32_t seen[16]; int seenN = 0; int shown = 0;
        for (uint32_t i = 0; i < onum && shown < 12; i++) {
            if ((i & 1023) == 0) {
                uint32_t left = onum - i; if (left > 1024) left = 1024;
                if (!RangeReadable(objs + i, left * sizeof(void*))) break;
            }
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x40)) continue;
            const char* cn = ObjClassName(o);
            if (!cn || !strstr(cn, "Item")) continue;
            // NOT static: a function-local static survives the call, so a
            // second run found no NEW classes and printed "none at all",
            // which reads as an empty inventory. That is a false negative
            // dressed as a measurement - the exact thing this scan exists to
            // avoid. Per-snapshot, declared by the caller.
            const uint32_t ci = *(uint32_t*)(*(uint8_t**)(o + kClassOff) + kNameOff);
            int dup = 0;
            for (int k = 0; k < seenN; k++) if (seen[k] == ci) dup = 1;
            if (dup) continue;
            if (seenN < 16) seen[seenN++] = ci;
            Log("bq/item:   class present: %s", cn);
            shown++;
        }
        if (!shown)
            Log("bq/item:   none found with 'Item' in the class name. This does "
                "NOT establish an empty inventory - it is one scan, of one "
                "moment, matching on a substring.");
    }
    Log("bq/item: any bone above is read through an UNVALIDATED record layout "
        "and is a candidate, not a measurement. Nothing here has yet agreed or "
        "disagreed with the authored socket defaults - the pistol's default is "
        "LeftHandWpn, and no live result has contradicted it.");
}


// ---- step 2a part 1: the three hand controls ------------------------------
//
// Read-only. Nothing is enabled, re-flagged, grafted or ticked - the point is
// to observe what is there, and changing a control to make it observable
// changes the thing being measured.
//
// WHAT THIS REPORT REFUSES TO CLAIM. A control's variable name does not
// establish which bone it drives. ENGINE_NOTES already records that the CAMERA
// control aims the arms at the view and went untouched for a dozen builds
// while people looked at the hand controls - so on this asset the naming has
// demonstrably misled before. Where a control's own fields do not name a
// target, this prints UNKNOWN.
//
// Sharing is the first question and it is cheap: if two of the three pointers
// are the same object, then anything done to one is done to the other, and
// every later plan that treats them as independent is wrong from the start.
static void BqControls(void)
{
    static const char* const kName[3] = {
        "m_pLookAtControl_LeftHand", "m_pLookAtControl_RightHand",
        "m_pLookAtControl_Camera"
    };
    const uint32_t oCtl[3] = {
        PrOff("DishonoredPlayerPawn", "m_pLookAtControl_LeftHand"),
        PrOff("DishonoredPlayerPawn", "m_pLookAtControl_RightHand"),
        PrOff("DishonoredPlayerPawn", "m_pLookAtControl_Camera")
    };
    const uint32_t oCName = PrOff("SkelControlBase", "ControlName");
    const uint32_t oCStr  = PrOff("SkelControlBase", "ControlStrength");
    const uint32_t oCNext = PrOff("SkelControlBase", "NextControl");
    const uint32_t oCTag  = PrOff("SkelControlBase", "ControlTickTag");
    const uint32_t oCScale= PrOff("SkelControlBase", "BoneScale");

    Log("bq/ctl: ==== the three hand controls ==== (offsets: name +0x%X, "
        "strength +0x%X, next +0x%X, ticktag +0x%X, bonescale +0x%X)",
        oCName, oCStr, oCNext, oCTag, oCScale);

    uint8_t* obj[3] = { NULL, NULL, NULL };
    for (int i = 0; i < 3; i++) {
        if (!oCtl[i]) { Log("bq/ctl: %s did not resolve - UNKNOWN", kName[i]); continue; }
        if (!RangeReadable(g_pePawn + oCtl[i], 4)) continue;
        obj[i] = *(uint8_t**)(g_pePawn + oCtl[i]);
    }

    // SHARING FIRST. Two names for one object changes every plan downstream.
    for (int a = 0; a < 3; a++)
        for (int b = a + 1; b < 3; b++)
            if (obj[a] && obj[a] == obj[b])
                Log("bq/ctl: *** %s AND %s ARE THE SAME OBJECT (%p) *** - they "
                    "cannot be driven independently, and any plan that treats "
                    "them as two controls is wrong.",
                    kName[a], kName[b], (void*)obj[a]);

    for (int i = 0; i < 3; i++) {
        if (!obj[i]) { Log("bq/ctl: %s is null", kName[i]); continue; }
        if (((uintptr_t)obj[i] & 3) || !RangeReadable(obj[i], 0x120)) {
            Log("bq/ctl: %s = %p, not a readable object", kName[i], (void*)obj[i]);
            continue;
        }
        const char* cls = ObjClassName(obj[i]);
        const bool isSingle = BqReceiverIsA(obj[i], "SkelControlSingleBone");
        const bool isBase   = BqReceiverIsA(obj[i], "SkelControlBase");

        char nm[64] = "UNKNOWN";
        if (oCName && RangeReadable(obj[i] + oCName, 8)) {
            const uint32_t ni = *(uint32_t*)(obj[i] + oCName);
            const uint32_t nn = *(uint32_t*)(obj[i] + oCName + 4);
            const char* rn = RealName(ni);
            _snprintf(nm, sizeof(nm), "%s (num %u)", rn ? rn : "None/invalid", nn);
        }
        float str = -1.0f, scale = -1.0f; int tag = -1;
        if (oCStr && RangeReadable(obj[i] + oCStr, 4)) memcpy(&str, obj[i] + oCStr, 4);
        if (oCScale && RangeReadable(obj[i] + oCScale, 4)) memcpy(&scale, obj[i] + oCScale, 4);
        if (oCTag && RangeReadable(obj[i] + oCTag, 4)) memcpy(&tag, obj[i] + oCTag, 4);

        Log("bq/ctl: %s = %p, class '%s' (SkelControlSingleBone: %s, "
            "SkelControlBase: %s) | ControlName %s | strength %.3f | boneScale "
            "%.3f | tickTag %d",
            kName[i], (void*)obj[i], cls ? cls : "?",
            isSingle ? "yes" : "NO", isBase ? "yes" : "NO",
            nm, str, scale, tag);

        // The chain. Bounded and visited-checked, like every other walk here.
        if (oCNext) {
            uint8_t* c = obj[i]; char line[256]; int at = 0; int n = 0;
            uint8_t* seen[16]; int seenN = 0;
            while (c && n < 16) {
                if (((uintptr_t)c & 3) || !RangeReadable(c + oCNext, 4)) break;
                int dup = 0;
                for (int k = 0; k < seenN; k++) if (seen[k] == c) dup = 1;
                if (dup) { at += _snprintf(line + at, sizeof(line) - at, " -> CYCLE"); break; }
                if (seenN < 16) seen[seenN++] = c;
                const char* cn2 = ObjClassName(c);
                if (at < (int)sizeof(line) - 48)
                    at += _snprintf(line + at, sizeof(line) - at, "%s%s",
                                    n ? " -> " : "", cn2 ? cn2 : "?");
                c = *(uint8_t**)(c + oCNext);
                n++;
            }
            Log("bq/ctl:   NextControl chain: %s%s", n ? line : "(empty)",
                (n >= 16) ? " -> TRUNCATED" : "");
        }
    }

    Log("bq/ctl: WHICH BONE each control drives is NOT reported, because "
        "nothing read above names one. A control's variable name is not that "
        "evidence - ENGINE_NOTES records the CAMERA control aiming the arms at "
        "the view while the hand controls were being studied. Establishing the "
        "target needs the control's own bone field or a native consumer, and "
        "that is step 2a part 2.");
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
