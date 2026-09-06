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
    for (UINT i = 0; i < n; i++) {
        if (el[i].Stream != 0 || el[i].Type == D3DDECLTYPE_UNUSED) continue;
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
        g_msExtraStream = -1;
        for (UINT si = 1; si < 8; si++) {
            IDirect3DVertexBuffer9* ex = NULL; UINT eo = 0, es = 0;
            if (SUCCEEDED(dev->GetStreamSource(si, &ex, &eo, &es)) && ex) {
                ex->Release();
                if (es) { g_msExtraStream = (int)si; break; }
            }
        }
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
                Log("ms: REFUSED - could not create a %u byte vertex buffer; "
                    "falling back to the game's, which means no clipping", room);
                g_msOwnVb = false;
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
    if (!MsRead(dev, baseVertex, minIndex, numVertices, startIndex, primCount, bones))
        return false;
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
    Log("ms: ==== READY - mode %s. Numpad 0 cycles the mode, + / - move the "
        "wrist, * picks which arm the wrist knob moves, / re-derives. ====",
        MsModeName(g_msMode));
    return true;
}


// ---- the draw ---------------------------------------------------------------

// Emit the classes this mode wants, through OUR index buffer. Returns false if
// it drew nothing, and the caller then does whatever it would have done - which
// is the fail-soft: an auto-armed lock with no usable split draws the mesh
// exactly as the game asked for it.
static bool MsDraw(IDirect3DDevice9* dev, D3DPRIMITIVETYPE type, INT baseVertex,
                   UINT minIndex, UINT numVertices, UINT primCount)
{
    if (!g_msReady || !g_msIb || g_msMode == MS_MODE_OFF) return false;
    if (type != D3DPT_TRIANGLELIST) return false;
    if ((int)primCount != g_msTris) return false;   // a different draw of this pair

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
    if (SUCCEEDED(dev->SetIndices(g_msIb))) {
        if (boundVb)
            dvr::frame::orig_draw_indexed(dev, type, 0, 0,
                                          (UINT)(g_msVerts + g_msClipN),
                                          (UINT)start * 3u, (UINT)count);
        else
            dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex,
                                          numVertices, (UINT)start * 3u,
                                          (UINT)count);
        g_msDraws++;
    }
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
static void MsTick(void)
{
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
