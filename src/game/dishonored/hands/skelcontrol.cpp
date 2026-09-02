// game/dishonored/hands/skelcontrol.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static inline bool SkcAlive(int slot)
{
    if (slot < 0 || slot >= 8) return false;
    uint8_t* o = g_skcPlayer[slot];
    if (!o || ((uintptr_t)o & 3)) return false;
    uint32_t ix = g_skcObjIdx[slot];
    if (!ix) return false;
    void**   objs = *(void***)kGObjHdr;      // engine global, always mapped
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || ix >= num) return false;
    if ((uint8_t*)objs[ix] != o) return false;          // destroyed or recycled
    if (*(void**)(o + kClassOff) != g_skcObjCls[slot]) return false;
    return true;
}


static void SkcOffsetAudit()
{
    struct AudP { const char* cls; const char* prop; uint32_t off; uint32_t mask; };
    static const AudP kA[] = {
        { "SkelControlBase",       "ControlName",          kSkcName,      0 },
        { "SkelControlBase",       "ControlStrength",      kSkcStr,       0 },
        { "SkelControlBase",       "BoneScale",            kSkcScaleProp, 0 },
        { "SkelControlSingleBone", "BoneTranslation",      kSkcTrans,     0 },
        { "SkelControlSingleBone", "BoneTranslationSpace", kSkcTSpace,    0 },
        { "SkelControlSingleBone", "BoneRotation",         kSkcRot,       0 },
        { "SkelControlSingleBone", "BoneRotationSpace",    kSkcRSpace,    0 },
        { "SkelControlSingleBone", "bApplyTranslation",    kSkcBools, kSkcApplyTrans },
        { "SkelControlSingleBone", "bApplyRotation",       kSkcBools, kSkcApplyRot },
        { "SkelControlSingleBone", "bAddTranslation",      kSkcBools, kSkcAddTrans },
        { "SkelControlSingleBone", "bAddRotation",         kSkcBools, kSkcAddRot },
    };
    const int kN = (int)(sizeof(kA) / sizeof(kA[0]));
    uint32_t clsIdx[kN], prpIdx[kN];
    uint32_t engOff[kN], engMask[kN];
    bool     found[kN];
    for (int i = 0; i < kN; i++) {
        clsIdx[i] = FindNameIdx(kA[i].cls);
        prpIdx[i] = FindNameIdx(kA[i].prop);
        engOff[i] = 0; engMask[i] = 0; found[i] = false;
    }
    if (!RangeReadable((void*)kGObjHdr, 12)) { Log("skc/AUDIT: GObjects unreadable"); return; }
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    // ONE walk for all eleven - not eleven walks
    for (uint32_t i = 1; i < num; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = num - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        const uint32_t nm = *(uint32_t*)(o + kNameOff);
        int hit = -1;
        for (int k = 0; k < kN; k++)
            if (!found[k] && prpIdx[k] == nm) { hit = k; break; }
        if (hit < 0) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        const uint32_t onm = *(uint32_t*)(ou + kNameOff);
        for (int k = 0; k < kN; k++) {
            if (found[k] || prpIdx[k] != nm || clsIdx[k] != onm) continue;
            const char* pc = ObjClassName(o);
            if (!pc || !strstr(pc, "Property")) continue;
            engOff[k]  = *(uint32_t*)(o + 0x5c);
            engMask[k] = strstr(pc, "Bool") ? *(uint32_t*)(o + 0x6c) : 0;
            found[k] = true;
        }
    }
    Log("skc/AUDIT: ==== engine reflection vs our constants ====");
    int mismatches = 0;
    for (int k = 0; k < kN; k++) {
        if (!found[k]) {
            Log("skc/AUDIT: %-22s %-20s NOT FOUND in reflection (ours +0x%02x"
                " mask 0x%02x)", kA[k].cls, kA[k].prop, kA[k].off, kA[k].mask);
            continue;
        }
        bool offOk  = engOff[k] == kA[k].off;
        bool maskOk = kA[k].mask == 0 || engMask[k] == kA[k].mask;
        if (!offOk || !maskOk) mismatches++;
        Log("skc/AUDIT: %-22s %-20s engine +0x%02x mask 0x%02x | ours +0x%02x"
            " mask 0x%02x  %s", kA[k].cls, kA[k].prop, engOff[k], engMask[k],
            kA[k].off, kA[k].mask,
            (offOk && maskOk) ? "MATCH" : "<<< MISMATCH >>>");
    }
    Log("skc/AUDIT: ==== %d mismatch(es). Translation rows are the canaries -"
        " they work in-game, so reflection must agree with them. ====",
        mismatches);
}


static void SkelControlProbe()
{
    if (!RangeReadable((void*)kGObjHdr, 12)) { Log("skc: GObjects unreadable"); return; }
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) {
        Log("skc: GObjects looks wrong (num=%u)", num); return;
    }
    Log("skc: ==== SkelControl probe over %u objects ====", num);

    struct Cls { void* cls; const char* name; uint32_t n; uint8_t* first; };
    Cls c[48]; int cn = 0;
    uint32_t total = 0, limb = 0, single = 0, live = 0;
    // 30.91: the previous verdict counted CLASS DEFAULT OBJECTS as instances.
    // Every "Default__Foo" is a template UE3 creates for a class whether or not
    // the game ever uses it - so "1 limb control" was really "the SkelControlLimb
    // class exists", not "a limb control is instantiated". Only objects WITHOUT
    // the Default__ prefix are live, and only live ones can be driven.
    // 33.4: THIS WAS THE ARMS-AFTER-RELOAD BUG. 40 slots, filled in
    // GObjects order, everything past them silently dropped. On a fresh
    // launch the player's rig is created early and his 3 controls land
    // inside the first 40; a checkpoint reload RECREATES the rig at the END
    // of the table, past slot 40, where the ownership check never looked.
    // The controls existed and worked the whole time - the probe walked
    // straight past them on every retry. Big enough for any level now, and
    // an overflow LOGS instead of silently eating the player's hands.
    static const int kOwnMax = 256;
    static struct Own { uint8_t* obj; const char* cls; const char* nm;
                        uint32_t idx; } own[kOwnMax];
    int ownN = 0;
    int ownDropped = 0;

    for (uint32_t i = 1; i < num; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, kClassOff + 4)) continue;
        // fast path once we know the class: a pointer compare instead of a
        // name lookup and substring search, per object, 103k times
        if (g_skcClsCache) {
            if (*(void**)(o + kClassOff) != g_skcClsCache) continue;
        }
        const char* nm = ObjClassName(o);
        if (!nm || !strstr(nm, "SkelControl")) continue;
        if (!g_skcClsCache && !strcmp(nm, "SkelControlSingleBone"))
            g_skcClsCache = *(void**)(o + kClassOff);
        total++;
        const char* on = RangeReadable(o + kNameOff, 4)
                       ? RealName(*(uint32_t*)(o + kNameOff)) : "?";
        bool isCDO = (on && !strncmp(on, "Default__", 9));
        if (!isCDO) {
            live++;
            if (strstr(nm, "Limb"))       limb++;
            if (strstr(nm, "SingleBone")) single++;
            if (ownN < kOwnMax) { own[ownN].obj = o; own[ownN].cls = nm;
                                  own[ownN].nm = on; own[ownN].idx = i; ownN++; }
            else ownDropped++;
        }
        void* cls = *(void**)(o + kClassOff);
        int f = -1;
        for (int q = 0; q < cn; q++) if (c[q].cls == cls) { f = q; break; }
        if (f < 0 && cn < 48) { c[cn].cls = cls; c[cn].name = nm; c[cn].n = 0;
                                c[cn].first = o; f = cn++; }
        if (f >= 0) c[f].n++;
    }

    if (ownDropped)
        Log("skc: WARNING - %d live controls did NOT fit the %d-slot ownership "
            "buffer and were not checked. If the hands are dead, this is why.",
            ownDropped, kOwnMax);
    for (int q = 0; q < cn; q++)
        Log("skc:   %-40s x%-5u  first @%p  (name '%s')", c[q].name, c[q].n,
            (void*)c[q].first,
            RangeReadable(c[q].first + kNameOff, 4)
                ? RealName(*(uint32_t*)(c[q].first + kNameOff)) : "?");

    // WHO OWNS THEM. This is the gating question: a foot-placement control on a
    // guard is useless to us. Walk the Outer chain - if none of these live
    // controls sit under the first-person rig, the lane is closed no matter how
    // many exist.
    g_skcPlayerN = 0;
    g_skcCamIdx  = -1;                     // 32.6: rebuilt below, by name
    g_skcStale   = 0;
    for (int z = 0; z < 8; z++) { g_skcObjIdx[z] = 0; g_skcObjCls[z] = NULL;
                                  g_skcPlayer[z] = NULL; g_skcHandOf[z] = -1; }
    Log("skc: ---- ownership of the %d LIVE controls ----", ownN);
    for (int q = 0; q < ownN; q++) {
        char chain[256]; chain[0] = 0;
        uint8_t* cur = own[q].obj;
        for (int lvl = 0; lvl < 4; lvl++) {
            if (!RangeReadable(cur + kOuterOff, 4)) break;
            uint8_t* out = *(uint8_t**)(cur + kOuterOff);
            if (!out || ((uintptr_t)out & 3) || !RangeReadable(out, kClassOff + 4)) break;
            const char* onm = RangeReadable(out + kNameOff, 4)
                            ? RealName(*(uint32_t*)(out + kNameOff)) : "?";
            const char* ocl = ObjClassName(out);
            size_t used = strlen(chain);
            if (used + 80 >= sizeof(chain)) break;
            _snprintf(chain + used, sizeof(chain) - used, "%s<- %s (%s)",
                      used ? " " : "", onm ? onm : "?", ocl ? ocl : "?");
            cur = out;
        }
        // 33.3: THE NAME CHECK DIES AT CHECKPOINT RELOADS. The log shows it
        // plainly: pre-death this found 3 player controls via the string
        // "DishonoredPlayerPawn" in the ownership chain; after a checkpoint
        // restore every one of the 56 live controls prints an NPC chain and
        // the player's name appears NOWHERE - the engine rewires ownership on
        // restore, and a search for the wrong name retries forever for free.
        // Identify the player by POINTER instead: control -> Outer (the
        // AnimTree instance) -> Outer (the mesh component). If that component
        // is one of the player's own view-model components - the FpCand list,
        // which is re-collected every 750 ms and provably survives reloads -
        // the control is the player's. The name check stays as a fallback for
        // the fresh-launch case where FpCand may not be populated yet.
        bool playerByPtr = false;
        {
            uint8_t* o1 = own[q].obj;
            if (RangeReadable(o1 + kOuterOff, 4)) {
                uint8_t* tree = *(uint8_t**)(o1 + kOuterOff);
                if (tree && !((uintptr_t)tree & 3) &&
                    RangeReadable(tree + kOuterOff, 4)) {
                    uint8_t* comp = *(uint8_t**)(tree + kOuterOff);
                    for (int f2 = 0; f2 < g_fpCandN && !playerByPtr; f2++)
                        if (g_fpCand[f2].obj == comp) playerByPtr = true;
                }
            }
        }
        if ((playerByPtr || strstr(chain, "DishonoredPlayerPawn")) &&
            g_skcPlayerN < 8 &&
            !strcmp(own[q].cls, "SkelControlSingleBone")) {
            int slot = g_skcPlayerN++;
            g_skcPlayer[slot] = own[q].obj;
            // 32.6: the liveness stamp - GObjects index + class pointer
            g_skcObjIdx[slot] = own[q].idx;
            g_skcObjCls[slot] = *(void**)(own[q].obj + kClassOff);
            const char* cnm = RangeReadable(own[q].obj + kSkcName, 4)
                            ? RealName(*(uint32_t*)(own[q].obj + kSkcName)) : "?";
            g_skcHandOf[slot] = -1;
            if (cnm) {
                if (strstr(cnm, "LeftHand"))       g_skcHandOf[slot] = 0;
                else if (strstr(cnm, "RightHand")) g_skcHandOf[slot] = 1;
                else if (strstr(cnm, "Camera"))    g_skcCamIdx = slot;
            }
            Log("skc: player control %d matched by %s ('%s')", slot,
                playerByPtr ? "POINTER (component identity)" : "name chain",
                cnm ? cnm : "?");
            float st = RangeReadable(own[q].obj + kSkcStr, 4)
                     ? *(float*)(own[q].obj + kSkcStr) : -1.0f;
            float* tv = (float*)(own[q].obj + kSkcTrans);
            Log("skc:   >>> PLAYER control #%d  ControlName='%s'  strength=%.2f  "
                "trans=(%.1f,%.1f,%.1f) tspace=%u bools=%08x -> %s",
                slot, cnm ? cnm : "?", st,
                tv[0], tv[1], tv[2],
                (unsigned)*(uint8_t*)(own[q].obj + kSkcTSpace),
                *(uint32_t*)(own[q].obj + kSkcBools),
                g_skcHandOf[slot] < 0 ? "not driven" :
                (g_skcHandOf[slot] ? "RIGHT controller" : "LEFT controller"));
        }
        Log("skc:   %-32s '%s'  %s", own[q].cls, own[q].nm, chain);
    }

    Log("skc: ---- %u SkelControl objects, %u LIVE (rest are class defaults), "
        "%u classes ----", total, live, cn);
    if (!live) {
        Log("skc: VERDICT: none exist. Dishonored's AnimTrees carry no procedural");
        Log("skc:   bone controls, so there is no engine-side input to write and");
        Log("skc:   this lane is closed. Our own models stay the answer.");
    } else {
        Log("skc: VERDICT: %u LIVE controls - %u limb (IK), %u single-bone.",
            live, limb, single);
        if (limb) {
            Log("skc:   A limb control is the good case - set its effector to the");
            Log("skc:   controller and the elbow solves itself.");
        } else if (single) {
            Log("skc:   Single-bone only: usable, but the arm will not solve - we");
            Log("skc:   would drive the hand bone and the forearm follows rigidly.");
        }
        Log("skc:   NEXT: find which of these belong to the first-person AnimTree.");
    }
    // ---- 30.92: the property layout ---------------------------------------
    // Three live SkelControlSingleBone instances hang off pMesh, the player's
    // own skeletal component - the first-person rig. To drive one we need the
    // offsets of ControlStrength / BoneTranslation / BoneRotation, and guessing
    // them is exactly the sort of thing that crashed 30.41. So read them from
    // the engine's own reflection instead: every UProperty is itself a UObject
    // whose Outer is the class that declares it. Dump the candidate dwords per
    // property and the Offset column identifies itself - it ascends in
    // declaration order and stays below the class's instance size.
    {
        void* targetCls = NULL;
        for (int q = 0; q < ownN; q++) {
            if (!strcmp(own[q].cls, "SkelControlSingleBone") &&
                RangeReadable(own[q].obj, kClassOff + 4)) {
                targetCls = *(void**)(own[q].obj + kClassOff);
                break;
            }
        }
        if (!targetCls) {
            Log("skc/prop: no SkelControlSingleBone instance to read a class from");
        } else {
            Log("skc/prop: ---- properties declared on SkelControlSingleBone ----");
            // +0x40 is ElementSize (c for the FVectors, 8 for FNames, 1 for
            // bytes) and +0x44 is PropertyFlags, so Offset is further along.
            // UE3 UProperty continues: Category FName, ArraySizeEnum, RepOffset,
            // RepIndex, then Offset - dump through 0x6c and it identifies
            // itself as the column of small, distinct, ordered values.
            Log("skc/prop: name                        class            offset | "
                "+60      +64      +68      +6c      +70      +74      +78");
            int found = 0;
            for (uint32_t i = 1; i < num && found < 40; i++) {
                uint8_t* o = (uint8_t*)objs[i];
                if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
                if (*(void**)(o + kOuterOff) != targetCls) continue;
                const char* pc = ObjClassName(o);
                if (!pc || !strstr(pc, "Property")) continue;
                const char* pn = RealName(*(uint32_t*)(o + kNameOff));
                uint32_t* w = (uint32_t*)o;
                // For BoolProperty the BITMASK matters as much as the offset:
                // 30.97 assumed UE3 hands them out 1,2,4,8 in declaration order.
                // The hands still followed the head in world mode, which would
                // be impossible if we were really writing an absolute position -
                // so that assumption is now the prime suspect. Dump further and
                // read the mask instead of believing it.
                Log("skc/prop: %-27s %-21s off=%3x | %8x %8x %8x %8x %8x %8x %8x",
                    pn ? pn : "?", pc, w[0x5c/4],
                    w[0x60/4], w[0x64/4], w[0x68/4], w[0x6c/4],
                    w[0x70/4], w[0x74/4], w[0x78/4]);
                found++;
            }
            Log("skc/prop: ---- %d properties. Also walking the parent class ----", found);
            // SkelControlBase declares ControlStrength, so climb one level
            uint8_t* cls = (uint8_t*)targetCls;
            if (RangeReadable(cls + kOuterOff, 4)) {
                // UStruct::SuperField sits just past the UObject header on UE3;
                // rather than guess, list properties of every class whose name
                // starts with SkelControl - the parent shows up by name.
                for (uint32_t i = 1; i < num && found < 80; i++) {
                    uint8_t* o = (uint8_t*)objs[i];
                    if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
                    uint8_t* ou = *(uint8_t**)(o + kOuterOff);
                    if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
                    const char* on2 = RealName(*(uint32_t*)(ou + kNameOff));
                    if (!on2 || strcmp(on2, "SkelControlBase")) continue;
                    const char* pc = ObjClassName(o);
                    if (!pc || !strstr(pc, "Property")) continue;
                    const char* pn = RealName(*(uint32_t*)(o + kNameOff));
                    uint32_t* w = (uint32_t*)o;
                    Log("skc/prop: [base] %-21s %-21s %8x %8x %8x %8x %8x %8x %8x",
                        pn ? pn : "?", pc,
                        w[0x50/4], w[0x54/4], w[0x58/4], w[0x5c/4],
                        w[0x60/4], w[0x64/4], w[0x68/4]);
                    found++;
                }
            }
        }
    }
    if (!g_skcPlayerN) g_skcProbeFails++;
    else               g_skcProbeFails = 0;
    Log("skc: ==== probe done (%d player controls) ====", g_skcPlayerN);
    if (g_graftOn) {                       // 35.7: rig reloaded - the hosts
        for (int u = 0; u < 3; u++) {      // died; restore donors, drop state
            if (!g_graftHost[u]) continue;
            uint8_t* d = g_graftDonor[u];
            if (d && RangeReadable(d, 0x100)) memcpy(d, g_graftSave[u], 0x100);
            g_graftHost[u] = NULL;
            g_graftHand[u] = -1;
        }
        g_graftOn = false;
        Log("graft: auto-disengaged (rig reloaded) - donors restored");
    }
    if (g_skcPlayerN) {                    // 33.8: the audit, once per session
        static bool audited = false;
        if (!audited) { audited = true; SkcOffsetAudit(); }
        static bool grafted = false;       // 35.5: donor discovery, once
        if (!grafted) { grafted = true; GraftDiscover(); }
        // 35.8: the drive is a WISH, not a session event - if the user had it
        // on, re-graft onto the freshly discovered controls automatically.
        if (g_graftWant && !g_graftOn && g_graftDonorN) {
            GraftTestSet(true);
            if (g_graftOn) Log("graft: auto RE-ENGAGED after rig reload");
        }
    }
}


// Writes the neutral to the ini the moment it is known, rather than waiting
// for "save as defaults". A neutral that only survives if the user remembers
// to press a button is the same per-launch drift wearing a hat - and 31.9
// already cost this project an hour of tuning by trusting exactly that.
static void SkcSaveNeutral(const char* why)
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    static const char* kAx[3] = { "Right", "Up", "Fwd" };
    char v[64], k[64];
    for (int hh = 0; hh < 2; hh++)
        for (int q = 0; q < 3; q++) {
            _snprintf(k, 64, "Neutral%c%s", hh ? 'R' : 'L', kAx[q]);
            _snprintf(v, 64, "%.4f", g_skcNeutral[hh][q]);
            WritePrivateProfileStringA("Hands", k, v, ini);
        }
    WritePrivateProfileStringA("Hands", "NeutralSaved", "1", ini);
    g_skcNeutralSaved = true;
    Log("skc: hand neutral SAVED (%s)  L=(%.3f,%.3f,%.3f) R=(%.3f,%.3f,%.3f) m",
        why,
        g_skcNeutral[0][0], g_skcNeutral[0][1], g_skcNeutral[0][2],
        g_skcNeutral[1][0], g_skcNeutral[1][1], g_skcNeutral[1][2]);
}

static void ApplyHandToMeshInner()
{
    // 40.1 GATE STATE. When the hands are wrong the first question is always
    // "is the drive even reaching the rig", and until now the log could not
    // answer it: the two heartbeat counters both read 0 by design (see
    // frame_hooks.cpp), so a healthy run and a dead one looked identical.
    // These two lines make the gate itself observable.
    //   Debug: the whole gate every 3 s, for anyone reading a full trace.
    //   Info : ONLY the broken combination - drive on, zero controls found -
    //          because a line that only prints when something is wrong is the
    //          one people actually notice.
    // Cost: both sit behind the per-category level gate and a GetTickCount
    //       comparison, so a default (info) build evaluates one byte compare
    //       per call in the common case and formats nothing.
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 3000,
        "skc/gate: drive=%d slots=%d stale=%d probeFails=%d camIdx=%d "
        "handMesh=%d armsHidden=%d menu=%d/%d handSize=%.2f",
        (int)g_skcDrive, (int)g_skcPlayerN, (int)g_skcStale, (int)g_skcProbeFails,
        (int)g_skcCamIdx, (int)g_handMesh, (int)g_armsHidden,
        (int)g_menuOpen, (int)g_inMenu, g_skcHandSize);
    if (g_skcDrive && !g_skcPlayerN)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
            "skc: drive is ON but NO SkelControl slots are latched (probeFails=%d, "
            "stale=%d) - nothing is writing the rig, so hand size, pose and trims "
            "all do nothing. The arms you see are the game's own, unscaled.",
            (int)g_skcProbeFails, (int)g_skcStale);

    // 30.95: the SkelControl probe and drive run FIRST, above every early
    // return. They have nothing to do with the old hand system - they walk
    // GObjects and write AnimTree properties - and burying them below
    // "if (!g_handMesh) return", a pawn check and a candidate-list check meant
    // five separate ways for them to silently never execute. That is the same
    // failure that made the oracle's inventory vanish; not repeating it.
    // find the controls automatically - the user should not have to press a
    // probe button to get working hands
    // the AnimTree is rebuilt on level load, so the cached pointers die with
    // it - validate one of them periodically and re-probe when it stops
    // reading as a control
    // 32.6: liveness is now checked at every write site (SkcAlive), and any
    // failure raises g_skcStale immediately - within one dispatch of the GC
    // nulling the GObjects slot, not up to two seconds later. All this does is
    // notice the flag, drop the cache and schedule a re-probe.
    if (g_skcDrive && g_skcPlayerN) {
        bool ok = !g_skcStale;
        if (ok) for (int z = 0; z < g_skcPlayerN; z++)
            if (!SkcAlive(z)) { ok = false; break; }
        if (!ok) {
            Log("skc: cached controls went stale (level load?) - re-probing");
            g_skcStale = 0;
            g_skcPlayerN = 0;
            g_skcProbeFails = 0;
            g_skcCamIdx = -1;
            for (int z = 0; z < 8; z++) {
                g_skcPlayer[z] = NULL; g_skcObjIdx[z] = 0;
                g_skcObjCls[z] = NULL; g_skcHandOf[z] = -1;
            }
        }
    }
    // 33.2: THE ARMS-DIE-ON-RELOAD CHAIN, finally complete:
    //   death screen raises g_menuOpen -> the flag STICKS (measured, again) ->
    //   this gate blocks every re-probe forever; and the probes that ran
    //   DURING the death screen found no player rig, so each burned one of
    //   only 12 lifetime retries. Two fixes, one per link:
    //   - gameplay is judged by the fork's splice counter, exactly like the
    //     crouch gates (32.95) and the SBS mono switch - the menu flag is
    //     only the fallback when the fork export is absent;
    //   - the retry budget REFILLS whenever gameplay resumes: 12 was a cap
    //     for one level-load, never a lifetime allowance.
    {
        bool skcInGameplay = (!g_menuOpen && !g_inMenu);   // 41.0: no splice counter
        static bool skcWasGameplay = false;
        if (skcInGameplay && !skcWasGameplay) {
            if (g_skcProbeFails >= 12)
                Log("skc: gameplay resumed - probe retry budget refilled");
            g_skcProbeFails = 0;
        }
        skcWasGameplay = skcInGameplay;
        // 33.2: heal the stuck menu flag itself while we are here - two
        // seconds of real 3D rendering IS gameplay, whatever the flag says.
        static double gpSinceMs = 0.0;
        double tgp = MaimNowMs();
        if (skcInGameplay && (g_menuOpen || g_inMenu)) {
            if (gpSinceMs == 0.0) gpSinceMs = tgp;
            if (tgp - gpSinceMs > 2000.0) {
                g_menuOpen = false; g_inMenu = false;
                Log("menu: flag cleared - 2 s of full scene rendering while "
                    "it claimed a menu was open (death screen leaves it "
                    "stuck; this is the arms-after-reload fix)");
            }
        } else {
            gpSinceMs = 0.0;
        }
        if (g_skcDrive && !g_skcPlayerN && skcInGameplay &&
            g_skcProbeFails < 12) {
            static double nextTry = 0.0;
            double tnow = MaimNowMs();
            if (tnow >= nextTry) {
                // back off as failures mount: 4s, 8s, 12s ...
                nextTry = tnow + 4000.0 + 4000.0 * (double)g_skcProbeFails;
                g_skcReq = 1;
            }
        }
    }
    if (InterlockedExchange(&g_skcReq, 0)) SkelControlProbe();   // 30.90
    {                                                            // 32.8 blink
        LONG bq = InterlockedExchange(&g_bpReq, 0);
        if (bq == 1) BlinkProbeArm();
        else if (bq == 2) BlinkProbeReport();
    }
    BlinkProbeSample();
    BlinkLatch();          // 32.14
    BlinkHookTick();       // 32.21
    CrouchStateTick();     // 32.25
    BlinkDestTick();       // 32.26
    BlinkTraceTick();      // 32.31
    if (InterlockedExchange(&g_skcCalReq, 0)) {                  // 32.12
        g_skcCalGo = true;
        g_skcCalUntil = MaimNowMs() + 3000.0;
        memset(g_skcCalSum, 0, sizeof(g_skcCalSum));
        memset(g_skcCalN, 0, sizeof(g_skcCalN));
        Log("skc: CALIBRATING for 3 s - hold your controllers where you want "
            "your hands and keep still");
    }
    if (g_skcDrive && !g_skcStale && g_skcPlayerN) {             // 30.94/96
        if (InterlockedExchange(&g_skcRecap, 0))
            g_skcHaveNeutral[0] = g_skcHaveNeutral[1] = false;
        uint32_t mask = 0;
        if (g_skcDoTrans) mask |= kSkcApplyTrans | (g_skcAddMode ? kSkcAddTrans : 0);
        if (g_skcDoRot)   mask |= kSkcApplyRot   | (g_skcAddMode ? kSkcAddRot : 0);
        for (int q = 0; q < g_skcPlayerN; q++) {
            if (!SkcAlive(q)) { g_skcStale = 1; break; }   // 32.6
            uint8_t* o = g_skcPlayer[q];
            if (!RangeReadable(o + kSkcRSpace, 4)) continue;
            int hand = (q < 8) ? g_skcHandOf[q] : -1;
            if (hand < 0) {
                // the camera look-at: turn it DOWN to stop it orienting the
                // arms toward the view. 1 = stock, 0 = off.
                if (q == g_skcCamIdx && g_skcCamStrength < 0.999f)
                    *(float*)(o + kSkcStr) = g_skcCamStrength;
                continue;
            }
            float v[3] = { g_skcTrans[0], g_skcTrans[1], g_skcTrans[2] };
            if (g_skcLive) {
                // hand offset from the head, in head axes - the same quantity
                // the render drive used, but handed to the ENGINE this time
                int dev = g_ctrlIdx[hand];
                if (dev < 0 || dev >= 16 || !g_devPoseOk[dev] || !g_devPoseOk[0]) continue;
                float (*hc)[4] = g_devPose[dev];
                float (*hm)[4] = g_devPose[0];
                float f[3] = { -hm[0][2], -hm[1][2], -hm[2][2] };
                float fl[3] = { f[0], 0.0f, f[2] };
                if (V3Norm(fl) < 0.2f) continue;
                float* flat = fl;
                float up[3] = { 0, 1, 0 }, r[3];
                V3Cross(flat, up, r); if (V3Norm(r) < 0.2f) continue;
                float u[3]; V3Cross(r, flat, u); V3Norm(u);
                float d[3] = { hc[0][3]-hm[0][3], hc[1][3]-hm[1][3], hc[2][3]-hm[2][3] };
                float ph[3] = { V3Dot(d, r), V3Dot(d, u), V3Dot(d, flat) };
                // 32.12: an explicit calibration AVERAGES three seconds of
                // pose. A single instantaneous sample bakes in whatever
                // tracking jitter and hand tremor existed on that one frame,
                // and then every launch inherits it.
                if (g_skcCalGo) {
                    for (int q = 0; q < 3; q++) g_skcCalSum[hand][q] += ph[q];
                    g_skcCalN[hand]++;
                    if (MaimNowMs() >= g_skcCalUntil) {
                        bool ok = g_skcCalN[0] > 10 && g_skcCalN[1] > 10;
                        if (ok) {
                            for (int hh = 0; hh < 2; hh++)
                                for (int q = 0; q < 3; q++)
                                    g_skcNeutral[hh][q] =
                                        (float)(g_skcCalSum[hh][q] / (double)g_skcCalN[hh]);
                            g_skcHaveNeutral[0] = g_skcHaveNeutral[1] = true;
                            g_skcYaw0[hand] = atan2f(fl[0], -fl[2]);
                            SkcSaveNeutral("3 s calibration");
                        } else {
                            Log("skc: calibration got too few samples (L=%u R=%u)"
                                " - are both controllers awake?",
                                g_skcCalN[0], g_skcCalN[1]);
                        }
                        g_skcCalGo = false;
                    }
                    continue;                       // don't drive mid-capture
                }
                if (!g_skcHaveNeutral[hand]) {
                    g_skcYaw0[hand] = atan2f(fl[0], -fl[2]);
                    memcpy(g_skcNeutral[hand], ph, sizeof(ph));
                    g_skcHaveNeutral[hand] = true;
                    Log("skc: %s neutral = (%.3f,%.3f,%.3f) m",
                        hand ? "RIGHT" : "LEFT", ph[0], ph[1], ph[2]);
                    // persist as soon as BOTH hands have one, so this is the
                    // last launch that ever has to capture
                    if (!g_skcNeutralSaved &&
                        g_skcHaveNeutral[0] && g_skcHaveNeutral[1])
                        SkcSaveNeutral("first capture");
                }
                float dp[3] = { ph[0]-g_skcNeutral[hand][0],
                                ph[1]-g_skcNeutral[hand][1],
                                ph[2]-g_skcNeutral[hand][2] };
                // UE3 bone space: X forward, Y right, Z up
                float sc = g_skcWorld ? g_skcWorldScale : g_skcScaleUU;
                v[0] = dp[2] * sc;
                v[1] = dp[0] * sc;
                v[2] = dp[1] * sc;
                for (int k = 0; k < 3; k++) {
                    if (v[k] >  g_skcMax) v[k] =  g_skcMax;
                    if (v[k] < -g_skcMax) v[k] = -g_skcMax;
                }
            }
            uint8_t  useSpace = (uint8_t)g_skcSpace;
            uint32_t useMask  = mask;
            if (g_skcPinTest && CamStillValid() && RangeReadable(g_camObj + 0x80, 12)) {
                float* cp = (float*)(g_camObj + 0x80);
                if (!g_skcPinHave) {
                    g_skcPinAt[0] = cp[0] + 60.0f;
                    g_skcPinAt[1] = cp[1];
                    g_skcPinAt[2] = cp[2] + 30.0f;
                    g_skcPinHave = true;
                    Log("skc/pin: pinned to world (%.0f,%.0f,%.0f) - walk and turn; "
                        "if the hand stays there, space 0 IS world space",
                        g_skcPinAt[0], g_skcPinAt[1], g_skcPinAt[2]);
                }
                *(float*)(o + kSkcStr) = 1.0f;
                float* t = (float*)(o + kSkcTrans);
                t[0] = g_skcPinAt[0]; t[1] = g_skcPinAt[1]; t[2] = g_skcPinAt[2];
                *(uint8_t*)(o + kSkcTSpace) = (uint8_t)g_skcSpace;
                uint32_t* b = (uint32_t*)(o + kSkcBools);
                *b = (*b & ~(kSkcApplyTrans|kSkcAddTrans)) | kSkcApplyTrans;
                InterlockedIncrement(&g_skcHits);
                continue;
            }
            // world-space ORIENTATION from the controller, in game axes
            if (g_skcLive && g_skcWorld && g_skcWorldRot && g_skcDoRot &&
                CamStillValid() && RangeReadable(g_camObj + 0x50, 0x40)) {
                int dev2 = g_ctrlIdx[hand];
                if (dev2 >= 0 && dev2 < 16 && g_devPoseOk[dev2] && g_devPoseOk[0]) {
                    float (*hc2)[4] = g_devPose[dev2];
                    float (*hm2)[4] = g_devPose[0];
                    // controller axes relative to the roll-free head
                    float f2[3] = { -hm2[0][2], -hm2[1][2], -hm2[2][2] };
                    float fl[3] = { f2[0], 0.0f, f2[2] };
                    if (V3Norm(fl) > 0.2f) {
                        float upw[3] = { 0, 1, 0 }, rr[3];
                        V3Cross(fl, upw, rr);
                        if (V3Norm(rr) > 0.2f) {
                            float uu2[3]; V3Cross(rr, fl, uu2); V3Norm(uu2);
                            float cfw[3] = { -hc2[0][2], -hc2[1][2], -hc2[2][2] };
                            float cup[3] = {  hc2[0][1],  hc2[1][1],  hc2[2][1] };
                            float relF[3] = { V3Dot(cfw, rr), V3Dot(cfw, uu2), V3Dot(cfw, fl) };
                            float relU[3] = { V3Dot(cup, rr), V3Dot(cup, uu2), V3Dot(cup, fl) };
                            float* cf2 = (float*)(g_camObj + 0x50);
                            float* cr2 = (float*)(g_camObj + 0x60);
                            float* cu2 = (float*)(g_camObj + 0x70);
                            float wf[3], wu[3];
                            for (int k = 0; k < 3; k++) {
                                wf[k] = cf2[k]*relF[2] + cr2[k]*relF[0] + cu2[k]*relF[1];
                                wu[k] = cf2[k]*relU[2] + cr2[k]*relU[0] + cu2[k]*relU[1];
                            }
                            V3Norm(wf); V3Norm(wu);
                            float yaw   = atan2f(wf[1], wf[0]);
                            float ph2   = wf[2]; if (ph2 > 1.f) ph2 = 1.f; if (ph2 < -1.f) ph2 = -1.f;
                            float pitch = asinf(ph2);
                            // roll: the up vector's tilt about the forward axis
                            float sr[3] = { -sinf(yaw), cosf(yaw), 0.0f };
                            float roll  = atan2f(V3Dot(wu, sr), wu[2]) * g_skcRollGain;
                            const float k2u = 32768.0f / 3.14159265f;
                            g_skcWantRot[hand][0] = (int32_t)(pitch * k2u);
                            g_skcWantRot[hand][1] = (int32_t)(yaw   * k2u);
                            g_skcWantRot[hand][2] = (int32_t)(roll  * k2u);
                            g_skcWantRotOk[hand] = true;
                        }
                    }
                }
            }
            if (g_skcLive && g_skcWorld && CamStillValid() &&
                RangeReadable(g_camObj + 0x50, 0x40)) {
                // absolute world placement: camera position + the controller's
                // offset from your head, rotated by the GAME's camera basis
                float* cf = (float*)(g_camObj + 0x50);   // forward
                float* cr = (float*)(g_camObj + 0x60);   // right
                float* cu = (float*)(g_camObj + 0x70);   // up
                float* cp = (float*)(g_camObj + 0x80);   // position
                float fwd = v[0], rgt = v[1], upv = v[2];   // already uu
                float w[3];
                // 32.39: standing trim ALWAYS applies; crouching adds an
                // offset on top of it. Never a swap.
                // 34.9: blocking while STANDING adds its own offset the same
                // way (crouched block was already right, so crouch wins).
                float tr[3];
                {
                    BlockStateTick();
                    bool useC = g_skcCrouchTrimOn && g_pawnCrouched;
                    bool useB = g_skcBlockTrimOn && g_blockHeld && !g_pawnCrouched;
                    for (int k2 = 0; k2 < 3; k2++)
                        tr[k2] = g_skcTrim[hand][k2]
                               + (useC ? g_skcTrimCrouch[hand][k2] : 0.0f)
                               + (useB ? g_skcTrimBlock[hand][k2] : 0.0f);
                }
                for (int k = 0; k < 3; k++)
                    w[k] = cp[k] + cf[k]*fwd + cr[k]*rgt + cu[k]*upv
                         + cf[k]*tr[0] + cr[k]*tr[1] + cu[k]*tr[2]
                         // 38.18: hands follow the 38.15 crouch eye-drop.
                         // The VIEW sinks via the VP shear but this anchor
                         // is the camera OBJECT (un-dropped), so crouched
                         // hands floated a head above you under tables.
                         // Same smoothed variable = perfectly in step.
                         - cu[k]*g_crouchDropUU;
                // 38.27: HANDS NEVER GO THROUGH THE FLOOR. This anchor is
                // the camera OBJECT position (+0x80) - the very field the
                // 38.24 eye clamp pushes down - so when the clamp drops the
                // view to the top of a 33 uu crawl capsule (58 uu above the
                // boots), a seated player's hands, which sit a good 40-50 uu
                // below their real head, land UNDER the world. Measured as
                // "my arms are now going below the ground when crouched
                // under objects". The pawn's own capsule gives the floor for
                // free: bottom = pawnZ - CollisionHeight. Clamp the anchor to
                // just above it. Standing is untouched (hands are nowhere
                // near the boots); reaching down still reaches the ground,
                // it just stops there. [Hands] HandFloor=0 reverts.
                if (g_handFloorCfg && g_pePawn && g_actorLocFound &&
                    g_cylLast > 10.0f && (MaimNowMs() - g_cylOkMs) < 1500.0 &&
                    RangeReadable(g_pePawn + g_actorLocOff, 12)) {
                    float fz = ((const float*)(g_pePawn + g_actorLocOff))[2]
                             - g_cylLast + g_handFloorMargin;
                    if (w[2] < fz && (cp[2] - w[2]) < 400.0f) {
                        static double hfl = 0.0; double hfn = MaimNowMs();
                        if (hfn - hfl > 2000.0) { hfl = hfn;
                            Log("handfloor: hand z %.1f -> %.1f (pawnZ %.1f "
                                "cyl %.1f) - arms stay out of the ground",
                                w[2], fz,
                                ((const float*)(g_pePawn + g_actorLocOff))[2],
                                g_cylLast);
                        }
                        w[2] = fz;
                    }
                }
                if (w[0] == w[0] && w[1] == w[1] && w[2] == w[2]) {
                    v[0] = w[0]; v[1] = w[1]; v[2] = w[2];
                    useSpace = 0;                                  // BCS_WorldSpace
                    useMask  = (useMask & ~kSkcAddTrans) | kSkcApplyTrans;  // replace
                }
            }
            if (g_skcLive && !g_skcWorld && g_skcCounterYaw > 0.001f &&
                g_skcHaveNeutral[hand] && g_devPoseOk[0]) {
                float (*hm3)[4] = g_devPose[0];
                float f3[3] = { -hm3[0][2], -hm3[1][2], -hm3[2][2] };
                float fl3[3] = { f3[0], 0.0f, f3[2] };
                if (V3Norm(fl3) > 0.2f) {
                    float yawNow = atan2f(fl3[0], -fl3[2]);
                    float dy = yawNow - g_skcYaw0[hand];
                    while (dy >  3.14159265f) dy -= 6.28318531f;
                    while (dy < -3.14159265f) dy += 6.28318531f;
                    const float k2u = 32768.0f / 3.14159265f;
                    int32_t* rot = (int32_t*)(o + kSkcRot);
                    rot[0] = 0;
                    rot[1] = (int32_t)(-dy * g_skcCounterYaw * k2u);
                    rot[2] = 0;
                    *(uint8_t*)(o + kSkcRSpace) = (uint8_t)g_skcSpace;
                    uint32_t* rb2 = (uint32_t*)(o + kSkcBools);
                    *rb2 |= kSkcApplyRot | kSkcAddRot;   // ADD, so animation survives
                }
            }
            if (g_skcRotPin) {
                int32_t* rot = (int32_t*)(o + kSkcRot);
                rot[0] = 0; rot[1] = 0; rot[2] = 0;   // face world +X, level
                *(uint8_t*)(o + kSkcRSpace) = 0;      // world
                uint32_t* rb3 = (uint32_t*)(o + kSkcBools);
                *rb3 = (*rb3 & ~kSkcAddRot) | kSkcApplyRot;
            }
            // 33.7: THE ROTATION DRIVE, delivered to the lane that WINS.
            // 33.6 computed the right rotation and wrote it HERE, in the
            // per-frame loop - where the engine restamps rotators before
            // they ever render. The codebase already knew: SkcRotApply exists
            // precisely because "one write per frame would lose that race",
            // and it already has a commanded-rotation mailbox
            // (g_skcWantRot/Ok) hammered at ProcessEvent rate. The pin
            // visibly worked because the PIN branch lives in that fast lane.
            // So the drive now only COMPUTES here and mails the result; the
            // fast lane delivers it. It also forces g_skcDoRot on while the
            // drive is active - the mailbox is dead letter mail without it.
            // 35.8: the graft drive rides the same compute + mailbox - the
            // only difference is WHERE the fast lane delivers (donor vs host).
            if ((g_skcRotDrive || g_graftOn) && hand >= 0 && !g_skcRotPin) {
                g_skcDoRot = true;             // the fast lane's master gate
                int devR = g_ctrlIdx[hand];
                if (devR >= 0 && devR < 16 && g_devPoseOk[devR] && g_injSnapOk) {
                    float (*hR)[4] = g_devPose[devR];
                    float fwdR[3]  = { -hR[0][2], -hR[1][2], -hR[2][2] };
                    float downR[3] = { -hR[0][1], -hR[1][1], -hR[2][1] };
                    float aR = g_maimPitchOff * 3.14159265f / 180.0f;
                    float caR = cosf(aR), saR = sinf(aR);
                    float ray[3] = { fwdR[0]*caR + downR[0]*saR,
                                     fwdR[1]*caR + downR[1]*saR,
                                     fwdR[2]*caR + downR[2]*saR };
                    if (V3Norm(ray) > 0.5f) {
                        float rYaw = atan2f(ray[0], -ray[2]);
                        float rPitRaw = ray[1] < -1.f ? -1.f
                                      : (ray[1] > 1.f ? 1.f : ray[1]);
                        float rPit = asinf(rPitRaw);
                        // 36.0: (35.9's live-head swap measured as a no-op -
                        // the snap IS republished per frame, matched with the
                        // view rotation the injector wrote from it.) The head
                        // leak is a SPACE error: rot space 0 is TRUE world,
                        // so the command must be the controller's ABSOLUTE
                        // game-world aim, not a head-relative offset. Mapping
                        // tracking->game for any direction t is
                        //   game(t) = viewYaw + (t - hmdYaw)*flipYaw
                        // (same formula the injector applies to the HMD), and
                        // every term below comes from one matched pair.
                        float yawC = rYaw - g_injHmdYawSnap;
                        while (yawC >  3.14159265f) yawC -= 6.2831853f;
                        while (yawC < -3.14159265f) yawC += 6.2831853f;
                        float pitC = rPit - g_injHmdPitchSnap;
                        if (g_skcRotNeuOk[hand]) {
                            yawC -= g_skcRotNeu[hand][0];
                            pitC -= g_skcRotNeu[hand][1];
                        }
                        float yawCmd, pitCmd;
                        if (g_graftAimAbs && g_graftOn) {
                            // 36.4: ABSOLUTE aim - controller raw minus its
                            // captured neutral. 36.5: minus the parent
                            // chain's partial head-follow, coefficient set
                            // by the user's slider (turn head, drag until
                            // the weapon holds still).
                            float ya = rYaw - (g_skcRotNeuOk[hand]
                                               ? g_skcRotNeuAbs[hand][0] : 0.f);
                            float pa = rPit - (g_skcRotNeuOk[hand]
                                               ? g_skcRotNeuAbs[hand][1] : 0.f);
                            if (g_skcRotNeuOk[hand]) {
                                float dhy = g_injHmdYawSnap
                                          - g_skcRotNeuHmd[hand][0];
                                while (dhy >  3.14159265f) dhy -= 6.2831853f;
                                while (dhy < -3.14159265f) dhy += 6.2831853f;
                                ya -= g_graftHCY * dhy;
                                pa -= g_graftHCP * (g_injHmdPitchSnap
                                                  - g_skcRotNeuHmd[hand][1]);
                            }
                            while (ya >  3.14159265f) ya -= 6.2831853f;
                            while (ya < -3.14159265f) ya += 6.2831853f;
                            yawCmd = ya * (float)g_skcRotSignY;
                            pitCmd = pa * (float)g_skcRotSignP;
                        } else {
                            yawCmd = yawC * (float)g_skcRotSignY;
                            pitCmd = pitC * (float)g_skcRotSignP;
                            if (g_graftHeadComp && g_graftOn &&
                                g_graftRotSpace == 0) {
                                yawCmd = g_viewYawRad + yawC * (float)g_flipYaw
                                                      * (float)g_skcRotSignY;
                                pitCmd = g_viewPitchRad + pitC * (float)g_flipPitch
                                                        * (float)g_skcRotSignP;
                            }
                        }
                        const float k2u = 10430.378f;
                        g_skcWantRot[hand][0] = (int32_t)(pitCmd * k2u);
                        g_skcWantRot[hand][1] = (int32_t)(yawCmd * k2u);
                        g_skcWantRot[hand][2] = 0;
                        g_skcWantRotOk[hand]  = true;
                        // 36.3: TELEMETRY. Three head-leak theories in a row
                        // were wrong (live-head 35.9, world-compose 36.0,
                        // bone-space 36.x test) - so stop theorizing and log
                        // the whole chain once a second: what we command,
                        // what frame terms it was built from, and what is
                        // ACTUALLY in the donor after the engine's tick.
                        static double rotTell = 0.0;
                        double nowT = MaimNowMs();
                        if (g_graftOn && nowT >= rotTell) {
                            rotTell = nowT + 1000.0;
                            uint8_t* dn = NULL; int du = -1;
                            for (int u2 = 0; u2 < 3; u2++)
                                if (g_graftHand[u2] == hand) {
                                    dn = g_graftDonor[u2]; du = u2; break; }
                            int32_t dr[3] = { 0, 0, 0 };
                            float dstr = -1.0f; int dspc = -1, link = -1;
                            if (dn && RangeReadable(dn, 0x100)) {
                                memcpy(dr, dn + kSkcRot, 12);
                                dstr = *(float*)(dn + g_graftOffStr);
                                dspc = *(uint8_t*)(dn + kSkcRSpace);
                                link = (du >= 0 && g_graftHost[du] &&
                                        RangeReadable(g_graftHost[du], 0x100) &&
                                        *(uint8_t**)(g_graftHost[du] +
                                            g_graftOffNext) == dn) ? 1 : 0;
                            }
                            Log("rotT: h%d spc%d cmp%d abs%d hcY %.2f hcP %.2f "
                                "sY%d sP%d fY%d | "
                                "hmd %.1f view %.1f ctl %.1f yawC %.1f "
                                "cmd %.1f | donor rot %ld,%ld,%ld str %.2f "
                                "spc %d link %d",
                                hand, g_graftRotSpace, (int)g_graftHeadComp,
                                (int)g_graftAimAbs, g_graftHCY, g_graftHCP,
                                g_skcRotSignY, g_skcRotSignP, g_flipYaw,
                                g_injHmdYawSnap * 57.2958f,
                                g_viewYawRad * 57.2958f, rYaw * 57.2958f,
                                yawC * 57.2958f, yawCmd * 57.2958f,
                                (long)dr[0], (long)dr[1], (long)dr[2],
                                dstr, dspc, link);
                        }
                    }
                }
            } else if (!g_skcRotDrive && !g_graftOn) {
                // drive off: release the mailbox so other systems can use it
                static bool relTold = false;
                if (g_skcWantRotOk[hand]) g_skcWantRotOk[hand] = false;
                (void)relTold;
            }
            *(float*)(o + kSkcStr) = g_skcStrength * g_skcHandCtlStr[hand];
            if (g_skcHandSize > 0.05f && g_skcHandSize < 4.0f)
                *(float*)(o + kSkcScaleProp) = g_skcHandSize;
            float* t = (float*)(o + kSkcTrans);
            t[0] = v[0]; t[1] = v[1]; t[2] = v[2];
            *(uint8_t*)(o + kSkcTSpace) = useSpace;
            uint32_t* b = (uint32_t*)(o + kSkcBools);
            *b = (*b & ~(kSkcApplyTrans|kSkcAddTrans)) | useMask;
            InterlockedIncrement(&g_skcHits);
        }
    }

    if (!g_handMesh || g_armsHidden) return;   // 38.31: fp-MESH drive only

    static uint32_t lastFrame = 0xffffffffu;
    if (g_frame == lastFrame) return;               // once per rendered frame
    lastFrame = g_frame;

    uint8_t* pawn = FpPawn();
    if (!pawn) { FpWhy("no pawn latched yet"); return; }
    double now = MaimNowMs();
    if (now > g_fpCollectMs || pawn != g_fpLastPawn) {
        g_fpCollectMs = now + 750.0;     // weapons swap - so must the list
        FpCollect();
    }
    if (g_fpSel < 0 || g_fpSel >= g_fpCandN) return;

    uint8_t* mesh = g_fpCand[g_fpSel].obj;
    if (!LooksLikeObj(mesh) || !FpFieldsLookRight(mesh)) {
        g_fpCandN = 0; g_fpSel = -1; g_fpWritten = NULL;   // stale - rescan
        return;
    }

    if (g_fpPivotPend > 0) { g_fpPivotPend = 0; g_fpCalPhase = 1; g_fpCalFrame = 0; }
    if (g_fpCalPhase) { FpCalibrateTick(); return; }

    // ground-truth self-test owns the meshes while it runs; also hold off
    // re-collection (it would zero our commanded rotations mid-measurement)
    if (g_gtActive) { g_fpCollectMs = now + 2000.0; GtTick(); return; }

    // 30.63: while the bone-bank test runs, hold the OLD component drive off.
    // It writes the component's own Rotation/Translation, so leaving it on
    // would move the weapon for a second reason and make "did the tilt happen"
    // unreadable. Collection above keeps running, which is what the bone test
    // needs (turning tracking off with HOME would starve it of candidates).
    if (g_boneWigGo) return;

    // 30.70: the render-time drive owns the rigs now. Leaving the old
    // component drive on would move the same weapon for a second reason, from
    // a head-coupled source - exactly the drift we are here to kill. Put the
    // components back to (0,0,0) once, then stand down. Collection above keeps
    // running, so melee swings, the reticle and per-weapon identification are
    // all untouched.
    HmPickModels();       // 30.78: equipped weapon -> which model is in your hand

    // 30.97: BOTH newer drives suppress the legacy component drive. Retiring
    // the render-time one would otherwise have woken this back up to fight the
    // SkelControl drive - it writes component rotation/translation every frame
    // and would drag the same arms around for a second reason.
    static bool stoodDown = false;
    if (g_rtdEnable || g_skcDrive) {
        if (!stoodDown) {
            FpRestoreRotation();
            stoodDown = true;
            Log("handmesh: legacy component drive stood down - %s has the hands",
                g_skcDrive ? "the SkelControl drive" : "the render-time drive");
        }
        return;
    }
    if (stoodDown) {                       // overlay flipped back to the old drive
        stoodDown = false;
        Log("handmesh: legacy component drive resumed");
    }

    int driven = 0;
    for (int i = 0; i < g_fpCandN; i++) {
        if (!FpIsViewModel(&g_fpCand[i])) continue;
        if (FpDrive(i, FpHandFor(&g_fpCand[i]))) driven++;
    }
    if (!driven) {
        // 30.10: the old fallback drove g_fpSel here - and with the weapons
        // HOLSTERED there are no view models, so g_fpSel is candidate 0,
        // which is pMesh, which carries the camera. That is exactly the
        // "holstered weapons let the controllers steer my view" bug. No view
        // models means there is nothing of ours to drive: do nothing.
        FpWhy("no view models to drive (weapons holstered?)");
        return;
    }
    g_fpWrites++;
    (void)mesh;

    static int hb = 0;
    if (++hb >= 120) {
        hb = 0;
        float lyr, lpa, lp[3];
        float dF = 0.0f;
        if (HandAnglesPos(0, &lyr, &lpa, lp) && g_fpHaveNeutral[0])
            dF = (lp[2] - g_fpNeutral[0][2]) * g_fpPosScale;
        float pawnYaw = 0.0f; bool hp = FpPawnYaw(&pawnYaw);
        Log("handmesh: driving %d  writes=%ld Lyaw%+d Lpitch%+d sign%+d depth=%d "
            "Lpush=%.1fuu | view=%.0f pawn=%.0f%s deg", driven, g_fpWrites,
            g_fpFlipYaw[0], g_fpFlipPitch[0], g_fpPosSign, (int)g_fpPosOn, dF,
            g_viewYawRad*57.2958f, pawnYaw*57.2958f, hp ? "" : "(none)");
        for (int i = 0; i < g_fpCandN; i++)
            if (FpIsViewModel(&g_fpCand[i]) && g_fpCand[i].havePivot) {
                const float* T = (const float*)(g_fpCand[i].obj + kMeshTrans);
                Log("handmesh:    '%s' hand=%s pivot(%.0f,%.0f,%.0f) T(%.0f,%.0f,%.0f)",
                    g_fpCand[i].asset, FpHandFor(&g_fpCand[i]) ? "R" : "L",
                    g_fpCand[i].pivot[0], g_fpCand[i].pivot[1], g_fpCand[i].pivot[2],
                    T[0], T[1], T[2]);
            }
    }
}


static void AutoHandStartTick()
{
    if (g_autoHandDone || !g_autoHand || g_handMesh) return;
    bool ok = g_peInstalled && FpPawn() != NULL;
    if (ok) {
        float pr[3];
        ok = HandRoomPos(0, pr) && HandRoomPos(1, pr);
    }
    double now = MaimNowMs();
    if (!ok) { g_autoHandArmMs = 0.0; return; }
    if (g_autoHandArmMs == 0.0) { g_autoHandArmMs = now + g_autoHandDelay * 1000.0; return; }
    if (now < g_autoHandArmMs) return;

    g_autoHandDone = true;
    g_handMesh = true;
    // 30.33: weapon tracking auto-started = we are definitively in gameplay.
    // Close the menu flag here - loading into a level does not always fire a
    // menu-close script event, which left SBS presentation stuck on mono
    // until the player round-tripped the pause menu.
    if (g_menuOpen) { g_menuOpen = false; Log("menu: closed (auto-start)"); }
    g_fpHaveBase = false;
    FpCaptureNeutral("auto-start");
    g_fpWrites = 0; g_fpRestores = 0;
    MaimHaptic(g_maimHand, 0.8f, 0.12f);
    Log("handmesh: AUTO-START - weapon tracking is on. If the pose feels off,");
    Log("handmesh: hold the controllers where you want neutral and press END.");
}


// The fault boundary. Everything the hand-mesh system touches - the collect
// walk, calibration, driving - runs behind setjmp with the vectored handler
// armed. If any object dies under us mid-frame, we land back here, abandon
// the frame, drop every cached pointer and let the next collect rebuild.
static void ApplyHandToMesh()
{
    AutoHandStartTick();
    // 38.30: ArmsHideTick MUST run above every early return. In 38.29 it sat
    // inside ApplyHandToMeshInner, below the g_armsHidden skip - so the first
    // hide killed the only thing that could ever un-hide. The arms went away
    // on the first crouch and never came back, exactly as reported.
    ArmsHideTick();
    if (!g_handMesh) return;
    // 38.19: crawl tuck - the automated version of the HOME test that the
    // user measured as flawless. Skipping the whole apply is exactly what
    // HOME does; the engine's own recompute restores the animation pose
    // within a frame, and we resume the moment the crawl ends.
    {
        static bool tucked = false;
        bool t = CrawlTuckNow();
        if (t != tucked) {
            tucked = t;
            // 38.20: a tuck must RELEASE the arms, not freeze them - 38.19
            // skipped the writes but left the bone controls latched at the
            // last world position ("hands stayed in the world", and a
            // world-pinned arm is itself a crawl blocker - the boat). Entry
            // = the HOME-off cleanup: zero the weapon-mesh writes and drop
            // our hand controls' strength so the game's animation takes the
            // bones back. Exit restores strength (measured 1.0 default).
            if (t) { FpRestoreRotation(); g_fpHaveBase = false; }
            if (g_graftOffStr) {
                float sv = t ? 0.0f : 1.0f;
                for (int i2 = 0; i2 < g_skcPlayerN && i2 < 8; i2++) {
                    uint8_t* c2 = g_skcPlayer[i2];
                    if (!c2 || ((uintptr_t)c2 & 3) ||
                        !RangeReadable(c2, g_graftOffStr + 4)) continue;
                    *(float*)(c2 + g_graftOffStr) = sv;
                    if (g_graftOffSTgt && RangeReadable(c2, g_graftOffSTgt + 4))
                        *(float*)(c2 + g_graftOffSTgt) = sv;
                }
            }
            Log("hands: %s (cylinder %.1f)",
                t ? "TUCKED while crouched" : "back", g_cylLast);
        }
        if (t) return;
    }
    static bool vehOnce = false;
    if (!vehOnce) { vehOnce = true; AddVectoredExceptionHandler(1, WalkVEH); }

    g_walkTid = GetCurrentThreadId();
    if (setjmp(g_walkJmp)) {
        g_walkTid = 0;
        g_fpCandN = 0; g_fpSel = -1; g_fpWritten = NULL; g_fpWritten2 = NULL;
        g_fpRef = NULL; g_fpHaveRef = false;
        g_fpCalPhase = 0; g_fpPivotPend = 0;
        g_fpCollectMs = MaimNowMs() + 1500.0;
        if (g_gtActive) { g_gtActive = false; Log("gt: ==== aborted - an object died mid-test ===="); }
        Log("handmesh: recovered from a dying object (fault #%ld) - rescanning",
            (long)g_walkFaults);
        Log("handmesh: fault eip=%p touching=%p anchor(WalkVEH)=%p",
            (void*)g_walkFaultEip, (void*)g_walkFaultAddr, (void*)&WalkVEH);
        return;
    }
    DbgProbeTick();
    CamSeamTick();     // 30.40: camera-seam recon (read-only, game thread)
    // 30.62: drive the wiggle phases (game thread; the writes themselves ride
    // the high-frequency script dispatch in BoneWigApply)
    if (g_boneWigGo) {
        double now = MaimNowMs();
        if (g_bwPhase < 0) {
            BoneWigBuildMask();              // 30.64: resolve names once
            g_bwPhase = 0; g_bwNext = now + 3000.0;
            Log("bonewig: window 1 - bank +0x208 (SpaceBases?) NOW");
        } else if (now >= g_bwNext) {
            g_bwPhase++;
            g_bwNext = now + (g_bwPhase == 1 ? 2000.0 : 3000.0);
            if (g_bwPhase == 2) Log("bonewig: window 2 - bank +0x214 (LocalAtoms?) NOW");
            if (g_bwPhase >= 3) {
                Log("bonewig: done");
                g_boneWigGo = false; g_bwPhase = -1;
            }
        }
    }
    if (g_boneRtGo) {
        double bnow = MaimNowMs();
        if (g_brtIdx < 0) {
            g_brtIdx = 0; g_brtTarget = kBrtSizes[0]; g_brtHits = 0;
            g_brtUntil = bnow + 2000.0;
            Log("bonert: sweep window 1/%d - offsetting c6 x%u", kBrtSizeN, g_brtTarget);
        } else if (bnow > g_brtUntil) {
            Log("bonert:   window %d (c6 x%u) done, %ld uploads offset",
                g_brtIdx + 1, g_brtTarget, (long)g_brtHits);
            g_brtIdx++;
            if (g_brtIdx >= kBrtSizeN) {
                g_boneRtGo = false; g_brtIdx = -1; g_brtTarget = 0;
                Log("bonert: sweep complete");
            } else {
                g_brtTarget = kBrtSizes[g_brtIdx]; g_brtHits = 0;
                g_brtUntil = bnow + 2000.0;
                Log("bonert: sweep window %d/%d - offsetting c6 x%u",
                    g_brtIdx + 1, kBrtSizeN, g_brtTarget);
            }
        }
    }
    // 32.20: CameraTraceTick was DEFINED AND NEVER CALLED - dead code left
    // behind when the camera hunt finished. I built the Blink-aim trace on top
    // of it and told the user to go run it, and the log shows exactly what
    // that produced: "trace: Blink-aim trace requested" and then silence - no
    // ARMED line, no error line, because the function holding both of them
    // never executed. Check the call site exists, not just the function.
    CameraTraceTick();
    FocusGuardTick();  // 30.54: keep input alive when SteamVR steals focus
    CamPovProbe();     // 30.43: camera POV block scan (read-only)
    CamPovWiggle();    // 30.45: which POV block does the renderer obey
    CamFovWiggle();    // 30.46: is +0x138 the live DefaultFOV input
    CamFovHunt();      // 30.48: find the field that tracks the rendered FOV
    ApplyHandToMeshInner();
    g_walkTid = 0;
}


// Cheap enough for the ProcessEvent hot path: three objects, a few stores, no
// logging, no allocation, no game reads.
static inline void SkcRotApply()
{
    if (!g_skcDrive || g_skcStale) return;
    // strength gets the high-frequency treatment as well - if the game drives
    // these look-ats it may restamp their strength too, and one write per
    // frame would lose that race the same way the rotators did.
    // 32.6: this was the single most dangerous store in the DLL - it fires on
    // every ProcessEvent dispatch (CameraLookAtStrength is 0.00 in the user's
    // config, so it is always live) with nothing but a null check in front of
    // it. Through a level load that is a write into a freed UObject a few
    // thousand times a second.
    if (g_skcCamIdx >= 0 && g_skcCamIdx < g_skcPlayerN && g_skcCamStrength < 0.999f) {
        if (!SkcAlive(g_skcCamIdx)) { g_skcStale = 1; GraftEmergencyRestore(); return; }
        *(float*)(g_skcPlayer[g_skcCamIdx] + kSkcStr) = g_skcCamStrength;
    }
    if (!g_skcDoRot) return;
    for (int q = 0; q < g_skcPlayerN; q++) {
        if (!SkcAlive(q)) { g_skcStale = 1; GraftEmergencyRestore(); return; }
        uint8_t* o = g_skcPlayer[q];
        int hand = (q < 8) ? g_skcHandOf[q] : -1;
        if (hand < 0) continue;
        if (g_skcRotPin) {
            int32_t* rot = (int32_t*)(o + kSkcRot);
            rot[0] = 0; rot[1] = 0; rot[2] = 0;
            *(uint8_t*)(o + kSkcRSpace) = 0;
        } else {
            if (!g_skcWantRotOk[hand]) continue;
            // 35.8: with the graft engaged, deliver to the DONOR - the tail
            // control the tree cannot eat (proven 35.7). The host's own
            // rotation stays stock; without a graft, old behavior verbatim.
            uint8_t* tgt = o;
            uint8_t  spc = 0;                      // world (33.7 convention)
            if (g_graftOn) {
                int u;
                for (u = 0; u < 3; u++)
                    if (g_graftHost[u] == o) break;
                if (u < 3) {
                    if (!GraftDonorAlive(u) ||
                        *(uint8_t**)(o + g_graftOffNext) != g_graftDonor[u]) {
                        // donor died or the engine rewrote the link: stand
                        // down cleanly, the next probe re-engages the wish
                        GraftEmergencyRestore();
                        continue;
                    }
                    tgt = g_graftDonor[u];
                    spc = (uint8_t)g_graftRotSpace;
                    // restamp the strength family every pass - one write per
                    // frame loses races here, that lesson is carved in stone
                    *(float*)(tgt + g_graftOffStr)  = 1.0f;
                    *(float*)(tgt + g_graftOffSTgt) = 1.0f;
                    *(float*)(tgt + g_graftOffBTG)  = 0.0f;
                    // 36.5: the donor carries its own BoneScale and was
                    // stomping the hand-size slider (template default 1.0
                    // wins over the host's value). It carries the user's
                    // size now.
                    if (g_skcHandSize > 0.05f && g_skcHandSize < 4.0f)
                        *(float*)(tgt + kSkcScaleProp) = g_skcHandSize;
                }
            }
            int32_t* rot = (int32_t*)(tgt + kSkcRot);
            rot[0] = g_skcWantRot[hand][0];
            rot[1] = g_skcWantRot[hand][1];
            rot[2] = g_skcWantRot[hand][2];
            *(uint8_t*)(tgt + kSkcRSpace) = spc;
            uint32_t* b2 = (uint32_t*)(tgt + kSkcBools);
            *b2 = (*b2 & ~kSkcAddRot) | kSkcApplyRot;
            if (g_skcRemoveMeshRot) *b2 |= 0x10; else *b2 &= ~0x10u;
            InterlockedIncrement(&g_skcRotWrites);
            continue;
        }
        uint32_t* b = (uint32_t*)(o + kSkcBools);
        *b = (*b & ~kSkcAddRot) | kSkcApplyRot;
        if (g_skcRemoveMeshRot) *b |= 0x10; else *b &= ~0x10u;
        InterlockedIncrement(&g_skcRotWrites);
    }
}
