// core/gfx/depth_probe.cpp - see depth_probe.h.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/depth_probe.h"
#include "core/util/log.h"

#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::depthprobe {
namespace {

const int kMax = 6;            // candidates kept (the scene colour plus its siblings)
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
    float a[kGrid][kGrid], lum[kGrid][kGrid];
    const int tb = texel_bytes(c.fmt);
    for (int j = 0; j < kGrid; ++j)
        for (int i = 0; i < kGrid; ++i) {
            float v[4];
            decode(c.fmt, (const uint8_t*)lr.pBits + j * lr.Pitch + i * tb, v);
            a[j][i] = v[3];
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
}

} // namespace

void note_texture(IDirect3DTexture9* tex, UINT w, UINT h, DWORD usage, D3DFORMAT fmt) {
    if (!tex || !(usage & D3DUSAGE_RENDERTARGET) || !is_float(fmt) || w < 512 || h < 512) return;
    int slot = 0;
    for (int i = 0; i < kMax; ++i) {
        if (!g_c[i].tex) { slot = i; break; }
        if (g_c[i].serial < g_c[slot].serial) slot = i;   // replace the oldest
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
        // eye-size targets first; the rest only if nothing is eye-sized
        if (backW && (c.w != backW || c.h != backH)) continue;
        probe_one(dev, c); ++probed;
    }
    if (!probed)
        for (const Cand& c : g_c) if (c.tex) { probe_one(dev, c); ++probed; }
    DVR_INFO("depthprobe: run %d%s - %d target(s) read at %ux%u; read the alpha rows against where you stood "
             "(near wall, open street, looking down at your hands)", g_runs, now ? " (asked)" : "", probed, backW, backH);
}

void on_reset() {
    for (Cand& c : g_c) { if (c.tex) c.tex->Release(); c = Cand{}; }
    for (Scratch& s : g_s) {
        if (s.sys) s.sys->Release();
        if (s.rt) s.rt->Release();
        s = Scratch{};
    }
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
    if (args && !_stricmp(args, "on")) set_enabled(true, "the seam");
    else if (args && !_stricmp(args, "off")) set_enabled(false, "the seam");
    else request("the seam");
    return true;
}

} // namespace dvr::depthprobe
