// core/gfx/capture.cpp - see capture.h.
//
// Three modes ([Capture] Mode=, `capture mode <m>` live, default sync):
//   sync      GetRenderTargetData(backbuffer) every present: the CPU waits for
//             the GPU to finish the frame in flight, then copies it up. The
//             path every build since 30.x shipped; the A/B baseline.
//   deferred  StretchRect(backbuffer -> a default-pool copy) and queue its
//             GetRenderTargetData this present, LOCK the previous present's
//             readback. MEASURED (runs 16-18): GetRenderTargetData itself
//             returns in ~3 us; it is LockRect that waits (2.4-3.1 ms of the
//             5 ms) because the readback is queued behind the frame in
//             flight, and reading back the previous copy while locking it in
//             the same present still waits (run 18). Locking one present
//             after the readback was queued is what removes the wait; the
//             StretchRect also resolves a multisampled backbuffer (the run 6
//             failure). The frame reaches the headset one present late; the
//             eye tag a stereo method attached travels with the slot so tags
//             and pixels stay paired (delivered_tag()).
// (A fourth path, a SYSTEMMEM surface over the mod's own buffer so the row
// copy disappears, was tried in run 17: CreateOffscreenPlainSurface refuses
// the user-memory pointer with D3DERR_INVALIDCALL on this runtime. Recorded in
// ENGINE_NOTES, not kept as a mode.)
//   shared    the D3D9 surface opened on the D3D11 side (only when the probe
//             said AVAILABLE): StretchRect into it and sample it directly, no
//             CPU round trip; a D3D9 event query is the fence, checked (never
//             waited on) at the next present. The bbox instrument samples a
//             readback of it every 3 s instead of every present.
// A mode that cannot run (shared on a device that cannot share) logs why and
// leaves the previous mode running - fail soft, like the stereo methods.
#define DVR_CAT ::dvr::log::Cat::capture
#include "core/gfx/capture.h"

#include "core/framework/perf.h"
#include "core/util/log.h"

#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::capture {
namespace {

// ---- the frame ----------------------------------------------------------------
IDirect3DSurface9*        g_sysmem = nullptr;   // D3D9 system-memory readback target
uint8_t*                  g_pixels = nullptr;   // cached heap copy, BGRA
uint32_t                  g_w = 0, g_h = 0;
D3DFORMAT                 g_fmt = D3DFMT_UNKNOWN;
ID3D11Texture2D*          g_tex = nullptr;      // the uploaded frame (sync, deferred)
ID3D11ShaderResourceView* g_srv = nullptr;
uint32_t                  g_texW = 0, g_texH = 0;
uint32_t                  g_grabs = 0;
Bbox                      g_bbox;
bool                      g_warnedFormat = false;
bool                      g_warnedRtd = false;
bool                      g_bboxSaidForSize = false;
int                       g_bboxClass = -1;   // 0 all black, 1 cropped, 2 full

// ---- the mode -----------------------------------------------------------------
Mode g_mode = Mode::Sync;
Mode g_modeWant = Mode::Sync;
const char* const kModeNames[] = {"sync", "deferred", "shared"};

// The eye tag a method attaches to the present being grabbed, and the tag of
// the content the last grab actually delivered (equal except under deferred).
int      g_pendingTag = 0;
int      g_deliveredTag = 0;
uint32_t g_serial = 0;            // grab serial (the present the content came from)
uint32_t g_deliveredSerial = 0;

// deferred: two default-pool copies and two readback surfaces, alternating:
// slot i is blitted + read back (queued) at present N, locked at present N+1
IDirect3DSurface9* g_rt[2] = {nullptr, nullptr};
IDirect3DSurface9* g_sys[2] = {nullptr, nullptr};
bool     g_rtValid[2] = {false, false};
int      g_rtTag[2] = {0, 0};
uint32_t g_rtSerial[2] = {0, 0};
int      g_rtCur = 0;

// shared: the D3D9 surface, its D3D11 view, the fence
IDirect3DSurface9*        g_sharedRt = nullptr;
ID3D11Texture2D*          g_sharedTex = nullptr;
ID3D11ShaderResourceView* g_sharedSrv = nullptr;
IDirect3DQuery9*          g_fence = nullptr;
bool                      g_fenceIssued = false;
uint32_t                  g_fenceLate = 0;
uint64_t                  g_sharedBboxMs = 0;

// ---- the cost window ----------------------------------------------------------
// Sums of the phases over the grabs since the last 3 s line, and the averages
// that line published (what cost() returns).
long long g_qpcFreq = 0;
uint64_t  g_sumRtd = 0, g_sumLock = 0, g_sumCopy = 0, g_sumUpload = 0, g_sumBlit = 0;
uint32_t  g_windowGrabs = 0;
uint64_t  g_windowMs = 0;
Cost      g_cost;
Cost      g_last;      // this present's grab (zeros when nothing was grabbed)

inline long long qpc_now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
inline uint64_t qpc_us(long long from, long long to) {
    if (!g_qpcFreq) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_qpcFreq = f.QuadPart ? f.QuadPart : 1;
    }
    return (uint64_t)((to - from) * 1000000 / g_qpcFreq);
}

// Close the window every 3 s: publish the averages and print them. The line
// carries the mode, the frame size and the bytes moved so the number can be
// read as a bandwidth as well as a stall.
void cost_tick() {
    const uint64_t now = GetTickCount64();
    if (g_windowMs == 0) { g_windowMs = now; return; }
    if (now - g_windowMs < 3000) return;
    if (g_windowGrabs) {
        g_cost.rtdUs = (uint32_t)(g_sumRtd / g_windowGrabs);
        g_cost.lockUs = (uint32_t)(g_sumLock / g_windowGrabs);
        g_cost.copyUs = (uint32_t)(g_sumCopy / g_windowGrabs);
        g_cost.uploadUs = (uint32_t)(g_sumUpload / g_windowGrabs);
        g_cost.blitUs = (uint32_t)(g_sumBlit / g_windowGrabs);
        g_cost.totalUs = g_cost.rtdUs + g_cost.lockUs + g_cost.copyUs + g_cost.uploadUs + g_cost.blitUs;
        g_cost.grabsInWindow = g_windowGrabs;
        DVR_INFO("capture: cost/present rtd=%u lock=%u copy=%u upload=%u blit=%u total=%u us (%u grabs in "
                 "%.1f s, mode=%s, %ux%u, %.1f MB each way%s)",
                 g_cost.rtdUs, g_cost.lockUs, g_cost.copyUs, g_cost.uploadUs, g_cost.blitUs, g_cost.totalUs,
                 g_windowGrabs, (double)(now - g_windowMs) / 1000.0, kModeNames[(int)g_mode], g_w, g_h,
                 (double)g_w * (double)g_h * 4.0 / (1024.0 * 1024.0),
                 g_mode == Mode::Shared ? "; shared: no CPU copy, rtd is the 3 s bbox sample" : "");
        if (g_mode == Mode::Shared && g_fenceLate)
            DVR_INFO("capture: shared fence late %u times this window (the previous blit had not "
                     "finished when the next present began; counted, never waited on)", g_fenceLate);
        g_fenceLate = 0;
    }
    g_sumRtd = g_sumLock = g_sumCopy = g_sumUpload = g_sumBlit = 0;
    g_windowGrabs = 0;
    g_windowMs = now;
}

// ---- the bbox instrument ------------------------------------------------------
// Strided sample of the CPU pixels: every 8th row and column, a pixel counts
// as content when any channel clears 8/255. Cheap (1/64 of the frame) and
// enough to place the box within 8 px, which is what the diagnosis needs.
void sample_bbox() {
    const uint32_t step = 8;
    uint32_t minX = g_w, minY = g_h, maxX = 0, maxY = 0;
    uint32_t hits = 0, total = 0;
    for (uint32_t y = 0; y < g_h; y += step) {
        const uint8_t* row = g_pixels + (size_t)y * g_w * 4;
        for (uint32_t x = 0; x < g_w; x += step) {
            const uint8_t* p = row + (size_t)x * 4;
            ++total;
            if (p[0] > 8 || p[1] > 8 || p[2] > 8) {
                ++hits;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }
    Bbox b;
    b.valid = hits > 0;
    if (b.valid) {
        b.x0 = minX; b.y0 = minY; b.x1 = maxX; b.y1 = maxY;
        b.pctW = 100.0f * (float)(maxX - minX + step) / (float)g_w;
        b.pctH = 100.0f * (float)(maxY - minY + step) / (float)g_h;
        if (b.pctW > 100.0f) b.pctW = 100.0f;
        if (b.pctH > 100.0f) b.pctH = 100.0f;
    }
    b.nonBlackPct = total ? 100.0f * (float)hits / (float)total : 0.0f;
    g_bbox = b;
    const bool full = b.valid && b.pctW >= 97.0f && b.pctH >= 97.0f;
    const int cls = !b.valid ? 0 : full ? 2 : 1;
    if (!g_bboxSaidForSize || cls != g_bboxClass) {
        g_bboxSaidForSize = true;
        g_bboxClass = cls;
        DVR_INFO("capture: %ux%u content bbox [%u,%u]-[%u,%u] = %.0f%% x %.0f%% (%s), "
                 "%.0f%% of samples non-black",
                 g_w, g_h, b.x0, b.y0, b.x1, b.y1, b.pctW, b.pctH,
                 !b.valid ? "ALL BLACK" : full ? "FULL" : "CROPPED", b.nonBlackPct);
    } else {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 10000,
                         "capture: %ux%u content bbox [%u,%u]-[%u,%u] = %.0f%% x %.0f%% (%s), "
                         "%.0f%% non-black",
                         g_w, g_h, b.x0, b.y0, b.x1, b.y1, b.pctW, b.pctH,
                         !b.valid ? "ALL BLACK" : full ? "FULL" : "CROPPED", b.nonBlackPct);
    }
}

// ---- the shared-surface probe -------------------------------------------------
// Can the GAME's D3D9 device hand the D3D11 side a surface without a CPU
// round trip? D3D9 shares resources only under D3D9Ex (a plain IDirect3D9
// device refuses a non-null pSharedHandle), and a 9Ex device refuses
// D3DPOOL_MANAGED, which UE3's D3D9 RHI depends on - so the expected answer on
// this game is REFUSED, and the probe is what makes that a measured fact
// instead of a belief. Runs once at the first grab; every HRESULT is logged;
// the verdict line names the path a cheaper capture can take.
bool g_probed = false;
bool g_sharedOk = false;

void probe_shared(IDirect3DDevice9* dev, ID3D11Device* dev11) {
    if (g_probed) return;
    g_probed = true;
    IDirect3DDevice9Ex* ex = nullptr;
    const bool isEx = SUCCEEDED(dev->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&ex)) && ex;
    if (ex) ex->Release();
    DVR_INFO("capture/probe: the game's device %s IDirect3DDevice9Ex (%s)",
             isEx ? "IS" : "is NOT", isEx ? "shared surfaces are a D3D9Ex feature: possible"
                                          : "created through Direct3DCreate9; D3D9 shares only under 9Ex");
    IDirect3DSurface9* rt = nullptr;
    HANDLE shared = nullptr;
    const HRESULT hr = dev->CreateRenderTarget(g_w, g_h, g_fmt, D3DMULTISAMPLE_NONE, 0, FALSE, &rt, &shared);
    if (FAILED(hr) || !rt) {
        DVR_INFO("capture/probe: CreateRenderTarget %ux%u fmt=%d with a shared handle -> 0x%08lx%s",
                 g_w, g_h, (int)g_fmt, (unsigned long)hr,
                 hr == D3DERR_INVALIDCALL ? " (D3DERR_INVALIDCALL: the runtime refuses pSharedHandle on this device)" : "");
        DVR_INFO("capture/probe: shared surface REFUSED - the D3D9 device cannot share; the CPU "
                 "readback stays, [Capture] Mode=deferred is the cheaper path (ROADMAP S1)");
        return;
    }
    DVR_INFO("capture/probe: CreateRenderTarget with a shared handle -> OK, handle=%p", shared);
    ID3D11Texture2D* tex = nullptr;
    const HRESULT hr2 = shared ? dev11->OpenSharedResource(shared, __uuidof(ID3D11Texture2D), (void**)&tex)
                               : E_HANDLE;
    if (FAILED(hr2) || !tex) {
        DVR_INFO("capture/probe: OpenSharedResource on the mod's D3D11 device -> 0x%08lx", (unsigned long)hr2);
        DVR_INFO("capture/probe: shared surface REFUSED - D3D9 created it but D3D11 cannot open it; "
                 "[Capture] Mode=deferred is the cheaper path");
    } else {
        D3D11_TEXTURE2D_DESC td = {};
        tex->GetDesc(&td);
        g_sharedOk = true;
        DVR_INFO("capture/probe: shared surface AVAILABLE - D3D9 %ux%u opened as D3D11 %ux%u fmt=%d "
                 "(no CPU round trip: [Capture] Mode=shared)",
                 g_w, g_h, td.Width, td.Height, (int)td.Format);
        tex->Release();
    }
    rt->Release();
}

// ---- resources per mode -------------------------------------------------------
bool ensure_texture(ID3D11Device* dev) {
    if (g_tex && g_texW == g_w && g_texH == g_h) return true;
    if (g_srv) { g_srv->Release(); g_srv = nullptr; }
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = g_w; td.Height = g_h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // D3D9 X8R8G8B8 byte order
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_tex)) ||
        FAILED(dev->CreateShaderResourceView(g_tex, nullptr, &g_srv))) {
        DVR_ERROR("capture: D3D11 texture %ux%u failed - nothing reaches the headset", g_w, g_h);
        if (g_tex) { g_tex->Release(); g_tex = nullptr; }
        return false;
    }
    g_texW = g_w; g_texH = g_h;
    DVR_INFO("capture: D3D11 game texture %ux%u (BGRA)", g_w, g_h);
    return true;
}

void release_deferred() {
    for (int i = 0; i < 2; ++i) {
        if (g_rt[i]) { g_rt[i]->Release(); g_rt[i] = nullptr; }
        if (g_sys[i]) { g_sys[i]->Release(); g_sys[i] = nullptr; }
        g_rtValid[i] = false;
    }
    g_rtCur = 0;
}

void release_shared() {
    if (g_sharedSrv) { g_sharedSrv->Release(); g_sharedSrv = nullptr; }
    if (g_sharedTex) { g_sharedTex->Release(); g_sharedTex = nullptr; }
    if (g_fence) { g_fence->Release(); g_fence = nullptr; }
    if (g_sharedRt) { g_sharedRt->Release(); g_sharedRt = nullptr; }
    g_fenceIssued = false;
}

bool ensure_deferred(IDirect3DDevice9* dev) {
    for (int i = 0; i < 2; ++i) {
        if (g_rt[i] && g_sys[i]) continue;
        if (FAILED(dev->CreateRenderTarget(g_w, g_h, g_fmt, D3DMULTISAMPLE_NONE, 0, FALSE, &g_rt[i], nullptr)) ||
            !g_rt[i] ||
            FAILED(dev->CreateOffscreenPlainSurface(g_w, g_h, g_fmt, D3DPOOL_SYSTEMMEM, &g_sys[i], nullptr)) ||
            !g_sys[i]) {
            DVR_ERROR("capture: deferred copy target / readback surface %ux%u failed - back to sync", g_w, g_h);
            release_deferred();
            return false;
        }
        g_rtValid[i] = false;
    }
    return true;
}

bool ensure_shared(IDirect3DDevice9* dev, ID3D11Device* dev11) {
    if (g_sharedRt && g_sharedTex && g_sharedSrv && g_fence) return true;
    release_shared();
    HANDLE shared = nullptr;
    HRESULT hr = dev->CreateRenderTarget(g_w, g_h, g_fmt, D3DMULTISAMPLE_NONE, 0, FALSE, &g_sharedRt, &shared);
    if (FAILED(hr) || !g_sharedRt || !shared) {
        DVR_ERROR("capture: shared render target %ux%u failed (0x%08lx) - back to sync", g_w, g_h, (unsigned long)hr);
        release_shared();
        return false;
    }
    hr = dev11->OpenSharedResource(shared, __uuidof(ID3D11Texture2D), (void**)&g_sharedTex);
    if (FAILED(hr) || !g_sharedTex ||
        FAILED(dev11->CreateShaderResourceView(g_sharedTex, nullptr, &g_sharedSrv))) {
        DVR_ERROR("capture: opening the shared surface on D3D11 failed (0x%08lx) - back to sync", (unsigned long)hr);
        release_shared();
        return false;
    }
    if (FAILED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &g_fence)) || !g_fence) {
        DVR_ERROR("capture: the event query for the shared surface failed - back to sync");
        release_shared();
        return false;
    }
    D3D11_TEXTURE2D_DESC td = {};
    g_sharedTex->GetDesc(&td);
    DVR_INFO("capture: shared surface %ux%u live - D3D11 sees fmt=%d; no CPU copy per present, the "
             "bbox samples a readback every 3 s", g_w, g_h, (int)td.Format);
    return true;
}

// GetRenderTargetData(src -> dst): the rtd phase. The call returns at once;
// the copy is queued on the GPU behind everything submitted before it.
bool read_back_queue(IDirect3DDevice9* dev, IDirect3DSurface9* src, IDirect3DSurface9* dst, uint64_t* rtdUs) {
    const long long t0 = qpc_now();
    dvr::perf::gpu_mark(dvr::perf::kGpuRtdA);   // the readback copy's own GPU time (perf)
    const HRESULT hr = dev->GetRenderTargetData(src, dst);
    dvr::perf::gpu_mark(dvr::perf::kGpuRtdB);
    if (rtdUs) *rtdUs = qpc_us(t0, qpc_now());
    if (FAILED(hr)) {
        if (!g_warnedRtd) {
            g_warnedRtd = true;
            DVR_ERROR("capture: GetRenderTargetData failed (0x%08lx) - multisampled backbuffer? "
                      "(the game's AA setting; `capture mode deferred` resolves it); the headset gets nothing",
                      (unsigned long)hr);
        }
        return false;
    }
    return true;
}

// Lock a readback surface (this is where the CPU waits for the queued copy)
// and row-copy it into g_pixels: the lock and copy phases.
bool lock_copy(IDirect3DSurface9* sys, uint64_t* lockUs, uint64_t* copyUs) {
    const long long t1 = qpc_now();
    D3DLOCKED_RECT lr;
    if (FAILED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;
    const long long tl = qpc_now();
    // Straight per-row copy into cached memory: the system surface is slow to
    // read more than once (measured 0.7 ms per 8 MB here).
    const size_t rowBytes = (size_t)g_w * 4;
    for (uint32_t y = 0; y < g_h; ++y)
        memcpy(g_pixels + y * rowBytes, (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch, rowBytes);
    sys->UnlockRect();
    const long long t2 = qpc_now();
    if (lockUs) *lockUs = qpc_us(t1, tl);
    if (copyUs) *copyUs = qpc_us(tl, t2);
    return true;
}

// The synchronous readback: queue and lock in the same present.
bool read_back(IDirect3DDevice9* dev, IDirect3DSurface9* src, uint64_t* rtdUs, uint64_t* lockUs, uint64_t* copyUs) {
    return read_back_queue(dev, src, g_sysmem, rtdUs) && lock_copy(g_sysmem, lockUs, copyUs);
}

// Apply a queued mode change at the top of a grab (present thread).
void apply_mode_want(IDirect3DDevice9* dev, ID3D11Device* dev11) {
    (void)dev; (void)dev11;
    if (g_modeWant == g_mode) return;
    if (g_modeWant == Mode::Shared && g_probed && !g_sharedOk) {
        DVR_WARN("capture: mode shared refused - the probe said the device cannot share; staying on %s",
                 kModeNames[(int)g_mode]);
        g_modeWant = g_mode;
        return;
    }
    release_deferred();
    release_shared();
    DVR_INFO("capture: mode %s -> %s%s", kModeNames[(int)g_mode], kModeNames[(int)g_modeWant],
             g_modeWant == Mode::Deferred
                 ? " (the frame reaches the headset one present late; the readback waits on the "
                   "previous present's copy, not the frame in flight)"
                 : g_modeWant == Mode::Shared ? " (no CPU round trip)"
                                              : " (the readback is queued and locked in the same present)");
    g_mode = g_modeWant;
}

} // namespace

bool grab(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx) {
    g_last = Cost();
    if (!dev || !dev11 || !ctx) return false;
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    D3DSURFACE_DESC desc;
    bb->GetDesc(&desc);
    if (desc.Format != D3DFMT_X8R8G8B8 && desc.Format != D3DFMT_A8R8G8B8) {
        if (!g_warnedFormat) {
            g_warnedFormat = true;
            DVR_ERROR("capture: backbuffer format %d is not handled (X8R8G8B8/A8R8G8B8 only) - "
                      "the headset gets nothing", (int)desc.Format);
        }
        bb->Release();
        return false;
    }
    if (!g_sysmem || desc.Width != g_w || desc.Height != g_h || desc.Format != g_fmt) {
        if (g_sysmem) { g_sysmem->Release(); g_sysmem = nullptr; }
        release_deferred();
        release_shared();
        free(g_pixels); g_pixels = nullptr;
        if (FAILED(dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format,
                                                    D3DPOOL_SYSTEMMEM, &g_sysmem, nullptr))) {
            DVR_ERROR("capture: system-memory surface %ux%u failed", desc.Width, desc.Height);
            bb->Release();
            return false;
        }
        // 40.1: a size change after the first is the most consequential event
        // in a session - every downstream size derives from this one - and it
        // used to log as a repeat of the same line. Name it, at Warn.
        const uint32_t oldW = g_w, oldH = g_h;
        g_w = desc.Width; g_h = desc.Height; g_fmt = desc.Format;
        g_pixels = (uint8_t*)malloc((size_t)g_w * g_h * 4);
        g_bboxSaidForSize = false;
        if (oldW && oldH && (oldW != g_w || oldH != g_h))
            DVR_WARN("capture: RESOLUTION CHANGED MID-SESSION %ux%u -> %ux%u - the frame the "
                     "game hands us just changed size; the eye swapchains rebuild at the new "
                     "size (expect a stall and a scale jump)", oldW, oldH, g_w, g_h);
        else
            DVR_INFO("capture: %ux%u fmt=%d mode=%s", g_w, g_h, (int)desc.Format, kModeNames[(int)g_mode]);
    }
    if (!g_pixels) { bb->Release(); return false; }
    probe_shared(dev, dev11);
    apply_mode_want(dev, dev11);
    if (g_mode == Mode::Shared && !g_sharedOk) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                     "capture: [Capture] Mode=shared but the probe said the device cannot share - running sync");
        g_mode = g_modeWant = Mode::Sync;
    }
    ++g_serial;
    const uint32_t thisSerial = g_serial;
    const int thisTag = g_pendingTag;
    g_pendingTag = 0;
    uint64_t rtdUs = 0, lockUs = 0, copyUs = 0, uploadUs = 0, blitUs = 0;
    bool delivered = false;

    if (g_mode == Mode::Sync) {
        const bool ok = read_back(dev, bb, &rtdUs, &lockUs, &copyUs);
        bb->Release();
        if (!ok) return false;
        if (!ensure_texture(dev11)) return false;
        const long long t0 = qpc_now();
        ctx->UpdateSubresource(g_tex, 0, nullptr, g_pixels, (UINT)g_w * 4, 0);
        uploadUs = qpc_us(t0, qpc_now());
        g_deliveredTag = thisTag; g_deliveredSerial = thisSerial;
        delivered = true;
        sample_bbox();
    } else if (g_mode == Mode::Deferred) {
        if (!ensure_deferred(dev)) { g_mode = g_modeWant = Mode::Sync; bb->Release(); return false; }
        const int cur = g_rtCur, prev = g_rtCur ^ 1;
        const long long t0 = qpc_now();
        dvr::perf::gpu_mark(dvr::perf::kGpuRtdA);   // deferred: the blit + the queued readback
        const HRESULT hr = dev->StretchRect(bb, nullptr, g_rt[cur], nullptr, D3DTEXF_NONE);
        blitUs = qpc_us(t0, qpc_now());
        bb->Release();
        if (FAILED(hr)) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Error,
                         "capture: StretchRect backbuffer -> copy target failed (0x%08lx) - the headset "
                         "gets nothing in deferred mode; `capture mode sync`", (unsigned long)hr);
            return false;
        }
        // Queue this present's readback now; it completes while the game
        // builds the next frame, and the lock below never meets it.
        if (!read_back_queue(dev, g_rt[cur], g_sys[cur], &rtdUs)) return false;
        g_rtValid[cur] = true; g_rtTag[cur] = thisTag; g_rtSerial[cur] = thisSerial;
        g_rtCur = prev;
        if (!g_rtValid[prev]) {
            // The first present of the mode: nothing to deliver yet.
            cost_tick();
            return false;
        }
        if (!lock_copy(g_sys[prev], &lockUs, &copyUs)) return false;
        if (!ensure_texture(dev11)) return false;
        const long long t1 = qpc_now();
        ctx->UpdateSubresource(g_tex, 0, nullptr, g_pixels, (UINT)g_w * 4, 0);
        uploadUs = qpc_us(t1, qpc_now());
        g_deliveredTag = g_rtTag[prev]; g_deliveredSerial = g_rtSerial[prev];
        delivered = true;
        sample_bbox();
    } else {   // Shared
        if (!ensure_shared(dev, dev11)) { g_mode = g_modeWant = Mode::Sync; bb->Release(); return false; }
        if (g_fenceIssued && g_fence->GetData(nullptr, 0, 0) == S_FALSE) ++g_fenceLate;
        const long long t0 = qpc_now();
        dvr::perf::gpu_mark(dvr::perf::kGpuRtdA);   // shared: the blit alone
        const HRESULT hr = dev->StretchRect(bb, nullptr, g_sharedRt, nullptr, D3DTEXF_NONE);
        dvr::perf::gpu_mark(dvr::perf::kGpuRtdB);
        g_fence->Issue(D3DISSUE_END);
        g_fenceIssued = true;
        blitUs = qpc_us(t0, qpc_now());
        bb->Release();
        if (FAILED(hr)) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Error,
                         "capture: StretchRect backbuffer -> shared surface failed (0x%08lx) - the headset "
                         "gets nothing in shared mode; `capture mode sync`", (unsigned long)hr);
            return false;
        }
        g_deliveredTag = thisTag; g_deliveredSerial = thisSerial;
        delivered = true;
        const uint64_t now = GetTickCount64();
        if (g_sharedBboxMs == 0 || now - g_sharedBboxMs >= 3000 || !g_bboxSaidForSize) {
            g_sharedBboxMs = now;
            if (read_back(dev, g_sharedRt, &rtdUs, &lockUs, &copyUs)) sample_bbox();
        }
    }
    if (delivered) ++g_grabs;
    g_sumRtd += rtdUs; g_sumLock += lockUs; g_sumCopy += copyUs; g_sumUpload += uploadUs; g_sumBlit += blitUs;
    ++g_windowGrabs;
    g_last.rtdUs = (uint32_t)rtdUs; g_last.lockUs = (uint32_t)lockUs; g_last.copyUs = (uint32_t)copyUs;
    g_last.uploadUs = (uint32_t)uploadUs; g_last.blitUs = (uint32_t)blitUs;
    g_last.totalUs = (uint32_t)(rtdUs + lockUs + copyUs + uploadUs + blitUs);
    g_last.grabsInWindow = 1;
    cost_tick();
    return delivered;
}

ID3D11Texture2D* texture() { return g_mode == Mode::Shared && g_sharedTex ? g_sharedTex : g_tex; }
ID3D11ShaderResourceView* srv() { return g_mode == Mode::Shared && g_sharedSrv ? g_sharedSrv : g_srv; }
uint32_t width() { return g_w; }
uint32_t height() { return g_h; }
const uint8_t* pixels() { return g_pixels; }
Bbox bbox() { return g_bbox; }
uint32_t grabs() { return g_grabs; }
Cost cost() { return g_cost; }
Cost last_grab() { return g_last; }
bool shared_available() { return g_sharedOk; }
bool probed() { return g_probed; }

bool snapshot_pixels(IDirect3DDevice9* dev) {
    if (g_mode != Mode::Shared || !g_sharedRt || !dev) return g_pixels != nullptr;
    return read_back(dev, g_sharedRt, nullptr, nullptr, nullptr);
}

bool set_mode(const char* name) {
    Mode m;
    if (!name || !name[0]) return false;
    if (!_stricmp(name, "sync")) m = Mode::Sync;
    else if (!_stricmp(name, "deferred")) m = Mode::Deferred;
    else if (!_stricmp(name, "shared")) m = Mode::Shared;
    else {
        DVR_WARN("capture: unknown mode '%s' (sync|deferred|shared) - staying on %s", name,
                 kModeNames[(int)g_mode]);
        return false;
    }
    if (m == Mode::Shared && g_probed && !g_sharedOk) {
        DVR_WARN("capture: mode shared refused - the probe said this device cannot share (the log's "
                 "capture/probe lines say why); staying on %s", kModeNames[(int)g_mode]);
        return false;
    }
    g_modeWant = m;
    if (m != g_mode)
        DVR_INFO("capture: mode %s requested (applied at the next grab)", kModeNames[(int)m]);
    return true;
}
Mode mode() { return g_mode; }
const char* mode_name() { return kModeNames[(int)g_mode]; }

void set_pending_tag(int eyeSign) { g_pendingTag = eyeSign < 0 ? -1 : eyeSign > 0 ? 1 : 0; }
int delivered_tag() { return g_deliveredTag; }
uint32_t delivered_serial() { return g_deliveredSerial; }
uint32_t serial() { return g_serial; }
uint32_t fence_late() { return g_fenceLate; }

void on_reset() {
    if (g_sysmem) { g_sysmem->Release(); g_sysmem = nullptr; }
    release_deferred();   // default pool: must go before the device resets (38.63)
    release_shared();
    g_w = g_h = 0;
    g_fmt = D3DFMT_UNKNOWN;
}

void shutdown() {
    on_reset();
    free(g_pixels); g_pixels = nullptr;
    if (g_srv) { g_srv->Release(); g_srv = nullptr; }
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
    g_texW = g_texH = 0;
}

} // namespace dvr::capture
