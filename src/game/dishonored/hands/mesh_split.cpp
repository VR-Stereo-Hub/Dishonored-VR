// game/dishonored/hands/mesh_split.cpp - included by src/mod/dishonoredvr.cpp
// (unity build). See state chunk 55 for why this exists and what it reads.
//
// The chain, in order:
//
//   MsRead      copy the draw's index range and vertex window out of the
//               game's buffers, once, and VALIDATE the copy
//   MsBones     per-bone weight and centroid; the co-influence graph
//   MsSides     split the bones into two arms - by the graph if it says two
//               limbs, by the widest coordinate gap if it does not
//   MsWrist     per side, find the hand bone and derive the wrist radius from
//               the gap in the bone spacing
//   MsClassify  every triangle to a class by the influence it actually carries
//   MsUpload    emit the classes into OUR index buffer, contiguous per class
//
// Everything after MsRead is arithmetic on the copy, so a wrist tweak is a
// reclassify and a refill - no second lock of an engine buffer.

// ---- reading the declaration ------------------------------------------------

struct MsElem { int off, type, have; };

// Where POSITION, BLENDWEIGHT and BLENDINDICES sit in a stream-0 vertex, and
// in what format. Anything this does not understand is REFUSED by name rather
// than read as though it were something else.
static bool MsDecl(IDirect3DVertexDeclaration9* d, MsElem* pos, MsElem* wt, MsElem* idx)
{
    pos->have = wt->have = idx->have = 0;
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH];
    UINT n = 0;
    if (!d || FAILED(d->GetDeclaration(el, &n))) {
        Log("ms: the vertex declaration would not read - no classification is "
            "possible without knowing where the blend indices are");
        return false;
    }
    if (n > MAXD3DDECLLENGTH) n = MAXD3DDECLLENGTH;
    // Keep every stream-0 element: the clip has to interpolate ALL of them to
    // make a new vertex, not just the three it classifies by. An element it
    // cannot interpolate is copied from the nearer parent, which is right for a
    // bone index and close enough on a ring one triangle wide.
    g_msNel = 0;
    g_msDeclStreams = 0;
    for (UINT i = 0; i < n; i++) {
        if (el[i].Type == D3DDECLTYPE_UNUSED) continue;
        if (el[i].Stream < 32) g_msDeclStreams |= (1u << el[i].Stream);
        if (el[i].Stream != 0) continue;
        if (g_msNel < MAXD3DDECLLENGTH) g_msEl[g_msNel++] = el[i];
    }
    for (UINT i = 0; i < n; i++) {
        if (el[i].Stream != 0) continue;
        MsElem* t = NULL;
        if (el[i].Usage == D3DDECLUSAGE_POSITION && el[i].UsageIndex == 0) t = pos;
        else if (el[i].Usage == D3DDECLUSAGE_BLENDWEIGHT)  t = wt;
        else if (el[i].Usage == D3DDECLUSAGE_BLENDINDICES) t = idx;
        if (!t || t->have) continue;
        t->off = el[i].Offset; t->type = el[i].Type; t->have = 1;
    }
    Log("ms: decl stream0 - POSITION off=%d type=%d | BLENDWEIGHT off=%d type=%d "
        "| BLENDINDICES off=%d type=%d  (D3DDECLTYPE numbers: 2=FLOAT3, "
        "3=FLOAT4, 4=D3DCOLOR, 5=UBYTE4, 8=UBYTE4N)",
        pos->have ? pos->off : -1, pos->have ? pos->type : -1,
        wt->have ? wt->off : -1, wt->have ? wt->type : -1,
        idx->have ? idx->off : -1, idx->have ? idx->type : -1);
    if (!pos->have || !wt->have || !idx->have) {
        Log("ms: REFUSED - this declaration has no %s%s%s, so it is not a "
            "skinned stream and there is nothing to classify by",
            pos->have ? "" : "POSITION ", wt->have ? "" : "BLENDWEIGHT ",
            idx->have ? "" : "BLENDINDICES");
        return false;
    }
    return true;
}


// Four blend weights out of one element. Returns false for a format this does
// not know, so an unknown encoding refuses instead of producing plausible junk.
static bool MsReadWeights(const uint8_t* v, const MsElem* e, float* w)
{
    const uint8_t* p = v + e->off;
    w[0] = w[1] = w[2] = w[3] = 0.0f;
    switch (e->type) {
    case D3DDECLTYPE_FLOAT1: memcpy(w, p, 4);  w[3] = 1.0f - w[0]; return true;
    case D3DDECLTYPE_FLOAT2: memcpy(w, p, 8);  return true;
    case D3DDECLTYPE_FLOAT3: memcpy(w, p, 12); w[3] = 1.0f - w[0] - w[1] - w[2]; return true;
    case D3DDECLTYPE_FLOAT4: memcpy(w, p, 16); return true;
    case D3DDECLTYPE_UBYTE4:
    case D3DDECLTYPE_UBYTE4N:
        for (int i = 0; i < 4; i++) w[i] = p[i] / 255.0f;
        return true;
    case D3DDECLTYPE_D3DCOLOR:
        // A DWORD stored ARGB, delivered to the shader as (R,G,B,A) - so the
        // component order is NOT the byte order, and reading it as bytes gives
        // a silently swapped palette.
        w[0] = p[2] / 255.0f; w[1] = p[1] / 255.0f;
        w[2] = p[0] / 255.0f; w[3] = p[3] / 255.0f;
        return true;
    default: return false;
    }
}


static bool MsReadIndices(const uint8_t* v, const MsElem* e, uint8_t* b)
{
    const uint8_t* p = v + e->off;
    switch (e->type) {
    case D3DDECLTYPE_UBYTE4:
    case D3DDECLTYPE_UBYTE4N:
        b[0] = p[0]; b[1] = p[1]; b[2] = p[2]; b[3] = p[3];
        return true;
    case D3DDECLTYPE_D3DCOLOR:
        b[0] = p[2]; b[1] = p[1]; b[2] = p[0]; b[3] = p[3];
        return true;
    default: return false;
    }
}


// ---- step 1: the copy, and its validation -----------------------------------

// The copy is only worth anything if it really is the mesh. Each check below is
// a fact the data MUST satisfy, so a write-only buffer that handed back an
// uninitialised page fails at least one of them and the whole build refuses.
// This is the part that lets the instrument print the unwelcome answer.
static bool MsValidate(uint32_t bones)
{
    int badIdx = 0, badBone = 0, badWt = 0, nonFinite = 0;
    uint32_t maxBone = 0;
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    for (int i = 0; i < g_msTris * 3; i++) {
        if (g_msIdx[i] < g_msMinIndex ||
            (int)(g_msIdx[i] - g_msMinIndex) >= g_msVerts) badIdx++;
    }
    for (int v = 0; v < g_msVerts; v++) {
        const MsVert* q = &g_msVert[v];
        float s = 0.0f;
        for (int k = 0; k < 4; k++) {
            if (q->bi[k] > maxBone) maxBone = q->bi[k];
            if (q->bi[k] >= bones && q->bw[k] > 0.01f) badBone++;
            s += q->bw[k];
        }
        if (s < 0.90f || s > 1.10f) badWt++;
        for (int a = 0; a < 3; a++) {
            const float c = q->p[a];
            if (!(c > -1e9f && c < 1e9f)) { nonFinite++; break; }
            if (c < lo[a]) lo[a] = c;
            if (c > hi[a]) hi[a] = c;
        }
    }
    const int badWtPct = g_msVerts ? (badWt * 100 / g_msVerts) : 100;
    Log("ms: validation - %d/%d indices outside the draw's own vertex window, "
        "%d influence(s) naming a bone >= the palette size %u (max seen %u), "
        "%d%% of vertices whose weights do not sum to 1, %d non-finite "
        "position(s). bbox (%.1f %.1f %.1f) .. (%.1f %.1f %.1f)",
        badIdx, g_msTris * 3, badBone, bones, maxBone, badWtPct, nonFinite,
        lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
    if (badIdx || badBone || nonFinite || badWtPct > 10) {
        Log("ms: REFUSED - the copy does not look like this mesh. The most "
            "likely cause is a D3DUSAGE_WRITEONLY buffer handing back an "
            "uninitialised page on a read lock, which is the driver's right. "
            "The slice mask (Numpad 9/7/8, `dc mask s19`) is unaffected and is "
            "still the way to cut this mesh.");
        return false;
    }
    for (int a = 0; a < 3; a++) if (hi[a] - lo[a] < 1e-4f) {
        Log("ms: REFUSED - the mesh has no extent on axis %d (%.6f). A "
            "degenerate bounding box cannot be split into two arms.",
            a, hi[a] - lo[a]);
        return false;
    }
    return true;
}


// Lock the game's buffers, copy the draw's range out, unlock. Once per lock.
//
// D3D OBJECT RULE: GetIndices / GetStreamSource / GetVertexDeclaration each
// AddRef. Every one is released before this function returns and none is
// dereferenced after its Release. Nothing is written to an engine buffer.
static bool MsRead(IDirect3DDevice9* dev, INT baseVertex, UINT minIndex,
                   UINT numVertices, UINT startIndex, UINT primCount, uint32_t bones)
{
    if ((int)numVertices > MS_MAX_VERTS || (int)primCount > MS_MAX_TRIS) {
        Log("ms: REFUSED - the draw is %u verts / %u tris, past this module's "
            "%d / %d ceiling. Raise MS_MAX_VERTS / MS_MAX_TRIS if this really "
            "is the arm mesh; it measured 2771 / 4448.",
            numVertices, primCount, MS_MAX_VERTS, MS_MAX_TRIS);
        return false;
    }
    if (numVertices < 3 || primCount < 1) return false;

    bool ok = false;
    IDirect3DIndexBuffer9* ib = NULL;
    IDirect3DVertexBuffer9* vb = NULL;
    IDirect3DVertexDeclaration9* decl = NULL;
    UINT streamOff = 0, stride = 0;

    do {
        if (FAILED(dev->GetIndices(&ib)) || !ib) break;
        if (FAILED(dev->GetStreamSource(0, &vb, &streamOff, &stride)) || !vb) break;
        if (FAILED(dev->GetVertexDeclaration(&decl)) || !decl) break;

        D3DINDEXBUFFER_DESC id; memset(&id, 0, sizeof(id));
        D3DVERTEXBUFFER_DESC vd; memset(&vd, 0, sizeof(vd));
        if (FAILED(ib->GetDesc(&id)) || FAILED(vb->GetDesc(&vd))) break;
        // Log the descriptors BEFORE the lock: if a read lock on a write-only
        // buffer is what breaks this, the log has to say so from the run that
        // broke, not from a theory written afterwards.
        Log("ms: ib fmt=%s usage=0x%X pool=%d size=%u | vb usage=0x%X pool=%d "
            "size=%u stride=%u off=%u  (usage bit 0x8 is D3DUSAGE_WRITEONLY, "
            "pool 0=DEFAULT 1=MANAGED 2=SYSTEMMEM; a WRITEONLY DEFAULT buffer "
            "is the case where a read may legally return nothing real)",
            id.Format == D3DFMT_INDEX16 ? "INDEX16" :
            id.Format == D3DFMT_INDEX32 ? "INDEX32" : "?",
            (unsigned)id.Usage, (int)id.Pool, id.Size,
            (unsigned)vd.Usage, (int)vd.Pool, vd.Size, stride, streamOff);
        if (id.Format != D3DFMT_INDEX16 && id.Format != D3DFMT_INDEX32) break;
        if (!stride) break;

        MsElem pos, wt, idx;
        if (!MsDecl(decl, &pos, &wt, &idx)) break;
        if ((UINT)(pos.off + 12) > stride || (UINT)(idx.off + 4) > stride) {
            Log("ms: REFUSED - the declaration puts POSITION at %d and "
                "BLENDINDICES at %d in a %u byte vertex; one of them would read "
                "past the end", pos.off, idx.off, stride);
            break;
        }

        const UINT ibStride = (id.Format == D3DFMT_INDEX16) ? 2u : 4u;
        const UINT ibOff = startIndex * ibStride;
        const UINT ibLen = primCount * 3u * ibStride;
        const UINT vbOff = streamOff + (UINT)((INT)minIndex + baseVertex) * stride;
        const UINT vbLen = numVertices * stride;
        if (ibOff + ibLen > id.Size || vbOff + vbLen > vd.Size) {
            Log("ms: REFUSED - the draw range runs past the buffer: ib needs "
                "%u..%u of %u, vb needs %u..%u of %u",
                ibOff, ibOff + ibLen, id.Size, vbOff, vbOff + vbLen, vd.Size);
            break;
        }

        const DWORD ibFlag = (id.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY;
        const DWORD vbFlag = (vd.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY;

        void* p = NULL;
        if (FAILED(ib->Lock(ibOff, ibLen, &p, ibFlag)) || !p) {
            Log("ms: REFUSED - the index buffer would not lock (usage 0x%X). "
                "That is a legitimate refusal for a write-only buffer and it "
                "closes this route on this driver; the slice mask still works.",
                (unsigned)id.Usage);
            break;
        }
        g_msTris = (int)primCount;
        if (ibStride == 2) {
            const uint16_t* s = (const uint16_t*)p;
            for (int i = 0; i < g_msTris * 3; i++) g_msIdx[i] = s[i];
        } else {
            memcpy(g_msIdx, p, (size_t)g_msTris * 3 * 4);
        }
        ib->Unlock();

        p = NULL;
        if (FAILED(vb->Lock(vbOff, vbLen, &p, vbFlag)) || !p) {
            Log("ms: REFUSED - the vertex buffer would not lock (usage 0x%X)",
                (unsigned)vd.Usage);
            break;
        }
        g_msVerts = (int)numVertices;
        g_msMinIndex = minIndex;
        g_msStride = stride;
        bool fmtOk = true;
        for (int v = 0; v < g_msVerts && fmtOk; v++) {
            const uint8_t* q = (const uint8_t*)p + (size_t)v * stride;
            memcpy(&g_msRaw[(size_t)v * MS_MAX_STRIDE], q, stride);
            memcpy(g_msVert[v].p, q + pos.off, 12);
            fmtOk = MsReadIndices(q, &idx, g_msVert[v].bi) &&
                    MsReadWeights(q, &wt, g_msVert[v].bw);
        }
        vb->Unlock();
        if (!fmtOk) {
            Log("ms: REFUSED - blend indices type %d / weights type %d are a "
                "D3DDECLTYPE this module does not decode. Guessing at an "
                "encoding would classify triangles by noise.", idx.type, wt.type);
            break;
        }
        g_msIbFmt = (uint32_t)id.Format;

        // Can we own stream 0 outright? Re-basing the indices onto a buffer of
        // ours desynchronises any OTHER stream the game has bound, because its
        // vertices are addressed by the same index. So look, and say what was
        // found - a veto here is the difference between a clipped edge and a
        // sawtooth one, and it must not be silent.
        // BOUND IS NOT USED. A stream left bound by an earlier draw is not
        // data this one consumes: the declaration decides what the shader
        // reads. The first version vetoed on any bound stream, which can throw
        // away the clip - and the caps with it - over leftover state. Only a
        // stream the declaration NAMES is allowed to veto.
        g_msExtraStream = -1;
        for (UINT si = 1; si < 8; si++) {
            if (!(g_msDeclStreams & (1u << si))) continue;   // decl ignores it
            IDirect3DVertexBuffer9* ex = NULL; UINT eo = 0, es = 0;
            if (SUCCEEDED(dev->GetStreamSource(si, &ex, &eo, &es)) && ex) {
                ex->Release();
                if (es) { g_msExtraStream = (int)si; break; }
            }
        }
        if (g_msDeclStreams & ~1u)
            Log("ms: the declaration references stream mask 0x%X; only a stream "
                "it actually names can veto the clip, because a stream merely "
                "left bound by an earlier draw is not data this one reads",
                g_msDeclStreams);
        g_msOwnVb = (g_msExtraStream < 0) && (stride <= MS_MAX_STRIDE);
        if (!g_msOwnVb)
            Log("ms: stream %d is also bound (or the %u byte vertex is past this "
                "module's %d ceiling), so the index list cannot be re-based onto "
                "a buffer of ours. The edge will be cut to whole triangles "
                "instead of clipped - `ms edge 1` is then the least ragged rule.",
                g_msExtraStream, stride, MS_MAX_STRIDE);
        else
            Log("ms: stream 0 is the only stream, %u bytes a vertex, %d "
                "element(s) - the triangles that straddle the cut can be clipped "
                "exactly rather than kept or dropped whole",
                stride, g_msNel);

        ok = MsValidate(bones);
    } while (0);

    if (decl) decl->Release();
    if (vb) vb->Release();
    if (ib) ib->Release();
    return ok;
}


// ---- step 2: per-bone facts and the co-influence graph -----------------------

static inline void MsAdjSet(int a, int b)
{
    g_msAdj[a][b >> 3] |= (uint8_t)(1u << (b & 7));
    g_msAdj[b][a >> 3] |= (uint8_t)(1u << (a & 7));
}
static inline bool MsAdjGet(int a, int b)
{
    return (g_msAdj[a][b >> 3] & (1u << (b & 7))) != 0;
}

// Two bones are adjacent when some vertex is meaningfully moved by BOTH. That
// is the mesh's own statement that they are joined, and it needs no bone names,
// no hierarchy and no engine structures - which is the whole reason it is used
// here instead of walking the RefSkeleton for names.
#define MS_ADJ_W 0.05f

static void MsBones(uint32_t bones)
{
    g_msBones = (int)(bones < MS_MAX_BONES ? bones : MS_MAX_BONES);
    memset(g_msBoneW, 0, sizeof(g_msBoneW));
    memset(g_msBoneCen, 0, sizeof(g_msBoneCen));
    memset(g_msAdj, 0, sizeof(g_msAdj));
    memset(g_msBoneSide, 0, sizeof(g_msBoneSide));
    memset(g_msBoneHand, 0, sizeof(g_msBoneHand));
    for (int v = 0; v < g_msVerts; v++) {
        const MsVert* q = &g_msVert[v];
        for (int k = 0; k < 4; k++) {
            const int b = q->bi[k];
            if (b >= g_msBones || q->bw[k] <= 0.0f) continue;
            g_msBoneW[b] += q->bw[k];
            for (int a = 0; a < 3; a++) g_msBoneCen[b][a] += q->bw[k] * q->p[a];
        }
        for (int k = 0; k < 4; k++) {
            if (q->bi[k] >= g_msBones || q->bw[k] < MS_ADJ_W) continue;
            for (int j = k + 1; j < 4; j++) {
                if (q->bi[j] >= g_msBones || q->bw[j] < MS_ADJ_W) continue;
                if (q->bi[k] != q->bi[j]) MsAdjSet(q->bi[k], q->bi[j]);
            }
        }
    }
    int used = 0;
    for (int b = 0; b < g_msBones; b++) {
        if (g_msBoneW[b] <= 0.0f) continue;
        used++;
        for (int a = 0; a < 3; a++) g_msBoneCen[b][a] /= g_msBoneW[b];
    }
    Log("ms: %d of %d palette bones carry weight on this mesh", used, g_msBones);
}


// ---- step 3: which arm ------------------------------------------------------

// Flood the co-influence graph. Two separate limbs share no vertex, so the
// graph should fall into exactly two components of real size - and if it does
// not, that is worth saying out loud rather than papering over with geometry.
static int MsComponents(int* comp)
{
    for (int b = 0; b < g_msBones; b++) comp[b] = -1;
    int n = 0;
    int stack[MS_MAX_BONES];
    for (int s = 0; s < g_msBones; s++) {
        if (comp[s] >= 0 || g_msBoneW[s] <= 0.0f) continue;
        int sp = 0; stack[sp++] = s; comp[s] = n;
        while (sp) {
            const int b = stack[--sp];
            for (int o = 0; o < g_msBones; o++)
                if (comp[o] < 0 && g_msBoneW[o] > 0.0f && MsAdjGet(b, o)) {
                    comp[o] = n; stack[sp++] = o;
                }
        }
        n++;
    }
    return n;
}

static bool MsSides(void)
{
    int comp[MS_MAX_BONES];
    const int nc = MsComponents(comp);
    int size[MS_MAX_BONES]; memset(size, 0, sizeof(size));
    for (int b = 0; b < g_msBones; b++) if (comp[b] >= 0) size[comp[b]]++;
    int big[2] = { -1, -1 };
    for (int c = 0; c < nc; c++) {
        if (size[c] < 4) continue;
        if (big[0] < 0 || size[c] > size[big[0]]) { big[1] = big[0]; big[0] = c; }
        else if (big[1] < 0 || size[c] > size[big[1]]) big[1] = c;
    }
    if (big[0] >= 0 && big[1] >= 0) {
        for (int b = 0; b < g_msBones; b++) {
            if (comp[b] == big[0]) g_msBoneSide[b] = 1;
            else if (comp[b] == big[1]) g_msBoneSide[b] = 2;
        }
        Log("ms: two arms found in the SKINNING GRAPH - %d component(s), the two "
            "largest carrying %d and %d bones. No vertex is shared between them, "
            "which is what two separate limbs look like.",
            nc, size[big[0]], size[big[1]]);
    } else {
        // The graph did not separate. Fall back to the widest coordinate gap:
        // the axis on which the bone centroids split into two clusters with the
        // largest empty band between them.
        int bestAxis = -1; float bestGap = -1.0f, bestCut = 0.0f;
        for (int a = 0; a < 3; a++) {
            float c[MS_MAX_BONES]; int n = 0;
            for (int b = 0; b < g_msBones; b++)
                if (g_msBoneW[b] > 0.0f) c[n++] = g_msBoneCen[b][a];
            if (n < 8) continue;
            for (int i = 1; i < n; i++) {           // insertion sort, n <= 128
                const float k = c[i]; int j = i - 1;
                while (j >= 0 && c[j] > k) { c[j + 1] = c[j]; j--; }
                c[j + 1] = k;
            }
            const float span = c[n - 1] - c[0];
            if (span <= 0.0f) continue;
            for (int i = n / 4; i < n - n / 4 - 1; i++) {
                const float g = (c[i + 1] - c[i]) / span;
                if (g > bestGap) { bestGap = g; bestAxis = a; bestCut = 0.5f * (c[i] + c[i + 1]); }
            }
        }
        if (bestAxis < 0 || bestGap < 0.05f) {
            Log("ms: REFUSED - the bones form %d graph component(s), not two, "
                "and no axis splits their centroids with a gap worth trusting "
                "(best %.3f of the span). This mesh may be one arm, or the copy "
                "may be wrong. Nothing is cut.", nc, bestGap);
            return false;
        }
        for (int b = 0; b < g_msBones; b++)
            if (g_msBoneW[b] > 0.0f)
                g_msBoneSide[b] = (g_msBoneCen[b][bestAxis] < bestCut) ? 1 : 2;
        Log("ms: the skinning graph gave %d component(s), not two, so the arms "
            "were separated GEOMETRICALLY instead: axis %d, cut at %.2f, an "
            "empty band %.1f%% of the span wide. Less trustworthy than the "
            "graph split - if the cut looks wrong, this line is why.",
            nc, bestAxis, bestCut, bestGap * 100.0f);
    }

    // Name the sides by the axis they differ on most, so the log and the knob
    // agree about which arm is which. UE3 mesh space is X forward, Y right,
    // Z up, so a larger coordinate on that axis reads as the RIGHT arm - stated
    // as an assumption because nothing here has verified the convention on this
    // asset. It affects only which arm the knob moves, never the cut itself.
    int nA = 0, nB = 0; float sA[3] = { 0, 0, 0 }, sB[3] = { 0, 0, 0 };
    for (int b = 0; b < g_msBones; b++) {
        if (!g_msBoneSide[b]) continue;
        float* s = (g_msBoneSide[b] == 1) ? sA : sB;
        for (int a = 0; a < 3; a++) s[a] += g_msBoneCen[b][a];
        if (g_msBoneSide[b] == 1) nA++; else nB++;
    }
    if (!nA || !nB) { Log("ms: REFUSED - one side came out empty"); return false; }
    for (int a = 0; a < 3; a++) { sA[a] /= nA; sB[a] /= nB; }
    int axis = 0; float best = -1.0f;
    for (int a = 0; a < 3; a++) {
        const float d = (sA[a] > sB[a]) ? (sA[a] - sB[a]) : (sB[a] - sA[a]);
        if (d > best) { best = d; axis = a; }
    }
    g_msSideAxis = axis;
    g_msSideSign[1] = sA[axis]; g_msSideSign[2] = sB[axis];
    Log("ms: side A = %d bones, centroid (%.1f %.1f %.1f); side B = %d bones, "
        "centroid (%.1f %.1f %.1f). They differ most on axis %d, so that is the "
        "lateral axis; on the UE3 convention (X fwd, Y right, Z up) the larger "
        "coordinate is the RIGHT arm, which makes side %s the right one.",
        nA, sA[0], sA[1], sA[2], nB, sB[0], sB[1], sB[2], axis,
        (sA[axis] > sB[axis]) ? "A" : "B");
    return true;
}


// ---- step 4: where the wrist is ---------------------------------------------

// The hand bone is the one with the most NEIGHBOURS. A forearm joins two
// things; a hand joins the forearm and every finger it carries. That is a
// structural fact about hands, not a measurement of this asset, so it does not
// have to be re-derived if the mesh changes.
//
// The wrist then falls out of the SPACING. Finger bones sit inside a hand's
// worth of space around that bone; the forearm, upper arm and clavicle are
// strung out along the limb. Sort every bone in the side by its distance from
// the hand bone, and the biggest gap in that list IS the wrist - nothing is
// eyeballed and nothing is a percentage of the triangle order.
static bool MsWrist(int side)
{
    int deg[MS_MAX_BONES]; memset(deg, 0, sizeof(deg));
    int best = -1;
    for (int b = 0; b < g_msBones; b++) {
        if (g_msBoneSide[b] != side) continue;
        for (int o = 0; o < g_msBones; o++)
            if (g_msBoneSide[o] == side && o != b && MsAdjGet(b, o)) deg[b]++;
        if (best < 0 || deg[b] > deg[best] ||
            (deg[b] == deg[best] && g_msBoneW[b] < g_msBoneW[best])) best = b;
    }
    if (best < 0) return false;
    g_msHandBone[side] = best;

    float d[MS_MAX_BONES]; int n = 0;
    for (int b = 0; b < g_msBones; b++) {
        if (g_msBoneSide[b] != side) continue;
        float s = 0.0f;
        for (int a = 0; a < 3; a++) {
            const float e = g_msBoneCen[b][a] - g_msBoneCen[best][a];
            s += e * e;
        }
        d[n++] = sqrtf(s);
    }
    if (n < 5) {
        Log("ms: side %d has only %d bone(s) - too few for a spacing gap to "
            "mean anything", side, n);
        return false;
    }
    for (int i = 1; i < n; i++) {
        const float k = d[i]; int j = i - 1;
        while (j >= 0 && d[j] > k) { d[j + 1] = d[j]; j--; }
        d[j + 1] = k;
    }
    // Look for the gap past the third bone - a hand is never one bone, and the
    // first gaps in the list are between finger joints.
    int cut = -1; float gap = -1.0f;
    for (int i = 2; i < n - 1; i++)
        if (d[i + 1] - d[i] > gap) { gap = d[i + 1] - d[i]; cut = i; }
    if (cut < 0) return false;
    g_msWristR[side] = 0.5f * (d[cut] + d[cut + 1]);
    Log("ms: side %d - hand bone %d (%d neighbours, the most in this arm). The "
        "biggest gap in the bone spacing is %.1f wide after %d bone(s), so the "
        "wrist radius is %.1f and the arm runs on to %.1f.",
        side, best, deg[best], gap, cut + 1, g_msWristR[side], d[n - 1]);
    return true;
}


// ---- step 5: the axis, the plane, and the triangles --------------------------

// The centroid of one triangle, in mesh space.
static void MsTriCentroid(int t, float* c)
{
    c[0] = c[1] = c[2] = 0.0f;
    for (int k = 0; k < 3; k++) {
        const uint32_t rel = g_msIdx[t * 3 + k] - g_msMinIndex;
        if ((int)rel >= g_msVerts) continue;
        for (int a = 0; a < 3; a++) c[a] += g_msVert[rel].p[a];
    }
    for (int a = 0; a < 3; a++) c[a] *= (1.0f / 3.0f);
}


// Which ARM each triangle belongs to. This half of the classification was never
// the problem - the graph split is clean, and the two arms come out with
// identical triangle counts, which is what a mirrored asset looks like - so it
// is computed once at build time and reused by every reclassify.
static void MsTriSides(void)
{
    int n[3] = { 0, 0, 0 };
    for (int t = 0; t < g_msTris; t++) {
        float acc[3] = { 0.0f, 0.0f, 0.0f };
        for (int c = 0; c < 3; c++) {
            const uint32_t rel = g_msIdx[t * 3 + c] - g_msMinIndex;
            if ((int)rel >= g_msVerts) continue;
            const MsVert* q = &g_msVert[rel];
            for (int k = 0; k < 4; k++) {
                const int b = q->bi[k];
                if (b >= g_msBones || q->bw[k] <= 0.0f || !g_msBoneSide[b]) continue;
                acc[g_msBoneSide[b]] += q->bw[k];
            }
        }
        const uint8_t sd = (acc[1] <= 0.0f && acc[2] <= 0.0f) ? (uint8_t)0
                         : (uint8_t)(acc[1] >= acc[2] ? 1 : 2);
        g_msTriSide[t] = sd;
        n[sd]++;
    }
    Log("ms: triangles by ARM - side A %d, side B %d, neither %d, of %d",
        n[1], n[2], n[0], g_msTris);
}


// The dominant direction of a set of triangle centroids, by power iteration on
// their covariance. For a limb that is the limb's own axis, which is what makes
// the cut a circle rather than an ellipse slanted across the forearm.
//
// prevAxis/lo/hi restrict the set to a BAND, so a second pass sees the forearm
// alone: taken over the whole arm the hand and fingers drag the axis toward
// themselves and the ring comes out tilted.
static void MsPca(int side, const float* prevAxis, float lo, float hi, float* out)
{
    float mean[3] = { 0.0f, 0.0f, 0.0f };
    int n = 0;
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float c[3]; MsTriCentroid(t, c);
        if (prevAxis) {
            const float d = c[0] * prevAxis[0] + c[1] * prevAxis[1] + c[2] * prevAxis[2];
            if (d < lo || d > hi) continue;
        }
        for (int a = 0; a < 3; a++) mean[a] += c[a];
        n++;
    }
    if (n < 8) {
        out[0] = prevAxis ? prevAxis[0] : 1.0f;
        out[1] = prevAxis ? prevAxis[1] : 0.0f;
        out[2] = prevAxis ? prevAxis[2] : 0.0f;
        return;
    }
    for (int a = 0; a < 3; a++) mean[a] /= n;

    float cov[3][3] = { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } };
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float c[3]; MsTriCentroid(t, c);
        if (prevAxis) {
            const float d = c[0] * prevAxis[0] + c[1] * prevAxis[1] + c[2] * prevAxis[2];
            if (d < lo || d > hi) continue;
        }
        const float e[3] = { c[0] - mean[0], c[1] - mean[1], c[2] - mean[2] };
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) cov[i][j] += e[i] * e[j];
    }
    float v[3] = { 1.0f, 1.0f, 1.0f };
    for (int it = 0; it < 40; it++) {
        float w[3];
        for (int i = 0; i < 3; i++)
            w[i] = cov[i][0] * v[0] + cov[i][1] * v[1] + cov[i][2] * v[2];
        const float len = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
        if (len < 1e-12f) break;
        for (int i = 0; i < 3; i++) v[i] = w[i] / len;
    }
    out[0] = v[0]; out[1] = v[1]; out[2] = v[2];
}


// How many triangles of this side the SPHERE would keep at the current wrist
// scale. The plane's starting position is set to keep the same number, so
// changing the SHAPE of the cut does not move it - the tuned look survives, it
// just stops being blobby.
static int MsSphereCount(int side)
{
    const int h = g_msHandBone[side];
    if (h < 0) return 0;
    const float r = g_msWristR[side] * g_msWristScale[side];
    int keep = 0;
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float hw = 0.0f, aw = 0.0f;
        for (int c = 0; c < 3; c++) {
            const uint32_t rel = g_msIdx[t * 3 + c] - g_msMinIndex;
            if ((int)rel >= g_msVerts) continue;
            const MsVert* q = &g_msVert[rel];
            for (int k = 0; k < 4; k++) {
                const int b = q->bi[k];
                if (b >= g_msBones || q->bw[k] <= 0.0f || g_msBoneSide[b] != side) continue;
                float e2 = 0.0f;
                for (int a = 0; a < 3; a++) {
                    const float e = g_msBoneCen[b][a] - g_msBoneCen[h][a];
                    e2 += e * e;
                }
                if (sqrtf(e2) <= r) hw += q->bw[k]; else aw += q->bw[k];
            }
        }
        if (hw > aw) keep++;
    }
    return keep;
}


// The axial coordinate that keeps exactly `want` triangles of this side. Rank
// based rather than a distance, so it is stable when the axis is refined.
static float MsCutForCount(int side, const float* axis, int want)
{
    static float d[MS_MAX_TRIS];
    int n = 0;
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float c[3]; MsTriCentroid(t, c);
        d[n++] = c[0] * axis[0] + c[1] * axis[1] + c[2] * axis[2];
    }
    if (!n) return 0.0f;
    for (int i = 1; i < n; i++) {                    // descending insertion sort
        const float k = d[i]; int j = i - 1;
        while (j >= 0 && d[j] < k) { d[j + 1] = d[j]; j--; }
        d[j + 1] = k;
    }
    if (want <= 0) return d[0] + 1.0f;               // keep nothing
    if (want >= n) return d[n - 1] - 1.0f;           // keep everything
    return 0.5f * (d[want - 1] + d[want]);
}


// The forearm's direction, straight from the SKELETON: the hand bone minus the
// bone it hangs off. That bone is the hand's graph neighbour whose centroid is
// farthest away - fingers sit inside a hand's width, the forearm reaches back
// up the limb - so it needs no names and no hierarchy, like everything else
// here.
//
// This is a different claim from the PCA axis and can disagree with it. PCA
// answers "which way is this cloud of triangles longest", and a tapered sleeve
// can lean that answer off the bone; the bone pair answers "which way does the
// forearm point", which is the axis a ring should be square to. Both are
// computed, the angle between them is logged, and `ms axis` switches live -
// because which one is right is a question about this asset, not about
// geometry, and a run can settle it in one press.
static bool MsBoneAxis(int side, float* out)
{
    const int h = g_msHandBone[side];
    if (h < 0) return false;
    int f = -1; float best = -1.0f;
    for (int b = 0; b < g_msBones; b++) {
        if (g_msBoneSide[b] != side || b == h || !MsAdjGet(h, b)) continue;
        float e2 = 0.0f;
        for (int a = 0; a < 3; a++) {
            const float e = g_msBoneCen[b][a] - g_msBoneCen[h][a];
            e2 += e * e;
        }
        if (e2 > best) { best = e2; f = b; }
    }
    if (f < 0 || best <= 1e-8f) return false;
    const float len = sqrtf(best);
    for (int a = 0; a < 3; a++)
        out[a] = (g_msBoneCen[h][a] - g_msBoneCen[f][a]) / len;
    Log("ms: side %d - the forearm bone is %d, %.1f back from the hand bone %d, "
        "so the skeleton says the arm points (%.3f %.3f %.3f)",
        side, f, len, h, out[0], out[1], out[2]);
    return true;
}


// Derive the limb axis and the plane for one side. Two passes: a rough axis
// over the whole arm, then a refined one over a band around the rough cut, so
// the final ring is perpendicular to the FOREARM and not to the whole limb
// with the hand's mass pulling on it.
static bool MsPlaneDerive(int side)
{
    const int h = g_msHandBone[side];
    if (h < 0) return false;

    float axis[3];
    MsPca(side, NULL, 0.0f, 0.0f, axis);
    // Point it distally - toward the hand bone, away from the arm's own middle.
    {
        float mean[3] = { 0.0f, 0.0f, 0.0f }; int n = 0;
        for (int t = 0; t < g_msTris; t++) {
            if (g_msTriSide[t] != side) continue;
            float c[3]; MsTriCentroid(t, c);
            for (int a = 0; a < 3; a++) mean[a] += c[a];
            n++;
        }
        if (!n) return false;
        for (int a = 0; a < 3; a++) mean[a] /= n;
        float dot = 0.0f;
        for (int a = 0; a < 3; a++) dot += (g_msBoneCen[h][a] - mean[a]) * axis[a];
        if (dot < 0.0f) for (int a = 0; a < 3; a++) axis[a] = -axis[a];
    }

    float lo = 1e30f, hi = -1e30f;
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float c[3]; MsTriCentroid(t, c);
        const float d = c[0] * axis[0] + c[1] * axis[1] + c[2] * axis[2];
        if (d < lo) lo = d;
        if (d > hi) hi = d;
    }
    const float span = hi - lo;
    if (span <= 1e-4f) {
        Log("ms: REFUSED - side %d has no extent along its own axis", side);
        return false;
    }

    g_msSeedN[side] = MsSphereCount(side);
    const float cut0 = MsCutForCount(side, axis, g_msSeedN[side]);

    // Second pass: the forearm's own axis, from a band around that first cut.
    float axis2[3];
    MsPca(side, axis, cut0 - 0.16f * span, cut0 + 0.06f * span, axis2);
    {
        float dot = 0.0f;
        for (int a = 0; a < 3; a++) dot += axis2[a] * axis[a];
        if (dot < 0.0f) { for (int a = 0; a < 3; a++) axis2[a] = -axis2[a]; dot = -dot; }
        if (dot > 1.0f) dot = 1.0f;
        const float deg = acosf(dot) * 57.29578f;
        if (deg > 40.0f) {
            Log("ms: side %d - the forearm band's axis is %.0f degrees off the "
                "whole arm's, which is too far to be a refinement. Keeping the "
                "whole-arm axis; the ring may sit slightly slanted.", side, deg);
            for (int a = 0; a < 3; a++) axis2[a] = axis[a];
        } else {
            Log("ms: side %d - forearm axis refined %.1f degree(s) off the "
                "whole-arm direction", side, deg);
        }
    }

    // The skeleton's own answer, and how far it disagrees with the cloud's.
    {
        float bone[3];
        if (MsBoneAxis(side, bone)) {
            float dot = 0.0f;
            for (int a = 0; a < 3; a++) dot += bone[a] * axis2[a];
            if (dot > 1.0f) dot = 1.0f;
            if (dot < -1.0f) dot = -1.0f;
            Log("ms: side %d - the bone axis and the triangle-cloud axis are "
                "%.1f degrees apart. A ring square to the wrong one takes more "
                "off one side of the forearm than the other, which is what a "
                "slanted cut looks like. Using the %s axis (`ms axis bone|pca` "
                "switches, ini [Hands] WristAxis).",
                side, acosf(dot < 0.0f ? -dot : dot) * 57.29578f,
                g_msAxisMode == 0 ? "BONE" : "cloud");
            if (g_msAxisMode == 0) for (int a = 0; a < 3; a++) axis2[a] = bone[a];
        } else if (g_msAxisMode == 0) {
            Log("ms: side %d - no forearm bone could be identified, so the "
                "triangle-cloud axis is used whatever WristAxis says", side);
        }
    }

    for (int a = 0; a < 3; a++) g_msAxis[side][a] = axis2[a];
    float lo2 = 1e30f, hi2 = -1e30f;
    for (int t = 0; t < g_msTris; t++) {
        if (g_msTriSide[t] != side) continue;
        float c[3]; MsTriCentroid(t, c);
        const float d = c[0] * axis2[0] + c[1] * axis2[1] + c[2] * axis2[2];
        if (d < lo2) lo2 = d;
        if (d > hi2) hi2 = d;
    }
    g_msAxialLen[side] = hi2 - lo2;
    const float cut = MsCutForCount(side, axis2, g_msSeedN[side]);
    float hAx = 0.0f;
    for (int a = 0; a < 3; a++) hAx += g_msBoneCen[h][a] * axis2[a];
    if (!g_msCutSet[side]) g_msCutRel[side] = cut - hAx;
    Log("ms: side %d - axis (%.3f %.3f %.3f), the arm is %.1f long along it. "
        "The plane %s: %.1f from the hand bone, which is %.0f%% of the arm's "
        "length back from the fingertips. Each + / - press moves it %.1f.",
        side, axis2[0], axis2[1], axis2[2], g_msAxialLen[side],
        g_msCutSet[side] ? "came from the ini"
                         : "starts where the sphere was, keeping the same triangles",
        g_msCutRel[side], (hi2 - (hAx + g_msCutRel[side])) * 100.0f / g_msAxialLen[side],
        0.02f * g_msAxialLen[side]);
    return true;
}


// ---- the clip ---------------------------------------------------------------

// One vertex somewhere between two others. Every element the declaration
// carries is interpolated by its own type; a bone INDEX is a name and not a
// quantity, so blend indices - and the weights that go with them, which would
// otherwise name the wrong bones - come whole from the nearer parent.
static void MsLerpVertex(const uint8_t* va, const uint8_t* vb, float t, uint8_t* out)
{
    const uint8_t* near_ = (t < 0.5f) ? va : vb;
    memcpy(out, near_, g_msStride);
    for (int e = 0; e < g_msNel; e++) {
        const int off = g_msEl[e].Offset;
        if ((UINT)(off + 4) > g_msStride) continue;
        const uint8_t* pa = va + off;
        const uint8_t* pb = vb + off;
        uint8_t* po = out + off;
        if (g_msEl[e].Usage == D3DDECLUSAGE_BLENDINDICES ||
            g_msEl[e].Usage == D3DDECLUSAGE_BLENDWEIGHT) continue;   // from the parent
        switch (g_msEl[e].Type) {
        case D3DDECLTYPE_FLOAT1: case D3DDECLTYPE_FLOAT2:
        case D3DDECLTYPE_FLOAT3: case D3DDECLTYPE_FLOAT4: {
            const int n = g_msEl[e].Type - D3DDECLTYPE_FLOAT1 + 1;
            if ((UINT)(off + 4 * n) > g_msStride) break;
            for (int i = 0; i < n; i++) {
                float x, y;
                memcpy(&x, pa + 4 * i, 4); memcpy(&y, pb + 4 * i, 4);
                const float r = x + (y - x) * t;
                memcpy(po + 4 * i, &r, 4);
            }
            break;
        }
        case D3DDECLTYPE_D3DCOLOR: case D3DDECLTYPE_UBYTE4N:
            for (int i = 0; i < 4; i++) {
                const float r = pa[i] + ((float)pb[i] - (float)pa[i]) * t;
                po[i] = (uint8_t)(r < 0.0f ? 0.0f : (r > 255.0f ? 255.0f : r + 0.5f));
            }
            break;
        case D3DDECLTYPE_SHORT2N: case D3DDECLTYPE_SHORT4N:
        case D3DDECLTYPE_SHORT2:  case D3DDECLTYPE_SHORT4: {
            const int n = (g_msEl[e].Type == D3DDECLTYPE_SHORT2N ||
                           g_msEl[e].Type == D3DDECLTYPE_SHORT2) ? 2 : 4;
            if ((UINT)(off + 2 * n) > g_msStride) break;
            for (int i = 0; i < n; i++) {
                int16_t x, y;
                memcpy(&x, pa + 2 * i, 2); memcpy(&y, pb + 2 * i, 2);
                const float r = x + ((float)y - (float)x) * t;
                const int16_t o = (int16_t)(r < -32768.0f ? -32768.0f
                                          : (r > 32767.0f ? 32767.0f : r));
                memcpy(po + 2 * i, &o, 2);
            }
            break;
        }
        default: break;      // FLOAT16, UBYTE4, UDEC3 and friends: nearer parent
        }
    }
}


// Emit one triangle into the rebuilt list.
static void MsEmit(uint32_t a, uint32_t b, uint32_t c, int cls)
{
    if (g_msOutN >= MS_MAX_OUT) return;
    g_msOutIdx[g_msOutN * 3 + 0] = a;
    g_msOutIdx[g_msOutN * 3 + 1] = b;
    g_msOutIdx[g_msOutN * 3 + 2] = c;
    g_msOutCls[g_msOutN] = (uint8_t)cls;
    g_msOutN++;
}


// A new vertex t of the way from a to b, appended to our own buffer. Returns
// its index in the rebuilt vertex list.
static uint32_t MsClipVertex(uint32_t a, uint32_t b, float t)
{
    if (g_msClipN >= MS_MAX_CLIPV) return a;
    uint8_t* out = &g_msClipRaw[(size_t)g_msClipN * MS_MAX_STRIDE];
    MsLerpVertex(&g_msRaw[(size_t)a * MS_MAX_STRIDE],
                 &g_msRaw[(size_t)b * MS_MAX_STRIDE], t, out);
    return (uint32_t)(g_msVerts + g_msClipN++);
}


// The plane clip proper. `d` is each vertex's signed distance, positive on the
// hand side. Both halves are emitted, so the arm class stays the exact inverse
// of the hand class and no geometry is invented or lost.
static void MsClipTri(const uint32_t* v, const float* d, int handCls, int armCls,
                      int sd)
{
    int ni = 0;
    for (int i = 0; i < 3; i++) if (d[i] >= 0.0f) ni++;
    if (ni == 3) { MsEmit(v[0], v[1], v[2], handCls); return; }
    if (ni == 0) { MsEmit(v[0], v[1], v[2], armCls);  return; }

    // WINDING. Exactly one vertex is on its own side; call it `i` and take the
    // other two in CYCLIC order, (i+1) then (i+2). Picking them by ascending
    // index instead reverses the triangle whenever the odd vertex is the middle
    // one - a third of every cut - and a reversed triangle is culled, which is
    // precisely the "some pieces cut on one clean line, the rest missing or
    // floating loose" the first clipped build produced.
    int i = 0;
    const bool oddIsIn = (ni == 1);
    for (int k = 0; k < 3; k++) if ((d[k] >= 0.0f) == oddIsIn) { i = k; break; }
    const int j = (i + 1) % 3, k2 = (i + 2) % 3;

    const uint32_t a = v[i], b = v[j], c = v[k2];
    const float da = d[i], db = d[j], dc = d[k2];
    const uint32_t ab = MsClipVertex(a, b, da / (da - db));
    const uint32_t ca = MsClipVertex(a, c, da / (da - dc));

    // The corner piece keeps the original orientation; the quad on the far side
    // is fanned from the same edge, so both halves wind the same way the source
    // triangle did.
    const int cornerCls = oddIsIn ? handCls : armCls;
    const int quadCls   = oddIsIn ? armCls  : handCls;
    MsEmit(a, ab, ca, cornerCls);
    MsEmit(ab, b, c, quadCls);
    MsEmit(ab, c, ca, quadCls);

    // The boundary segment this triangle contributed, stored in the direction
    // the HAND-side piece walks it. The corner piece walks ab -> ca and the
    // quad piece walks ca -> ab, so which of the two is the hand depends on
    // which side the odd vertex fell. A cap triangle must walk the shared edge
    // the other way round from the surface it closes, and storing one agreed
    // direction here is what lets both caps be wound from the same record.
    if (g_msCapSegN < MS_MAX_CAPSEG && sd > 0) {
        MsCapSeg* e = &g_msCapSeg[g_msCapSegN++];
        e->a = oddIsIn ? ab : ca;
        e->b = oddIsIn ? ca : ab;
        e->sd = (uint8_t)sd;
    }
}


// ---- the cap ----------------------------------------------------------------

// Where an element with this usage sits, or -1. The declaration was kept whole
// in MsDecl precisely so the cap can ask questions like this one.
static int MsElemFind(int usage, int usageIndex)
{
    for (int e = 0; e < g_msNel; e++)
        if (g_msEl[e].Usage == usage && g_msEl[e].UsageIndex == usageIndex)
            return e;
    return -1;
}


// The raw bytes behind a rebuilt-list vertex index, from the game's window or
// from the vertices the clip made.
static const uint8_t* MsRawOf(uint32_t v)
{
    if ((int)v < g_msVerts) return &g_msRaw[(size_t)v * MS_MAX_STRIDE];
    const int c = (int)v - g_msVerts;
    return &g_msClipRaw[(size_t)(c >= 0 && c < g_msClipN ? c : 0) * MS_MAX_STRIDE];
}


// A cap vertex: appearance (texture coordinate, colour, tangent frame) copied
// whole from `look`, position and SKINNING taken from `skin`, and the normal
// forced flat to the cut plane. Returns its index, or `skin` itself if there is
// no room left - a degenerate triangle rather than a wild one.
static uint32_t MsCapVertex(uint32_t skin, const uint8_t* look, const float* p,
                            const float* n, int posOff, int nrmOff)
{
    if (g_msClipN >= MS_MAX_CLIPV) return skin;
    uint8_t* out = &g_msClipRaw[(size_t)g_msClipN * MS_MAX_STRIDE];
    memcpy(out, look, g_msStride);
    const uint8_t* sv = MsRawOf(skin);
    for (int e = 0; e < g_msNel; e++) {
        if (g_msEl[e].Usage != D3DDECLUSAGE_BLENDINDICES &&
            g_msEl[e].Usage != D3DDECLUSAGE_BLENDWEIGHT) continue;
        const int off = g_msEl[e].Offset;
        if ((UINT)(off + 16) <= g_msStride)     memcpy(out + off, sv + off, 16);
        else if ((UINT)(off + 4) <= g_msStride) memcpy(out + off, sv + off, 4);
    }
    memcpy(out + posOff, p, 12);
    if (nrmOff >= 0) memcpy(out + nrmOff, n, 12);
    return (uint32_t)(g_msVerts + g_msClipN++);
}


// Half floats, because this asset stores its texture coordinates as FLOAT16_2.
// The first build of the cap asked for a FLOAT2 and the log answered "NO
// TEXCOORD0", so the mode never ran at all and every cap silently fell back to
// ring vertex 0 - which happened to look right, and would not have on a ring
// that landed on a seam. A refusal that produces a plausible picture is the
// worst kind, so the decode is here rather than the check being loosened.
static float MsHalf(uint16_t h)
{
    const int ex = (h >> 10) & 0x1F, ma = h & 0x3FF;
    const float sg = (h & 0x8000) ? -1.0f : 1.0f;
    if (ex == 0)    return sg * ma * (1.0f / 16384.0f) * (1.0f / 1024.0f);
    if (ex == 31)   return sg * (ma ? 0.0f : 1e30f);      // NaN reads as 0
    return sg * (1.0f + ma / 1024.0f) * powf(2.0f, (float)(ex - 15));
}


// One texture coordinate out of a vertex, whatever the declaration packed it
// as. Only the MODE compares these, and the cap's bytes are copied whole from
// the winning vertex, so an encoding this does not know costs a worse choice of
// donor and never a corrupt vertex.
static bool MsReadUv(const uint8_t* v, int off, int type, float* uv)
{
    const uint8_t* p = v + off;
    switch (type) {
    case D3DDECLTYPE_FLOAT2: case D3DDECLTYPE_FLOAT3: case D3DDECLTYPE_FLOAT4:
        memcpy(uv, p, 8); return true;
    case D3DDECLTYPE_FLOAT16_2: case D3DDECLTYPE_FLOAT16_4: {
        uint16_t h[2]; memcpy(h, p, 4);
        uv[0] = MsHalf(h[0]); uv[1] = MsHalf(h[1]); return true;
    }
    case D3DDECLTYPE_SHORT2: case D3DDECLTYPE_SHORT4: {
        int16_t q[2]; memcpy(q, p, 4);
        uv[0] = (float)q[0]; uv[1] = (float)q[1]; return true;
    }
    case D3DDECLTYPE_SHORT2N: case D3DDECLTYPE_SHORT4N: {
        int16_t q[2]; memcpy(q, p, 4);
        uv[0] = q[0] / 32767.0f; uv[1] = q[1] / 32767.0f; return true;
    }
    case D3DDECLTYPE_UBYTE4: case D3DDECLTYPE_UBYTE4N:
        uv[0] = p[0] / 255.0f; uv[1] = p[1] / 255.0f; return true;
    case D3DDECLTYPE_D3DCOLOR:
        uv[0] = p[2] / 255.0f; uv[1] = p[1] / 255.0f; return true;
    default: return false;
    }
}


// How many bytes an element of this type occupies, or 0 for one this module
// does not size. The cap needs it only to prove a read stays inside the vertex.
static int MsTypeSize(int type)
{
    switch (type) {
    case D3DDECLTYPE_FLOAT1: return 4;
    case D3DDECLTYPE_FLOAT2: return 8;
    case D3DDECLTYPE_FLOAT3: return 12;
    case D3DDECLTYPE_FLOAT4: return 16;
    case D3DDECLTYPE_D3DCOLOR: case D3DDECLTYPE_UBYTE4:
    case D3DDECLTYPE_UBYTE4N: case D3DDECLTYPE_SHORT2:
    case D3DDECLTYPE_SHORT2N: case D3DDECLTYPE_FLOAT16_2: return 4;
    case D3DDECLTYPE_SHORT4: case D3DDECLTYPE_SHORT4N:
    case D3DDECLTYPE_FLOAT16_4: return 8;
    default: return 0;
    }
}


// The most common texture coordinate around a ring: for each ring vertex, how
// many of the others sit within a small radius of it in UV space, and the
// winner takes the cap. This is a MODE and not a mean on purpose - a mean lands
// between the islands of an atlas and samples whatever is parked there, which
// is how a cap ends up the colour of some unrelated part of the character.
static int MsRingUvMode(const uint32_t* ring, int n, int uvOff, int uvType)
{
    if (n <= 0) return -1;
    if (uvOff < 0) return 0;
    float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
    for (int i = 0; i < n; i++) {
        float uv[2];
        if (!MsReadUv(MsRawOf(ring[i]), uvOff, uvType, uv)) return 0;
        for (int k = 0; k < 2; k++) {
            if (uv[k] < lo[k]) lo[k] = uv[k];
            if (uv[k] > hi[k]) hi[k] = uv[k];
        }
    }
    const float dx = hi[0] - lo[0], dy = hi[1] - lo[1];
    // A sixth of the ring's own UV spread: wide enough that the neighbours of a
    // representative vertex all count, narrow enough that a second island of
    // the atlas does not vote for the first.
    const float r = sqrtf(dx * dx + dy * dy) * 0.166f;
    if (!(r > 0.0f)) return 0;
    const float r2 = r * r;
    int best = 0, bestN = -1;
    for (int i = 0; i < n; i++) {
        float a[2];
        if (!MsReadUv(MsRawOf(ring[i]), uvOff, uvType, a)) continue;
        int c = 0;
        for (int j = 0; j < n; j++) {
            float b[2];
            if (!MsReadUv(MsRawOf(ring[j]), uvOff, uvType, b)) continue;
            const float ex = b[0] - a[0], ey = b[1] - a[1];
            if (ex * ex + ey * ey <= r2) c++;
        }
        if (c > bestN) { bestN = c; best = i; }
    }
    return best;
}


// Close both stumps at the cut: one fan per side, per class.
static void MsCaps(void)
{
    g_msCapTris = 0;
    g_msCapUv[1] = g_msCapUv[2] = -1;
    if (!g_msCap || !g_msCapSegN) return;

    const int pe = MsElemFind(D3DDECLUSAGE_POSITION, 0);
    if (pe < 0 || g_msEl[pe].Type != D3DDECLTYPE_FLOAT3 ||
        (UINT)(g_msEl[pe].Offset + 12) > g_msStride) {
        Log("ms: the cut end is NOT capped - POSITION is not a FLOAT3 this "
            "module can write back, so a cap vertex could not be placed");
        return;
    }
    const int posOff = g_msEl[pe].Offset;
    const int ne = MsElemFind(D3DDECLUSAGE_NORMAL, 0);
    const int nrmOff = (ne >= 0 && g_msEl[ne].Type == D3DDECLTYPE_FLOAT3 &&
                        (UINT)(g_msEl[ne].Offset + 12) <= g_msStride)
                       ? g_msEl[ne].Offset : -1;
    const int ue = MsElemFind(D3DDECLUSAGE_TEXCOORD, 0);
    const int uvSz = (ue >= 0) ? MsTypeSize(g_msEl[ue].Type) : 0;
    const int uvType = (ue >= 0) ? g_msEl[ue].Type : 0;
    const int uvOff = (ue >= 0 && uvSz >= 4 &&
                       (UINT)(g_msEl[ue].Offset + uvSz) <= g_msStride)
                      ? g_msEl[ue].Offset : -1;

    static uint32_t ring[MS_MAX_CAPSEG * 2];
    const int ringMax = (int)(sizeof(ring) / sizeof(ring[0]));
    for (int sd = 1; sd <= 2; sd++) {
        // The ring: every endpoint of every boundary segment on this side.
        int n = 0;
        for (int i = 0; i < g_msCapSegN && n + 2 <= ringMax; i++) {
            if (g_msCapSeg[i].sd != sd) continue;
            ring[n++] = g_msCapSeg[i].a;
            ring[n++] = g_msCapSeg[i].b;
        }
        if (n < 6) continue;   // fewer than three segments is not a ring

        float cen[3] = { 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < n; i++) {
            float q[3]; memcpy(q, MsRawOf(ring[i]) + posOff, 12);
            for (int a = 0; a < 3; a++) cen[a] += q[a];
        }
        for (int a = 0; a < 3; a++) cen[a] /= (float)n;

        const int mode = MsRingUvMode(ring, n, uvOff, uvType);
        const uint8_t* look = MsRawOf(ring[mode < 0 ? 0 : mode]);
        g_msCapUv[sd] = mode;

        // The ring vertex nearest the centre lends the centre its skinning: it
        // is the closest thing on the rim to being in the middle, so the fan's
        // hub follows the same bones as the ring around it and the disc cannot
        // swim away from the arm it closes.
        int hub = 0; float bestD = 1e30f;
        for (int i = 0; i < n; i++) {
            float q[3]; memcpy(q, MsRawOf(ring[i]) + posOff, 12);
            float e2 = 0.0f;
            for (int a = 0; a < 3; a++) { const float e = q[a] - cen[a]; e2 += e * e; }
            if (e2 < bestD) { bestD = e2; hub = i; }
        }

        // The hand's piece is the +axis side, so its cap faces back down the
        // arm; the sleeve's cap faces the other way. Each gets its own hub and
        // rim vertices, because the normal is the one thing they cannot share.
        //
        // AND EACH IS EMITTED TWICE, facing both ways, unless that is turned
        // off. This is not insurance against getting the winding wrong - it is
        // what a plug in an open end actually needs. In hands-only mode the arm
        // is not drawn at all, so nothing stands between the eye and the BACK
        // of the hand's cap; a one-sided disc would be culled from exactly the
        // angle the hole was visible from, and the hole would still be there.
        const float* ax = g_msAxis[sd];
        for (int pass = 0; pass < 2; pass++) {
            const bool handSide = (pass == 0);
            const int cls = handSide ? (MS_CLS_HAND_A + sd - 1)
                                     : (MS_CLS_ARM_A + sd - 1);
            for (int face = 0; face < (g_msCapTwo ? 2 : 1); face++) {
                const float sign = (face == 0) ? 1.0f : -1.0f;
                float nrm[3];
                for (int a = 0; a < 3; a++)
                    nrm[a] = (handSide ? -ax[a] : ax[a]) * sign;
                const uint32_t hubV = MsCapVertex(ring[hub], look, cen, nrm, posOff, nrmOff);
                for (int i = 0; i < g_msCapSegN; i++) {
                    if (g_msCapSeg[i].sd != sd) continue;
                    float pa[3], pb[3];
                    memcpy(pa, MsRawOf(g_msCapSeg[i].a) + posOff, 12);
                    memcpy(pb, MsRawOf(g_msCapSeg[i].b) + posOff, 12);
                    const uint32_t va = MsCapVertex(g_msCapSeg[i].a, look, pa, nrm, posOff, nrmOff);
                    const uint32_t vb = MsCapVertex(g_msCapSeg[i].b, look, pb, nrm, posOff, nrmOff);
                    // The surface walks a -> b on the hand side, so the cap that
                    // closes it walks the same edge b -> a; the sleeve's cap is
                    // the mirror of that, and the back face of each is the
                    // reverse again.
                    const bool fwd = (handSide != (face != 0));
                    if (fwd) MsEmit(hubV, vb, va, cls);
                    else     MsEmit(hubV, va, vb, cls);
                    g_msCapTris++;
                }
            }
        }
    }

    if (g_msClipN >= MS_MAX_CLIPV)
        Log("ms: WARNING - ran out of vertex room while capping at %d, so part "
            "of the cut end is left open", MS_MAX_CLIPV);
    Log("ms: cut end CAPPED %s - %d triangle(s) closing the stumps. Colour: %s "
        "(TEXCOORD0 off=%d type=%d), winning ring vertex %d on side A and %d on "
        "side B. Normal: %s. [Hands] CutCap=0 turns it off and the hole it "
        "leaves is the pre-cap look; CutCapTwoSided=0 halves it, which is the "
        "check for a cap that is present but facing away.",
        g_msCapTwo ? "on BOTH faces" : "on one face only", g_msCapTris,
        uvOff >= 0 ? "the MODE of the ring's own texture coordinates"
                   : "ring vertex 0 copied whole - this declaration has no "
                     "TEXCOORD0 this module can decode, so the choice of donor "
                     "is arbitrary and a ring landing on a seam would show it",
        ue >= 0 ? g_msEl[ue].Offset : -1, ue >= 0 ? g_msEl[ue].Type : -1,
        g_msCapUv[1], g_msCapUv[2],
        nrmOff >= 0 ? "forced flat to the cut plane"
                    : "inherited from the donor vertex - NORMAL is not a FLOAT3 "
                      "here, and re-encoding a packed normal without knowing "
                      "the asset's bias would be a guess");
}


static void MsClassify(void)
{
    // The sphere is kept as the fallback shape and as the thing the plane is
    // seeded from - not deleted, because it is what proved the classification
    // works at all, and it is one ini key away if the plane misbehaves.
    for (int b = 0; b < g_msBones; b++) {
        if (!g_msBoneSide[b]) { g_msBoneHand[b] = 0; continue; }
        const int s = g_msBoneSide[b];
        const int h = g_msHandBone[s];
        if (h < 0) { g_msBoneHand[b] = 0; continue; }
        float e2 = 0.0f;
        for (int a = 0; a < 3; a++) {
            const float e = g_msBoneCen[b][a] - g_msBoneCen[h][a];
            e2 += e * e;
        }
        g_msBoneHand[b] = (sqrtf(e2) <= g_msWristR[s] * g_msWristScale[s]) ? 1 : 0;
    }

    float cut[3] = { 0.0f, 0.0f, 0.0f };
    for (int s = 1; s <= 2; s++) {
        const int h = g_msHandBone[s];
        if (h < 0) continue;
        for (int a = 0; a < 3; a++) cut[s] += g_msBoneCen[h][a] * g_msAxis[s][a];
        cut[s] += g_msCutRel[s];
    }
    // Clipping needs a vertex buffer of our own; without one the rule falls
    // back to whole triangles, and says so rather than silently doing something
    // else than the mode claims.
    const bool clip = (g_msEdge == 3) && g_msPlane && g_msOwnVb;
    if (g_msEdge == 3 && !clip)
        Log("ms: the clip was asked for and is not available (%s) - keeping "
            "whole triangles whose three vertices are all past the plane",
            !g_msPlane ? "the cut is a sphere, which has no plane to clip to"
                       : "stream 0 is not the only stream");

    g_msOutN = 0; g_msClipN = 0; g_msCapSegN = 0;
    int clipped = 0;
    for (int t = 0; t < g_msTris; t++) {
        uint32_t v[3];
        for (int k = 0; k < 3; k++) v[k] = g_msIdx[t * 3 + k] - g_msMinIndex;
        const int sd = g_msTriSide[t];
        if (!sd) { MsEmit(v[0], v[1], v[2], MS_CLS_OTHER); continue; }
        const int handCls = sd - 1;                 // HAND_A / HAND_B
        const int armCls  = MS_CLS_ARM_A + sd - 1;
        if (!g_msPlane) {
            float hw = 0.0f, aw = 0.0f;
            for (int k = 0; k < 3; k++) {
                if ((int)v[k] >= g_msVerts) continue;
                const MsVert* q = &g_msVert[v[k]];
                for (int j = 0; j < 4; j++) {
                    const int b = q->bi[j];
                    if (b >= g_msBones || q->bw[j] <= 0.0f || g_msBoneSide[b] != sd) continue;
                    if (g_msBoneHand[b]) hw += q->bw[j]; else aw += q->bw[j];
                }
            }
            MsEmit(v[0], v[1], v[2], (hw > aw) ? handCls : armCls);
            continue;
        }
        const float* ax = g_msAxis[sd];
        float d[3];
        for (int k = 0; k < 3; k++) {
            const float* q = ((int)v[k] < g_msVerts) ? g_msVert[v[k]].p : g_msVert[0].p;
            d[k] = q[0] * ax[0] + q[1] * ax[1] + q[2] * ax[2] - cut[sd];
        }
        if (clip) {
            if ((d[0] >= 0.0f) != (d[1] >= 0.0f) || (d[1] >= 0.0f) != (d[2] >= 0.0f))
                clipped++;
            MsClipTri(v, d, handCls, armCls, sd);
        } else {
            int past = 0;
            for (int k = 0; k < 3; k++) if (d[k] >= 0.0f) past++;
            bool hand;
            if (g_msEdge == 0)      hand = ((d[0] + d[1] + d[2]) >= 0.0f);
            else if (g_msEdge == 2) hand = (past > 0);
            else                    hand = (past == 3);
            MsEmit(v[0], v[1], v[2], hand ? handCls : armCls);
        }
    }

    // The cap is built from the ring the clip just produced, so it goes after
    // the loop and before anything counts triangles.
    MsCaps();

    int count[MS_CLS_N]; memset(count, 0, sizeof(count));
    for (int i = 0; i < g_msOutN; i++) count[g_msOutCls[i]]++;
    memcpy(g_msClsCount, count, sizeof(count));
    Log("ms: triangles by class - handA %d, handB %d, armA %d, armB %d, "
        "unclassified %d; %d out of %d source triangle(s), %d of them CUT by "
        "the plane into %d new vertex(es), of which %d triangle(s) are the CAP "
        "closing the cut end. Rule: %s.",
        count[MS_CLS_HAND_A], count[MS_CLS_HAND_B], count[MS_CLS_ARM_A],
        count[MS_CLS_ARM_B], count[MS_CLS_OTHER], g_msOutN, g_msTris,
        clipped, g_msClipN, g_msCapTris,
        !g_msPlane ? "SPHERE around the hand bone (moves in whole bones)"
        : clip      ? "PLANE, triangles that straddle it are CLIPPED at it - the "
                      "boundary is the plane itself, not a row of triangle edges"
        : g_msEdge == 0 ? "PLANE, kept when the triangle's centroid is past it"
        : g_msEdge == 2 ? "PLANE, kept when any vertex is past it"
                        : "PLANE, kept only when all three vertices are past it");
    if (g_msOutN >= MS_MAX_OUT)
        Log("ms: WARNING - the rebuilt triangle list hit its %d ceiling, so the "
            "tail of the mesh was dropped", MS_MAX_OUT);
    if (g_msClipN >= MS_MAX_CLIPV)
        Log("ms: WARNING - ran out of room for clipped vertices at %d; the rest "
            "of the ring is not cut cleanly", MS_MAX_CLIPV);
    if (!count[MS_CLS_HAND_A] || !count[MS_CLS_HAND_B])
        Log("ms: WARNING - one hand came out with NO triangles. The wrist knob "
            "(Numpad + / -) will not rescue that; it means the side split is "
            "wrong, and the side line above says how it was made.");
}


// ---- step 6: our buffers ----------------------------------------------------

static bool MsUpload(IDirect3DDevice9* dev)
{
    if (!dev) return false;
    // A REBUILD IS A NEW SOURCE GENERATION. The anchor identities and the
    // frozen orientation slot are about to be re-derived, so anything solved
    // against the old ones - the grip transform above all - stops being valid.
    // An index that is still inside the palette does not prove it names the
    // same transform, and a stale G applied against a changed source-frame
    // convention would look like a grip that is simply wrong.
    g_mpSrcGen++;
    g_mpSrcOk[0] = g_mpSrcOk[1] = false;
    g_mpSrcRefOk[0] = g_mpSrcRefOk[1] = false;
    g_mpCtlRefOk[0] = g_mpCtlRefOk[1] = false;
    if (g_msIbDev != dev) {
        if (g_msIb) { g_msIb->Release(); g_msIb = NULL; }
        if (g_msVb) { g_msVb->Release(); g_msVb = NULL; }
    }
    const int nv = g_msVerts + g_msClipN;

    // The VERTEX buffer, when stream 0 is ours to own: the draw's window
    // verbatim, then the vertices the clip made. Indices are re-based onto it,
    // so the draw runs with baseVertex 0 and a window starting at 0.
    if (g_msOwnVb) {
        const UINT vbytes = (UINT)nv * g_msStride;
        if (g_msVb) {
            D3DVERTEXBUFFER_DESC d;
            if (FAILED(g_msVb->GetDesc(&d)) || d.Size < vbytes) {
                g_msVb->Release(); g_msVb = NULL;
            }
        }
        if (!g_msVb) {
            // Room to grow, so moving the ring does not recreate the buffer on
            // every press. MANAGED for the same reason as the index buffer:
            // nothing for hkReset to release.
            const UINT room = vbytes + 1024u * g_msStride;
            if (FAILED(dev->CreateVertexBuffer(room, D3DUSAGE_WRITEONLY, 0,
                                               D3DPOOL_MANAGED, &g_msVb, NULL)) ||
                !g_msVb) {
                // NEVER SUBMIT GENERATED-VERTEX INDICES WITHOUT THE VERTICES.
                // The clip has already run by the time we get here, so
                // g_msOutIdx contains indices at or past g_msVerts that exist
                // ONLY in the buffer we just failed to create. Clearing the
                // flag would send those indices to the GAME's vertex buffer,
                // where they address whatever happens to be there. Falling
                // back is not free once vertices have been invented.
                Log("ms: REFUSED - could not create a %u byte vertex buffer "
                    "(%d clipped vertex(es) already generated). Re-deriving "
                    "WITHOUT the clip rather than pointing generated indices at "
                    "the game's vertices, which would draw garbage.",
                    room, g_msClipN);
                g_msOwnVb = false;
                g_msDegraded = true;
                MsClassify();          // whole triangles, no invented vertices
                if (g_msClipN) {
                    Log("ms: REFUSED - the reclassify still produced %d clipped "
                        "vertex(es) with no buffer to hold them. Standing the "
                        "split down entirely.", g_msClipN);
                    return false;
                }
            }
        }
    }
    if (g_msOwnVb && g_msVb) {
        void* vp = NULL;
        if (FAILED(g_msVb->Lock(0, (UINT)nv * g_msStride, &vp, 0)) || !vp) {
            Log("ms: REFUSED - our own vertex buffer would not lock");
            return false;
        }
        for (int v = 0; v < g_msVerts; v++)
            memcpy((uint8_t*)vp + (size_t)v * g_msStride,
                   &g_msRaw[(size_t)v * MS_MAX_STRIDE], g_msStride);
        for (int v = 0; v < g_msClipN; v++)
            memcpy((uint8_t*)vp + (size_t)(g_msVerts + v) * g_msStride,
                   &g_msClipRaw[(size_t)v * MS_MAX_STRIDE], g_msStride);
        g_msVb->Unlock();
    }

    // 32-bit indices if the rebuilt list can outgrow a 16-bit one. It cannot on
    // this asset, but the arithmetic is what decides, not the asset.
    const bool wide = (nv > 65535) || (g_msIbFmt == D3DFMT_INDEX32);
    const D3DFORMAT ifmt = wide ? D3DFMT_INDEX32 : D3DFMT_INDEX16;
    const UINT istride = wide ? 4u : 2u;
    const UINT ibytes = (UINT)g_msOutN * 3u * istride;
    if (g_msIb) {
        D3DINDEXBUFFER_DESC d;
        if (FAILED(g_msIb->GetDesc(&d)) || d.Size < ibytes || d.Format != ifmt) {
            g_msIb->Release(); g_msIb = NULL;
        }
    }
    if (!g_msIb) {
        const UINT room = ibytes + 4096u * istride;
        if (FAILED(dev->CreateIndexBuffer(room, D3DUSAGE_WRITEONLY, ifmt,
                                          D3DPOOL_MANAGED, &g_msIb, NULL)) ||
            !g_msIb) {
            Log("ms: REFUSED - could not create a %u byte index buffer", room);
            return false;
        }
    }
    g_msIbDev = dev;
    void* p = NULL;
    if (FAILED(g_msIb->Lock(0, ibytes, &p, 0)) || !p) {
        Log("ms: REFUSED - our own index buffer would not lock");
        return false;
    }
    int at = 0;
    for (int cls = 0; cls < MS_CLS_N; cls++) {
        g_msClsStart[cls] = at;
        int n = 0;
        for (int t = 0; t < g_msOutN; t++) {
            if (g_msOutCls[t] != cls) continue;
            for (int c = 0; c < 3; c++) {
                // Re-based onto our vertex buffer, or left as the game's own
                // values when the game's buffer is the one being drawn from.
                const uint32_t v = g_msOwnVb ? g_msOutIdx[t * 3 + c]
                                             : g_msOutIdx[t * 3 + c] + g_msMinIndex;
                if (istride == 2) ((uint16_t*)p)[at * 3 + c] = (uint16_t)v;
                else              ((uint32_t*)p)[at * 3 + c] = v;
            }
            at++; n++;
        }
        g_msClsCount[cls] = n;

        // THE PALM ANCHOR for this class.
        //
        // The first version walked triangle corners on an index-modulo rule.
        // Triangle order is not spatial order, so that sampled the WHOLE hand,
        // fingers included, and could take the same vertex twice. An anchor
        // containing finger vertices moves when the fingers animate, and
        // pinning that average then drags the palm in response to finger
        // motion - the correction would fight the animation it is supposed to
        // ride on top of.
        //
        // So take a spatially COMPACT patch: the vertices nearest the hand
        // class's own bind-pose centroid. Fingers are the extremities of that
        // cloud and fall out naturally, without needing to identify a joint.
        // Deduplicated, ORIGINAL vertices only (the clip's generated vertices
        // have no blend data to skin with), and fixed from here on.
        if (cls == MS_CLS_HAND_A || cls == MS_CLS_HAND_B) {
            g_mpAnchorN[cls] = 0;
            float cen[3] = { 0.0f, 0.0f, 0.0f };
            int cn = 0;
            for (int t = 0; t < g_msOutN; t++) {
                if (g_msOutCls[t] != cls) continue;
                for (int c = 0; c < 3; c++) {
                    const uint32_t v = g_msOutIdx[t * 3 + c];
                    if ((int)v >= g_msVerts) continue;
                    cen[0] += g_msVert[v].p[0]; cen[1] += g_msVert[v].p[1];
                    cen[2] += g_msVert[v].p[2]; cn++;
                }
            }
            if (cn > 0) {
                cen[0] /= (float)cn; cen[1] /= (float)cn; cen[2] /= (float)cn;
                // Selection sort of the nearest MP_ANCHOR_N, deduplicated.
                float best[MP_ANCHOR_N];
                for (int k = 0; k < MP_ANCHOR_N; k++) best[k] = 3.4e38f;
                for (int t = 0; t < g_msOutN; t++) {
                    if (g_msOutCls[t] != cls) continue;
                    for (int c = 0; c < 3; c++) {
                        const uint32_t v = g_msOutIdx[t * 3 + c];
                        if ((int)v >= g_msVerts) continue;
                        bool dup = false;
                        for (int k = 0; k < g_mpAnchorN[cls] && !dup; k++)
                            if (g_mpAnchorIdx[cls][k] == v) dup = true;
                        if (dup) continue;
                        const float dx = g_msVert[v].p[0] - cen[0];
                        const float dy = g_msVert[v].p[1] - cen[1];
                        const float dz = g_msVert[v].p[2] - cen[2];
                        const float d2 = dx*dx + dy*dy + dz*dz;
                        int at = -1;
                        for (int k = 0; k < MP_ANCHOR_N; k++)
                            if (d2 < best[k]) { at = k; break; }
                        if (at < 0) continue;
                        for (int k = MP_ANCHOR_N - 1; k > at; k--) {
                            best[k] = best[k - 1];
                            g_mpAnchorIdx[cls][k] = g_mpAnchorIdx[cls][k - 1];
                        }
                        best[at] = d2;
                        g_mpAnchorIdx[cls][at] = v;
                        if (g_mpAnchorN[cls] < MP_ANCHOR_N) g_mpAnchorN[cls]++;
                    }
                }
            }
            // How tight the patch actually is, so "compact" is a number rather
            // than an intention: a wide radius means fingers are still in it.
            float rad = 0.0f;
            for (int k = 0; k < g_mpAnchorN[cls]; k++) {
                const MsVert* v = &g_msVert[g_mpAnchorIdx[cls][k]];
                const float dx = v->p[0] - cen[0], dy = v->p[1] - cen[1],
                            dz = v->p[2] - cen[2];
                const float d = sqrtf(dx*dx + dy*dy + dz*dz);
                if (d > rad) rad = d;
            }
            Log("ms/palette/anchor: class %s - %d vertex(es) within %.2f uu of "
                "the class centroid (%.1f %.1f %.1f), from %d triangle(s). "
                "FIXED identities, deduplicated, bind-pose compact. A radius "
                "approaching the hand's own size would mean fingers are in the "
                "patch and their animation would drag the anchor.",
                cls == MS_CLS_HAND_A ? "A (left)" : "B (right)",
                g_mpAnchorN[cls], rad, cen[0], cen[1], cen[2], n);

            // THE DOMINANT PALETTE SLOT for this class's orientation source.
            // Frozen here, with the anchor, against this source generation -
            // a rebuild or a reset invalidates both, because an index still
            // inside the palette does not prove it names the same transform.
            //
            // This is a SLOT, not a joint. It is chosen by weight, and weight
            // does not establish anatomy: whether it follows the palm rigidly
            // is what the frame instrument has to measure.
            {
                float w[256]; memset(w, 0, sizeof(w));
                float tot = 0.0f;
                for (int k = 0; k < g_mpAnchorN[cls]; k++) {
                    const MsVert* v = &g_msVert[g_mpAnchorIdx[cls][k]];
                    for (int i = 0; i < 4; i++) {
                        const int b = (int)v->bi[i];
                        if (v->bw[i] > 0.0f && b >= 0 && b < 256)
                            { w[b] += v->bw[i]; tot += v->bw[i]; }
                    }
                }
                int best = -1; float bestW = 0.0f, secondW = 0.0f;
                for (int b = 0; b < 256; b++) {
                    if (w[b] > bestW) { secondW = bestW; bestW = w[b]; best = b; }
                    else if (w[b] > secondW) secondW = w[b];
                }
                g_mpDomSlot[cls]   = best;
                g_mpDomWeight[cls] = (tot > 0.0f) ? bestW / tot : 0.0f;
                Log("ms/palette/frame: class %s - orientation will be read from "
                    "palette SLOT %d, which carries %.0f%% of the anchor's weight "
                    "(runner-up %.0f%%), frozen against source generation %u. This "
                    "is a render slot, NOT a named joint: dominant weight does not "
                    "prove it follows the palm rather than a finger or the "
                    "forearm, and a skinning matrix can carry an inverse-bind "
                    "rotation. The ms/palette/frame beat measures whether it "
                    "actually tracks the palm; any fixed bind orientation is "
                    "absorbed into the grip transform G.",
                    cls == MS_CLS_HAND_A ? "A (left)" : "B (right)",
                    best, (double)(g_mpDomWeight[cls] * 100.0f),
                    (double)((tot > 0.0f ? secondW / tot : 0.0f) * 100.0f),
                    g_mpSrcGen);
                const int h = (cls == MS_CLS_HAND_B) ? 1 : 0;
                g_mpSrcRefOk[h] = false; g_mpCtlRefOk[h] = false;
            }
        }
    }
    g_msIb->Unlock();
    Log("ms: buffers rebuilt - handA %d@%d, handB %d@%d, armA %d@%d, armB "
        "%d@%d, other %d@%d (triangle count @ triangle offset), %d vertex(es) "
        "in %s. The two hand classes are adjacent on purpose, so drawing both "
        "hands is ONE call over a contiguous range.",
        g_msClsCount[0], g_msClsStart[0], g_msClsCount[1], g_msClsStart[1],
        g_msClsCount[2], g_msClsStart[2], g_msClsCount[3], g_msClsStart[3],
        g_msClsCount[4], g_msClsStart[4], nv,
        g_msOwnVb ? "a vertex buffer of ours" : "the game's own vertex buffer");
    return true;
}


// Reclassify and refill from the copy already in memory. No engine buffer is
// touched, so the wrist knob costs nothing but arithmetic.
static bool MsReclassify(IDirect3DDevice9* dev)
{
    if (!g_msVerts || !g_msTris || !dev) return false;
    MsClassify();
    return MsUpload(dev);
}


static bool MsBuild(IDirect3DDevice9* dev, INT baseVertex, UINT minIndex,
                    UINT numVertices, UINT startIndex, UINT primCount, uint32_t bones)
{
    g_msReady = 0;
    Log("ms: ==== deriving the hand/arm split from bone influence ==== "
        "(draw: base %d, min %u, %u verts, start %u, %u tris, palette %u bones)",
        baseVertex, minIndex, numVertices, startIndex, primCount, bones);
    g_msRetryLater = false;

    // CHEAP FIRST. Ask whether this draw can be clipped BEFORE locking and
    // copying its buffers - the first version declined only after the full
    // readback, so every skipped candidate paid for two buffer locks and a
    // 2771-vertex copy it then threw away.
    if (g_msEdge == 3 && g_msStreamSkips < MS_MAX_STREAM_SKIPS) {
        IDirect3DVertexDeclaration9* d0 = NULL;
        uint32_t mask = 0;
        if (SUCCEEDED(dev->GetVertexDeclaration(&d0)) && d0) {
            D3DVERTEXELEMENT9 el0[MAXD3DDECLLENGTH]; UINT n0 = 0;
            if (SUCCEEDED(d0->GetDeclaration(el0, &n0))) {
                if (n0 > MAXD3DDECLLENGTH) n0 = MAXD3DDECLLENGTH;
                for (UINT i = 0; i < n0; i++)
                    if (el0[i].Type != D3DDECLTYPE_UNUSED && el0[i].Stream < 32)
                        mask |= (1u << el0[i].Stream);
            }
            d0->Release();
        }
        int veto = -1;
        for (UINT si = 1; si < 8 && mask; si++) {
            if (!(mask & (1u << si))) continue;
            IDirect3DVertexBuffer9* ex = NULL; UINT eo = 0, es = 0;
            if (SUCCEEDED(dev->GetStreamSource(si, &ex, &eo, &es)) && ex) {
                ex->Release();
                if (es) { veto = (int)si; break; }
            }
        }
        if (veto >= 0) {
            g_msStreamSkips++;
            g_msRetryLater = true;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
                "ms: this draw declares AND binds stream %d, so the clip cannot "
                "run on it - declining before the readback (skip %d of %d). "
                "Nothing was locked or copied.",
                veto, g_msStreamSkips, MS_MAX_STREAM_SKIPS);
            return false;
        }
    }

    if (!MsRead(dev, baseVertex, minIndex, numVertices, startIndex, primCount, bones))
        return false;

    // WAIT FOR A PASS WE CAN CLIP ON. This mesh is drawn by more than one
    // pass and only some bind stream 0 alone; accepting a multi-stream one
    // silently costs the clipped edge and the caps with it. Decline and let
    // the next matching draw try - this is NOT a refusal, so the lock is not
    // burned and the good pass still gets its chance.
    if (!g_msOwnVb && g_msEdge == 3 && g_msStreamSkips < MS_MAX_STREAM_SKIPS) {
        // Backstop: the cheap check above should have caught this, so reaching
        // here means the two disagree - worth knowing.
        g_msStreamSkips++;
        g_msRetryLater = true;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "ms: this draw of the mesh binds stream %d as well, so the clip "
            "cannot run on it - DECLINING and waiting for a pass that binds "
            "stream 0 alone (skip %d of %d). The same mesh is drawn by several "
            "passes and only some can be clipped; taking this one is how the "
            "wrist caps go missing.",
            g_msExtraStream, g_msStreamSkips, MS_MAX_STREAM_SKIPS);
        return false;
    }
    if (!g_msOwnVb && g_msEdge == 3) {
        g_msDegraded = true;
        Log("ms: WARNING - %d build candidates in a row declared and bound a "
            "second stream, so no clippable pass was found. Taking the "
            "whole-triangle cut: a sawtooth edge and NO caps. This is DEGRADED, "
            "not settled - a later clippable pass is allowed to replace it, and "
            "the pending path draws the game's own mesh meanwhile, so the "
            "alternative was never no hands.", g_msStreamSkips);
    }
    MsBones(bones);
    if (!MsSides()) return false;
    if (!MsWrist(1) || !MsWrist(2)) {
        Log("ms: REFUSED - a side produced no hand bone, so there is no wrist "
            "to cut at");
        return false;
    }
    MsTriSides();
    if (!MsPlaneDerive(1) || !MsPlaneDerive(2)) {
        Log("ms: the plane could not be derived for both arms - falling back to "
            "the sphere cut, which is blobbier but proven");
        g_msPlane = false;
    }
    if (!MsReclassify(dev)) return false;
    g_msReady = 1;
    if (g_msOwnVb) { g_msStreamSkips = 0; g_msDegraded = false; }   // a good pass clears it

    // The draw contract this split describes. Every later draw that wants to
    // use it must match, or the re-based indices address the wrong vertices.
    g_msBuiltBaseVertex = baseVertex;
    g_msBuiltMinIndex   = minIndex;
    g_msBuiltNumVerts   = numVertices;
    g_msBuiltStartIndex = startIndex;
    {
        IDirect3DVertexDeclaration9* d = NULL;
        if (SUCCEEDED(dev->GetVertexDeclaration(&d)) && d) { g_msBuiltDecl = d; d->Release(); }
        IDirect3DVertexBuffer9* vb0 = NULL; UINT o0 = 0, s0 = 0;
        if (SUCCEEDED(dev->GetStreamSource(0, &vb0, &o0, &s0)) && vb0) {
            g_msBuiltStream0Off = o0; vb0->Release();
        }
    }
    Log("ms: built from base %d, min %u, %u verts, start %u, stream0 offset %u, "
        "decl %p. A draw that does not match this contract cannot use this "
        "split - its indices are re-based onto our own vertex buffer.",
        g_msBuiltBaseVertex, g_msBuiltMinIndex, g_msBuiltNumVerts,
        g_msBuiltStartIndex, g_msBuiltStream0Off, g_msBuiltDecl);
    Log("ms: ==== READY - mode %s. Numpad 0 cycles the mode, + / - move the "
        "wrist, * picks which arm the wrist knob moves, / re-derives. ====",
        MsModeName(g_msMode));
    return true;
}


// ---- the draw ---------------------------------------------------------------

// The palette does not survive a device reset: the constants are gone and any
// register we still believe in describes a device that no longer exists. There
// was no invalidation at all before - the cache simply kept its last contents.
static void MpOnReset(void)
{
    memset(g_waCommon, 0, sizeof(g_waCommon));
    g_waMeshN = 0;
    g_pcLayShader = NULL;
    g_pcLayVp = g_pcLayL2W = g_pcLayBones = -1;
    g_pcLayBonesN = 0;
    memset(g_mpValid, 0, sizeof(g_mpValid));
    g_mpValidN = 0;
    g_mpCacheN = 0;
    g_mpPalN   = 0;
    g_mpResidOk[0]  = g_mpResidOk[1]  = false;
    // The SOURCE GENERATION moves too. The dominant slot, the anchor and any
    // grip transform solved against them describe a palette layout that no
    // longer exists; a slot index still inside the new palette would name a
    // different transform, and a stale G would then be applied against a
    // different source-frame convention without anything looking wrong.
    g_mpSrcGen++;
    g_mpSrcOk[0] = g_mpSrcOk[1] = false;
    g_mpSrcRefOk[0] = g_mpSrcRefOk[1] = false;
    g_mpCtlRefOk[0] = g_mpCtlRefOk[1] = false;
    Log("ms/palette: device reset - the palette cache, the calibrated origins "
        "and the residuals are all dropped, and the source generation is now "
        "%u so the frozen orientation slot and any grip transform solved "
        "against it are invalid. Constants do not survive a reset and a "
        "register we still believed in would describe a dead device.",
        g_mpSrcGen);
}


// One-time init, from DllMain: the pose lock, the grip matrices, and the frame
// maths self-test. The self-test runs in EVERY build, on the tester's machine,
// and writes its result to the log - so the log carries proof that the
// arithmetic in THAT build is the arithmetic that was checked, and the tester
// never has to run anything.
static void MpFrameSelfTestReport(void* ctx, const char* name, bool pass,
                                  const char* detail)
{
    (void)ctx;
    if (pass)
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                "ms/frame/selftest: %-28s PASS  %s", name, detail);
    else
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Error,
                "ms/frame/selftest: %-28s FAIL  %s", name, detail);
}

static void MpFrameInit(void)
{
    if (!g_mpPoseCsOk) { InitializeCriticalSection(&g_mpPoseCs); g_mpPoseCsOk = true; }
    memset(&g_mpPosePub, 0, sizeof(g_mpPosePub));
    for (int h = 0; h < 2; h++) {
        g_mpGrip[h] = dvr::hf::identity3();
        g_mpSrcR[h] = dvr::hf::identity3();
        g_mpSrcRef[h] = dvr::hf::identity3();
        g_mpCtlRef[h] = dvr::hf::identity3();
    }
    g_mpSelfTestFailed = dvr::hf::test::run_all(MpFrameSelfTestReport, NULL);
    if (g_mpSelfTestFailed == 0)
        Log("ms/frame/selftest: all cases passed. The rotation/grip frame maths "
            "in this build is the maths that was checked, including the "
            "stationary-controller head-turn counterexample that the rejected "
            "similarity transform fails by 180 degrees.");
    else
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Error,
                "ms/frame/selftest: %d CASE(S) FAILED. The rotation lever will "
                "REFUSE - placement stays translation-only, which is the "
                "headset-confirmed behaviour. Send this log.",
                g_mpSelfTestFailed);
}


static inline bool MpFinite(float x)
{
    return x == x && x < 3.4e38f && x > -3.4e38f;
}


// PLACEMENT THROUGH THE MEASURED CHAIN.
//
// THE SAMPLE UNIT IS THE ORIGINAL DRAW, NOT THE HAND.
//
// This function used to read the device itself, from inside MsDraw's per-hand
// loop. So its "ordinal" counted HANDS: ordinal 0 was the left hand and
// ordinal 1 the right hand OF THE SAME ORIGINAL DRAW, whose constants are
// identical by construction. Comparing them measured nothing about eyes, and
// 24,376 zero-difference pairs were reported as evidence that LocalToWorld
// carries no eye information. It is not evidence either way - that question is
// open again - and the "third draw" counter was simply the next original
// draw's left hand.
//
// The context is therefore acquired ONCE per original draw, above the hand
// loop, and both hands consume the same immutable copy. That also halves the
// constant reads. An ordinal is telemetry about draws; it is never an eye
// label.
struct MpDrawCtx {
    float r[3], u[3], f[3];     // camera basis, from the ViewProjection rows
    float col[3][3], t[3];      // LocalToWorld, columns and translation
    float projRight;            // L's translation on the right axis (telemetry)
    float vp[16], l2w[16];      // kept whole so a diagnostic can diff them
    void* target;              // borrowed render-target identity for weapon passes
    D3DVIEWPORT9 viewport;
    uint32_t drawId;
    bool ok;
    const char* why;
    // VR-33 rotation: the basis and LocalToWorld rotation as matrices, and ONE
    // pose snapshot that every consumer of this draw shares. Latching it here
    // is what stops the two hands - and later the weapon, which draws
    // separately - from sampling different controller poses within one view.
    dvr::hf::Mat3 B;            // columns right | up | forward
    dvr::hf::Mat3 R_L;          // LocalToWorld's rotation
    bool          basisProper;  // B*F is orthonormal (either parity)
    int           basisParity;  // +1 or -1: does the mapping mirror?
    MpPoseSnap    pose;
    bool          poseOk;
};


// Read the draw's own constants and validate them. One call per original draw.
static bool MpAcquireCtx(IDirect3DDevice9* dev, MpDrawCtx* c)
{
    memset(c, 0, sizeof(*c));
    c->why = "not attempted";
    if (!dev) { c->why = "no device"; return false; }
    if (g_pcLayVp < 0 || g_pcLayL2W < 0) { c->why = "no shader layout"; return false; }

    float vp[4][4], l2w[4][4];
    if (FAILED(dev->GetVertexShaderConstantF((UINT)g_pcLayVp, &vp[0][0], 4)))
        { c->why = "VP read failed"; return false; }
    if (FAILED(dev->GetVertexShaderConstantF((UINT)g_pcLayL2W, &l2w[0][0], 4)))
        { c->why = "LocalToWorld read failed"; return false; }
    IDirect3DSurface9* target = NULL;
    if (SUCCEEDED(dev->GetRenderTarget(0, &target)) && target) {
        c->target = target;
        target->Release();
    }
    dev->GetViewport(&c->viewport);
    memcpy(c->vp, vp, sizeof(c->vp));
    memcpy(c->l2w, l2w, sizeof(c->l2w));

    // The camera basis, from the rows of the ViewProjection. NOTE the standing
    // caveat: normalising rows this way assumes a symmetric projection. It has
    // held on every captured packet so far and is checked below, but an
    // asymmetric projection would need the principal-point terms removed first.
    float r[3] = { vp[0][0], vp[1][0], vp[2][0] };
    float u[3] = { vp[0][1], vp[1][1], vp[2][1] };
    float f[3] = { vp[0][3], vp[1][3], vp[2][3] };
    const float rn = sqrtf(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]);
    const float un = sqrtf(u[0]*u[0]+u[1]*u[1]+u[2]*u[2]);
    const float fn = sqrtf(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
    if (!(rn > 1e-4f) || !(un > 1e-4f)) { c->why = "degenerate focal scales"; return false; }
    if (fabsf(fn - 1.0f) > 0.01f) { c->why = "w row is not unit - not a standard perspective"; return false; }
    for (int i = 0; i < 3; i++) { r[i] /= rn; u[i] /= un; }
    const float ru = r[0]*u[0] + r[1]*u[1] + r[2]*u[2];
    const float rf = r[0]*f[0] + r[1]*f[1] + r[2]*f[2];
    const float uf = u[0]*f[0] + u[1]*f[1] + u[2]*f[2];
    if (fabsf(ru) > 0.02f || fabsf(rf) > 0.02f || fabsf(uf) > 0.02f)
        { c->why = "camera basis is not orthonormal"; return false; }
    memcpy(c->r, r, sizeof(r)); memcpy(c->u, u, sizeof(u)); memcpy(c->f, f, sizeof(f));

    // VR-68: publish the RENDERED camera's yaw for the lag finder. This runs on
    // the RENDER thread and the finder runs on the present thread, so it is a
    // seqlock, not a bare global - a bare global read across those two threads
    // is the exact mistake behind the retracted 39 % figure in this project.
    // Once per present: DvrConsumePoses sets the want flag, this clears it, so
    // the two eyes cannot contribute two samples to one frame interval.
    if (g_bvWant) {
        g_bvWant = false;
        const float y = atan2f(c->f[1], c->f[0]);   // game space; only DELTAS are ever used
        InterlockedIncrement(&g_bvSeq);             // odd: write in progress
        g_bvYaw = y;
        InterlockedIncrement(&g_bvSeq);             // even: value settled
    }

    // LocalToWorld: columns in the first three registers. ORTHOGONALITY is
    // checked, not just column length - unit columns alone do not make a
    // rotation, and the transpose is only the inverse if it is one.
    for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) c->col[j][i] = l2w[j][i];
    for (int i = 0; i < 3; i++) c->t[i] = l2w[3][i];
    for (int j = 0; j < 3; j++) {
        const float n = sqrtf(c->col[j][0]*c->col[j][0] + c->col[j][1]*c->col[j][1] +
                              c->col[j][2]*c->col[j][2]);
        if (fabsf(n - 1.0f) > 0.02f) { c->why = "LocalToWorld column is not unit"; return false; }
    }
    for (int j = 0; j < 3; j++)
        for (int kk = j + 1; kk < 3; kk++) {
            const float d = c->col[j][0]*c->col[kk][0] + c->col[j][1]*c->col[kk][1] +
                            c->col[j][2]*c->col[kk][2];
            if (fabsf(d) > 0.02f) { c->why = "LocalToWorld columns are not orthogonal"; return false; }
        }

    c->projRight = c->t[0]*r[0] + c->t[1]*r[1] + c->t[2]*r[2];

    // The same two matrices, in the form the rotation maths wants. B's columns
    // are the camera's right, up and forward; R_L is LocalToWorld's rotation,
    // already validated orthonormal above.
    c->B   = dvr::hf::basis_from_cols(c->r, c->u, c->f);
    c->R_L = dvr::hf::basis_from_cols(c->col[0], c->col[1], c->col[2]);

    // HANDEDNESS, MEASURED AND CARRIED - not required to be positive.
    //
    // MEASURED 2026-09-07 over 116,908 draws: B is RIGHT-handed here, so the
    // pose mapping B*F*transpose(R_head) is a MIRROR between XR's frame and the
    // game's camera-relative world frame. That is what a right-handed runtime
    // and a left-handed engine should produce.
    //
    // The first build of this demanded a PROPER rotation and refused every
    // single draw. The guard was wrong, not the game. A reflection is a
    // coordinate convention: it is carried through by full basis change and it
    // CANCELS between O_C and the grip transform, so what is finally composed
    // onto the palette is a proper rotation at every controller pose. See
    // hand_frame.h and the self-test's improper_basis_roundtrip.
    //
    // What is still required is ORTHONORMALITY, which is a broken read rather
    // than a convention. A draw failing that refuses to ROTATE and still
    // PLACES, because placement never needed the basis to be orthonormal.
    c->basisProper = dvr::hf::basis_is_orthonormal(c->B, 0.02f);
    c->basisParity = dvr::hf::basis_parity(c->B);
    if (!c->basisProper) g_mpBasisImproper++;
    else if (g_mpParitySeen == 0) {
        g_mpParitySeen = c->basisParity;
        Log("ms/palette/frame: the pose mapping B*F*transpose(R_head) has parity "
            "%+d - the draw basis is %s-handed, so the mapping between XR and "
            "the game's camera-relative frame is %s. This is a measured "
            "coordinate convention, not a fault: it is carried through by full "
            "basis change and cancels between the controller orientation and "
            "the grip transform, so what reaches the palette is a proper "
            "rotation either way.",
            c->basisParity, c->basisParity > 0 ? "left" : "right",
            c->basisParity > 0 ? "orientation-preserving" : "a MIRROR");
    }

    // ONE POSE SNAPSHOT for this draw, copied whole under the lock. See
    // MpPoseSnap for why a validity flag beside loose floats is not
    // publication. The lane the draw runs on is recorded here too, so the
    // thread contract is measured instead of assumed.
    {
        const DWORD tid = GetCurrentThreadId();
        if (g_mpDrawTid == 0) g_mpDrawTid = tid;
        if (g_mpLaneSame < 0 && g_mpTickTid != 0)
            g_mpLaneSame = (g_mpTickTid == tid) ? 1 : 0;
    }
    c->poseOk = false;
    if (g_mpPoseCsOk) {
        EnterCriticalSection(&g_mpPoseCs);
        c->pose = g_mpPosePub;
        LeaveCriticalSection(&g_mpPoseCs);
        c->poseOk = (c->pose.gen != 0);
    }
    {   // a snapshot older than the previous draw's means publication and
        // consumption have crossed; it is not fatal, but it must be visible
        static uint32_t lastGen = 0;
        if (c->poseOk && c->pose.gen < lastGen) InterlockedIncrement(&g_mpPoseStale);
        if (c->poseOk) lastGen = c->pose.gen;
    }

    c->drawId = ++g_mpDrawSeq;
    c->ok = true;
    c->why = NULL;
    return true;
}


// THE SOURCE PALM FRAME, from the frozen dominant palette slot.
//
// Returns the slot's normalised rotation and the uniform scale it was carrying.
// The scale is DERIVED from this matrix on every call, never a constant: the
// 0.999512 measured on the saved packets is what those captures held, not a
// property of every future pose, mesh and pass.
//
// Only the FRAME is normalised. The rendered palette keeps its own scale,
// because D is composed onto the original matrices.
static bool MpSourceFrame(int cls, const float* pal, UINT count,
                          dvr::hf::ScaledRot* out, const char** why)
{
    const char* dummy = NULL; if (!why) why = &dummy;
    if (cls < 0 || cls >= MS_CLS_N)          { *why = "bad class"; return false; }
    const int slot = g_mpDomSlot[cls];
    if (slot < 0)                            { *why = "no dominant slot frozen for this class"; return false; }
    const int bones = (int)(count / 3);
    if (slot >= bones) {
        *why = "the frozen orientation slot is outside the uploaded palette - "
               "the layout changed under a frozen index";
        return false;
    }
    const float* r0 = pal + (slot * 3 + 0) * 4;
    const float* r1 = pal + (slot * 3 + 1) * 4;
    const float* r2 = pal + (slot * 3 + 2) * 4;
    dvr::hf::Mat3 m;
    for (int c = 0; c < 3; c++) { m.m[0*3+c] = r0[c]; m.m[1*3+c] = r1[c]; m.m[2*3+c] = r2[c]; }
    if (!dvr::hf::decompose_scaled_rotation(m, g_mpFrameTolAniso,
                                            g_mpFrameTolOrtho, out)) {
        *why = "the orientation slot is not a uniformly scaled rotation "
               "(anisotropic, sheared, mirrored or degenerate)";
        return false;
    }
    return true;
}


// COMPARE SUCCESSIVE ORIGINAL DRAWS - the comparison that was never made.
//
// The previous diagnostic compared the two hands of one draw and found them
// identical, which was arithmetic rather than evidence. This compares one
// original draw against the previous one, which is the only pair that CAN
// differ by an eye.
//
// It states what it sampled, not just what it found: draws entered, sampled,
// rejected and compared are separate counts, and "no difference observed" is
// reported separately from "nothing was sampled".
// Decide the eye ONCE PER PRESENT. Within a Present every draw carries the
// same constants - that is what the runs of exact zeroes are - and the eye
// changes between Presents, showing up as a right-axis jump of about one IPD.
//
// Head motion also moves that projection between Presents, so a jump alone is
// not enough: the band is bounded on both sides, and anything outside it
// leaves the eye UNKNOWN rather than guessed. Unknown means no offset, which
// is the consistent head-centre placement rather than a full IPD of error.
// VR-76: THE FLICKER HISTORY. The tester reports the hands and the held weapon
// jumping RIGHT for a single frame, clearest on the desktop mirror. Its legacy
// tag policy can pin RIGHT with delayed capture. A one-frame fault previously
// left nothing in the log. This ring keeps what the placement did on each
// present of the last few seconds and prints it only when the tester presses
// the marker key (V, hotkeys.cpp). Nothing here changes a draw.
//
// One record per present that drew the hands: the eye decision and why, the
// jump it was decided from, and where each hand's target landed on the draw's
// right axis. The runtime's own eye tag comes from a second ring filled on the
// present lane, so the two can disagree on the page.
struct MfRec {
    uint32_t present;        // dvr::frame::count() while the draws ran
    double   ms;             // MaimNowMs() at the first hand draw
    float    d, projRight, ipdUU;
    float    tR[2];          // placed target on the right axis, uu (last draw of the present)
    uint32_t poseGen;
    int8_t   eye;            // g_mpEyeState after the decision: -1 L, +1 R, 0 unknown
    char     why;            // T toggled, S same eye kept, A ambiguous, F first sample
    uint8_t  placed[2], refused[2];
    uint8_t  waHit, waMiss;  // later draws that did / did not find this present's correction
};
static const int kMfRing = 1024;
static const double kMfWindowMs = 2500.0;
static MfRec     g_mfRing[kMfRing];
static int       g_mfHead = -1;              // the newest record, -1 = none yet
static uint32_t  g_mfTagCount[kMfRing];      // dvr::frame::count() at the game tick
static int8_t    g_mfTagSign[kMfRing];       // last_output().eyeSign read there
static int       g_mfTagHead = -1;
static uint32_t  g_mfMarks = 0;

static MfRec* MfCur()
{
    if (g_mfHead < 0) return NULL;
    MfRec* r = &g_mfRing[g_mfHead];
    return (r->present == (uint32_t)dvr::frame::count()) ? r : NULL;
}

static void MfOpen(uint32_t pres, const MpDrawCtx* c, char why, float d, float ipdUU)
{
    g_mfHead = (g_mfHead + 1) % kMfRing;
    MfRec* r = &g_mfRing[g_mfHead];
    memset(r, 0, sizeof(*r));
    r->present = pres; r->ms = MaimNowMs();
    r->d = d; r->projRight = c->projRight; r->ipdUU = ipdUU;
    r->poseGen = c->pose.gen;
    r->eye = (int8_t)g_mpEyeState; r->why = why;
}

static void MfNoteHand(int hand, const MpDrawCtx* c, const float* dcam)
{
    MfRec* r = MfCur();
    if (!r || hand < 0 || hand > 1) return;
    r->tR[hand] = dcam[0]*c->r[0] + dcam[1]*c->r[1] + dcam[2]*c->r[2];
    if (r->placed[hand] < 255) r->placed[hand]++;
}

static void MfNoteRefused(int hand)
{
    MfRec* r = MfCur();
    if (!r || hand < 0 || hand > 1) return;
    if (r->refused[hand] < 255) r->refused[hand]++;
}

static void MfNoteWeapon(bool found)
{
    MfRec* r = MfCur();
    if (!r) return;
    if (found) { if (r->waHit < 255) r->waHit++; }
    else if (r->waMiss < 255) r->waMiss++;
}

// Present lane, once per present from DvrGameTick. last_output() there is the
// PREVIOUS present's; which draws that present carried is an offset the marker
// measures rather than assumes (see MfTagFor's callers).
static void MfNoteTag(void)
{
    const int i = (g_mfTagHead + 1) % kMfRing;
    g_mfTagCount[i] = (uint32_t)dvr::frame::count();
    g_mfTagSign[i]  = (int8_t)dvr::stereo::last_output().eyeSign;
    g_mfTagHead = i;
}

// The runtime tag recorded at game tick (present + off); -2 = not in the ring.
static int MfTagFor(uint32_t present, int off)
{
    if (g_mfTagHead < 0) return -2;
    const uint32_t want = present + (uint32_t)off;
    const uint32_t headCount = g_mfTagCount[g_mfTagHead];
    if (want > headCount || headCount - want >= (uint32_t)kMfRing) return -2;
    const int i = (g_mfTagHead + kMfRing - (int)(headCount - want)) % kMfRing;
    return (g_mfTagCount[i] == want) ? g_mfTagSign[i] : -2;
}

static char MfEyeChar(int e) { return e == -2 ? '?' : e < 0 ? 'L' : e > 0 ? 'R' : '0'; }

// The marker. One labelled line, then the history of the window before it.
static void MfMarker(void)
{
    ++g_mfMarks;
    SYSTEMTIME st; GetLocalTime(&st);
    const uint32_t pres = (uint32_t)dvr::frame::count();
    const double now = MaimNowMs();
    DVR_WARN("MARKER #%u (V) at %02u:%02u:%02u.%03u | present #%u | the tester saw the hands jump just "
             "before this line; the flicker history below covers the %.1f s before it",
             g_mfMarks, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, pres, kMfWindowMs / 1000.0);

    // Oldest first, inside the window.
    static MfRec rec[kMfRing];
    int n = 0;
    if (g_mfHead >= 0) {
        int back = 0;
        for (; back < kMfRing; back++) {
            const MfRec& r = g_mfRing[(g_mfHead + kMfRing - back) % kMfRing];
            if (r.present == 0 || r.present > pres || now - r.ms > kMfWindowMs) break;
        }
        for (int k = back - 1; k >= 0; k--) rec[n++] = g_mfRing[(g_mfHead + kMfRing - k) % kMfRing];
    }
    if (n == 0) {
        DVR_WARN("marker #%u: NO hand draws in the last %.1f s - the placement did not run, so this flicker "
                 "cannot be the palette placement (or the hands were not on screen)",
                 g_mfMarks, kMfWindowMs / 1000.0);
        ::dvr::log::flush();
        return;
    }

    // Which game tick's tag belongs to which draws is MEASURED: the offset
    // whose tags agree with the placement's own eye decisions. A healthy run
    // reads near 100 % at one offset and near 0 % at its neighbours; no clear
    // winner means the tags cannot be aligned and the tag rows mean nothing.
    int agree[4] = {0, 0, 0, 0}, total[4] = {0, 0, 0, 0};
    for (int off = 1; off <= 3; off++)
        for (int i = 0; i < n; i++) {
            const int t = MfTagFor(rec[i].present, off);
            if (t == -2 || t == 0 || rec[i].eye == 0) continue;
            total[off]++;
            if (t == rec[i].eye) agree[off]++;
        }
    int best = 2;
    for (int off = 1; off <= 3; off++)
        if (total[off] && (!total[best] || agree[off] * total[best] > agree[best] * total[off])) best = off;
    const float ipd = rec[n - 1].ipdUU;
    DVR_WARN("marker #%u: %d present(s) drew the hands, #%u..#%u (%.0f ms) | draws on %s lane | runtime tag "
             "offset +1 agrees %d/%d, +2 %d/%d, +3 %d/%d -> using +%d | IPD %.2f uu | healthy: tag and eye both "
             "alternate L/R and match, why is all T, tR alternates by about one IPD",
             g_mfMarks, n, rec[0].present, rec[n - 1].present, now - rec[0].ms,
             (g_mpDrawTid == 0) ? "an unrecorded" : (GetCurrentThreadId() == g_mpDrawTid) ? "this (the marker's)" : "ANOTHER",
             agree[1], total[1], agree[2], total[2], agree[3], total[3], best, (double)ipd);

    // THE FILTER, stated as one. Zero flagged means no logged placement
    // quantity differed on any present in the window, which points the fault
    // outside the placement arithmetic.
    int flagged = 0, printed = 0;
    for (int i = 0; i < n; i++) {
        const MfRec& r = rec[i];
        const int t = MfTagFor(r.present, best);
        char why[160]; int w = 0; why[0] = 0;
        if (t != -2 && t != 0 && r.eye != t)
            w += _snprintf(why + w, sizeof(why) - w, " eye %c but tag %c;", MfEyeChar(r.eye), MfEyeChar(t));
        if (r.why != 'T')
            w += _snprintf(why + w, sizeof(why) - w, " decision %c;", r.why);
        if (r.refused[0] || r.refused[1])
            w += _snprintf(why + w, sizeof(why) - w, " refused %u/%u;", r.refused[0], r.refused[1]);
        if (r.waMiss)
            w += _snprintf(why + w, sizeof(why) - w, " weapon miss %u;", r.waMiss);
        for (int h = 0; h < 2 && i >= 2 && i + 2 < n; h++) {
            const MfRec& a = rec[i - 2]; const MfRec& b = rec[i + 2];
            if (!r.placed[h] || !a.placed[h] || !b.placed[h] || a.eye != r.eye || b.eye != r.eye) continue;
            const float dev = r.tR[h] - 0.5f * (a.tR[h] + b.tR[h]);
            if (fabsf(dev) > 0.35f * ipd)
                w += _snprintf(why + w, sizeof(why) - w, " hand %d tR %+.2f uu off its same-eye neighbours;", h, (double)dev);
        }
        if (w <= 0) continue;
        why[sizeof(why) - 1] = 0;
        flagged++;
        if (printed < 30) {
            printed++;
            DVR_WARN("marker #%u flag: #%u %.0f ms before the marker:%s", g_mfMarks, r.present, now - r.ms, why);
        }
    }
    DVR_WARN("marker #%u: %d present(s) flagged (%d printed) - a flag is a tag/eye disagreement, a decision "
             "other than T, a refused hand, a weapon miss, or a target more than %.2f uu (0.35 IPD) off its "
             "same-eye neighbours", g_mfMarks, flagged, printed, (double)(0.35f * ipd));

    // THE ENUMERATION: every present in the window, unfiltered.
    // Timeline rows: one character per present number, '.' = no hand draw.
    {
        char tagRow[128], eyeRow[128], whyRow[128];
        int col = 0; uint32_t rowStart = rec[0].present, p = rec[0].present;
        int i = 0;
        while (i < n) {
            const bool have = (rec[i].present == p);
            const int t = MfTagFor(p, best);
            tagRow[col] = MfEyeChar(t);
            eyeRow[col] = have ? MfEyeChar(rec[i].eye) : '.';
            whyRow[col] = have ? rec[i].why : '.';
            col++;
            if (have) i++;
            p++;
            if (col == 100 || i == n || (i < n && rec[i].present - p > 50)) {
                tagRow[col] = eyeRow[col] = whyRow[col] = 0;
                DVR_WARN("marker #%u rows from #%u: tag %s", g_mfMarks, rowStart, tagRow);
                DVR_WARN("marker #%u rows from #%u: eye %s", g_mfMarks, rowStart, eyeRow);
                DVR_WARN("marker #%u rows from #%u: why %s", g_mfMarks, rowStart, whyRow);
                col = 0;
                if (i < n && rec[i].present - p > 50) p = rec[i].present;   // a long gap: skip it
                rowStart = p;
            }
        }
    }
    // Numbers: present tag/eye why | jump d | projRight | hand targets on the right axis | pose gen.
    for (int i = 0; i < n; i += 5) {
        char line[1024]; int w = 0; line[0] = 0;
        for (int k = i; k < i + 5 && k < n && w < (int)sizeof(line) - 180; k++) {
            const MfRec& r = rec[k];
            // These draws reach Present r.present+1. This join is independent
            // of the measured offset used for the delivered texture's tag.
            dvr::desktop_eye::Record mirror;
            const bool haveMirror = dvr::desktop_eye::record_for(r.present + 1, mirror);
            w += _snprintf(line + w, sizeof(line) - w, " | #%u %c/%c%c d%+.2f pr%+.2f tR %+.2f%s/%+.2f%s g%u desk=%c:%c/%c/%c/%c",
                           r.present, MfEyeChar(MfTagFor(r.present, best)), MfEyeChar(r.eye), r.why,
                           (double)r.d, (double)r.projRight,
                           (double)r.tR[0], r.placed[0] ? "" : "x", (double)r.tR[1], r.placed[1] ? "" : "x",
                           r.poseGen, haveMirror ? mirror.source : '?',
                           haveMirror ? MfEyeChar(mirror.draw) : '?',
                           haveMirror ? MfEyeChar(mirror.tag) : '?',
                           haveMirror ? mirror.action : '?',
                           haveMirror ? MfEyeChar(mirror.shown) : '?');
        }
        line[sizeof(line) - 1] = 0;
        DVR_WARN("marker #%u n -%.0fms%s", g_mfMarks, now - rec[i].ms, line);
    }
    DVR_WARN("marker #%u end", g_mfMarks);
    ::dvr::log::flush();
}

// VR-69: restore the pre-regression render-side decision. A live script
// doubling flag cannot identify an older queued render view.
static void MpEyeForPresent(const MpDrawCtx* c)
{
    const uint32_t pres = (uint32_t)dvr::frame::count();
    if (pres == g_mpEyePresent) return;          // same Present, decision stands
    g_mpEyePresent = pres;

    const float ipdUU = g_ipdM * ((g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f)
                                  * g_mpDriveGain);
    if (!g_mpEyeHavePrev) {
        g_mpEyeHavePrev = true; g_mpEyePrevFirst = c->projRight;
        g_mpEyeState = 0;                        // nothing to compare against yet
        MfOpen(pres, c, 'F', 0.0f, ipdUU);
        return;
    }
    const float d = c->projRight - g_mpEyePrevFirst;
    const float ad = fabsf(d);
    char why;
    if (ad > 0.45f * ipdUU && ad < 2.0f * ipdUU) {
        // The eye changed. The SIGN gives it absolutely, with no vote: the
        // smaller right-axis projection is the right eye.
        g_mpEyeState = (d < 0.0f) ? +1 : -1;
        g_mpEyeToggles++;
        why = 'T';
    } else if (ad <= 0.45f * ipdUU) {
        // Same eye as the previous Present - or too small to tell apart.
        g_mpEyeSame++;
        why = 'S';
    } else {
        g_mpEyeState = 0;                        // head moved too far to judge
        g_mpEyeAmbiguous++;
        why = 'A';
    }
    g_mpEyePrevFirst = c->projRight;
    MfOpen(pres, c, why, d, ipdUU);              // VR-76: after the decision the draws use
}

#if DVR_WITH_LEGACY
#include "legacy/vr69/palette_eye_experiment.inc"
#endif

static void MpDrawCompare(const MpDrawCtx* c)
{
    if (!c || !c->ok) return;
    MpEyeForPresent(c);
    if (!g_mpEyeHunt) return;
    if (g_mpPrevDrawOk) {
        float dv = 0.0f, dl = 0.0f; int dvIdx = -1, dlIdx = -1;
        for (int i = 0; i < 16; i++) {
            const float d = fabsf(c->vp[i] - g_mpPrevVp[i]);
            if (d > dv) { dv = d; dvIdx = i; }
        }
        for (int i = 0; i < 16; i++) {
            const float d = fabsf(c->l2w[i] - g_mpPrevL2w[i]);
            if (d > dl) { dl = d; dlIdx = i; }
        }
        const float dProj = c->projRight - g_mpPrevProj;
        if (dv > g_mpCmpMaxVp) g_mpCmpMaxVp = dv;
        if (dl > g_mpCmpMaxL2w) g_mpCmpMaxL2w = dl;
        if (fabsf(dProj) > fabsf(g_mpCmpMaxProj)) g_mpCmpMaxProj = dProj;
        g_mpCmpCount++;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "ms/palette/cmp: draw %u vs %u | VP max element delta %.6f (idx %d) "
            "| LocalToWorld max %.6f (idx %d) | L translation on the right axis "
            "moved %.3f uu | running maxima VP %.6f L %.6f right %.3f over %ld "
            "comparisons. This compares SUCCESSIVE ORIGINAL DRAWS, the only "
            "pair that can differ by an eye; the previous diagnostic compared "
            "the two HANDS of one draw and its zeroes meant nothing. Expected "
            "IPD %.2f uu - but a matrix element delta is not a length, so treat "
            "the right-axis translation as the only directly comparable number.",
            c->drawId, g_mpPrevDrawId, (double)dv, dvIdx, (double)dl, dlIdx,
            (double)dProj, (double)g_mpCmpMaxVp, (double)g_mpCmpMaxL2w,
            (double)g_mpCmpMaxProj, g_mpCmpCount,
            (double)(g_ipdM * g_skcWorldScale));
    }
    memcpy(g_mpPrevVp, c->vp, sizeof(g_mpPrevVp));
    memcpy(g_mpPrevL2w, c->l2w, sizeof(g_mpPrevL2w));
    g_mpPrevProj = c->projRight;
    g_mpPrevDrawId = c->drawId;
    g_mpPrevDrawOk = true;
}


// Place one hand through an already-acquired draw context. Reads no device
// state of its own, so both hands of a draw are guaranteed to use identical
// constants rather than merely expected to.
static bool MpWorldTarget(const MpDrawCtx* c, int hand, int cls,
                          const float* qLocal, dvr::hf::Xform* outD,
                          const char** why)
{
    const char* dummy = NULL; if (!why) why = &dummy;
    if (!c || !c->ok)                { *why = c ? c->why : "no context"; return false; }
    if (hand < 0 || hand > 1)        { *why = "bad hand"; return false; }
    if (!c->poseOk || !c->pose.ok[hand]) {
        *why = (g_mpTickRan == 0)
             ? "the pose tick has NEVER RUN - the palette backend is off, or "
               "this build gated the tick on a flag that is not set"
             : "the controller pose is invalid (tracking lost or not yet "
               "acquired); the pose tick is running";
        return false;
    }

    const float k = (g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f) * g_mpDriveGain;
    const float a = c->pose.ruf[hand][0], b = c->pose.ruf[hand][1],
                cc = c->pose.ruf[hand][2];
    float dcam[3];
    for (int i = 0; i < 3; i++)
        dcam[i] = k * (a * c->r[i] + b * c->u[i] + cc * c->f[i]);

    // THE EYE, from the measurement the corrected sampling finally made.
    //
    // Comparing SUCCESSIVE ORIGINAL DRAWS shows two clean regimes: the
    // right-axis component of LocalToWorld's translation either does not move
    // at all between draws, or it moves by about one IPD - 6.76 uu measured
    // against 6.31 predicted. So LocalToWorld does carry the eye after all;
    // the per-hand sampling that appeared to rule it out was comparing a draw
    // with itself.
    //
    // The eye is constant within a Present and changes between them, which is
    // what the runs of exact zeroes are. So the eye is decided once per
    // Present, in MpEyeForPresent, by comparing this Present's first draw
    // against the previous one's - and never from an ordinal, a hand side or a
    // moving midpoint, all of which have now failed.
    if (g_mpEyeOffset && g_mpEyeState != 0) {
        // Camera-relative: a position is world - camera, so the RIGHT eye's
        // camera being further right makes its positions smaller on that axis.
        // g_mpEyeState is -1 for left, +1 for right.
        const float halfIpdUU = 0.5f * g_ipdM * k;
        for (int i = 0; i < 3; i++) dcam[i] -= (float)g_mpEyeState * halfIpdUU * c->r[i];
        if (g_mpEyeState > 0) g_mpEyeSeen[1]++; else g_mpEyeSeen[0]++;
    } else {
        g_mpEyeUnclassified++;
    }

    // ---- the rotational half ------------------------------------------------
    //
    // Everything above is the headset-confirmed translation path and is
    // unchanged. Rotation is a SEPARATE decision on top of it: if any part of
    // it refuses, the delta falls back to translation only rather than sending
    // a correctly tracked hand back to where the engine put it. A hand that
    // tracks but is not oriented is worth much more than a hand that does not
    // track.
    bool rotate = false;
    dvr::hf::Mat3 O_C = dvr::hf::identity3();
    dvr::hf::Mat3 R_src = dvr::hf::identity3();

    if (g_mpRotate) {
        const char* rwhy = "not attempted";
        dvr::hf::ScaledRot sr;
        if (g_mpSelfTestFailed != 0) {
            rwhy = "the frame maths self-test did not pass in this build";
        } else if (!c->basisProper) {
            rwhy = "the draw's camera basis is not orthonormal, so transpose "
                   "would not be its inverse - this is a broken read, not a "
                   "handedness convention";
        } else if (!MpSourceFrame(cls, g_mpCache, g_mpCacheN, &sr, &rwhy)) {
            /* rwhy set */
        } else {
            R_src = sr.r;
            g_mpSrcR[hand] = sr.r; g_mpSrcOk[hand] = true;
            g_mpSrcScale[hand] = sr.scale;
            g_mpSrcAniso[hand] = sr.aniso;
            g_mpSrcOrtho[hand] = sr.ortho;
            O_C = dvr::hf::head_orient_to_camera(c->B, c->pose.inHead[hand]);

            // THE GRIP CAPTURE. Consumed ONCE, from a qualified original draw,
            // against ONE coherent pose snapshot, per side, and always from the
            // ORIGINAL palette rather than one already corrected.
            //
            // Both operands must live in the same space: the source frame is
            // measured in the component's LOCAL space, so it is carried to
            // camera-relative world by THIS draw's LocalToWorld before being
            // compared with the controller. Comparing them directly would bake
            // the component's own orientation into G.
            //
            // The press snaps the hand back to the game's own animated
            // orientation at that instant. That is what calibration means here,
            // and the tester is told to expect it.
            MpFrameId nowId;
            nowId.slot    = g_mpDomSlot[cls];
            nowId.anchorN = g_mpAnchorN[cls];
            nowId.anchor0 = (g_mpAnchorN[cls] > 0) ? g_mpAnchorIdx[cls][0] : 0u;

            const LONG capPrev = InterlockedAnd(&g_mpGripCapReq,
                                                ~(LONG)(1 << hand));
            if (capPrev & (1 << hand)) {
                g_mpGrip[hand] = dvr::hf::grip_solve(O_C, c->R_L, R_src);
                g_mpGripId[hand] = nowId;
                g_mpGripFromIni[hand] = false;
                g_mpGripHave[hand] = true;
                // SPLIT THE REFLECTION OUT BEFORE IT IS EVER WRITTEN DOWN.
                // G is improper here, and no product of three proper rotations
                // can be. Storing a parity sign beside a proper rotation is
                // what makes the calibration survive a restart; the previous
                // format silently dropped the reflection and the hands came
                // back mirrored.
                int par = 0; dvr::hf::Mat3 proper;
                dvr::hf::split_parity(g_mpGrip[hand], &par, &proper);
                float ex, ey, ez;
                dvr::hf::mat_to_euler_xyz_deg(proper, &ex, &ey, &ez);
                g_mpGripDeg[hand][0] = ex; g_mpGripDeg[hand][1] = ey;
                g_mpGripDeg[hand][2] = ez;
                g_mpGripParity[hand] = par;
                g_mpGripVer[hand] = MP_GRIP_VERSION;
                g_mpGripSaveDeg[hand][0] = ex; g_mpGripSaveDeg[hand][1] = ey;
                g_mpGripSaveDeg[hand][2] = ez;
                g_mpGripSaveParity[hand] = par;
                InterlockedOr(&g_mpGripSaveReq, (LONG)(1 << hand));
                InterlockedIncrement(&g_mpGripCapDone);
                Log("ms/palette/grip: SOLVED for the %s hand against source "
                    "generation %u - parity %+d, proper rotation %+.2f %+.2f "
                    "%+.2f degrees (extrinsic X,Y,Z; R = Rz*Ry*Rx). It is being "
                    "SAVED to the ini automatically, so it survives a restart "
                    "with no key press. The reflection is stored as the parity "
                    "sign: three Euler angles alone cannot carry it, which is "
                    "what made the hands come back inside out. The hand has just "
                    "snapped to the game's own animated orientation at the "
                    "moment of the press - that IS the calibration.",
                    hand ? "RIGHT" : "LEFT", g_mpSrcGen, par,
                    (double)ex, (double)ey, (double)ez);
            }

            // A grip solved in this session is only valid while the frame
            // convention it was solved against still holds. A grip read from
            // the ini is not checked here: it was written down deliberately
            // against a recorded convention, and refusing it on a fingerprint
            // it predates would make the ini route unusable.
            if (!g_mpGripFromIni[hand] &&
                (g_mpGripId[hand].slot    != nowId.slot ||
                 g_mpGripId[hand].anchorN != nowId.anchorN ||
                 g_mpGripId[hand].anchor0 != nowId.anchor0)) {
                rwhy = "the grip transform was solved against a different source "
                       "frame - the orientation slot or the anchor has been "
                       "re-derived since, so it no longer describes this frame. "
                       "Press SHIFT+F7 to solve it again";
            } else {
                rotate = true;
            }
        }
        if (!rotate) { g_mpRotWhy = rwhy; InterlockedIncrement(&g_mpRotRefused); }
        else         InterlockedIncrement(&g_mpRotOk);
    }

    // THE GRIP ACTUALLY USED. With no calibration yet, identity is the WRONG
    // default: the pose mapping is improper here, so identity makes O_C*G
    // improper and the hand is drawn INSIDE OUT - which is exactly what the
    // first headset run showed before SHIFT+F7. The parity-matched default is
    // still uncalibrated and still at a wrong angle, but it is not mirrored.
    dvr::hf::Mat3 Guse = g_mpGrip[hand];
    if (rotate && !g_mpGripHave[hand] && g_mpGripVer[hand] != MP_GRIP_VERSION) {
        Guse = dvr::hf::parity_factor(dvr::hf::parity_of(O_C));
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
            "ms/palette/grip: NO CALIBRATION for the %s hand yet, so the grip is "
            "the parity-matched default (%+d). The hand tracks and is NOT "
            "mirrored, but its angle is uncalibrated until SHIFT+F7.",
            hand ? "right" : "left", dvr::hf::parity_of(O_C));
    }

    dvr::hf::Xform D;
    if (rotate) {
        // Through the SHARED target helper, which needs only the pose, the
        // draw's basis and the calibration - no hand source palette. That is
        // what lets a held weapon be placed before either hand has drawn.
        // The trim's translation is metres in the palm frame; k converts it
        // once, through the same effective scale the position path uses.
        const float trimUU[3] = { g_mpTrimT[hand][0] * k, g_mpTrimT[hand][1] * k,
                                  g_mpTrimT[hand][2] * k };
        const dvr::hf::Mat3 trimR = dvr::hf::euler_xyz_deg_to_mat(
            g_mpTrimR[hand][0], g_mpTrimR[hand][1], g_mpTrimR[hand][2]);
        const dvr::hf::Xform target = dvr::hf::palm_target(O_C, Guse, dcam,
                                                           trimR, trimUU);
        g_mpPalmTarget[hand] = target;
        g_mpPalmTargetOk[hand] = true;
        float palmLocal[3];
        D = dvr::hf::delta_from_target(c->R_L, c->t, target, R_src, qLocal,
                                       palmLocal);
        // The model scale, about the palm. At 1.0 this is exactly the identity
        // it replaces, so a build with the lever at its default is the build
        // that was measured without it.
        if (g_mpModelScale != 1.0f)
            D = dvr::hf::scale_about(D, palmLocal, g_mpModelScale);

        // VR-33 W2/W3: publish THIS correction for the weapon path, conjugated
        // out of the hand's local space into the draw's camera-relative world
        // so any other member of the same view can consume it. Published here,
        // AFTER the model scale, so that factor is carried exactly once.
        WaPublishCommon(hand, c, D);
    } else {
        D = dvr::hf::delta_local(c->R_L, c->t, O_C, Guse, dcam, R_src, qLocal,
                                 false);
        g_mpPalmTargetOk[hand] = false;
    }
    for (int i = 0; i < 3; i++)
        if (!MpFinite(D.t[i])) { *why = "non-finite target"; return false; }
    for (int i = 0; i < 9; i++)
        if (!MpFinite(D.r.m[i])) { *why = "non-finite rotation"; return false; }
    *outD = D;

    // The instrument's three SEPARATELY NAMED quantities. They are different
    // measurements and conflating them is what made the previous plan's motion
    // gate impossible: with rotation off, the controller moving while the
    // source frame does not is CORRECT, because nothing connects them yet.
    {
        if (!g_mpSrcRefOk[hand] && g_mpSrcOk[hand])
            { g_mpSrcRef[hand] = g_mpSrcR[hand]; g_mpSrcRefOk[hand] = true; }
        if (!g_mpCtlRefOk[hand])
            { g_mpCtlRef[hand] = c->pose.inHead[hand]; g_mpCtlRefOk[hand] = true; }
        if (g_mpSrcRefOk[hand] && g_mpSrcOk[hand]) {
            g_mpSrcMoved[hand] = dvr::hf::rotation_diff_deg(g_mpSrcRef[hand],
                                                            g_mpSrcR[hand]);
            if (g_mpSrcMoved[hand] > g_mpSrcMovedMax[hand])
                g_mpSrcMovedMax[hand] = g_mpSrcMoved[hand];
        }
        if (g_mpCtlRefOk[hand]) {
            g_mpCtlMoved[hand] = dvr::hf::rotation_diff_deg(g_mpCtlRef[hand],
                                                            c->pose.inHead[hand]);
            if (g_mpCtlMoved[hand] > g_mpCtlMovedMax[hand])
                g_mpCtlMovedMax[hand] = g_mpCtlMoved[hand];
        }
        // The APPLIED orientation, which exists only when rotating.
        g_mpOutOk[hand] = rotate;
        if (rotate) g_mpOutMoved[hand] = dvr::hf::rotation_angle_deg(D.r);
    }

    float targetLocal[3];
    { float d[3];
      for (int i = 0; i < 3; i++) d[i] = dcam[i] - c->t[i];
      for (int j = 0; j < 3; j++)
          targetLocal[j] = c->col[j][0]*d[0] + c->col[j][1]*d[1] + c->col[j][2]*d[2]; }
    memcpy(g_mpLastTargetLocal[hand], targetLocal, sizeof(targetLocal));
    memcpy(g_mpLastPCam[hand], dcam, sizeof(dcam));
    MfNoteHand(hand, c, dcam);                   // VR-76: the flicker history
    return true;
}


// Where the palm anchor actually IS this frame, in the palette's output space.
// Skins the chosen vertices with the palette the GAME asked for - never one we
// have already moved, or the correction compounds frame on frame.
//
// The blend is the same one the shader does: sum over influences of
// weight * (M[index] * vertex). Returns false rather than guessing if the
// class has no anchor or an influence names a bone outside the block, because
// a silently clamped index would put the anchor somewhere plausible and wrong.
static bool MpAnchorPos(int cls, const float* pal, UINT count, float* out)
{
    if (cls < 0 || cls >= MS_CLS_N || g_mpAnchorN[cls] <= 0) return false;
    const int bones = (int)(count / 3);
    float acc[3] = { 0.0f, 0.0f, 0.0f };

    // EVERY anchor vertex must be valid or the whole anchor is refused. The
    // previous version skipped a bad vertex and averaged the rest, returning
    // success if ANY vertex worked - so a short or wrong palette silently
    // changed WHICH point was being measured while the code's own comment
    // promised refusal. A moved anchor and a moved hand are indistinguishable
    // downstream, which is the one thing this measurement cannot afford.
    for (int a = 0; a < g_mpAnchorN[cls]; a++) {
        const uint32_t vi = g_mpAnchorIdx[cls][a];
        if ((int)vi >= g_msVerts) return false;
        const MsVert* v = &g_msVert[vi];
        float wsum = 0.0f;
        for (int i = 0; i < 4; i++) wsum += v->bw[i];
        if (!(wsum > 0.0001f)) return false;             // also catches NaN
        float q[3] = { 0.0f, 0.0f, 0.0f };
        for (int i = 0; i < 4; i++) {
            const float wgt = v->bw[i];
            if (wgt <= 0.0f) continue;
            const int b = (int)v->bi[i];
            if (b < 0 || b >= bones) return false;
            const float* r0 = pal + (b * 3 + 0) * 4;
            const float* r1 = pal + (b * 3 + 1) * 4;
            const float* r2 = pal + (b * 3 + 2) * 4;
            q[0] += wgt * (r0[0]*v->p[0] + r0[1]*v->p[1] + r0[2]*v->p[2] + r0[3]);
            q[1] += wgt * (r1[0]*v->p[0] + r1[1]*v->p[1] + r1[2]*v->p[2] + r1[3]);
            q[2] += wgt * (r2[0]*v->p[0] + r2[1]*v->p[1] + r2[2]*v->p[2] + r2[3]);
        }
        // THE WEIGHT SUM IS REPORTED, NOT REPAIRED. Dividing by it here was
        // fixing the CPU copy of an arithmetic the GPU may not perform, which
        // would make this point something the shader never renders. Worse, if
        // the shader really does use unnormalised weights then a palette
        // translation T moves the vertex by wsum*T, so `target - q` would not
        // even produce the displacement it claims. The shader has not been
        // read yet, so this records the deviation and leaves the arithmetic
        // alone; sums far from 1 make the whole anchor untrustworthy.
        if (fabsf(wsum - 1.0f) > g_mpWsumTol) {
            g_mpWsumWorst = wsum;
            return false;
        }
        acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2];
    }
    const float inv = 1.0f / (float)g_mpAnchorN[cls];
    out[0] = acc[0] * inv; out[1] = acc[1] * inv; out[2] = acc[2] * inv;
    // A real finite test. `x == x` rejects NaN and cheerfully accepts
    // infinity, and an infinite anchor would propagate into the submitted
    // transform as a plausible-looking huge number.
    for (int i = 0; i < 3; i++)
        if (!MpFinite(out[i])) return false;
    return true;
}


// D * M for every skinning matrix in the block, where D is a rigid transform
// in whatever space the palette is already expressed in. Each matrix is 3
// float4 rows, row-major 3x4, with the translation in .w.
//
// Every bone gets the SAME D, which is what keeps the animation: the weighted
// blend commutes with a common rigid transform, so the engine goes on
// animating M_i and D only moves the result. `compose_commutes` in the
// self-test pins that property.
//
// D's rotation is applied to the whole 3x3, so NORMALS AND TANGENTS COME WITH
// IT. Two of the three shaders that draw this mesh (F2E11B73, 11DD5E8A) run
// normals and tangents through these same blended rows and build the bitangent
// from their cross product, so a proper rigid D carries the tangent frame
// correctly and the uniform palette scale is harmless. Their WorldToLocal
// constant converts view and light vectors INTO component space and is not a
// second skinning matrix - it must NOT be touched.
//
// D's rotation is normalised by construction, so the palette's own uniform
// scale survives in M and is not quietly removed. The ONE exception is the
// model scale ([Hands] ModelScale), which multiplies D deliberately to resize
// the hand and anything held in it - see scale_about in hand_frame.h for why
// that is uniform and why the tangent frame survives it.
static void MpBuild(float* out, const float* src, UINT count,
                    const dvr::hf::Xform* D)
{
    memcpy(out, src, sizeof(float) * 4 * count);
    for (UINT b = 0; b + 3 <= count; b += 3)
        dvr::hf::compose_3x4(*D, src + b * 4, out + b * 4);
}


// Emit the classes this mode wants, through OUR index buffer. Returns false if
// it drew nothing, and the caller then does whatever it would have done - which
// is the fail-soft: an auto-armed lock with no usable split draws the mesh
// exactly as the game asked for it.
// THE GEOMETRY QUALIFIER, shared by the draw and the capture.
//
// Pulled out of MsDraw so the capture can ask "is this the hand draw, and does
// it match the contract the split was built from?" WITHOUT calling MsDraw,
// which actually draws and changes device state. A qualifier that has to draw
// to answer is not a qualifier.
//
// Read-only: it takes no reference it does not release, and it writes only the
// out-parameters. `why` gets a short reason on refusal so a capture that never
// fires can say which field disagreed.
struct MsContract {
    UINT stream0Off, stride, startIndex, minIndex, numVertices;
    INT  baseVertex;
    void* decl;
};

static bool MsQualify(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                      UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount,
                      MsContract* out, const char** why)
{
    const char* dummy = NULL;
    if (!why) why = &dummy;
    *why = NULL;
    if (!dev)                        { *why = "no device";        return false; }
    if (!g_msReady || !g_msIb)       { *why = "no split built";   return false; }
    if (type != D3DPT_TRIANGLELIST)  { *why = "not a triangle list"; return false; }
    if ((int)primCount != g_msTris)  { *why = "primitive count";  return false; }

    // A FAILED QUERY MUST NOT CERTIFY THE CONTRACT. These used to leave their
    // outputs at zero/NULL on failure, and the comparisons then skipped the
    // very fields that could not be read - an unreadable device state passed
    // as compatible.
    UINT curOff = 0; IDirect3DVertexBuffer9* vb0 = NULL; UINT s0 = 0;
    const bool ssOk = SUCCEEDED(dev->GetStreamSource(0, &vb0, &curOff, &s0)) && vb0 != NULL;
    if (vb0) vb0->Release();
    IDirect3DVertexDeclaration9* d = NULL; void* dp = NULL;
    const bool dclOk = SUCCEEDED(dev->GetVertexDeclaration(&d)) && d != NULL;
    if (d) { dp = d; d->Release(); }
    if (!ssOk || !dclOk) { *why = "device state unreadable"; return false; }

    if (baseVertex != g_msBuiltBaseVertex)      { *why = "base vertex";   return false; }
    if (minIndex != g_msBuiltMinIndex)          { *why = "min index";     return false; }
    if (numVertices != g_msBuiltNumVerts)       { *why = "vertex count";  return false; }
    if (curOff != g_msBuiltStream0Off)          { *why = "stream0 offset"; return false; }
    if ((int)startIndex != g_msBuiltStartIndex) { *why = "start index";   return false; }
    if (g_msStride && s0 != g_msStride)         { *why = "stream stride"; return false; }
    if (g_msBuiltDecl && dp != g_msBuiltDecl)   { *why = "declaration";   return false; }

    if (out) {
        out->stream0Off = curOff; out->stride = s0; out->startIndex = startIndex;
        out->minIndex = minIndex; out->numVertices = numVertices;
        out->baseVertex = baseVertex; out->decl = dp;
    }
    return true;
}


static bool MsDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT startIndex, UINT primCount)
{
    g_msPassThrough = false;
    if (g_msMode == MS_MODE_OFF) return false;
    MsContract con;
    {
        const char* why = NULL;
        if (!MsQualify(dev, type, baseVertex, minIndex, numVertices, startIndex,
                       primCount, &con, &why)) {
            // Not this geometry at all: stay silent and let the caller decide.
            if (!g_msReady || !g_msIb || type != D3DPT_TRIANGLELIST ||
                (int)primCount != g_msTris)
                return false;
            // It IS this geometry but the contract disagrees. Draw it NORMALLY
            // rather than replacing or dropping it - our indices are re-based
            // and would address the wrong vertices, and the auto-arm fail-soft
            // only covers the case where no split exists.
            g_msIncompat++;
            g_msPassThrough = true;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                "ms: a draw of this geometry does NOT match the contract the "
                "split was built from - %s disagrees (base %d vs %d, min %u vs "
                "%u, verts %u vs %u, startIndex %u vs %d). Drawing it normally.",
                why, baseVertex, g_msBuiltBaseVertex, minIndex, g_msBuiltMinIndex,
                numVertices, g_msBuiltNumVerts, startIndex, g_msBuiltStartIndex);
            return false;
        }
    }
    int lo, hi;
    switch (g_msMode) {
    case MS_MODE_HANDS: lo = MS_CLS_HAND_A; hi = MS_CLS_HAND_B; break;
    case MS_MODE_ARMS:  lo = MS_CLS_ARM_A;  hi = MS_CLS_ARM_B;  break;
    case MS_MODE_OTHER: lo = MS_CLS_OTHER;  hi = MS_CLS_OTHER;  break;
    case MS_MODE_ALL:   lo = MS_CLS_HAND_A; hi = MS_CLS_OTHER;  break;
    default: return false;
    }
    const int start = g_msClsStart[lo];
    int count = 0;
    for (int c = lo; c <= hi; c++) count += g_msClsCount[c];
    if (count <= 0) { g_msDraws++; return true; }   // drawing nothing IS the answer

    // Keep the live register layout current for placement. Cheap: it compares
    // the shader pointer and only re-reads when it changes.
#if DVR_WITH_LEGACY
    if (g_mpWorld || (g_pcOn && g_pcWant > 0)) PcRefreshLayout(dev);
#else
    if (g_mpWorld) PcRefreshLayout(dev);
#endif

#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_packet_capture_draw.inc"
#endif

    // WHAT GETS DRAWN, AND IN HOW MANY DRAWS.
    //
    // Normally one merged draw over the whole class span, exactly as before.
    // With the draw-scoped palette armed, one draw PER HAND CLASS instead, so
    // each can carry its own c6 block - which is the entire point: the two
    // hands share one upload from the engine and cannot otherwise be given
    // different deltas. Off, or with no c6 block seen yet, this falls back to
    // the merged draw and the backend costs nothing.
    struct MpRange { int cls, start, count; };
    MpRange rng[2];
    int nrng = 0;
    // The palette must be the COMPLETE verified interval for this split, not
    // merely three registers of something. g_mpCacheN is 0 until every
    // register in the interval is valid, so this is a state test.
    bool perClass = g_mpOn && g_msMode == MS_MODE_HANDS &&
                    g_mpPalN > 0 && g_mpCacheN == g_mpPalN;
    if (perClass) {
        for (int c = MS_CLS_HAND_A; c <= MS_CLS_HAND_B; c++)
            if (g_msClsCount[c] > 0) {
                rng[nrng].cls   = c;
                rng[nrng].start = g_msClsStart[c];
                rng[nrng].count = g_msClsCount[c];
                nrng++;
            }
        if (!nrng) perClass = false;
    }
    if (g_mpOn && g_msMode == MS_MODE_HANDS && !perClass)
        InterlockedIncrement(&g_mpNoCache);
    if (!perClass) {
        rng[0].cls = -1; rng[0].start = start; rng[0].count = count; nrng = 1;
    }

    // The engine's buffers are put back before returning, on every path. Every
    // reference is taken and released inside this one call, so nothing outlives
    // the detour.
    IDirect3DIndexBuffer9* savedIb = NULL;
    IDirect3DVertexBuffer9* savedVb = NULL;
    UINT savedOff = 0, savedStride = 0;
    if (FAILED(dev->GetIndices(&savedIb))) savedIb = NULL;
    bool boundVb = false;
    if (g_msOwnVb && g_msVb) {
        if (FAILED(dev->GetStreamSource(0, &savedVb, &savedOff, &savedStride)))
            savedVb = NULL;
        boundVb = SUCCEEDED(dev->SetStreamSource(0, g_msVb, 0, g_msStride));
    }
    // THE SAME RULE AT DRAW TIME. If binding our vertex buffer failed but the
    // split contains generated vertices, our index list cannot be drawn
    // against the game's vertices - the re-based indices address our buffer,
    // not theirs. Abort the replacement and let the caller draw the original,
    // rather than submitting a draw that reads the wrong memory.
    if (!boundVb && g_msClipN > 0) {
        Log("ms: REFUSED at draw time - our vertex buffer would not bind and "
            "the split holds %d generated vertex(es), so its indices have no "
            "matching data in the game's buffer. Passing the draw through "
            "untouched.", g_msClipN);
        if (savedIb) savedIb->Release();
        if (savedVb) savedVb->Release();
        g_msPassThrough = true;
        return false;
    }
    // The depth-range lever. The game draws this mesh with MaxZ 0.001 so it can
    // never be occluded; restore the full range for our draws only, and put the
    // game's own back on every exit path below.
    D3DVIEWPORT9 savedVp; bool vpSaved = false;
    if (g_mpDepth && SUCCEEDED(dev->GetViewport(&savedVp))) {
        g_mpDepthSeen[0] = savedVp.MinZ; g_mpDepthSeen[1] = savedVp.MaxZ;
        if (savedVp.MaxZ < 0.5f) {          // only when it really is crushed
            D3DVIEWPORT9 full = savedVp;
            full.MinZ = 0.0f; full.MaxZ = 1.0f;
            if (SUCCEEDED(dev->SetViewport(&full))) {
                vpSaved = true;
                InterlockedIncrement(&g_mpDepthUsed);
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                    "ms/palette/depth: the game draws these hands with MinZ "
                    "%.4f MaxZ %.4f - the nearest thousandth of the depth "
                    "buffer, which is what makes them draw over everything. "
                    "Restoring 0..1 for our draws only. If they still composite "
                    "on top, the pass runs after a depth clear and there is "
                    "nothing to occlude against.",
                    savedVp.MinZ, savedVp.MaxZ);
            }
        }
    }

    // ONE CONTEXT PER ORIGINAL DRAW, acquired above the hand loop so both
    // hands consume identical constants by construction. This is the unit the
    // eye question has to be asked in; asking it per hand compared a draw with
    // itself.
    MpDrawCtx ctx; ctx.ok = false; ctx.why = "not acquired";
    if (g_mpWorld) {
        g_mpDrawsEntered++;
        if (MpAcquireCtx(dev, &ctx)) {
            g_mpDrawsSampled++;
            MpDrawCompare(&ctx);
        } else {
            g_mpDrawsRejected++;
            g_mpDrawRejectWhy = ctx.why;
        }
    }

    if (SUCCEEDED(dev->SetIndices(g_msIb))) {
        for (int r = 0; r < nrng; r++) {
            // The palette this range draws under. The delta is applied to the
            // class the tester selected and to no other, so the OTHER hand is
            // the control: if both move, the per-class scoping is not working
            // and the reading means nothing.
            if (perClass) {
#if DVR_WITH_LEGACY
                const bool hit = (g_mpHand == 2) ||
                                 (g_mpHand == 0 && rng[r].cls == MS_CLS_HAND_A) ||
                                 (g_mpHand == 1 && rng[r].cls == MS_CLS_HAND_B);
#endif
                // WHAT DELTA THIS RANGE CARRIES. The drive owns it when
                // armed and feeds BOTH classes; otherwise the axis probe does,
                // and that only touches the selected class so the other stays
                // as the reference.
                dvr::hf::Xform T;
                T.r = dvr::hf::identity3();
                T.t[0] = T.t[1] = T.t[2] = 0.0f;
                bool  useT = false;
                if (g_mpWorld) {
                    // BUILD A2: placement through the measured chain. No
                    // calibration, no neutral - the palm's current position is
                    // re-measured from the game's own palette every frame and
                    // the delta is target minus that, so the animated baseline
                    // is subtracted rather than left underneath.
                    const int hIdx = (rng[r].cls == MS_CLS_HAND_B) ? 1 : 0;
                    float q[3];
                    const char* why = "anchor refused";
                    if (MpAnchorPos(rng[r].cls, g_mpCache, g_mpCacheN, q) &&
                        MpWorldTarget(&ctx, hIdx, rng[r].cls, q, &T, &why)) {
                        useT = true;
                        InterlockedIncrement(&g_mpWorldOk);
                    } else {
                        // Refusing draws the engine's own hand. That is the
                        // fail-soft, and the reason is named so it can never
                        // read as "the feature does not work".
                        g_mpWorldWhy = why;
                        InterlockedIncrement(&g_mpWorldRefused);
                        MfNoteRefused(hIdx);     // VR-76: the flicker history
                        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
                            "ms/palette/world: hand %d NOT placed - %s. The "
                            "engine's own hand is drawn instead.", hIdx, why);
                    }
                }
                if (useT) {
                    static float buf[4 * 256];
                    MpBuild(buf, g_mpCache, g_mpCacheN, &T);
                    dvr::frame::orig_set_vs_const(dev, 6, buf, g_mpCacheN);
                } else {
                    dvr::frame::orig_set_vs_const(dev, 6, g_mpCache, g_mpCacheN);
                }
                InterlockedIncrement(&g_mpDraws);
            }
            if (boundVb)
                dvr::frame::orig_draw_indexed(dev, type, 0, 0,
                                              (UINT)(g_msVerts + g_msClipN),
                                              (UINT)rng[r].start * 3u,
                                              (UINT)rng[r].count);
            else
                dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex,
                                              numVertices,
                                              (UINT)rng[r].start * 3u,
                                              (UINT)rng[r].count);
            g_msDraws++;
        }
        // A D3D9 constant is CURRENT STATE, not a one-shot (draw_census.cpp:334
        // paid for that once already). Put the game's own block back, or every
        // draw after this one inherits our delta.
        if (perClass)
            dvr::frame::orig_set_vs_const(dev, 6, g_mpCache, g_mpCacheN);

#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_axis_beat.inc"
#else
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
            "ms/palette: %s, %d ranges, c6 x%u; per-class draws %ld, missing palette %ld",
            perClass ? "per-class" : "merged", nrng, g_mpCacheN, g_mpDraws, g_mpNoCache);
#endif
    }
    if (vpSaved) dev->SetViewport(&savedVp);   // the game's own range, always
    dev->SetIndices(savedIb);
    if (savedIb) savedIb->Release();
    if (boundVb) {
        dev->SetStreamSource(0, savedVb, savedOff, savedStride);
        if (savedVb) savedVb->Release();
    }
    return true;
}


// ---- the levers -------------------------------------------------------------

static const char* MsModeName(int m)
{
    switch (m) {
    case MS_MODE_OFF:   return "off (the split does nothing)";
    case MS_MODE_HANDS: return "HANDS - both hands, no arms";
    case MS_MODE_ALL:   return "all (every triangle, through our buffer - the A/B)";
    case MS_MODE_ARMS:  return "arms (the inverse cut - arms only, hands gone)";
    case MS_MODE_OTHER: return "other (only the triangles no arm claimed)";
    default: return "?";
    }
}

// Requests only. This runs on the PRESENT thread and touches no D3D object at
// all: the reclassify it asks for is performed by the draw detour, on the
// render thread, next to the draw it affects. Locking our own index buffer from
// this lane while the renderer was drawing from it would be a race with no
// symptom until the frame it corrupted.
// Rung 2. Present thread, from MsTick: turn each controller's travel since its
// neutral into a palette delta. It reads the pose slots present_tick has
// already filled (head = 0, hands = 3 and 4), so it makes no runtime call of
// its own and cannot race the thread that owns them.
// The controller offset the placement consumes. Present thread, from MsTick.
// It reads the pose slots present_tick has already filled (head = 0, hands = 3
// and 4), so it makes no runtime call of its own and cannot race the thread
// that owns them.
//
// GATED ON THE BACKEND, NOT ON ITS CONSUMERS. This went wrong three times, the
// same shape each time: MsTick hidden behind g_dcOn, then the poses behind
// g_mpDrive when another mode needed them, then behind two flags when a third
// did. Every time the engine's own hands were drawn instead, and every time it
// read as "the feature does not work" rather than as a lane that never ran.
// Listing consumers in a gate means editing the gate whenever one is added,
// and it will be forgotten again.
static void MpDriveTick(void)
{
    if (!g_mpOn) return;
    InterlockedIncrement(&g_mpTickRan);
    if (g_mpTickTid == 0) g_mpTickTid = GetCurrentThreadId();

    // ONE SNAPSHOT, built locally and published whole. Nothing below writes
    // anything a draw can see until the copy at the end, so a reader can never
    // combine this sample's orientation with the previous sample's position.
    // VR-68: WHICH head this normalisation uses. Default 0 is the historical
    // behaviour (the freshest head). The measured answer is 2 - the generation
    // the rendered view was actually built from - and the A/B below walks
    // 0/2/0/2 so a headset run decides it rather than an argument.
    if (g_mpPoseLagAb) {
        const uint64_t nowMs = GetTickCount64();
        if (g_mpPoseLagAbT0 == 0) g_mpPoseLagAbT0 = nowMs;
        const uint32_t seg = (uint32_t)((nowMs - g_mpPoseLagAbT0) / 15000u);
        if (seg != g_mpPoseLagAbSeg) {
            g_mpPoseLagAbSeg = seg;
            static const int kPlan[4] = { 0, 2, 0, 2 };
            if (seg < 4) {
                g_mpPoseLag = kPlan[seg];
                Log("ms/poselag: A/B segment %u of 4 - the hand is now normalised against the head "
                    "%s. %s Turn your head side to side and watch the WEAPON against the world, not "
                    "against your hand. Nothing else changed.",
                    seg + 1,
                    kPlan[seg] == 0 ? "FRESH (lag 0, today's behaviour)"
                                    : "the view was rendered from (lag 2, the measured answer)",
                    seg == 0 ? "This is the BASELINE."
                    : seg == 2 ? "BASELINE AGAIN - if the previous segment was better it must be worse now, "
                                 "or the improvement was not real."
                               : "This is the ALTERNATIVE.");
            } else {
                g_mpPoseLag = 0;
                g_mpPoseLagAb = false;
                Log("ms/poselag: A/B complete, baseline restored. Report which segments were worst and "
                    "best by NUMBER.");
            }
        }
    }
    int hlag = g_mpPoseLag;
    if (hlag < 0) hlag = 0;
    if (hlag > DVR_HEAD_HIST - 1) hlag = DVR_HEAD_HIST - 1;
    if (hlag >= g_headHistN) hlag = 0;                  // not enough history yet: fail soft to fresh
    const int hidx = (g_headHistIdx - hlag + DVR_HEAD_HIST) % DVR_HEAD_HIST;
    const float (*HEAD)[4] = g_headHistOk[hidx] ? g_headHist[hidx] : g_devPose[0];

    MpPoseSnap snap;
    memset(&snap, 0, sizeof(snap));
    snap.headOk = g_devPoseOk[0];
    for (int h = 0; h < 2; h++) {
        snap.inHead[h] = dvr::hf::identity3();
        if (!g_devPoseOk[0] || !g_devPoseOk[3 + h]) {
            if (g_mpCtlRUFOk[h]) {
                g_mpCtlRUFOk[h] = false;
                Log("ms/palette: hand %d lost its pose (head ok=%d hand ok=%d) "
                    "- dropping its target, so the hand goes back to the "
                    "engine's own position instead of sticking where it was.",
                    h, g_devPoseOk[0] ? 1 : 0, g_devPoseOk[3 + h] ? 1 : 0);
            }
            continue;
        }
        // Hand minus head in XR world metres, resolved into the HEAD's own
        // right/up/forward. Frame-free scalars: the draw turns them into a
        // world vector with the basis from its own constants, so nothing here
        // assumes anything about the game's axes.
        float w[3];
        for (int r = 0; r < 3; r++) w[r] = g_devPose[3 + h][r][3] - HEAD[r][3];
        const float rx = HEAD[0][0], ry = HEAD[1][0], rz = HEAD[2][0];
        const float ux = HEAD[0][1], uy = HEAD[1][1], uz = HEAD[2][1];
        const float fx = -HEAD[0][2], fy = -HEAD[1][2], fz = -HEAD[2][2];
        snap.ruf[h][0] = w[0]*rx + w[1]*ry + w[2]*rz;
        snap.ruf[h][1] = w[0]*ux + w[1]*uy + w[2]*uz;
        snap.ruf[h][2] = w[0]*fx + w[1]*fy + w[2]*fz;

        // THE ORIENTATION, through the SAME physical mapping as the position
        // above. g_devPose holds XR device-to-tracking matrices whose columns
        // are right, up and BACK; the position path negates the head's third
        // column to get forward, and F = diag(1,1,-1) is that same conversion
        // written as a matrix. What is published is
        //
        //     F * transpose(R_head) * R_controller
        //
        // which the draw completes by multiplying with its own camera basis B.
        // It is a POSE conversion, mapping controller-local axes into another
        // frame - NOT a similarity transform of a head-relative rotation. The
        // similarity form rotates the hands with the head while the controller
        // stands still, and `head_turn` in the self-test is that counterexample.
        {
            float hc[3][3], cc[3][3];
            for (int rr = 0; rr < 3; rr++)
                for (int c2 = 0; c2 < 3; c2++) {
                    hc[rr][c2] = HEAD[rr][c2];
                    cc[rr][c2] = g_devPose[3 + h][rr][c2];
                }
            dvr::hf::Mat3 R_H, R_C;
            for (int rr = 0; rr < 3; rr++)
                for (int c2 = 0; c2 < 3; c2++) {
                    R_H.m[rr*3+c2] = hc[rr][c2];
                    R_C.m[rr*3+c2] = cc[rr][c2];
                }
            snap.inHead[h] = dvr::hf::controller_orient_in_head(R_H, R_C);
        }
        snap.ok[h] = true;
        memcpy(g_mpCtlRUF[h], snap.ruf[h], sizeof(snap.ruf[h]));
        g_mpCtlRUFOk[h] = true;
    }

    // PUBLISH. One lock, one whole structure, one generation. See MpPoseSnap.
    if (g_mpPoseCsOk) {
        snap.gen = ++g_mpPoseGen;
        EnterCriticalSection(&g_mpPoseCs);
        g_mpPosePub = snap;
        LeaveCriticalSection(&g_mpPoseCs);
    }

    if (!g_mpWorld) return;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "ms/palette/world: L ctl r/u/f (%+.3f %+.3f %+.3f) m -> pcam "
        "(%+.1f %+.1f %+.1f) uu | R ctl (%+.3f %+.3f %+.3f) m | placed %ld "
        "refused %ld (%s) | layout vp c%d l2w c%d | %.0f uu/m x %.2f",
        g_mpCtlRUF[0][0], g_mpCtlRUF[0][1], g_mpCtlRUF[0][2],
        g_mpLastPCam[0][0], g_mpLastPCam[0][1], g_mpLastPCam[0][2],
        g_mpCtlRUF[1][0], g_mpCtlRUF[1][1], g_mpCtlRUF[1][2],
        g_mpWorldOk, g_mpWorldRefused, g_mpWorldWhy,
        g_pcLayVp, g_pcLayL2W, (double)g_skcWorldScale, (double)g_mpDriveGain);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "ms/palette/sampling: original draws entered %ld, sampled %ld, rejected "
        "%ld (%s) | draw-to-draw comparisons %ld",
        g_mpDrawsEntered, g_mpDrawsSampled, g_mpDrawsRejected,
        g_mpDrawRejectWhy ? g_mpDrawRejectWhy : "none", g_mpCmpCount);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "ms/palette/eye: state %s | L %ld R %ld unknown %ld draws | presents: "
        "%ld toggled, %ld same eye, %ld ambiguous | expected IPD %.2f uu. The "
        "eye is decided once per PRESENT from the right-axis jump in "
        "LocalToWorld's translation, and its SIGN gives left or right "
        "absolutely. 'Ambiguous' rising means the head moved far enough between "
        "presents to leave the band, and those draws take NO offset.",
        g_mpEyeState < 0 ? "LEFT" : g_mpEyeState > 0 ? "RIGHT" : "unknown",
        g_mpEyeSeen[0], g_mpEyeSeen[1], g_mpEyeUnclassified,
        g_mpEyeToggles, g_mpEyeSame, g_mpEyeAmbiguous,
        (double)(g_ipdM * g_skcWorldScale));

    // THE FRAME BEAT. Three SEPARATELY NAMED orientations, because they are
    // three different measurements and conflating them produced a test that
    // could not be passed: with rotation OFF nothing connects the controller to
    // the game's palette, so the controller moving while the source frame does
    // not is the CORRECT reading, not a failure.
    //
    //   src  the candidate palm frame, read from the original palette. Moves
    //        only when the GAME animates the hand.
    //   ctl  the controller, from the tracked pose. Moves when you move.
    //   out  the correction actually applied, and it exists only when rotating.
    //
    // Each is reported as an angle from the first sample seen, which is the
    // only honest single number for an orientation difference: three Euler
    // numbers wrap and reorder and can read as an axis failure when nothing is
    // wrong.
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "ms/palette/frame: L src %s %+.1f deg (max %.1f) scale %.6f aniso %.5f "
        "ortho %.5f | L ctl %+.1f deg (max %.1f) | L out %s %.1f deg || "
        "R src %s %+.1f (max %.1f) | R ctl %+.1f (max %.1f) | R out %s %.1f | "
        "rotate=%d placed %ld refused %ld (%s) | slots L%d R%d gen %u | "
        "selftest %s. src and ctl are INDEPENDENT while rotate=0 - that is "
        "correct, nothing joins them yet; src should move only when the game "
        "animates the hand.",
        g_mpSrcOk[0] ? "ok" : "REFUSED", (double)g_mpSrcMoved[0],
        (double)g_mpSrcMovedMax[0], (double)g_mpSrcScale[0],
        (double)g_mpSrcAniso[0], (double)g_mpSrcOrtho[0],
        (double)g_mpCtlMoved[0], (double)g_mpCtlMovedMax[0],
        g_mpOutOk[0] ? "on" : "off", (double)g_mpOutMoved[0],
        g_mpSrcOk[1] ? "ok" : "REFUSED", (double)g_mpSrcMoved[1],
        (double)g_mpSrcMovedMax[1], (double)g_mpCtlMoved[1],
        (double)g_mpCtlMovedMax[1],
        g_mpOutOk[1] ? "on" : "off", (double)g_mpOutMoved[1],
        g_mpRotate ? 1 : 0, g_mpRotOk, g_mpRotRefused, g_mpRotWhy,
        g_mpDomSlot[MS_CLS_HAND_A], g_mpDomSlot[MS_CLS_HAND_B], g_mpSrcGen,
        g_mpSelfTestFailed == 0 ? "PASSED" :
            (g_mpSelfTestFailed < 0 ? "NOT RUN" : "FAILED"));

    // A PRESS THAT DID NOTHING MUST SAY SO. The grip capture is consumed by the
    // next qualified draw, so if rotation is refusing, the request just sits
    // there and SHIFT+F7 appears to do nothing at all - which is exactly what
    // happened on the first headset run. This makes the pending request and the
    // reason visible without needing the tester to ask.
    if (g_mpGripCapReq)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
            "ms/palette/grip: a capture is STILL PENDING for %s%s%s - no "
            "qualified draw has consumed it. %s. Nothing has been solved and "
            "the hands have not moved.",
            (g_mpGripCapReq & 1) ? "LEFT" : "",
            (g_mpGripCapReq == 3) ? " and " : "",
            (g_mpGripCapReq & 2) ? "RIGHT" : "",
            !g_mpRotate ? "[Hands] PaletteRotate is 0, so rotation is off"
                        : g_mpRotWhy);

    // The lane contract, and the coherence of the snapshot that crosses it.
    // Measured rather than assumed, and printed rarely because it does not
    // change: two lanes that turn out to be one thread is a fact worth having
    // in the log, and one that is not is a fact worth having BEFORE a race is
    // blamed on the maths.
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 30000,
        "ms/palette/lane: pose published on thread %lu, draws consume on thread "
        "%lu - %s. Snapshot generation %u, %ld draw(s) saw a snapshot older "
        "than the previous draw's. The whole snapshot crosses under one lock, "
        "so a draw can never mix this sample's orientation with the last "
        "sample's position; the counter is what would make that visible.",
        (unsigned long)g_mpTickTid, (unsigned long)g_mpDrawTid,
        g_mpLaneSame < 0 ? "not yet compared" :
            (g_mpLaneSame ? "the SAME lane" : "DIFFERENT lanes"),
        g_mpPoseGen, g_mpPoseStale);
}


// THE CALIBRATION AND TRIM TICK. Present thread, because file I/O has no place
// in a draw detour: the draw only sets a bit.
// The four adjust modes, named the way the log has to name them: which hand,
// and whether the keys are moving it or turning it. A mode INDEX in a log is
// no use to somebody in a headset who cannot read the log while pressing.
static const char* MpAdjModeName(int m)
{
    switch (m) {
    case 0: return "LEFT hand POSITION";
    case 1: return "LEFT hand ROTATION";
    case 2: return "RIGHT hand POSITION";
    default: return "RIGHT hand ROTATION";
    }
}

// The palm frame axes the trim is actually stored in. The keys are labelled
// with the BRVR words the tester knows and every line prints BOTH, so a press
// can be checked against the number it changed rather than trusted.
//
//   TX across the palm   -> right / left    (RX turns about it: pitch)
//   TY along the fingers -> forward / back  (RY turns about it: roll)
//   TZ out of the palm   -> up / down       (RZ turns about it: yaw)
//
// pitch/yaw/roll are named for the HAND, not the head: forward is the
// fingers, up is out of the palm, right is across it.
static const char* MpTrimAxisName(int a)
{
    switch (a) {
    case 0: return "TX (across the palm: right/left)";
    case 1: return "TY (along the fingers: forward/back)";
    case 2: return "TZ (out of the palm: up/down)";
    case 3: return "RX (about the across-palm axis: pitch)";
    case 4: return "RY (about the finger axis: roll)";
    default: return "RZ (about the out-of-palm axis: yaw)";
    }
}

// Request bit 0..5 -> which stored axis the key drives, and its sign.
// The bits are in key order: 8, 2, 6, 4, 0, 5.
static const int   kMpAdjAxis[6] = { 1, 1, 0, 0, 2, 2 };
static const float kMpAdjSign[6] = { +1.0f, -1.0f, +1.0f, -1.0f, +1.0f, -1.0f };
static const char* kMpAdjKeyName[6] = { "Numpad 8", "Numpad 2", "Numpad 6",
                                        "Numpad 4", "Numpad 0", "Numpad 5" };

static void MpCalibTick(void)
{
    // Save a freshly solved grip, once, per side.
    const LONG save = InterlockedExchange(&g_mpGripSaveReq, 0);
    if (save) {
        for (int h = 0; h < 2; h++) {
            if (!(save & (1 << h))) continue;
            const char* sfx = h ? "R" : "L";
            char key[32], v[64];
            _snprintf(key, sizeof(key), "Grip%sVersion", sfx);
            _snprintf(v, sizeof(v), "%d", MP_GRIP_VERSION);
            ConfigWriteKey("Hands", key, v, "the grip capture");
            _snprintf(key, sizeof(key), "Grip%sParity", sfx);
            _snprintf(v, sizeof(v), "%d", g_mpGripSaveParity[h]);
            ConfigWriteKey("Hands", key, v, "the grip capture");
            static const char* ax[3] = { "X", "Y", "Z" };
            for (int a = 0; a < 3; a++) {
                _snprintf(key, sizeof(key), "Grip%s%s", sfx, ax[a]);
                _snprintf(v, sizeof(v), "%.4f", g_mpGripSaveDeg[h][a]);
                ConfigWriteKey("Hands", key, v, "the grip capture");
            }
            Log("ms/palette/grip: the %s hand's calibration is SAVED (version %d, "
                "parity %+d). It will be loaded automatically on the next launch "
                "- no key press, and no ini editing.",
                h ? "right" : "left", MP_GRIP_VERSION, g_mpGripSaveParity[h]);
        }
    }

    // ---- THE NUMPAD ADJUST -------------------------------------------------
    const LONG req = InterlockedExchange(&g_mpAdjReq, 0);
    if (!req) return;

    // Numpad 9: cycle the mode.
    if (req & 0x80) {
        g_mpAdjMode = (g_mpAdjMode + 1) % 4;
        const int mh = g_mpAdjMode >> 1;
        Log("ms/palette/adjust: >>> %s <<< | step %.2f %s | this hand is now at "
            "translation (%+.1f %+.1f %+.1f) mm, rotation (%+.2f %+.2f %+.2f) "
            "deg. Numpad 8/2 forward/back, 6/4 right/left, 0/5 up/down; "
            "Numpad 7 changes the step, Numpad 9 the mode.",
            MpAdjModeName(g_mpAdjMode),
            (g_mpAdjMode & 1) ? (double)kMpAdjStepR[g_mpAdjStepR]
                              : (double)(kMpAdjStepT[g_mpAdjStepT] * 100.0f),
            (g_mpAdjMode & 1) ? "deg" : "cm",
            (double)(g_mpTrimT[mh][0]*1000.0f), (double)(g_mpTrimT[mh][1]*1000.0f),
            (double)(g_mpTrimT[mh][2]*1000.0f),
            (double)g_mpTrimR[mh][0], (double)g_mpTrimR[mh][1],
            (double)g_mpTrimR[mh][2]);
        return;
    }

    // Numpad 7: cycle the step. Position and rotation keep their own index, so
    // switching mode never silently changes the other one's step under you.
    if (req & 0x40) {
        char sv[32];
        if (g_mpAdjMode & 1) {
            g_mpAdjStepR = (g_mpAdjStepR + 1) % 7;
            Log("ms/palette/adjust: step now %.2f deg (%s).",
                (double)kMpAdjStepR[g_mpAdjStepR], MpAdjModeName(g_mpAdjMode));
            _snprintf(sv, sizeof(sv), "%d", g_mpAdjStepR);
            ConfigWriteKey("Hands", "AdjStepR", sv, "the numpad adjust");
        } else {
            g_mpAdjStepT = (g_mpAdjStepT + 1) % 3;
            Log("ms/palette/adjust: step now %.1f cm (%s).",
                (double)(kMpAdjStepT[g_mpAdjStepT] * 100.0f),
                MpAdjModeName(g_mpAdjMode));
            _snprintf(sv, sizeof(sv), "%d", g_mpAdjStepT);
            ConfigWriteKey("Hands", "AdjStepT", sv, "the numpad adjust");
        }
        return;
    }

    // One of the six directional keys. Take the LOWEST pending bit only: two
    // opposite keys landing in one tick must not cancel into silence.
    int bit = -1;
    for (int b = 0; b < 6; b++) if (req & (1L << b)) { bit = b; break; }
    if (bit < 0) return;

    const int   h    = g_mpAdjMode >> 1;         // 0 left, 1 right
    const bool  rot  = (g_mpAdjMode & 1) != 0;
    const int   ax   = kMpAdjAxis[bit];
    const float step = rot ? kMpAdjStepR[g_mpAdjStepR]
                           : kMpAdjStepT[g_mpAdjStepT];
    float* cell  = rot ? &g_mpTrimR[h][ax] : &g_mpTrimT[h][ax];
    const float before = *cell;
    *cell += kMpAdjSign[bit] * step;

    // Clamp, and SAY SO. A silent clamp reads in a headset as "the key did
    // nothing", which is the same symptom as a dead binding.
    const float lim = rot ? 45.0f : 0.25f;
    bool clamped = false;
    if (*cell >  lim) { *cell =  lim; clamped = true; }
    if (*cell < -lim) { *cell = -lim; clamped = true; }
    if (clamped)
        Log("ms/palette/adjust: CLAMPED at %+.2f %s. The key IS working and the "
            "trim will not go further. A correction this large is a wrong grip "
            "calibration rather than a trim - press SHIFT+F7 again.",
            (double)(*cell * (rot ? 1.0f : 100.0f)), rot ? "deg" : "cm");

    // DOES THIS PRESS REACH THE HANDS? The trim is only applied through
    // palm_target, which is on the ROTATION path. With rotation refused the
    // number below still changes and is still saved, and the hand does not
    // move - which is exactly the symptom of a dead key. Say which it is,
    // rather than letting the tester find out by pressing it thirty times.
    if (g_mpRotOk == 0)
        Log("ms/palette/adjust: WARNING - this press will NOT move the hand "
            "yet. The trim rides the rotation path (palm_target) and rotation "
            "has placed 0 draws so far (%ld refused; %s). The value below is "
            "still saved and will apply the moment rotation starts placing.",
            g_mpRotRefused, g_mpRotWhy);

    // Write back under the per-hand key so a restart brings it back.
    static const char* tk[3] = { "TX", "TY", "TZ" };
    static const char* rk[3] = { "RX", "RY", "RZ" };
    char key[32], v[64];
    _snprintf(key, sizeof(key), "Trim%s%s", h ? "R" : "L", rot ? rk[ax] : tk[ax]);
    _snprintf(v, sizeof(v), rot ? "%.2f" : "%.4f", (double)*cell);
    ConfigWriteKey("Hands", key, v, "the numpad adjust");

    Log("ms/palette/adjust: %s | %s -> %s %+.2f %s (was %+.2f, step %.2f) | the "
        "%s hand is now at translation (%+.1f %+.1f %+.1f) mm, rotation "
        "(%+.2f %+.2f %+.2f) deg. Saved to [Hands] %s, so it survives a "
        "restart. The trim is in the CALIBRATED PALM FRAME - it rides the palm "
        "rather than the world, and it moves anything held in that hand by the "
        "same transform.",
        MpAdjModeName(g_mpAdjMode), kMpAdjKeyName[bit],
        MpTrimAxisName(rot ? ax + 3 : ax),
        (double)(*cell * (rot ? 1.0f : 100.0f)), rot ? "deg" : "cm",
        (double)(before * (rot ? 1.0f : 100.0f)),
        (double)(step * (rot ? 1.0f : 100.0f)),
        h ? "RIGHT" : "LEFT",
        (double)(g_mpTrimT[h][0]*1000.0f), (double)(g_mpTrimT[h][1]*1000.0f),
        (double)(g_mpTrimT[h][2]*1000.0f),
        (double)g_mpTrimR[h][0], (double)g_mpTrimR[h][1],
        (double)g_mpTrimR[h][2], key);
}


static void MsTick(void)
{
    MpDriveTick();
    MpCalibTick();
#if DVR_WITH_LEGACY
    PcTick();
#endif
#if DVR_WITH_LEGACY
    WiFinishTick();     // VR-33 W1: the report, from whichever lane gets there
#endif
    WaBeat();           // VR-33 W2/W3: prints even when nothing ever matched
#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_axis_tick.inc"
#endif
    if (!g_msOn) return;
    if (g_msModeReq) {
        const int r = g_msModeReq; g_msModeReq = 0;
        int m = g_msMode + (r > 0 ? 1 : -1);
        if (m >= MS_MODE_N) m = 0;
        if (m < 0) m = MS_MODE_N - 1;
        g_msMode = m;
        Log("ms/mode: >>> %s <<<  (Numpad 0 for the next mode)", MsModeName(m));
        if (!g_msReady)
            Log("ms/mode:   no split is built yet, so the mode does nothing "
                "until the arm mesh is drawn and read. `ms status` says why.");
    }
    if (g_msStepReq) {
        g_msStepReq = 0;
        g_msStepMode = (g_msStepMode + 1) % 3;
        Log("ms/wrist: step size -> %s, %.1f%% of the arm (%.2f on side A, %.2f "
            "on side B) per press",
            g_msStepMode == 0 ? "COARSE" : g_msStepMode == 1 ? "fine" : "ULTRAFINE",
            kMsStep[g_msStepMode] * 100.0f,
            kMsStep[g_msStepMode] * g_msAxialLen[1],
            kMsStep[g_msStepMode] * g_msAxialLen[2]);
    }
    if (g_msSideReq) {
        g_msSideReq = 0;
        g_msKnobSide = (g_msKnobSide + 1) % 3;
        Log("ms/wrist: the + / - knob now moves %s",
            g_msKnobSide == 0 ? "BOTH arms" :
            g_msKnobSide == 1 ? "side A only" : "side B only");
    }
    if (g_msWristReq) {
        const int r = g_msWristReq; g_msWristReq = 0;
        if (!g_msReady) {
            Log("ms/wrist: nothing to move - no split has been built yet");
        } else {
            // The plane moves in LENGTH, a fixed fraction of the arm, so the
            // ring travels smoothly instead of waiting for a whole bone to
            // flip. The sphere's knob is still its radius, for the fallback.
            for (int s = 1; s <= 2; s++) {
                if (g_msKnobSide && g_msKnobSide != s) continue;
                if (g_msPlane) {
                    g_msCutRel[s] -= (r > 0 ? 1.0f : -1.0f) *
                                     kMsStep[g_msStepMode] * g_msAxialLen[s];
                    const float lim = 0.9f * g_msAxialLen[s];
                    if (g_msCutRel[s] < -lim) g_msCutRel[s] = -lim;
                    if (g_msCutRel[s] >  lim) g_msCutRel[s] =  lim;
                    g_msCutSet[s] = 1;
                } else {
                    g_msWristScale[s] *= (r > 0 ? 1.06f : 1.0f / 1.06f);
                    if (g_msWristScale[s] < 0.2f) g_msWristScale[s] = 0.2f;
                    if (g_msWristScale[s] > 5.0f) g_msWristScale[s] = 5.0f;
                }
            }
            if (g_msPlane)
                Log("ms/wrist: %s - the ring is now %.1f from the hand bone on "
                    "side A and %.1f on side B. Those two numbers are exactly "
                    "what [Hands] WristCutA / WristCutB take, so a look worth "
                    "keeping can be made the default without another walk.",
                    r > 0 ? "MORE kept as hand, the ring moved up the arm"
                          : "LESS kept as hand, the ring moved toward the fingers",
                    g_msCutRel[1], g_msCutRel[2]);
            else
                Log("ms/wrist: %s - side A scale %.2f (radius %.1f), side B %.2f "
                    "(radius %.1f)",
                    r > 0 ? "MORE kept as hand" : "LESS kept as hand",
                    g_msWristScale[1], g_msWristR[1] * g_msWristScale[1],
                    g_msWristScale[2], g_msWristR[2] * g_msWristScale[2]);
            g_msReclassReq = 1;
        }
    }
    if (g_msRebuildReq) {
        g_msRebuildReq = 0;
        g_msReady = 0; g_msRefused = 0;
        Log("ms/rebuild: the split will be re-derived from the buffers on the "
            "next draw of the locked mesh");
    }
    if (g_msReady || g_msRefused) {
        const double now = MaimNowMs();
        if (now >= g_msNextReport) {
            g_msNextReport = now + 15000.0;
            Log("ms: beat - mode %s | %u draw(s) served from our index buffer, "
                "%u fell through to the game's. BOTH at zero means the locked "
                "mesh is not being drawn at all, which is normal in a menu or a "
                "cutscene; a rising fallback with a ready split means the draw "
                "did not match the one the split was built from.",
                MsModeName(g_msMode), g_msDraws, g_msFallback);
            g_msDraws = g_msFallback = 0;
        }
    }
}


static bool MsCommand(const char* args)
{
    char sub[24] = ""; float f = 0.0f;
    const int got = sscanf(args, "%23s %f", sub, &f);
    if (got < 1 || !_stricmp(sub, "status")) {
        Log("ms: %s | mode %s | %s | %d tri(s), %d vert(s), %d bone(s)",
            g_msOn ? "enabled" : "disabled", MsModeName(g_msMode),
            g_msReady ? "split READY" :
            g_msRefused ? "a read was tried and REFUSED - the reason is above" :
                          "no split built yet (the arm mesh has not been read)",
            g_msTris, g_msVerts, g_msBones);
        if (g_msReady) {
            Log("ms:   handA %d, handB %d, armA %d, armB %d, other %d | hand "
                "bones %d and %d | lateral axis %d",
                g_msClsCount[0], g_msClsCount[1], g_msClsCount[2],
                g_msClsCount[3], g_msClsCount[4],
                g_msHandBone[1], g_msHandBone[2], g_msSideAxis);
            if (g_msPlane)
                Log("ms:   PLANE cut - ring A %.1f from the hand bone (arm %.1f "
                    "long, axis %.3f %.3f %.3f), ring B %.1f (arm %.1f). Edge "
                    "rule %d. These are [Hands] WristCutA / WristCutB.",
                    g_msCutRel[1], g_msAxialLen[1], g_msAxis[1][0], g_msAxis[1][1],
                    g_msAxis[1][2], g_msCutRel[2], g_msAxialLen[2], g_msEdge);
            else
                Log("ms:   SPHERE cut - radius A %.1f (x%.2f), B %.1f (x%.2f)",
                    g_msWristR[1] * g_msWristScale[1], g_msWristScale[1],
                    g_msWristR[2] * g_msWristScale[2], g_msWristScale[2]);
            Log("ms:   %d output triangle(s), %d clipped vertex(es), stream 0 %s "
                "| step %s (%.1f%% of the arm per press)",
                g_msOutN, g_msClipN,
                g_msOwnVb ? "is ours" : "belongs to the game (no clipping)",
                g_msStepMode == 0 ? "COARSE" : g_msStepMode == 1 ? "fine" : "ULTRAFINE",
                kMsStep[g_msStepMode] * 100.0f);
        }
        return true;
    }
    if (!_stricmp(sub, "off"))   { g_msMode = MS_MODE_OFF;   Log("ms: %s", MsModeName(g_msMode)); return true; }
    if (!_stricmp(sub, "hands")) { g_msMode = MS_MODE_HANDS; Log("ms: %s", MsModeName(g_msMode)); return true; }
    if (!_stricmp(sub, "arms"))  { g_msMode = MS_MODE_ARMS;  Log("ms: %s", MsModeName(g_msMode)); return true; }
    if (!_stricmp(sub, "all"))   { g_msMode = MS_MODE_ALL;   Log("ms: %s", MsModeName(g_msMode)); return true; }
    if (!_stricmp(sub, "other")) { g_msMode = MS_MODE_OTHER; Log("ms: %s", MsModeName(g_msMode)); return true; }
    if (!_stricmp(sub, "rebuild")) { g_msRebuildReq = 1; Log("ms: rebuild queued"); return true; }
    if (!_stricmp(sub, "wrist") && got >= 2) {
        if (f < 0.2f) f = 0.2f;
        if (f > 5.0f) f = 5.0f;
        for (int s = 1; s <= 2; s++)
            if (!g_msKnobSide || g_msKnobSide == s) g_msWristScale[s] = f;
        g_msReclassReq = 1;
        Log("ms: wrist scale -> side A %.2f, side B %.2f (reclassified on the "
            "next tick, which is where a device is in hand)",
            g_msWristScale[1], g_msWristScale[2]);
        return true;
    }
    if (!_stricmp(sub, "cut")) {
        // `ms cut <a> [b]` places the ring exactly, in mesh units from the hand
        // bone, which is the number the knob prints. This is how a tuned look
        // becomes [Hands] WristCutA / WristCutB without repeating the walk.
        float a = 0.0f, b = 0.0f;
        const char* p = args;
        while (*p && *p != ' ') p++;
        const int nf = sscanf(p, "%f %f", &a, &b);
        if (nf < 1) {
            Log("ms: cut <a> [b] - mesh units from the hand bone, positive "
                "toward the fingers. Now A %.1f, B %.1f; each arm is %.1f / "
                "%.1f long.", g_msCutRel[1], g_msCutRel[2],
                g_msAxialLen[1], g_msAxialLen[2]);
            return true;
        }
        if (nf < 2) b = a;
        g_msCutRel[1] = a; g_msCutRel[2] = b;
        g_msCutSet[1] = g_msCutSet[2] = 1;
        g_msReclassReq = 1;
        Log("ms: ring -> A %.1f, B %.1f from the hand bone", a, b);
        return true;
    }
    if (!_stricmp(sub, "axis")) {
        const char* q = args;
        while (*q && *q != ' ') q++;
        while (*q == ' ') q++;
        if (!_stricmp(q, "bone"))     g_msAxisMode = 0;
        else if (!_stricmp(q, "pca")) g_msAxisMode = 1;
        else { Log("ms: axis bone | pca (now %s). BONE is the direction from "
                   "the forearm bone to the hand bone; PCA is the longest "
                   "direction of the arm's triangles. A ring square to the "
                   "wrong one is slanted across the forearm.",
                   g_msAxisMode == 0 ? "bone" : "pca"); return true; }
        g_msRederiveReq = 1;
        Log("ms: axis -> %s, re-deriving both rings",
            g_msAxisMode == 0 ? "the forearm BONE" : "the triangle cloud (PCA)");
        return true;
    }
    if (!_stricmp(sub, "shape")) {
        const char* p = args;
        while (*p && *p != ' ') p++;
        while (*p == ' ') p++;
        if (!_stricmp(p, "sphere"))     g_msPlane = false;
        else if (!_stricmp(p, "plane")) g_msPlane = true;
        else { Log("ms: shape plane | sphere (now %s)",
                   g_msPlane ? "plane" : "sphere"); return true; }
        g_msReclassReq = 1;
        Log("ms: cut shape -> %s. The sphere moves in whole bones and leaves a "
            "blobby edge; the plane cuts the forearm in a circle and moves "
            "smoothly.", g_msPlane ? "PLANE" : "sphere");
        return true;
    }
    if (!_stricmp(sub, "edge") && got >= 2) {
        g_msEdge = (int)f; if (g_msEdge < 0 || g_msEdge > 3) g_msEdge = 3;
        g_msReclassReq = 1;
        Log("ms: edge rule %d - %s. 3 is the only one whose boundary is the "
            "plane itself; 0, 1 and 2 all round the cut to whole triangles and "
            "leave a sawtooth one triangle high.", g_msEdge,
            g_msEdge == 0 ? "kept when the centroid is past the plane" :
            g_msEdge == 1 ? "kept only when all three vertices are past it" :
            g_msEdge == 2 ? "kept when any vertex is past it" :
                            "CLIPPED at the plane, new vertices and all");
        return true;
    }
    if (!_stricmp(sub, "side") && got >= 2) {
        g_msKnobSide = ((int)f % 3 + 3) % 3;
        Log("ms: the wrist knob moves %s", g_msKnobSide == 0 ? "both arms" :
            g_msKnobSide == 1 ? "side A" : "side B");
        return true;
    }
    Log("ms: usage - ms status | off | hands | arms | all | other | rebuild | "
        "cut <a> [b] | axis bone|pca | shape plane|sphere | edge <0|1|2|3> | "
        "wrist <0.2..5> | side <0|1|2>");
    return true;
}
