// VR-138: fill the unmodelled side of the pistol and crossbow with a mirrored
// draw. Plan and rationale: docs/dishonored/WEAPON_MIRROR_PLAN.md.
//
// Included by the unity build immediately before weapon_attach.cpp, which calls
// WmDraw from inside its patched-palette scope, right after a PLACED draw of an
// allowed asset succeeds and before the palette and viewport are restored:
//
//   mirror[i] = delta * P[i] * S      S = reflection in reference-pose space
//
// S on the right reflects the vertex buffer's own coordinates before skinning,
// so every bone takes the same S and the weighted blend commutes (frame_test
// `mirror_compose`). The cull mode is flipped for the copy, and the copy draws
// through OUR index buffer holding only the triangles whose mirror image lands
// where the original has no geometry, so both-sided parts do not z-fight.
//
// Deviation from the plan, on purpose: the per-geometry state lives in its own
// small table keyed by the draw's buffers and ranges plus the asset name, not in
// WaMesh. WaMesh is memset and evicted in several places; a separate table needs
// one release hook (WaInvalidateContracts) and rebuilds on any key mismatch.
//
// LANE: the render thread, inside the game's own draw call. The Get* references
// are released inside the call; only the saved index buffer is held across our
// draw and released right after its restore (the mesh_split.cpp pattern).
#include <vector>
#include <unordered_map>

namespace {
struct WmEntry {
    // key
    void* vb; void* ib; UINT startIndex, primCount, numVerts, minIndex, stride; INT baseVertex;
    char asset[64];
    // result
    int state;                 // 1 built, 2 refused (until rebuild)
    IDirect3DDevice9* dev;
    IDirect3DIndexBuffer9* ours;
    UINT prims;
    float n[3], c[3];
    bool measured;
    unsigned long long lastUse;
    LONG drawn, failed;
};
constexpr int kWmMax = 8;
WmEntry g_wm[kWmMax];
int g_wmN = 0;
bool g_wmOn = false;
float g_wmEps = 0.25f, g_wmFill = 1.5f;
char g_wmAssets[256] = "Wpn_PlyGunElite,crossbow_01";
char g_wmIni[MAX_PATH] = "";
LONG g_wmDrawn = 0, g_wmFailed = 0, g_wmRefused = 0;
}

static void WmReleaseEntry(WmEntry* e) {
    if (e->ours) { e->ours->Release(); e->ours = nullptr; }
    memset(e, 0, sizeof(*e));
}
static void WmReleaseAll(const char* why) {
    if (!g_wmN) return;
    for (int i = 0; i < g_wmN; ++i) WmReleaseEntry(&g_wm[i]);
    Log("mirror: released %d built mirror(s) - %s", g_wmN, why);
    g_wmN = 0;
}
static void WmSet(bool on) {
    g_wmOn = on;
    Log("mirror: %s (VR-138; assets %s, Eps %.2f uu, FillRadius %.2f uu)", on ? "ON" : "off", g_wmAssets, g_wmEps, g_wmFill);
}
static bool WmEnabled() { return g_wmOn; }
static void WmConfigure(const char* ini) {
    _snprintf(g_wmIni, sizeof(g_wmIni), "%s", ini); g_wmIni[sizeof(g_wmIni) - 1] = 0;
    GetPrivateProfileStringA("Mirror", "Assets", "Wpn_PlyGunElite,crossbow_01", g_wmAssets, sizeof(g_wmAssets), ini);
    char v[32];
    GetPrivateProfileStringA("Mirror", "Eps", "0.25", v, sizeof(v), ini); g_wmEps = (float)atof(v);
    GetPrivateProfileStringA("Mirror", "FillRadius", "1.5", v, sizeof(v), ini); g_wmFill = (float)atof(v);
    if (!(g_wmEps > 0.0f && g_wmEps < 10.0f)) g_wmEps = 0.25f;
    if (!(g_wmFill > 0.0f && g_wmFill < 50.0f)) g_wmFill = 1.5f;
    WmSet(GetPrivateProfileIntA("Mirror", "Enabled", 0, ini) != 0);
}

static bool WmAssetAllowed(const char* asset) {
    if (!asset || !asset[0]) return false;
    char list[256]; strncpy_s(list, g_wmAssets, _TRUNCATE);
    char* ctx = nullptr;
    for (char* t = strtok_s(list, ", ", &ctx); t; t = strtok_s(nullptr, ", ", &ctx))
        if (!_stricmp(t, asset)) return true;
    return false;
}

// The ini override: Plane_<asset>=<x|y|z>,<offset>,<+|->. The sign names the
// MODELLED half (the side the normal points into).
static bool WmPlaneOverride(const char* asset, float n[3], float c[3]) {
    if (!g_wmIni[0]) return false;
    char key[96], v[64];
    _snprintf(key, sizeof(key), "Plane_%s", asset); key[sizeof(key) - 1] = 0;
    if (!GetPrivateProfileStringA("Mirror", key, "", v, sizeof(v), g_wmIni) || !v[0]) return false;
    char axis = 0, sign = '+'; float off = 0;
    if (sscanf(v, " %c , %f , %c", &axis, &off, &sign) < 2) return false;
    const int a = axis == 'x' || axis == 'X' ? 0 : axis == 'y' || axis == 'Y' ? 1 : axis == 'z' || axis == 'Z' ? 2 : -1;
    if (a < 0) return false;
    n[0] = n[1] = n[2] = 0; c[0] = c[1] = c[2] = 0;
    n[a] = sign == '-' ? -1.0f : 1.0f; c[a] = off;
    return true;
}

// Read the draw's geometry, find the plane, build our index buffer. Every
// refusal is logged with its numbers and leaves state=2 (no retry until a
// rebuild or a key change).
static void WmBuild(IDirect3DDevice9* dev, WmEntry* e, INT baseVertex, UINT minIndex, UINT numVerts,
                    UINT startIndex, UINT primCount) {
    e->state = 2;
    auto refuse = [&](const char* why) {
        InterlockedIncrement(&g_wmRefused);
        Log("mirror/build: '%s' REFUSED - %s (verts %u prims %u). The weapon draws unmirrored.", e->asset, why, numVerts, primCount);
    };
    if (numVerts < 16 || numVerts > 200000 || primCount < 4 || primCount > 400000) { refuse("geometry size out of range"); return; }
    // The position element, from the declaration this draw is using.
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH]; UINT en = MAXD3DDECLLENGTH;
    IDirect3DVertexDeclaration9* decl = nullptr;
    if (FAILED(dev->GetVertexDeclaration(&decl)) || !decl) { refuse("no vertex declaration"); return; }
    const HRESULT dh = decl->GetDeclaration(el, &en); decl->Release();
    if (FAILED(dh) || en > MAXD3DDECLLENGTH) { refuse("declaration unreadable"); return; }
    int posOff = -1;
    for (UINT i = 0; i < en; ++i)
        if (el[i].Type != D3DDECLTYPE_UNUSED && el[i].Usage == D3DDECLUSAGE_POSITION && el[i].UsageIndex == 0) {
            if (el[i].Stream != 0 || el[i].Type != D3DDECLTYPE_FLOAT3) { refuse("POSITION is not FLOAT3 in stream 0"); return; }
            posOff = el[i].Offset;
        }
    if (posOff < 0 || (UINT)posOff + 12 > e->stride) { refuse("no POSITION element"); return; }

    IDirect3DVertexBuffer9* vb = nullptr; IDirect3DIndexBuffer9* ib = nullptr; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vb, &off, &stride)) || !vb) { refuse("no stream 0"); return; }
    if (FAILED(dev->GetIndices(&ib)) || !ib) { vb->Release(); refuse("no index buffer"); return; }
    std::vector<uint32_t> idx; std::vector<float> pos;
    const char* why = nullptr;
    do {
        D3DVERTEXBUFFER_DESC vd; D3DINDEXBUFFER_DESC id;
        if (FAILED(vb->GetDesc(&vd)) || FAILED(ib->GetDesc(&id))) { why = "buffer desc unreadable"; break; }
        if (id.Format != D3DFMT_INDEX16 && id.Format != D3DFMT_INDEX32) { why = "index format"; break; }
        const UINT is = id.Format == D3DFMT_INDEX16 ? 2 : 4;
        const uint64_t io = (uint64_t)startIndex * is, il = (uint64_t)primCount * 3 * is;
        const int64_t first = (int64_t)baseVertex + minIndex;
        if (first < 0 || stride != e->stride || io + il > id.Size) { why = "index range or stride mismatch"; break; }
        const uint64_t vo = (uint64_t)off + (uint64_t)first * stride, vl = (uint64_t)numVerts * stride;
        if (vo + vl > vd.Size) { why = "vertex range outside the buffer"; break; }
        void* data = nullptr;
        if (FAILED(ib->Lock((UINT)io, (UINT)il, &data, (id.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY)) || !data) { why = "index buffer would not lock"; break; }
        idx.resize((size_t)primCount * 3);
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = is == 2 ? ((uint16_t*)data)[i] : ((uint32_t*)data)[i];
        ib->Unlock();
        for (uint32_t x : idx) if (x < minIndex || x - minIndex >= numVerts) { why = "an index outside [minIndex, minIndex+numVerts)"; break; }
        if (why) break;
        if (FAILED(vb->Lock((UINT)vo, (UINT)vl, &data, (vd.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY)) || !data) { why = "vertex buffer would not lock"; break; }
        pos.resize((size_t)numVerts * 3);
        for (UINT i = 0; i < numVerts; ++i) memcpy(&pos[i * 3], (const uint8_t*)data + (size_t)i * stride + posOff, 12);
        vb->Unlock();
    } while (false);
    ib->Release(); vb->Release();
    if (why) { refuse(why); return; }
    for (float f : pos) if (!std::isfinite(f)) { refuse("a non-finite position"); return; }

    // Used vertices, bbox.
    std::vector<uint8_t> used(numVerts, 0);
    for (uint32_t x : idx) used[x - minIndex] = 1;
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }; UINT nUsed = 0;
    for (UINT i = 0; i < numVerts; ++i) if (used[i]) {
        ++nUsed;
        for (int a = 0; a < 3; ++a) { lo[a] = (std::min)(lo[a], pos[i*3+a]); hi[a] = (std::max)(hi[a], pos[i*3+a]); }
    }
    const float ext[3] = { hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] };
    const int barrel = ext[0] >= ext[1] && ext[0] >= ext[2] ? 0 : ext[1] >= ext[2] ? 1 : 2;

    // The plane: the ini override, else the cut face (the densest face slab).
    UINT faceCnt[3][2] = {};
    for (UINT i = 0; i < numVerts; ++i) if (used[i])
        for (int a = 0; a < 3; ++a) {
            const float tol = (std::max)(0.1f, 0.002f * ext[a]);
            if (pos[i*3+a] - lo[a] <= tol) ++faceCnt[a][0];
            if (hi[a] - pos[i*3+a] <= tol) ++faceCnt[a][1];
        }
    e->measured = !WmPlaneOverride(e->asset, e->n, e->c);
    int bestA = -1, bestF = 0; UINT bestCnt = 0, bestOpp = 0;
    if (e->measured) {
        for (int a = 0; a < 3; ++a) if (a != barrel)
            for (int f = 0; f < 2; ++f)
                if (faceCnt[a][f] > bestCnt) { bestCnt = faceCnt[a][f]; bestOpp = faceCnt[a][1 - f]; bestA = a; bestF = f; }
        char msg[200];
        if (bestA < 0 || bestCnt < nUsed * 3 / 100 || bestCnt < 3 * (bestOpp + 1)) {
            _snprintf(msg, sizeof(msg), "no cut face (best %c%s %u of %u used verts, opposite %u; need >= 3%% and >= 3x) - set [Mirror] Plane_%s",
                      bestA < 0 ? '?' : "xyz"[bestA], bestF ? "max" : "min", bestCnt, nUsed, bestOpp, e->asset);
            msg[sizeof(msg) - 1] = 0; refuse(msg); return;
        }
        e->n[0] = e->n[1] = e->n[2] = 0; e->c[0] = e->c[1] = e->c[2] = 0;
        e->n[bestA] = bestF ? -1.0f : 1.0f;            // into the modelled half
        e->c[bestA] = bestF ? hi[bestA] : lo[bestA];
    }
    float S[12]; dvr::hf::reflection_3x4(e->n, e->c, S);
    auto dist = [&](const float* p) { return e->n[0]*(p[0]-e->c[0]) + e->n[1]*(p[1]-e->c[1]) + e->n[2]*(p[2]-e->c[2]); };

    // What already exists on the unmodelled side, hashed by FillRadius cells.
    const float cell = g_wmFill;
    auto key = [&](const float* p) -> long long {
        const long long x = (long long)floorf(p[0] / cell), y = (long long)floorf(p[1] / cell), z = (long long)floorf(p[2] / cell);
        return ((x & 0x1FFFFF) << 42) | ((y & 0x1FFFFF) << 21) | (z & 0x1FFFFF);
    };
    std::unordered_map<long long, std::vector<UINT>> other;
    UINT nOther = 0;
    for (UINT i = 0; i < numVerts; ++i) if (used[i] && dist(&pos[i*3]) < -g_wmEps) { other[key(&pos[i*3])].push_back(i); ++nOther; }
    auto occupied = [&](const float* m) {
        for (int dx = -1; dx <= 1; ++dx) for (int dy = -1; dy <= 1; ++dy) for (int dz = -1; dz <= 1; ++dz) {
            const float q[3] = { m[0] + dx * cell, m[1] + dy * cell, m[2] + dz * cell };
            auto it = other.find(key(q));
            if (it == other.end()) continue;
            for (UINT i : it->second) {
                const float ddx = pos[i*3] - m[0], ddy = pos[i*3+1] - m[1], ddz = pos[i*3+2] - m[2];
                if (ddx*ddx + ddy*ddy + ddz*ddz <= cell * cell) return true;
            }
        }
        return false;
    };
    std::vector<uint32_t> kept; kept.reserve(idx.size());
    UINT skipSide = 0, skipPlane = 0, skipCovered = 0;
    uint32_t maxIdx = 0;
    for (size_t t = 0; t + 2 < idx.size(); t += 3) {
        const float* p0 = &pos[(idx[t] - minIndex) * 3]; const float* p1 = &pos[(idx[t+1] - minIndex) * 3];
        const float* p2 = &pos[(idx[t+2] - minIndex) * 3];
        const float d0 = dist(p0), d1 = dist(p1), d2 = dist(p2);
        if (d0 < -g_wmEps || d1 < -g_wmEps || d2 < -g_wmEps) { ++skipSide; continue; }
        if ((std::max)(d0, (std::max)(d1, d2)) <= g_wmEps) { ++skipPlane; continue; }
        const float cen[3] = { (p0[0]+p1[0]+p2[0]) / 3, (p0[1]+p1[1]+p2[1]) / 3, (p0[2]+p1[2]+p2[2]) / 3 };
        float m[3];
        for (int i = 0; i < 3; ++i) m[i] = S[i*4]*cen[0] + S[i*4+1]*cen[1] + S[i*4+2]*cen[2] + S[i*4+3];
        if (nOther && occupied(m)) { ++skipCovered; continue; }
        kept.push_back(idx[t]); kept.push_back(idx[t+1]); kept.push_back(idx[t+2]);
        maxIdx = (std::max)(maxIdx, (std::max)(idx[t], (std::max)(idx[t+1], idx[t+2])));
    }
    const UINT keptPrims = (UINT)(kept.size() / 3);
    Log("mirror/build: '%s' verts %u used %u prims %u | bbox (%.1f %.1f %.1f)-(%.1f %.1f %.1f) | barrel axis %c | "
        "plane n=(%.0f %.0f %.0f) through %c=%.2f %s%s | faces min/max x %u/%u y %u/%u z %u/%u | kept %u of %u "
        "(skipped: other side %u, on plane %u, already modelled %u; %u verts on the other side) Eps %.2f Fill %.2f | "
        "normal maps on the copy light from the mirrored side (cosmetic, accepted)",
        e->asset, numVerts, nUsed, primCount, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], "xyz"[barrel],
        e->n[0], e->n[1], e->n[2], "xyz"[e->n[0] != 0 ? 0 : e->n[1] != 0 ? 1 : 2],
        e->n[0] != 0 ? e->c[0] : e->n[1] != 0 ? e->c[1] : e->c[2],
        e->measured ? "MEASURED" : "from [Mirror] Plane_ (NOT measured)",
        e->measured ? "" : "", faceCnt[0][0], faceCnt[0][1], faceCnt[1][0], faceCnt[1][1], faceCnt[2][0], faceCnt[2][1],
        keptPrims, primCount, skipSide, skipPlane, skipCovered, nOther, g_wmEps, g_wmFill);
    if (!keptPrims) { refuse("nothing to fill (0 triangles kept) - the plane may be wrong"); return; }

    const bool wide = maxIdx > 0xFFFF;
    const UINT isz = wide ? 4 : 2, bytes = (UINT)kept.size() * isz;
    IDirect3DIndexBuffer9* ours = nullptr;
    if (FAILED(dev->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, wide ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                                      D3DPOOL_MANAGED, &ours, NULL)) || !ours) { refuse("our index buffer could not be created"); return; }
    void* p = nullptr;
    if (FAILED(ours->Lock(0, bytes, &p, 0)) || !p) { ours->Release(); refuse("our index buffer would not lock"); return; }
    for (size_t i = 0; i < kept.size(); ++i) { if (wide) ((uint32_t*)p)[i] = kept[i]; else ((uint16_t*)p)[i] = (uint16_t)kept[i]; }
    ours->Unlock();
    e->ours = ours; e->prims = keptPrims; e->dev = dev; e->state = 1;
}

// Called inside the caller's patched scope, after a successful placed draw.
static void WmDraw(IDirect3DDevice9* dev, const WaMesh* w, const float* source, UINT boneReg, UINT regs,
                   const dvr::hf::Xform& delta, D3DPRIMITIVETYPE type, INT baseVertex, UINT minIndex,
                   UINT numVerts, UINT startIndex, UINT primCount) {
    if (!g_wmOn || !w || type != D3DPT_TRIANGLELIST || !WmAssetAllowed(w->asset) || regs == 0 || regs > WA_MAX_REGS) return;
    IDirect3DVertexBuffer9* vbo = nullptr; IDirect3DIndexBuffer9* ibo = nullptr; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vbo, &off, &stride)) || !vbo) return;
    void* vb = vbo; vbo->Release();
    if (FAILED(dev->GetIndices(&ibo)) || !ibo) return;
    void* ib = ibo;   // the reference is kept for the restore below, released after it

    const unsigned long long now = GetTickCount64();
    WmEntry* e = nullptr;
    for (int i = 0; i < g_wmN; ++i) {
        WmEntry& k = g_wm[i];
        if (k.vb == vb && k.ib == ib && k.startIndex == startIndex && k.primCount == primCount && k.numVerts == numVerts &&
            k.minIndex == minIndex && k.baseVertex == baseVertex && k.stride == stride && !strcmp(k.asset, w->asset)) { e = &k; break; }
    }
    if (e && e->state == 1 && e->dev != dev) { WmReleaseEntry(e); e = nullptr; }
    if (!e) {
        int slot = g_wmN < kWmMax ? g_wmN++ : 0;
        if (slot == 0 && g_wmN == kWmMax)
            for (int i = 1; i < kWmMax; ++i) if (g_wm[i].lastUse < g_wm[slot].lastUse) slot = i;
        e = &g_wm[slot]; WmReleaseEntry(e);
        e->vb = vb; e->ib = ib; e->startIndex = startIndex; e->primCount = primCount; e->numVerts = numVerts;
        e->minIndex = minIndex; e->baseVertex = baseVertex; e->stride = stride;
        strncpy_s(e->asset, w->asset, _TRUNCATE);
        WmBuild(dev, e, baseVertex, minIndex, numVerts, startIndex, primCount);
    }
    e->lastUse = now;
    if (e->state != 1 || !e->ours) { ibo->Release(); return; }

    static float mirrored[WA_MAX_REGS*4], patched[WA_MAX_REGS*4];
    float S[12]; dvr::hf::reflection_3x4(e->n, e->c, S);
    dvr::hf::mirror_palette_right(source, S, mirrored, regs);
    MpBuild(patched, mirrored, regs, &delta);
    bool ok = SUCCEEDED(dvr::frame::orig_set_vs_const(dev, boneReg, patched, regs));
    DWORD cull = D3DCULL_NONE;
    const bool haveCull = ok && SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE, &cull));
    if (haveCull && cull != D3DCULL_NONE) dev->SetRenderState(D3DRS_CULLMODE, cull == D3DCULL_CW ? D3DCULL_CCW : D3DCULL_CW);
    if (ok && SUCCEEDED(dev->SetIndices(e->ours))) {
        ok = SUCCEEDED(dvr::frame::orig_draw_indexed(dev, D3DPT_TRIANGLELIST, baseVertex, minIndex, numVerts, 0, e->prims));
        dev->SetIndices(ibo);
    } else ok = false;
    if (haveCull && cull != D3DCULL_NONE) dev->SetRenderState(D3DRS_CULLMODE, cull);
    ibo->Release();
    // The caller restores the game's palette after this returns.
    if (ok) { InterlockedIncrement(&g_wmDrawn); InterlockedIncrement(&e->drawn); }
    else { InterlockedIncrement(&g_wmFailed); InterlockedIncrement(&e->failed); }
    DVR_LOG_EVERY_MS(::dvr::log::Cat::hands, ::dvr::log::Level::Info, 5000,
        "mirror/beat: drawn %ld failed %ld refused builds %ld | last '%s' kept %u prims, native cull %lu (1 none 2 cw 3 ccw), %d built",
        g_wmDrawn, g_wmFailed, g_wmRefused, e->asset, e->prims, (unsigned long)cull, g_wmN);
}

static bool WmCommand(const char* args) {
    bool b = false;
    if (DvrOnOff(args, &b)) { WmSet(b); return true; }
    if (!strcmp(args, "rebuild")) { WmReleaseAll("rebuild by request"); return true; }
    if (!strncmp(args, "plane ", 6)) {
        char asset[64], axis[4], sign[4] = "+"; float off = 0;
        if (sscanf(args + 6, "%63s %3s %f %3s", asset, axis, &off, sign) >= 3 && g_wmIni[0]) {
            char key[96], v[48];
            _snprintf(key, sizeof(key), "Plane_%s", asset); key[sizeof(key) - 1] = 0;
            _snprintf(v, sizeof(v), "%c,%.3f,%c", axis[0], off, sign[0]); v[sizeof(v) - 1] = 0;
            WritePrivateProfileStringA("Mirror", key, v, g_wmIni);
            Log("mirror: [Mirror] %s=%s written; rebuilding", key, v);
            WmReleaseAll("plane override changed");
        } else Log("mirror: plane <asset> <x|y|z> <offset> [+|-]");
        return true;
    }
    Log("mirror: %s | drawn %ld failed %ld refused %ld | %d built | assets %s | usage: mirror on|off|rebuild|plane <asset> <axis> <offset> [sign]",
        g_wmOn ? "ON" : "off", g_wmDrawn, g_wmFailed, g_wmRefused, g_wmN, g_wmAssets);
    for (int i = 0; i < g_wmN; ++i)
        Log("mirror:   [%d] '%s' state %d kept %u drawn %ld failed %ld plane n=(%.0f %.0f %.0f) c=(%.2f %.2f %.2f) %s", i,
            g_wm[i].asset, g_wm[i].state, g_wm[i].prims, g_wm[i].drawn, g_wm[i].failed, g_wm[i].n[0], g_wm[i].n[1], g_wm[i].n[2],
            g_wm[i].c[0], g_wm[i].c[1], g_wm[i].c[2], g_wm[i].measured ? "measured" : "override");
    return true;
}
