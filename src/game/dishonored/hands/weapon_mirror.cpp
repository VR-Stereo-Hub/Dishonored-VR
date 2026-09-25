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
    IDirect3DIndexBuffer9* caps;   // hole caps: fans over open boundary loops, existing vertices only
    UINT capPrims;
    float n[3], c[3];
    bool measured;
    unsigned long long lastUse;
    LONG drawn, failed;
};
constexpr int kWmMax = 8;
WmEntry g_wm[kWmMax];
int g_wmN = 0;
bool g_wmOn = false;
float g_wmEps = 0.25f, g_wmFill = 1.5f, g_wmCoverTol = 0.3f;
float g_wmStraddle = 2.0f;   // [Mirror] Straddle: plane-crossing triangles mostly on the modelled side are mirrored too (uu past the plane)
float g_wmBias = 0.0f;       // [Mirror] DepthBias: push the copy back so a real surface always wins (1e-4 of the depth range per unit)
LONG g_wmBiasDraws = 0;
bool g_wmBack = false;   // [Mirror] BackFaces: redraw the weapon with its back faces (fills one-sided holes)
bool g_wmCaps = true;        // [Mirror] Caps: close open holes (where the hand covered the model) with fans
bool g_wmBodyOnly = true;    // [Mirror] BodyBoneOnly (VR-225): copy and cap only geometry rigid on the body bone
LONG g_wmBackDrawn = 0, g_wmCapDrawn = 0;
char g_wmAssets[256] = "Wpn_PlyGunElite,crossbow_01";
char g_wmIni[MAX_PATH] = "";
LONG g_wmDrawn = 0, g_wmFailed = 0, g_wmRefused = 0;
}

static void WmReleaseEntry(WmEntry* e) {
    if (e->ours) { e->ours->Release(); e->ours = nullptr; }
    if (e->caps) { e->caps->Release(); e->caps = nullptr; }
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
    Log("mirror: %s (VR-138; assets %s, Eps %.2f uu, FillRadius %.2f uu, back faces %s, hole caps %s, body bone only %s)",
        on ? "ON" : "off", g_wmAssets, g_wmEps, g_wmFill, g_wmBack ? "ON" : "off", g_wmCaps ? "ON" : "off",
        g_wmBodyOnly ? "ON" : "off");
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
    g_wmBack = GetPrivateProfileIntA("Mirror", "BackFaces", 0, ini) != 0;
    g_wmCaps = GetPrivateProfileIntA("Mirror", "Caps", 1, ini) != 0;
    g_wmBodyOnly = GetPrivateProfileIntA("Mirror", "BodyBoneOnly", 1, ini) != 0;
    GetPrivateProfileStringA("Mirror", "CoverTol", "0.3", v, sizeof(v), ini); g_wmCoverTol = (float)atof(v);
    GetPrivateProfileStringA("Mirror", "Straddle", "2.0", v, sizeof(v), ini); g_wmStraddle = (float)atof(v);
    if (!(g_wmStraddle >= 0.0f && g_wmStraddle < 20.0f)) g_wmStraddle = 2.0f;
    GetPrivateProfileStringA("Mirror", "DepthBias", "0", v, sizeof(v), ini); g_wmBias = (float)atof(v);
    if (!(g_wmBias >= 0.0f && g_wmBias <= 100.0f)) g_wmBias = 0.0f;
    if (!(g_wmCoverTol > 0.0f && g_wmCoverTol < 5.0f)) g_wmCoverTol = 0.3f;
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

static IDirect3DIndexBuffer9* WmMakeIb(IDirect3DDevice9* dev, const std::vector<uint32_t>& v) {
    uint32_t mx = 0; for (uint32_t x : v) mx = (std::max)(mx, x);
    const bool wide = mx > 0xFFFF; const UINT isz = wide ? 4 : 2, bytes = (UINT)v.size() * isz;
    IDirect3DIndexBuffer9* ib = nullptr; void* p = nullptr;
    if (v.empty() || FAILED(dev->CreateIndexBuffer(bytes, D3DUSAGE_WRITEONLY, wide ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                                                   D3DPOOL_MANAGED, &ib, NULL)) || !ib) return nullptr;
    if (FAILED(ib->Lock(0, bytes, &p, 0)) || !p) { ib->Release(); return nullptr; }
    for (size_t i = 0; i < v.size(); ++i) { if (wide) ((uint32_t*)p)[i] = v[i]; else ((uint16_t*)p)[i] = (uint16_t)v[i]; }
    ib->Unlock();
    return ib;
}

// Hole caps (installed473 report: the pistol near the handle and the crossbow's
// lower left stay open, and the crossbow is already left/right symmetric,
// x 0.976, so a mirror has nothing to add there). A first-person model is not
// modelled where the hand covered it, so with the hand cut away those regions
// may be OPEN: a loop of edges each used by one triangle. Each loop is closed
// with a fan over its own existing vertices (no new vertices, so the skinning
// and palette are the weapon's own), drawn with culling off. Positions are
// welded first so a UV seam is not mistaken for a hole. Refused per loop, the
// reason counted: longer than 96 edges or wider than 45% of the model (a
// silhouette, not a hole), and a SHEET outline - an open flat part (a bow limb
// modelled as one layer) whose adjacent triangles lie INSIDE the loop, where a
// cap would lay a second surface on the sheet and z-fight.
static void WmBuildCaps(IDirect3DDevice9* dev, WmEntry* e, const std::vector<uint32_t>& idx, const std::vector<float>& pos,
                        UINT minIndex, const float ext[3], const std::vector<uint8_t>& body) {
    if (!g_wmCaps) return;
    const float weld = 0.02f;
    std::unordered_map<long long, uint32_t> weldMap; std::vector<uint32_t> wid(pos.size() / 3, 0xFFFFFFFFu), rep;
    auto wkey = [&](const float* p) {
        const long long x = (long long)floorf(p[0] / weld + 0.5f), y = (long long)floorf(p[1] / weld + 0.5f),
                        z = (long long)floorf(p[2] / weld + 0.5f);
        return ((x & 0x1FFFFF) << 42) | ((y & 0x1FFFFF) << 21) | (z & 0x1FFFFF);
    };
    for (uint32_t x : idx) {
        const uint32_t v = x - minIndex;
        if (wid[v] != 0xFFFFFFFFu) continue;
        auto it = weldMap.emplace(wkey(&pos[v * 3]), (uint32_t)rep.size());
        if (it.second) rep.push_back(v);
        wid[v] = it.first->second;
    }
    std::unordered_map<unsigned long long, int> uses;
    auto ukey = [](uint32_t a, uint32_t b) { return a < b ? ((unsigned long long)a << 32) | b : ((unsigned long long)b << 32) | a; };
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const uint32_t a = wid[idx[t + k] - minIndex], b = wid[idx[t + (k + 1) % 3] - minIndex];
            if (a != b) ++uses[ukey(a, b)];
        }
    // A boundary edge a->b in its triangle's winding; the triangle's third corner rides along.
    std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> next;
    std::unordered_map<uint32_t, uint32_t> rawOf;
    UINT boundary = 0, branchy = 0;
    for (size_t t = 0; t + 2 < idx.size(); t += 3)
        for (int k = 0; k < 3; ++k) {
            const uint32_t ra = idx[t + k], rb = idx[t + (k + 1) % 3], rc = idx[t + (k + 2) % 3];
            const uint32_t a = wid[ra - minIndex], b = wid[rb - minIndex];
            if (a == b || uses[ukey(a, b)] != 1) continue;
            ++boundary;
            if (!next.emplace(a, std::make_pair(b, rc)).second) ++branchy;
            rawOf.emplace(a, ra); rawOf.emplace(b, rb);
        }
    const float maxExt = (std::max)(ext[0], (std::max)(ext[1], ext[2]));
    std::vector<uint32_t> out; std::unordered_map<uint32_t, bool> done;
    UINT loops = 0, capped = 0, tooLong = 0, tooWide = 0, sheet = 0, open = 0, moving = 0;
    for (auto& kv : next) {
        if (done.count(kv.first)) continue;
        std::vector<uint32_t> loop, thirds; uint32_t cur = kv.first; bool closed = false;
        while (loop.size() <= 97) {
            if (done.count(cur)) { closed = cur == kv.first; break; }
            done[cur] = true; loop.push_back(cur);
            auto it = next.find(cur);
            if (it == next.end()) break;
            thirds.push_back(it->second.second);
            cur = it->second.first;
        }
        ++loops;
        if (!closed) { ++open; continue; }
        if (loop.size() < 3) continue;
        if (loop.size() > 96) { ++tooLong; continue; }
        // VR-225: a fan over existing vertices is only a flat cap while those
        // vertices keep their reference-pose layout. A loop touching a bone that
        // moves on its own (the bow's limbs and string when it fires, a part the
        // game collapses when it is empty) stretches its fan into a sheet.
        if (!body.empty()) {
            bool rigid = true;
            for (uint32_t w : loop) if (!body[rawOf[w] - minIndex]) { rigid = false; break; }
            if (!rigid) { ++moving; continue; }
        }
        float c[3] = {}, n[3] = {}, dia = 0;
        for (uint32_t w : loop) for (int a = 0; a < 3; ++a) c[a] += pos[rep[w] * 3 + a] / (float)loop.size();
        for (size_t i = 0; i < loop.size(); ++i) {   // Newell normal and diameter
            const float* p = &pos[rep[loop[i]] * 3]; const float* q = &pos[rep[loop[(i + 1) % loop.size()]] * 3];
            n[0] += (p[1] - q[1]) * (p[2] + q[2]); n[1] += (p[2] - q[2]) * (p[0] + q[0]); n[2] += (p[0] - q[0]) * (p[1] + q[1]);
            const float dx = p[0] - c[0], dy = p[1] - c[1], dz = p[2] - c[2];
            dia = (std::max)(dia, 2 * sqrtf(dx * dx + dy * dy + dz * dz));
        }
        if (dia > 0.45f * maxExt) { ++tooWide; continue; }
        const float nlen = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (nlen > 1e-6f) {
            const float nn[3] = { n[0] / nlen, n[1] / nlen, n[2] / nlen };
            const float ax[3] = { fabsf(nn[0]) < 0.9f ? 1.f : 0.f, fabsf(nn[0]) < 0.9f ? 0.f : 1.f, 0.f };
            float u[3] = { ax[1] * nn[2] - ax[2] * nn[1], ax[2] * nn[0] - ax[0] * nn[2], ax[0] * nn[1] - ax[1] * nn[0] };
            const float ul = sqrtf(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]); for (float& x : u) x /= ul;
            const float v[3] = { nn[1] * u[2] - nn[2] * u[1], nn[2] * u[0] - nn[0] * u[2], nn[0] * u[1] - nn[1] * u[0] };
            auto proj = [&](const float* p, float& x, float& y, float& d) {
                const float r[3] = { p[0] - c[0], p[1] - c[1], p[2] - c[2] };
                x = r[0] * u[0] + r[1] * u[1] + r[2] * u[2]; y = r[0] * v[0] + r[1] * v[1] + r[2] * v[2];
                d = fabsf(r[0] * nn[0] + r[1] * nn[1] + r[2] * nn[2]);
            };
            std::vector<float> px(loop.size()), py(loop.size()); float dd;
            for (size_t i = 0; i < loop.size(); ++i) proj(&pos[rep[loop[i]] * 3], px[i], py[i], dd);
            UINT flatInside = 0;
            for (uint32_t r : thirds) {
                float qx, qy, qd; proj(&pos[(r - minIndex) * 3], qx, qy, qd);
                bool in = false;
                for (size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++)
                    if ((py[i] > qy) != (py[j] > qy) && qx < (px[j] - px[i]) * (qy - py[i]) / (py[j] - py[i]) + px[i]) in = !in;
                if (in && qd < 0.1f * dia + 0.05f) ++flatInside;
            }
            if (flatInside * 2 > thirds.size()) { ++sheet; continue; }
        }
        for (size_t i = 1; i + 1 < loop.size(); ++i) {
            out.push_back(rawOf[loop[0]]); out.push_back(rawOf[loop[i]]); out.push_back(rawOf[loop[i + 1]]);
        }
        ++capped;
    }
    e->caps = WmMakeIb(dev, out); e->capPrims = e->caps ? (UINT)(out.size() / 3) : 0;
    Log("mirror/caps: '%s' boundary edges %u (welded at %.2f uu; %u branch points) | loops %u: capped %u (%u triangles), "
        "refused: open chain %u, longer than 96 edges %u, wider than 45%% of the model %u, sheet outline %u, on a moving bone %u%s",
        e->asset, boundary, weld, branchy, loops, capped, e->capPrims, open, tooLong, tooWide, sheet, moving,
        boundary ? "" : " - the model is CLOSED: the missing areas are not holes in this mesh");
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
    // VR-225: the skin, so the copy and the caps can stay on the body bone.
    MsElem wt = {}, bi = {};
    for (UINT i = 0; i < en; ++i) {
        if (el[i].Type == D3DDECLTYPE_UNUSED || el[i].Stream != 0) continue;
        if (el[i].Usage == D3DDECLUSAGE_BLENDWEIGHT && !wt.have)  wt = { (int)el[i].Offset, (int)el[i].Type, 1 };
        if (el[i].Usage == D3DDECLUSAGE_BLENDINDICES && !bi.have) bi = { (int)el[i].Offset, (int)el[i].Type, 1 };
    }
    const bool skinned = wt.have && bi.have && (UINT)wt.off + 4 <= e->stride && (UINT)bi.off + 4 <= e->stride;

    IDirect3DVertexBuffer9* vb = nullptr; IDirect3DIndexBuffer9* ib = nullptr; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vb, &off, &stride)) || !vb) { refuse("no stream 0"); return; }
    if (FAILED(dev->GetIndices(&ib)) || !ib) { vb->Release(); refuse("no index buffer"); return; }
    std::vector<uint32_t> idx; std::vector<float> pos;
    std::vector<int> rbone;   // the one bone a vertex is rigid on (weight >= 0.99), -1 blended, -2 unreadable
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
        if (skinned) {
            rbone.assign(numVerts, -1);
            for (UINT i = 0; i < numVerts; ++i) {
                const uint8_t* v = (const uint8_t*)data + (size_t)i * stride;
                float w4[4]; uint8_t b4[4];
                if (!MsReadWeights(v, &wt, w4) || !MsReadIndices(v, &bi, b4)) { rbone[i] = -2; continue; }
                for (int j = 0; j < 4; ++j) if (w4[j] >= 0.99f) { rbone[i] = b4[j]; break; }
            }
        }
        vb->Unlock();
    } while (false);
    ib->Release(); vb->Release();
    if (why) { refuse(why); return; }
    for (float f : pos) if (!std::isfinite(f)) { refuse("a non-finite position"); return; }
    // A refused copy still draws its hole caps.
    auto refuseKeepCaps = [&](const char* w) { refuse(w); if (e->caps) { e->state = 1; e->prims = 0; e->dev = dev; } };

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
    // VR-225: the body bone. S reflects the reference pose BEFORE skinning, so a
    // copied triangle is still skinned by its ORIGINAL bones: a left-limb triangle
    // lands on the right side but swings about the left limb's pivot. While the
    // weapon holds its reference pose nothing shows; when the bow fires, and for
    // as long as it is empty, the limbs and string move and the copy lands as a
    // displaced piece on the mirrored side (the same holds for a cap's fan). The
    // body (the bone the most vertices are rigid on) does not move against the
    // weapon, so with [Mirror] BodyBoneOnly only geometry rigid on it is copied
    // or capped. Counterprediction in the log: 'moving bone' counts > 0 on the
    // crossbow; if they are 0 the stray piece is not a skinning artefact.
    std::vector<uint8_t> body;
    {
        std::unordered_map<int, UINT> census; UINT blended = 0, unread = 0;
        for (UINT i = 0; i < numVerts && !rbone.empty(); ++i) if (used[i]) {
            if (rbone[i] >= 0) ++census[rbone[i]]; else if (rbone[i] == -1) ++blended; else ++unread;
        }
        int bodyBone = -1; UINT bodyN = 0;
        for (auto& kv : census) if (kv.second > bodyN) { bodyN = kv.second; bodyBone = kv.first; }
        char list[256] = ""; size_t at = 0;
        for (auto& kv : census)
            if (at + 24 < sizeof(list)) at += _snprintf(list + at, sizeof(list) - at, "%s%d:%u", at ? " " : "", kv.first, kv.second);
        if (!skinned)
            Log("mirror/skin: '%s' no BLENDWEIGHT/BLENDINDICES in stream 0 - body-bone filter OFF for this weapon (copies and caps as before)", e->asset);
        else
            Log("mirror/skin: '%s' rigid vertices per bone {%s} | blended %u unreadable %u | body bone %d (%u of %u used) | filter %s",
                e->asset, list, blended, unread, bodyBone, bodyN, nUsed,
                g_wmBodyOnly ? "ON: only geometry rigid on the body bone is copied or capped" : "off ([Mirror] BodyBoneOnly=0)");
        if (skinned && g_wmBodyOnly && bodyBone >= 0) {
            body.assign(numVerts, 0);
            for (UINT i = 0; i < numVerts; ++i) body[i] = rbone[i] == bodyBone;
        }
    }
    WmBuildCaps(dev, e, idx, pos, minIndex, ext, body);

    // The plane: the ini override, else the SYMMETRY PLANE. Build464 refused
    // both weapons under the first rule (a cut face: 26 of 6330 pistol and 8 of
    // 1961 crossbow vertices on the densest face), so these meshes are not cut
    // open at a plane; they carry a symmetric core (barrel, grip, bow arms) and
    // miss detail on one side. The mirror plane is the one across which the most
    // vertices find a DIFFERENT vertex at their reflection. Vertices within
    // 0.5 uu of a candidate plane are left out of the score: they match
    // themselves and would make any dense slab look symmetric.
    UINT faceCnt[3][2] = {};
    for (UINT i = 0; i < numVerts; ++i) if (used[i])
        for (int a = 0; a < 3; ++a) {
            const float tol = (std::max)(0.1f, 0.002f * ext[a]);
            if (pos[i*3+a] - lo[a] <= tol) ++faceCnt[a][0];
            if (hi[a] - pos[i*3+a] <= tol) ++faceCnt[a][1];
        }
    e->measured = !WmPlaneOverride(e->asset, e->n, e->c);
    float bestScore[3] = { -1, -1, -1 }, bestC[3] = { 0, 0, 0 };
    UINT sideHi = 0, sideLo = 0;
    if (e->measured) {
        const float r = 0.5f;
        auto vkey = [&](float x, float y, float z) -> long long {
            const long long a = (long long)floorf(x / r), b = (long long)floorf(y / r), c = (long long)floorf(z / r);
            return ((a & 0x1FFFFF) << 42) | ((b & 0x1FFFFF) << 21) | (c & 0x1FFFFF);
        };
        std::unordered_map<long long, std::vector<UINT>> grid;
        std::vector<UINT> sample;
        for (UINT i = 0; i < numVerts; ++i) if (used[i]) grid[vkey(pos[i*3], pos[i*3+1], pos[i*3+2])].push_back(i);
        const UINT stepV = nUsed > 1500 ? nUsed / 1500 : 1; UINT seen = 0;
        for (UINT i = 0; i < numVerts; ++i) if (used[i] && (seen++ % stepV) == 0) sample.push_back(i);
        auto score = [&](int a, float c) -> float {
            UINT hit = 0, n = 0;
            for (UINT i : sample) {
                const float d = pos[i*3+a] - c;
                if (fabsf(d) < r) continue;
                float m[3] = { pos[i*3], pos[i*3+1], pos[i*3+2] }; m[a] = c - d;
                ++n;
                bool found = false;
                for (int dx = -1; dx <= 1 && !found; ++dx) for (int dy = -1; dy <= 1 && !found; ++dy) for (int dz = -1; dz <= 1 && !found; ++dz) {
                    auto it = grid.find(vkey(m[0] + dx * r, m[1] + dy * r, m[2] + dz * r));
                    if (it == grid.end()) continue;
                    for (UINT k : it->second) {
                        if (k == i) continue;
                        const float ex = pos[k*3] - m[0], ey = pos[k*3+1] - m[1], ez = pos[k*3+2] - m[2];
                        if (ex*ex + ey*ey + ez*ez <= r * r) { found = true; break; }
                    }
                }
                if (found) ++hit;
            }
            return n >= 32 ? (float)hit / (float)n : -1.0f;
        };
        for (int a = 0; a < 3; ++a) {
            if (ext[a] < 1.0f) continue;
            // coarse: 33 offsets across the middle 80% of the extent, then refine
            const float c0 = lo[a] + 0.1f * ext[a], span = 0.8f * ext[a], st = span / 32.0f;
            for (int k = 0; k <= 32; ++k) { const float c = c0 + k * st, sc = score(a, c); if (sc > bestScore[a]) { bestScore[a] = sc; bestC[a] = c; } }
            const float cc = bestC[a];
            for (int k = -8; k <= 8; ++k) { const float c = cc + k * st / 8.0f, sc = score(a, c); if (sc > bestScore[a]) { bestScore[a] = sc; bestC[a] = c; } }
        }
        int bestA = 0;
        for (int a = 1; a < 3; ++a) if (bestScore[a] > bestScore[bestA]) bestA = a;
        float second = -1; for (int a = 0; a < 3; ++a) if (a != bestA && bestScore[a] > second) second = bestScore[a];
        Log("mirror/plane: '%s' symmetry search (fraction of vertices whose reflection lands on another vertex, 0.5 uu): "
            "x %.3f at %.2f | y %.3f at %.2f | z %.3f at %.2f | bbox (%.1f %.1f %.1f)-(%.1f %.1f %.1f)",
            e->asset, bestScore[0], bestC[0], bestScore[1], bestC[1], bestScore[2], bestC[2], lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        char msg[200];
        if (bestScore[bestA] < 0.20f || bestScore[bestA] < 1.15f * second) {
            _snprintf(msg, sizeof(msg), "no clear symmetry plane (best %c %.3f, next %.3f; need >= 0.20 and >= 1.15x) - set [Mirror] Plane_%s",
                      "xyz"[bestA], bestScore[bestA], second, e->asset);
            msg[sizeof(msg) - 1] = 0; refuseKeepCaps(msg); return;
        }
        e->n[0] = e->n[1] = e->n[2] = 0; e->c[0] = e->c[1] = e->c[2] = 0;
        e->n[bestA] = 1.0f; e->c[bestA] = bestC[bestA];   // provisional; the modelled side is chosen below
    }
    // Triangles: area, unit normal, centroid. The winding convention is not
    // assumed (UE3 flips it): the sign that makes normals point OUT of the model
    // is measured, area-weighted, against the mesh centre.
    const size_t nTri = idx.size() / 3;
    std::vector<float> tn(nTri * 3), tc(nTri * 3), ta(nTri);
    double mc[3] = {}, mcw = 0, outward = 0;
    for (size_t t = 0; t < nTri; ++t) {
        const float* p0 = &pos[(idx[t*3] - minIndex) * 3]; const float* p1 = &pos[(idx[t*3+1] - minIndex) * 3];
        const float* p2 = &pos[(idx[t*3+2] - minIndex) * 3];
        const float u[3] = { p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2] }, v[3] = { p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2] };
        const float n[3] = { u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0] };
        const float len = sqrtf(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        ta[t] = 0.5f * len;
        for (int i = 0; i < 3; ++i) { tn[t*3+i] = len > 1e-12f ? n[i] / len : 0; tc[t*3+i] = (p0[i] + p1[i] + p2[i]) / 3; mc[i] += tc[t*3+i] * ta[t]; }
        mcw += ta[t];
    }
    for (double& x : mc) x = mcw > 0 ? x / mcw : 0;
    for (size_t t = 0; t < nTri; ++t)
        outward += ta[t] * ((tc[t*3]-mc[0])*tn[t*3] + (tc[t*3+1]-mc[1])*tn[t*3+1] + (tc[t*3+2]-mc[2])*tn[t*3+2]);
    const float sgn = outward >= 0 ? 1.0f : -1.0f;
    for (float& x : tn) x *= sgn;
    const int ax = e->n[0] != 0 ? 0 : e->n[1] != 0 ? 1 : 2;
    // Installed476: the crossbow's VERTICES are 97.6% symmetric, yet one whole
    // side draws nothing - its vertices exist (edges, the thickness of the other
    // side's plates) but no faces look out of that side. So the modelled side is
    // the one with more area FACING OUT of it, and coverage below asks for a
    // surface with the same facing, not merely a vertex nearby.
    double faceOut[2] = {};   // [0] the +axis side, [1] the -axis side
    for (size_t t = 0; t < nTri; ++t) {
        const float d = tc[t*3+ax] - e->c[ax], f = tn[t*3+ax];
        if (d > g_wmEps && f > 0.3f) faceOut[0] += ta[t];
        else if (d < -g_wmEps && f < -0.3f) faceOut[1] += ta[t];
    }
    if (e->measured) {
        e->n[ax] = faceOut[0] >= faceOut[1] ? 1.0f : -1.0f;
        for (UINT i = 0; i < numVerts; ++i) if (used[i]) {
            const float d = (pos[i*3+ax] - e->c[ax]) * e->n[ax];
            if (d > g_wmEps) ++sideHi; else if (d < -g_wmEps) ++sideLo;
        }
    }
    float S[12]; dvr::hf::reflection_3x4(e->n, e->c, S);
    auto dist = [&](const float* p) { return e->n[0]*(p[0]-e->c[0]) + e->n[1]*(p[1]-e->c[1]) + e->n[2]*(p[2]-e->c[2]); };

    // What already exists on the unmodelled side: every triangle touching it,
    // bucketed into FillRadius cells by its bounding box. Installed480 measured
    // coverage against SAMPLE POINTS within FillRadius (1.5 uu) and the pistol
    // lost areas it had in 476: a small raised part whose mirror lands within
    // 1.5 uu of any same-facing surface counted as modelled. Coverage is now the
    // true point-to-triangle distance, within [Mirror] CoverTol, on a triangle
    // facing the same way (normal dot > 0.5).
    const float cell = g_wmFill, tol = g_wmCoverTol;
    auto ckey = [&](long long x, long long y, long long z) { return ((x & 0x1FFFFF) << 42) | ((y & 0x1FFFFF) << 21) | (z & 0x1FFFFF); };
    std::unordered_map<long long, std::vector<UINT>> other;
    UINT nOther = 0;
    for (size_t t = 0; t < nTri; ++t) {
        const float* q[3] = { &pos[(idx[t*3] - minIndex) * 3], &pos[(idx[t*3+1] - minIndex) * 3], &pos[(idx[t*3+2] - minIndex) * 3] };
        if (!(dist(q[0]) < -g_wmEps || dist(q[1]) < -g_wmEps || dist(q[2]) < -g_wmEps)) continue;
        ++nOther;
        long long lo3[3], hi3[3];
        for (int i = 0; i < 3; ++i) {
            const float mn = (std::min)(q[0][i], (std::min)(q[1][i], q[2][i])) - tol, mx = (std::max)(q[0][i], (std::max)(q[1][i], q[2][i])) + tol;
            lo3[i] = (long long)floorf(mn / cell); hi3[i] = (long long)floorf(mx / cell);
        }
        if ((hi3[0]-lo3[0]+1) * (hi3[1]-lo3[1]+1) * (hi3[2]-lo3[2]+1) > 4096) continue;   // absurdly large: never on a weapon
        for (long long x = lo3[0]; x <= hi3[0]; ++x) for (long long y = lo3[1]; y <= hi3[1]; ++y) for (long long z = lo3[2]; z <= hi3[2]; ++z)
            other[ckey(x, y, z)].push_back((UINT)t);
    }
    // Squared distance from p to triangle (a, b, c): the closest-point regions test.
    auto triDist2 = [](const float* p, const float* a, const float* b, const float* c) {
        auto sub = [](const float* x, const float* y, float* o) { o[0] = x[0]-y[0]; o[1] = x[1]-y[1]; o[2] = x[2]-y[2]; };
        auto dot = [](const float* x, const float* y) { return x[0]*y[0] + x[1]*y[1] + x[2]*y[2]; };
        float ab[3], ac[3], ap[3], bp[3], cp[3], r[3];
        sub(b, a, ab); sub(c, a, ac); sub(p, a, ap);
        const float d1 = dot(ab, ap), d2 = dot(ac, ap);
        auto at = [&](const float* x) { sub(p, x, r); return dot(r, r); };
        if (d1 <= 0 && d2 <= 0) return at(a);
        sub(p, b, bp); const float d3 = dot(ab, bp), d4 = dot(ac, bp);
        if (d3 >= 0 && d4 <= d3) return at(b);
        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0 && d1 >= 0 && d3 <= 0) { const float v = d1 / (d1 - d3); const float x[3] = { a[0]+v*ab[0], a[1]+v*ab[1], a[2]+v*ab[2] }; return at(x); }
        sub(p, c, cp); const float d5 = dot(ab, cp), d6 = dot(ac, cp);
        if (d6 >= 0 && d5 <= d6) return at(c);
        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0 && d2 >= 0 && d6 <= 0) { const float w = d2 / (d2 - d6); const float x[3] = { a[0]+w*ac[0], a[1]+w*ac[1], a[2]+w*ac[2] }; return at(x); }
        const float va = d3 * d6 - d5 * d4;
        if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
            const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            const float x[3] = { b[0]+w*(c[0]-b[0]), b[1]+w*(c[1]-b[1]), b[2]+w*(c[2]-b[2]) }; return at(x);
        }
        const float den = 1.0f / (va + vb + vc), v = vb * den, w = vc * den;
        const float x[3] = { a[0]+ab[0]*v+ac[0]*w, a[1]+ab[1]*v+ac[1]*w, a[2]+ab[2]*v+ac[2]*w };
        return at(x);
    };
    auto covered = [&](const float* m, const float* rn) {
        auto it = other.find(ckey((long long)floorf(m[0] / cell), (long long)floorf(m[1] / cell), (long long)floorf(m[2] / cell)));
        if (it == other.end()) return false;
        for (UINT t : it->second) {
            const float* on = &tn[t * 3];
            if (on[0]*rn[0] + on[1]*rn[1] + on[2]*rn[2] <= 0.5f) continue;
            if (triDist2(m, &pos[(idx[t*3] - minIndex) * 3], &pos[(idx[t*3+1] - minIndex) * 3], &pos[(idx[t*3+2] - minIndex) * 3]) <= tol * tol)
                return true;
        }
        return false;
    };
    std::vector<uint32_t> kept; kept.reserve(idx.size());
    UINT skipSide = 0, skipPlane = 0, skipCovered = 0, straddled = 0, skipMoving = 0;
    uint32_t maxIdx = 0;
    for (size_t t = 0; t < nTri; ++t) {
        const float* p0 = &pos[(idx[t*3] - minIndex) * 3]; const float* p1 = &pos[(idx[t*3+1] - minIndex) * 3];
        const float* p2 = &pos[(idx[t*3+2] - minIndex) * 3];
        const float d0 = dist(p0), d1 = dist(p1), d2 = dist(p2);
        // Installed483: two gaps on the pistol barrel. A barrel is centred on the
        // plane, so its faces near the top and bottom CROSS it, and a triangle with
        // any corner past the plane was never mirrored. One that sits mostly on
        // the modelled side (reaching at most [Mirror] Straddle past the plane) is
        // mirrored now; where the copy overlaps its own original, the coverage test
        // below skips it (same surface, same facing) and DepthBias settles the rest.
        const float dMin = (std::min)(d0, (std::min)(d1, d2)), dMax = (std::max)(d0, (std::max)(d1, d2));
        if (dMin < -g_wmEps) {
            if (!(dMax > g_wmEps && -dMin <= g_wmStraddle && dMax > -dMin)) { ++skipSide; continue; }
            ++straddled;
        }
        if ((std::max)(d0, (std::max)(d1, d2)) <= g_wmEps) { ++skipPlane; continue; }
        if (!body.empty() && !(body[idx[t*3] - minIndex] && body[idx[t*3+1] - minIndex] && body[idx[t*3+2] - minIndex])) { ++skipMoving; continue; }
        float rn[3] = { tn[t*3], tn[t*3+1], tn[t*3+2] }; rn[ax] = -rn[ax];   // the copy's outward facing
        float m[3];
        for (int i = 0; i < 3; ++i) m[i] = S[i*4]*tc[t*3] + S[i*4+1]*tc[t*3+1] + S[i*4+2]*tc[t*3+2] + S[i*4+3];
        // Skip only when the mirrored centroid AND all three mirrored corners
        // land on existing surface FACING THE SAME WAY: really modelled already.
        if (nOther && covered(m, rn)) {
            bool all = true;
            for (const float* pv : { p0, p1, p2 }) {
                float mv[3];
                for (int i = 0; i < 3; ++i) mv[i] = S[i*4]*pv[0] + S[i*4+1]*pv[1] + S[i*4+2]*pv[2] + S[i*4+3];
                if (!covered(mv, rn)) { all = false; break; }
            }
            if (all) { ++skipCovered; continue; }
        }
        kept.push_back(idx[t*3]); kept.push_back(idx[t*3+1]); kept.push_back(idx[t*3+2]);
        maxIdx = (std::max)(maxIdx, (std::max)(idx[t*3], (std::max)(idx[t*3+1], idx[t*3+2])));
    }
    Log("mirror/straddle: '%s' %u plane-crossing triangle(s) considered (reaching <= %.2f uu past the plane, mostly on the modelled side)",
        e->asset, straddled, g_wmStraddle);
    Log("mirror/facing: '%s' outward sign %+.0f | area facing OUT of the +%c side %.1f, of the -%c side %.1f -> modelled side %c%c "
        "(the side with no outward faces is the one that draws empty; copies need a same-facing surface to be skipped)",
        e->asset, sgn, "xyz"[ax], faceOut[0], "xyz"[ax], faceOut[1], e->n[ax] > 0 ? '+' : '-', "xyz"[ax]);
    const UINT keptPrims = (UINT)(kept.size() / 3);
    Log("mirror/build: '%s' verts %u used %u prims %u | bbox (%.1f %.1f %.1f)-(%.1f %.1f %.1f) | barrel axis %c | "
        "plane n=(%.0f %.0f %.0f) through %c=%.2f %s%s | sides %u/%u | faces min/max x %u/%u y %u/%u z %u/%u | kept %u of %u "
        "(skipped: other side %u, on plane %u, already modelled %u, on a moving bone %u; %u triangles on the other side) Eps %.2f Fill %.2f CoverTol %.2f | "
        "normal maps on the copy light from the mirrored side (cosmetic, accepted)",
        e->asset, numVerts, nUsed, primCount, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], "xyz"[barrel],
        e->n[0], e->n[1], e->n[2], "xyz"[e->n[0] != 0 ? 0 : e->n[1] != 0 ? 1 : 2],
        e->n[0] != 0 ? e->c[0] : e->n[1] != 0 ? e->c[1] : e->c[2],
        e->measured ? "MEASURED" : "from [Mirror] Plane_ (NOT measured)",
        e->measured ? "" : "", sideHi, sideLo, faceCnt[0][0], faceCnt[0][1], faceCnt[1][0], faceCnt[1][1], faceCnt[2][0], faceCnt[2][1],
        keptPrims, primCount, skipSide, skipPlane, skipCovered, skipMoving, nOther, g_wmEps, g_wmFill, g_wmCoverTol);
    (void)maxIdx;
    if (!keptPrims) { refuseKeepCaps("nothing to fill (0 triangles kept) - the plane may be wrong"); return; }
    IDirect3DIndexBuffer9* ours = WmMakeIb(dev, kept);
    if (!ours) { refuseKeepCaps("our index buffer could not be created"); return; }
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
    DWORD cull = D3DCULL_NONE;
    const bool haveCull = SUCCEEDED(dev->GetRenderState(D3DRS_CULLMODE, &cull));
    const DWORD flipped = cull == D3DCULL_CW ? D3DCULL_CCW : D3DCULL_CW;
    // The back-face pass: the same draw, same (corrected) palette the caller
    // just drew with, cull flipped. On a closed surface the back faces sit
    // behind the front ones and fail the depth test; where the model is
    // one-sided (a hole seen from the unmodelled side) they show, filling it.
    if (g_wmBack && haveCull && cull != D3DCULL_NONE) {
        dev->SetRenderState(D3DRS_CULLMODE, flipped);
        if (SUCCEEDED(dvr::frame::orig_draw_indexed(dev, type, baseVertex, minIndex, numVerts, startIndex, primCount)))
            InterlockedIncrement(&g_wmBackDrawn);
        dev->SetRenderState(D3DRS_CULLMODE, cull);
    }
    // The hole caps: the weapon's own palette (the caller's, still bound), culling
    // off because a fan's winding follows the loop, not the outside.
    if (e->state == 1 && e->caps && e->capPrims) {
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        if (SUCCEEDED(dev->SetIndices(e->caps))) {
            if (SUCCEEDED(dvr::frame::orig_draw_indexed(dev, D3DPT_TRIANGLELIST, baseVertex, minIndex, numVerts, 0, e->capPrims)))
                InterlockedIncrement(&g_wmCapDrawn);
            dev->SetIndices(ibo);
        }
        if (haveCull) dev->SetRenderState(D3DRS_CULLMODE, cull);
    }
    if (e->state != 1 || !e->ours) {
        ibo->Release();
        DVR_LOG_EVERY_MS(::dvr::log::Cat::hands, ::dvr::log::Level::Info, 5000,
            "mirror/beat: '%s' copy not drawn (state %d); hole caps %u prims, cap passes %ld, back-face passes %ld",
            e->asset, e->state, e->capPrims, g_wmCapDrawn, g_wmBackDrawn);
        return;
    }

    static float mirrored[WA_MAX_REGS*4], patched[WA_MAX_REGS*4];
    float S[12]; dvr::hf::reflection_3x4(e->n, e->c, S);
    dvr::hf::mirror_palette_right(source, S, mirrored, regs);
    MpBuild(patched, mirrored, regs, &delta);
    bool ok = SUCCEEDED(dvr::frame::orig_set_vs_const(dev, boneReg, patched, regs));
    if (ok && haveCull && cull != D3DCULL_NONE) dev->SetRenderState(D3DRS_CULLMODE, flipped);
    // Installed483: the crossbow's top left flickered where a copy lands close to
    // a real surface. The copy is pushed back by DepthBias (scaled to the game's
    // own viewport depth range: the weapon draws into a compressed MinZ..MaxZ)
    // plus a slope term, so the original wins every near-coincident pixel.
    DWORD oldBias = 0, oldSlope = 0; bool biased = false;
    D3DVIEWPORT9 vp{};
    if (ok && g_wmBias > 0 && SUCCEEDED(dev->GetViewport(&vp)) &&
        SUCCEEDED(dev->GetRenderState(D3DRS_DEPTHBIAS, &oldBias)) && SUCCEEDED(dev->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, &oldSlope))) {
        const float range = vp.MaxZ - vp.MinZ > 0 ? vp.MaxZ - vp.MinZ : 1.0f;
        const float bias = g_wmBias * 1e-4f * range, slope = g_wmBias;
        dev->SetRenderState(D3DRS_DEPTHBIAS, *(const DWORD*)&bias);
        dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, *(const DWORD*)&slope);
        biased = true; InterlockedIncrement(&g_wmBiasDraws);
    }
    if (ok && SUCCEEDED(dev->SetIndices(e->ours))) {
        ok = SUCCEEDED(dvr::frame::orig_draw_indexed(dev, D3DPT_TRIANGLELIST, baseVertex, minIndex, numVerts, 0, e->prims));
        dev->SetIndices(ibo);
    } else ok = false;
    if (biased) { dev->SetRenderState(D3DRS_DEPTHBIAS, oldBias); dev->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, oldSlope); }
    if (haveCull && cull != D3DCULL_NONE) dev->SetRenderState(D3DRS_CULLMODE, cull);
    ibo->Release();
    // The caller restores the game's palette after this returns.
    if (ok) { InterlockedIncrement(&g_wmDrawn); InterlockedIncrement(&e->drawn); }
    else { InterlockedIncrement(&g_wmFailed); InterlockedIncrement(&e->failed); }
    DVR_LOG_EVERY_MS(::dvr::log::Cat::hands, ::dvr::log::Level::Info, 5000,
        "mirror/beat: drawn %ld failed %ld refused builds %ld back-face passes %ld cap passes %ld biased %ld (DepthBias %.2f, viewport depth %.4f..%.4f) | last '%s' kept %u prims, caps %u prims, native cull %lu (1 none 2 cw 3 ccw), %d built",
        g_wmDrawn, g_wmFailed, g_wmRefused, g_wmBackDrawn, g_wmCapDrawn, g_wmBiasDraws, g_wmBias, vp.MinZ, vp.MaxZ, e->asset, e->prims, e->capPrims, (unsigned long)cull, g_wmN);
}

static bool WmCommand(const char* args) {
    bool b = false;
    if (DvrOnOff(args, &b)) { WmSet(b); return true; }
    if (!strcmp(args, "rebuild")) { WmReleaseAll("rebuild by request"); return true; }
    if (!strncmp(args, "back ", 5) && DvrOnOff(args + 5, &b)) {
        g_wmBack = b; Log("mirror: back faces %s", b ? "ON" : "off");
        if (g_wmIni[0]) WritePrivateProfileStringA("Mirror", "BackFaces", b ? "1" : "0", g_wmIni);
        return true;
    }
    if (!strncmp(args, "bias ", 5)) {
        const float f = (float)atof(args + 5);
        if (f >= 0 && f <= 100) { g_wmBias = f; Log("mirror: depth bias %.2f (live)", f);
            char v[32]; _snprintf(v, sizeof(v), "%.2f", f); v[sizeof(v) - 1] = 0;
            if (g_wmIni[0]) WritePrivateProfileStringA("Mirror", "DepthBias", v, g_wmIni); }
        return true;
    }
    if (!strncmp(args, "caps ", 5) && DvrOnOff(args + 5, &b)) {
        g_wmCaps = b; Log("mirror: hole caps %s (rebuilding)", b ? "ON" : "off");
        if (g_wmIni[0]) WritePrivateProfileStringA("Mirror", "Caps", b ? "1" : "0", g_wmIni);
        WmReleaseAll("caps toggled");
        return true;
    }
    if (!strncmp(args, "body ", 5) && DvrOnOff(args + 5, &b)) {
        g_wmBodyOnly = b; Log("mirror: body bone only %s (VR-225; rebuilding)", b ? "ON" : "off");
        if (g_wmIni[0]) WritePrivateProfileStringA("Mirror", "BodyBoneOnly", b ? "1" : "0", g_wmIni);
        WmReleaseAll("body-bone filter toggled");
        return true;
    }
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
    Log("mirror: %s | drawn %ld failed %ld refused %ld | %d built | assets %s | usage: mirror on|off|rebuild|caps on|off|body on|off|back on|off|bias <n>|plane <asset> <axis> <offset> [sign]",
        g_wmOn ? "ON" : "off", g_wmDrawn, g_wmFailed, g_wmRefused, g_wmN, g_wmAssets);
    for (int i = 0; i < g_wmN; ++i)
        Log("mirror:   [%d] '%s' state %d kept %u drawn %ld failed %ld plane n=(%.0f %.0f %.0f) c=(%.2f %.2f %.2f) %s", i,
            g_wm[i].asset, g_wm[i].state, g_wm[i].prims, g_wm[i].drawn, g_wm[i].failed, g_wm[i].n[0], g_wm[i].n[1], g_wm[i].n[2],
            g_wm[i].c[0], g_wm[i].c[1], g_wm[i].c[2], g_wm[i].measured ? "measured" : "override");
    return true;
}
