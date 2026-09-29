// game/dishonored/hands/blade_axis.cpp - included by weapon_attach.cpp, after
// bolt_model_ray.cpp. VR-173 prerequisite 2: WHERE IS THE BLADE.
//
// What the player would get from it: a sword attack timed by the blade reaching
// something. Before anything can ask what the blade touches, the mod has to know where
// the blade is in the hand. This measures it from the sword's own drawn mesh and
// publishes a base and a tip in the palm frame.
//
// MEASUREMENT ONLY. It never alters a draw, a palette or a game object: it reads the
// vertex buffer of a draw the weapon path has ALREADY placed and verified as the held
// sword, skins it through that draw's own palette, and fits a line.
//
// LANE: the draw detour, which is the present thread (the hand draws and the pose tick
// are one thread, ENGINE_NOTES). The snapshot is published under a lock for the aim
// ray's marker (present lane) and, later, the contact trace (script lane).
//
// COST. Off ([Blade] Measure=0, the default): one atomic read per placed weapon draw.
// On and not yet latched: one skinning pass over the mesh per present (2481 vertices on
// the player's sword). Latched: ONE vertex per present, the tip's.
//
// The vertex buffer is locked ONCE per geometry, and again no sooner than 5 s after a
// refusal, exactly as the bolt reader does.
#include "game/dishonored/blade_math.h"
#include <vector>

static std::atomic<bool>  g_blOn{false};            // [Blade] Measure
static std::atomic<bool>  g_blMarker{false};        // [Blade] Marker
static std::atomic<float> g_blMarkerOffsetM{0.0f};  // `blade marker offset <m>`, never saved
static std::atomic<bool>  g_blForget{false};        // `blade forget`, taken on the draw lane
static SRWLOCK g_blLock = SRWLOCK_INIT;
static dvr::hands::BladeSnapshot g_blPub;           // under g_blLock

namespace dvr::hands {
BladeSnapshot blade_snapshot(int hand) {
    BladeSnapshot r;
    if (hand != g_waSwordHand) { r.why = "not the sword hand"; return r; }
    AcquireSRWLockShared(&g_blLock); r = g_blPub; ReleaseSRWLockShared(&g_blLock);
    return r;
}
bool blade_marker(float* offsetM) {
    if (offsetM) *offsetM = g_blMarkerOffsetM.load();
    return g_blMarker.load();
}
} // namespace dvr::hands

struct BlGeom {
    void *vb = nullptr, *ib = nullptr, *decl = nullptr;
    UINT offset = 0, stride = 0, start = 0, count = 0, prims = 0, minIndex = 0;
    INT  base = 0;
    std::vector<dvr::blade::Vertex> verts;          // the vertices the draw's indices use
    bool ok = false;
    uint64_t tried = 0;
    char why[160] = "not read yet";
    int  bones = 0;                                 // distinct dominant bones
    int  tipVertex = -1;                            // index into verts, set by the fit that latched
};
static BlGeom g_blGeom;
static dvr::blade::Latch  g_blLatch;
static dvr::blade::Bounds g_blBounds;
static void*    g_blKeyItem = nullptr;              // the engine's equipped item the latch belongs to
static float    g_blKeyScale = 0.0f;                // [Hands] ModelScale it was measured under
static uint32_t g_blKeyCal = 0;                     // the hand calibration revision
static uint32_t g_blRevision = 0;
static char     g_blAsset[64] = "";
// What the last few seconds looked like, for the beat and `blade status`.
struct BlStats {
    LONG draws = 0, fits = 0, refusedFit = 0, refusedSeg[dvr::blade::kCount] = {}, notIdle = 0, votes = 0, latches = 0;
    LONG liveTips = 0, liveFailed = 0;
    float lastRatio = 0, lastLengthUU = 0;
    dvr::blade::Segment lastSeg;
    float attackMaxDeg = 0, attackMaxM = 0; LONG attackSamples = 0;   // constant against live, in an attack
    float idleMaxM = 0; LONG idleSamples = 0;                          // and at rest: the noise floor
    char  lastWhy[200] = "no sword draw has reached the measurement";
} g_blS;

static void BlWhy(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    _vsnprintf_s(g_blS.lastWhy, sizeof(g_blS.lastWhy), _TRUNCATE, fmt, ap);
    va_end(ap);
}
static void BlPublish(const dvr::hands::BladeSnapshot& s) {
    AcquireSRWLockExclusive(&g_blLock); g_blPub = s; ReleaseSRWLockExclusive(&g_blLock);
}
static void BlDrop(const char* why) {
    const bool had = g_blLatch.have;
    g_blLatch.forget(); g_blGeom.tipVertex = -1;
    dvr::hands::BladeSnapshot s; s.why = "measuring"; s.revision = ++g_blRevision;
    BlPublish(s);
    if (had) Log("blade: the latched blade is DROPPED - %s. It is measured again from the next sword draws at rest.", why);
}

// The sword's vertices, as the draw's index range uses them. Unlike the bolt reader this
// takes any vertex count the index range names and any number of bones: it keeps every
// vertex's four weights, and the caller skins through the palette.
static bool BlReadGeometry(IDirect3DDevice9* dev, WaMesh* w, BlGeom& g) {
    g = BlGeom{};
    g.vb = w->vb; g.ib = w->ib; g.decl = w->decl; g.offset = w->streamOffset;
    g.stride = w->stride; g.start = w->startIndex; g.count = w->numVerts; g.prims = w->primCount;
    g.base = w->baseVertex; g.minIndex = w->minIndex; g.tried = GetTickCount64();
    auto no = [&](const char* fmt, ...) {
        va_list ap; va_start(ap, fmt); _vsnprintf_s(g.why, sizeof(g.why), _TRUNCATE, fmt, ap); va_end(ap);
        return false;
    };
    if (w->type != D3DPT_TRIANGLELIST) return no("the draw is not a triangle list (type %d)", (int)w->type);
    if (w->numVerts < 16 || w->numVerts > 32768) return no("%u vertices, outside 16 to 32768", w->numVerts);
    if (w->primCount > 65536) return no("%u triangles, more than 65536", w->primCount);
    if (w->stride > 128) return no("a %u byte vertex, more than 128", w->stride);
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH]; UINT en = MAXD3DDECLLENGTH;
    IDirect3DVertexDeclaration9* decl = nullptr;
    if (FAILED(dev->GetVertexDeclaration(&decl)) || !decl) return no("no vertex declaration");
    const HRESULT dh = decl->GetDeclaration(el, &en); decl->Release();
    if (FAILED(dh) || en > MAXD3DDECLLENGTH) return no("the vertex declaration would not read");
    MsElem pos = {}, wt = {}, bi = {};
    for (UINT i = 0; i < en; ++i) {
        if (el[i].Type == D3DDECLTYPE_UNUSED) continue;
        MsElem* dst = nullptr;
        if (el[i].Usage == D3DDECLUSAGE_POSITION && el[i].UsageIndex == 0) dst = &pos;
        if (el[i].Usage == D3DDECLUSAGE_BLENDWEIGHT)  dst = &wt;
        if (el[i].Usage == D3DDECLUSAGE_BLENDINDICES) dst = &bi;
        if (dst) { if (el[i].Stream || dst->have) return no("a skinning element is duplicated or off stream 0"); *dst = {(int)el[i].Offset, (int)el[i].Type, 1}; }
    }
    const int weightBytes = wt.type <= D3DDECLTYPE_FLOAT4 ? (wt.type + 1) * 4 : 4;
    if (!pos.have || !wt.have || !bi.have || pos.type != D3DDECLTYPE_FLOAT3 || pos.off + 12 > (int)w->stride ||
        wt.off + weightBytes > (int)w->stride || bi.off + 4 > (int)w->stride)
        return no("the layout has no readable position, weights and bone indices (pos %d/%d wt %d/%d idx %d/%d)",
                  pos.have, pos.type, wt.have, wt.type, bi.have, bi.type);
    IDirect3DVertexBuffer9* vb = nullptr; IDirect3DIndexBuffer9* ib = nullptr; UINT off = 0, stride = 0;
    if (FAILED(dev->GetStreamSource(0, &vb, &off, &stride)) || !vb) return no("no stream 0");
    if (FAILED(dev->GetIndices(&ib)) || !ib) { vb->Release(); return no("no index buffer"); }
    bool ok = false;
    do {
        D3DVERTEXBUFFER_DESC vd; D3DINDEXBUFFER_DESC id;
        if (FAILED(vb->GetDesc(&vd)) || FAILED(ib->GetDesc(&id))) { no("the buffers would not describe themselves"); break; }
        if (id.Format != D3DFMT_INDEX16 && id.Format != D3DFMT_INDEX32) { no("an index format that is neither 16 nor 32 bit"); break; }
        const UINT is = id.Format == D3DFMT_INDEX16 ? 2 : 4;
        const uint64_t io = (uint64_t)w->startIndex * is, il = (uint64_t)w->primCount * 3 * is;
        const int64_t first = (int64_t)w->baseVertex + w->minIndex;
        const uint64_t vo = (uint64_t)off + (first < 0 ? 0 : (uint64_t)first) * stride, vl = (uint64_t)w->numVerts * stride;
        if (first < 0 || stride != w->stride || off != w->streamOffset || io + il > id.Size || vo + vl > vd.Size) {
            no("the draw's ranges do not fit its buffers"); break;
        }
        std::vector<uint8_t> used(w->numVerts, 0);
        void* data = nullptr;
        if (FAILED(ib->Lock((UINT)io, (UINT)il, &data, (id.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY)) || !data) {
            no("the index buffer would not lock"); break;
        }
        bool valid = true;
        for (UINT i = 0; i < w->primCount * 3; ++i) {
            const UINT x = is == 2 ? ((uint16_t*)data)[i] : ((uint32_t*)data)[i];
            if (x < w->minIndex || x - w->minIndex >= w->numVerts) { valid = false; break; }
            used[x - w->minIndex] = 1;
        }
        ib->Unlock();
        if (!valid) { no("an index points outside the draw's vertex range"); break; }
        if (FAILED(vb->Lock((UINT)vo, (UINT)vl, &data, (vd.Usage & D3DUSAGE_WRITEONLY) ? 0 : D3DLOCK_READONLY)) || !data) {
            no("the vertex buffer would not lock"); break;
        }
        g.verts.reserve(w->numVerts);
        bool seen[256] = {};
        for (UINT i = 0; i < w->numVerts && valid; ++i) if (used[i]) {
            const uint8_t* v = (const uint8_t*)data + i * stride;
            dvr::blade::Vertex bv;
            if (!MsReadWeights(v, &wt, bv.w) || !MsReadIndices(v, &bi, bv.bone)) { valid = false; break; }
            float sum = 0;
            for (int j = 0; j < 4; ++j) { if (!std::isfinite(bv.w[j]) || bv.w[j] < 0) valid = false; sum += bv.w[j]; }
            if (!valid || fabsf(sum - 1) > 0.02f) { valid = false; break; }
            memcpy(bv.pos, v + pos.off, 12);
            for (int a = 0; a < 3; ++a) if (!std::isfinite(bv.pos[a])) valid = false;
            const int db = dvr::blade::dominant_bone(bv);
            if (!seen[db]) { seen[db] = true; ++g.bones; }
            g.verts.push_back(bv);
        }
        vb->Unlock();
        if (!valid) { g.verts.clear(); no("a vertex carries weights that do not sum to one, or a value that is not finite"); break; }
        if (g.verts.size() < 16) { no("only %u vertices are used by the draw", (unsigned)g.verts.size()); break; }
        g.ok = ok = true;
        _snprintf_s(g.why, sizeof(g.why), _TRUNCATE, "read");
    } while (false);
    ib->Release(); vb->Release();
    return ok;
}

// One line per bone the mesh is skinned to: how many vertices it owns and where they sit
// along the fitted axis. This is what says whether the sword is one rigid piece, a hilt
// and a blade, or a folding mechanism, before any of that is assumed.
static void BlCensus(const BlGeom& g, const std::vector<float>& posed, const dvr::blade::Axis& ax) {
    struct Row { int n = 0; float lo = 1e20f, hi = -1e20f; };
    Row rows[256];
    for (size_t i = 0; i < g.verts.size(); ++i) {
        const int b = dvr::blade::dominant_bone(g.verts[i]);
        float t = 0;
        for (int a = 0; a < 3; ++a) t += (posed[i*3+a] - ax.fit.center[a]) * ax.fit.dir[a];
        Row& r = rows[b]; ++r.n; if (t < r.lo) r.lo = t; if (t > r.hi) r.hi = t;
    }
    char line[700]; int at = 0;
    for (int b = 0; b < 256 && at < (int)sizeof(line) - 60; ++b)
        if (rows[b].n) at += _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, " bone %d: %d vertices at %.1f..%.1f;", b, rows[b].n, rows[b].lo, rows[b].hi);
    Log("blade: '%s' is %u vertices on %d bone(s), fitted over %d of them: long axis %.1f uu (%.1f to %.1f along it), "
        "variance ratio %.1f to 1 |%s positions are uu along the fitted axis from its centre, in the pose this draw had",
        g_blAsset, (unsigned)g.verts.size(), g.bones, ax.fitted, ax.high - ax.low, ax.low, ax.high, ax.fit.ratio, line);
}

static void BlBeat() {
    static uint64_t next = 0; const uint64_t now = GetTickCount64();
    if (now < next) return;
    next = now + 5000;
    const dvr::blade::Segment& s = g_blS.lastSeg;
    Log("blade: beat %s | sword draws measured %ld, fits %ld (refused %ld), skipped because the body was not at rest %ld, "
        "votes %d of %d, latched %ld time(s) | last: ratio %.1f, %.1f uu long, tip %.3f m from the palm, blade line %.3f m "
        "off the palm, far end %.1fx the near one | live tip %ld (failed %ld); constant against live at rest: max %.4f m "
        "over %ld, in an attack: max %.2f deg %.4f m over %ld | %s. A zero in every count with the sword out means no "
        "sword draw was placed by the weapon path this window (it is sheathed, or the view is not the colour view)",
        g_blLatch.have ? "LATCHED" : "measuring", g_blS.draws, g_blS.fits, g_blS.refusedFit, g_blS.notIdle,
        g_blLatch.votes, g_blLatch.needVotes, g_blS.latches, g_blS.lastRatio, g_blS.lastLengthUU, s.reachM, s.offPalmM,
        s.endRatio, g_blS.liveTips, g_blS.liveFailed, g_blS.idleMaxM, g_blS.idleSamples, g_blS.attackMaxDeg,
        g_blS.attackMaxM, g_blS.attackSamples, g_blS.lastWhy);
}

static void BlMeasure(IDirect3DDevice9* dev, WaMesh* w, const float* palette, UINT regs, const dvr::hf::Xform& delta) {
    if (!g_blOn.load(std::memory_order_relaxed)) return;
    if (w->hand != g_waSwordHand) return;
    if (g_blForget.exchange(false)) BlDrop("asked for (`blade forget`)");
    // The ENGINE says what is equipped; the asset name is for the log only.
    if ((int)InterlockedCompareExchange(&g_rflPrimaryKind, 0, 0) != 1) { BlWhy("the engine does not report a sword in the right hand"); return; }
    if (BrIsLoadedProjectile(w->asset)) return;
    const WaCommon* wc = WaCommonFor(w->hand, nullptr);
    if (!wc || !w->heldOk || w->heldPresent != (uint32_t)dvr::frame::count() || !w->lastL2WOk) return;
    // Only the colour view of the hand's own draw, as the bolt measurement requires.
    D3DVIEWPORT9 vp; DWORD color = 0;
    if (FAILED(dev->GetViewport(&vp)) || FAILED(dev->GetRenderState(D3DRS_COLORWRITEENABLE, &color)) || !color || g_pcLayVp < 0) return;
    IDirect3DSurface9* target = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &target)) || !target) return;
    const bool sameTarget = target == wc->target; target->Release();
    if (!sameTarget || vp.X != wc->viewport.X || vp.Y != wc->viewport.Y ||
        vp.Width != wc->viewport.Width || vp.Height != wc->viewport.Height) return;
    static uint32_t measured = ~0u;
    if (measured == wc->present) return;
    measured = wc->present;
    InterlockedIncrement(&g_blS.draws);
    BlBeat();

    // The latch belongs to ONE item, one model scale and one hand calibration.
    MpHandCal cal; const bool haveCal = MpReadHandCal(w->hand, &cal);
    void* item = g_rflHeldObj[1];
    if (g_blLatch.have && (item != g_blKeyItem || fabsf(g_mpModelScale - g_blKeyScale) > 1e-4f ||
                           (haveCal && cal.revision != g_blKeyCal))) {
        char why[160];
        _snprintf_s(why, sizeof(why), _TRUNCATE, "its key changed (item %p -> %p, model scale %.3f -> %.3f, hand calibration %u -> %u)",
                    g_blKeyItem, item, g_blKeyScale, g_mpModelScale, g_blKeyCal, haveCal ? cal.revision : 0u);
        BlDrop(why);
    }

    BlGeom& g = g_blGeom;
    const uint64_t now = GetTickCount64();
    const bool same = g.vb == w->vb && g.ib == w->ib && g.decl == w->decl && g.stride == w->stride &&
        g.offset == w->streamOffset && g.start == w->startIndex && g.count == w->numVerts &&
        g.prims == w->primCount && g.base == w->baseVertex && g.minIndex == w->minIndex;
    if (!same || (!g.ok && now - g.tried > 5000)) {
        if (!same && g_blLatch.have) BlDrop("the sword is drawn from different geometry");
        strncpy_s(g_blAsset, w->asset, _TRUNCATE);
        if (!BlReadGeometry(dev, w, g)) {
            BlWhy("'%s' could not be read: %s", g_blAsset, g.why);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "blade: '%s' REFUSED - %s (%u vertices, %u triangles, stride %u)",
                             g_blAsset, g.why, w->numVerts, w->primCount, w->stride);
            return;
        }
        Log("blade: read '%s' - %u vertices used of %u, %u triangles, skinned to %d bone(s). The vertex buffer is not locked "
            "again for this geometry", g_blAsset, (unsigned)g.verts.size(), w->numVerts, w->primCount, g.bones);
    }
    if (!g.ok) return;

    // component local -> the draw's camera-relative world -> the palm frame
    float l2w[16];
    if (g_pcLayL2W < 0 || FAILED(dev->GetVertexShaderConstantF(g_pcLayL2W, l2w, 4))) return;
    dvr::hf::Xform draw;
    for (int r = 0; r < 3; ++r) { draw.t[r] = l2w[12 + r]; for (int c = 0; c < 3; ++c) draw.r.m[r*3+c] = l2w[c*4+r]; }
    dvr::hf::Xform invPalm;
    if (!dvr::wf::inverse(wc->palm, &invPalm) || wc->unitsPerMeter < 1) return;
    const dvr::hf::Xform toPalm = dvr::hf::xform_mul(invPalm, dvr::hf::xform_mul(draw, delta));
    auto palm_of = [&](const float* local, float* out) {
        float p[3]; dvr::hf::mulv3(toPalm.r, local, p);
        for (int i = 0; i < 3; ++i) out[i] = (p[i] + toPalm.t[i]) / wc->unitsPerMeter;
    };

    const dvr::anim::Snapshot body = dvr::anim::snapshot();
    const bool attacking = body.valid && !strcmp(body.state[1], "StatePlayerMeleeAttack");
    const bool atRest = body.valid && !strcmp(body.state[1], "StatePlayerUpperIdle") && !body.cameraAction;

    if (g_blLatch.have) {
        // LIVE: the tip vertex alone, through this draw's palette.
        dvr::hands::BladeSnapshot s;
        AcquireSRWLockShared(&g_blLock); s = g_blPub; ReleaseSRWLockShared(&g_blLock);
        float local[3];
        if (g.tipVertex >= 0 && g.tipVertex < (int)g.verts.size() && dvr::blade::skin(g.verts[g.tipVertex], palette, regs, local)) {
            palm_of(local, s.liveTipPalm);
            s.liveOk = true; s.liveMs = now;
            InterlockedIncrement(&g_blS.liveTips);
            dvr::blade::Segment live = g_blLatch.kept;
            for (int i = 0; i < 3; ++i) live.tip[i] = s.liveTipPalm[i];
            float d[3] = { live.tip[0]-live.base[0], live.tip[1]-live.base[1], live.tip[2]-live.base[2] };
            const float n = dvr::blade::len3(d);
            if (n > 1e-4f) for (int i = 0; i < 3; ++i) live.dir[i] = d[i] / n;
            const dvr::blade::Apart a = dvr::blade::apart(live, g_blLatch.kept);
            if (attacking) {
                ++g_blS.attackSamples;
                if (a.deg > g_blS.attackMaxDeg) g_blS.attackMaxDeg = a.deg;
                if (a.tipM > g_blS.attackMaxM) g_blS.attackMaxM = a.tipM;
            } else if (atRest) {
                ++g_blS.idleSamples;
                if (a.tipM > g_blS.idleMaxM) g_blS.idleMaxM = a.tipM;
            }
        } else { s.liveOk = false; InterlockedIncrement(&g_blS.liveFailed); }
        // WHERE THE RENDERER PUT IT, in the game's world: the latched points carried the
        // way this draw went (palm frame -> the hand draw's space) and back across the
        // coordinate bridge the weapon path identifies its draws with. No headset pose is
        // in it. The bridge's anchor is the arm mesh's own component transform from the
        // script lane's snapshot, which is the known answer the XR route is judged by.
        {
            const int e = wc->eye > 0 ? 1 : 0;
            s.drawnOk[e] = false;
            const WaComp* ref = nullptr;
            for (int i = 0; i < wc->componentCount && i < WA_MAX_COMP; ++i)
                if (wc->components[i].ok && wc->components[i].isRef) { ref = &wc->components[i]; break; }
            dvr::hf::Xform br, ibr;
            if (ref) {
                const dvr::hf::Xform nr = { ref->R, { ref->t[0], ref->t[1], ref->t[2] } };
                if (dvr::wf::bridge(nr, wc->L_hand, &br) && dvr::wf::inverse(br, &ibr)) {
                    const dvr::hf::Xform toWorld = dvr::hf::xform_mul(ibr, wc->palm);
                    float b[3], t[3];
                    for (int i = 0; i < 3; ++i) { b[i] = s.basePalm[i] * wc->unitsPerMeter; t[i] = s.tipPalm[i] * wc->unitsPerMeter; }
                    dvr::hf::apply_point(toWorld, b, s.drawnBaseWorld[e]);
                    dvr::hf::apply_point(toWorld, t, s.drawnTipWorld[e]);
                    bool fin = true;
                    for (int i = 0; i < 3; ++i) fin = fin && MpFinite(s.drawnBaseWorld[e][i]) && MpFinite(s.drawnTipWorld[e][i]);
                    s.drawnOk[e] = fin; s.drawnMs[e] = now;
                }
            }
        }
        BlPublish(s);
        return;
    }

    // NOT LATCHED: the whole mesh, in the pose this draw has.
    if (!atRest) {
        InterlockedIncrement(&g_blS.notIdle);
        BlWhy("waiting for the body to be at rest (upper=%s) - a blade latched during an attack or a draw would be the clip's pose",
              body.valid ? body.state[1] : "no animation snapshot");
        return;
    }
    static std::vector<float> posed;
    posed.resize(g.verts.size() * 3);
    for (size_t i = 0; i < g.verts.size(); ++i)
        if (!dvr::blade::skin(g.verts[i], palette, regs, &posed[i*3])) {
            InterlockedIncrement(&g_blS.refusedFit);
            BlWhy("vertex %u names a bone this draw's palette does not carry (%u registers)", (unsigned)i, regs);
            return;
        }
    dvr::blade::Axis ax;
    // 2:1 is "has a long axis at all". The ratio is LOGGED; what decides is the segment's
    // own bounds below and the marker on the drawn blade.
    if (!dvr::blade::fit((const float (*)[3])posed.data(), (int)g.verts.size(), 2.0f, ax)) {
        InterlockedIncrement(&g_blS.refusedFit);
        BlWhy("'%s' has no long axis (the fit refused at 2 to 1)", g_blAsset);
        return;
    }
    InterlockedIncrement(&g_blS.fits);
    g_blS.lastRatio = ax.fit.ratio; g_blS.lastLengthUU = ax.high - ax.low;
    static void* censusFor = nullptr;
    if (censusFor != g.vb) { censusFor = g.vb; BlCensus(g, posed, ax); }
    float endLow[3], endHigh[3];
    palm_of(&posed[ax.lowIndex*3], endLow);
    palm_of(&posed[ax.highIndex*3], endHigh);
    const dvr::blade::Segment seg = dvr::blade::segment(endLow, endHigh, g_blBounds);
    g_blS.lastSeg = seg;
    if (!seg.ok) {
        InterlockedIncrement(&g_blS.refusedSeg[seg.refuse]);
        BlWhy("candidate REFUSED: %s (tip %.3f m from the palm, far end %.1fx the near one, line %.3f m off the palm; "
              "bounds %.2f..%.2f m, %.1fx, %.2f m)", dvr::blade::refuse_text(seg.refuse), seg.reachM, seg.endRatio,
              seg.offPalmM, g_blBounds.minReachM, g_blBounds.maxReachM, g_blBounds.minEndRatio, g_blBounds.maxOffPalmM);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000, "blade: %s", g_blS.lastWhy);
        return;
    }
    const bool latched = g_blLatch.feed(seg, now);
    g_blS.votes = g_blLatch.votes;
    BlWhy("a candidate is settling: %d of %d agreeing draws over %llu of %llu ms", g_blLatch.votes, g_blLatch.needVotes,
          (unsigned long long)(now - g_blLatch.firstMs), (unsigned long long)g_blLatch.needMs);
    if (!latched) return;
    InterlockedIncrement(&g_blS.latches);
    g.tipVertex = seg.tipIsHigh > 0 ? ax.highIndex : ax.lowIndex;
    g_blKeyItem = item; g_blKeyScale = g_mpModelScale; g_blKeyCal = haveCal ? cal.revision : 0;
    const dvr::blade::Segment& k = g_blLatch.kept;
    dvr::hands::BladeSnapshot s;
    s.ok = true; s.lengthM = k.lengthM; s.reachM = k.reachM; s.ratio = ax.fit.ratio; s.revision = ++g_blRevision;
    for (int i = 0; i < 3; ++i) { s.basePalm[i] = k.base[i]; s.tipPalm[i] = k.tip[i]; s.liveTipPalm[i] = k.tip[i]; }
    s.liveOk = true; s.liveMs = now; s.why = "latched";
    BlPublish(s);
    BlWhy("latched");
    Log("blade: LATCHED '%s' for item %p - base (%.3f %.3f %.3f) tip (%.3f %.3f %.3f) m in the palm frame: %.3f m of blade "
        "ahead of the palm, tip %.3f m from the palm, the blade line passes %.3f m from it; variance ratio %.1f to 1, the "
        "whole mesh %.1f uu long; model scale %.2f, %.0f uu per metre of hand travel. Agreed over %d draws and %llu ms with "
        "the body at rest, so it is the held sword's pose and not a clip's. It follows the hand and the hand trim because "
        "it is stored in the palm frame; a different item, model scale or hand calibration drops it",
        g_blAsset, item, k.base[0], k.base[1], k.base[2], k.tip[0], k.tip[1], k.tip[2], k.lengthM, k.reachM, k.offPalmM,
        ax.fit.ratio, ax.high - ax.low, g_mpModelScale, wc->unitsPerMeter, g_blLatch.votes,
        (unsigned long long)(now - g_blLatch.firstMs));
}

// ---- levers, the word and the status ------------------------------------------------

static void BlSet(bool on, const char* who) {
    const bool was = g_blOn.exchange(on);
    if (was == on) return;
    if (!on) g_blForget.store(true);
    Log("blade: measurement %s (%s). %s", on ? "ON" : "off", who,
        on ? "The held sword's mesh is read once and fitted while the body is at rest; `blade status` says where it is"
           : "Nothing reads the sword's draw; the latched blade is dropped at the next sword draw");
    if (!on) { dvr::hands::BladeSnapshot s; s.revision = ++g_blRevision; BlPublish(s); }
}
static void BlConfigure(const char* ini) {
    g_blOn.store(IniFloat(ini, "Blade", "Measure", 0) != 0.0f);
    g_blMarker.store(IniFloat(ini, "Blade", "Marker", 0) != 0.0f);
    Log("config: [Blade] Measure=%d Marker=%d - the held sword's blade, measured from its drawn mesh (VR-173). Off, nothing "
        "reads the sword's draw. The marker shows the measured base and tip as two dots, to be judged against the drawn blade",
        (int)g_blOn.load(), (int)g_blMarker.load());
}
static void BlSave(const char* ini) {
    WritePrivateProfileStringA("Blade", "Measure", g_blOn.load() ? "1" : "0", ini);
    WritePrivateProfileStringA("Blade", "Marker", g_blMarker.load() ? "1" : "0", ini);
}
static void BlReport() {
    const dvr::hands::BladeSnapshot s = dvr::hands::blade_snapshot(g_waSwordHand);
    const uint64_t now = GetTickCount64();
    Log("blade: measure=%d marker=%d (offset %.3f m) | %s | base (%.3f %.3f %.3f) tip (%.3f %.3f %.3f) m, palm frame, %.3f m "
        "of blade, tip %.3f m from the palm | live tip %s (%.3f %.3f %.3f), %llu ms old | revision %u | %s",
        (int)g_blOn.load(), (int)g_blMarker.load(), g_blMarkerOffsetM.load(), s.ok ? "LATCHED" : "not latched",
        s.basePalm[0], s.basePalm[1], s.basePalm[2], s.tipPalm[0], s.tipPalm[1], s.tipPalm[2], s.lengthM, s.reachM,
        s.liveOk ? "ok" : "none", s.liveTipPalm[0], s.liveTipPalm[1], s.liveTipPalm[2],
        s.liveMs ? (unsigned long long)(now - s.liveMs) : 0ull, s.revision, g_blS.lastWhy);
    Log("blade: draws measured %ld, fits %ld (refused %ld), not at rest %ld, latches %ld | refused candidates: ends alike %ld, "
        "too short %ld, too long %ld, off the palm %ld | constant against live, at rest max %.4f m over %ld, in an attack "
        "max %.2f deg %.4f m over %ld",
        g_blS.draws, g_blS.fits, g_blS.refusedFit, g_blS.notIdle, g_blS.latches,
        g_blS.refusedSeg[dvr::blade::kEndsAlike], g_blS.refusedSeg[dvr::blade::kTooShort],
        g_blS.refusedSeg[dvr::blade::kTooLong], g_blS.refusedSeg[dvr::blade::kOffThePalm],
        g_blS.idleMaxM, g_blS.idleSamples, g_blS.attackMaxDeg, g_blS.attackMaxM, g_blS.attackSamples);
}
static bool BlCommand(const char* args) {
    char a[24] = {}, b[24] = {}, c[24] = {};
    sscanf(args ? args : "", "%23s %23s %23s", a, b, c);
    if (!strcmp(a, "on"))  { BlSet(true, "the seam"); return true; }
    if (!strcmp(a, "off")) { BlSet(false, "the seam"); return true; }
    if (!strcmp(a, "forget")) { g_blForget.store(true); Log("blade: the latch is dropped at the next sword draw and measured again"); return true; }
    if (!strcmp(a, "marker")) {
        if (!strcmp(b, "on") || !strcmp(b, "off")) {
            g_blMarker.store(!strcmp(b, "on"));
            Log("blade: marker %s - %s", g_blMarker.load() ? "ON" : "off", g_blMarker.load()
                ? "two dots, the larger at the measured tip and the smaller at the base; they must sit on the drawn blade in BOTH eyes"
                : "no dots");
            return true;
        }
        if (!strcmp(b, "offset") && *c) {
            float v = (float)atof(c); v = v < -0.5f ? -0.5f : v > 0.5f ? 0.5f : v;
            g_blMarkerOffsetM.store(v);
            Log("blade: marker offset %.3f m across the blade - a DELIBERATE error, so the check that reads the marker "
                "against the drawn blade can be shown to fail. 0 puts it back. Never saved", v);
            return true;
        }
    }
    if (!strcmp(a, "attack") && !strcmp(b, "reset")) {
        g_blS.attackMaxDeg = g_blS.attackMaxM = g_blS.idleMaxM = 0; g_blS.attackSamples = g_blS.idleSamples = 0;
        Log("blade: the constant-against-live figures are cleared");
        return true;
    }
    if (!strcmp(a, "save")) {
        char ini[MAX_PATH]; _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir); ini[MAX_PATH-1] = 0;
        BlSave(ini); Log("blade: [Blade] written to %s", ini); return true;
    }
    if (!strcmp(a, "world") || !strcmp(a, "trace")) return BladeContactCommand(a, b, c);   // blade_contact.cpp
    if (*a && strcmp(a, "status"))
        Log("blade: status | on|off | forget | marker on|off | marker offset <m> | attack reset | save | "
            "world on|off|status|reset (the XR-to-world bridge, checked against the draw)");
    BlReport();
    return true;
}
static void BlStatus(dvr::status::Writer& w) {
    const dvr::hands::BladeSnapshot s = dvr::hands::blade_snapshot(g_waSwordHand);
    const uint64_t now = GetTickCount64();
    w.obj("blade");
    w.kv("measure", g_blOn.load()); w.kv("marker", g_blMarker.load());
    w.kv("markerOffsetM", (double)g_blMarkerOffsetM.load());
    w.kv("latched", s.ok); w.kv("revision", (unsigned long)s.revision);
    w.kv("lengthM", (double)s.lengthM); w.kv("reachM", (double)s.reachM); w.kv("ratio", (double)s.ratio);
    w.kv("tipX", (double)s.tipPalm[0]); w.kv("tipY", (double)s.tipPalm[1]); w.kv("tipZ", (double)s.tipPalm[2]);
    w.kv("baseX", (double)s.basePalm[0]); w.kv("baseY", (double)s.basePalm[1]); w.kv("baseZ", (double)s.basePalm[2]);
    w.kv("liveOk", s.liveOk); w.kv("liveAgeMs", (unsigned long)(s.liveMs ? now - s.liveMs : 0));
    w.kv("draws", (unsigned long)g_blS.draws); w.kv("fits", (unsigned long)g_blS.fits);
    w.kv("latches", (unsigned long)g_blS.latches); w.kv("notAtRest", (unsigned long)g_blS.notIdle);
    w.kv("attackSamples", (unsigned long)g_blS.attackSamples);
    w.kv("attackMaxDeg", (double)g_blS.attackMaxDeg); w.kv("attackMaxM", (double)g_blS.attackMaxM);
    w.kv("idleMaxM", (double)g_blS.idleMaxM);
    w.kv("why", g_blS.lastWhy);
    w.end_obj();
}
