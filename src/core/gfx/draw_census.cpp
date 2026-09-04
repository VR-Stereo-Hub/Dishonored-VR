// core/gfx/draw_census.cpp - see draw_census.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/draw_census.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/hooks/vtable.h"
#include "core/util/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::draws {
namespace {

typedef HRESULT (__stdcall *PFN_DrawPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT, UINT);
typedef HRESULT (__stdcall *PFN_DrawIndexedPrimitive)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT,
                                                      UINT, UINT, UINT);
typedef HRESULT (__stdcall *PFN_DrawPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
                                                 const void*, UINT);
typedef HRESULT (__stdcall *PFN_DrawIndexedPrimitiveUP)(IDirect3DDevice9*, D3DPRIMITIVETYPE, UINT,
                                                        UINT, UINT, const void*, D3DFORMAT,
                                                        const void*, UINT);
typedef HRESULT (__stdcall *PFN_SetViewport)(IDirect3DDevice9*, const D3DVIEWPORT9*);
typedef HRESULT (__stdcall *PFN_SetRenderState)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
typedef HRESULT (__stdcall *PFN_SetTexture)(IDirect3DDevice9*, DWORD, IDirect3DBaseTexture9*);
typedef HRESULT (__stdcall *PFN_SetVertexDeclaration)(IDirect3DDevice9*,
                                                      IDirect3DVertexDeclaration9*);
typedef HRESULT (__stdcall *PFN_SetVertexShader)(IDirect3DDevice9*, IDirect3DVertexShader9*);
typedef HRESULT (__stdcall *PFN_SetPixelShader)(IDirect3DDevice9*, IDirect3DPixelShader9*);
typedef HRESULT (__stdcall *PFN_CreateStateBlock)(IDirect3DDevice9*, D3DSTATEBLOCKTYPE,
                                                  IDirect3DStateBlock9**);
typedef HRESULT (__stdcall *PFN_EndStateBlock)(IDirect3DDevice9*, IDirect3DStateBlock9**);

PFN_DrawPrimitive           g_origDp = nullptr;
PFN_DrawIndexedPrimitive    g_origDip = nullptr;
PFN_DrawPrimitiveUP         g_origDpUp = nullptr;
PFN_DrawIndexedPrimitiveUP  g_origDipUp = nullptr;
PFN_SetViewport             g_origSetVp = nullptr;
PFN_SetRenderState          g_origSetRs = nullptr;
PFN_SetTexture              g_origSetTex = nullptr;
PFN_SetVertexDeclaration    g_origSetDecl = nullptr;
PFN_SetVertexShader         g_origSetVs = nullptr;
PFN_SetPixelShader          g_origSetPs = nullptr;
PFN_CreateStateBlock        g_origCreateSb = nullptr;
PFN_EndStateBlock           g_origEndSb = nullptr;

bool g_hooksOk = false;

// The one gate every hook tests first: off = one bool per call and nothing else.
// g_wanted is what the ini or the seam asked for, which can be BEFORE the device
// exists (the config is read at DllMain); it is applied when the hooks install.
bool g_track = false;
bool g_wanted = false;

// The render-thread assumption, measured rather than assumed: the draws must
// arrive on the thread that presents, or every unlocked global in here is a
// race and the census refuses. The check costs one bool test per draw once it
// has an answer, and re-arms on a device Reset in case the answer changes.
bool  g_threadAsking = true;
DWORD g_drawTid = 0;
DWORD g_lastDrawTid = 0;
bool  g_threadRefused = false;

// State blocks bypass the Set* hooks: Scaleform may save and restore device
// state through IDirect3DStateBlock9::Apply, and a tracked value after such an
// Apply would be stale. Counting them says whether the Apply slot has to be
// hooked too; a zero here is the licence not to.
uint32_t g_stateBlocksCreated = 0;

// ---- the shadowed device state -------------------------------------------
// The device is PURE (ENGINE_NOTES, "The creation census"), so no Get* answers
// for state: every value a classifier needs must be shadowed from its setter.
IDirect3DSurface9*  g_rt0 = nullptr;        // pointer VALUE only, never a reference
IDirect3DSurface9*  g_bbPtr = nullptr;
uint32_t            g_bbW = 0, g_bbH = 0;
D3DVIEWPORT9        g_vp = {};
bool                g_vpKnown = false;
DWORD               g_zEnable = D3DZB_TRUE;
DWORD               g_zWrite = TRUE;
DWORD               g_alphaBlend = FALSE;
void*               g_ps = nullptr;
void*               g_vs = nullptr;
void*               g_vdecl = nullptr;
void*               g_tex0 = nullptr;

// ---- caches keyed on pointer VALUE ---------------------------------------
// Cleared every window and on Reset, so a reused pointer self-corrects.
template <int N> struct PtrMap {
    void*    key[N];
    uint32_t val[N];
    void clear() { memset(key, 0, sizeof(key)); memset(val, 0, sizeof(val)); }
    uint32_t* find_or_add(void* k) {
        uint32_t h = (uint32_t)(((uintptr_t)k >> 4) * 2654435761u) & (N - 1);
        for (int i = 0; i < 16; i++) {
            const uint32_t s = (h + i) & (N - 1);
            if (key[s] == k) return &val[s];
            if (key[s] == nullptr) { key[s] = k; val[s] = 0xffffffffu; return &val[s]; }
        }
        return nullptr;   // full: this pointer goes uncached this window
    }
};
PtrMap<512>  g_psHash;     // pixel shader -> FNV-1a of its bytecode
PtrMap<2048> g_texIsRt;    // texture -> 1 when it carries D3DUSAGE_RENDERTARGET
PtrMap<256>  g_surfSize;   // surface -> (w << 16) | h

uint32_t g_psDistinct = 0;

uint32_t fnv32(const uint8_t* p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

uint32_t ps_hash(void* ps) {
    if (!ps) return 0;
    uint32_t* slot = g_psHash.find_or_add(ps);
    if (!slot) return 0;
    if (*slot != 0xffffffffu) return *slot;
    uint32_t h = 0;
    UINT size = 0;
    IDirect3DPixelShader9* s = (IDirect3DPixelShader9*)ps;
    if (SUCCEEDED(s->GetFunction(nullptr, &size)) && size && size <= 16384) {
        uint8_t buf[16384];
        if (SUCCEEDED(s->GetFunction(buf, &size))) h = fnv32(buf, size);
    }
    *slot = h;
    ++g_psDistinct;
    DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug, "draws: ps %08x first seen (%u bytes)",
            (unsigned)h, (unsigned)size);
    return h;
}

// 0 = no texture at stage 0, 1 = a plain texture, 2 = a render target.
// "tex0 is not a render target" is the fork's measured UI discriminator
// (ENGINE_NOTES, the samples-rt term): fonts and shapes sample plain textures,
// the scene blits and the tonemap sample the frame.
uint8_t tex0_class(void* t) {
    if (!t) return 0;
    uint32_t* slot = g_texIsRt.find_or_add(t);
    if (!slot) return 1;
    if (*slot != 0xffffffffu) return (uint8_t)*slot;
    uint32_t isRt = 0;
    IDirect3DBaseTexture9* b = (IDirect3DBaseTexture9*)t;
    if (b->GetType() == D3DRTYPE_TEXTURE) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(((IDirect3DTexture9*)b)->GetLevelDesc(0, &d)) &&
            (d.Usage & D3DUSAGE_RENDERTARGET))
            isRt = 1;
    }
    *slot = isRt;
    return isRt ? 2 : 1;
}

void surf_size(IDirect3DSurface9* s, uint16_t* w, uint16_t* h) {
    *w = 0; *h = 0;
    if (!s) return;
    uint32_t* slot = g_surfSize.find_or_add(s);
    if (!slot) return;
    if (*slot == 0xffffffffu) {
        D3DSURFACE_DESC d;
        *slot = SUCCEEDED(s->GetDesc(&d)) ? ((d.Width & 0xffff) << 16) | (d.Height & 0xffff) : 0;
    }
    *w = (uint16_t)(*slot >> 16); *h = (uint16_t)(*slot & 0xffff);
}

// ---- the bucket signature -------------------------------------------------
// Every column the fork's classifier used, plus the ones this path can see
// that it could not: the tonemap boundary and the pixel shader identity.
// NOTE the fork's rt-portrait term is deliberately NOT here: it excluded
// portrait targets because its side-by-side frame was always landscape, and
// our per-eye render is 2496x2688, portrait. Porting it would reject the
// whole frame.
#pragma pack(push, 1)
struct Sig {
    uint32_t psHash;
    uint32_t vdecl;      // low bits of the declaration pointer: an identity, not an address
    uint16_t rtW, rtH;
    uint8_t  entry;      // 0 DrawPrimitive, 1 Indexed, 2 UP, 3 IndexedUP
    uint8_t  rtClass;    // 0 the backbuffer, 1 another target, 2 unknown
    uint8_t  vpFull;     // the viewport covers the whole target at the origin
    uint8_t  zEnable;    // D3DZB_FALSE / TRUE / USEW
    uint8_t  alphaBlend;
    uint8_t  tex0;       // 0 none, 1 plain, 2 render target
    uint8_t  afterTm;    // after this present's first full-frame RT-sampling draw
    uint8_t  primBand;   // 0: <=2, 1: <=16, 2: <=256, 3: more
};
#pragma pack(pop)

uint64_t sig_key(const Sig& s) {
    const uint8_t* p = (const uint8_t*)&s;
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < sizeof(Sig); i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

// The candidate rule: the fork's ladder, translated to this render. A draw is
// HUD-class when it is a user-pointer draw to the whole backbuffer, with depth
// off, sampling a plain texture, after the frame has been tonemapped.
enum { kTermEntry = 0, kTermRt, kTermVp, kTermZ, kTermTex, kTermTm, kTermCount };
const char* const kTermName[kTermCount] = { "entry=UP", "rt=backbuffer", "viewport=full",
                                            "z=off", "tex0=plain", "afterTonemap" };

void sig_terms(const Sig& s, bool* t) {
    t[kTermEntry] = (s.entry == 2 || s.entry == 3);
    t[kTermRt]    = (s.rtClass == 0);
    t[kTermVp]    = (s.vpFull != 0);
    t[kTermZ]     = (s.zEnable == D3DZB_FALSE);
    t[kTermTex]   = (s.tex0 == 1);
    t[kTermTm]    = (s.afterTm != 0);
}

bool is_candidate(const Sig& s) {
    bool t[kTermCount];
    sig_terms(s, t);
    for (int i = 0; i < kTermCount; i++) if (!t[i]) return false;
    return true;
}

void sig_text(const Sig& s, char* out, size_t n) {
    static const char* kEntry[4] = { "DP", "DIP", "UP", "IUP" };
    static const char* kTex[3]   = { "tex=none", "tex=plain", "tex=RT" };
    static const char* kPrim[4]  = { "p<=2", "p<=16", "p<=256", "p>256" };
    char rt[32];
    if (s.rtClass == 0) _snprintf(rt, sizeof(rt), "bb");
    else if (s.rtClass == 1) _snprintf(rt, sizeof(rt), "rt%ux%u", (unsigned)s.rtW, (unsigned)s.rtH);
    else _snprintf(rt, sizeof(rt), "rt?");
    rt[sizeof(rt) - 1] = 0;
    _snprintf(out, n, "%-3s %-10s %s z%u %s %s %s ps=%08x vd=%08x %s",
              kEntry[s.entry & 3], rt, s.vpFull ? "vpF" : "vpP", (unsigned)s.zEnable,
              s.alphaBlend ? "blend" : "opaque", kTex[s.tex0 > 2 ? 2 : s.tex0],
              s.afterTm ? "aTM" : "bTM", (unsigned)s.psHash, (unsigned)s.vdecl,
              kPrim[s.primBand & 3]);
    out[n - 1] = 0;
}

// ---- the window's table ---------------------------------------------------
struct Row {
    uint64_t key;
    Sig      sig;
    uint32_t draws;
    uint32_t presents;      // presents this bucket appeared in
    uint32_t lastPresent;
    uint32_t minOrd, maxOrd;
};
const int kRows = 512;
Row      g_row[kRows];
uint32_t g_rowsUsed = 0;
uint32_t g_rowOverflow = 0;

Row* row_for(const Sig& s) {
    const uint64_t k = sig_key(s);
    uint32_t h = (uint32_t)(k ^ (k >> 32)) & (kRows - 1);
    for (int i = 0; i < 24; i++) {
        const uint32_t slot = (h + i) & (kRows - 1);
        Row& r = g_row[slot];
        if (r.key == k && r.draws) return &r;
        if (!r.draws) {
            memset(&r, 0, sizeof(r));
            r.key = k; r.sig = s; r.minOrd = 0xffffffffu;
            ++g_rowsUsed;
            return &r;
        }
    }
    ++g_rowOverflow;
    return nullptr;
}

// ---- window counters ------------------------------------------------------
uint32_t g_winPresents = 0;
uint32_t g_winDraws = 0;
uint32_t g_winByEntry[4] = {};
uint32_t g_winTonemapDraws = 0;
uint32_t g_winTmFirstOrdSum = 0, g_winTmLastOrdSum = 0, g_winTmPresents = 0;
uint32_t g_winOrdSum = 0;
uint32_t g_winRtSampleAfterCand = 0;   // an RT-sampling frame draw AFTER a candidate
unsigned long g_winStartMs = 0;

// ---- per-present state ----------------------------------------------------
uint32_t g_ord = 0;               // draw ordinal within this present
uint32_t g_presentNo = 0;
bool     g_tmSeen = false;        // the tonemap boundary has passed
uint32_t g_tmFirstOrd = 0, g_tmLastOrd = 0;
bool     g_candSeen = false;

// `draws kill`: the project's rule for identifying a render pass is to make it
// MOVE. A killed bucket's draws are dropped, so a capture before and after says
// by PICTURE whether the class is the HUD - which no counter can.
const int kKills = 8;
uint32_t g_kill[kKills] = {};
int      g_killN = 0;
bool     g_killHud = false;
uint32_t g_winKilled = 0;

// The two counters the game side lends this module. The viewport-draw count is
// the engine's own per-pass count (2 per tick under `stereo reentry`); the
// PostRender count is script-thread and one dispatch per pass. Their RATIO is
// the number that says whether the HUD is drawn once per tick or once per eye,
// and it is the only honest way to use a game-thread counter here: the game
// thread runs a frame AHEAD of the render thread, so a per-present delta of
// this counter is not aligned to the present it is printed beside.
uint32_t (*g_viewportDraws)() = nullptr;
uint32_t (*g_postRender)() = nullptr;
uint32_t g_prAtWinStart = 0, g_vdAtWinStart = 0;
uint32_t g_prLast = 0, g_prDeltaLast = 0;

// The short key the table prints and `draws kill` takes.
inline uint32_t short_key(uint64_t k) { return (uint32_t)(k >> 32); }

// The last verdict, for status.json and `draws status`.
char     g_verdict[320] = "not measured yet";
uint32_t g_candBuckets = 0;
double   g_candPerPresent = 0.0;

void note_draw() {
    if (g_threadAsking) g_lastDrawTid = GetCurrentThreadId();
}

// Returns true when this draw is to be DROPPED (the kill lever).
bool record(uint8_t entry, UINT prims) {
    ++g_ord;
    Sig s;
    memset(&s, 0, sizeof(s));
    s.entry = entry;
    s.psHash = ps_hash(g_ps);
    s.vdecl = (uint32_t)(uintptr_t)g_vdecl;
    s.zEnable = (uint8_t)g_zEnable;
    s.alphaBlend = g_alphaBlend ? 1 : 0;
    s.tex0 = tex0_class(g_tex0);
    s.primBand = prims <= 2 ? 0 : prims <= 16 ? 1 : prims <= 256 ? 2 : 3;

    IDirect3DSurface9* rt = g_rt0;
    uint16_t rw = 0, rh = 0;
    if (!rt || rt == g_bbPtr) {
        // A null RT0 is the implicit backbuffer: the game has not set one since
        // the device came up or was reset.
        s.rtClass = 0;
        rw = (uint16_t)g_bbW; rh = (uint16_t)g_bbH;
    } else {
        s.rtClass = 1;
        surf_size(rt, &rw, &rh);
        if (!rw) s.rtClass = 2;
    }
    s.rtW = rw; s.rtH = rh;
    s.vpFull = (g_vpKnown && rw && g_vp.X == 0 && g_vp.Y == 0 &&
                g_vp.Width == rw && g_vp.Height == rh) ? 1 : 0;

    // The tonemap boundary: a full-frame draw to the backbuffer that SAMPLES a
    // render target. The HUD is painted over it; the fork could not see this
    // boundary and had to infer it, and it is the one thing this path has that
    // the fork's author did not.
    const bool isTonemap = (s.rtClass == 0 && s.vpFull && s.tex0 == 2);
    if (isTonemap) {
        ++g_winTonemapDraws;
        if (!g_tmSeen) { g_tmSeen = true; g_tmFirstOrd = g_ord; }
        g_tmLastOrd = g_ord;
        if (g_candSeen) ++g_winRtSampleAfterCand;
    }
    s.afterTm = g_tmSeen ? 1 : 0;

    if (is_candidate(s)) g_candSeen = true;

    ++g_winDraws;
    ++g_winByEntry[entry & 3];
    Row* r = row_for(s);
    if (r) {
        ++r->draws;
        if (r->lastPresent != g_presentNo) { r->lastPresent = g_presentNo; ++r->presents; }
        if (g_ord < r->minOrd) r->minOrd = g_ord;
        if (g_ord > r->maxOrd) r->maxOrd = g_ord;
    }

    if (!g_killN && !g_killHud) return false;
    if (g_killHud && is_candidate(s)) { ++g_winKilled; return true; }
    const uint32_t sk = short_key(sig_key(s));
    for (int i = 0; i < g_killN; i++)
        if (g_kill[i] == sk) { ++g_winKilled; return true; }
    return false;
}

// ---- the hooks ------------------------------------------------------------
// Every one tests g_track first: an installed-but-off census is one bool.

HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT start,
                                  UINT prims) {
    note_draw();
    if (g_track && record(0, prims)) return D3D_OK;   // killed on purpose
    return g_origDp(self, type, start, prims);
}

HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, INT base,
                                         UINT minIdx, UINT numVerts, UINT startIdx, UINT prims) {
    note_draw();
    if (g_track && record(1, prims)) return D3D_OK;   // killed on purpose
    return g_origDip(self, type, base, minIdx, numVerts, startIdx, prims);
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT prims,
                                    const void* verts, UINT stride) {
    note_draw();
    if (g_track && record(2, prims)) return D3D_OK;   // killed on purpose
    return g_origDpUp(self, type, prims, verts, stride);
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
                                           UINT minIdx, UINT numVerts, UINT prims,
                                           const void* idxData, D3DFORMAT idxFmt,
                                           const void* verts, UINT stride) {
    note_draw();
    if (g_track && record(3, prims)) return D3D_OK;   // killed on purpose
    return g_origDipUp(self, type, minIdx, numVerts, prims, idxData, idxFmt, verts, stride);
}

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* self, const D3DVIEWPORT9* vp) {
    if (g_track && vp) { g_vp = *vp; g_vpKnown = true; }
    return g_origSetVp(self, vp);
}

HRESULT __stdcall hkSetRenderState(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value) {
    if (g_track) {
        if (state == D3DRS_ZENABLE) g_zEnable = value;
        else if (state == D3DRS_ZWRITEENABLE) g_zWrite = value;
        else if (state == D3DRS_ALPHABLENDENABLE) g_alphaBlend = value;
    }
    return g_origSetRs(self, state, value);
}

HRESULT __stdcall hkSetTexture(IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9* tex) {
    if (g_track && stage == 0) g_tex0 = tex;
    return g_origSetTex(self, stage, tex);
}

HRESULT __stdcall hkSetVertexDeclaration(IDirect3DDevice9* self,
                                         IDirect3DVertexDeclaration9* decl) {
    if (g_track) g_vdecl = decl;
    return g_origSetDecl(self, decl);
}

HRESULT __stdcall hkSetVertexShader(IDirect3DDevice9* self, IDirect3DVertexShader9* vs) {
    if (g_track) g_vs = vs;
    return g_origSetVs(self, vs);
}

HRESULT __stdcall hkSetPixelShader(IDirect3DDevice9* self, IDirect3DPixelShader9* ps) {
    if (g_track) g_ps = ps;
    return g_origSetPs(self, ps);
}

HRESULT __stdcall hkCreateStateBlock(IDirect3DDevice9* self, D3DSTATEBLOCKTYPE type,
                                     IDirect3DStateBlock9** out) {
    ++g_stateBlocksCreated;
    return g_origCreateSb(self, type, out);
}

HRESULT __stdcall hkEndStateBlock(IDirect3DDevice9* self, IDirect3DStateBlock9** out) {
    ++g_stateBlocksCreated;
    return g_origEndSb(self, out);
}

// ---- the separator search -------------------------------------------------
// A column separates when the set of values the candidates take and the set the
// rest take do not intersect. A set that overflows its cap counts as NOT
// separating: an instrument must be able to print the unwelcome answer.
struct ValSet {
    uint32_t v[24];
    int      n = 0;
    bool     over = false;
    void add(uint32_t x) {
        for (int i = 0; i < n; i++) if (v[i] == x) return;
        if (n >= 24) { over = true; return; }
        v[n++] = x;
    }
    bool disjoint(const ValSet& o) const {
        if (over || o.over) return false;
        for (int i = 0; i < n; i++)
            for (int j = 0; j < o.n; j++) if (v[i] == o.v[j]) return false;
        return true;
    }
};

uint32_t column(const Sig& s, int c) {
    switch (c) {
        case 0: return s.entry;
        case 1: return s.rtClass;
        case 2: return s.vpFull;
        case 3: return s.zEnable;
        case 4: return s.alphaBlend;
        case 5: return s.tex0;
        case 6: return s.afterTm;
        case 7: return s.psHash;
        case 8: return s.vdecl;
        default: return s.primBand;
    }
}
const char* const kColName[10] = { "entry", "rt", "viewport", "z", "blend",
                                   "tex0", "afterTonemap", "ps", "vdecl", "prims" };

void window_reset() {
    memset(g_row, 0, sizeof(g_row));
    g_rowsUsed = 0; g_rowOverflow = 0;
    g_winPresents = 0; g_winDraws = 0; g_winTonemapDraws = 0;
    g_winTmFirstOrdSum = g_winTmLastOrdSum = g_winTmPresents = 0;
    g_winOrdSum = 0; g_winRtSampleAfterCand = 0; g_winKilled = 0;
    g_prAtWinStart = g_postRender ? g_postRender() : 0;
    g_vdAtWinStart = g_viewportDraws ? g_viewportDraws() : 0;
    memset(g_winByEntry, 0, sizeof(g_winByEntry));
    g_psHash.clear(); g_texIsRt.clear(); g_surfSize.clear();
    g_psDistinct = 0;
    g_winStartMs = GetTickCount();
}

int cmp_rows(const void* a, const void* b) {
    const Row* x = (const Row*)a; const Row* y = (const Row*)b;
    return x->draws == y->draws ? 0 : (x->draws < y->draws ? 1 : -1);
}

// The lever's state machine. Every refusal names itself and its values, and an
// ask that arrives before the device does is ARMED, not lost: the ini is read
// from DllMain, long before CreateDevice.
void apply_wanted(const char* why) {
    if (g_wanted == g_track) return;
    if (g_wanted) {
        if (!g_hooksOk) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Info,
                         "draws: census ARMED (%s) - it starts when the device is created and the "
                         "draw hooks install", why);
            return;
        }
        if (g_threadRefused) {
            DVR_WARN("draws: census REFUSED (%s) - the draws run on thread %lu and Present on "
                     "thread %lu, and the census keeps no lock", why,
                     (unsigned long)g_drawTid, (unsigned long)GetCurrentThreadId());
            return;
        }
        window_reset();
        g_threadAsking = true;
    }
    g_track = g_wanted;
    DVR_INFO("draws: census %s (%s)%s",
             g_track ? "ON - a summary and a VERDICT every 3 s" : "off", why,
             g_track ? " - this costs one bucket lookup per draw, so turn it off when you are done"
                     : "");
}

} // namespace

// ---------------------------------------------------------------------------

void install(IDirect3DDevice9* dev) {
    if (!dev) return;
    void* old = PatchVtable(dev, 81, (void*)hkDrawPrimitive);
    if (old && !g_origDp) g_origDp = (PFN_DrawPrimitive)old;
    old = PatchVtable(dev, 82, (void*)hkDrawIndexedPrimitive);
    if (old && !g_origDip) g_origDip = (PFN_DrawIndexedPrimitive)old;
    old = PatchVtable(dev, 83, (void*)hkDrawPrimitiveUP);
    if (old && !g_origDpUp) g_origDpUp = (PFN_DrawPrimitiveUP)old;
    old = PatchVtable(dev, 84, (void*)hkDrawIndexedPrimitiveUP);
    if (old && !g_origDipUp) g_origDipUp = (PFN_DrawIndexedPrimitiveUP)old;
    old = PatchVtable(dev, 47, (void*)hkSetViewport);
    if (old && !g_origSetVp) g_origSetVp = (PFN_SetViewport)old;
    old = PatchVtable(dev, 57, (void*)hkSetRenderState);
    if (old && !g_origSetRs) g_origSetRs = (PFN_SetRenderState)old;
    old = PatchVtable(dev, 65, (void*)hkSetTexture);
    if (old && !g_origSetTex) g_origSetTex = (PFN_SetTexture)old;
    old = PatchVtable(dev, 87, (void*)hkSetVertexDeclaration);
    if (old && !g_origSetDecl) g_origSetDecl = (PFN_SetVertexDeclaration)old;
    old = PatchVtable(dev, 92, (void*)hkSetVertexShader);
    if (old && !g_origSetVs) g_origSetVs = (PFN_SetVertexShader)old;
    old = PatchVtable(dev, 107, (void*)hkSetPixelShader);
    if (old && !g_origSetPs) g_origSetPs = (PFN_SetPixelShader)old;
    old = PatchVtable(dev, 59, (void*)hkCreateStateBlock);
    if (old && !g_origCreateSb) g_origCreateSb = (PFN_CreateStateBlock)old;
    old = PatchVtable(dev, 61, (void*)hkEndStateBlock);
    if (old && !g_origEndSb) g_origEndSb = (PFN_EndStateBlock)old;
    g_hooksOk = g_origDp && g_origDip && g_origDpUp && g_origDipUp && g_origSetVp &&
                g_origSetRs && g_origSetTex && g_origSetDecl && g_origSetVs && g_origSetPs;
    DVR_INFO("draws: hooks %s on device %p (Draw x4, SetViewport, SetRenderState, SetTexture, "
             "SetVertexDeclaration, SetVertexShader, SetPixelShader, state blocks; SetRenderTarget "
             "is observed through frame_hooks). Forward-only until the census lever is on",
             g_hooksOk ? "installed" : "PARTIAL - the census will refuse", dev);
    apply_wanted("armed by the ini");
}

void set_game_counters(uint32_t (*viewportDraws)(), uint32_t (*postRenderDispatches)()) {
    g_viewportDraws = viewportDraws;
    g_postRender = postRenderDispatches;
    g_prLast = g_postRender ? g_postRender() : 0;
}

void on_set_render_target(DWORD idx, IDirect3DSurface9* rt) {
    if (idx == 0) g_rt0 = rt;   // pointer VALUE; no reference is taken
}

void present_tick(IDirect3DDevice9* dev) {
    // The assumption this module is built on, checked every present and able to
    // fail: if the draws arrive on a thread other than the presenting one, the
    // unlocked state below is a race and the census must not run.
    const DWORD tid = GetCurrentThreadId();
    const DWORD drew = g_lastDrawTid;
    if (drew && drew != tid) {
        g_threadAsking = false;
        if (!g_threadRefused) {
            g_threadRefused = true;
            g_track = false;
            DVR_WARN("draws: REFUSED - the D3D draws arrive on thread %lu but Present runs on "
                     "thread %lu. The census keeps no lock because it assumes one thread, so it "
                     "will not run. (A HUD redirect would have the same problem.)",
                     (unsigned long)drew, (unsigned long)tid);
        }
    } else if (drew) {
        g_threadAsking = false;
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Info,
                     "draws: the draw thread IS the present thread (%lu) - the census may keep its "
                     "state unlocked", (unsigned long)tid);
    }
    g_drawTid = drew;

    if (!g_track) return;

    // Close the present that just ended.
    if (g_ord) {
        ++g_winPresents;
        g_winOrdSum += g_ord;
        if (g_tmSeen) { ++g_winTmPresents; g_winTmFirstOrdSum += g_tmFirstOrd; g_winTmLastOrdSum += g_tmLastOrd; }
    }
    ++g_presentNo;
    g_ord = 0; g_tmSeen = false; g_tmFirstOrd = g_tmLastOrd = 0; g_candSeen = false;
    if (g_postRender) {
        const uint32_t pr = g_postRender();
        g_prDeltaLast = pr - g_prLast;
        g_prLast = pr;
    }

    // The backbuffer's identity for the next present. GetBackBuffer hands back a
    // reference; it is released inside this call, as capture::grab does.
    if (dev) {
        IDirect3DSurface9* bb = nullptr;
        if (SUCCEEDED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) && bb) {
            D3DSURFACE_DESC d;
            if (SUCCEEDED(bb->GetDesc(&d))) { g_bbW = d.Width; g_bbH = d.Height; }
            g_bbPtr = bb;
            bb->Release();
        }
    }

    if (GetTickCount() - g_winStartMs >= 3000) log_summary("3s");
}

void on_reset() {
    g_stateBlocksCreated = 0;
    g_threadAsking = !g_threadRefused;   // a new device may draw from a new thread
    g_lastDrawTid = 0;
    g_rt0 = nullptr; g_bbPtr = nullptr; g_bbW = g_bbH = 0;
    g_vpKnown = false;
    g_ps = g_vs = g_vdecl = g_tex0 = nullptr;
    window_reset();
}

void shutdown() {
    g_threadAsking = false;
    g_track = false;
    g_killN = 0; g_killHud = false;
}

bool enabled() { return g_track; }

void set_enabled(bool on) {
    g_wanted = on;
    apply_wanted("asked");
}

void log_summary(const char* why) {
    if (!g_track) { DVR_INFO("draws: census is off ([Draws] Census=1 or `draws on`)"); return; }
    const uint32_t presents = g_winPresents ? g_winPresents : 1;
    const double perPresent = (double)g_winDraws / (double)presents;
    DVR_INFO("draws: %s: presents=%u draws/present=%.0f (DP %.0f, DIP %.0f, UP %.0f, IUP %.0f) "
             "buckets=%u (overflow %u) ps distinct=%u stateBlocks=%u tonemap draws/present=%.1f "
             "at ord %u..%u of %.0f",
             why, (unsigned)g_winPresents, perPresent,
             (double)g_winByEntry[0] / presents, (double)g_winByEntry[1] / presents,
             (double)g_winByEntry[2] / presents, (double)g_winByEntry[3] / presents,
             (unsigned)g_rowsUsed, (unsigned)g_rowOverflow, (unsigned)g_psDistinct,
             (unsigned)g_stateBlocksCreated, (double)g_winTonemapDraws / presents,
             (unsigned)(g_winTmPresents ? g_winTmFirstOrdSum / g_winTmPresents : 0),
             (unsigned)(g_winTmPresents ? g_winTmLastOrdSum / g_winTmPresents : 0),
             (double)g_winOrdSum / presents);
    {   // The HUD is drawn from inside PostRender, so this ratio says how many
        // times per engine tick the HUD is drawn: 2.0 under `stereo reentry`
        // (once per eye), 1.0 on the mono screen. A ratio near 0 means the
        // dispatch was never seen and the column is meaningless, which the line
        // says rather than printing a silent zero.
        const uint32_t pr = g_postRender ? g_postRender() - g_prAtWinStart : 0;
        const uint32_t vd = g_viewportDraws ? g_viewportDraws() - g_vdAtWinStart : 0;
        if (!g_postRender)
            DVR_INFO("draws: postRender: no counter registered (the game side did not hand one over)");
        else if (!pr)
            DVR_INFO("draws: postRender: 0 dispatches in the window - the HUD's own event never "
                     "fired, so nothing in this table was drawn from it");
        else
            DVR_INFO("draws: postRender %u dispatches, viewport draws %u -> %.2f per draw "
                     "(2.00 under reentry = the HUD is drawn once per EYE; 1.00 = once per tick). "
                     "Last present's delta %u, which is a game-thread count read a frame ahead of "
                     "these draws and is NOT aligned to this present",
                     (unsigned)pr, (unsigned)vd, vd ? (double)pr / (double)vd : 0.0,
                     (unsigned)g_prDeltaLast);
    }
    if (g_killN || g_killHud)
        DVR_INFO("draws: KILLING %s%s%d key(s): %.1f draws/present dropped - the picture is NOT "
                 "what the game drew", g_killHud ? "the HUD candidates" : "", g_killHud && g_killN ? " and " : "",
                 g_killN, (double)g_winKilled / presents);

    if (!g_winDraws) {
        DVR_INFO("draws: no draws in the window - the census sees nothing (is the game rendering?)");
        window_reset();
        return;
    }

    // Sort a copy so the table reads top-down by weight.
    static Row sorted[kRows];
    memcpy(sorted, g_row, sizeof(sorted));
    qsort(sorted, kRows, sizeof(Row), cmp_rows);

    char txt[160];
    uint32_t candBuckets = 0, candDraws = 0;
    for (int i = 0; i < kRows; i++)
        if (sorted[i].draws && is_candidate(sorted[i].sig)) { ++candBuckets; candDraws += sorted[i].draws; }

    for (int i = 0; i < kRows && i < 12; i++) {
        const Row& r = sorted[i];
        if (!r.draws) break;
        sig_text(r.sig, txt, sizeof(txt));
        DVR_INFO("draws:   %s k=%08x %s n=%.1f/present ord %u..%u in %u presents",
                 is_candidate(r.sig) ? "HUD?" : "    ", (unsigned)short_key(r.key), txt,
                 (double)r.draws / presents, (unsigned)r.minOrd, (unsigned)r.maxOrd,
                 (unsigned)r.presents);
    }

    // The separator search, and the near misses: a bucket that fails the rule
    // in exactly ONE term is what a redirect would mis-route.
    ValSet cand[10], rest[10];
    uint32_t restBuckets = 0;
    for (int i = 0; i < kRows; i++) {
        const Row& r = g_row[i];
        if (!r.draws) continue;
        const bool c = is_candidate(r.sig);
        if (!c) ++restBuckets;
        for (int col = 0; col < 10; col++) (c ? cand[col] : rest[col]).add(column(r.sig, col));
    }
    char seps[192] = "";
    for (int col = 0; col < 10; col++) {
        if (!cand[col].disjoint(rest[col])) continue;
        if (seps[0]) strncat(seps, ", ", sizeof(seps) - strlen(seps) - 1);
        strncat(seps, kColName[col], sizeof(seps) - strlen(seps) - 1);
    }

    if (!candBuckets) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO HUD-CLASS DRAWS: nothing in %u buckets is a user-pointer draw to the whole "
                  "backbuffer with depth off sampling a plain texture after the tonemap",
                  (unsigned)g_rowsUsed);
    } else if (!restBuckets) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO CLEAN SEPARATOR: EVERY bucket in the window is a candidate (%u), so the rule "
                  "does not separate anything - it would put the world on the panel",
                  (unsigned)candBuckets);
    } else if (!seps[0]) {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "NO CLEAN SEPARATOR: %u candidate buckets, but no single column separates them "
                  "from the other %u; only the conjunction does, and it is the rule itself",
                  (unsigned)candBuckets, (unsigned)restBuckets);
    } else {
        _snprintf(g_verdict, sizeof(g_verdict),
                  "HUD candidates %u buckets, %.1f draws/present of %.0f; separators with NO "
                  "overlap: %s", (unsigned)candBuckets, (double)candDraws / presents, perPresent, seps);
    }
    g_verdict[sizeof(g_verdict) - 1] = 0;
    g_candBuckets = candBuckets;
    g_candPerPresent = (double)candDraws / presents;
    DVR_INFO("draws: VERDICT: %s | rtSampleAfterCandidate=%u (0 = the tonemap never runs after a "
             "candidate, so after-first equals after-last)",
             g_verdict, (unsigned)g_winRtSampleAfterCand);

    for (int i = 0, shown = 0; i < kRows && shown < 5; i++) {
        const Row& r = sorted[i];
        if (!r.draws || is_candidate(r.sig)) continue;
        bool t[kTermCount]; sig_terms(r.sig, t);
        int missing = -1, misses = 0;
        for (int k = 0; k < kTermCount; k++) if (!t[k]) { missing = k; ++misses; }
        if (misses != 1) continue;
        ++shown;
        sig_text(r.sig, txt, sizeof(txt));
        DVR_INFO("draws:   NEAR MISS (fails only %s): %s n=%.1f/present", kTermName[missing], txt,
                 (double)r.draws / presents);
    }
    window_reset();
}

void status(dvr::status::Writer& w) {
    w.kv("on", g_track);
    w.kv("hooks", g_hooksOk);
    w.kv("threadOk", !g_threadRefused);
    w.kv("candBuckets", (unsigned long)g_candBuckets);
    w.kv("candPerPresent", g_candPerPresent);
    w.kv("stateBlocks", (unsigned long)g_stateBlocksCreated);
    w.kv("killHud", g_killHud);
    w.kv("killKeys", (int)g_killN);
    w.kv("postRender", (unsigned long)(g_postRender ? g_postRender() : 0));
    w.kv("verdict", g_verdict);
}

bool command(const char* args) {
    if (!strcmp(args, "on"))  { set_enabled(true);  return true; }
    if (!strcmp(args, "off")) { set_enabled(false); return true; }
    if (!strcmp(args, "unkill")) {
        g_killN = 0; g_killHud = false;
        DVR_INFO("draws: kill list cleared - the game's own draws again");
        return true;
    }
    if (!strncmp(args, "kill", 4)) {
        const char* a = args + 4;
        while (*a == ' ') ++a;
        if (!g_track) {
            DVR_WARN("draws: kill needs the census on (the buckets are what it kills) - `draws on`");
            return true;
        }
        if (!strcmp(a, "hud")) {
            g_killHud = true;
            DVR_INFO("draws: kill HUD armed - every candidate draw is dropped. Take `dump capture` "
                     "before and after: if exactly the HUD went and the world did not, the rule "
                     "found the HUD; if any world geometry went, it did not");
            return true;
        }
        const uint32_t k = (uint32_t)strtoul(a, nullptr, 16);
        if (!k) {
            DVR_WARN("draws: kill wants a bucket key from the table (`draws kill 1a2b3c4d`) or "
                     "`draws kill hud`; got '%s'", a);
            return true;
        }
        if (g_killN >= kKills) {
            DVR_WARN("draws: kill list is full (%d) - `draws unkill` first", kKills);
            return true;
        }
        g_kill[g_killN++] = k;
        DVR_INFO("draws: kill %08x armed (%d in the list) - dump a capture before and after",
                 (unsigned)k, g_killN);
        return true;
    }
    log_summary("status");
    return true;
}

} // namespace dvr::draws
