// core/gfx/depth_probe.cpp - see depth_probe.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/depth_probe.h"
#include "core/gfx/clarity.h"
#include "core/gfx/dlss.h"
#include "core/gfx/capture.h"
#include "core/gfx/shared_capture_texture.h"
#include "core/util/log.h"
#include "core/framework/perf.h"
#include "core/framework/frame_hooks.h"
#include "core/gfx/shared_capture_texture.h"

#include <d3d11.h>

#include <atomic>
#include <initializer_list>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace dvr::depthprobe {
namespace {

const int kMax = 24;           // candidates kept (the first build kept 6 and lost the eye-size RGBA16F pair)
const int kGrid = 5;           // 5 x 5 samples, 10 % .. 90 % of each axis
const int kRuns = 30;          // automatic probes per session (five minutes)
const DWORD kEveryMs = 10000;
const DWORD kFirstMs = 30000;  // after the first present: past the menus, into a level

struct Cand {
    IDirect3DTexture9* tex = nullptr;
    UINT w = 0, h = 0;
    D3DFORMAT fmt = D3DFMT_UNKNOWN;
    int serial = 0;            // creation order, to name it across runs
};
Cand g_c[kMax];
int g_serial = 0;
std::atomic<bool> g_on{false};
std::atomic<bool> g_now{false};
int g_runs = 0;
DWORD g_nextMs = 0;
// One small target and its readback per format (the StretchRect destination must match).
struct Scratch { D3DFORMAT fmt = D3DFMT_UNKNOWN; IDirect3DSurface9* rt = nullptr; IDirect3DSurface9* sys = nullptr; };
Scratch g_s[3];

bool is_float(D3DFORMAT f) {
    return f == D3DFMT_A16B16G16R16F || f == D3DFMT_A32B32G32R32F || f == D3DFMT_G16R16F ||
           f == D3DFMT_R16F || f == D3DFMT_R32F || f == D3DFMT_G32R32F;
}
int texel_bytes(D3DFORMAT f) {
    switch (f) {
    case D3DFMT_R16F: return 2;
    case D3DFMT_G16R16F: case D3DFMT_R32F: return 4;
    case D3DFMT_A16B16G16R16F: case D3DFMT_G32R32F: return 8;
    case D3DFMT_A32B32G32R32F: return 16;
    default: return 0;
    }
}
float half_to_float(uint16_t h) {
    const uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
    float v;
    if (e == 0) v = ldexpf((float)m, -24);
    else if (e == 31) v = m ? NAN : INFINITY;
    else v = ldexpf((float)(m | 1024), (int)e - 25);
    return s ? -v : v;
}
// r, g, b, a of one texel (absent channels 0; alpha absent reads as NAN so it cannot pass)
void decode(D3DFORMAT f, const uint8_t* p, float out[4]) {
    out[0] = out[1] = out[2] = 0; out[3] = NAN;
    const uint16_t* h = (const uint16_t*)p; const float* x = (const float*)p;
    switch (f) {
    case D3DFMT_A16B16G16R16F: for (int i = 0; i < 4; ++i) out[i] = half_to_float(h[i]); break;
    case D3DFMT_A32B32G32R32F: for (int i = 0; i < 4; ++i) out[i] = x[i]; break;
    case D3DFMT_G16R16F: out[0] = half_to_float(h[0]); out[1] = half_to_float(h[1]); break;
    case D3DFMT_G32R32F: out[0] = x[0]; out[1] = x[1]; break;
    case D3DFMT_R16F: out[0] = half_to_float(h[0]); break;
    case D3DFMT_R32F: out[0] = x[0]; break;
    default: break;
    }
}
Scratch* scratch(IDirect3DDevice9* dev, D3DFORMAT f) {
    for (Scratch& s : g_s) if (s.fmt == f && s.rt) return &s;
    for (Scratch& s : g_s) {
        if (s.rt) continue;
        if (FAILED(dev->CreateRenderTarget(kGrid, kGrid, f, D3DMULTISAMPLE_NONE, 0, FALSE, &s.rt, nullptr)) ||
            FAILED(dev->CreateOffscreenPlainSurface(kGrid, kGrid, f, D3DPOOL_SYSTEMMEM, &s.sys, nullptr))) {
            if (s.rt) { s.rt->Release(); s.rt = nullptr; }
            DVR_WARN("depthprobe: cannot make a %dx%d fmt=%d scratch target - that format is skipped", kGrid, kGrid, (int)f);
            return nullptr;
        }
        s.fmt = f;
        return &s;
    }
    return nullptr;
}

// Reads the 5x5 grid of `src` (D3D9) into alpha[][]; false if any step refused.
bool read_grid_d3d9(IDirect3DDevice9* dev, IDirect3DSurface9* src, UINT w, UINT h, D3DFORMAT fmt, float alpha[kGrid][kGrid]) {
    Scratch* s = scratch(dev, fmt);
    if (!s) return false;
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            const LONG x = (LONG)(w * (0.1 + 0.2 * i)), y = (LONG)(h * (0.1 + 0.2 * j));
            RECT sr = {x, y, x + 1, y + 1}, dr = {i, j, i + 1, j + 1};
            if (FAILED(dev->StretchRect(src, &sr, s->rt, &dr, D3DTEXF_POINT))) return false;
        }
    if (FAILED(dev->GetRenderTargetData(s->rt, s->sys))) return false;
    D3DLOCKED_RECT lr = {};
    if (FAILED(s->sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return false;
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            float v[4]; decode(fmt, (const uint8_t*)lr.pBits + j * lr.Pitch + i * texel_bytes(fmt), v);
            alpha[j][i] = v[3];
        }
    s->sys->UnlockRect();
    return true;
}

void probe_one(IDirect3DDevice9* dev, const Cand& c) {
    Scratch* s = scratch(dev, c.fmt);
    if (!s) return;
    IDirect3DSurface9* src = nullptr;
    if (FAILED(c.tex->GetSurfaceLevel(0, &src)) || !src) return;
    int copied = 0;
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            const LONG x = (LONG)(c.w * (0.1 + 0.2 * i)), y = (LONG)(c.h * (0.1 + 0.2 * j));
            RECT sr = {x, y, x + 1, y + 1}, dr = {i, j, i + 1, j + 1};
            if (SUCCEEDED(dev->StretchRect(src, &sr, s->rt, &dr, D3DTEXF_POINT))) ++copied;
        }
    src->Release();
    if (copied == 0 || FAILED(dev->GetRenderTargetData(s->rt, s->sys))) {
        DVR_WARN("depthprobe: target #%d %ux%u fmt=%d - the copy was refused (%d of %d samples)", c.serial, c.w, c.h,
                 (int)c.fmt, copied, kGrid * kGrid);
        return;
    }
    D3DLOCKED_RECT lr = {};
    if (FAILED(s->sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) return;
    float a[kGrid][kGrid], lum[kGrid][kGrid], red[kGrid][kGrid];
    const int tb = texel_bytes(c.fmt);
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            float v[4];
            decode(c.fmt, (const uint8_t*)lr.pBits + j * lr.Pitch + i * tb, v);
            a[j][i] = v[3];
            red[j][i] = v[0];   // a single-channel depth target keeps it here
            lum[j][i] = 0.2126f * v[0] + 0.7152f * v[1] + 0.0722f * v[2];
        }
    s->sys->UnlockRect();
    char at[400] = "", lt[400] = "";
    int na = 0, nl = 0;
    float mn = INFINITY, mx = -INFINITY;
    for (int j = 0; j < kGrid; ++j) {
        na += _snprintf_s(at + na, sizeof(at) - na, _TRUNCATE, "%s", j ? " /" : "");
        nl += _snprintf_s(lt + nl, sizeof(lt) - nl, _TRUNCATE, "%s", j ? " /" : "");
        for (int i = 0; i < kGrid; ++i) {
            na += _snprintf_s(at + na, sizeof(at) - na, _TRUNCATE, " %.4g", a[j][i]);
            nl += _snprintf_s(lt + nl, sizeof(lt) - nl, _TRUNCATE, " %.3g", lum[j][i]);
            if (a[j][i] == a[j][i]) { mn = fminf(mn, a[j][i]); mx = fmaxf(mx, a[j][i]); }
        }
    }
    const char* verdict =
        !(mx == mx) || mx == -INFINITY ? "NO ALPHA in this format - not the depth carrier" :
        (mx - mn) < 1e-3f ? "alpha is CONSTANT - fails the depth hypothesis for this target" :
        mn < 0.0f ? "alpha goes NEGATIVE - not a plain depth" :
        (a[4][2] < a[1][2] && a[4][2] < a[0][2]) ? "varies, bottom-middle (the hands) NEARER than the upper middle: fits DEPTH"
                                                 : "varies, but bottom-middle is not nearer than the upper middle: undecided (look at the hands?)";
    DVR_INFO("depthprobe: target #%d %ux%u fmt=%d | alpha 5x5 rows top->bottom:%s | range %.4g..%.4g | %s",
             c.serial, c.w, c.h, (int)c.fmt, at, mn, mx, verdict);
    DVR_INFO("depthprobe: target #%d luminance 5x5:%s", c.serial, lt);
    {
        char rt[400] = ""; int nr = 0;
        for (int j = 0; j < kGrid; ++j) {
            nr += _snprintf_s(rt + nr, sizeof(rt) - nr, _TRUNCATE, "%s", j ? " /" : "");
            for (int i = 0; i < kGrid; ++i) nr += _snprintf_s(rt + nr, sizeof(rt) - nr, _TRUNCATE, " %.4g", red[j][i]);
        }
        DVR_INFO("depthprobe: target #%d red channel 5x5 (a one-channel depth would live here):%s", c.serial, rt);
    }
}

} // namespace

void note_texture(IDirect3DTexture9* tex, UINT w, UINT h, DWORD usage, D3DFORMAT fmt) {
    if (!tex || !(usage & D3DUSAGE_RENDERTARGET) || !is_float(fmt) || w < 512 || h < 512) return;
    // A free slot; if none, replace the oldest NON-eye-size one (the largest targets are the
    // scene's; a resolution change goes through Reset, which clears the list anyway).
    int slot = -1;
    for (int i = 0; i < kMax && slot < 0; ++i) if (!g_c[i].tex) slot = i;
    if (slot < 0) {
        for (int i = 0; i < kMax; ++i)
            if (g_c[i].w * g_c[i].h < w * h && (slot < 0 || g_c[i].serial < g_c[slot].serial)) slot = i;
        if (slot < 0) return;
    }
    if (g_c[slot].tex) g_c[slot].tex->Release();
    tex->AddRef();
    g_c[slot].tex = tex; g_c[slot].w = w; g_c[slot].h = h; g_c[slot].fmt = fmt; g_c[slot].serial = ++g_serial;
    DVR_INFO("depthprobe: candidate #%d - float render target %ux%u fmt=%d (%s)", g_serial, w, h, (int)fmt,
             g_on.load() ? "will be probed" : "probe off");
}

void fgproj_tick_fwd();
void tick(IDirect3DDevice9* dev, UINT backW, UINT backH) {
    fgproj_tick_fwd();   // VR-39: the arms-FOV instrument's log (render thread, every present)
    fgmask_beat();       // VR-39 run 17: the foreground mask's counters
    const bool now = g_now.exchange(false);
    if (!dev || (!g_on.load() && !now)) return;
    const DWORD t = GetTickCount();
    if (!g_nextMs) g_nextMs = t + kFirstMs;
    if (!now) {
        if (g_runs >= kRuns || (int)(t - g_nextMs) < 0) return;
        ++g_runs;
    }
    g_nextMs = t + kEveryMs;
    int probed = 0;
    for (const Cand& c : g_c) {
        if (!c.tex) continue;
        // eye-size and half-size targets (UE3 keeps half-resolution copies for its effects)
        if (backW && !((c.w == backW && c.h == backH) || (c.w * 2 == backW && c.h * 2 == backH))) continue;
        probe_one(dev, c); ++probed;
    }
    if (!probed)
        for (const Cand& c : g_c) if (c.tex) { probe_one(dev, c); ++probed; }
    DVR_INFO("depthprobe: run %d%s - %d target(s) read at %ux%u; read the alpha rows against where you stood "
             "(near wall, open street, looking down at your hands)", g_runs, now ? " (asked)" : "", probed, backW, backH);
}

// ---- step 2: the shared depth ----------------------------------------------------
// A ring of three: each present's copy is keyed by the serial the colour grab of the SAME
// present will carry (capture::serial() + 1), so step 3 can take the depth that belongs to
// whichever grab the capture delivers (this present's, or the previous one's in shared mode).
std::atomic<bool> g_share{false}, g_retry{false};
void retry() { g_retry.store(true); }
const int kRing = 3;
struct Slot {
    dvr::capture::interop::Image img;              // D3D9 owner + surface, D3D11 texture
    IDirect3DQuery9* fence = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t serial = 0;
    bool fenced = false;
    ID3D11Query* readFence = nullptr;
    bool reading = false, borrowed = false;
};
Slot g_ring[kRing];
int g_ringNext = 0;
// VR-39: the scene target as it was when the FOREGROUND pass (the player's arms and weapon, drawn with a
// crushed viewport depth range) began. A texel whose depth changed after that was drawn by the foreground:
// the AFW rebuild's foreground mask, whatever the distance (the depth threshold it replaces put a far sword
// tip into the world and a near wall into the foreground). Keyed like the depth ring.
Slot g_pre[kRing];
int g_preNext = 0;
std::atomic<bool> g_preWanted{false};
uint32_t g_preKey = 0;            // the serial the current frame's snapshot went to (once per frame)
uint64_t g_preCopies = 0, g_preMissed = 0, g_preNotScene = 0, g_preFailed = 0;
IDirect3DSurface9* g_sceneSurf = nullptr;   // the scene target's level 0, owned (released with the ring)
ID3D11Texture2D* g_depthStage = nullptr;           // 5x5 staging for the check
UINT g_depthW = 0, g_depthH = 0;
bool g_shareFailed = false;
uint64_t g_shareCopies = 0, g_shareChecks = 0, g_shareAgree = 0, g_shareMissed = 0;
DWORD g_shareNextMs = 0;

void pre_release() {
    for (Slot& r : g_pre) {
        if (r.srv) r.srv->Release();
        if (r.fence) r.fence->Release();
        if (r.readFence) r.readFence->Release();
        r.readFence = nullptr; r.reading = r.borrowed = false;
        r.img.reset();
        r.srv = nullptr; r.fence = nullptr; r.serial = 0; r.fenced = false;
    }
    if (g_sceneSurf) { g_sceneSurf->Release(); g_sceneSurf = nullptr; }
    g_preKey = 0;
}
void share_release() {
    pre_release();
    if (g_depthStage) { g_depthStage->Release(); g_depthStage = nullptr; }
    for (Slot& r : g_ring) {
        if (r.srv) r.srv->Release();
        if (r.fence) r.fence->Release();
        if (r.readFence) r.readFence->Release();
        r.readFence = nullptr; r.reading = r.borrowed = false;
        r.img.reset();
        r.srv = nullptr; r.fence = nullptr; r.serial = 0; r.fenced = false;
    }
    g_depthW = g_depthH = 0;
}

void on_reset() {
    fgmask_reset();   // VR-39 run 17: DEFAULT-pool shared slots (the hkReset LAW)
    share_release();
    g_shareFailed = false;
    for (Cand& c : g_c) { if (c.tex) c.tex->Release(); c = Cand{}; }
    for (Scratch& s : g_s) {
        if (s.sys) s.sys->Release();
        if (s.rt) s.rt->Release();
        s = Scratch{};
    }
}

void set_share(bool on, const char* who) {
    g_share.store(on); g_shareFailed = false;
    DVR_INFO("depthshare: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - the scene target's depth is copied to D3D11 every present; checked every 5 s" : "");
}
bool share_on() { return g_share.load(); }
bool share_tick_needed() {
    return g_share.load() || (dvr::clarity::temporal_on() && dvr::clarity::motion_on()) || dvr::dlss::mode() != 0 || g_depthW != 0 || g_shareFailed;
}

void share_tick(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx11, UINT backW, UINT backH) {
    if (g_retry.exchange(false)) g_shareFailed = false;
    const bool wanted = g_share.load() || (dvr::clarity::temporal_on() && dvr::clarity::motion_on()) || dvr::dlss::mode() != 0;
    if (!wanted) { if (g_depthW) share_release(); g_shareFailed = false; return; }
    if (g_shareFailed || !dev || !dev11 || !ctx11 || !backW) return;
    // The scene target: the first-created eye-size RGBA16F (step 1: its alpha is depth).
    const Cand* scene = nullptr;
    for (const Cand& c : g_c)
        if (c.tex && c.fmt == D3DFMT_A16B16G16R16F && c.w == backW && c.h == backH && (!scene || c.serial < scene->serial))
            scene = &c;
    if (!scene) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                         "depthshare: no eye-size RGBA16F target (%ux%u) seen yet - nothing to share", backW, backH);
        return;
    }
    if (g_depthW != scene->w || g_depthH != scene->h || !g_ring[0].img.texture || (g_preWanted.load() && !g_pre[0].img.texture)) {
        share_release();
        HRESULT hr = S_OK; const char* step = "";
        for (Slot& r : g_ring) {
            const auto cr = dvr::capture::interop::create(dev, dev11, scene->w, scene->h, D3DFMT_A16B16G16R16F, r.img);
            if (FAILED(cr.hr)) { hr = cr.hr; step = cr.step; break; }
            if (FAILED(hr = dev->CreateQuery(D3DQUERYTYPE_EVENT, &r.fence))) { step = "fence"; break; }
            if (FAILED(hr = dev11->CreateShaderResourceView(r.img.texture, nullptr, &r.srv))) { step = "SRV"; break; }
            D3D11_QUERY_DESC qd = {D3D11_QUERY_EVENT, 0};
            if (FAILED(hr = dev11->CreateQuery(&qd, &r.readFence))) { step = "read fence"; break; }
        }
        if (SUCCEEDED(hr) && g_preWanted.load())
            for (Slot& r : g_pre) {
                const auto cr = dvr::capture::interop::create(dev, dev11, scene->w, scene->h, D3DFMT_A16B16G16R16F, r.img);
                if (FAILED(cr.hr)) { hr = cr.hr; step = "foreground ring"; break; }
                if (FAILED(hr = dev->CreateQuery(D3DQUERYTYPE_EVENT, &r.fence))) { step = "foreground fence"; break; }
                if (FAILED(hr = dev11->CreateShaderResourceView(r.img.texture, nullptr, &r.srv))) { step = "foreground SRV"; break; }
                D3D11_QUERY_DESC qd = {D3D11_QUERY_EVENT, 0};
                if (FAILED(hr = dev11->CreateQuery(&qd, &r.readFence))) { step = "foreground read fence"; break; }
            }
        if (SUCCEEDED(hr) && g_preWanted.load()) scene->tex->GetSurfaceLevel(0, &g_sceneSurf);
        D3D11_TEXTURE2D_DESC sd = {};
        sd.Width = kGrid; sd.Height = kGrid; sd.MipLevels = 1; sd.ArraySize = 1;
        sd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (SUCCEEDED(hr) && FAILED(hr = dev11->CreateTexture2D(&sd, nullptr, &g_depthStage))) step = "staging";
        if (FAILED(hr)) {
            DVR_WARN("depthshare: REFUSED - shared %ux%u RGBA16F ring, %s (0x%08lx); the depth stays on D3D9",
                     scene->w, scene->h, step, (unsigned long)hr);
            share_release(); g_shareFailed = true; return;
        }
        g_depthW = scene->w; g_depthH = scene->h; g_ringNext = 0;
        DVR_INFO("depthshare: shared depth %ux%u RGBA16F live (target #%d), a ring of %d keyed by the colour grab's "
                 "serial; copied at every present, fenced%s", g_depthW, g_depthH, scene->serial, kRing,
                 g_sceneSurf ? "; plus the pre-foreground ring (the AFW foreground mask)" : "");
    }
    // A present which cannot copy must not expose a previous copy with the same
    // speculative serial (capture off/refusal does not advance capture::serial).
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "depthshare: copies %llu, unavailable/busy %llu, ring %.1f MiB; nonblocking fences | pre-foreground copies %llu, "
        "ring busy %llu, not the scene target %llu, refused %llu (0 copies with the arms in view = no foreground pass seen)",
        (unsigned long long)g_shareCopies, (unsigned long long)g_shareMissed,
        (double)g_depthW*g_depthH*8*kRing*(g_sceneSurf ? 2 : 1)/(1024*1024),
        (unsigned long long)g_preCopies, (unsigned long long)g_preMissed, (unsigned long long)g_preNotScene,
        (unsigned long long)g_preFailed);
    const uint32_t serial = dvr::capture::serial() + 1;
    for (Slot& s : g_ring) if (s.serial == serial) s.serial = 0;
    Slot* freeSlot = nullptr;
    for (int i = 0; i < kRing; ++i) {
        const int n = (g_ringNext + i) % kRing;
        Slot& s = g_ring[n];
        if (s.borrowed) continue;
        if (s.reading) {
            const HRESULT hr = ctx11->GetData(s.readFence, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (hr != S_OK) continue; // timeout/error is never permission to overwrite
            s.reading = false;
        }
        freeSlot = &s; g_ringNext = (n + 1) % kRing; break;
    }
    if (!freeSlot) { ++g_shareMissed; return; }
    Slot& r = *freeSlot;
    r.serial = 0;
    IDirect3DSurface9* src = nullptr;
    if (FAILED(scene->tex->GetSurfaceLevel(0, &src)) || !src) return;
    const DWORD t = GetTickCount();
    const bool check = g_share.load() && (int)(t - g_shareNextMs) >= 0;
    float d9[kGrid][kGrid] = {};
    bool have9 = false;
    if (check) have9 = read_grid_d3d9(dev, src, scene->w, scene->h, D3DFMT_A16B16G16R16F, d9);   // this present, D3D9
    dvr::perf::gpu_mark(dvr::perf::kGpuDepthA);
    const HRESULT hc = dev->StretchRect(src, nullptr, r.img.surface, nullptr, D3DTEXF_POINT);
    dvr::perf::gpu_mark(dvr::perf::kGpuDepthB);
    src->Release();
    if (FAILED(hc)) {
        r.serial = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "depthshare: the copy was refused (0x%08lx)", (unsigned long)hc);
        return;
    }
    ++g_shareCopies;
    if (FAILED(r.fence->Issue(D3DISSUE_END))) { ++g_shareMissed; return; }
    r.serial = serial;
    r.fenced = true;
    if (!check) return;
    g_shareNextMs = t + 5000;
    // The check only: wait for this copy, then read the same 25 texels on D3D11.
    const DWORD t0 = GetTickCount();
    while (r.fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE && GetTickCount() - t0 < 50) Sleep(0);
    if (r.fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK) return;
    r.borrowed = true;
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            const UINT x = (UINT)(scene->w * (0.1 + 0.2 * i)), y = (UINT)(scene->h * (0.1 + 0.2 * j));
            D3D11_BOX b = {x, y, 0, x + 1, y + 1, 1};
            ctx11->CopySubresourceRegion(g_depthStage, 0, i, j, 0, r.img.texture, 0, &b);
        }
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(ctx11->Map(g_depthStage, 0, D3D11_MAP_READ, 0, &m))) { read_done(ctx11); return; }
    float d11[kGrid][kGrid];
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            float v[4]; decode(D3DFMT_A16B16G16R16F, (const uint8_t*)m.pData + j * m.RowPitch + i * 8, v);
            d11[j][i] = v[3];
        }
    ctx11->Unmap(g_depthStage, 0);
    read_done(ctx11);
    ++g_shareChecks;
    float worst = 0.0f; char t11[400] = ""; int n = 0;
    for (int j = 0; j < kGrid; ++j) {
        n += _snprintf_s(t11 + n, sizeof(t11) - n, _TRUNCATE, "%s", j ? " /" : "");
        for (int i = 0; i < kGrid; ++i) {
            n += _snprintf_s(t11 + n, sizeof(t11) - n, _TRUNCATE, " %.4g", d11[j][i]);
            if (have9) { const float dd = fabsf(d11[j][i] - d9[j][i]); if (dd > worst || dd != dd) worst = dd != dd ? INFINITY : dd; }
        }
    }
    const bool agree = have9 && worst == 0.0f;
    if (agree) ++g_shareAgree;
    DVR_INFO("depthshare: check %llu - D3D11 depth 5x5:%s | %s (worst diff %.4g) | copies %llu, agreed %llu of %llu, "
             "step-3 lookups that found no depth for their grab %llu",
             (unsigned long long)g_shareChecks, t11,
             !have9 ? "D3D9 read refused, no comparison" : agree ? "IDENTICAL to the game's own target this present"
                                                                   : "DIFFERS from the game's target",
             worst, (unsigned long long)g_shareCopies, (unsigned long long)g_shareAgree, (unsigned long long)g_shareChecks,
             (unsigned long long)g_shareMissed);
}

ID3D11ShaderResourceView* depth_srv_for(uint32_t grabSerial, UINT* w, UINT* h) {
    Slot* best = nullptr;
    for (Slot& r : g_ring) if (r.srv && r.serial == grabSerial) best = &r;
    if (!best) { ++g_shareMissed; return nullptr; }
    if (best->fenced) {   // the copy must have executed before D3D11 reads the shared texture
        // Submit once and poll once. Never stall a VR frame waiting for depth.
        const HRESULT q = best->fence->GetData(nullptr, 0, D3DGETDATA_FLUSH);
        if (q != S_OK) { ++g_shareMissed; return nullptr; }
        best->fenced = false;
    }
    if (w) *w = g_depthW;
    if (h) *h = g_depthH;
    best->borrowed = true;
    return best->srv;
}

std::atomic<unsigned> g_preOwners{0};
void set_prefg_wanted(unsigned owner, bool on) {
    const unsigned was = on ? g_preOwners.fetch_or(owner) : g_preOwners.fetch_and(~owner);
    const unsigned now = on ? (was | owner) : (was & ~owner);
    const bool want = now != 0;
    if (g_preWanted.exchange(want) != want) {
        DVR_INFO("depthshare: pre-foreground ring %s (owners: AFW %d, DLSS %d)", want ? "wanted" : "released",
                 (now & 1) ? 1 : 0, (now & 2) ? 1 : 0);
        if (!want) g_retry.store(true);
    }
}

// The foreground pass draws with a crushed depth range (MaxZ 0.001; the hands code keys on the same). Run 9 (build
// v1.0.1-173): the crushed viewport is SET while another target is still bound (about 48 crushed viewports a frame,
// none with the scene target at that moment, 870720 refusals, 0 copies), so the viewport only ARMS the snapshot and
// the copy is taken at the first draw under it that renders into the scene target.
bool g_crushArmed = false;
uint64_t g_lastPreCopyMs = 0;
bool g_preRtLogged = false;
void try_pre_copy(IDirect3DDevice9* dev) {
    if (!g_preWanted.load() || !g_sceneSurf || !dev) return;
    const uint32_t key = dvr::capture::serial() + 1;   // this frame's grab, as the depth ring keys it
    if (key == g_preKey) return;                       // once per frame: the first foreground draw
    // Run 10: slot 0 of the foreground pass is a 2750x2850 A8R8G8B8 target, never the RGBA16F scene target, yet the
    // arms' depth reaches the scene target's alpha - so it is bound in another slot (MRT). Every slot is checked.
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) return;
    bool scene = rt == g_sceneSurf;
    int sceneSlot = scene ? 0 : -1;
    for (DWORD i = 1; i < 4 && !scene; ++i) {
        IDirect3DSurface9* o = nullptr;
        if (SUCCEEDED(dev->GetRenderTarget(i, &o)) && o) { if (o == g_sceneSurf) { scene = true; sceneSlot = (int)i; } o->Release(); }
    }
    if (scene && sceneSlot > 0)
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Info, "depthshare: the foreground pass writes the scene target in render-target "
                     "slot %d (slot 0 is its colour target): the AFW foreground mask is armed there", sceneSlot);
    if (!scene && !g_preRtLogged && g_preNotScene > 2000 && g_preCopies == 0) {
        // Name the target the foreground actually draws into, once, if it never is the scene target.
        g_preRtLogged = true;
        D3DSURFACE_DESC a = {}, b = {};
        rt->GetDesc(&a); g_sceneSurf->GetDesc(&b);
        DVR_WARN("depthshare: the foreground pass's draws render into %ux%u fmt %d usage 0x%lx (slots 1-3 hold no scene target "
                 "either), never the scene target %ux%u fmt %d (%llu refusals, 0 copies) - the AFW foreground mask is unavailable, "
                 "the depth limit is used", a.Width, a.Height, (int)a.Format, (unsigned long)a.Usage, b.Width, b.Height,
                 (int)b.Format, (unsigned long long)g_preNotScene);
    }
    rt->Release();
    if (!scene) { ++g_preNotScene; return; }
    g_preKey = key;
    for (Slot& s : g_pre) if (s.serial == key) s.serial = 0;
    Slot* slot = nullptr;
    for (int i = 0; i < kRing; ++i) {
        Slot& s = g_pre[(g_preNext + i) % kRing];
        if (s.borrowed || s.reading) continue;   // the D3D11 side still reads it: never overwritten under it
        slot = &s; g_preNext = (g_preNext + i + 1) % kRing; break;
    }
    if (!slot) { ++g_preMissed; return; }
    slot->serial = 0;
    if (FAILED(dev->StretchRect(g_sceneSurf, nullptr, slot->img.surface, nullptr, D3DTEXF_POINT))) {
        ++g_preFailed;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "depthshare: the pre-foreground copy was refused");
        return;
    }
    if (FAILED(slot->fence->Issue(D3DISSUE_END))) { ++g_preFailed; return; }
    slot->serial = key; slot->fenced = true;
    ++g_preCopies;
    g_lastPreCopyMs = GetTickCount64();
}

// VR-39: WHICH FOV ARE THE ARMS DRAWN WITH? The question two instruments answered two ways (the run-6 stereo
// disparity said 108.07; the run-13 force test and the scripts' m_fCurFOV_Arms said the lever's 103). This reads
// it off the draws themselves: the horizontal half-angle tangent of the view-projection in c0..c3 (the game's
// upload, before our jitter) is 1 / |column 0 of rows 0..2|, the same arithmetic that gives the capture's tanH
// from fresh.vp. Draws under a crushed-depth viewport (MaxZ < 0.5: the foreground DPG, the arms and weapon) are
// binned apart from the rest (the world), each into 0.1-degree bins, and the busiest bins of each class are
// logged. It CAN say the unwelcome thing: a foreground peak that is not the world's peak is an arms FOV of its
// own, and it says so with the number. c0..c3 is re-uploaded per pass and object, so not every sample is a view
// projection: only perspective matrices (column 3 non-zero) at 20-160 deg count, and the histogram shows the rest
// as separate peaks rather than averaging them in. World draws are sampled 1 in 8 (cost); foreground draws all.
namespace {
constexpr int kFpBins = 1400;                 // 20.0 .. 160.0 deg in 0.1
uint32_t g_fpHist[2][kFpBins];
uint64_t g_fpSeen[2], g_fpRefused[2];
bool     g_fpCrushed = false;
DWORD    g_fpVpW = 0;
uint32_t g_fpTick = 0;
DWORD    g_fpNextLogMs = 0;
int      g_fpSaidFg = -1, g_fpSaidWorld = -1;
std::atomic<bool> g_fpOn{true};
void fp_sample() {
    const int cls = g_fpCrushed ? 1 : 0;
    if (!cls && (++g_fpTick & 7)) return;
    if (g_fpVpW < 512) return;                                    // shadow maps, small targets
    const float* r0 = ::dvr::frame::vs_const_shadow_row(0);
    const float* r1 = ::dvr::frame::vs_const_shadow_row(1);
    const float* r2 = ::dvr::frame::vs_const_shadow_row(2);
    if (!r0 || !r1 || !r2) return;
    const float w = fabsf(r0[3]) + fabsf(r1[3]) + fabsf(r2[3]);   // perspective: w depends on position
    const float n2 = r0[0] * r0[0] + r1[0] * r1[0] + r2[0] * r2[0];
    if (!(w > 1e-4f) || !(n2 > 1e-8f) || !(n2 < 1e8f)) { ++g_fpRefused[cls]; return; }
    const float deg = 2.0f * atanf(1.0f / sqrtf(n2)) * 57.2957795f;
    const int b = (int)((deg - 20.0f) * 10.0f + 0.5f);
    if (b < 0 || b >= kFpBins) { ++g_fpRefused[cls]; return; }
    ++g_fpHist[cls][b]; ++g_fpSeen[cls];
}
// The three busiest bins, "103.0 x812 (61%)".
void fp_peaks(int cls, char* out, size_t n, int* top) {
    int best[3] = {-1, -1, -1};
    for (int b = 0; b < kFpBins; ++b) {
        const uint32_t c = g_fpHist[cls][b];
        if (!c) continue;
        for (int k = 0; k < 3; ++k)
            if (best[k] < 0 || c > g_fpHist[cls][best[k]]) { for (int j = 2; j > k; --j) best[j] = best[j - 1]; best[k] = b; break; }
    }
    *top = best[0];
    size_t used = 0; out[0] = 0;
    for (int k = 0; k < 3 && best[k] >= 0; ++k)
        used += _snprintf(out + used, n - used, "%s%.1f x%u (%.0f%%)", k ? ", " : "", 20.0f + best[k] * 0.1f,
                          g_fpHist[cls][best[k]], 100.0 * g_fpHist[cls][best[k]] / (double)(g_fpSeen[cls] ? g_fpSeen[cls] : 1));
    if (!out[0]) _snprintf(out, n, "none");
    out[n - 1] = 0;
}
void fp_tick() {
    if (!g_fpOn.load()) return;
    const DWORD t = GetTickCount();
    if (t < g_fpNextLogMs) return;
    const bool first = g_fpNextLogMs == 0;
    g_fpNextLogMs = t + 5000;
    if (first) return;
    if (!g_fpSeen[0] && !g_fpSeen[1]) return;                 // menus, loads: nothing drawn in perspective
    char world[160], fg[160]; int tw = -1, tf = -1;
    fp_peaks(0, world, sizeof(world), &tw);
    fp_peaks(1, fg, sizeof(fg), &tf);
    const float wDeg = tw >= 0 ? 20.0f + tw * 0.1f : 0.0f, fDeg = tf >= 0 ? 20.0f + tf * 0.1f : 0.0f;
    const bool changed = tw != g_fpSaidWorld || tf != g_fpSaidFg;
    static DWORD nextBeat = 0;
    if (changed || t >= nextBeat) {
        nextBeat = t + 30000;
        g_fpSaidWorld = tw; g_fpSaidFg = tf;
        DVR_INFO("fgproj: the draws' own projection over 5 s - WORLD hfov %s [%llu samples, %llu not a projection] | "
                 "FOREGROUND (MaxZ<0.5: arms, weapon) hfov %s [%llu, %llu] -> %s", world,
                 (unsigned long long)g_fpSeen[0], (unsigned long long)g_fpRefused[0], fg,
                 (unsigned long long)g_fpSeen[1], (unsigned long long)g_fpRefused[1],
                 tf < 0 ? "NO foreground draw seen in perspective: the arms FOV is unmeasured this window"
                 : fabsf(fDeg - wDeg) < 0.25f ? "the arms are drawn at the WORLD's FOV: AFW should reproject them with it"
                 : "the arms have their OWN FOV (the foreground peak): AFW must reproject them with that number");
    }
    memset(g_fpHist, 0, sizeof(g_fpHist));
    memset(g_fpSeen, 0, sizeof(g_fpSeen));
    memset(g_fpRefused, 0, sizeof(g_fpRefused));
}
} // namespace

void note_viewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp) {
    if (!vp || fgmask_in_draw()) return;   // our own mask redraw restoring the game's viewport
    g_crushArmed = vp->MaxZ < 0.5f;
    g_fpCrushed = vp->MaxZ < 0.5f; g_fpVpW = vp->Width;
    if (g_crushArmed) try_pre_copy(dev);
}
void note_draw(IDirect3DDevice9* dev) {
    if (g_fpOn.load(std::memory_order_relaxed)) fp_sample();
    if (g_crushArmed) try_pre_copy(dev);
}

ID3D11ShaderResourceView* prefg_srv_for(uint32_t grabSerial, bool* sawForeground) {
    if (sawForeground) *sawForeground = false;
    Slot* best = nullptr;
    for (Slot& r : g_pre) if (r.srv && r.serial == grabSerial) best = &r;
    if (!best) return nullptr;
    if (sawForeground) *sawForeground = true;
    if (best->fenced) {
        if (best->fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK) return nullptr;
        best->fenced = false;
    }
    best->borrowed = true;
    return best->srv;
}
// The mask is trusted only while the detector is producing copies: a frame with no copy then means no foreground
// pass (no arms in view). A detector that never copies (run 9) must not turn every frame into "no arms" - the
// rebuild then drew the hands at the world's FOV; it falls back to the depth limit instead.
bool prefg_ready() { return g_sceneSurf != nullptr && g_lastPreCopyMs && GetTickCount64() - g_lastPreCopyMs < 2000; }

void read_done(ID3D11DeviceContext* ctx) {
    if (!ctx) return;
    fgmask_read_done(ctx);
    bool issued = false;
    for (Slot* ring : {g_ring, g_pre})
        for (int i = 0; i < kRing; ++i) {
            Slot& s = ring[i];
            if (s.borrowed) { ctx->End(s.readFence); s.reading = true; s.borrowed = false; issued = true; }
        }
    if (issued) ctx->Flush();
}

void set_enabled(bool on, const char* who) {
    g_on.store(on);
    if (on) g_runs = 0;
    DVR_INFO("depthprobe: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - 30 reads, one every 10 s from 30 s after start, each a brief GPU sync" : "");
}
bool enabled() { return g_on.load(); }
void request(const char* who) { g_now.store(true); DVR_INFO("depthprobe: one read asked (%s)", who ? who : "?"); }
void fgproj_tick_fwd() { fp_tick(); }
bool command(const char* args) {
    if (args && !_stricmp(args, "fgmask on"))  { fgmask_set(true, "the seam"); return true; }
    if (args && !_stricmp(args, "fgmask off")) { fgmask_set(false, "the seam"); return true; }
    if (args && !_stricmp(args, "fgproj on"))  { g_fpOn = true;  DVR_INFO("fgproj: ON (the seam)"); return true; }
    if (args && !_stricmp(args, "fgproj off")) { g_fpOn = false; DVR_INFO("fgproj: off (the seam)"); return true; }
    if (args && !_stricmp(args, "share on")) { set_share(true, "the seam"); return true; }
    if (args && !_stricmp(args, "share off")) { set_share(false, "the seam"); return true; }
    if (args && !_stricmp(args, "on")) set_enabled(true, "the seam");
    else if (args && !_stricmp(args, "off")) set_enabled(false, "the seam");
    else request("the seam");
    return true;
}


// ---- VR-39 run 17: THE FOREGROUND MASK, DRAWN ----------------------------------------------------------------------
// AFW told the player's hands and weapon from the world by depth alone (nearer than 0.30 units, about 0.69 m): a close
// NPC's face was rebuilt as hands and split against the rest of it, and the far half of a blade pointed away was world
// and trailed every turn. The pre-foreground copy that was meant to replace the limit never engaged: the foreground
// pass draws into the game's A8R8G8B8 target, never the scene target (run 10).
//
// So each foreground draw (a crushed-depth viewport, MaxZ < 0.5, at least 512 wide: the draws `fgproj:` measures) is
// issued a SECOND time, from inside orig_draw_* so the state is exactly what was drawn (the weapon attachment sets its
// own constants), into a mask target: the game's vertex shader and depth test, a constant pixel shader, no depth or
// stencil writes, no blending. The mask is at the render size, keyed by the capture serial like the marker layer
// (six fenced shared slots), and AFW signs its depth snapshot with it instead of the depth limit.
namespace {
struct MaskSlot {
    dvr::capture::interop::Image image;
    IDirect3DQuery9* fence = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    ID3D11Query* read = nullptr;
    bool pending = false, reading = false, borrowed = false;
    uint32_t serial = 0, draws = 0;
};
MaskSlot g_mask[6];
int g_maskCur = -1, g_maskNext = 0;
uint32_t g_maskW = 0, g_maskH = 0;
bool g_maskFailed = false, g_maskIn = false;
IDirect3DPixelShader9* g_maskPs2 = nullptr;
IDirect3DPixelShader9* g_maskPs3 = nullptr;
std::atomic<bool> g_maskOn{true};           // `depthprobe fgmask on|off`
uint64_t g_maskDraws = 0, g_maskSizeSkip = 0, g_maskFrames = 0, g_maskServed = 0, g_maskNotReady = 0, g_maskNoSlot = 0;
// Saved across the redraw.
IDirect3DSurface9* g_mSavedRt[4] = {};
IDirect3DPixelShader9* g_mSavedPs = nullptr;
D3DVIEWPORT9 g_mSavedVp = {};
DWORD g_mSavedRs[9] = {};
// ZFUNC LESSEQUAL: the draw already wrote its depth, and the identical draw lands on it exactly.
const D3DRENDERSTATETYPE kMaskRs[9] = {D3DRS_ZWRITEENABLE, D3DRS_ALPHABLENDENABLE, D3DRS_ALPHATESTENABLE, D3DRS_COLORWRITEENABLE,
                                       D3DRS_SRGBWRITEENABLE, D3DRS_STENCILWRITEMASK, D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_FOGENABLE,
                                       D3DRS_ZFUNC};
const DWORD kMaskRsVal[9] = {FALSE, FALSE, FALSE, 0xF, FALSE, 0, FALSE, FALSE, D3DCMP_LESSEQUAL};
// def c0, 1, 1, 1, 1 ; mov oC0, c0 - hand-assembled (ps_2_0 and ps_3_0 differ only in the version token): a vs_3_0 must
// be paired with a ps_3_0, anything older with ps_2_0.
const DWORD kMaskPsBody[] = {0x05000051, 0xA00F0000, 0x3F800000, 0x3F800000, 0x3F800000, 0x3F800000,
                             0x02000001, 0x800F0800, 0xA0E40000, 0x0000FFFF};
IDirect3DPixelShader9* mask_ps(IDirect3DDevice9* dev, DWORD version) {
    IDirect3DPixelShader9*& ps = version == 0xFFFF0300 ? g_maskPs3 : g_maskPs2;
    if (!ps) {
        DWORD code[1 + sizeof(kMaskPsBody) / sizeof(DWORD)];
        code[0] = version;
        memcpy(code + 1, kMaskPsBody, sizeof(kMaskPsBody));
        const HRESULT hr = dev->CreatePixelShader(code, &ps);
        if (FAILED(hr)) {
            ps = nullptr;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "fgmask: the constant pixel shader (version %08lx) was refused "
                             "(0x%08lx) - AFW keeps the depth limit", (unsigned long)version, (unsigned long)hr);
        }
    }
    return ps;
}
void mask_release_all() {
    for (MaskSlot& s : g_mask) {
        if (s.srv) s.srv->Release(); if (s.fence) s.fence->Release(); if (s.read) s.read->Release();
        s.srv = nullptr; s.fence = nullptr; s.read = nullptr; s.image.reset();
        s.pending = s.reading = s.borrowed = false; s.serial = s.draws = 0;
    }
    if (g_maskPs2) { g_maskPs2->Release(); g_maskPs2 = nullptr; }
    if (g_maskPs3) { g_maskPs3->Release(); g_maskPs3 = nullptr; }
    g_maskCur = -1; g_maskNext = 0; g_maskW = g_maskH = 0; g_maskIn = false;
}
} // namespace

bool fgmask_on() { return g_maskOn.load(); }
void fgmask_reset() { mask_release_all(); g_maskFailed = false; }
bool fgmask_wanted() { return g_maskOn.load() && (g_preOwners.load() & 1u) != 0; }   // AFW's owner bit (its foreground mask on)
void fgmask_set(bool on, const char* who) {
    if (g_maskOn.exchange(on) != on)
        DVR_INFO("fgmask: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
                 on ? " - the foreground draws are drawn again into a mask; AFW takes the hands and weapon from it"
                    : " - AFW falls back to the depth limit (a close face is rebuilt as hands, a far blade as world)");
}

void fgmask_prepare(IDirect3DDevice9* d9, ID3D11Device* d11, ID3D11DeviceContext* ctx, uint32_t w, uint32_t h) {
    g_maskCur = -1;
    if (!fgmask_wanted() || !d9 || !d11 || !ctx || !w || !h || g_maskFailed) return;
    if (w != g_maskW || h != g_maskH) {
        mask_release_all();
        HRESULT hr = S_OK;
        const char* step = "";
        for (MaskSlot& s : g_mask) {
            const auto cr = dvr::capture::interop::create(d9, d11, w, h, D3DFMT_A8R8G8B8, s.image);
            hr = cr.hr; step = cr.step;
            if (SUCCEEDED(hr)) { step = "fence"; hr = d9->CreateQuery(D3DQUERYTYPE_EVENT, &s.fence); }
            if (SUCCEEDED(hr)) { step = "view"; hr = d11->CreateShaderResourceView(s.image.texture, nullptr, &s.srv); }
            D3D11_QUERY_DESC q = {D3D11_QUERY_EVENT, 0};
            if (SUCCEEDED(hr)) { step = "read query"; hr = d11->CreateQuery(&q, &s.read); }
            if (FAILED(hr)) break;
        }
        if (FAILED(hr)) {
            mask_release_all(); g_maskFailed = true;
            DVR_WARN("fgmask: the mask slots %ux%u were refused at %s (0x%08lx) - AFW keeps the depth limit until a reset",
                     w, h, step, (unsigned long)hr);
            return;
        }
        g_maskW = w; g_maskH = h;
        DVR_INFO("fgmask: six %ux%u mask slots (shared D3D9 -> D3D11, fenced, keyed by the capture serial)", w, h);
    }
    MaskSlot& s = g_mask[g_maskNext];
    if (s.borrowed) { ++g_maskNoSlot; return; }
    if (s.reading) {
        if (ctx->GetData(s.read, nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) { ++g_maskNoSlot; return; }
        s.reading = false;
    }
    if (s.pending) {
        if (s.fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK) { ++g_maskNoSlot; return; }
        s.pending = false;
    }
    s.serial = s.draws = 0;
    if (FAILED(d9->ColorFill(s.image.surface, nullptr, 0))) { ++g_maskNoSlot; return; }
    g_maskCur = g_maskNext; g_maskNext = (g_maskNext + 1) % 6;
    ++g_maskFrames;
}

bool fgmask_begin(IDirect3DDevice9* dev) {
    if (g_maskIn || g_maskCur < 0 || !g_fpCrushed || g_fpVpW < 512 || !dev) return false;
    MaskSlot& s = g_mask[g_maskCur];
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &rt)) || !rt) return false;
    D3DSURFACE_DESC d = {};
    rt->GetDesc(&d);
    if (d.Width != g_maskW || d.Height != g_maskH) { rt->Release(); ++g_maskSizeSkip; return false; }
    IDirect3DVertexShader9* vs = nullptr;
    DWORD version = 0xFFFF0200;
    if (SUCCEEDED(dev->GetVertexShader(&vs)) && vs) {
        static IDirect3DVertexShader9* lastVs = nullptr;   // identity only (never dereferenced): the version of the last one read
        static DWORD lastVersion = 0xFFFF0200;
        if (vs != lastVs) {
            UINT n = 0;
            std::vector<DWORD> f;
            if (SUCCEEDED(vs->GetFunction(nullptr, &n)) && n >= 4) {
                f.resize((n + 3) / 4);
                if (SUCCEEDED(vs->GetFunction(f.data(), &n)))
                    lastVersion = (f[0] & 0xFFFFFF00) == 0xFFFE0300 ? 0xFFFF0300 : 0xFFFF0200;
            }
            lastVs = vs;
        }
        version = lastVersion;
        vs->Release();
    }
    IDirect3DPixelShader9* ps = mask_ps(dev, version);
    if (!ps) { rt->Release(); return false; }
    g_mSavedRt[0] = rt;
    for (DWORD i = 1; i < 4; ++i) { g_mSavedRt[i] = nullptr; dev->GetRenderTarget(i, &g_mSavedRt[i]); }
    dev->GetPixelShader(&g_mSavedPs);
    dev->GetViewport(&g_mSavedVp);
    for (int i = 0; i < 9; ++i) dev->GetRenderState(kMaskRs[i], &g_mSavedRs[i]);
    g_maskIn = true;   // our own SetViewport and SetRenderState pass through the viewport hook: it must ignore them
    dvr::frame::orig_set_render_target(dev, 0, s.image.surface);
    for (DWORD i = 1; i < 4; ++i) if (g_mSavedRt[i]) dvr::frame::orig_set_render_target(dev, i, nullptr);
    dev->SetViewport(&g_mSavedVp);   // a render-target change resets it
    dev->SetPixelShader(ps);
    for (int i = 0; i < 9; ++i) dev->SetRenderState(kMaskRs[i], kMaskRsVal[i]);
    return true;
}
void fgmask_end(IDirect3DDevice9* dev, HRESULT drawn) {
    if (!g_maskIn) return;
    MaskSlot& s = g_mask[g_maskCur];
    if (SUCCEEDED(drawn)) { ++s.draws; ++g_maskDraws; }
    for (int i = 0; i < 9; ++i) dev->SetRenderState(kMaskRs[i], g_mSavedRs[i]);
    dev->SetPixelShader(g_mSavedPs);
    for (DWORD i = 0; i < 4; ++i) {
        if (i == 0 || g_mSavedRt[i]) dvr::frame::orig_set_render_target(dev, i, g_mSavedRt[i]);
        if (g_mSavedRt[i]) { g_mSavedRt[i]->Release(); g_mSavedRt[i] = nullptr; }
    }
    if (g_mSavedPs) { g_mSavedPs->Release(); g_mSavedPs = nullptr; }
    dev->SetViewport(&g_mSavedVp);
    g_maskIn = false;
}
bool fgmask_in_draw() { return g_maskIn; }

void fgmask_seal(uint32_t serial) {
    for (MaskSlot& s : g_mask) if (s.serial == serial) s.serial = 0;
    if (g_maskCur < 0) return;
    MaskSlot& s = g_mask[g_maskCur];
    s.serial = serial;
    s.pending = s.fence && SUCCEEDED(s.fence->Issue(D3DISSUE_END));
    if (!s.pending) s.serial = 0;
    g_maskCur = -1;
}

ID3D11ShaderResourceView* fgmask_srv_for(uint32_t serial, uint32_t* draws, uint32_t* w, uint32_t* h) {
    if (draws) *draws = 0;
    if (!serial) return nullptr;
    for (MaskSlot& s : g_mask) {
        if (s.serial != serial || !s.srv) continue;
        if (s.pending) {
            if (s.fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) != S_OK) { ++g_maskNotReady; return nullptr; }
            s.pending = false;
        }
        if (draws) *draws = s.draws;
        if (w) *w = g_maskW;
        if (h) *h = g_maskH;
        s.borrowed = true;
        ++g_maskServed;
        return s.srv;
    }
    return nullptr;
}
void fgmask_read_done(ID3D11DeviceContext* ctx) {
    if (!ctx) return;
    bool issued = false;
    for (MaskSlot& s : g_mask)
        if (s.borrowed) { ctx->End(s.read); s.reading = true; s.borrowed = false; issued = true; }
    if (issued) ctx->Flush();
}
void fgmask_beat() {
    static DWORD next = 0;
    const DWORD t = GetTickCount();
    if (t < next) return;
    next = t + 5000;
    if (!fgmask_wanted() && !g_maskFrames) return;
    DVR_INFO("fgmask: %s - %llu frames armed, %llu foreground draws drawn again into the mask, %llu skipped (the target "
             "was not the render size), %llu masks served to AFW, %llu not finished in time, %llu frames without a free slot "
             "(served 0 while AFW runs = the depth limit is still deciding)",
             fgmask_wanted() ? "ON" : "off", (unsigned long long)g_maskFrames, (unsigned long long)g_maskDraws,
             (unsigned long long)g_maskSizeSkip, (unsigned long long)g_maskServed, (unsigned long long)g_maskNotReady,
             (unsigned long long)g_maskNoSlot);
    g_maskFrames = g_maskDraws = g_maskSizeSkip = g_maskServed = g_maskNotReady = g_maskNoSlot = 0;
}

} // namespace dvr::depthprobe
