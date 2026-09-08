// game/dishonored/hands/weapon_id.cpp - included by src/mod/dishonoredvr.cpp
// (unity build). See state chunk 56 for what this is and why it exists.
//
// VR-33 W1: joins the SCRIPT side's named components to the DRAW side's
// signatures by hiding one component at a time and recording which draws stop
// arriving. Identification by making the picture move, not by palette size.

// ---- the identity hash ------------------------------------------------------

static inline uint64_t WiMix(uint64_t h, uint64_t v)
{
    h ^= v; h *= 1099511628211ull; return h;
}

// The signature of one draw. Everything that distinguishes geometry and pass:
// the buffers, the declaration, the shader, the stride and the draw's own
// range. Two draws differing in any of these are different work.
static uint64_t WiHash(const WiSig* s)
{
    uint64_t h = 1469598103934665603ull;
    h = WiMix(h, (uint64_t)(uintptr_t)s->vb);
    h = WiMix(h, (uint64_t)(uintptr_t)s->ib);
    h = WiMix(h, (uint64_t)(uintptr_t)s->vs);
    h = WiMix(h, (uint64_t)(uintptr_t)s->decl);
    h = WiMix(h, (uint64_t)s->stride);
    h = WiMix(h, (uint64_t)s->primCount);
    h = WiMix(h, (uint64_t)s->numVerts);
    h = WiMix(h, (uint64_t)s->startIndex);
    h = WiMix(h, (uint64_t)(int64_t)s->baseVertex);
    h = WiMix(h, (uint64_t)s->bones);
    return h;
}


// Record one skinned draw against the phase currently being collected.
//
// D3D OBJECT RULE (CLAUDE.md): GetStreamSource / GetIndices /
// GetVertexDeclaration / GetVertexShader all AddRef. Each is released
// IMMEDIATELY and only the pointer VALUE is kept as an identity token. Nothing
// here dereferences one and no reference outlives this call.
static void WiNoteDraw(IDirect3DDevice9* dev, INT baseVertex, UINT minIndex,
                       UINT numVertices, UINT startIndex, UINT primCount)
{
    const LONG phase = g_wiPhase;
    if (!g_wiOn || phase < 0 || phase >= WI_MAX_PHASE || !dev) return;
    (void)minIndex;

    WiSig s;
    memset(&s, 0, sizeof(s));
    s.baseVertex = baseVertex;
    s.numVerts   = numVertices;
    s.startIndex = startIndex;
    s.primCount  = primCount;
    s.bones      = g_dcPendingBones;

    IDirect3DVertexBuffer9* vb = NULL; UINT off = 0, stride = 0;
    if (SUCCEEDED(dev->GetStreamSource(0, &vb, &off, &stride))) {
        s.vb = vb; s.stride = stride;
        if (vb) vb->Release();
    }
    IDirect3DIndexBuffer9* ib = NULL;
    if (SUCCEEDED(dev->GetIndices(&ib))) { s.ib = ib; if (ib) ib->Release(); }
    IDirect3DVertexDeclaration9* de = NULL;
    if (SUCCEEDED(dev->GetVertexDeclaration(&de))) { s.decl = de; if (de) de->Release(); }
    IDirect3DVertexShader9* vs = NULL;
    if (SUCCEEDED(dev->GetVertexShader(&vs))) { s.vs = vs; if (vs) vs->Release(); }

    s.h = WiHash(&s);
    for (int i = 0; i < g_wiSigN; i++)
        if (g_wiSig[i].h == s.h) { g_wiSeen[i][phase]++; InterlockedIncrement(&g_wiDraws); return; }
    if (g_wiSigN >= WI_MAX_SIG) { InterlockedIncrement(&g_wiOverflow); return; }
    g_wiSig[g_wiSigN] = s;
    memset(g_wiSeen[g_wiSigN], 0, sizeof(g_wiSeen[0]));
    g_wiSeen[g_wiSigN][phase] = 1;
    g_wiSigN++;
    InterlockedIncrement(&g_wiDraws);
}


// ---- the report -------------------------------------------------------------

// Phase layout. Phase 0 is the baseline. Component n then occupies phases
// 1 + n*4 .. 4 + n*4 as HIDE, SHOW, HIDE, SHOW.
static inline int WiHidePhase(int comp, int rep) { return 1 + comp*WI_PER_COMP + rep*2; }
static inline int WiShowPhase(int comp, int rep) { return 2 + comp*WI_PER_COMP + rep*2; }

// A signature BELONGS to a component when the picture MOVED with it, twice:
// drawn in the baseline, absent through BOTH of that component's hide phases,
// and back through BOTH of its show phases. A single disappearance is not
// evidence - a camera move, an NPC leaving frame or an LOD switch all produce
// one - and this project has already paid for reading one as an identity.
static bool WiOwns(int sig, int comp, uint32_t* h0, uint32_t* s0,
                   uint32_t* h1, uint32_t* s1)
{
    *h0 = g_wiSeen[sig][WiHidePhase(comp, 0)];
    *s0 = g_wiSeen[sig][WiShowPhase(comp, 0)];
    *h1 = g_wiSeen[sig][WiHidePhase(comp, 1)];
    *s1 = g_wiSeen[sig][WiShowPhase(comp, 1)];
    return g_wiSeen[sig][0] != 0 && *h0 == 0 && *h1 == 0 && *s0 != 0 && *s1 != 0;
}

static void WiReport(void)
{
    const int comps = (g_wiPhaseN - 1) / WI_PER_COMP;

    Log("wid: ================ WEAPON IDENTIFICATION ================");

    // THE VOID CHECK FIRST, so nothing below can be read as an answer when it
    // is not one. An overflowed table cannot record a signature it has not
    // already seen, which means a "0 draws while hidden" can equally mean "the
    // table was full and refused to count it". That is not a footnote.
    if (g_wiOverflow) {
        Log("wid: *** THIS REPORT IS VOID *** - %ld draw(s) did not fit the "
            "%d-signature table, so an absence below can mean 'the table was "
            "full and refused to record it' just as well as 'the component was "
            "hidden'. No attribution is printed. %d signature(s) were held over "
            "%ld counted draw(s). Raise WI_MAX_SIG, or run the sweep somewhere "
            "with fewer skinned actors in view - a quiet room, weapon drawn, "
            "facing a wall.",
            g_wiOverflow, WI_MAX_SIG, g_wiSigN, g_wiDraws);
        Log("wid: ======================================================");
        return;
    }

    int bad = 0;
    for (int p = 1; p < g_wiPhaseN; p++) if (g_wiPhaseBad[p]) bad++;
    if (bad)
        Log("wid: WARNING - %d phase(s) did not do what they said: a hide or a "
            "restore call was refused. Any component whose phases are among "
            "them is reported below as UNTESTED rather than as owning nothing.",
            bad);

    Log("wid: %d distinct skinned draw signature(s) over %ld draw(s), 0 that "
        "did not fit the table. Phase 0 is the baseline. Each component then "
        "gets HIDE, SHOW, HIDE, SHOW. A signature is only attributed when it "
        "is drawn in the baseline, absent through BOTH hides and back through "
        "BOTH shows - a prediction made four times, which is what separates a "
        "hide from a camera move.",
        g_wiSigN, g_wiDraws);

    for (int c = 0; c < comps; c++) {
        const int k = g_wiCompOf[WiHidePhase(c, 0)];
        const char* asset = (k >= 0 && k < g_fpCandN) ? g_fpCand[k].asset : "?";
        bool phasesOk = true;
        for (int r = 0; r < 2; r++)
            if (g_wiPhaseBad[WiHidePhase(c, r)] || g_wiPhaseBad[WiShowPhase(c, r)])
                phasesOk = false;
        if (!phasesOk) {
            Log("wid:   '%s' UNTESTED - one of its four phases was refused, so "
                "an absent draw here would be evidence of nothing.", asset);
            continue;
        }

        int hits = 0, once = 0;
        for (int i2 = 0; i2 < g_wiSigN; i2++) {
            uint32_t h0, s0, h1, s1;
            if (WiOwns(i2, c, &h0, &s0, &h1, &s1)) {
                hits++;
                Log("wid:   '%s' OWNS signature %016llx - baseline %u | hide 0 "
                    "| show %u | hide 0 | show %u | c6 x%u (%u bones) prim %u "
                    "verts %u base %d start %u stride %u | vb %p ib %p vs %p "
                    "decl %p",
                    asset, (unsigned long long)g_wiSig[i2].h, g_wiSeen[i2][0],
                    s0, s1, g_wiSig[i2].bones * 3, g_wiSig[i2].bones,
                    g_wiSig[i2].primCount, g_wiSig[i2].numVerts,
                    g_wiSig[i2].baseVertex, g_wiSig[i2].startIndex,
                    g_wiSig[i2].stride, g_wiSig[i2].vb, g_wiSig[i2].ib,
                    g_wiSig[i2].vs, g_wiSig[i2].decl);
            } else if (g_wiSeen[i2][0] && (h0 == 0 || h1 == 0)) {
                once++;
            }
        }
        Log("wid:   '%s': %d signature(s) owned, %d that vanished on ONE hide "
            "but not the other and were REJECTED. Those rejects are the "
            "instrument working: they are what a single-cycle sweep would have "
            "reported as owned.",
            asset, hits, once);
        if (!hits)
            Log("wid:   '%s' owns NO signature. Either it was not drawn in the "
                "baseline (not equipped, or off screen), or hiding it stopped "
                "nothing that came back. Both are answers; neither is an "
                "identity.", asset);
    }

    // The control. Stated rather than left out: a report that only lists
    // successes cannot be checked.
    int orphan = 0, ambiguous = 0;
    for (int i2 = 0; i2 < g_wiSigN; i2++) {
        if (g_wiSeen[i2][0] == 0) continue;
        int owners = 0;
        for (int c = 0; c < comps; c++) {
            uint32_t h0, s0, h1, s1;
            if (WiOwns(i2, c, &h0, &s0, &h1, &s1)) owners++;
        }
        if (owners == 0) orphan++;
        else if (owners > 1) ambiguous++;
    }
    Log("wid: %d baseline signature(s) survived EVERY hide - the player body, "
        "the world, and anything these components do not own. That number is "
        "the report's own control: if it were 0 the sweep would be hiding "
        "everything and the attributions above would mean nothing. %d "
        "signature(s) satisfied MORE THAN ONE component and are not an "
        "identity for any of them - a nonzero count there means the view was "
        "moving during the sweep and the whole run should be repeated standing "
        "still.", orphan, ambiguous);
    Log("wid: ======================================================");
}


// ---- the sweep, on the script lane -----------------------------------------

// Build the phase list: one phase per resolved component that reports at least
// one material section, plus the baseline. Uses the SAME candidate list and the
// SAME hide call the material route already proved in the headset.
static bool WiBuildPlan(void)
{
    g_wiPhaseN = 1;                       // phase 0 = baseline
    int comps = 0;
    for (int c = 0; c < g_fpCandN && comps < WI_MAX_COMP; c++) {
        FpCand* k = &g_fpCand[c];
        if (!LooksLikeObj(k->obj)) continue;
        if (MatNumElements(k->obj) <= 0) continue;
        for (int r = 0; r < WI_PER_COMP; r++) {
            g_wiCompOf[g_wiPhaseN] = c;
            g_wiHidden[g_wiPhaseN] = (r % 2) == 0;   // hide, show, hide, show
            g_wiPhaseN++;
        }
        comps++;
    }
    if (!comps) return false;
    Log("wid: sweep planned - baseline plus %d component(s) x 4 phases (hide, "
        "show, hide, show), %.1f s each, about %.0f s total. STAND STILL and "
        "keep the weapon in view for the whole sweep: the test is 'this draw "
        "stops and comes back with the hide', and a view that changes under it "
        "produces the same signal.",
        comps, g_wiPhaseMs / 1000.0, g_wiPhaseN * (g_wiPhaseMs / 1000.0));
    for (int c = 0; c < comps; c++) {
        const int k = g_wiCompOf[WiHidePhase(c, 0)];
        Log("wid:   component %d = '%s' (%s)", c, g_fpCand[k].asset,
            g_fpCand[k].name);
    }
    return true;
}


// Is the candidate list ready to be planned against? Two conditions, both of
// which the first run failed silently:
//
//   - it must contain something that is NOT the player body. A plan that can
//     only hide Skm_Player cannot identify a crossbow, and running it anyway
//     produces a confident report about the wrong question;
//   - it must have STOPPED CHANGING. FpCollect rebuilds it live as the player
//     equips and stows, and a plan taken mid-rebuild is a plan against a list
//     that no longer exists by the second phase.
static bool WiAssetsReady(double now)
{
    int weapons = 0;
    for (int c = 0; c < g_fpCandN; c++) {
        FpCand* k = &g_fpCand[c];
        if (!LooksLikeObj(k->obj)) continue;
        if (FpIsViewModel(k)) continue;                 // pPlayerMesh, the body
        if (MatNumElements(k->obj) <= 0) continue;
        weapons++;
    }

    void* first = g_fpCandN ? (void*)g_fpCand[0].obj : NULL;
    if (g_wiSeenN != g_fpCandN || g_wiSeenFirst != first) {
        g_wiSeenN = g_fpCandN; g_wiSeenFirst = first; g_wiStable = now;
        g_wiWaitSaid = false;
    }

    if (!weapons) {
        if (!g_wiWaitSaid) {
            g_wiWaitSaid = true;
            Log("wid: WAITING - %d component(s) resolved and none of them is a "
                "weapon (the only hideable thing is the player body). The sweep "
                "REFUSES to run against a list that cannot contain the answer: "
                "that is exactly what produced the false positive on "
                "2026-09-07. Draw a weapon and it will start on its own.",
                g_fpCandN);
            for (int c = 0; c < g_fpCandN; c++)
                Log("wid:   have [%d] '%s' asset=%s sections=%d", c,
                    g_fpCand[c].name, g_fpCand[c].asset,
                    LooksLikeObj(g_fpCand[c].obj) ? MatNumElements(g_fpCand[c].obj) : -1);
        }
        return false;
    }

    if (now - g_wiStable < (double)WI_SETTLE_MS) {
        if (!g_wiWaitSaid) {
            g_wiWaitSaid = true;
            Log("wid: %d weapon component(s) resolved - waiting %.1f s for the "
                "component list to settle before planning. It is rebuilt live "
                "as things are equipped, and a plan taken mid-rebuild names "
                "components that are gone by the second phase.",
                weapons, (double)WI_SETTLE_MS / 1000.0);
        }
        return false;
    }
    return true;
}


// THE REPORT MUST NOT NEED ONE MORE SCRIPT TICK.
//
// The 2026-09-07 run swept perfectly - all sixteen phases, every hide and
// every restore verified against HiddenMaterials - and then printed NOTHING.
// WiTick runs on the script lane (ApplyHandToMesh), the final phase ends by
// its deadline passing, and the report therefore needed one more tick of that
// lane. The lane went quiet half a second later ("viewinject: script camera
// writes went stale") and the whole 26-second sweep was lost.
//
// So the terminal step is split out and driven from BOTH lanes. It touches no
// engine object and calls no native - it reads counters and logs - so the
// present thread may run it. The last phase is a SHOW phase by construction,
// which is why there is nothing here to restore: only the script lane calls
// ProcessEvent, and it still does. The interlock makes whichever lane arrives
// first the one that prints, once.
static volatile LONG g_wiReported = 0;

static void WiFinish(void)
{
    if (InterlockedExchange(&g_wiReported, 1)) return;
    g_wiPhase = -1;
    g_wiDone  = true;
    WiReport();
}

// The present lane's half. Fires on the same deadline the script lane would
// have used, so a report is produced even if the script lane never ticks
// again.
static void WiFinishTick(void)
{
    if (!g_wiOn || g_wiDone || g_wiReported) return;
    if (g_wiStep < 0 || g_wiStep < g_wiPhaseN - 1) return;
    if (MaimNowMs() < g_wiUntil) return;
    Log("wid: the sweep's last phase is over and the report is being printed "
        "from the PRESENT thread. The script lane owns the hides, but it can "
        "stop between the last phase and the report - which is exactly how the "
        "2026-09-07 sweep was lost after running correctly end to end.");
    WiFinish();
}


static void WiTick(void)
{
    if (!g_wiOn || g_wiDone) return;

    const double now = MaimNowMs();

    if (g_wiStep < 0) {
        // TWO INSTRUMENTS MUST NOT DRIVE ONE LEVER. The material cycler hides
        // and restores through the same calls this sweep uses, so with both
        // armed a numpad press would restore a component mid-phase and the
        // sweep would record "nothing vanished" for a hide that was undone
        // under it. One feature driving two things at once has cost this
        // project a session already (7099c3b0); refuse instead.
        if (g_matCycleCfg) {
            g_wiDone = true;
            Log("wid: REFUSED - the material cycler ([Hands] MatCycle) is armed "
                "and drives the same hide/restore calls this sweep needs. A "
                "numpad press mid-phase would restore a component under the "
                "sweep and the result would read as 'this component owns no "
                "draws'. Set MatCycle=0 to run the identifier.");
            return;
        }
        // Wait for the rig, then for the WEAPONS, then plan once. Bounded:
        // a refusal that repeats forever is noise rather than evidence, but
        // the bound is long because a weapon is drawn when the player chooses.
        if (!g_fpCandN || !WiAssetsReady(now)) {
            if (++g_wiTries > 36000) {          // ~10 min of ticks
                g_wiDone = true;
                Log("wid: GIVING UP - no weapon component ever resolved, so "
                    "there was never anything to correlate draws against. This "
                    "is not a failure of the sweep: nothing was equipped, or "
                    "the component scan never reached it. Restart with a weapon "
                    "drawn to try again.");
            }
            return;
        }
        g_wiStep  = 0;
        g_wiPhase = 0;                    // the baseline starts collecting NOW
        if (!WiBuildPlan()) {
            g_wiDone = true; g_wiPhase = -1;
            Log("wid: no component reported a material section, so no hide can "
                "be made and no draw can be attributed. The sweep is off.");
            return;
        }
        g_wiUntil = now + g_wiPhaseMs;
        Log("wid: >>> phase 0/%d: BASELINE, nothing hidden <<<", g_wiPhaseN - 1);
        return;
    }

    if (now < g_wiUntil) return;

    // End the phase that just finished: restore whatever it hid.
    if (g_wiHiding) { MatRestoreAll("wid phase end"); g_wiHiding = false; }

    g_wiStep++;
    if (g_wiStep >= g_wiPhaseN) {
        WiFinish();
        return;
    }

    const int c = g_wiCompOf[g_wiStep];
    FpCand* k = (c >= 0 && c < g_fpCandN) ? &g_fpCand[c] : NULL;
    if (!k || !LooksLikeObj(k->obj)) {
        // A component that has gone away mid-sweep poisons its own phases and
        // must say so, not record "nothing vanished" for a hide that never
        // happened.
        g_wiPhaseBad[g_wiStep] = true;
        Log("wid: phase %d/%d POISONED - component [%d] is no longer live, so "
            "this phase records an absence that no hide produced.",
            g_wiStep, g_wiPhaseN - 1, c);
        g_wiPhase = g_wiStep;
        g_wiUntil = now + g_wiPhaseMs;
        return;
    }

    // A HIDE phase calls the native; a SHOW phase calls NOTHING. The restore
    // above (MatRestoreAll) has already put the component back, and it does so
    // respecting what the game itself had hidden before we arrived - forcing
    // every section visible here would change the picture rather than restore
    // it, and the show phase would then be testing our own write.
    //
    // ACCEPTANCE IS THE FLAG, NOT THE CALL. MatShowSection returns true when
    // the native was CALLED, which is not the same as the section being
    // hidden: it logs "[DID NOT TAKE]" and still returns true. So the phase is
    // judged by reading HiddenMaterials back afterwards. A phase whose flags
    // do not match what it asked for is POISONED, and its component is
    // reported UNTESTED rather than as owning nothing.
    const bool wantHidden = g_wiHidden[g_wiStep];
    const int  nsec = MatNumElements(k->obj);
    if (wantHidden) {
        for (int i2 = 0; i2 < nsec; i2++) MatShowSection(k->obj, i2, false, 0);
        g_wiHiding = true;
    }
    int got = 0, unreadable = 0;
    for (int i2 = 0; i2 < nsec; i2++) {
        const int f = MatHiddenFlag(k->obj, i2, 0);
        if (f < 0) unreadable++;
        else if ((f != 0) == wantHidden) got++;
    }
    const bool acted = (nsec > 0) && (got == nsec);
    g_wiPhaseBad[g_wiStep] = !acted;
    g_wiPhase  = g_wiStep;
    g_wiUntil  = now + g_wiPhaseMs;
    Log("wid: >>> phase %d/%d: '%s' %s - %d/%d section(s) read back as %s"
        "%s <<<",
        g_wiStep, g_wiPhaseN - 1, k->asset,
        wantHidden ? "HIDE" : "SHOW (restored, nothing called)",
        got, nsec, wantHidden ? "hidden" : "visible",
        unreadable
            ? ", and the HiddenMaterials array was UNREADABLE for some of them - "
              "this phase is POISONED and its component will be reported UNTESTED"
            : (acted ? ", as asked"
                     : " - this phase is POISONED and its component will be "
                       "reported UNTESTED"));
}
