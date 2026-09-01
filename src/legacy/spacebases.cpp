// legacy/spacebases.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static bool NameHas(const char* s, const char* frag)
{
    if (!s || !frag) return false;
    for (const char* p = s; *p; p++) {
        const char *a = p, *b = frag;
        while (*a && *b) {
            char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
            char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
            if (ca != cb) break;
            a++; b++;
        }
        if (!*b) return true;
    }
    return false;
}


static void BoneWigBuildMask()
{
    g_bwObjN = 0;
    for (int i = 0; i < g_fpCandN && g_bwObjN < 8; i++) {
        FpCand* k = &g_fpCand[i];
        if (!LooksLikeObj(k->obj)) continue;
        uint8_t* a = FpAssetObj(k->obj);
        if (!a) continue;
        uint8_t* rd = NULL; int rn = 0; uint32_t rst = 0, rno = 0;
        if (!FindRefSkel(a, &rd, &rn, &rst, &rno)) continue;
        if (rn < 2 || rn > 192) continue;
        int slot = g_bwObjN++;
        g_bwObj[slot] = k->obj; g_bwBoneN[slot] = rn;
        bool isBody = NameHas(k->asset, "Skm_Player");
        int sel = 0;
        char shown[180]; shown[0] = 0;
        for (int b = 0; b < rn; b++) {
            const char* nm = RealName(*(uint32_t*)(rd + (size_t)b * rst + rno));
            bool want = !isBody;                       // weapon prop: every bone
            if (isBody && nm)
                want = NameHas(nm, "hand") || NameHas(nm, "arm") ||
                       NameHas(nm, "finger") || NameHas(nm, "thumb") ||
                       NameHas(nm, "wpn") || NameHas(nm, "weapon") ||
                       NameHas(nm, "wrist") || NameHas(nm, "grip");
            g_bwSel[slot][b] = want ? 1 : 0;
            if (want) {
                sel++;
                if (strlen(shown) < 140 && nm) {
                    strcat(shown, nm); strcat(shown, " ");
                }
            }
        }
        Log("bonewig: '%s' %d bones, %d selected%s%s", k->asset, rn, sel,
            sel ? ": " : "", shown);
    }
}


static void SbApply(const char* where)
{
    if (!g_sbGo || !g_sbOnPhase) return;
    int banks = 0;
    bool inv = false;
    for (int i = 0; i < g_fpCandN; i++) {
        // 30.86: was gated on FpIsViewModel, which matched exactly ONE
        // component - so we were writing one rig and watching another's
        // upload. Write every skeletal candidate and report its bone count, so
        // the bank can be matched to the c6 size it should be driving.
        uint8_t* obj = g_fpCand[i].obj;
        if (!obj || !RangeReadable(obj + g_sbBank, 12)) continue;
        uint8_t* d = *(uint8_t**)(obj + g_sbBank);
        int32_t num = *(int32_t*)(obj + g_sbBank + 4);
        if (!d || ((uintptr_t)d & 3) || num < 2 || num > 512) continue;
        if (!RangeReadable(d, (size_t)num * 0x20)) continue;
        if (inv)
            Log("sbo/inv: cand %d '%s' (%s) bank +0x%03x num=%d  -> would drive c6 x%d",
                i, g_fpCand[i].asset, g_fpCand[i].name, (unsigned)g_sbBank,
                (int)num, (int)num * 3);
        for (int b = 0; b < num; b++) {
            float* q = (float*)(d + (size_t)b * 0x20);
            float n = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
            if (!(n > 0.5f && n < 2.0f)) continue;
            float* t = q + 4;
            if (t[0] != t[0]) continue;
            t[0] += g_sbOff + (float)i * 25.0f;   // per-candidate signature
        }
        // read straight back: did the write actually take?
        float* q0 = (float*)d;
        g_sbWrote[0] = q0[4]; g_sbWrote[1] = q0[5]; g_sbWrote[2] = q0[6];
        banks++;
        InterlockedIncrement(&g_sbWrites);
    }
    g_sbBanks = banks;
    (void)where;
}


static void SbTick()
{
    if (!g_sbGo) return;
    double now = MaimNowMs();
    if (InterlockedExchange(&g_sbInv, 0) != 0) {
        for (int q = 0; q < 86; q++) { g_sbMin[q] = 1e9f; g_sbMax[q] = -1e9f; g_sbSeen[q] = 0; }
        Log("sbo/inv: ---- skeletal components the drive can see ----");
        for (int i = 0; i < g_fpCandN; i++) {
            uint8_t* obj = g_fpCand[i].obj;
            int n208 = -1, n214 = -1;
            if (obj && RangeReadable(obj + 0x208, 12)) n208 = *(int32_t*)(obj + 0x208 + 4);
            if (obj && RangeReadable(obj + 0x214, 12)) n214 = *(int32_t*)(obj + 0x214 + 4);
            Log("sbo/inv: [%d] '%s' (%s)  writes +%.0f uu  |  +0x208 num=%d (c6 x%d)"
                "  +0x214 num=%d (c6 x%d)",
                i, g_fpCand[i].asset, g_fpCand[i].name, g_sbOff + (float)i * 25.0f,
                n208, n208 > 0 ? n208 * 3 : 0, n214, n214 > 0 ? n214 * 3 : 0);
        }
        Log("sbo/inv: ---- %d candidates; candidate i is written +%.0f uu, so a "
            "range of that size names it ----", g_fpCandN, g_sbOff);
        // hunt every bone-sized array on the FIRST component that has one
        g_sbHuntN = 0;
        for (int i = 0; i < g_fpCandN && g_sbHuntN == 0; i++) {
            uint8_t* obj = g_fpCand[i].obj;
            if (!obj) continue;
            int want = 0;
            if (RangeReadable(obj + 0x208, 12)) want = *(int32_t*)(obj + 0x208 + 4);
            if (want < 8 || want > 200) continue;
            for (int off = 0x80; off <= 0x800 && g_sbHuntN < 24; off += 4) {
                if (!RangeReadable(obj + off, 12)) continue;
                uint8_t* d = *(uint8_t**)(obj + off);
                int32_t num = *(int32_t*)(obj + off + 4);
                int32_t mx  = *(int32_t*)(obj + off + 8);
                if (num != want || mx < num) continue;
                if (!d || ((uintptr_t)d & 3) || !RangeReadable(d, (size_t)num * 0x20)) continue;
                int good = 0;
                for (int b = 0; b < 4 && b < num; b++) {
                    float* q = (float*)(d + (size_t)b * 0x20);
                    float n = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
                    if (n > 0.8f && n < 1.25f) good++;
                }
                if (good < 3) continue;
                g_sbHunt[g_sbHuntN++] = off;
                Log("sbo/hunt: '%s' has a %d-bone atom array at +0x%03x",
                    g_fpCand[i].asset, (int)num, (unsigned)off);
            }
            if (g_sbHuntN)
                Log("sbo/hunt: %d candidate banks on '%s' - sweeping them 1 s each",
                    g_sbHuntN, g_fpCand[i].asset);
        }
        if (g_sbHuntN) { g_sbSweep = true; g_sbSweepIdx = -1; g_sbSweepNext = 0.0; }
    }
    if (g_sbSweep) {
        if (now >= g_sbSweepNext) {
            if (g_sbSweepIdx >= 0) {
                int moved = 0;
                for (int q = 1; q < 86; q++) {
                    if (!g_sbSeen[q]) continue;
                    float rng = g_sbMax[q] - g_sbMin[q];
                    if (rng > 10.0f) {
                        Log("sbo/sweep: bank +0x%03x -> c6 x%d MOVED (range %.2f)",
                            (unsigned)g_sbHunt[g_sbSweepIdx], q * 3, rng);
                        moved++;
                    }
                }
                if (!moved)
                    Log("sbo/sweep: bank +0x%03x -> nothing moved",
                        (unsigned)g_sbHunt[g_sbSweepIdx]);
            }
            g_sbSweepIdx++;
            if (g_sbSweepIdx >= g_sbHuntN) {
                g_sbSweep = false;
                g_sbGo = false;
                Log("sbo/sweep: ==== every bank on the component tried ====");
                return;
            }
            g_sbBank = (uint32_t)g_sbHunt[g_sbSweepIdx];
            for (int q = 0; q < 86; q++) { g_sbMin[q] = 1e9f; g_sbMax[q] = -1e9f; g_sbSeen[q] = 0; }
            g_sbSweepNext = now + 1000.0;
            g_sbUntil = now + 2000.0;         // keep the run alive while sweeping
            Log("sbo/sweep: now writing bank +0x%03x", (unsigned)g_sbBank);
        }
        g_sbOnPhase = true;                    // sweep writes continuously
        SbApply("sweep");
        (void)g_sbSweepBase;
        return;
    }
    if (now >= g_sbFlip) {                       // A/B: 500 ms written, 500 ms not
        g_sbFlip = now + 500.0;
        g_sbOnPhase = !g_sbOnPhase;
    }
    if (g_sbWritePoint == 1) SbApply("draw");

    // read the bank back a second time, from OUTSIDE the write, to see whether
    // the engine has restamped it since
    for (int i = 0; i < g_fpCandN; i++) {
        if (!FpIsViewModel(&g_fpCand[i])) continue;
        uint8_t* obj = g_fpCand[i].obj;
        if (!obj || !RangeReadable(obj + g_sbBank, 12)) continue;
        uint8_t* d = *(uint8_t**)(obj + g_sbBank);
        if (!d || ((uintptr_t)d & 3) || !RangeReadable(d, 0x20)) continue;
        float* q0 = (float*)d;
        g_sbRead[0] = q0[4]; g_sbRead[1] = q0[5]; g_sbRead[2] = q0[6];
        break;
    }

    if (now >= g_sbNext) {
        g_sbNext = now + 250.0;
        Log("sbo: %s bank=+0x%03x banks=%ld writes=%ld | wrote=(%.2f,%.2f,%.2f) "
            "readback=(%.2f,%.2f,%.2f) | arms c6=(%.2f,%.2f,%.2f) wpn c6=(%.2f,%.2f,%.2f)",
            g_sbOnPhase ? "WRITING " : "resting ", (unsigned)g_sbBank,
            (long)g_sbBanks, (long)g_sbWrites,
            g_sbWrote[0], g_sbWrote[1], g_sbWrote[2],
            g_sbRead[0], g_sbRead[1], g_sbRead[2],
            g_sbC6[0], g_sbC6[1], g_sbC6[2], g_sbC6w[0], g_sbC6w[1], g_sbC6w[2]);
    }
    if (now >= g_sbUntil) {
        g_sbGo = false;
        Log("sbo: ==== done. Compare the WRITING lines against the resting ones:");
        Log("sbo:   readback tracks 'wrote'   -> the write lands");
        Log("sbo:   c6 differs between phases -> THE RENDERER READS THIS BANK");
        Log("sbo:   c6 identical in both      -> it does not, on this bank");
        int moved = 0;
        for (int q = 1; q < 86; q++) {
            if (!g_sbSeen[q]) continue;
            float rng = g_sbMax[q] - g_sbMin[q];
            Log("sbo/range: c6 x%-4d uploads=%-6u bone0.x range=%.2f%s",
                q * 3, g_sbSeen[q], rng, rng > 10.0f ? "   <== MOVED" : "");
            if (rng > 10.0f) moved++;
        }
        Log("sbo/range: %d of the uploads moved more than idle animation would",
            moved);
    }
}


static inline void BoneWigApply()
{
    if (!g_boneWigGo || g_bwPhase < 0) return;
    if (g_bwPhase != 0 && g_bwPhase != 2) return;          // resting
    const uint32_t bank = (g_bwPhase == 0) ? 0x208 : 0x214;
    // 30.65: TRANSLATE, don't rotate. These banks store each bone absolutely, so
    // spinning individual quats only twists joints in place (the "slightly
    // different, can't really tell" result). Shoving every selected bone 25
    // units sideways moves the whole hand+weapon bodily - impossible to miss,
    // and it is exactly the write the real drive will perform.
    const float kOff = 25.0f;
    for (int s = 0; s < g_bwObjN; s++) {
        uint8_t* obj = g_bwObj[s];
        if (!obj || !RangeReadable(obj + bank, 12)) continue;
        uint8_t* d  = *(uint8_t**)(obj + bank);
        int32_t num = *(int32_t*)(obj + bank + 4);
        if (!d || ((uintptr_t)d & 3) || num < 2 || num > 512) continue;
        if (num > g_bwBoneN[s]) num = g_bwBoneN[s];
        if (!RangeReadable(d, (size_t)num * 0x20)) continue;
        for (int b = 0; b < num; b++) {
            if (!g_bwSel[s][b]) continue;
            float* q = (float*)(d + (size_t)b * 0x20);
            float n = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
            if (!(n > 0.5f && n < 2.0f)) continue;          // not a quat, skip
            float* t = q + 4;                                // translation
            if (t[0] != t[0] || t[1] != t[1] || t[2] != t[2]) continue;
            t[0] += kOff;
        }
    }
}
