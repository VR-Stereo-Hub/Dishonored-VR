// game/dishonored/hands/arms_hide.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ---------------------------------------------------------------------------
// Bone probe (build 30.7, READ-ONLY). Groundwork for hiding the arms while
// keeping the hands: for every candidate component, locate anything shaped
// like BoneVisibilityStates (TArray of bytes all <= 2 - logging the actual
// values settles which value means "visible" by observation, not by trusting
// my memory of the UE3 enum), and walk the component's SkeletalMesh asset for
// the RefSkeleton bone-name table. Validation is built in: the weapon view
// model must report exactly the 14 bones the handoff documented.
// ---------------------------------------------------------------------------
static uint8_t* FpAssetObj(uint8_t* comp)
{
    // 30.15: candidate [1] (the real first-person arms, most likely) resolved
    // no asset - widen the window and accept engine-specific subclasses whose
    // class name merely CONTAINS SkeletalMesh
    for (uint32_t o = 0x20; o + 4 <= 0x600; o += 4) {
        if (!RangeReadable(comp + o, 4)) break;
        uint8_t* a = *(uint8_t**)(comp + o);
        if (!LooksLikeObj(a)) continue;
        const char* ac = ObjClassName(a);
        if (ac && strstr(ac, "SkeletalMesh") && !strstr(ac, "Component"))
            return a;
    }
    return NULL;
}


// ---------------------------------------------------------------------------
// Arms experiment (build 30.13). The 30.12 probe found a per-bone byte array
// at component+0x288 on the player rig: 79 entries, 0xFF everywhere except
// 0x02 on spine_3_jnt - the chest bone the game itself hides in first person.
// Hypothesis: 0xFF = visible, 0x02 = hidden. F3 toggles writing 0x02 onto the
// arm-chain bones (collarbone/shoulder/upper/lower arm/sleeve - never hands
// or fingers), saving and restoring the exact original bytes.
// ---------------------------------------------------------------------------
static bool FindRefSkel(uint8_t* a, uint8_t** dOut, int* nOut, uint32_t* stOut,
                        uint32_t* noOut)
{
    for (uint32_t o = 0x28; o + 12 <= 0x300; o += 4) {
        if (!RangeReadable(a + o, 12)) break;
        uint8_t* d  = *(uint8_t**)(a + o);
        int32_t num = *(int32_t*)(a + o + 4);
        int32_t max = *(int32_t*)(a + o + 8);
        if (num < 8 || num > 256 || max < num || max > 512) continue;
        if (!d || ((uintptr_t)d & 3)) continue;
        for (uint32_t st = 12; st <= 64; st += 4) {
            if (!RangeReadable(d, (size_t)num * st)) continue;
            for (uint32_t no = 0; no + 8 <= st; no += 4) {
                int tries = num < 6 ? num : 6, good = 0;
                for (int e = 0; e < tries; e++)
                    if (RealName(*(uint32_t*)(d + (size_t)e*st + no))) good++;
                if (good != tries) continue;
                *dOut = d; *nOut = (int)num; *stOut = st; *noOut = no;
                return true;
            }
        }
    }
    return false;
}


static bool ArmsPrep()
{
    g_armBoneCnt = 0; g_armComp = NULL; g_bvBoneTotal = 0;
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        if (!LooksLikeObj(k->obj)) continue;
        uint8_t* a = FpAssetObj(k->obj);
        if (!a) continue;
        uint8_t* rd; int rn; uint32_t rs, rno;
        if (!FindRefSkel(a, &rd, &rn, &rs, &rno) || rn < 60) continue;
        for (int e = 0; e < rn && g_armBoneCnt < 40; e++) {
            const uint32_t nidx = *(uint32_t*)(rd + (size_t)e*rs + rno);
            const uint32_t nnum = *(uint32_t*)(rd + (size_t)e*rs + rno + 4);
            const char* nm = RealName(nidx);
            if (!nm) continue;
            if (strstr(nm, "upper_arm") || strstr(nm, "lower_arm") ||
                strstr(nm, "shoulder_") || strstr(nm, "Collarbone") ||
                strstr(nm, "sleeve")) {
                ArmBone* b = &g_armBones[g_armBoneCnt++];
                // 30.16: force the FName Number half to 0. The dword after
                // the name index in FMeshBone may be Flags, not Number - a
                // garbage Number makes every lookup silently miss, which
                // matches the zero-effect zero-allocation result of 30.15.
                b->idx = nidx; b->num = 0;
                b->bone = e;    // VR-31: the index BoneVisibilityStates uses
                if (nnum != 0)
                    Log("arms: note - raw dword after '%s' index was %u",
                        nm, nnum);
                snprintf(b->nm, sizeof(b->nm), "%s", nm);
            }
        }
        if (!g_armBoneCnt) continue;
        g_armComp = k->obj;
        g_bvBoneTotal = rn;     // VR-31: what a per-bone array on this rig must measure
        return true;
    }
    Log("arms: could not locate the body rig");
    return false;
}


// after hiding, see who actually reacted: HideBoneByName makes the engine
// allocate its per-bone visibility array, so a component that matched any of
// the names now has a byte array with 0/2 values where there was none before
static void ArmsReport(uint8_t* comp, const char* asset)
{
    for (uint32_t o = 0x100; o + 12 <= 0x1000; o += 4) {
        if (!RangeReadable(comp + o, 12)) break;
        uint8_t* d  = *(uint8_t**)(comp + o);
        int32_t num = *(int32_t*)(comp + o + 4);
        int32_t max = *(int32_t*)(comp + o + 8);
        if (num < 8 || num > 256 || max < num || max > 512) continue;
        if (!d || !RangeReadable(d, (size_t)num)) continue;
        bool allSmall = true; int twos = 0;
        for (int b = 0; b < num; b++) {
            if (d[b] > 2) { allSmall = false; break; }
            if (d[b] == 2) twos++;
        }
        if (!allSmall) continue;
        Log("arms:    '%s' visibility array +0x%03x num=%d hidden=%d",
            asset, (unsigned)o, (int)num, twos);
    }
}


// 30.17: per-bone hiding is a dead end in this branch (ProcessEvent-called
// HideBoneByName provably does nothing - no visuals, no allocation, twice).
// Cull whole components instead, with a lever ALREADY PROVEN to work: the
// transform-input block. MaxDrawDistance lives at +0x1bc/+0x1c0 beside the
// Rotation we write every frame; 0.01 uu means "cull unless the camera is
// inside it". Children keep their own draw distances, so the weapons stay.
// Targets: every non-view-model unknown except Skm_Player (never touch the
// camera carrier) and the crossbow add-on parts.
static void ArmsToggle()
{
    if (!g_armHidden) {
        int n = 0;
        for (int i = 0; i < g_fpCandN; i++) {
            FpCand* k = &g_fpCand[i];
            if (FpIsViewModel(k)) continue;
            if (!LooksLikeObj(k->obj) || !FpFieldsLookRight(k->obj)) continue;
            if (strstr(k->asset, "Skm_Player")) continue;
            if (strstr(k->asset, "crossbow")) continue;
            if (strstr(k->asset, "bolt")) continue;
            if (!RangeReadable(k->obj + 0x1bc, 8)) continue;
            float* dd = (float*)(k->obj + 0x1bc);
            k->ddSave[0] = dd[0]; k->ddSave[1] = dd[1];
            dd[0] = 0.01f; dd[1] = 0.01f;
            k->ddCulled = true;
            Log("arms: culled [%d] '%s' name=%s cls=%s (was %.0f/%.0f)",
                i, k->asset, k->name, k->cls, k->ddSave[0], k->ddSave[1]);
            n++;
        }
        g_armHidden = true;
        Log("arms: draw-distance cull on %d component(s) - F3 restores", n);
    } else {
        for (int i = 0; i < g_fpCandN; i++) {
            FpCand* k = &g_fpCand[i];
            if (!k->ddCulled) continue;
            if (LooksLikeObj(k->obj) && RangeReadable(k->obj + 0x1bc, 8)) {
                float* dd = (float*)(k->obj + 0x1bc);
                dd[0] = k->ddSave[0]; dd[1] = k->ddSave[1];
            }
            k->ddCulled = false;
        }
        g_armHidden = false;
        Log("arms: draw distances restored");
    }
}


// 38.29 -------------------------------------------------------------------
// ARMS AWAY WHILE CROUCHED - the user's own A/B, repeated twice and finally
// believed: with the first-person mesh drive OFF (the HOME toggle) he can
// crouch under furniture every time; with it ON he wedges. That is the one
// experiment in this whole saga that reproduces on demand, and it points at
// OUR arms, not the game's collision, not the camera, not the stick. His
// complaint about HOME was never that it failed - it was that it leaves the
// arms planted in the world, which feels awful. So: while he is crouched,
// the arms go away entirely - the fp-mesh drive is released (exactly the
// HOME-off state that works) AND the hand controls are scaled to nothing so
// there is nothing planted to look at. Standing restores both.
// [Hands] CrouchHideArms=0 reverts; CrouchHideCyl sets what counts as
// crouched (default 76 - below the game's 87.5 standing cylinder, so it
// covers plain crouch 65 and the slide/vent 33 alike).
static void ArmsHideTick()
{
    if (!g_crouchHideCfg) {
        if (g_armsHidden) { g_armsHidden = false; g_fpHaveBase = false; }
        return;
    }
    bool want = (g_cylLast > 10.0f && g_cylLast < g_crouchHideCyl &&
                 (MaimNowMs() - g_cylOkMs) < 1500.0);
    if (want == g_armsHidden) return;
    g_armsHidden = want;
    if (want) {
        FpRestoreRotation();          // put the mesh back before we let go
        g_fpHaveBase = false;
        Log("arms: fp-MESH DRIVE RELEASED (cyl %.1f) - arms stay visible, "
            "normal size, still on your controllers", g_cylLast);
    } else {
        g_fpHaveBase = false;
        Log("arms: fp-mesh drive back (cyl %.1f)", g_cylLast);
    }
}


// ===========================================================================
// VR-31 route (a): the per-bone visibility array, resolved BY NAME (session 18)
//
// The ask was "write 0x02 into the array at component +0x288 for the arm
// chain". That experiment already ran, and its result IS on the record - not
// in ENGINE_NOTES, and not in this file, but in a comment the original author
// left in the state chunk beside it (src/mod/state/40_..._arms_hide.inc):
//
//     30.14: the +0x288 byte poke was wrong - that array turned out to be some
//     per-bone animation control (arms froze to the view and rode the head).
//
// So +0x288 is not BoneVisibilityStates, and the symptom names what it is
// likely to be instead. UE3's USkeletalMeshComponent carries a second per-bone
// byte array, SkelControlIndex, whose "no control on this bone" value is 255 -
// which is exactly the 0xFF-everywhere-with-one-0x02 pattern 30.12 found. On
// that reading 30.13 did not hide the arm bones, it ATTACHED them to
// SkelControl #2, and "froze to the view and rode the head" is what an attached
// control does. A hidden bone does not ride anything.
//
// That is a hypothesis, and the engine can settle it without a guess: this
// build ships full UE3 reflection (FindPropOffset reads UProperty::Offset out
// of GObjects, the same technique the crouch cylinder and the graft use). So
// ask the engine where BoneVisibilityStates, SkelControlIndex and RequiredBones
// actually live, print all three against 0x288, and only then write - into the
// offset the engine named, never into a number a probe pattern-matched.
//
// VERDICT, four runs later: this build has NO BoneVisibilityStates - not on
// SkeletalMeshComponent, not on any class in GObjects, and no byte array on the
// rig is shaped like one. Route (a) is closed and the lever is back to OFF
// ([Hands] BoneVisHide=0); what is kept is the diagnostic, because it is the
// thing that closed the route and it re-answers the question on demand.
// "arms vis status" runs the whole of it, "arms vis chain" prints the arm
// chain, "arms vis on" still refuses and says why.
//
// The census exists because of VR-30's method note: measure write survival
// before tuning what a write contains. Every tick the lever is on it reads back
// what it wrote and classifies each byte ours / reverted / third party, so the
// log can tell "the write did not survive" apart from "the write survived and
// the renderer does not care", which look identical on screen.
// ===========================================================================

// Read a UE3 TArray of bytes (Data, ArrayNum, ArrayMax) off a component.
static bool BoneVisReadArray(uint8_t* comp, uint32_t off,
                             uint8_t** dOut, int32_t* nOut)
{
    *dOut = NULL; *nOut = 0;
    if (!comp || !off || !RangeReadable(comp + off, 12)) return false;
    uint8_t* d  = *(uint8_t**)(comp + off);
    int32_t  n  = *(int32_t*)(comp + off + 4);
    int32_t  mx = *(int32_t*)(comp + off + 8);
    if (n < 0 || n > 1024 || mx < n || mx > 4096) return false;
    if (n > 0 && (!d || !RangeReadable(d, (size_t)n))) return false;
    *dOut = d; *nOut = n;
    return true;
}


// One reflection lookup, logged whether it succeeds or not. A 0 here is a real
// answer: it means this build's SkeletalMeshComponent does not declare the
// property at all, which closes the route by itself.
static void BoneVisResolve(const char* why)
{
    if (g_bvResolved) return;
    g_bvResolved = true;
    g_bvOffVis = FindPropOffset("SkeletalMeshComponent", "BoneVisibilityStates");
    g_bvOffCtl = FindPropOffset("SkeletalMeshComponent", "SkelControlIndex");
    g_bvOffReq = FindPropOffset("SkeletalMeshComponent", "RequiredBones");
    Log("bonevis: reflection (%s) - BoneVisibilityStates +0x%03x, "
        "SkelControlIndex +0x%03x, RequiredBones +0x%03x "
        "(0 = the engine does not declare that property on this class)",
        why, g_bvOffVis, g_bvOffCtl, g_bvOffReq);
    const char* what = g_bvOffVis == kSkcIndexOff ? "BoneVisibilityStates"
                     : g_bvOffCtl == kSkcIndexOff ? "SkelControlIndex"
                     : g_bvOffReq == kSkcIndexOff ? "RequiredBones"
                     : "NONE of the three";
    Log("bonevis: +0x288 - the array 30.12 found and 30.13 wrote 0x02 into - "
        "is %s. 30.14 recorded that write as freezing the arms to the view, "
        "which is what SkelControlIndex would do and what hiding would not.",
        what);
    if (!g_bvOffVis)
        Log("bonevis: no BoneVisibilityStates property - route (a) is CLOSED on "
            "this build, and not because a write failed");
}


// The component and the arm chain. Re-prepped whenever the cached component is
// no longer a live object; the fp candidate list is rebuilt behind our back.
static bool BoneVisPrep()
{
    BoneVisResolve("prep");
    if (!g_bvOffVis) return false;
    if (g_armComp && LooksLikeObj(g_armComp) && g_armBoneCnt > 0) return true;
    if (!ArmsPrep()) return false;          // logs its own failure
    Log("bonevis: rig prepped - %d arm-chain bone(s) of %d in the ref skeleton",
        g_armBoneCnt, g_bvBoneTotal);
    return g_armBoneCnt > 0 && g_bvBoneTotal > 0;
}


static void BoneVisChain()
{
    if (!BoneVisPrep()) { Log("bonevis: no rig to list"); return; }
    for (int i = 0; i < g_armBoneCnt; i++)
        Log("bonevis:   [%2d] bone %3d %s", i, g_armBones[i].bone,
            g_armBones[i].nm);
    Log("bonevis: %d bone(s). Hands and fingers are excluded by the name match. "
        "If the weapon flies off when this is on, its attach bone is in this "
        "list: UE3 gives a hidden bone a zero transform and the attachment path "
        "inverse-decomposes it, which is the rule BioShock paid for.",
        g_armBoneCnt);
}


static void BoneVisOff(const char* why)
{
    if (!g_bvOn) return;
    g_bvOn = false;
    uint8_t* d; int32_t n; int restored = 0;
    if (g_bvSaveComp && LooksLikeObj(g_bvSaveComp) &&
        BoneVisReadArray(g_bvSaveComp, g_bvOffVis, &d, &n)) {
        for (int i = 0; i < g_armBoneCnt; i++) {
            const int b = g_armBones[i].bone;
            if (b < 0 || b >= n) continue;
            d[b] = g_bvSave[i];
            restored++;
        }
    }
    Log("bonevis: OFF (%s) - %d/%d byte(s) restored to what the engine had",
        why, restored, g_armBoneCnt);
    g_bvSaveComp = NULL;
}


// UE3 EBoneVisibilityStatus: 0 = hidden by parent, 1 = visible,
// 2 = explicitly hidden. Only 2 is ours to write.
static const uint8_t kBvsVisible = 1, kBvsHidden = 2;

static bool BoneVisOn()
{
    if (g_bvOn) return true;
    if (!BoneVisPrep()) return false;
    uint8_t* d; int32_t n;
    if (!BoneVisReadArray(g_armComp, g_bvOffVis, &d, &n)) {
        Log("bonevis: REFUSED - component+0x%03x does not hold a readable "
            "byte array on this component", g_bvOffVis);
        return false;
    }
    if (n == 0) {
        Log("bonevis: REFUSED - BoneVisibilityStates is EMPTY (num=0) on this "
            "rig. The engine never allocated it, so there is nothing per-bone "
            "hiding can drive and nothing our write could reach. That is route "
            "(a) closed with a reason rather than a failed write, and it is the "
            "same fact 30.17 saw when HideBoneByName allocated nothing.");
        return false;
    }
    if (n != g_bvBoneTotal) {
        Log("bonevis: REFUSED - the array holds %d bytes but the ref skeleton "
            "has %d bones, so it is not per-bone for THIS mesh and our indices "
            "would land somewhere else", n, g_bvBoneTotal);
        return false;
    }
    int wrote = 0, alreadyHidden = 0, oddValue = 0;
    for (int i = 0; i < g_armBoneCnt; i++) {
        const int b = g_armBones[i].bone;
        if (b < 0 || b >= n) continue;
        g_bvSave[i] = d[b];
        if (d[b] == kBvsHidden) alreadyHidden++;
        else if (d[b] != kBvsVisible) oddValue++;
        d[b] = kBvsHidden;
        wrote++;
    }
    g_bvSaveComp = g_armComp;
    g_bvOn = true;
    g_bvHeld = g_bvReverted = g_bvOther = 0;
    g_bvWrites = wrote;
    g_bvNextLogMs = 0.0;
    Log("bonevis: ON - wrote %u into %d of %d bone(s) at component+0x%03x "
        "(%d were already hidden, %d held a value that is neither visible nor "
        "hidden). arms vis off restores the saved bytes; the census says "
        "whether they survive.",
        kBvsHidden, wrote, n, g_bvOffVis, alreadyHidden, oddValue);
    BoneVisChain();
    return true;
}




// ---------------------------------------------------------------------------
// Closing route (a) properly (session 18, run 1).
//
// Run 1 answered the identification outright: +0x288 IS SkelControlIndex
// (RequiredBones came back at +0x23c, so the resolver was working on the right
// class), and BoneVisibilityStates did not resolve at all. That is the route
// closed - but a 0 from FindPropOffset has two readings, and only one of them
// is "the array is not there":
//
//   1. the property is declared on a SUBCLASS, so matching Outer ==
//      SkeletalMeshComponent missed it;
//   2. the array exists in C++ but is not exposed to script reflection, which
//      UE3 does for plenty of transient render-side state.
//
// Both are cheap to rule out, and both are read-only. BoneVisFindPropAnywhere
// asks for the property by NAME with no class constraint and reports whichever
// class declares it. BoneVisScanArrays walks the actual rig component for a
// byte array shaped like BoneVisibilityStates and validated against the thing
// 30.12's probe could not check: its length must equal the ref skeleton's bone
// count, and its values must all be legal visibility states.
//
// If both come back empty, route (a) is closed on evidence rather than on one
// lookup returning zero.
// ---------------------------------------------------------------------------

static uint32_t BoneVisFindPropAnywhere(const char* propName, char* clsOut,
                                        size_t clsCap)
{
    if (clsOut && clsCap) clsOut[0] = 0;
    uint32_t pi = FindNameIdx(propName);
    if (pi == 0xffffffffu) return 0;
    if (!RangeReadable((void*)kGObjHdr, 12)) return 0;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return 0;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (*(uint32_t*)(o + kNameOff) != pi) continue;
        const char* pc = ObjClassName(o);
        if (!pc || !strstr(pc, "Property")) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        const char* on = NULL;
        if (ou && !((uintptr_t)ou & 3) && RangeReadable(ou, kNameOff + 4))
            on = NameFromIndex(*(uint32_t*)(ou + kNameOff));
        if (clsOut && clsCap)
            snprintf(clsOut, clsCap, "%s", on ? on : "<unnamed outer>");
        return *(uint32_t*)(o + 0x5c);
    }
    return 0;
}


// One-shot, read-only. Every byte array on the rig component whose length is
// the ref skeleton's bone count, with what its values actually are - so a
// candidate can be judged instead of guessed at. 30.12 found +0x288 by shape
// alone and the bone count is exactly the check it was missing.
static void BoneVisScanArrays()
{
    if (!g_armComp || !LooksLikeObj(g_armComp) || g_bvBoneTotal <= 0) {
        Log("bonevis: scan skipped - no prepped rig to scan");
        return;
    }
    int found = 0;
    for (uint32_t o = 0x20; o + 12 <= 0x1000; o += 4) {
        if (!RangeReadable(g_armComp + o, 12)) break;
        uint8_t* d; int32_t n;
        if (!BoneVisReadArray(g_armComp, o, &d, &n)) continue;
        if (n != g_bvBoneTotal || !d) continue;
        int v[4] = { 0, 0, 0, 0 };      // counts of 0, 1, 2, everything else
        uint8_t mn = 255, mx = 0;
        for (int b = 0; b < n; b++) {
            const uint8_t x = d[b];
            v[x < 3 ? x : 3]++;
            if (x < mn) mn = x;
            if (x > mx) mx = x;
        }
        const bool legal = (v[3] == 0);
        Log("bonevis: scan +0x%03x num=%d zeros=%d ones=%d twos=%d other=%d "
            "range %u..%u -> %s", (unsigned)o, n, v[0], v[1], v[2], v[3],
            mn, mx,
            legal ? "SHAPED LIKE BoneVisibilityStates (all values 0..2)"
                  : "not a visibility array (values outside 0..2)");
        found++;
    }
    Log("bonevis: scan done - %d per-bone byte array(s) of length %d on the "
        "rig. A line saying SHAPED LIKE is a candidate the reflection did not "
        "name; none at all means the array is not on this component in any "
        "form, and route (a) is closed on evidence.", found, g_bvBoneTotal);

    // Free while we are here, and it is the array this build actually has:
    // what SkelControlIndex holds on the arm chain says whether route (c) -
    // pointing those bones at a control of OUR own - has anywhere to point.
    // 0xFF on a bone means no control owns it and the slot is free; a real
    // index means the game is already driving it and 30.13's lesson applies.
    uint8_t* sd; int32_t sn;
    if (g_bvOffCtl && BoneVisReadArray(g_armComp, g_bvOffCtl, &sd, &sn) &&
        sn == g_bvBoneTotal) {
        int free = 0, owned = 0;
        for (int i = 0; i < g_armBoneCnt; i++) {
            const int b = g_armBones[i].bone;
            if (b < 0 || b >= sn) continue;
            if (sd[b] == 0xFF) free++; else owned++;
            Log("bonevis: skc  bone %3d %-24s SkelControlIndex=%u%s",
                g_armBones[i].bone, g_armBones[i].nm, sd[b],
                sd[b] == 0xFF ? " (free)" : " (the game drives this one)");
        }
        Log("bonevis: SkelControlIndex on the arm chain - %d free, %d already "
            "driven by the game. All free = route (c) has somewhere to point; "
            "any driven = taking it means taking over what the game was doing "
            "with it, which is what 30.13 did by accident.", free, owned);
    }
}


// Called when the reflection lookup comes back empty. The two cross-checks have
// DIFFERENT preconditions and run 2 paid for conflating them: the name search
// needs only GObjects and answered on the first tick, while the scan needs a
// prepped rig - and the first tick is about ten seconds before the pawn is
// latched, so the scan reported "could not prep the rig" and the one-shot flag
// then stopped it ever trying again. The name search stays one-shot; the scan
// retries until there is something to scan.
static void BoneVisCloseOut()
{
    if (g_bvClosedOut) return;
    g_bvClosedOut = true;
    char cls[64] = "";
    const uint32_t any = BoneVisFindPropAnywhere("BoneVisibilityStates", cls,
                                                 sizeof(cls));
    if (any)
        Log("bonevis: BoneVisibilityStates IS declared, on class '%s' at "
            "+0x%03x - the SkeletalMeshComponent lookup missed it because it "
            "lives on a subclass. Route (a) is NOT closed; retarget there.",
            cls, any);
    else
        Log("bonevis: no UProperty named BoneVisibilityStates on ANY class in "
            "GObjects, so it is not merely on a subclass");
    Log("bonevis: the byte-array scan waits for a live rig - it runs on the "
        "first tick after the pawn is latched, not now");
}


// The half that needs a rig. Called every tick until it gets one; the
// precondition is checked HERE rather than inside ArmsPrep, because ArmsPrep
// logs its own failure and calling it on a dead rig once a tick would bury the
// run in "could not locate the body rig".
static void BoneVisScanWhenReady()
{
    if (g_bvScanned || !g_bvClosedOut) return;
    if (!FpPawn()) return;                    // no pawn yet, say nothing
    // Run 3: the candidate list was empty for the whole run, because BOTH of
    // FpCollect's callers sit behind the hand-mesh drive and the tester runs
    // with [Hands] Enabled=0. So the precondition could never pass, and the
    // scan waited forever for a list nobody was going to build. Build it here
    // instead: with the drive off nothing else owns these components, and
    // FpCollect's restore is a no-op when there is nothing being driven.
    if (!g_fpCandN) {
        const double t = MaimNowMs();
        if (t < g_bvNextCollectMs) return;
        g_bvNextCollectMs = t + 2000.0;
        if (++g_bvCollects > 5) {
            Log("bonevis: 5 collects and still no first-person components - "
                "the scan cannot run, so the C++-only reading stays open");
            g_bvScanned = true;
            return;
        }
        Log("bonevis: no candidate list (the hand drive is off, so nobody "
            "built one) - collecting once for the scan, attempt %d",
            g_bvCollects);
        FpCollect();
        if (!g_fpCandN) return;               // try again on the next tick
    }
    if (!ArmsPrep()) {                        // ArmsPrep logs why
        Log("bonevis: the rig is up but the arm chain did not resolve - the "
            "byte-array scan cannot run, so the C++-only reading stays open");
        g_bvScanned = true;                   // do not retry a real refusal
        return;
    }
    g_bvScanned = true;
    Log("bonevis: rig prepped for the scan - %d arm-chain bone(s) of %d",
        g_armBoneCnt, g_bvBoneTotal);
    BoneVisScanArrays();
}

// The write-survival census. Runs on the script lane beside ArmsHideTick.
static void BoneVisTick()
{
    BoneVisScanWhenReady();     // no-op once it has run, silent until the rig is up
    MatTickAll();               // VR-31 route (d): census, then the automatic A/B
    if (!g_bvOn) {
        // [Hands] BoneVisHide=1 arms it from the ini. The rig is not there on
        // the first ticks of a level, so retry - but cap the attempts, because
        // a refusal that repeats forever is noise, not evidence.
        if (!g_boneVisCfg || g_bvArmTries >= 10) return;
        const double t = MaimNowMs();
        if (t < g_bvNextTryMs) return;
        g_bvNextTryMs = t + 3000.0;
        g_bvArmTries++;
        BoneVisResolve("arm");
        if (!g_bvOffVis) {
            // A missing property does not become present on the 10th try, so
            // the arming stops here - but the SCAN half keeps looking for a
            // rig, which is the thing run 2 got wrong.
            BoneVisCloseOut();
            g_bvArmTries = 10;
            return;
        }
        if (!BoneVisOn() && g_bvArmTries >= 10)
            Log("bonevis: [Hands] BoneVisHide=1 but 10 attempts refused - "
                "giving up for this run; arms vis on retries by hand");
        return;
    }
    if (!g_bvSaveComp || !LooksLikeObj(g_bvSaveComp)) {
        Log("bonevis: the rig went away under us - dropping the write "
            "(nothing to restore, the component is gone)");
        g_bvOn = false; g_bvSaveComp = NULL;
        return;
    }
    uint8_t* d; int32_t n;
    if (!BoneVisReadArray(g_bvSaveComp, g_bvOffVis, &d, &n) ||
        n != g_bvBoneTotal) {
        Log("bonevis: the array stopped reading as %d per-bone bytes - "
            "dropping the write", g_bvBoneTotal);
        g_bvOn = false; g_bvSaveComp = NULL;
        return;
    }
    int held = 0, reverted = 0, other = 0;
    for (int i = 0; i < g_armBoneCnt; i++) {
        const int b = g_armBones[i].bone;
        if (b < 0 || b >= n) continue;
        const uint8_t now = d[b];
        if (now == kBvsHidden)          held++;
        else if (now == g_bvSave[i])    reverted++;
        else                            other++;
        if (now != kBvsHidden) { d[b] = kBvsHidden; g_bvWrites++; }
    }
    g_bvHeld = held; g_bvReverted = reverted; g_bvOther = other;

    const double nowMs = MaimNowMs();
    if (nowMs < g_bvNextLogMs) return;
    g_bvNextLogMs = nowMs + 2000.0;
    // What would move these numbers: reverted climbs if the engine rebuilds the
    // visibility array (RebuildVisibilityArray, a mesh swap, an anim notify);
    // other climbs if a third writer touches the same bytes. All held with the
    // arms still on screen is the interesting answer - the write survives and
    // the renderer does not read this array.
    Log("bonevis: census held=%d reverted=%d other=%d of %d, rewrites=%d "
        "(all held plus visible arms = the array is not what the renderer "
        "reads; reverted above 0 = the engine puts it back and the write needs "
        "a later lane)",
        held, reverted, other, g_armBoneCnt, g_bvWrites);
}


static void BoneVisStatus()
{
    BoneVisResolve("status");
    Log("bonevis: %s. BoneVisibilityStates +0x%03x, SkelControlIndex +0x%03x, "
        "RequiredBones +0x%03x, rig %s (%d arm bone(s) of %d)",
        g_bvOn ? "ON" : "off", g_bvOffVis, g_bvOffCtl, g_bvOffReq,
        g_armComp ? "prepped" : "not prepped", g_armBoneCnt, g_bvBoneTotal);
    if (g_bvOn)
        Log("bonevis: census held=%d reverted=%d other=%d, rewrites=%d",
            g_bvHeld, g_bvReverted, g_bvOther, g_bvWrites);
    if (!g_bvOffVis) BoneVisCloseOut();   // one-shot, safe to ask for again
}


// "arms vis on|off|status|chain" - the seam word. Anything unrecognised prints
// the vocabulary rather than being swallowed.
static bool ArmsCommand(const char* args)
{
    char sub[16] = "", rest[32] = "";
    const int got = sscanf(args, "%15s %31s", sub, rest);
    if (got >= 1 && !_stricmp(sub, "mat")) return MatCommand(args + 3);
    if (got >= 1 && !_stricmp(sub, "vis")) {
        if (got >= 2 && (!_stricmp(rest, "on") || !strcmp(rest, "1"))) {
            if (!BoneVisOn()) Log("bonevis: stayed off (see the refusal above)");
            return true;
        }
        if (got >= 2 && (!_stricmp(rest, "off") || !strcmp(rest, "0"))) {
            BoneVisOff("seam"); return true;
        }
        if (got >= 2 && !_stricmp(rest, "chain")) { BoneVisChain(); return true; }
        BoneVisStatus();
        return true;
    }
    Log("arms: vis on|off|status|chain | mat census|hide <id>|show <id>|restore");
    return true;
}
