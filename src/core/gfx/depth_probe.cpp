// core/gfx/depth_probe.cpp - see depth_probe.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/depth_probe.h"
#include "core/gfx/capture.h"
#include "core/gfx/shared_capture_texture.h"
#include "core/util/log.h"

#include <d3d11.h>

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

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

void tick(IDirect3DDevice9* dev, UINT backW, UINT backH) {
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
std::atomic<bool> g_share{false};
const int kRing = 3;
struct Slot {
    dvr::capture::interop::Image img;              // D3D9 owner + surface, D3D11 texture
    IDirect3DQuery9* fence = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t serial = 0;
    bool fenced = false;
};
Slot g_ring[kRing];
int g_ringNext = 0;
ID3D11Texture2D* g_depthStage = nullptr;           // 5x5 staging for the check
UINT g_depthW = 0, g_depthH = 0;
bool g_shareFailed = false;
uint64_t g_shareCopies = 0, g_shareChecks = 0, g_shareAgree = 0, g_shareMissed = 0;
DWORD g_shareNextMs = 0;

void share_release() {
    if (g_depthStage) { g_depthStage->Release(); g_depthStage = nullptr; }
    for (Slot& r : g_ring) {
        if (r.srv) r.srv->Release();
        if (r.fence) r.fence->Release();
        r.img.reset();
        r.srv = nullptr; r.fence = nullptr; r.serial = 0; r.fenced = false;
    }
    g_depthW = g_depthH = 0;
}

void on_reset() {
    share_release();
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

void share_tick(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx11, UINT backW, UINT backH) {
    if (!g_share.load() || g_shareFailed || !dev || !dev11 || !ctx11 || !backW) return;
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
    if (g_depthW != scene->w || g_depthH != scene->h || !g_ring[0].img.texture) {
        share_release();
        HRESULT hr = S_OK; const char* step = "";
        for (Slot& r : g_ring) {
            const auto cr = dvr::capture::interop::create(dev, dev11, scene->w, scene->h, D3DFMT_A16B16G16R16F, r.img);
            if (FAILED(cr.hr)) { hr = cr.hr; step = cr.step; break; }
            if (FAILED(hr = dev->CreateQuery(D3DQUERYTYPE_EVENT, &r.fence))) { step = "fence"; break; }
            if (FAILED(hr = dev11->CreateShaderResourceView(r.img.texture, nullptr, &r.srv))) { step = "SRV"; break; }
        }
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
                 "serial; copied at every present, fenced", g_depthW, g_depthH, scene->serial, kRing);
    }
    Slot& r = g_ring[g_ringNext];
    g_ringNext = (g_ringNext + 1) % kRing;
    IDirect3DSurface9* src = nullptr;
    if (FAILED(scene->tex->GetSurfaceLevel(0, &src)) || !src) return;
    const DWORD t = GetTickCount();
    const bool check = (int)(t - g_shareNextMs) >= 0;
    float d9[kGrid][kGrid] = {};
    bool have9 = false;
    if (check) have9 = read_grid_d3d9(dev, src, scene->w, scene->h, D3DFMT_A16B16G16R16F, d9);   // this present, D3D9
    const HRESULT hc = dev->StretchRect(src, nullptr, r.img.surface, nullptr, D3DTEXF_POINT);
    src->Release();
    if (FAILED(hc)) {
        r.serial = 0;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "depthshare: the copy was refused (0x%08lx)", (unsigned long)hc);
        return;
    }
    ++g_shareCopies;
    r.serial = dvr::capture::serial() + 1;   // the grab later in this present takes this serial
    r.fence->Issue(D3DISSUE_END);
    r.fenced = true;
    if (!check) return;
    g_shareNextMs = t + 5000;
    // The check only: wait for this copy, then read the same 25 texels on D3D11.
    const DWORD t0 = GetTickCount();
    while (r.fence->GetData(nullptr, 0, D3DGETDATA_FLUSH) == S_FALSE && GetTickCount() - t0 < 50) Sleep(0);
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            const UINT x = (UINT)(scene->w * (0.1 + 0.2 * i)), y = (UINT)(scene->h * (0.1 + 0.2 * j));
            D3D11_BOX b = {x, y, 0, x + 1, y + 1, 1};
            ctx11->CopySubresourceRegion(g_depthStage, 0, i, j, 0, r.img.texture, 0, &b);
        }
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(ctx11->Map(g_depthStage, 0, D3D11_MAP_READ, 0, &m))) return;
    float d11[kGrid][kGrid];
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            float v[4]; decode(D3DFMT_A16B16G16R16F, (const uint8_t*)m.pData + j * m.RowPitch + i * 8, v);
            d11[j][i] = v[3];
        }
    ctx11->Unmap(g_depthStage, 0);
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
        const DWORD t0 = GetTickCount();
        HRESULT q;
        while ((q = best->fence->GetData(nullptr, 0, D3DGETDATA_FLUSH)) == S_FALSE && GetTickCount() - t0 < 20) Sleep(0);
        if (q == S_FALSE) { ++g_shareMissed; return nullptr; }
        best->fenced = false;
    }
    if (w) *w = g_depthW;
    if (h) *h = g_depthH;
    return best->srv;
}

void set_enabled(bool on, const char* who) {
    g_on.store(on);
    if (on) g_runs = 0;
    DVR_INFO("depthprobe: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - 30 reads, one every 10 s from 30 s after start, each a brief GPU sync" : "");
}
bool enabled() { return g_on.load(); }
void request(const char* who) { g_now.store(true); DVR_INFO("depthprobe: one read asked (%s)", who ? who : "?"); }
bool command(const char* args) {
    if (args && !_stricmp(args, "share on")) { set_share(true, "the seam"); return true; }
    if (args && !_stricmp(args, "share off")) { set_share(false, "the seam"); return true; }
    if (args && !_stricmp(args, "on")) set_enabled(true, "the seam");
    else if (args && !_stricmp(args, "off")) set_enabled(false, "the seam");
    else request("the seam");
    return true;
}

} // namespace dvr::depthprobe
