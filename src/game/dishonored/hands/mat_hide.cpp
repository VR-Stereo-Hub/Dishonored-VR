// game/dishonored/hands/mat_hide.cpp - included by src/mod/dishonoredvr.cpp (unity build).
//
// ===========================================================================
// VR-31 route (d): MATERIAL-SECTION hiding
//
// Routes (b) and (c) both fight the skeleton. This one does not touch it at
// all: it removes GEOMETRY from a draw and leaves every bone transform, every
// animation and every attachment exactly where the engine put them.
//
// What this build declares (names and signatures read off the local script
// dump, which is gitignored and never committed - only findings live here):
//
//   MeshComponent::Materials                    array<MaterialInterface>
//   MeshComponent::GetNumElements()             native, returns int
//   MeshComponent::GetMaterial(int)             native, returns MaterialInterface
//   SkeletalMeshComponent::LODInfo[]            .HiddenMaterials is member 0
//   SkeletalMeshComponent::ShowMaterialSection(int MaterialID, bool bShow, int LOD)
//   DisSkeletalMeshComponent::m_UsedMaterials   {int m_Index; bool m_bShown;}
//   SkeletalMesh::Materials                     the ASSET's section list
//   SkeletalMesh::m_MaterialsToBodyParts        BodyPart[] (NPC dismemberment)
//
// ---------------------------------------------------------------------------
// FOUR FAULTS FROM THE ALPHA-303 REVIEW, AND WHAT REPLACED THEM
//
// 1. The visibility readback could never work. MatArray refused off == 0, and
//    both HiddenMaterials reads pass 0 (it is member 0 of the LODInfo struct),
//    so the census reported 0 entries and the post-call readback said
//    "unreadable" whether or not hiding worked. Split in two: MatArrayAt takes
//    any offset including 0 and is for reads INSIDE a struct; MatArrayProp
//    requires a resolved property offset and is for reads off an object.
//
// 2. The name-anywhere fallback was unsound. Materials and LODInfo are
//    declared on several unrelated types, and an offset found on SkeletalMesh
//    is meaningless applied to a component - logging the class it came from
//    does not make the number valid. There is no fallback now: every field
//    names its VERIFIED declaring class and a miss is a refusal, logged.
//
// 3. The sweep could not prove what it was changing. It took g_armComp or
//    candidate 0, and the log carries two player skeletal components with no
//    evidence which one draws the arms; it also only ever touched LOD 0. It
//    now sweeps EVERY component that reports sections, and every LOD each one
//    declares, naming the component, section and LOD at every step.
//
// 4. "Restore" meant show-everything-it-touched, not restore. Nothing saved
//    the prior visibility, so a section the game had already hidden could be
//    left visible afterwards. Every hide now records the exact prior flag per
//    (component, section, LOD) and puts that value back.
//
// Cross-check, also from the review: GetNumElements and GetMaterial are
// dispatched through the engine, so the section count and the material NAMES
// are the engine's own answer rather than another memory-layout guess. When
// the two disagree the log says so and the dispatch wins. On this rig the
// material name is the only handle there is, because m_MaterialsToBodyParts
// is an NPC dismemberment feature and the player asset carries none.
//
// Nothing here waits for a command: the census and the sweep both run
// themselves and the sweep restores what it touched.
// ===========================================================================

// UE3 FName in memory: {int Index; int Number;}. Only Index names the string.
static const char* MatFName(uint8_t* p)
{
    if (!RangeReadable(p, 8)) return NULL;
    return RealName(*(uint32_t*)p);
}


// A TArray header read at an arbitrary offset, 0 included. For reads INSIDE a
// struct, where 0 is a legitimate member offset (fault 1).
static bool MatArrayAt(uint8_t* base, uint32_t off, uint8_t** dOut,
                       int32_t* nOut, int32_t maxN)
{
    *dOut = NULL; *nOut = 0;
    if (!base || !RangeReadable(base + off, 12)) return false;
    uint8_t* d = *(uint8_t**)(base + off);
    int32_t  n = *(int32_t*)(base + off + 4);
    if (n < 0 || n > maxN) return false;
    if (n > 0 && (!d || ((uintptr_t)d & 3))) return false;
    *dOut = d; *nOut = n;
    return true;
}

// The same read off an OBJECT, where a 0 offset means the property never
// resolved and reading base+0 would hand back the vtable as an array header.
static bool MatArrayProp(uint8_t* base, uint32_t off, uint8_t** dOut,
                         int32_t* nOut, int32_t maxN)
{
    *dOut = NULL; *nOut = 0;
    if (!off) return false;
    return MatArrayAt(base, off, dOut, nOut, maxN);
}


// Every field names the class that VERIFIABLY declares it. No fallback: an
// offset from an unrelated class is worse than no offset at all (fault 2).
static uint32_t MatProp(const char* cls, const char* name)
{
    const uint32_t off = FindPropOffset(cls, name);
    if (off) Log("mat:   %-24s +0x%03x on %s", name, off, cls);
    else     Log("mat:   %-24s NOT FOUND on %s - REFUSED, and deliberately no "
                 "fallback: an offset borrowed from another class is a wrong "
                 "number applied with confidence", name, cls);
    return off;
}


static void MatResolve(const char* why)
{
    if (g_matResolved) return;
    g_matResolved = true;
    Log("mat: reflection (%s), each on its verified declaring class:", why);
    g_matOffLodInfo   = MatProp("SkeletalMeshComponent", "LODInfo");
    g_matOffMaterials = MatProp("MeshComponent", "Materials");
    g_matOffUsed      = MatProp("DisSkeletalMeshComponent", "m_UsedMaterials");
    g_matOffAssetMats = MatProp("SkeletalMesh", "Materials");
    g_matOffToBody    = MatProp("SkeletalMesh", "m_MaterialsToBodyParts");
    g_fnShowMatSection = FindFunctionObj("ShowMaterialSection");
    g_fnGetNumElements = FindFunctionObj("GetNumElements");
    g_fnGetMaterial    = FindFunctionObj("GetMaterial");
    Log("mat: natives - ShowMaterialSection %s, GetNumElements %s, "
        "GetMaterial %s",
        g_fnShowMatSection ? "found" : "MISSING",
        g_fnGetNumElements ? "found" : "MISSING",
        g_fnGetMaterial    ? "found" : "MISSING");
}


// The engine's own answer for how many sections a component draws, independent
// of every offset above - which is the entire point of asking it.
static int MatNumElements(uint8_t* comp)
{
    if (!g_fnGetNumElements || !comp || !LooksLikeObj(comp)) return -1;
    struct { int32_t ReturnValue; } p;
    p.ReturnValue = -1;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(comp, g_fnGetNumElements, &p, NULL);
    g_peReentry = false;
    return (p.ReturnValue >= 0 && p.ReturnValue < 256) ? p.ReturnValue : -1;
}


static const char* MatMaterialName(uint8_t* comp, int i)
{
    if (!g_fnGetMaterial || !comp) return NULL;
    struct { int32_t ElementIndex; uint8_t* ReturnValue; } p;
    p.ElementIndex = i; p.ReturnValue = NULL;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(comp, g_fnGetMaterial, &p, NULL);
    g_peReentry = false;
    if (!p.ReturnValue || !LooksLikeObj(p.ReturnValue)) return NULL;
    return RealName(*(uint32_t*)(p.ReturnValue + kNameOff));
}


// How many LODs this component declares, so a sweep is not silently LOD 0 only.
static int MatNumLods(uint8_t* comp)
{
    uint8_t* d; int32_t n;
    if (MatArrayProp(comp, g_matOffLodInfo, &d, &n, 8)) return n;
    return 0;
}


// HiddenMaterials for one LOD. LOD 0 needs no stride (member 0 of element 0
// starts at the element pointer); higher LODs do, and it is derived from the
// declared members rather than assumed: array<bool> 12 + 2 UBOOL 8 + enum 4 +
// int 4 = 28. If that is wrong, LOD 0 still reads correctly and the higher
// LODs report unreadable rather than reporting a wrong number confidently.
static const uint32_t kLodInfoStride = 28;

static bool MatHiddenArray(uint8_t* comp, int lod, uint8_t** dOut, int32_t* nOut)
{
    *dOut = NULL; *nOut = 0;
    uint8_t* li; int32_t nLi;
    if (!MatArrayProp(comp, g_matOffLodInfo, &li, &nLi, 8)) return false;
    if (lod < 0 || lod >= nLi || !li) return false;
    return MatArrayAt(li + (size_t)lod * kLodInfoStride, 0, dOut, nOut, 64);
}


static int MatHiddenFlag(uint8_t* comp, int id, int lod)
{
    uint8_t* h; int32_t n;
    if (!MatHiddenArray(comp, lod, &h, &n)) return -1;
    if (id < 0 || id >= n || !RangeReadable(h + (size_t)id * 4, 4)) return -1;
    return *(int32_t*)(h + (size_t)id * 4) ? 1 : 0;
}


// The census for ONE component. Read-only, and it reports the engine's own
// element count beside the memory-derived ones so a disagreement is visible.
static void MatCensusOne(uint8_t* comp, const char* asset, const char* cls)
{
    uint8_t* d; int32_t n;

    int nCompMat = 0, nAssetMat = 0;
    if (MatArrayProp(comp, g_matOffMaterials, &d, &n, 64)) nCompMat = n;
    uint8_t* mesh = FpAssetObj(comp);
    if (mesh && MatArrayProp(mesh, g_matOffAssetMats, &d, &n, 64)) nAssetMat = n;

    const int nEng = MatNumElements(comp);
    const int nLod = MatNumLods(comp);

    uint8_t* body = NULL; int32_t nBody = 0;
    if (mesh) MatArrayProp(mesh, g_matOffToBody, &body, &nBody, 64);
    uint8_t* used = NULL; int32_t nUsed = 0;
    MatArrayProp(comp, g_matOffUsed, &used, &nUsed, 64);

    Log("mat: --- '%s' (%s) asset=%s | GetNumElements=%d | asset Materials=%d "
        "| component override=%d | LODs=%d | bodyparts=%d | used[]=%d",
        asset, cls, mesh ? "found" : "NOT FOUND", nEng, nAssetMat, nCompMat,
        nLod, nBody, nUsed);
    if (!mesh)
        Log("mat:     no SkeletalMesh asset resolved off this component - that "
            "is a probe limit, not a mesh with no materials");
    if (nEng >= 0 && nAssetMat > 0 && nEng != nAssetMat)
        Log("mat:     DISAGREEMENT: the engine says %d sections, the asset "
            "array says %d. The engine wins and the array read is suspect.",
            nEng, nAssetMat);

    const int sections = nEng >= 0 ? nEng
                       : (nAssetMat > nCompMat ? nAssetMat : nCompMat);
    if (sections <= 0 && nBody <= 0) {
        Log("mat:     no sections and no body parts to report");
        return;
    }

    for (int i = 0; i < sections && i < 64; i++) {
        const char* mn = MatMaterialName(comp, i);
        const char* owner = "-"; const char* cut = "-";
        if (body && i < nBody && RangeReadable(body + (size_t)i * 20, 20)) {
            const char* o = MatFName(body + (size_t)i * 20);
            const char* c = MatFName(body + (size_t)i * 20 + 8);
            if (o) owner = o;
            if (c) cut = c;
        }
        int shown = -1;
        if (used && i < nUsed && RangeReadable(used + (size_t)i * 8, 8))
            shown = *(int32_t*)(used + (size_t)i * 8 + 4) ? 1 : 0;
        // The material NAME is the independent handle, and on this rig it is
        // the ONLY one: the body-part table is an NPC dismemberment feature
        // and the player asset carries none.
        Log("mat:   [%2d] material=%-30s owner=%-18s cut=%-14s shown=%-2d hidden(LOD0)=%d",
            i, mn ? mn : "?", owner, cut, shown, MatHiddenFlag(comp, i, 0));
    }
}


static void MatCensus(const char* why)
{
    MatResolve(why);
    if (!g_fpCandN && FpPawn()) {
        Log("mat: no candidate list (the hand drive is off, so nobody built "
            "one) - collecting once for the census");
        FpCollect();
    }
    if (!g_fpCandN) { Log("mat: no first-person components to census yet"); return; }
    Log("mat: ==== material census (%s), %d component(s) ====", why, g_fpCandN);
    for (int i = 0; i < g_fpCandN; i++) {
        FpCand* k = &g_fpCand[i];
        if (!LooksLikeObj(k->obj)) continue;
        Log("mat: component [%d] of %d", i, g_fpCandN - 1);
        MatCensusOne(k->obj, k->asset, k->cls);
    }
    Log("mat: ==== census done ====");
}


// Hide or show through the engine's own native, saving the exact prior flag so
// a restore is a restore and not a blanket show (fault 4).
static bool MatShowSection(uint8_t* comp, int id, bool show, int lod)
{
    if (!g_fnShowMatSection || !comp || !LooksLikeObj(comp)) return false;
    const int before = MatHiddenFlag(comp, id, lod);
    if (!show) {
        if (g_matHidN >= 64) {
            Log("mat: REFUSED - the restore table is full, not hiding more");
            return false;
        }
        g_matHid[g_matHidN].comp = comp;
        g_matHid[g_matHidN].id   = id;
        g_matHid[g_matHidN].lod  = lod;
        g_matHid[g_matHidN].wasHidden = before;      // -1 = was unreadable
        g_matHidN++;
    }
    struct Parms { int32_t MaterialID; int32_t bShow; int32_t LODIndex; } p;
    p.MaterialID = id; p.bShow = show ? 1 : 0; p.LODIndex = lod;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(comp, g_fnShowMatSection, &p, NULL);
    g_peReentry = false;
    const int after = MatHiddenFlag(comp, id, lod);
    const int want = show ? 0 : 1;
    Log("mat: ShowMaterialSection(id %d, %s, LOD %d) - HiddenMaterials %s -> %s%s",
        id, show ? "show" : "HIDE", lod,
        before < 0 ? "unreadable" : (before ? "1" : "0"),
        after  < 0 ? "unreadable" : (after  ? "1" : "0"),
        after < 0 ? "  [the array is not readable, so this says NOTHING about "
                    "whether the native worked]"
                  : (after == want ? "  [took]" : "  [DID NOT TAKE]"));
    return true;
}


// Put every touched section back to the value it actually had.
static void MatRestoreAll(const char* why)
{
    int shown = 0, leftHidden = 0;
    for (int i = g_matHidN - 1; i >= 0; i--) {
        MatHid* h = &g_matHid[i];
        if (!h->comp || !LooksLikeObj(h->comp)) continue;
        // wasHidden 1 means the GAME had it hidden before we arrived, so
        // showing it would be a change, not a restore.
        if (h->wasHidden == 1) { leftHidden++; continue; }
        struct Parms { int32_t MaterialID; int32_t bShow; int32_t LODIndex; } p;
        p.MaterialID = h->id; p.bShow = 1; p.LODIndex = h->lod;
        g_peReentry = true;
        ((PFN_ProcessEventCall)kProcessEvent)(h->comp, g_fnShowMatSection, &p, NULL);
        g_peReentry = false;
        shown++;
    }
    Log("mat: restore (%s) - %d shown again, %d left hidden because the game "
        "already had them hidden before we touched them", why, shown, leftHidden);
    g_matHidN = 0;
}


// ---------------------------------------------------------------------------
// The automatic experiment. Sweeps EVERY component that reports sections, and
// every LOD each one declares, so a negative result covers what it claims to
// (fault 3).
// ---------------------------------------------------------------------------

static void MatAutoBuildPlan()
{
    g_matPlanN = 0;
    int nSec[24], nLods[24], maxLod = 1;
    for (int c = 0; c < g_fpCandN && c < 24; c++) {
        nSec[c] = 0; nLods[c] = 0;
        FpCand* k = &g_fpCand[c];
        if (!LooksLikeObj(k->obj)) continue;
        const int nEng = MatNumElements(k->obj);
        if (nEng <= 0) {
            Log("mat/auto: component [%d] '%s' reports %d section(s) - skipped",
                c, k->asset, nEng);
            continue;
        }
        int nl = MatNumLods(k->obj);
        if (nl <= 0) nl = 1;              // LOD 0 is always worth trying
        nSec[c] = nEng; nLods[c] = nl;
        if (nl > maxLod) maxLod = nl;
        Log("mat/auto: component [%d] '%s' - %d section(s) x %d LOD(s) queued",
            c, k->asset, nEng, nl);
    }
    // LOD-major order: every component's LOD 0 first. The first-person mesh is
    // drawn at LOD 0, so the answer almost certainly arrives in the first pass;
    // the higher LODs are only there so a negative result covers what it claims
    // to rather than quietly meaning "LOD 0 only".
    for (int l = 0; l < maxLod && g_matPlanN < 96; l++)
        for (int c = 0; c < g_fpCandN && c < 24 && g_matPlanN < 96; c++) {
            if (l >= nLods[c]) continue;
            for (int i = 0; i < nSec[c] && g_matPlanN < 96; i++) {
                g_matPlan[g_matPlanN].comp = g_fpCand[c].obj;
                g_matPlan[g_matPlanN].id   = i;
                g_matPlan[g_matPlanN].lod  = l;
                g_matPlan[g_matPlanN].cand = c;
                g_matPlanN++;
            }
        }
    Log("mat/auto: plan is %d hide(s), about %.0f s at %.1f s each, LOD 0 for "
        "every component FIRST. Every step names the component, section and "
        "LOD it hides. [Hands] MatStepMs changes the pace.",
        g_matPlanN, g_matPlanN * (g_matStepMs / 1000.0), g_matStepMs / 1000.0);
}


static void MatAutoStep(double now)
{
    if (g_matPlanAt > 0) MatRestoreAll("step end");
    if (g_matPlanAt >= g_matPlanN) {
        Log("mat/auto: SWEEP DONE - %d step(s), everything restored. If nothing "
            "ever changed on screen, ShowMaterialSection does not affect what "
            "this rig draws and route (d) is closed. The [took] / [DID NOT "
            "TAKE] marks say whether the flag itself ever moved, which is a "
            "different question and the one that says whose fault it is.",
            g_matPlanN);
        g_matAutoPhase = -1;
        return;
    }
    MatPlan* s = &g_matPlan[g_matPlanAt];
    const char* asset = (s->cand >= 0 && s->cand < g_fpCandN)
                      ? g_fpCand[s->cand].asset : "?";
    Log("mat/auto: >>> step %d/%d: component [%d] '%s', section %d, LOD %d is "
        "now HIDDEN <<<", g_matPlanAt + 1, g_matPlanN, s->cand, asset,
        s->id, s->lod);
    MatShowSection(s->comp, s->id, false, s->lod);
    g_matPlanAt++;
    g_matAutoUntil = now + g_matStepMs;
}


static void MatAutoTick()
{
    if (!g_matAutoCfg || g_matAutoPhase < 0 || !g_matCensusDone) return;
    const double now = MaimNowMs();
    if (g_matAutoPhase == 0) {
        if (!g_fnShowMatSection) {
            Log("mat/auto: no ShowMaterialSection native - nothing changed, and "
                "route (d) is untested rather than closed");
            g_matAutoPhase = -1;
            return;
        }
        MatAutoBuildPlan();
        if (!g_matPlanN) {
            Log("mat/auto: no component reports any section, so there is "
                "nothing to hide. That is a REFUSAL, not a negative result - "
                "route (d) stays untested on this rig.");
            g_matAutoPhase = -1;
            return;
        }
        g_matPlanAt = 0;
        g_matAutoPhase = 1;
        MatAutoStep(now);
        return;
    }
    if (now < g_matAutoUntil) return;
    MatAutoStep(now);
}


// `arms mat [census|hide <id> [lod]|show <id> [lod]|restore]`
static bool MatCommand(const char* args)
{
    char sub[16] = ""; int id = -1, lod = 0;
    const int got = sscanf(args, "%15s %d %d", sub, &id, &lod);
    if (got < 1 || !_stricmp(sub, "census")) { MatCensus("seam"); return true; }
    if (!_stricmp(sub, "restore")) { MatRestoreAll("seam"); return true; }
    const bool hide = !_stricmp(sub, "hide");
    if ((hide || !_stricmp(sub, "show")) && got >= 2) {
        uint8_t* comp = (g_armComp && LooksLikeObj(g_armComp)) ? g_armComp
                      : (g_fpCandN ? g_fpCand[0].obj : NULL);
        if (!comp) { Log("mat: no live component"); return true; }
        MatShowSection(comp, id, !hide, lod);
        return true;
    }
    Log("mat: census | hide <id> [lod] | show <id> [lod] | restore");
    return true;
}


// One automatic census per run once there is a rig to census. Read-only.
static void MatTick()
{
    if (g_matCensusDone || !g_matCensusCfg) return;
    if (!FpPawn()) return;
    const double t = MaimNowMs();
    if (t < g_matNextTryMs) return;
    g_matNextTryMs = t + 2000.0;
    if (++g_matTries > 5) {
        g_matCensusDone = true;
        Log("mat: 5 attempts and still no components - census skipped");
        return;
    }
    if (!g_fpCandN && FpPawn()) FpCollect();
    if (!g_fpCandN) return;
    g_matCensusDone = true;
    MatCensus("auto");
}


// ---------------------------------------------------------------------------
// The numpad cycler. Numpad 1 / 3 step back and forward through the plan one
// section at a time, Numpad 2 puts everything back. Built so the sections can
// be identified at a human pace instead of on a 5 s timer.
//
// LANE: the hotkey runs on the PRESENT thread and only sets a request. Every
// ShowMaterialSection dispatch happens here, on the script lane, the same
// split the console word uses. A ProcessEvent from the present thread is the
// kind of lane error this project has a rule about.
// ---------------------------------------------------------------------------
static void MatCycleTick()
{
    if (!g_matCycleReq) return;
    const int req = g_matCycleReq;
    g_matCycleReq = 0;
    if (!g_matPlanN) {
        MatResolve("cycle");
        if (!g_fpCandN && FpPawn()) FpCollect();
        MatAutoBuildPlan();
        if (!g_matPlanN) {
            Log("mat/cycle: nothing to cycle - no component reports a section");
            return;
        }
    }
    MatRestoreAll("cycle");
    if (req == 2) {
        g_matCycleAt = -1;
        Log("mat/cycle: ALL VISIBLE - nothing hidden");
        return;
    }
    g_matCycleAt += (req > 0 ? 1 : -1);
    if (g_matCycleAt >= g_matPlanN) g_matCycleAt = 0;
    if (g_matCycleAt < 0)           g_matCycleAt = g_matPlanN - 1;
    MatPlan* s = &g_matPlan[g_matCycleAt];
    const char* asset = (s->cand >= 0 && s->cand < g_fpCandN)
                      ? g_fpCand[s->cand].asset : "?";
    Log("mat/cycle: >>> [%d of %d] HIDDEN: component [%d] '%s', section %d, "
        "LOD %d <<<  (Numpad 3 next, Numpad 1 back, Numpad 2 all visible)",
        g_matCycleAt + 1, g_matPlanN, s->cand, asset, s->id, s->lod);
    MatShowSection(s->comp, s->id, false, s->lod);
}


static void MatTickAll()
{
    MatTick();
    MatAutoTick();
    MatCycleTick();
}
