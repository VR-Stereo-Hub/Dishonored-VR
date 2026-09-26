// core/gfx/sampler_force.cpp - see sampler_force.h. Runs on whichever thread the
// game draws from (one thread in practice: UE3's rendering thread); the levers
// are atomics, the per-stage bookkeeping belongs to that thread.
#define DVR_CAT ::dvr::log::Cat::device
#include "core/gfx/sampler_force.h"
#include "core/util/log.h"

#include <atomic>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::samplers {
namespace {

std::atomic<int>  g_aniso{0};
std::atomic<bool> g_trilinear{false};

// Pixel samplers 0..15, the displacement-map sampler, vertex samplers 0..3.
const int kStages = 21;
int stage_index(DWORD s) {
    if (s < 16) return (int)s;
    if (s == D3DDMAPSAMPLER) return 16;
    if (s >= D3DVERTEXTEXTURESAMPLER0 && s <= D3DVERTEXTEXTURESAMPLER3) return 17 + (int)(s - D3DVERTEXTEXTURESAMPLER0);
    return -1;
}

// What the game asked for, and what the device was last given, per stage.
// D3D9's defaults: MINFILTER POINT, MIPFILTER NONE, MAXANISOTROPY 1.
struct Stage {
    DWORD gameMin = D3DTEXF_POINT, gameMip = D3DTEXF_NONE, gameAniso = 1;
    DWORD devMip = D3DTEXF_NONE, devAniso = 1;
};
Stage g_stage[kStages];

// Counters for the status line (read racily by the logger; they are counts).
std::atomic<uint32_t> g_anisoSets{0}, g_anisoRaised{0}, g_mipUpgraded{0}, g_gameAnisoMax{0};
uint64_t g_lastLogMs = 0;

DWORD want_aniso(const Stage& st) {
    const int lever = g_aniso.load(std::memory_order_relaxed);
    if (lever > 0 && st.gameMin == D3DTEXF_ANISOTROPIC && (DWORD)lever > st.gameAniso) return (DWORD)lever;
    return st.gameAniso;
}
DWORD want_mip(const Stage& st) {
    if (g_trilinear.load(std::memory_order_relaxed) && st.gameMin == D3DTEXF_ANISOTROPIC && st.gameMip == D3DTEXF_POINT)
        return D3DTEXF_LINEAR;
    return st.gameMip;
}

void periodic() {
    const int lever = g_aniso.load(std::memory_order_relaxed);
    const bool tri = g_trilinear.load(std::memory_order_relaxed);
    if (lever <= 0 && !tri) return;
    const uint64_t now = GetTickCount64();
    if (now - g_lastLogMs < 10000) return;
    g_lastLogMs = now;
    log_status("10 s");
}

} // namespace

void set_anisotropy(int degree, const char* who) {
    if (degree < 0) degree = 0;
    if (degree == 1) degree = 0;
    if (degree > 16) degree = 16;
    if (g_aniso.exchange(degree) != degree)
        DVR_INFO("samplers: anisotropy -> %s (live, %s; applies as the game binds textures, which is every frame)",
                 degree ? (degree == 16 ? "16x" : degree == 8 ? "8x" : degree == 4 ? "4x" : "2x") : "the game's own",
                 who ? who : "?");
}
int anisotropy() { return g_aniso.load(); }
void set_trilinear(bool on, const char* who) {
    if (g_trilinear.exchange(on) != on)
        DVR_INFO("samplers: trilinear mips -> %s (live, %s)", on ? "on" : "off (the game's own mip filter)", who ? who : "?");
}
bool trilinear() { return g_trilinear.load(); }

HRESULT set_sampler_state(IDirect3DDevice9* dev, DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value,
                          PFN_SetSamplerState orig) {
    const int i = stage_index(sampler);
    if (i < 0 || (type != D3DSAMP_MINFILTER && type != D3DSAMP_MIPFILTER && type != D3DSAMP_MAXANISOTROPY))
        return orig(dev, sampler, type, value);
    Stage& st = g_stage[i];
    HRESULT hr;
    if (type == D3DSAMP_MAXANISOTROPY) {
        st.gameAniso = value;
        g_anisoSets.fetch_add(1, std::memory_order_relaxed);
        if (value > g_gameAnisoMax.load(std::memory_order_relaxed)) g_gameAnisoMax.store(value, std::memory_order_relaxed);
        const DWORD w = want_aniso(st);
        if (w != value) g_anisoRaised.fetch_add(1, std::memory_order_relaxed);
        st.devAniso = w;
        hr = orig(dev, sampler, type, w);
    } else if (type == D3DSAMP_MIPFILTER) {
        st.gameMip = value;
        const DWORD w = want_mip(st);
        if (w != value) g_mipUpgraded.fetch_add(1, std::memory_order_relaxed);
        st.devMip = w;
        hr = orig(dev, sampler, type, w);
    } else {   // MINFILTER: the other two may now want different values on this stage
        st.gameMin = value;
        hr = orig(dev, sampler, type, value);
        const DWORD wa = want_aniso(st), wm = want_mip(st);
        if (wa != st.devAniso) {
            if (wa != st.gameAniso) g_anisoRaised.fetch_add(1, std::memory_order_relaxed);
            orig(dev, sampler, D3DSAMP_MAXANISOTROPY, wa);
            st.devAniso = wa;
        }
        if (wm != st.devMip) {
            if (wm != st.gameMip) g_mipUpgraded.fetch_add(1, std::memory_order_relaxed);
            orig(dev, sampler, D3DSAMP_MIPFILTER, wm);
            st.devMip = wm;
        }
    }
    periodic();
    return hr;
}

void on_reset() {
    for (Stage& st : g_stage) st = Stage{};
}

void log_status(const char* why) {
    const int lever = g_aniso.load();
    const uint32_t sets = g_anisoSets.exchange(0), raised = g_anisoRaised.exchange(0), mips = g_mipUpgraded.exchange(0);
    DVR_INFO("samplers (%s): anisotropy %s, trilinear mips %s | the game set MAXANISOTROPY %u times (highest it asked: "
             "%ux), %u sampler binds raised, %u point-mip binds made trilinear%s",
             why, lever ? (lever == 16 ? "16x" : lever == 8 ? "8x" : lever == 4 ? "4x" : "2x") : "the game's own",
             g_trilinear.load() ? "on" : "off", sets, g_gameAnisoMax.load(), raised, mips,
             (lever > 0 && raised == 0) ? " - NOTHING RAISED: no anisotropic sampler below the asked degree was bound "
                                          "(the lever has nothing to act on, or the game already asks this much)" : "");
}

bool command(const char* args) {
    char a[16] = "", b[16] = "";
    const int n = args ? sscanf(args, "%15s %15s", a, b) : 0;
    if (n >= 1 && !_stricmp(a, "trilinear")) {
        if (n >= 2) set_trilinear(!_stricmp(b, "on") || !strcmp(b, "1"), "the seam");
        log_status("asked");
        return true;
    }
    if (n >= 1 && (isdigit((unsigned char)a[0]) || !_stricmp(a, "off"))) {
        set_anisotropy(!_stricmp(a, "off") ? 0 : atoi(a), "the seam");
        return true;
    }
    log_status("asked");
    return true;
}

} // namespace dvr::samplers
