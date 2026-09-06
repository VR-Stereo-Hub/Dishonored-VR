// game/dishonored/hands/draw_census.cpp - included by src/mod/dishonoredvr.cpp
// (unity build).
//
// ===========================================================================
// VR-31 route (b), STEP 1: identify the individual DRAWS, not the upload sizes
//
// The question: are the first-person arms and hands separate geometry that the
// renderer submits separately, or one inseparable draw?
//
// Everything measured so far narrows it to this. Route (a), per-bone
// visibility, is closed - no BoneVisibilityStates exists on any class in
// GObjects. Route (d), material sections, is closed for hands - Skm_Player has
// exactly ONE section, confirmed in the headset by walking the material
// cycler: one press removes both arms and both hands together. What is left is
// the geometry the renderer actually submits, and nothing in this codebase has
// looked at that yet.
//
// WHY UPLOAD SIZE CANNOT ANSWER IT, which is why this instrument exists.
// The inherited knowledge is "bone palettes arrive at c6, 3 registers per
// bone; x144 = arms, x36 = sword, x204 = NPC", and [VRHands] HideGameArms
// hides by matching those SIZES. Size cannot settle an arm/hand split:
//
//   - two chunks can carry the same bone count, so the same upload size;
//   - several draws can reuse one upload, so one palette is not one draw;
//   - a size list built by observation (HideSizes) contains only what someone
//     has already happened to see.
//
// So this censuses the DRAW. Every DrawIndexedPrimitive that consumes a c6
// palette is identified by what the renderer was actually holding: the stream-0
// vertex buffer and its stride, the index buffer, the vertex declaration, the
// vertex shader, and the draw's own index range and primitive count. Two draws
// differing in any of those are different geometry, whatever their palette
// sizes agree on.
//
// LANE: DrawIndexedPrimitive runs on whichever thread the renderer draws on,
// which is not necessarily the present thread. Nothing here writes engine
// memory or dispatches ProcessEvent. The cycler's hotkeys only post an index
// and the detour reads it.
//
// D3D OBJECT RULE (CLAUDE.md: never take a reference to an engine D3D object
// inside a detour). GetStreamSource / GetIndices / GetVertexDeclaration /
// GetVertexShader all AddRef. Each is released IMMEDIATELY and only the
// pointer VALUE is kept, as an identity token. Nothing here dereferences one,
// and no reference outlives the call.
//
// COST: those Get calls are not free and there are thousands of draws a frame,
// so they run ONLY when a palette upload is pending for this draw and the
// census is armed. Switched off, this costs one predictable branch per draw.
//
// RACE, stated rather than papered over: the table is written by the render
// thread and read by the reporter on the game thread. A new row is filled
// COMPLETELY before g_dcN is incremented, so a reader never sees a half-built
// row; the counters can be a few draws stale in a report, which does not
// matter for identification. This is a diagnostic, not a load-bearing path.
// ===========================================================================

// 41.2 SECOND PASS, from review. The first version compared vb/ib/decl/vs,
// stride and the index range - and concluded from three matching rows that the
// arms and hands are ONE geometry drawn three times. That conclusion was not
// yet earned: the comparison omitted BaseVertexIndex, the stream BYTE OFFSET
// (GetStreamSource returned it and the code threw it away) and the primitive
// type. Two draws can share a vertex buffer, an index buffer and a recorded
// index range and still address different vertices through a different base or
// a different stream offset. All three are part of the identity now, so if the
// rows still collapse to one signature the "one geometry, three passes" reading
// is measured rather than assumed.
static bool DcSame(const DcDraw* a, const DcDraw* b)
{
    return a->vb == b->vb && a->ib == b->ib && a->decl == b->decl && a->vs == b->vs &&
           a->stride == b->stride && a->streamOff == b->streamOff &&
           a->baseVertex == b->baseVertex && a->primType == b->primType &&
           a->minIndex == b->minIndex &&
           a->numVerts == b->numVerts && a->startIndex == b->startIndex &&
           a->prims == b->prims && a->bones == b->bones;
}


// The same geometry, ignoring WHICH SHADER drew it.
//
// This is the distinction the first walk was missing, and it is why the tester
// could hide row 0 or row 1 and see nothing change. Rows 0-2 are one mesh drawn
// three times by three different vertex shaders; suppress one and the other two
// still draw the geometry. Only the pass whose CONTRIBUTION is visible on its
// own - the lighting one, which left the arm undersides black - showed any
// effect at all.
//
// So the cycler steps by MESH, not by pass, and hides every pass over it. That
// is also exactly what route (b) will have to do: an index list applied to one
// pass and not its siblings would show through the others.
static bool DcSameGeom(const DcDraw* a, const DcDraw* b)
{
    return a->vb == b->vb && a->ib == b->ib && a->stride == b->stride &&
           a->streamOff == b->streamOff && a->baseVertex == b->baseVertex &&
           a->primType == b->primType && a->minIndex == b->minIndex &&
           a->numVerts == b->numVerts && a->startIndex == b->startIndex &&
           a->prims == b->prims;
}


// Ask the DECLARATION whether this draw is skinned at all. Done once per new
// row - the call walks an element array and is far too costly per draw.
// Released immediately, like every other D3D object this file touches.
static void DcReadDecl(DcDraw* r)
{
    r->declRead = 1;
    r->hasBlendIdx = 0; r->hasBlendWt = 0;
    if (!r->decl) return;
    IDirect3DVertexDeclaration9* d = (IDirect3DVertexDeclaration9*)r->decl;
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH];
    UINT n = 0;
    if (FAILED(d->GetDeclaration(el, &n))) return;
    if (n > MAXD3DDECLLENGTH) n = MAXD3DDECLLENGTH;
    for (UINT i = 0; i < n; i++) {
        if (el[i].Usage == D3DDECLUSAGE_BLENDINDICES) r->hasBlendIdx = 1;
        if (el[i].Usage == D3DDECLUSAGE_BLENDWEIGHT) r->hasBlendWt = 1;
    }
}


// A c6 upload has just landed. Called from the SetVertexShaderConstantF path.
// The register range is filtered only for PLAUSIBILITY as a bone palette
// (3..250 registers), never against the historical size list - the whole point
// is to find sizes nobody has written down.
static void DcNotePalette(UINT count)
{
    if (!g_dcOn) return;
    if (count < 3 || count > 250) return;
    g_dcPendingBones = count / 3;
    g_dcPendingSerial = 1;
    g_dcSinceUpload = 0;          // the reuse window reopens on every c6 write
}


static void DcReport(const char* why)
{
    const int n = g_dcN;
    Log("dc: ==== draw census (%s), %d distinct palette-fed draw(s) ====", why, n);
    if (!n) {
        Log("dc:   NOTHING SEEN. Either no skeletal draw ran in this window, or "
            "the c6 palette is not what feeds them on this build. That is a "
            "real answer and it falsifies the premise this route rests on - it "
            "is not the instrument being off, and `dc status` says which.");
        return;
    }
    for (int i = 0; i < n; i++) {
        DcDraw* d = &g_dcDraw[i];
        Log("dc:   [%2d]%s bones=%-3u skin=%c%c prims=%-6u verts=%-6u (min %u, "
            "start %u, base %d, prim %u) | vb=%p +%u stride=%u ib=%p decl=%p "
            "vs=%p | ord+%u | hits=%u (prev %u) in %u frame(s)",
            i, d->hits ? " LIVE" : "     ", d->bones,
            d->hasBlendIdx ? 'I' : '-', d->hasBlendWt ? 'W' : '-',
            d->prims, d->numVerts, d->minIndex, d->startIndex, d->baseVertex,
            d->primType, d->vb, d->streamOff, d->stride, d->ib, d->decl, d->vs,
            d->ordAfterUpload, d->hits, d->hitsPrev, d->frames);
    }
    Log("dc:   %u single-matrix draw(s) skipped this window (bones==1, never a "
        "skin palette). They are excluded on purpose: they used to fill this "
        "table, and a full table silently disabled suppression for any draw "
        "that arrived afterwards.", g_dcSkippedFlat);
    g_dcSkippedFlat = 0;
    if (n >= DC_MAX)
        Log("dc:   TABLE FULL at %d rows - later distinct draws were not "
            "recorded, so this list is a prefix and not the whole frame.", DC_MAX);
    Log("dc: ==== end. Two rows sharing a bone count but differing in vb/ib or "
        "index range are SEPARATE geometry that a size-based hide cannot tell "
        "apart - which is the question. Numpad 6 hides one row at a time. ====");
    Log("dc:   skin=IW means the DECLARATION carries blend indices and weights - "
        "the only evidence here that a row is really skinned. bones>=2 alone is "
        "a plausibility filter and a skin=-- row is not a skeletal draw.");
    for (int i = 0; i < n; i++) { g_dcDraw[i].hitsPrev = g_dcDraw[i].hits; g_dcDraw[i].hits = 0; }
}


// Is this draw bound to the locked mesh? Buffer identity only, and only while
// the lock is armed - otherwise this would run four COM calls on every draw in
// the frame.
static bool DcIsLocked(IDirect3DDevice9* self, bool needIb)
{
    void* wantVb = g_dcHideVb;
    if (!wantVb) return false;
    IDirect3DVertexBuffer9* vb = NULL; UINT off = 0, stride = 0;
    if (FAILED(self->GetStreamSource(0, &vb, &off, &stride)) || !vb) return false;
    const bool vbOk = ((void*)vb == wantVb);
    vb->Release();
    if (!vbOk) return false;
    if (!needIb) return true;
    void* wantIb = g_dcHideIb;
    if (!wantIb) return true;
    IDirect3DIndexBuffer9* ib = NULL;
    if (FAILED(self->GetIndices(&ib)) || !ib) return false;
    const bool ibOk = ((void*)ib == wantIb);
    ib->Release();
    return ibOk;
}


// The NON-indexed entry point. Hooked only so a mesh lock is not quietly
// partial: a pass that draws without an index buffer would otherwise survive
// every suppression this file performs and look like a rendering mystery.
static HRESULT __stdcall DcDrawPrim(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
                                    UINT startVertex, UINT primCount)
{
    if (g_dcOn && self && g_dcHideVb && DcIsLocked(self, false)) {
        g_dcDropPrim++;
        return D3D_OK;
    }
    return dvr::frame::orig_draw_prim(self, type, startVertex, primCount);
}


// The detour. Everything expensive sits behind the pending-palette gate.
static HRESULT __stdcall DcDrawIndexed(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
                                       INT baseVertex, UINT minIndex, UINT numVertices,
                                       UINT startIndex, UINT primCount)
{
    // MESH LOCK, ahead of the palette gate on purpose.
    //
    // Dropping the three palette-fed passes over the arm mesh left a faint arm
    // still drawn. That is proof the palette-gated census does not see every
    // draw of this geometry - a pass whose bone constants land outside the
    // reuse window, or at a register other than c6, is invisible to it.
    //
    // Buffer identity does not care. Any draw bound to this vertex and index
    // buffer IS this mesh, whatever the constants did. g_dcUnseen counts the
    // drops that happened while NO palette was current: that number is the
    // direct measure of what the census was blind to, and if it is 0 the
    // residual arm is not this mesh at all and the hunt moves elsewhere.
    if (g_dcOn && self && g_dcHideVb && DcIsLocked(self, true)) {
        g_dcDropIdx++;
        if (g_dcSinceUpload >= DC_REUSE_WINDOW || !g_dcPendingBones) g_dcUnseen++;
        const int hs = g_dcHideSlice;
        // SLICE MODE. The mesh is a triangle list (primitive type 4, measured),
        // so 3 indices per triangle and any contiguous run of triangles can be
        // drawn on its own by shifting startIndex and shrinking primCount. That
        // means the mesh can be cut up WITHOUT reading, capturing or replacing
        // a single index: the game's own buffer is still bound, we just ask for
        // part of it. Two calls draw everything except the hidden slice.
        //
        // This is the cheap way to find out where the hands are in the triangle
        // order. Once that is known, the same mechanism keeps the hand range
        // and drops the arm range permanently - the index-buffer filter, with
        // no buffer to build.
        const uint32_t mask = g_dcSliceMask;
        const uint32_t hidden = mask | ((hs >= 0 && hs < DC_SLICES) ? (1u << hs) : 0u);
        if (hidden && primCount >= DC_SLICES && type == D3DPT_TRIANGLELIST) {
            // Draw the visible slices as RUNS, so a mask with a few marks costs
            // a few calls rather than one per slice.
            HRESULT hr = D3D_OK;
            uint32_t i = 0;
            while (i < DC_SLICES) {
                if (hidden & (1u << i)) { i++; continue; }
                uint32_t j = i;
                while (j < DC_SLICES && !(hidden & (1u << j))) j++;
                const UINT lo = (UINT)((uint64_t)primCount * i / DC_SLICES);
                const UINT hi = (UINT)((uint64_t)primCount * j / DC_SLICES);
                if (hi > lo)
                    hr = dvr::frame::orig_draw_indexed(self, type, baseVertex, minIndex,
                                                       numVertices, startIndex + lo * 3,
                                                       hi - lo);
                i = j;
            }
            return hr;
        }
        return D3D_OK;      // whole mesh
    }

    // A palette is CURRENT STATE, not a one-shot. D3D9 constants persist until
    // overwritten, so several draws can share one upload - the first version
    // cleared the pending flag on the first draw and silently censused only
    // that one. Now the palette stays current until the next c6 write, and each
    // draw records how many draws it sits after the upload, so reuse is visible
    // instead of invisible. The window is bounded so world geometry drawn long
    // after a palette does not flood the table.
    if (!g_dcOn || !self || g_dcSinceUpload >= DC_REUSE_WINDOW || !g_dcPendingBones)
        return dvr::frame::orig_draw_indexed(self, type, baseVertex, minIndex,
                                             numVertices, startIndex, primCount);

    const uint32_t bones = g_dcPendingBones;
    const uint32_t ord = g_dcSinceUpload++;

    // Do not put single-matrix draws in the table. bones == 1 is one transform,
    // never a skin palette, and every such row came back skin=-- . They are also
    // what SATURATED the 96-row table - and saturation was not a cosmetic
    // problem: the old hide test lived inside `if (slot >= 0)`, so once the
    // table was full a new signature got slot = -1 and was NEVER suppressed.
    // That is the whole explanation for the faint arm that survived hiding all
    // three rows: the same mesh, drawn through signatures there was no room to
    // record. The mesh lock does not depend on the table at all, which is why
    // it removed the arms outright.
    if (bones < 2) {
        g_dcSkippedFlat++;
        return dvr::frame::orig_draw_indexed(self, type, baseVertex, minIndex,
                                             numVertices, startIndex, primCount);
    }

    DcDraw d;
    memset(&d, 0, sizeof(d));
    d.bones = bones;
    d.baseVertex = baseVertex; d.primType = (uint32_t)type;
    d.minIndex = minIndex; d.numVerts = numVertices;
    d.startIndex = startIndex; d.prims = primCount;
    d.ordAfterUpload = ord;

    // Identity only - see the D3D OBJECT RULE above. Each of these AddRefs and
    // is released on the next line; the pointer is never dereferenced.
    {
        IDirect3DVertexBuffer9* vb = NULL; UINT off = 0, stride = 0;
        if (SUCCEEDED(self->GetStreamSource(0, &vb, &off, &stride)) && vb) {
            d.vb = vb; d.stride = stride; d.streamOff = off;   // off was discarded before
            vb->Release();
        }
        IDirect3DIndexBuffer9* ib = NULL;
        if (SUCCEEDED(self->GetIndices(&ib)) && ib) { d.ib = ib; ib->Release(); }
        IDirect3DVertexDeclaration9* dcl = NULL;
        if (SUCCEEDED(self->GetVertexDeclaration(&dcl)) && dcl) { d.decl = dcl; dcl->Release(); }
        IDirect3DVertexShader9* vs = NULL;
        if (SUCCEEDED(self->GetVertexShader(&vs)) && vs) { d.vs = vs; vs->Release(); }
    }

    int slot = -1;
    const int have = g_dcN;
    for (int i = 0; i < have; i++)
        if (DcSame(&g_dcDraw[i], &d)) { slot = i; break; }
    if (slot < 0 && have < DC_MAX) {
        // Fill the row COMPLETELY before publishing it, so the reporter on the
        // other thread never walks a half-built entry.
        g_dcDraw[have] = d;
        g_dcDraw[have].hits = 0;
        g_dcDraw[have].frames = 0;
        g_dcDraw[have].lastFrame = 0;
        g_dcDraw[have].hitsPrev = 0;
        DcReadDecl(&g_dcDraw[have]);       // once per row, never per draw
        _ReadWriteBarrier();
        g_dcN = have + 1;
        slot = have;
    }
    if (slot >= 0) {
        DcDraw* r = &g_dcDraw[slot];
        r->hits++;
        const uint32_t f = dvr::frame::count();
        if (r->lastFrame != f) { r->lastFrame = f; r->frames++; }
        const int hide = g_dcHideAt;
        if (hide >= 0 && hide < g_dcN &&
            (slot == hide || DcSameGeom(r, &g_dcDraw[hide]))) {
            g_dcHidden++;
            return D3D_OK;      // every pass over this mesh is dropped
        }
    }
    return dvr::frame::orig_draw_indexed(self, type, baseVertex, minIndex,
                                         numVertices, startIndex, primCount);
}


// Numpad 6 next, Numpad 4 back, Numpad 5 everything visible - deliberately the
// same idiom as the material cycler the tester has already walked. One press,
// one row hidden, the log names which.
// Numpad 9 / 7 walk the slices, Numpad 8 shows the whole mesh again. Only
// meaningful once Numpad 6 has locked onto a mesh.
static void DcSliceTick()
{
    if (!g_dcSliceReq) return;
    const int req = g_dcSliceReq;
    g_dcSliceReq = 0;
    if (!g_dcHideVb) {
        Log("dc/slice: no mesh locked yet - press Numpad 6 first, which locks "
            "onto the arm mesh and hides all of it");
        return;
    }
    if (req == 2) {
        // Numpad 8 MARKS the slice under the cursor, so the marks build up.
        // The two arms are not one contiguous run, so one range was never going
        // to be enough - a set of them is.
        const int at = g_dcHideSlice;
        if (at < 0 || at >= DC_SLICES) {
            Log("dc/slice: nothing under the cursor to mark - press Numpad 9 first");
            return;
        }
        g_dcSliceMask ^= (1u << at);
        const bool on = (g_dcSliceMask & (1u << at)) != 0;
        int n = 0;
        for (int b = 0; b < DC_SLICES; b++) if (g_dcSliceMask & (1u << b)) n++;
        Log("dc/slice: slice %d of %d %s - %d slice(s) now marked, mask 0x%08X",
            at + 1, DC_SLICES, on ? "MARKED HIDDEN" : "unmarked", n,
            (unsigned)g_dcSliceMask);
        Log("dc/slice:   marked ranges: ");
        for (int b = 0; b < DC_SLICES; b++)
            if (g_dcSliceMask & (1u << b))
                Log("dc/slice:     slice %2d = triangles %d%%..%d%%",
                    b + 1, b * 100 / DC_SLICES, (b + 1) * 100 / DC_SLICES);
        return;
    }
    int at = g_dcHideSlice;
    if (at < 0) at = (req > 0 ? -1 : DC_SLICES);   // first press enters slice 0
    at += (req > 0 ? 1 : -1);
    if (at >= DC_SLICES) at = 0;
    if (at < 0) at = DC_SLICES - 1;
    g_dcHideSlice = at;
    Log("dc/slice: >>> cursor on slice %d of %d (triangles %d%%..%d%%)%s - the "
        "cursor slice plus %d marked one(s) are hidden <<<  (9 next, 7 back, "
        "8 mark/unmark, 5 release)",
        at + 1, DC_SLICES, at * 100 / DC_SLICES, (at + 1) * 100 / DC_SLICES,
        (g_dcSliceMask & (1u << at)) ? " [already MARKED]" : "",
        (int)__popcnt(g_dcSliceMask));
}


static void DcCycleTick()
{
    DcSliceTick();
    if (!g_dcCycleReq) return;
    const int req = g_dcCycleReq;
    g_dcCycleReq = 0;
    if (!g_dcN) {
        Log("dc/cycle: nothing censused yet - no palette-fed draw has run");
        return;
    }
    if (req == 2) {
        g_dcHideAt = -1;
        g_dcHideVb = NULL; g_dcHideIb = NULL; g_dcHideSlice = -1; g_dcSliceMask = 0;
        Log("dc/cycle: ALL VISIBLE - nothing hidden, mesh lock released");
        return;
    }
    // Only walk rows that are LIVE - drawn in the last window. Sixty stale
    // world rows from a previous area made the first walk unusable: the tester
    // had to press past them to reach anything that still renders.
    int at = g_dcHideAt;
    const int n = g_dcN;
    int step = (req > 0 ? 1 : -1), tries = 0;
    const DcDraw* cur = (at >= 0 && at < n) ? &g_dcDraw[at] : NULL;
    do {
        at += step;
        if (at >= n) at = 0;
        if (at < 0)  at = n - 1;
        if (!g_dcDraw[at].hits && !g_dcDraw[at].hitsPrev) continue;   // dead row
        if (cur && DcSameGeom(&g_dcDraw[at], cur)) continue;          // same mesh
        break;
    } while (++tries < n);
    if (tries >= n) {
        // Not an error, and the old text said the wrong thing: usually it means
        // there is exactly ONE live mesh and it is already the one being
        // hidden. Say that, and leave the lock where it is.
        Log("dc/cycle: only one live mesh in the census and it is already "
            "hidden (row %d). Nothing else is being drawn from a bone palette "
            "right now, so there is nowhere to step to.", g_dcHideAt);
        return;
    }
    g_dcHideAt = at;
    g_dcHideVb = g_dcDraw[at].vb;      // arm the MESH LOCK on this buffer pair
    g_dcHideIb = g_dcDraw[at].ib;
    g_dcDropIdx = g_dcDropPrim = g_dcUnseen = 0;
    g_dcHideSlice = -1; g_dcSliceMask = 0;   // a fresh lock starts on the whole mesh
    g_dcHidden = 0;
    DcDraw* d = &g_dcDraw[at];
    int passes = 0;
    for (int i = 0; i < n; i++) if (DcSameGeom(&g_dcDraw[i], d)) passes++;
    Log("dc/cycle: >>> [row %d of %d] HIDDEN - the MESH, all %d pass(es) over "
        "it: bones=%u skin=%c%c prims=%u verts=%u (min %u, start %u) vb=%p "
        "ib=%p <<<  (Numpad 6 next mesh, Numpad 4 back, Numpad 5 all visible)",
        at, n, passes, d->bones, d->hasBlendIdx ? 'I' : '-',
        d->hasBlendWt ? 'W' : '-', d->prims, d->numVerts, d->minIndex,
        d->startIndex, d->vb, d->ib);
}


static void DcTick()
{
    if (!g_dcOn) return;
    DcCycleTick();
    const double now = MaimNowMs();
    if (now < g_dcNextReport) return;
    const bool first = (g_dcNextReport == 0.0);
    g_dcNextReport = now + 10000.0;
    if (first) return;
    DcReport("10 s");
    if (g_dcHideAt >= 0) {
        Log("dc: MESH LOCK on row [%d] vb=%p ib=%p - dropped %u indexed, %u "
            "NON-indexed, of which %u had NO palette current. That last number "
            "is what the palette-gated census was blind to: above 0 and the "
            "residual arm was this mesh drawn by passes the census never saw; "
            "at 0 with an arm still on screen, the residual is NOT this mesh "
            "and it is a different asset.",
            g_dcHideAt, g_dcHideVb, g_dcHideIb,
            g_dcDropIdx, g_dcDropPrim, g_dcUnseen);
        g_dcDropIdx = g_dcDropPrim = g_dcUnseen = 0;
    }
    g_dcHidden = 0;
}


// `dc [status|report|hide <n>|show]`
static bool DcCommand(const char* args)
{
    char sub[16] = ""; int n = -1;
    const int got = sscanf(args, "%15s %d", sub, &n);
    if (got < 1 || !_stricmp(sub, "status")) {
        Log("dc: census %s | %d distinct draw(s) | hidden row %d | last palette "
            "%u bones", g_dcOn ? "ARMED" : "off", g_dcN, g_dcHideAt, g_dcPendingBones);
        return true;
    }
    if (!_stricmp(sub, "report")) { DcReport("seam"); return true; }
    if (!_stricmp(sub, "mask")) {
        // `dc mask <hex>` restores a cut measured in an earlier session, and
        // `dc mask s19` the one from 2026-09-06. Twelve key presses in a
        // headset should never have to be repeated to get back to a known cut.
        unsigned m = 0;
        const char* a = args;
        while (*a && *a != ' ') a++;
        while (*a == ' ') a++;
        if (!_stricmp(a, "s19")) m = DC_MASK_S19;
        else if (sscanf(a, "%x", &m) != 1) {
            Log("dc: mask <hex> | mask s19   (current 0x%08X)", (unsigned)g_dcSliceMask);
            return true;
        }
        g_dcSliceMask = m;
        Log("dc: slice mask -> 0x%08X (%d of %d slices hidden). The mask is "
            "PERCENTAGES of this mesh's triangle count, so it only means "
            "anything on the asset it was measured on.",
            m, (int)__popcnt(m), DC_SLICES);
        return true;
    }
    if (!_stricmp(sub, "show"))   { g_dcHideAt = -1; Log("dc: all visible"); return true; }
    if (!_stricmp(sub, "hide") && got >= 2) {
        if (n < 0 || n >= g_dcN) { Log("dc: no row %d (have %d)", n, g_dcN); return true; }
        g_dcHideAt = n; g_dcHidden = 0;
        Log("dc: hiding row %d", n);
        return true;
    }
    Log("dc: usage - dc status | report | hide <n> | show | mask <hex>|s19");
    return true;
}
