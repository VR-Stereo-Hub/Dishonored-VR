// core/gfx/draw_census.cpp - see draw_census.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/draw_census.h"

#include "core/framework/frame_hooks.h"
#include "core/hooks/vtable.h"
#include "core/util/log.h"

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

inline void note_draw() {
    if (g_threadAsking) g_lastDrawTid = GetCurrentThreadId();
}

HRESULT __stdcall hkDrawPrimitive(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT start,
                                  UINT prims) {
    note_draw();
    return g_origDp(self, type, start, prims);
}

HRESULT __stdcall hkDrawIndexedPrimitive(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, INT base,
                                         UINT minIdx, UINT numVerts, UINT startIdx, UINT prims) {
    note_draw();
    return g_origDip(self, type, base, minIdx, numVerts, startIdx, prims);
}

HRESULT __stdcall hkDrawPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type, UINT prims,
                                    const void* verts, UINT stride) {
    note_draw();
    return g_origDpUp(self, type, prims, verts, stride);
}

HRESULT __stdcall hkDrawIndexedPrimitiveUP(IDirect3DDevice9* self, D3DPRIMITIVETYPE type,
                                           UINT minIdx, UINT numVerts, UINT prims,
                                           const void* idxData, D3DFORMAT idxFmt,
                                           const void* verts, UINT stride) {
    note_draw();
    return g_origDipUp(self, type, minIdx, numVerts, prims, idxData, idxFmt, verts, stride);
}

HRESULT __stdcall hkSetViewport(IDirect3DDevice9* self, const D3DVIEWPORT9* vp) {
    return g_origSetVp(self, vp);
}

HRESULT __stdcall hkSetRenderState(IDirect3DDevice9* self, D3DRENDERSTATETYPE state, DWORD value) {
    return g_origSetRs(self, state, value);
}

HRESULT __stdcall hkSetTexture(IDirect3DDevice9* self, DWORD stage, IDirect3DBaseTexture9* tex) {
    return g_origSetTex(self, stage, tex);
}

HRESULT __stdcall hkSetVertexDeclaration(IDirect3DDevice9* self,
                                         IDirect3DVertexDeclaration9* decl) {
    return g_origSetDecl(self, decl);
}

HRESULT __stdcall hkSetVertexShader(IDirect3DDevice9* self, IDirect3DVertexShader9* vs) {
    return g_origSetVs(self, vs);
}

HRESULT __stdcall hkSetPixelShader(IDirect3DDevice9* self, IDirect3DPixelShader9* ps) {
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

} // namespace

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
    const bool all = g_origDp && g_origDip && g_origDpUp && g_origDipUp && g_origSetVp &&
                     g_origSetRs && g_origSetTex && g_origSetDecl && g_origSetVs && g_origSetPs;
    DVR_INFO("draws: hooks %s on device %p (Draw x4, SetViewport, SetRenderState, SetTexture, "
             "SetVertexDeclaration, SetVertexShader, SetPixelShader, state blocks; SetRenderTarget "
             "is observed through frame_hooks). Forward-only until the census lever is on",
             all ? "installed" : "PARTIAL - the census will refuse", dev);
}

void on_set_render_target(DWORD idx, IDirect3DSurface9* rt) {
    (void)idx; (void)rt;   // the census reads RT0 from here once it tracks
}

void present_tick(IDirect3DDevice9* dev) {
    (void)dev;
    // The assumption this module is built on, checked every present and able to
    // fail: if the draws arrive on a thread other than the presenting one, the
    // unlocked state below is a race and the census must not run.
    const DWORD tid = GetCurrentThreadId();
    const DWORD drew = g_lastDrawTid;
    if (drew && drew != tid) {
        g_threadAsking = false;
        if (!g_threadRefused) {
            g_threadRefused = true;
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
}

void on_reset() {
    g_stateBlocksCreated = 0;
    g_threadAsking = !g_threadRefused;   // a new device may draw from a new thread
    g_lastDrawTid = 0;
}

void shutdown() {
    g_threadAsking = false;
}

} // namespace dvr::draws
