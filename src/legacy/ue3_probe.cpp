// legacy/ue3_probe.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static LONG CALLBACK WalkVEH(EXCEPTION_POINTERS* xp)
{
    DWORD code = xp->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_IN_PAGE_ERROR)
        return EXCEPTION_CONTINUE_SEARCH;
    DWORD tid = GetCurrentThreadId();
    if ((g_walkTid != 0 && g_walkTid == tid) ||
        (g_walkTid2 != 0 && g_walkTid2 == tid)) {
        g_walkFaultEip  = (uintptr_t)xp->ExceptionRecord->ExceptionAddress;
        g_walkFaultAddr = (xp->ExceptionRecord->NumberParameters >= 2)
            ? (uintptr_t)xp->ExceptionRecord->ExceptionInformation[1] : 0;
        InterlockedIncrement(&g_walkFaults);
        if (g_walkTid != 0 && g_walkTid == tid) {
            g_walkTid = 0;
            longjmp(g_walkJmp, 1);           // unwind to the walk boundary
        }
        g_walkTid2 = 0;
        longjmp(g_walkJmp2, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}


static void RunUE3Probe()
{
    Log("probe: ---- UE3 probe run %d (frame %lu) ----", g_probeRuns, (unsigned long)g_frame);

    // 1) verify GNames
    if (!RangeReadable((void*)kGNamesData, 8)) { Log("probe: GNames globals unreadable"); return; }
    void**   nameData = *(void***)kGNamesData;
    uint32_t nameNum  = *(uint32_t*)kGNamesNum;
    Log("probe: GNames Data=%p Num=%u", (void*)nameData, nameNum);
    const char* n0 = NameFromIndex(0);
    const char* n1 = NameFromIndex(1);
    Log("probe: name[0]='%s' name[1]='%s'", n0 ? n0 : "?", n1 ? n1 : "?");
    if (!n0 || strcmp(n0, "None") != 0) { Log("probe: GNames verification FAILED"); return; }
    Log("probe: GNames VERIFIED");

    // 2) scan .data/BSS for GObjObjects (TArray<UObject*>)
    uintptr_t bestAddr = 0; int bestScore = 0; uint32_t bestNum = 0;
    for (uintptr_t a = kDataStart; a + 12 <= kDataEnd; a += 4) {
        void**   dat = *(void***)a;
        uint32_t num = *(uint32_t*)(a + 4);
        uint32_t max = *(uint32_t*)(a + 8);
        if (num < 2000 || num > 4000000 || max < num || max > 8000000) continue;
        if (((uintptr_t)dat & 3) || !RangeReadable(dat, 64 * sizeof(void*))) continue;
        int ok = 0, nonnull = 0;
        for (int i = 0; i < 64; i++) {
            uint8_t* o = (uint8_t*)dat[i];
            if (!o) continue;
            nonnull++;
            if (((uintptr_t)o & 3) == 0 && RangeReadable(o, 8)) {
                uintptr_t vt = *(uintptr_t*)o;
                if (vt > kModBase && vt < kModEnd) ok++;
            }
        }
        if (nonnull >= 32 && ok * 4 >= nonnull * 3) {
            Log("probe: GObjects candidate @0x%08x Data=%p Num=%u Max=%u vtbl-ok=%d/%d",
                (unsigned)a, (void*)dat, num, max, ok, nonnull);
            if (ok > bestScore) { bestScore = ok; bestAddr = a; bestNum = num; }
        }
    }
    if (!bestAddr) { Log("probe: no GObjects candidate found"); return; }
    Log("probe: best GObjects candidate @0x%08x (Num=%u)", (unsigned)bestAddr, bestNum);

    void**   objs = *(void***)bestAddr;
    uint32_t onum = *(uint32_t*)(bestAddr + 4);

    // 3) discover the UObject Name-field offset. Score each offset by how many
    //    sampled objects have a REAL (non-None) varied name there. Zero-field
    //    offsets collapse to "None" and score ~0, so they're rejected now.
    uint32_t nameOff = 0; int nameBest = 0;
    for (uint32_t off = 0x10; off <= 0x60; off += 4) {
        int real = 0, tested = 0;
        uint32_t distinctIdx[24]; int nd = 0;
        uint32_t step = onum / 400 + 1;
        for (uint32_t i = 0; i < onum && tested < 300; i += step) {
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, off + 8)) continue;
            tested++;
            uint32_t idx = *(uint32_t*)(o + off);
            if (!RealName(idx)) continue;
            real++;
            if (nd < 24) {
                bool seen = false;
                for (int k = 0; k < nd; k++) if (distinctIdx[k] == idx) { seen = true; break; }
                if (!seen) distinctIdx[nd++] = idx;
            }
        }
        // require a real name on most objects AND genuine variety (nd>=8)
        if (tested >= 100 && real * 2 >= tested && nd >= 8) {
            Log("probe: Name-offset +0x%02x real=%d/%d distinct>=%d", off, real, tested, nd);
            if (real > nameBest) { nameBest = real; nameOff = off; }
        }
    }
    if (!nameOff) { Log("probe: no Name offset discovered"); return; }
    Log("probe: USING Name offset +0x%02x", nameOff);

    // 4) sample real object names so we can eyeball the world
    int shown = 0;
    for (uint32_t i = 1; i < onum && shown < 16; i += onum / 400 + 7) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || !RangeReadable(o, nameOff + 8)) continue;
        const char* nm = RealName(*(uint32_t*)(o + nameOff));
        if (nm) { Log("probe: obj[%u] '%s'", i, nm); shown++; }
    }

    // UE3 UObject layout confirmed from run-2 dumps:
    //   +nameOff (0x28) = Name.Index, +nameOff+4 = Name.Number, +nameOff+8 = Class*
    // Class==0 number means a class/CDO; a live instance has Number != 0
    // (e.g. "DishonoredPlayerCamera_0"). Follow Class* to get the class name.
    const uint32_t classOff = nameOff + 8;

    // helper via lambda-like static: resolve an object's class name
    // (inline, since C++ here has no captures we need)
    #define CLASSNAME(objptr, out) do { \
        out = NULL; \
        if (RangeReadable((objptr), classOff + 4)) { \
            uint8_t* cls = *(uint8_t**)((objptr) + classOff); \
            if (cls && ((uintptr_t)cls & 3) == 0 && RangeReadable(cls, nameOff + 8)) \
                out = RealName(*(uint32_t*)(cls + nameOff)); \
        } \
    } while (0)

    // sanity: print class names of a few objects to confirm classOff
    for (uint32_t i = 1, shownc = 0; i < onum && shownc < 4; i += 5000) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || !RangeReadable(o, classOff + 4)) continue;
        const char* nm = RealName(*(uint32_t*)(o + nameOff));
        const char* cn; CLASSNAME(o, cn);
        if (nm && cn) { Log("probe: obj[%u] '%s' : class '%s'", i, nm, cn); shownc++; }
    }

    // 5) full-array scan for LIVE INSTANCES of the player camera/controller/pawn.
    //    Match by CLASS name, and require Name.Number != 0 (an actual instance).
    Log("probe: scanning %u objects for live player instances...", onum);
    uint32_t camIdx = 0, ctrlIdx = 0, pawnIdx = 0;
    int insts = 0;
    for (uint32_t i = 1; i < onum; i++) {
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, classOff + 4)) continue;
        uint32_t nidx = *(uint32_t*)(o + nameOff);
        uint32_t nnum = *(uint32_t*)(o + nameOff + 4);
        const char* nm = RealName(nidx);
        if (!nm) continue;
        const char* cn; CLASSNAME(o, cn);
        if (!cn) continue;
        bool isCam  = strstr(cn, "PlayerCamera") != NULL;
        bool isCtrl = strstr(cn, "PlayerController") != NULL;
        bool isPawn = strstr(cn, "PlayerPawn") != NULL;
        if (!isCam && !isCtrl && !isPawn) continue;
        bool instance = (nnum != 0) && (strncmp(nm, "Default__", 9) != 0);
        if (insts < 40)
            Log("probe: %s obj[%u] name='%s_%u' class='%s'",
                instance ? "INSTANCE" : "class   ", i, nm, nnum ? nnum - 1 : 0, cn);
        if (instance) {
            insts++;
            if (isCam  && !camIdx)  camIdx  = i;
            if (isCtrl && !ctrlIdx) ctrlIdx = i;
            if (isPawn && !pawnIdx) pawnIdx = i;
        }
    }
    Log("probe: live instances: cam=obj[%u] ctrl=obj[%u] pawn=obj[%u]",
        camIdx, ctrlIdx, pawnIdx);

    // 6) deep dump of the LIVE instances + flag FVector-looking float triples
    //    (world coords: each component finite, |v|<1e6, and not all tiny).
    uint32_t targets[3] = { pawnIdx, camIdx, ctrlIdx };
    const char* tlabel[3] = { "LIVE-PAWN", "LIVE-CAMERA", "LIVE-CONTROLLER" };
    for (int t = 0; t < 3; t++) {
        if (!targets[t]) { Log("probe: %s not found", tlabel[t]); continue; }
        uint8_t* o = (uint8_t*)objs[targets[t]];
        if (!o || !RangeReadable(o, 0x10)) continue;
        const char* nm = RealName(*(uint32_t*)(o + nameOff));
        char label[160];
        _snprintf(label, sizeof(label), "%s obj[%u] '%s'", tlabel[t], targets[t], nm ? nm : "?");
        HexDumpObject(label, o, 0x400);
        // scan for FVector Location candidates
        for (uint32_t off = 0x20; off + 12 <= 0x400; off += 4) {
            if (!RangeReadable(o + off, 12)) break;
            float* f = (float*)(o + off);
            int okc = 0; float mag = 0;
            for (int k = 0; k < 3; k++) {
                float v = f[k];
                if (v != v || v > 3.0e38f || v < -3.0e38f) { okc = -99; break; }
                float av = v < 0 ? -v : v;
                if (av > 1.0e6f) { okc = -99; break; }
                if (av > 1.0f) okc++;
                mag += av;
            }
            if (okc >= 2 && mag > 50.0f)
                Log("probe:   FVector? +0x%03x = (% .1f, % .1f, % .1f)",
                    (unsigned)off, f[0], f[1], f[2]);
        }
    }
    #undef CLASSNAME
    Log("probe: ---- probe done (live instances=%d) ----", insts);
}


static bool GraphInteresting(const char* c)
{
    return c && (strstr(c, "Mesh") || strstr(c, "Weapon") || strstr(c, "Pawn") ||
                 strstr(c, "Skel") || strstr(c, "Anim") || strstr(c, "Inventory") ||
                 strstr(c, "Hand") || strstr(c, "Arm")   || strstr(c, "Component"));
}


// Walk outward from the PAWN, not the controller: the first-person arms and
// weapon hang off the pawn as components. Everything is logged this time - no
// "interesting name" filter - because guessing at names is what wasted the
// earlier attempts. Read-only.
static void PawnWalk()
{
    // No cursor-visibility guard here: that heuristic reads "menu" for a while
    // after a load and just blocked the scan. Validating every pointer against
    // the live object table is what actually makes this safe.
    if (!BuildLiveSet()) { Log("pawnwalk: object table busy - try again"); return; }
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!FindPlayerController() || !IsLiveObject(g_pcObj)) {
        Log("pawnwalk: no player controller"); return;
    }

    // controller -> pawn (the graph walk found it at +0x248, but re-derive it)
    uint8_t* pawn = NULL;
    for (uint32_t off = 0x100; off + 4 <= 0x400; off += 4) {
        if (!RangeReadable(g_pcObj + off, 4)) break;
        uint8_t* p = *(uint8_t**)(g_pcObj + off);
        if (!IsLiveObject(p) || !RangeReadable(p, kClassOff + 4)) continue;
        const char* cn = ObjClassName(p);
        if (cn && strstr(cn, "PlayerPawn") && !strstr(cn, "Proxy")) {
            pawn = p;
            Log("pawnwalk: pawn '%s' @ %p (controller +0x%03x)", cn, (void*)p,
                (unsigned)off);
            break;
        }
    }
    if (!pawn) { Log("pawnwalk: pawn not found"); return; }

    int edges = 0;
    for (uint32_t off = 0x20; off + 4 <= 0x600 && edges < 120; off += 4) {
        if (!RangeReadable(pawn + off, 4)) break;
        uint8_t* c = *(uint8_t**)(pawn + off);
        if (!IsLiveObject(c) || !RangeReadable(c, kClassOff + 8)) continue;
        const char* cc = ObjClassName(c);
        if (!cc) continue;
        const char* nm = RealName(*(uint32_t*)(c + kNameOff));
        Log("pawnwalk:   pawn +0x%03x -> '%s'  (%s)", (unsigned)off,
            nm ? nm : "?", cc);
        edges++;

        // pMesh is the first-person arms+weapon. Dump its transform fields:
        // a UE3 component carries Translation / Rotation / Scale3D plus a
        // cached LocalToWorld matrix, and those are what we write to put it
        // in your hand.
        if ((nm && !strcmp(nm, "pMesh")) || strstr(cc, "SkeletalComponent")) {
            Log("pawnwalk:   >>> first-person mesh component @ %p - fields:", (void*)c);
            for (uint32_t o2 = 0x0c; o2 + 12 <= 0x400; o2 += 4) {
                if (!RangeReadable(c + o2, 12)) break;
                float* f = (float*)(c + o2);
                bool fin = true;
                for (int k = 0; k < 3; k++)
                    if (f[k] != f[k] || fabsf(f[k]) > 3.0e38f) { fin = false; break; }
                int32_t* r = (int32_t*)(c + o2);
                bool rot = true; int nz = 0;
                for (int k = 0; k < 3; k++) {
                    if (r[k] < -0x20000 || r[k] > 0x20000) { rot = false; break; }
                    if (r[k]) nz++;
                }
                if (fin) {
                    float mag = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
                    if (mag > 0.9f && mag < 1.1f)
                        Log("pawnwalk:       +0x%03x unit  (%.3f,%.3f,%.3f)",
                            (unsigned)o2, f[0], f[1], f[2]);
                    else if (mag > 0.01f && mag < 1.0e6f)
                        Log("pawnwalk:       +0x%03x vec   (%.2f,%.2f,%.2f) |v|=%.2f",
                            (unsigned)o2, f[0], f[1], f[2], mag);
                }
                if (rot && nz >= 1)
                    Log("pawnwalk:       +0x%03x ROT?  (%d,%d,%d)",
                        (unsigned)o2, r[0], r[1], r[2]);
            }
        }

        // one level deeper for anything mesh- or weapon-shaped
        if (strstr(cc, "Mesh") || strstr(cc, "Weapon") || strstr(cc, "Inventory")) {
            for (uint32_t o2 = 0x20; o2 + 4 <= 0x400 && edges < 120; o2 += 4) {
                if (!RangeReadable(c + o2, 4)) break;
                uint8_t* d = *(uint8_t**)(c + o2);
                if (!IsLiveObject(d) || !RangeReadable(d, kClassOff + 8)) continue;
                const char* dc = ObjClassName(d);
                if (!dc) continue;
                const char* dn = RealName(*(uint32_t*)(d + kNameOff));
                Log("pawnwalk:       +0x%03x -> '%s'  (%s)", (unsigned)o2,
                    dn ? dn : "?", dc);
                edges++;
            }
        }
    }
    Log("pawnwalk: ==== %d edge(s) ====", edges);
}


static void GraphWalk()
{
    if (g_inMenu) { Log("graph: skipped - not in gameplay"); return; }
    if (!BuildLiveSet()) { Log("graph: object table not stable right now - try again"); return; }
    if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
    if (!FindPlayerController()) { Log("graph: no player controller yet - load a save first"); return; }
    if (!IsLiveObject(g_pcObj)) { Log("graph: controller is not live - aborting"); return; }

    uint8_t* q[64]; int qd[64];
    uint8_t* seen[192]; int seenN = 0;
    int head = 0, tail = 0;
    q[tail] = g_pcObj; qd[tail] = 0; tail++;
    seen[seenN++] = g_pcObj;

    int edges = 0, dumped = 0;
    Log("graph: ==== walking out from '%s' ====", ObjClassName(g_pcObj));

    while (head < tail && edges < 160) {
        uint8_t* o = q[head]; int d = qd[head]; head++;
        const char* pc = ObjClassName(o);
        if (d >= 2) continue;
        for (uint32_t off = 0x20; off + 4 <= 0x400 && edges < 160; off += 4) {
            if (!RangeReadable(o + off, 4)) break;
            uint8_t* c = *(uint8_t**)(o + off);
            if (!LooksLikeObject(c)) continue;
            bool dup = false;
            for (int i = 0; i < seenN; i++) if (seen[i] == c) { dup = true; break; }
            if (dup) continue;
            if (seenN < 192) seen[seenN++] = c;

            const char* cc = ObjClassName(c);
            const char* nm = RealName(*(uint32_t*)(c + kNameOff));
            // depth 0 = everything the controller points at; deeper = only the
            // mesh/weapon side of the graph, so the log stays readable
            if (d == 0 || GraphInteresting(cc)) {
                Log("graph:%*s %s +0x%03x -> '%s'  (%s)", d * 3, "",
                    pc ? pc : "?", (unsigned)off, nm ? nm : "?", cc ? cc : "?");
                edges++;
            }
            if (tail < 64 && d + 1 < 2 && (d == 0 || GraphInteresting(cc))) {
                q[tail] = c; qd[tail] = d + 1; tail++;
            }
            // for anything mesh-like, show the transform fields we could write
            if (dumped < 5 && cc &&
                (strstr(cc, "SkeletalMeshComponent") || strstr(cc, "Weapon"))) {
                dumped++;
                Log("graph:   --- transform fields of '%s' ---", cc);
                float fwd[3] = {1,0,0};
                DumpAimFields(c, fwd);
            }
        }
    }
    Log("graph: ==== done: %d edge(s), %d mesh object(s) dumped ====", edges, dumped);
}


static void BoneDump()
{
    Log("bones: ==== bone probe over %d candidate(s) ====", g_fpCandN);
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        Log("bones: [%d] scanning '%s' obj=%p", i, k->asset, (void*)k->obj);
        if (!LooksLikeObj(k->obj)) continue;

        // BoneVisibilityStates-shaped arrays on the COMPONENT. 30.12: the
        // first pass over 0x100-0x800 found nothing, so hunt wider and also
        // report every TArray header whose count EQUALS the bone count -
        // whatever its element size - plus empty arrays (num=0) that might be
        // the lazily-allocated visibility array itself.
        int bvsNum = 0;
        for (uint32_t o = 0x100; o + 12 <= 0x1000; o += 4) {
            if (!RangeReadable(k->obj + o, 12)) break;
            uint8_t* d  = *(uint8_t**)(k->obj + o);
            int32_t num = *(int32_t*)(k->obj + o + 4);
            int32_t max = *(int32_t*)(k->obj + o + 8);
            if (num < 0 || num > 512 || max < num || max > 1024) continue;
            if (num == 0) continue;
            if (!d || !RangeReadable(d, (size_t)num)) continue;
            bool allSmall = true;
            for (int b = 0; b < num; b++)
                if (d[b] > 2) { allSmall = false; break; }
            if (!allSmall || num < 8) continue;
            char pre[80]; pre[0] = 0;
            int nb = num < 20 ? num : 20;
            for (int b = 0; b < nb; b++) {
                char one[4];
                snprintf(one, sizeof(one), "%d ", (int)d[b]);
                strcat(pre, one);
            }
            Log("bones: [%d] '%s' BVS? +0x%03x num=%d max=%d bytes: %s",
                i, k->asset, (unsigned)o, (int)num, (int)max, pre);
            if (!bvsNum) bvsNum = num;
        }

        // RefSkeleton bone names on the ASSET
        uint8_t* a = FpAssetObj(k->obj);
        if (!a) { Log("bones: [%d] '%s' no SkeletalMesh pointer", i, k->asset); continue; }
        bool found = false;
        int refN = 0;
        for (uint32_t o = 0x28; o + 12 <= 0x300 && !found; o += 4) {
            if (!RangeReadable(a + o, 12)) break;
            uint8_t* d  = *(uint8_t**)(a + o);
            int32_t num = *(int32_t*)(a + o + 4);
            int32_t max = *(int32_t*)(a + o + 8);
            if (num < 8 || num > 256 || max < num || max > 512) continue;
            if (bvsNum && num != bvsNum) continue;   // bone counts must agree
            if (!d || ((uintptr_t)d & 3)) continue;
            for (uint32_t st = 12; st <= 64 && !found; st += 4) {
                if (!RangeReadable(d, (size_t)num * st)) continue;
                for (uint32_t no = 0; no + 8 <= st && !found; no += 4) {
                    int tries = num < 6 ? num : 6, good = 0;
                    for (int e = 0; e < tries; e++)
                        if (RealName(*(uint32_t*)(d + (size_t)e*st + no))) good++;
                    if (good != tries) continue;
                    found = true;
                    refN = (int)num;
                    Log("bones: [%d] '%s' RefSkeleton +0x%03x stride=%u nameoff=%u num=%d",
                        i, k->asset, (unsigned)o, st, no, (int)num);
                    char line[200]; line[0] = 0;
                    for (int e = 0; e < num; e++) {
                        const char* nm = RealName(*(uint32_t*)(d + (size_t)e*st + no));
                        char one[52];
                        snprintf(one, sizeof(one), "%d:%s ", e, nm ? nm : "?");
                        if (strlen(line) + strlen(one) > 180) {
                            Log("bones:   %s", line);
                            line[0] = 0;
                        }
                        strcat(line, one);
                    }
                    if (line[0]) Log("bones:   %s", line);
                }
            }
        }
        // 30.61: SPACEBASES - the post-animation, component-space bone bank the
        // renderer must consume (the bioshock-vr author's hands mechanism).
        // Signature: a TArray on the COMPONENT whose count equals the bone
        // count, whose elements are FBoneAtom - unit quaternion (16B) +
        // translation (12B) + scale (4B), stride 0x20. Writing this bank, with
        // the controller pose pushed through the component's LocalToWorld
        // inverse, is what puts the weapon on the player's actual hand.
        if (refN > 0) {
            int sbHits = 0;
            for (uint32_t o = 0x100; o + 12 <= 0x600 && sbHits < 4; o += 4) {
                if (!RangeReadable(k->obj + o, 12)) break;
                uint8_t* d  = *(uint8_t**)(k->obj + o);
                int32_t num = *(int32_t*)(k->obj + o + 4);
                int32_t max = *(int32_t*)(k->obj + o + 8);
                if (num != refN || max < num || max > 512) continue;
                if (!d || ((uintptr_t)d & 3)) continue;
                for (uint32_t st = 0x20; st <= 0x40; st += 0x20) {
                    if (!RangeReadable(d, (size_t)num * st)) continue;
                    int tries = num < 8 ? num : 8, good = 0;
                    for (int e = 0; e < tries; e++) {
                        const float* q = (const float*)(d + (size_t)e * st);
                        float n = q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3];
                        if (n > 0.9f && n < 1.1f) good++;
                    }
                    if (good < tries - 1) continue;      // not unit quats
                    sbHits++;
                    Log("bones: [%d] '%s' SPACEBASES? +0x%03x stride=0x%x num=%d",
                        i, k->asset, (unsigned)o, st, (int)num);
                    for (int e = 0; e < 3 && e < num; e++) {
                        const float* q = (const float*)(d + (size_t)e * st);
                        const float* t = q + 4;
                        Log("bones:    atom[%d] q=(%.3f,%.3f,%.3f,%.3f) t=(%.1f,%.1f,%.1f)",
                            e, q[0], q[1], q[2], q[3], t[0], t[1], t[2]);
                    }
                    break;
                }
            }
            if (!sbHits)
                Log("bones: [%d] '%s' no SpaceBases-shaped array in +0x100..+0x600",
                    i, k->asset);
        }

        if (!found)
            Log("bones: [%d] '%s' RefSkeleton not identified (bvsNum=%d)",
                i, k->asset, bvsNum);

        // every COMPONENT array sized exactly to the bone count, whatever it
        // holds - one of these is where per-bone visibility must live
        if (refN > 0) {
            for (uint32_t o = 0x100; o + 12 <= 0x1000; o += 4) {
                if (!RangeReadable(k->obj + o, 12)) break;
                uint8_t* d  = *(uint8_t**)(k->obj + o);
                int32_t num = *(int32_t*)(k->obj + o + 4);
                int32_t max = *(int32_t*)(k->obj + o + 8);
                if (num != refN || max < num || max > 1024) continue;
                if (!d || !RangeReadable(d, 16)) continue;
                const uint32_t* w = (const uint32_t*)d;
                Log("bones: [%d] '%s' N-array +0x%03x num=%d max=%d data=%p w: %08x %08x %08x %08x",
                    i, k->asset, (unsigned)o, (int)num, (int)max, (void*)d,
                    w[0], w[1], w[2], w[3]);
            }
        }
    }
    Log("bones: ==== probe done ====");
}


// 30.9b: F3 sets a flag instead of dumping directly - the first F3 attempt
// ran on the RENDER thread while the game thread re-collected the candidate
// list underneath it, and died instantly (the fault boundary caught it).
// Here, on the game thread, the dump is serialized with collect and drive.

static void DbgProbeTick()
{
    if (g_bonesReq) {
        g_bonesReq = 0;
        FpArrayDump();
        BoneDump();
    }
    if (g_armReq) {
        g_armReq = 0;
        ArmsToggle();
    }
    static bool done = false;
    static double armMs = 0.0;
    if (done || !g_dbgProbe[0]) return;
    if (!g_handMesh || !g_fpCandN) return;
    double now = MaimNowMs();
    if (armMs == 0.0) { armMs = now + 2000.0; return; }
    if (now < armMs) return;
    done = true;
    Log("debug: running scheduled probe '%s'", g_dbgProbe);
    if      (!strcmp(g_dbgProbe, "bones"))  { FpArrayDump(); BoneDump(); }
    else if (!strcmp(g_dbgProbe, "census")) FpCensus();
    else if (!strcmp(g_dbgProbe, "graph"))  { GraphWalk(); PawnWalk(); }
    else if (!strcmp(g_dbgProbe, "ue3"))    RunUE3Probe();
    else if (!strcmp(g_dbgProbe, "view")) {
        if (!CamStillValid()) { g_camObj = NULL; FindLiveCamera(); }
        VpFindObjects();
        if (g_vpN) {
            memset(g_vpNoise, 0, sizeof(g_vpNoise));
            g_vpProbing = true;
            VpSnap(g_vpSnapA);
            g_vpPhase = 1; g_vpTimer = 10;
            Log("viewprobe: control pass (scheduled)");
        }
    }
    else Log("debug: unknown probe '%s'", g_dbgProbe);
}
