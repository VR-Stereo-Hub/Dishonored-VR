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
    g_armBoneCnt = 0; g_armComp = NULL;
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
                if (nnum != 0)
                    Log("arms: note - raw dword after '%s' index was %u",
                        nm, nnum);
                snprintf(b->nm, sizeof(b->nm), "%s", nm);
            }
        }
        if (!g_armBoneCnt) continue;
        g_armComp = k->obj;
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
