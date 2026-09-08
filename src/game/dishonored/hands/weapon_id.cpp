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

// A signature BELONGS to the component of phase p when it is drawn in the
// baseline and not drawn at all while that component is hidden. Stated as a
// rule the reader can check, and it prints the counts that produced it so a
// weak result cannot read as a strong one.
static void WiReport(void)
{
    Log("wid: ================ WEAPON IDENTIFICATION ================");
    Log("wid: %d distinct skinned draw signature(s) over %ld draw(s), %ld that "
        "did not fit the table. Phase 0 is the baseline with nothing hidden; "
        "each later phase hides exactly one component. A signature drawn in the "
        "baseline and NOT drawn while component N is hidden belongs to that "
        "component - that is identification by making the picture move, not by "
        "palette size or draw order.",
        g_wiSigN, g_wiDraws, g_wiOverflow);

    for (int p = 1; p < g_wiPhaseN; p++) {
        const int c = g_wiCompOf[p];
        const char* asset = (c >= 0 && c < g_fpCandN) ? g_fpCand[c].asset : "?";
        int hits = 0;
        for (int i = 0; i < g_wiSigN; i++) {
            const uint32_t base = g_wiSeen[i][0];
            const uint32_t here = g_wiSeen[i][p];
            if (base == 0 || here != 0) continue;
            hits++;
            Log("wid:   '%s' OWNS signature %016llx - baseline %u draw(s), 0 "
                "while hidden | c6 x%u (%u bones) prim %u verts %u base %d "
                "start %u stride %u | vb %p ib %p vs %p decl %p",
                asset, (unsigned long long)g_wiSig[i].h, base,
                g_wiSig[i].bones * 3, g_wiSig[i].bones, g_wiSig[i].primCount,
                g_wiSig[i].numVerts, g_wiSig[i].baseVertex,
                g_wiSig[i].startIndex, g_wiSig[i].stride,
                g_wiSig[i].vb, g_wiSig[i].ib, g_wiSig[i].vs, g_wiSig[i].decl);
        }
        if (!hits)
            Log("wid:   '%s' owns NO signature that the baseline also drew. "
                "Either the component was not drawn at all in the baseline "
                "(not equipped, or off screen), or hiding it did not stop any "
                "draw - both of which are answers, and neither is an identity.",
                asset);
    }

    // The signatures nothing accounted for, stated rather than left out. A
    // report that only lists successes cannot be checked.
    int orphan = 0;
    for (int i = 0; i < g_wiSigN; i++) {
        if (g_wiSeen[i][0] == 0) continue;
        bool owned = false;
        for (int p = 1; p < g_wiPhaseN && !owned; p++)
            if (g_wiSeen[i][p] == 0) owned = true;
        if (!owned) orphan++;
    }
    Log("wid: %d baseline signature(s) survived EVERY hide - the player body, "
        "the world, and anything these components do not own. That number is "
        "the report's own control: if it were 0, the sweep would be hiding "
        "everything and the attributions above would mean nothing.", orphan);
    Log("wid: ======================================================");
}


// ---- the sweep, on the script lane -----------------------------------------

// Build the phase list: one phase per resolved component that reports at least
// one material section, plus the baseline. Uses the SAME candidate list and the
// SAME hide call the material route already proved in the headset.
static bool WiBuildPlan(void)
{
    g_wiPhaseN = 1;                       // phase 0 = baseline
    for (int c = 0; c < g_fpCandN && g_wiPhaseN < WI_MAX_PHASE; c++) {
        FpCand* k = &g_fpCand[c];
        if (!LooksLikeObj(k->obj)) continue;
        if (MatNumElements(k->obj) <= 0) continue;
        g_wiCompOf[g_wiPhaseN] = c;
        g_wiPhaseN++;
    }
    if (g_wiPhaseN <= 1) return false;
    Log("wid: sweep planned - baseline plus %d component(s), %.1f s each, about "
        "%.0f s total. Equip the weapon you care about BEFORE it runs; a "
        "component that is not drawn in the baseline cannot be identified.",
        g_wiPhaseN - 1, g_wiPhaseMs / 1000.0,
        g_wiPhaseN * (g_wiPhaseMs / 1000.0));
    return true;
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
        // Wait for the rig, then plan once. Bounded retries: a refusal that
        // repeats forever is noise rather than evidence.
        if (!g_fpCandN) {
            if (++g_wiTries > 600) {
                g_wiDone = true;
                Log("wid: no first-person components ever resolved, so there is "
                    "nothing to correlate draws against. The sweep is off.");
            }
            return;
        }
        if (!WiBuildPlan()) {
            g_wiDone = true;
            Log("wid: no component reported a material section, so no hide can "
                "be made and no draw can be attributed. The sweep is off.");
            return;
        }
        g_wiStep  = 0;
        g_wiPhase = 0;                    // the baseline starts collecting NOW
        g_wiUntil = now + g_wiPhaseMs;
        Log("wid: >>> phase 0/%d: BASELINE, nothing hidden <<<", g_wiPhaseN - 1);
        return;
    }

    if (now < g_wiUntil) return;

    // End the phase that just finished: restore whatever it hid.
    if (g_wiHiding) { MatRestoreAll("wid phase end"); g_wiHiding = false; }

    g_wiStep++;
    if (g_wiStep >= g_wiPhaseN) {
        g_wiPhase = -1;
        g_wiDone  = true;
        WiReport();
        return;
    }

    const int c = g_wiCompOf[g_wiStep];
    FpCand* k = (c >= 0 && c < g_fpCandN) ? &g_fpCand[c] : NULL;
    if (!k || !LooksLikeObj(k->obj)) {
        // Skip a component that has gone away rather than hiding nothing and
        // recording the result as if it had been hidden.
        Log("wid: phase %d/%d SKIPPED - component [%d] is no longer live, so a "
            "phase here would record 'nothing vanished' for a hide that never "
            "happened.", g_wiStep, g_wiPhaseN - 1, c);
        g_wiUntil = now;                  // move straight on
        return;
    }
    const int nsec = MatNumElements(k->obj);
    bool hid = false;
    for (int i = 0; i < nsec; i++)
        if (MatShowSection(k->obj, i, false, 0)) hid = true;
    g_wiHiding = hid;
    g_wiPhase  = g_wiStep;
    g_wiUntil  = now + g_wiPhaseMs;
    Log("wid: >>> phase %d/%d: '%s' HIDDEN (%d section(s), hide %s) <<<",
        g_wiStep, g_wiPhaseN - 1, k->asset, nsec,
        hid ? "took" : "DID NOT TAKE - this phase cannot attribute anything");
}
