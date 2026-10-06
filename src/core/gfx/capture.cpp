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
#include "core/gfx/markers_sharp.h"
#include "core/gfx/depth_probe.h"
#include "core/gfx/shared_capture_texture.h"

#include "core/framework/perf.h"
#include "core/gfx/d3d9ex.h"
#include "core/gfx/frame_id.h"
#include "core/util/log.h"
#include "core/util/etw.h"

#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <stdio.h>
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
// 41.1 (session 9): `capture reinit` - rebuild the mode's slots (the shared
// surfaces, their fences and D3D11 views; the deferred ring) at the next grab
// WITHOUT changing the mode: the other isolated half of the user's remedy for
// the one-view state (deferred -> shared rebuilt the slots AND was followed by
// a pause). One present delivers nothing (a rebuild cannot deliver the frame
// it released), counted on the method's noFrame like a mode switch.
bool     g_reinitWant = false;
uint32_t g_reinits = 0;
const char* const kModeNames[] = {"sync", "deferred", "shared", "off"};
uint32_t g_offSkipped = 0;     // presents grab() took nothing from while Off (this window)

// The eye tag a method attaches to the present being grabbed, and the tag of
// the content the last grab actually delivered (equal except under deferred).
int      g_pendingTag = 0;
int      g_deliveredTag = 0;
// VR-65: the pose record travels with the pixels, on the same slots and by the
// same rule as the eye tag. Under SharedWait=0 the delivered slot is the
// PREVIOUS present's, so the record that comes back out here is a present older
// than the one being submitted - which is exactly the gap that makes choosing a
// pose by timing unsafe, and the reason the record has to ride the image.
uint32_t g_pendingRec = 0;
uint32_t g_deliveredRec = 0;
uint32_t g_rtRec[2] = {0, 0};
uint32_t g_sharedRec[4] = {0, 0, 0, 0};
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

// shared: a RING of D3D9 surfaces opened on D3D11, each with an event-query fence:
// slot cur is blitted at present N and delivered at N+depth (SharedWait=0) or at N
// after its fence (SharedWait=1). depth 1 = two slots, the 41.1 behaviour and the
// default; `capture depth 2|3` (uncap deep dive, 2026-09-27) adds slots so the
// delivered blit had two or three presents to finish and the render thread stops
// waiting on the GPU at the capture fence - one or two presents more latency, the
// pose record travelling with the image as before.
const int                 kMaxShared = 4;
interop::Image           g_sharedImage[kMaxShared];
IDirect3DSurface9*        g_sharedRt[kMaxShared] = {}; // borrowed from g_sharedImage
ID3D11Texture2D*          g_sharedTex[kMaxShared] = {}; // borrowed from g_sharedImage
ID3D11ShaderResourceView* g_sharedSrv[kMaxShared] = {};
IDirect3DQuery9*          g_fence[kMaxShared] = {};
bool                      g_fenceIssued[kMaxShared] = {};
ID3D11Query*              g_readQuery[kMaxShared] = {};   // the D3D11 read of the slot, for the next blit into it
bool                      g_readIssued[kMaxShared] = {};
uint32_t                  g_readWaits = 0, g_readTimeouts = 0, g_readWaitsWindow = 0;
bool                      g_sharedValid[kMaxShared] = {};
int                       g_sharedTag[kMaxShared] = {};
uint32_t                  g_sharedSerial[kMaxShared] = {};
int                       g_sharedCur = 0;
int                       g_sharedDepth = 1;        // presents between a slot's blit and its delivery
int                       g_sharedN = 0;            // slots live now (0 = none built)
int                       g_sharedDepthWant = 1;
// What a bounded capture wait does when its 10 ms run out ([Capture] TimeoutRefuse,
// `capture timeout deliver|refuse`). 1.0.1 delivered anyway; 1.0.2 (1d2ee24a5, a TAA
// hardening verified only on the simulator) refused the grab instead. On a GPU-bound
// machine the timeouts are routine, the eyes alternate, so the refusals land on the
// SAME eye every time: that eye's present goes out untagged and is held, and it
// refreshes at 6-9 Hz while the other gets 23-30 (GTX 1650 field logs, 2026-10-03,
// FLICKER_REFERENCE). Deliver is the default again; refuse stays as the A/B.
bool                      g_timeoutRefuse = false;
uint32_t                  g_timeoutDelivered = 0, g_timeoutDeliveredWindow = 0;
uint32_t                  g_timeoutRefused = 0, g_timeoutRefusedWindow = 0;
// [Capture] AutoDepth (default 1). Delivering a timed-out slot keeps the eyes paired, but
// the copy is not finished, and with two slots and alternating eyes each slot always holds
// the same eye - so the headset shows that eye's PREVIOUS frame: a one-frame hitch in one
// eye, worse when moving (GTX 1650, 2026-10-03: 68-81 timeouts per 3 s window, the capture
// wait 8.6-8.8 ms per present against 3-5 in 1.0.1). When at least 10% of the grabs time
// out for two windows running, the ring steps once to depth 2 (3 slots): each copy gets one
// more present to finish. An explicit SharedDepth (ini or `capture depth`) always wins.
bool                      g_autoDepth = true;
// [Capture] AutoDepthPercent (default 10, the rule above): the timeout share of a window that counts as a strike.
// 2026-10-05: a 4070 Ti at 120 Hz under afw timed out on 2-4% of grabs per window (max 10.9%, once), about one to
// three one-eye hitches a second, and the 10% rule never acted. 2 would have stepped it in its first windows.
int                       g_autoDepthPct = 10;
bool                      g_depthExplicit = false;
int                       g_autoStrikes = 0;
uint32_t                  g_fenceWaitUsWindow = 0;  // the fence wait's own sum, for the window line
int                       g_sharedDelivered = -1;   // the slot texture()/srv() hand out
bool                      g_sharedWait = false;
uint32_t                  g_fenceWaits = 0, g_fenceTimeouts = 0, g_fenceWaitsWindow = 0;
uint64_t                  g_sharedBboxMs = 0;
uint32_t                  g_bboxIntervalMs = 30000;   // see capture.h; 0 = size changes only
uint32_t                  g_bboxSamples = 0;
D3DFORMAT                 g_sharedFmt = D3DFMT_UNKNOWN;
ID3D11DeviceContext*      g_lastCtx = nullptr;

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
    if (g_mode == Mode::Off) {
        // The A/B control: the zero is by design and the line says what the
        // headset shows meanwhile, so a frozen image is not read as a fault.
        DVR_INFO("capture: OFF by request - 0 grabs by design (%u presents skipped in %.1f s), texture() re-shows "
                 "serial %u; the headset image is FROZEN on purpose ('capture mode sync' restores)",
                 g_offSkipped, (double)(now - g_windowMs) / 1000.0, g_deliveredSerial);
        g_offSkipped = 0;
    }
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
                 g_mode == Mode::Shared ? "; shared: no CPU copy, rtd is the 3 s bbox sample, lock is the fence wait" : "");
        if (g_mode == Mode::Shared)
            DVR_INFO("capture: shared delivery %s, %d slots | blit fence waits %u of %u grabs this window, %.2f ms per "
                     "grab spent in them (timeouts %u lifetime) | D3D11 read still pending at the next blit %u this "
                     "window (timeouts %u lifetime; each one was a frame that could have shown the OTHER eye's image "
                     "before the read fence) | a fence wait is the render thread idling on the GPU; if `capture depth 2` "
                     "leaves it at ~0 and the rate does not rise, the GPU is the ceiling",
                     g_sharedWait ? "this present after its fence (SharedWait=1)"
                                  : g_sharedDepth == 1 ? "the previous present's slot (depth 1)"
                                  : g_sharedDepth == 2 ? "the slot from 2 presents ago (depth 2)" : "the slot from 3 presents ago (depth 3)",
                     g_sharedN, g_fenceWaitsWindow, g_windowGrabs,
                     g_windowGrabs ? (double)g_fenceWaitUsWindow / 1000.0 / g_windowGrabs : 0.0, g_fenceTimeouts,
                     g_readWaitsWindow, g_readTimeouts);
        if (g_mode == Mode::Shared && (g_timeoutDeliveredWindow || g_timeoutRefusedWindow))
            DVR_INFO("capture: wait timeouts this window: %u delivered anyway, %u refused (policy %s) | lifetime %u "
                     "delivered, %u refused. A refused grab goes out untagged and is held; the eyes alternate, so "
                     "on a GPU-bound machine the refusals starve ONE eye ([Capture] TimeoutRefuse, `capture timeout`)",
                     g_timeoutDeliveredWindow, g_timeoutRefusedWindow, g_timeoutRefuse ? "refuse" : "deliver",
                     g_timeoutDelivered, g_timeoutRefused);
        {
            const uint32_t timeouts = g_timeoutDeliveredWindow + g_timeoutRefusedWindow;
            if (g_mode == Mode::Shared && g_autoDepth && !g_depthExplicit && !g_sharedWait &&
                g_sharedDepthWant == 1 && g_windowGrabs >= 30) {
                g_autoStrikes = (timeouts * 100 >= (uint32_t)g_autoDepthPct * g_windowGrabs && timeouts) ? g_autoStrikes + 1 : 0;
                if (g_autoStrikes >= 2) {
                    g_autoStrikes = 0;
                    DVR_WARN("capture: AUTO DEPTH - %u of %u grabs (%.0f%%) timed out waiting for the GPU's frame copy, "
                             "two windows running. A timed-out copy is not finished, so that eye shows its previous "
                             "frame (a one-frame hitch). Delivery depth 1 -> 2 (3 slots): each copy gets one more "
                             "present to finish, at one present more latency (the pose travels with the image). "
                             "[Capture] AutoDepth=0 or an explicit SharedDepth turns this off; `capture depth 1` undoes it live",
                             timeouts, g_windowGrabs, 100.0 * timeouts / g_windowGrabs);
                    set_shared_depth(2, "auto: capture timeouts");
                }
            }
        }
        g_timeoutDeliveredWindow = 0; g_timeoutRefusedWindow = 0;
        g_fenceWaitsWindow = 0; g_readWaitsWindow = 0; g_fenceWaitUsWindow = 0;
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
// Probe the same texture type and preferred/fallback formats used by live slots.
// A standalone surface or a rejected X8 format must not veto an A8 texture.
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
    interop::Image image;
    const D3DFORMAT first = interop::preferred_format(g_fmt);
    D3DFORMAT fmt = first;
    auto result = interop::create(dev, dev11, g_w, g_h, fmt, image);
    if (FAILED(result.hr)) {
        DVR_WARN("capture/probe: shared texture %ux%u fmt=%d refused at %s (0x%08lx)",
                 g_w, g_h, (int)fmt, result.step, (unsigned long)result.hr);
        if (first != g_fmt) {
            fmt = g_fmt;
            result = interop::create(dev, dev11, g_w, g_h, fmt, image);
            if (FAILED(result.hr))
                DVR_WARN("capture/probe: fallback fmt=%d refused at %s (0x%08lx)",
                         (int)fmt, result.step, (unsigned long)result.hr);
        }
    }
    if (FAILED(result.hr)) {
        DVR_WARN("capture/probe: shared texture REFUSED - CPU readback remains active; requested mode=%s",
                 kModeNames[(int)g_modeWant]);
        return;
    }
    D3D11_TEXTURE2D_DESC td = {};
    image.texture->GetDesc(&td);
    g_sharedOk = true;
    DVR_INFO("capture/probe: shared texture AVAILABLE - CreateTexture D3D9 %ux%u fmt=%d -> D3D11 %ux%u fmt=%d; no CPU round trip in Mode=shared",
             g_w, g_h, (int)fmt, td.Width, td.Height, (int)td.Format);
    if (g_modeWant != Mode::Shared)
        DVR_WARN("capture/probe: device can share but requested capture mode=%s retains CPU readback", kModeNames[(int)g_modeWant]);
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
    // The D3D11 side first (the opened texture must not outlive the D3D9
    // surface across a Reset), unbound from the context that sampled it.
    if (g_lastCtx) { ID3D11ShaderResourceView* nul = nullptr; g_lastCtx->PSSetShaderResources(0, 1, &nul); }
    for (int i = 0; i < kMaxShared; ++i) {
        if (g_readQuery[i]) { g_readQuery[i]->Release(); g_readQuery[i] = nullptr; }
        if (g_sharedSrv[i]) { g_sharedSrv[i]->Release(); g_sharedSrv[i] = nullptr; }
        g_sharedTex[i] = nullptr;
        if (g_fence[i]) { g_fence[i]->Release(); g_fence[i] = nullptr; }
        g_sharedRt[i] = nullptr;
        g_sharedImage[i].reset();
        g_fenceIssued[i] = false; g_sharedValid[i] = false; g_readIssued[i] = false;
    }
    g_sharedCur = 0; g_sharedDelivered = -1; g_sharedN = 0;
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

// One shared slot: the D3D9 render target with a shared handle (A8R8G8B8 when
// the backbuffer is X8R8G8B8, so D3D11 opens the same B8G8R8A8 the upload
// path uses; StretchRect converts at equal size), opened on D3D11, its SRV,
// its fence. Every refusal names the step and the HRESULT.
bool ensure_shared_slot(IDirect3DDevice9* dev, ID3D11Device* dev11, int i, D3DFORMAT fmt) {
    const auto result = interop::create(dev, dev11, g_w, g_h, fmt, g_sharedImage[i]);
    if (FAILED(result.hr)) {
        DVR_ERROR("capture: shared texture slot %d %ux%u fmt=%d refused at %s (0x%08lx)",
                  i, g_w, g_h, (int)fmt, result.step, (unsigned long)result.hr);
        return false;
    }
    g_sharedRt[i] = g_sharedImage[i].surface;
    g_sharedTex[i] = g_sharedImage[i].texture;
    HRESULT hr = S_OK;
    hr = dev11->CreateShaderResourceView(g_sharedTex[i], nullptr, &g_sharedSrv[i]);
    if (FAILED(hr) || !g_sharedSrv[i]) {
        D3D11_TEXTURE2D_DESC td = {};
        g_sharedTex[i]->GetDesc(&td);
        DVR_ERROR("capture: the SRV on shared slot %d refused (0x%08lx, DXGI fmt %d)", i, (unsigned long)hr, (int)td.Format);
        return false;
    }
    hr = dev->CreateQuery(D3DQUERYTYPE_EVENT, &g_fence[i]);
    if (FAILED(hr) || !g_fence[i]) {
        DVR_ERROR("capture: the event query for shared slot %d refused (0x%08lx) - no fence, no shared mode", i, (unsigned long)hr);
        return false;
    }
    D3D11_QUERY_DESC qd = {};
    qd.Query = D3D11_QUERY_EVENT;
    hr = dev11->CreateQuery(&qd, &g_readQuery[i]);
    if (FAILED(hr) || !g_readQuery[i]) {
        DVR_ERROR("capture: the D3D11 event query for shared slot %d refused (0x%08lx) - no read fence, no shared mode", i, (unsigned long)hr);
        return false;
    }
    g_fenceIssued[i] = false; g_sharedValid[i] = false;
    return true;
}

bool ensure_shared_slots(IDirect3DDevice9* dev, ID3D11Device* dev11, int n, D3DFORMAT fmt) {
    for (int i = 0; i < n; ++i) if (!ensure_shared_slot(dev, dev11, i, fmt)) return false;
    return true;
}

bool ensure_shared(IDirect3DDevice9* dev, ID3D11Device* dev11) {
    const int want = g_sharedDepthWant + 1;
    if (g_sharedN == want) return true;
    const int wasN = g_sharedN, wasDepth = g_sharedDepth;
    release_shared();
    g_sharedDepth = g_sharedDepthWant;
    if (wasN)
        DVR_INFO("capture: shared ring %d -> %d slots (delivery depth %d -> %d presents); the slots are rebuilt and "
                 "the first %d present(s) deliver nothing (held untagged)", wasN, want, wasDepth, g_sharedDepth,
                 g_sharedDepth);
    // The format: the backbuffer's when it carries alpha (A8R8G8B8 opens as
    // B8G8R8A8), else A8R8G8B8 first (the same D3D11 format the upload path
    // uses) and the backbuffer's own as the fallback.
    const D3DFORMAT first = interop::preferred_format(g_fmt);
    bool ok = ensure_shared_slots(dev, dev11, want, first);
    g_sharedFmt = first;
    if (!ok && first != g_fmt) {
        DVR_WARN("capture: shared slots in fmt=%d refused - retrying in the backbuffer's own fmt=%d", (int)first, (int)g_fmt);
        release_shared();
        ok = ensure_shared_slots(dev, dev11, want, g_fmt);
        g_sharedFmt = g_fmt;
    }
    if (!ok) {
        DVR_ERROR("capture: shared mode refused (the lines above say which step) - back to sync");
        release_shared();
        return false;
    }
    g_sharedN = want;
    D3D11_TEXTURE2D_DESC td = {};
    g_sharedTex[0]->GetDesc(&td);
    LUID luid = {};
    const bool haveLuid = dvr::d3d9ex::adapter_luid(&luid);
    DVR_INFO("capture: shared surfaces %ux%u live (%d slots, D3D9 fmt=%d -> D3D11 fmt=%d, D3D9 adapter LUID %08lx-%08lx%s); "
             "no CPU copy per present, the blit is fenced by a D3D9 event query, delivery = %s (depth %d); the bbox "
             "samples a readback every 3 s", g_w, g_h, g_sharedN, (int)g_sharedFmt, (int)td.Format,
             (unsigned long)luid.HighPart, (unsigned long)luid.LowPart, haveLuid ? "" : " (unknown)",
             g_sharedWait ? "this present after its fence" : g_sharedDepth == 1 ? "the previous present's slot"
                                                                                 : "an older present's slot",
             g_sharedWait ? 0 : g_sharedDepth);
    return true;
}

// Before D3D9 blits INTO a slot: the D3D11 read of that slot (the consumer's
// draw from srv() at the present it was delivered) must have executed, or the
// read sees the new frame - the other eye's image - land under it. Bounded
// like the blit fence; counted, because the count is the number of frames
// that could have swapped an eye before this fence existed.
bool read_wait(int i, uint64_t* lockUs) {
    if (!g_readIssued[i]) return true;
    if (!g_readQuery[i] || !g_lastCtx) return false;
    dvr::etw::Scope etwWait(dvr::etw::kCapRead, i);
    const long long t0 = qpc_now();
    HRESULT hr = g_lastCtx->GetData(g_readQuery[i], nullptr, 0, 0);
    if (hr == S_FALSE) {
        ++g_readWaits; ++g_readWaitsWindow;
        while (hr == S_FALSE && qpc_us(t0, qpc_now()) < 10000) {
            Sleep(0);
            hr = g_lastCtx->GetData(g_readQuery[i], nullptr, 0, 0);
        }
        if (hr == S_FALSE) ++g_readTimeouts;
    }
    *lockUs += qpc_us(t0, qpc_now());
    if (hr == S_FALSE && !g_timeoutRefuse) {
        // 1.0.1's behaviour: stop waiting on this read and blit anyway. The query is
        // dropped, not kept pending, or the same slot refuses on every later present.
        ++g_timeoutDelivered; ++g_timeoutDeliveredWindow;
        g_readIssued[i] = false;
        return true;
    }
    if (hr != S_OK) { if (hr == S_FALSE) { ++g_timeoutRefused; ++g_timeoutRefusedWindow; } return false; }
    g_readIssued[i] = false;
    return true;
}

// Wait for a slot's blit with a bound: S_OK at once is the common case (a
// whole present passed); otherwise spin on GetData with FLUSH for up to
// 10 ms, then refuse this delivery and count the timeout.
bool fence_wait(int i, uint64_t* lockUs) {
    if (!g_fenceIssued[i]) return true;
    if (!g_fence[i]) return false;
    dvr::etw::Scope etwWait(dvr::etw::kCapFence, i);
    const long long t0 = qpc_now();
    HRESULT hr = g_fence[i]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
        ++g_fenceWaits; ++g_fenceWaitsWindow;
        while (hr == S_FALSE && qpc_us(t0, qpc_now()) < 10000) {
            Sleep(0);
            hr = g_fence[i]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
        }
        if (hr == S_FALSE) ++g_fenceTimeouts;
    }
    *lockUs += qpc_us(t0, qpc_now());
    if (hr == S_FALSE && !g_timeoutRefuse) {
        // 1.0.1's behaviour: deliver the slot anyway. Its eye tag is still the right one,
        // so the pair stays a pair; at worst the blit is not finished for this present.
        ++g_timeoutDelivered; ++g_timeoutDeliveredWindow;
        g_fenceIssued[i] = false;
        return true;
    }
    if (hr != S_OK) { if (hr == S_FALSE) { ++g_timeoutRefused; ++g_timeoutRefusedWindow; } return false; }
    g_fenceIssued[i] = false;
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
    if (g_reinitWant) {
        g_reinitWant = false;
        ++g_reinits;
        release_deferred();
        release_shared();
        DVR_INFO("capture: %s slots REBUILT by request (reinit #%u: released and re-created at this grab, the mode "
                 "unchanged; this present delivers nothing, so its sibling stands alone once - the frameid line's next "
                 "pairs say whether a slot rebuild ALONE made two pictures of one)", kModeNames[(int)g_mode], g_reinits);
    }
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
                 : g_modeWant == Mode::Off    ? " (NO capture: the last frame stays on the headset)"
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
    if (g_mode == Mode::Off) {
        // Nothing grabbed: the consumer keeps the last texture (both methods
        // handle a false return by re-showing it). The tag is consumed so a
        // stereo method's pairing sees an untagged present, not a stale one.
        bb->Release();
        g_pendingTag = 0; g_pendingRec = 0;
        ++g_offSkipped;
        cost_tick();
        return false;
    }
    if (g_mode == Mode::Shared && !g_sharedOk) {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                     "capture: [Capture] Mode=shared but the probe said the device cannot share (a plain D3D9 "
                     "device: [Device] Ex=0, or the 9Ex creation fell back) - running deferred");
        g_mode = g_modeWant = Mode::Deferred;
    }
    ++g_serial;
    const uint32_t thisSerial = g_serial;
    const int thisTag = g_pendingTag;
    dvr::markersharp::seal(thisSerial,thisTag);
    dvr::depthprobe::fgmask_seal(thisSerial);   // VR-39 run 17: the foreground mask drawn for this grab
    g_pendingTag = 0;
    const uint32_t thisRec = g_pendingRec;
    g_pendingRec = 0;
    uint64_t rtdUs = 0, lockUs = 0, copyUs = 0, uploadUs = 0, blitUs = 0;
    bool delivered = false;
    // 41.1 (session 9): the frame-identity trace's stage bb - the backbuffer
    // as this grab found it, keyed by this grab's serial and tag.
    dvr::frameid::stage_backbuffer(dev, bb, thisSerial, thisTag);

    if (g_mode == Mode::Sync) {
        const bool ok = read_back(dev, bb, &rtdUs, &lockUs, &copyUs);
        bb->Release();
        if (!ok) return false;
        if (!ensure_texture(dev11)) return false;
        const long long t0 = qpc_now();
        ctx->UpdateSubresource(g_tex, 0, nullptr, g_pixels, (UINT)g_w * 4, 0);
        uploadUs = qpc_us(t0, qpc_now());
        g_deliveredTag = thisTag; g_deliveredSerial = thisSerial;
        g_deliveredRec = thisRec;
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
        g_rtRec[cur] = thisRec;
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
        g_deliveredRec = g_rtRec[prev];
        delivered = true;
        sample_bbox();
    } else {   // Shared
        if (!ensure_shared(dev, dev11)) { g_mode = g_modeWant = Mode::Sync; bb->Release(); return false; }
        g_lastCtx = ctx;
        const int cur = g_sharedCur;
        if (!read_wait(cur, &lockUs)) { bb->Release(); cost_tick(); return false; }   // the D3D11 side must be done reading this slot
        const long long t0 = qpc_now();
        dvr::perf::gpu_mark(dvr::perf::kGpuRtdA);   // shared: the blit alone
        const HRESULT hr = dev->StretchRect(bb, nullptr, g_sharedRt[cur], nullptr, D3DTEXF_NONE);
        dvr::perf::gpu_mark(dvr::perf::kGpuRtdB);
        const HRESULT fenceHr = g_fence[cur]->Issue(D3DISSUE_END);
        g_fenceIssued[cur] = SUCCEEDED(fenceHr);
        blitUs = qpc_us(t0, qpc_now());
        bb->Release();
        if (FAILED(hr) || FAILED(fenceHr)) {
            g_sharedValid[cur] = false;
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Error,
                         "capture: StretchRect backbuffer -> shared slot %d failed (0x%08lx; D3D9 fmt %d -> %d) - the "
                         "headset gets nothing in shared mode; `capture mode sync`", cur, (unsigned long)(FAILED(hr) ? hr : fenceHr), (int)g_fmt,
                         (int)g_sharedFmt);
            return false;
        }
        g_sharedValid[cur] = true; g_sharedTag[cur] = thisTag; g_sharedSerial[cur] = thisSerial;
        g_sharedRec[cur] = thisRec;
        g_sharedCur = (cur + 1) % g_sharedN;
        // Delivery: the previous slot (pipelined, the tag travels with it) or
        // this one after its fence; either way the fence is waited on with a
        // bound BEFORE the D3D11 side samples it (the consumer draws from
        // srv() right after this returns).
        const int slot = g_sharedWait ? cur : (cur - g_sharedDepth + g_sharedN) % g_sharedN;
        if (!g_sharedValid[slot]) { cost_tick(); return false; }   // the ring's first presents
        const uint64_t lockBefore = lockUs;
        if (!fence_wait(slot, &lockUs)) { cost_tick(); return false; }
        g_fenceWaitUsWindow += (uint32_t)(lockUs - lockBefore);
        g_sharedDelivered = slot;
        g_deliveredTag = g_sharedTag[slot]; g_deliveredSerial = g_sharedSerial[slot];
        g_deliveredRec = g_sharedRec[slot];
        delivered = true;
        // The bbox readback (see set_bbox_ms in capture.h): a full-frame CPU
        // round trip on the present thread, in the mode whose whole purpose is
        // not to make one. !g_bboxSaidForSize is kept unconditional so a size
        // change still resamples at once - that is the sample that decides
        // CROPPED vs FULL and it must not wait for an interval.
        const uint64_t now = GetTickCount64();
        const bool due = !g_bboxSaidForSize ||
                         (g_bboxIntervalMs != 0 &&
                          (g_sharedBboxMs == 0 || now - g_sharedBboxMs >= g_bboxIntervalMs));
        if (due) {
            g_sharedBboxMs = now;
            ++g_bboxSamples;
            if (read_back(dev, g_sharedRt[slot], &rtdUs, &lockUs, &copyUs)) sample_bbox();
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

void set_bbox_ms(uint32_t ms) {
    const uint32_t was = g_bboxIntervalMs;
    g_bboxIntervalMs = ms;
    if (was == ms) return;
    char nowTxt[48], wasTxt[48];
    if (ms) sprintf(nowTxt, "every %u ms", ms); else strcpy(nowTxt, "OFF (size changes only)");
    if (was) sprintf(wasTxt, "every %u ms", was); else strcpy(wasTxt, "off");
    DVR_INFO("capture: content-bbox resample %s, was %s - in shared mode each sample is a full-frame "
             "GetRenderTargetData + LockRect + row copy ON THE PRESENT THREAD (%ux%u = %.1f MB), the same "
             "operation that makes sync mode cost 17-21 ms/present, so every interval is a real stall. "
             "%u sample(s) taken so far",
             nowTxt, wasTxt, g_w, g_h, (double)g_w * g_h * 4.0 / (1024.0 * 1024.0), g_bboxSamples);
}

uint32_t bbox_ms() { return g_bboxIntervalMs; }
uint32_t bbox_samples() { return g_bboxSamples; }

ID3D11Texture2D* texture() {
    if (g_mode == Mode::Shared && g_sharedDelivered >= 0 && g_sharedTex[g_sharedDelivered]) return g_sharedTex[g_sharedDelivered];
    return g_tex;
}
ID3D11ShaderResourceView* srv() {
    if (g_mode == Mode::Shared && g_sharedDelivered >= 0 && g_sharedSrv[g_sharedDelivered]) return g_sharedSrv[g_sharedDelivered];
    return g_srv;
}
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
    if (g_mode != Mode::Shared || g_sharedDelivered < 0 || !g_sharedRt[g_sharedDelivered] || !dev) return g_pixels != nullptr;
    return read_back(dev, g_sharedRt[g_sharedDelivered], nullptr, nullptr, nullptr);
}

bool set_mode(const char* name) {
    Mode m;
    if (!name || !name[0]) return false;
    if (!_stricmp(name, "sync")) m = Mode::Sync;
    else if (!_stricmp(name, "deferred")) m = Mode::Deferred;
    else if (!_stricmp(name, "shared")) m = Mode::Shared;
    else if (!_stricmp(name, "off")) m = Mode::Off;
    else {
        DVR_WARN("capture: unknown mode '%s' (sync|deferred|shared|off) - staying on %s", name,
                 kModeNames[(int)g_mode]);
        return false;
    }
    if (m == Mode::Off)
        DVR_WARN("capture: mode off requested - the headset image FREEZES on the last frame by design (the tick "
                 "budget's A/B: what the tick rate is with no capture at all); 'capture mode sync' restores");
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
bool request_reinit() {
    if (g_mode != Mode::Shared && g_mode != Mode::Deferred) {
        DVR_WARN("capture: reinit refused - mode %s has no slots to rebuild (shared|deferred only)", kModeNames[(int)g_mode]);
        return false;
    }
    g_reinitWant = true;
    DVR_INFO("capture: reinit requested - the %s slots are released and re-created at the next grab, mode unchanged",
             kModeNames[(int)g_mode]);
    return true;
}
uint32_t reinits() { return g_reinits; }


void set_pending_tag(int eyeSign) { g_pendingTag = eyeSign < 0 ? -1 : eyeSign > 0 ? 1 : 0; }
int delivered_tag() { return g_deliveredTag; }
void set_pending_rec(uint32_t rec) { g_pendingRec = rec; }

// VR-80 F-late: relabel the most recent grab's slot while it is still waiting to be delivered
// (a pipelined mode: shared with SharedWait=0, or deferred). Only an UNTAGGED slot from the
// latest grab qualifies: a late tag proved which eye its pixels were, one present after they
// were grabbed. Sync and SharedWait=1 have already delivered it, so they refuse.
bool relabel_last_grab(int eyeSign, uint32_t rec) {
    if (eyeSign == 0) return false;
    const int tag = eyeSign < 0 ? -1 : 1;
    if (g_mode == Mode::Shared && !g_sharedWait) {
        if (g_sharedN < 2) return false;
        const int last = (g_sharedCur - 1 + g_sharedN) % g_sharedN;
        if (!g_sharedValid[last] || g_sharedSerial[last] != g_serial || g_sharedTag[last] != 0) return false;
        g_sharedTag[last] = tag; g_sharedRec[last] = rec;
        return true;
    }
    if (g_mode == Mode::Deferred) {
        const int last = g_rtCur ^ 1;
        if (!g_rtValid[last] || g_rtSerial[last] != g_serial || g_rtTag[last] != 0) return false;
        g_rtTag[last] = tag; g_rtRec[last] = rec;
        return true;
    }
    return false;
}
bool retire_last_right_grab(uint32_t expectedRec) {
    if (!expectedRec) return false;
    if (g_mode == Mode::Shared && !g_sharedWait) {
        if (g_sharedN < 2) return false;
        const int last = (g_sharedCur - 1 + g_sharedN) % g_sharedN;
        if (!g_sharedValid[last] || g_sharedSerial[last] != g_serial ||
            g_sharedTag[last] != +1 || g_sharedRec[last] != expectedRec) return false;
        g_sharedTag[last] = 0; g_sharedRec[last] = 0;
        return true;
    }
    if (g_mode == Mode::Deferred) {
        const int last = g_rtCur ^ 1;
        if (!g_rtValid[last] || g_rtSerial[last] != g_serial ||
            g_rtTag[last] != +1 || g_rtRec[last] != expectedRec) return false;
        g_rtTag[last] = 0; g_rtRec[last] = 0;
        return true;
    }
    return false;
}
uint32_t delivered_rec() { return g_deliveredRec; }
uint32_t delivered_serial() { return g_deliveredSerial; }
uint32_t serial() { return g_serial; }
int delivered_slot() { return g_mode == Mode::Shared ? g_sharedDelivered : g_mode == Mode::Deferred ? (g_rtCur ^ 1) : -1; }
void read_done(ID3D11DeviceContext* ctx) {
    if (g_mode != Mode::Shared || g_sharedDelivered < 0 || !ctx) return;
    const int slot = g_sharedDelivered;
    if (!g_readQuery[slot]) return;
    // Once per delivery: DLSS releases the slot right after copying it (dlss.cpp), and the
    // present's own call afterwards must not move the fence behind the DLSS wait again.
    static uint32_t endedFor = 0;
    if (g_readIssued[slot] && endedFor == g_deliveredSerial) return;
    endedFor = g_deliveredSerial;
    ctx->End(g_readQuery[slot]);
    ctx->Flush();   // the read goes to the GPU now, not at the runtime's next flush
    g_readIssued[slot] = true;
    g_lastCtx = ctx;
}
uint32_t read_waits() { return g_readWaits; }
uint32_t read_timeouts() { return g_readTimeouts; }

void set_shared_wait(bool on) {
    if (on == g_sharedWait) return;
    g_sharedWait = on;
    DVR_INFO("capture: shared delivery -> %s", on ? "this present after its fence (SharedWait=1: zero latency, the CPU waits for the frame in flight)"
                                                 : "the previous present's slot (SharedWait=0: one present late, no wait in the common case)");
}
bool shared_wait() { return g_sharedWait; }
void set_timeout_refuse(bool refuse, const char* who) {
    if (refuse == g_timeoutRefuse) return;
    g_timeoutRefuse = refuse;
    DVR_INFO("capture: timeout policy -> %s (%s)", refuse
                 ? "REFUSE: a capture wait that runs out drops the grab; the present goes out untagged (1.0.2/1.0.3 behaviour)"
                 : "DELIVER: a capture wait that runs out still delivers the slot with its eye tag (1.0.1 behaviour, the default)",
             who ? who : "?");
}
bool timeout_refuse() { return g_timeoutRefuse; }
void set_auto_depth(bool on, bool explicitDepth) {
    g_autoDepth = on;
    g_depthExplicit = explicitDepth;
    DVR_INFO("capture: auto depth %s (at >=%d%% of grabs timing out for two windows running; [Capture] AutoDepthPercent)%s",
             on ? "ON - steps the ring to depth 2 once when a GPU cannot finish a frame copy in one present"
                : "OFF ([Capture] AutoDepth=0)",
             g_autoDepthPct, explicitDepth ? "; [Capture] SharedDepth is set explicitly, so it never acts" : "");
}
void set_auto_depth_percent(int pct, const char* who) {
    if (pct < 1) pct = 1;
    if (pct > 100) pct = 100;
    if (pct == g_autoDepthPct) return;
    DVR_INFO("capture: auto depth threshold %d%% -> %d%% (%s) - a window whose grabs time out at least this often is a "
             "strike, two strikes step the ring to depth 2 once", g_autoDepthPct, pct, who ? who : "?");
    g_autoDepthPct = pct;
    g_autoStrikes = 0;
}
int auto_depth_percent() { return g_autoDepthPct; }
void set_shared_depth(int depth, const char* who) {
    // Anyone but the automatic step and the ini's default read makes the depth explicit.
    if (who && strncmp(who, "auto", 4) != 0 && strcmp(who, "ini") != 0) g_depthExplicit = true;
    if (depth < 1) depth = 1;
    if (depth > kMaxShared - 1) depth = kMaxShared - 1;
    if (depth == g_sharedDepthWant) return;
    g_sharedDepthWant = depth;
    DVR_INFO("capture: shared delivery depth -> %d (%s): %d slots, the image reaches the runtime %d present(s) after "
             "its blit%s; the ring is rebuilt at the next grab", depth, who ? who : "?", depth + 1, depth,
             depth == 1 ? " (the default)" : " - the render thread should stop waiting on the capture fence, at one "
                                           "present more latency per step (the pose record travels with the image)");
}
int shared_depth() { return g_sharedDepthWant; }
uint32_t fence_waits() { return g_fenceWaits; }
uint32_t fence_timeouts() { return g_fenceTimeouts; }

void on_reset() {
    if (g_sysmem) { g_sysmem->Release(); g_sysmem = nullptr; }
    release_deferred();   // default pool: must go before the device resets (38.63)
    release_shared();
    dvr::frameid::on_reset();   // its 64x64 D3D9 ring is default pool too
    g_w = g_h = 0;
    g_fmt = D3DFMT_UNKNOWN;
}

void exit_release_d3d11() {
    int released = 0;
    if (g_lastCtx) g_lastCtx->ClearState();
    for (int i = 0; i < kMaxShared; ++i) {
        if (g_readQuery[i]) { g_readQuery[i]->Release(); g_readQuery[i] = nullptr; }
        if (g_sharedSrv[i]) { g_sharedSrv[i]->Release(); g_sharedSrv[i] = nullptr; }
        if (g_sharedImage[i].texture) { g_sharedImage[i].texture->Release(); g_sharedImage[i].texture = nullptr; ++released; }
        g_sharedTex[i] = nullptr;
        g_sharedValid[i] = false; g_readIssued[i] = false;
    }
    if (g_lastCtx) g_lastCtx->Flush();
    DVR_INFO("capture: exit - released %d shared texture(s) on the D3D11 side and flushed its context "
             "(the D3D9 side is left to the game's own teardown)", released);
}

void shutdown() {
    on_reset();
    dvr::frameid::shutdown();
    free(g_pixels); g_pixels = nullptr;

    if (g_srv) { g_srv->Release(); g_srv = nullptr; }
    if (g_tex) { g_tex->Release(); g_tex = nullptr; }
    g_texW = g_texH = 0;
}

} // namespace dvr::capture
