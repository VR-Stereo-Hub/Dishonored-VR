// core/gfx/capture.cpp - see capture.h.
#define DVR_CAT ::dvr::log::Cat::capture
#include "core/gfx/capture.h"

#include "core/util/log.h"

#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::capture {
namespace {

IDirect3DSurface9*        g_sysmem = nullptr;   // D3D9 system-memory readback target
uint8_t*                  g_pixels = nullptr;   // cached heap copy, BGRA
uint32_t                  g_w = 0, g_h = 0;
D3DFORMAT                 g_fmt = D3DFMT_UNKNOWN;
ID3D11Texture2D*          g_tex = nullptr;
ID3D11ShaderResourceView* g_srv = nullptr;
uint32_t                  g_texW = 0, g_texH = 0;
uint32_t                  g_grabs = 0;
Bbox                      g_bbox;
bool                      g_warnedFormat = false;
bool                      g_warnedRtd = false;
bool                      g_bboxSaidForSize = false;
int                       g_bboxClass = -1;   // 0 all black, 1 cropped, 2 full

// The cost window: sums of the three phases over the grabs since the last
// 3 s line, and the averages that line published (what cost() returns).
long long g_qpcFreq = 0;
uint64_t  g_sumRtd = 0, g_sumCopy = 0, g_sumUpload = 0;
uint32_t  g_windowGrabs = 0;
uint64_t  g_windowMs = 0;
Cost      g_cost;

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
// carries the frame size and the bytes moved so the number can be read as a
// bandwidth as well as a stall.
void cost_tick() {
    const uint64_t now = GetTickCount64();
    if (g_windowMs == 0) { g_windowMs = now; return; }
    if (now - g_windowMs < 3000) return;
    if (g_windowGrabs) {
        g_cost.rtdUs = (uint32_t)(g_sumRtd / g_windowGrabs);
        g_cost.copyUs = (uint32_t)(g_sumCopy / g_windowGrabs);
        g_cost.uploadUs = (uint32_t)(g_sumUpload / g_windowGrabs);
        g_cost.totalUs = g_cost.rtdUs + g_cost.copyUs + g_cost.uploadUs;
        g_cost.grabsInWindow = g_windowGrabs;
        DVR_INFO("capture: cost/present rtd=%u copy=%u upload=%u total=%u us (%u grabs in %.1f s, "
                 "%ux%u, %.1f MB each way)",
                 g_cost.rtdUs, g_cost.copyUs, g_cost.uploadUs, g_cost.totalUs, g_windowGrabs,
                 (double)(now - g_windowMs) / 1000.0, g_w, g_h,
                 (double)g_w * (double)g_h * 4.0 / (1024.0 * 1024.0));
    }
    g_sumRtd = g_sumCopy = g_sumUpload = 0;
    g_windowGrabs = 0;
    g_windowMs = now;
}

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

} // namespace

bool grab(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx) {
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
            DVR_INFO("capture: %ux%u fmt=%d", g_w, g_h, (int)desc.Format);
    }
    if (!g_pixels) { bb->Release(); return false; }
    const long long t0 = qpc_now();
    HRESULT hr = dev->GetRenderTargetData(bb, g_sysmem);
    const long long t1 = qpc_now();
    bb->Release();
    if (FAILED(hr)) {
        if (!g_warnedRtd) {
            g_warnedRtd = true;
            DVR_ERROR("capture: GetRenderTargetData failed (0x%08lx) - multisampled backbuffer? "
                      "(the game's AA setting); the headset gets nothing", (unsigned long)hr);
        }
        return false;
    }
    D3DLOCKED_RECT lr;
    if (FAILED(g_sysmem->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;
    // Straight per-row copy into cached memory: the write-combined system
    // surface is very slow to read more than once.
    const size_t rowBytes = (size_t)g_w * 4;
    for (uint32_t y = 0; y < g_h; ++y)
        memcpy(g_pixels + y * rowBytes, (const uint8_t*)lr.pBits + (size_t)y * lr.Pitch, rowBytes);
    g_sysmem->UnlockRect();
    const long long t2 = qpc_now();

    if (!ensure_texture(dev11)) return false;
    ctx->UpdateSubresource(g_tex, 0, nullptr, g_pixels, (UINT)rowBytes, 0);
    const long long t3 = qpc_now();
    ++g_grabs;
    g_sumRtd += qpc_us(t0, t1);
    g_sumCopy += qpc_us(t1, t2);
    g_sumUpload += qpc_us(t2, t3);
    ++g_windowGrabs;
    cost_tick();
    sample_bbox();
    return true;
}

ID3D11Texture2D* texture() { return g_tex; }
ID3D11ShaderResourceView* srv() { return g_srv; }
uint32_t width() { return g_w; }
uint32_t height() { return g_h; }
const uint8_t* pixels() { return g_pixels; }
Bbox bbox() { return g_bbox; }
uint32_t grabs() { return g_grabs; }
Cost cost() { return g_cost; }

void on_reset() {
    if (g_sysmem) { g_sysmem->Release(); g_sysmem = nullptr; }
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
