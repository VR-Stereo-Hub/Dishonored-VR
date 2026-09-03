// core/gfx/frame_id.cpp - see frame_id.h.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/frame_id.h"

#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/util/log.h"

#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::frameid {
namespace {

constexpr uint32_t kThumb = 64;
constexpr uint32_t kPixels = kThumb * kThumb;
constexpr uint32_t kRecords = 16;     // records by capture serial (a ring)
constexpr uint32_t kReadBack = 3;     // presents between a copy and its read
constexpr uint32_t kRing = 4;         // readback objects per stage
constexpr uint32_t kBusyBlockAfter = 8;   // busy reads in a row before one blocking read
enum Stage { kBb = 0, kSlot, kOut, kSc, kStages };
const char* const kStageName[kStages] = {"bb", "slot", "out", "sc"};

struct Record {
    uint32_t serial = 0;
    bool     used = false;
    int      tag = 0;
    bool     c5Ok = false;
    float    c5[3] = {0, 0, 0};
    int      slot = -1;
    int      scTarget = -1;
    uint32_t scIndex = 0;
    uint8_t  mask = 0;      // stages whose thumbnail arrived
    uint8_t  tried = 0;     // stages whose read was attempted
    uint32_t sum[kStages] = {};
    uint8_t  luma[kStages][kPixels];
};
Record   g_rec[kRecords];
bool     g_enabled = true;
uint32_t g_curSerial = 0;          // the serial the stages slot/out/sc belong to this present
bool     g_curValid = false;
int      g_curTag = 0;
char     g_modeName[16] = "";
uint32_t g_lastEval = 0;
bool     g_lastEvalInit = false;
float    g_pendingC5[3] = {0, 0, 0};
bool     g_pendingC5Ok = false;

// ---- stage bb: the D3D9 ring -----------------------------------------------
IDirect3DDevice9*  g_dev9 = nullptr;
IDirect3DSurface9* g_rt9[kRing] = {};
IDirect3DSurface9* g_sys9[kRing] = {};
uint32_t           g_serial9[kRing] = {};
bool               g_issued9[kRing] = {};
bool               g_bbDead = false;      // the stage refused (logged once)
bool               g_bbLinear = true;     // StretchRect with LINEAR accepted

// ---- stages slot/out/sc: the D3D11 rings -----------------------------------
ID3D11Device*      g_dev11 = nullptr;
dvr::gfx::BlitQuad g_blit;
ID3D11Texture2D*        g_tex11[2][kRing] = {};     // slot, out: 64x64 RGBA render targets
ID3D11RenderTargetView* g_rtv11[2][kRing] = {};
ID3D11Texture2D*        g_staging[kStages][kRing] = {};   // per stage (bb unused)
uint32_t                g_serial11[kStages][kRing] = {};
bool                    g_issued11[kStages][kRing] = {};
DXGI_FORMAT             g_scFormat = DXGI_FORMAT_UNKNOWN;
bool                    g_dead11[kStages] = {};

// ---- the counters ----------------------------------------------------------
uint32_t g_busy[kStages] = {}, g_busyStreak[kStages] = {}, g_blocked[kStages] = {};
uint32_t g_missing[kStages] = {};
uint32_t g_pairs = 0, g_pairsWindow = 0;
uint32_t g_samePairs[kStages] = {}, g_samePairsWindow[kStages] = {};
uint32_t g_cmpWindow[kStages] = {};
double   g_diffSumWindow[kStages] = {};
float    g_diffMinWindow[kStages] = {}, g_diffMaxWindow[kStages] = {};
float    g_lastDiff[kStages] = {};
double   g_floorSumWindow = 0.0; uint32_t g_floorN = 0;
double   g_c5SumWindow = 0.0; uint32_t g_c5N = 0;
uint32_t g_slotRepeats = 0, g_scRepeats = 0;
int      g_lastSlot = -2, g_lastScTarget = -2;
uint64_t g_summaryMs = 0;
// the state: how many pairs in a row read as one picture / two pictures at stage bb
uint32_t g_oneStreak = 0, g_twoStreak = 0;
bool     g_onePicture = false;

inline Record& rec_for(uint32_t serial) { return g_rec[serial % kRecords]; }
inline Record* rec_get(uint32_t serial) {
    Record& r = g_rec[serial % kRecords];
    return (r.used && r.serial == serial) ? &r : nullptr;
}

uint32_t fnv1a(const uint8_t* p, uint32_t n) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

// 64x64 32-bit rows -> luma. `rAt` = the byte offset of red (2 for BGRA, 0 for RGBA).
void to_luma(const uint8_t* rows, uint32_t pitch, int rAt, uint8_t* out) {
    for (uint32_t y = 0; y < kThumb; ++y) {
        const uint8_t* p = rows + (size_t)y * pitch;
        for (uint32_t x = 0; x < kThumb; ++x, p += 4) {
            const uint32_t r = p[rAt], g = p[1], b = p[2 - rAt];
            out[y * kThumb + x] = (uint8_t)((77u * r + 150u * g + 29u * b) >> 8);
        }
    }
}

float mean_abs_diff(const uint8_t* a, const uint8_t* b) {
    uint32_t s = 0;
    for (uint32_t i = 0; i < kPixels; ++i) s += (uint32_t)(a[i] > b[i] ? a[i] - b[i] : b[i] - a[i]);
    return (float)s / (float)kPixels;
}

void store(Record& r, Stage st, const uint8_t* rows, uint32_t pitch, int rAt) {
    to_luma(rows, pitch, rAt, r.luma[st]);
    r.sum[st] = fnv1a(r.luma[st], kPixels);
    r.mask |= (uint8_t)(1u << st);
}

// ---- D3D9 ------------------------------------------------------------------
void release9() {
    for (uint32_t i = 0; i < kRing; ++i) {
        if (g_rt9[i]) { g_rt9[i]->Release(); g_rt9[i] = nullptr; }
        if (g_sys9[i]) { g_sys9[i]->Release(); g_sys9[i] = nullptr; }
        g_issued9[i] = false;
    }
    g_dev9 = nullptr;
}

bool ensure9(IDirect3DDevice9* dev) {
    if (g_dev9 == dev && g_rt9[kRing - 1] && g_sys9[kRing - 1]) return true;
    release9();
    for (uint32_t i = 0; i < kRing; ++i) {
        if (FAILED(dev->CreateRenderTarget(kThumb, kThumb, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &g_rt9[i], nullptr)) ||
            !g_rt9[i] ||
            FAILED(dev->CreateOffscreenPlainSurface(kThumb, kThumb, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &g_sys9[i], nullptr)) ||
            !g_sys9[i]) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn, "stereo: frameid stage bb OFF - the 64x64 D3D9 ring could not be created");
            release9();
            g_bbDead = true;
            return false;
        }
    }
    g_dev9 = dev;
    return true;
}

// Read the entry issued kReadBack grabs ago, never waiting (one blocking read
// after kBusyBlockAfter busy reads in a row, so a driver that never answers
// DONOTWAIT still yields a sample).
void read9(uint32_t serial) {
    if (serial < kReadBack) return;
    const uint32_t want = serial - kReadBack, k = want % kRing;
    if (!g_issued9[k] || g_serial9[k] != want) return;
    Record* r = rec_get(want);
    g_issued9[k] = false;
    if (!r) return;
    r->tried |= (uint8_t)(1u << kBb);
    D3DLOCKED_RECT lr;
    DWORD flags = D3DLOCK_READONLY | D3DLOCK_DONOTWAIT;
    if (g_busyStreak[kBb] >= kBusyBlockAfter) { flags = D3DLOCK_READONLY; ++g_blocked[kBb]; }
    HRESULT hr = g_sys9[k]->LockRect(&lr, nullptr, flags);
    if (hr == D3DERR_WASSTILLDRAWING) { ++g_busy[kBb]; ++g_busyStreak[kBb]; return; }
    if (FAILED(hr)) { ++g_missing[kBb]; return; }
    g_busyStreak[kBb] = 0;
    store(*r, kBb, (const uint8_t*)lr.pBits, (uint32_t)lr.Pitch, 2);
    g_sys9[k]->UnlockRect();
}

// ---- D3D11 -----------------------------------------------------------------
void release11() {
    for (int s = 0; s < 2; ++s)
        for (uint32_t i = 0; i < kRing; ++i) {
            if (g_rtv11[s][i]) { g_rtv11[s][i]->Release(); g_rtv11[s][i] = nullptr; }
            if (g_tex11[s][i]) { g_tex11[s][i]->Release(); g_tex11[s][i] = nullptr; }
        }
    for (int s = 0; s < kStages; ++s)
        for (uint32_t i = 0; i < kRing; ++i) {
            if (g_staging[s][i]) { g_staging[s][i]->Release(); g_staging[s][i] = nullptr; }
            g_issued11[s][i] = false;
        }
    g_blit.shutdown();
    g_scFormat = DXGI_FORMAT_UNKNOWN;
    g_dev11 = nullptr;
}

bool make_staging(ID3D11Device* dev, Stage st, DXGI_FORMAT fmt) {
    for (uint32_t i = 0; i < kRing; ++i) {
        if (g_staging[st][i]) continue;
        D3D11_TEXTURE2D_DESC sd = {};
        sd.Width = kThumb; sd.Height = kThumb; sd.MipLevels = 1; sd.ArraySize = 1;
        sd.Format = fmt; sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING; sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        if (FAILED(dev->CreateTexture2D(&sd, nullptr, &g_staging[st][i])) || !g_staging[st][i]) return false;
    }
    return true;
}

bool ensure11(ID3D11Device* dev, Stage st) {
    if (g_dead11[st]) return false;
    if (g_dev11 != dev) { release11(); g_dev11 = dev; }
    if (st == kSlot || st == kOut) {
        const int s = st == kSlot ? 0 : 1;
        if (!g_blit.init(dev)) { g_dead11[st] = true; return false; }
        for (uint32_t i = 0; i < kRing; ++i) {
            if (g_tex11[s][i] && g_rtv11[s][i]) continue;
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = kThumb; td.Height = kThumb; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_tex11[s][i])) || !g_tex11[s][i] ||
                FAILED(dev->CreateRenderTargetView(g_tex11[s][i], nullptr, &g_rtv11[s][i]))) {
                DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn, "stereo: frameid stage %s OFF - the 64x64 D3D11 ring could not be created", kStageName[st]);
                g_dead11[st] = true;
                return false;
            }
        }
        if (!make_staging(dev, st, DXGI_FORMAT_R8G8B8A8_UNORM)) { g_dead11[st] = true; return false; }
    }
    return true;
}

void read11(ID3D11DeviceContext* ctx, Stage st, uint32_t serial) {
    if (serial < kReadBack) return;
    const uint32_t want = serial - kReadBack, k = want % kRing;
    if (!g_issued11[st][k] || g_serial11[st][k] != want || !g_staging[st][k]) return;
    Record* r = rec_get(want);
    g_issued11[st][k] = false;
    if (!r) return;
    r->tried |= (uint8_t)(1u << st);
    UINT flags = D3D11_MAP_FLAG_DO_NOT_WAIT;
    if (g_busyStreak[st] >= kBusyBlockAfter) { flags = 0; ++g_blocked[st]; }
    D3D11_MAPPED_SUBRESOURCE m;
    const HRESULT hr = ctx->Map(g_staging[st][k], 0, D3D11_MAP_READ, flags, &m);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) { ++g_busy[st]; ++g_busyStreak[st]; return; }
    if (FAILED(hr)) { ++g_missing[st]; return; }
    g_busyStreak[st] = 0;
    store(*r, st, (const uint8_t*)m.pData, m.RowPitch, 0);
    ctx->Unmap(g_staging[st][k], 0);
}

void draw11(ID3D11Device* dev, ID3D11DeviceContext* ctx, Stage st, ID3D11ShaderResourceView* src) {
    if (!g_enabled || !g_curValid || !dev || !ctx || !src || !ensure11(dev, st)) return;
    const int s = st == kSlot ? 0 : 1;
    read11(ctx, st, g_curSerial);
    const uint32_t k = g_curSerial % kRing;
    g_blit.draw(ctx, src, g_rtv11[s][k], kThumb, kThumb);
    ctx->CopyResource(g_staging[st][k], g_tex11[s][k]);
    g_serial11[st][k] = g_curSerial; g_issued11[st][k] = true;
}

// ---- the judgement ---------------------------------------------------------
void summary(const char* prefix) {
    char buf[900];
    int n = _snprintf(buf, sizeof(buf), "stereo: frameid %s pairs=%u | one-picture pairs bb=%u slot=%u out=%u sc=%u",
                      prefix, g_pairsWindow, g_samePairsWindow[0], g_samePairsWindow[1], g_samePairsWindow[2], g_samePairsWindow[3]);
    n += _snprintf(buf + n, sizeof(buf) - n, " | L-R diff mean/min/max");
    for (int s = 0; s < kStages && n < (int)sizeof(buf) - 60; ++s) {
        if (g_cmpWindow[s])
            n += _snprintf(buf + n, sizeof(buf) - n, " %s=%.1f/%.1f/%.1f", kStageName[s],
                           g_diffSumWindow[s] / g_cmpWindow[s], g_diffMinWindow[s], g_diffMaxWindow[s]);
        else
            n += _snprintf(buf + n, sizeof(buf) - n, " %s=none", kStageName[s]);
    }
    n += _snprintf(buf + n, sizeof(buf) - n, " | same-eye floor bb=%.1f (%u) | c5 |d| mean=%.2f (%u)",
                   g_floorN ? g_floorSumWindow / g_floorN : 0.0, g_floorN, g_c5N ? g_c5SumWindow / g_c5N : 0.0, g_c5N);
    n += _snprintf(buf + n, sizeof(buf) - n, " | busy reads bb=%u slot=%u out=%u sc=%u (blocked %u/%u/%u/%u) missing %u/%u/%u/%u",
                   g_busy[0], g_busy[1], g_busy[2], g_busy[3], g_blocked[0], g_blocked[1], g_blocked[2], g_blocked[3],
                   g_missing[0], g_missing[1], g_missing[2], g_missing[3]);
    n += _snprintf(buf + n, sizeof(buf) - n, " | delivered slot repeats=%u sc-target repeats=%u | mode=%s%s",
                   g_slotRepeats, g_scRepeats, g_modeName[0] ? g_modeName : "?",
                   g_pairsWindow == 0 ? " (0 pairs: an untagged stream, the quad screen, no method stages, or the trace is off)" : "");
    DVR_INFO("%s", buf);
}

void window_reset() {
    g_pairsWindow = 0;
    for (int s = 0; s < kStages; ++s) {
        g_samePairsWindow[s] = 0; g_cmpWindow[s] = 0; g_diffSumWindow[s] = 0.0;
        g_diffMinWindow[s] = 0.0f; g_diffMaxWindow[s] = 0.0f;
    }
    g_floorSumWindow = 0.0; g_floorN = 0; g_c5SumWindow = 0.0; g_c5N = 0;
}

void judge_pair(const Record& l, const Record& r, uint32_t serialL) {
    // the same-eye floor: this left against the previous left at stage bb
    float floorBb = -1.0f;
    if (serialL >= 2) {
        const Record* pl = rec_get(serialL - 2);
        if (pl && pl->tag < 0 && (pl->mask & 1u) && (l.mask & 1u)) floorBb = mean_abs_diff(pl->luma[kBb], l.luma[kBb]);
    }
    float diff[kStages];
    bool  have[kStages];
    const float sameBelow = floorBb >= 0.0f ? (1.5f * floorBb > 1.0f ? 1.5f * floorBb : 1.0f) : 1.0f;
    int firstSame = -1;
    for (int s = 0; s < kStages; ++s) {
        have[s] = ((l.mask & r.mask) >> s) & 1u;
        diff[s] = have[s] ? mean_abs_diff(l.luma[s], r.luma[s]) : -1.0f;
        if (have[s]) {
            ++g_cmpWindow[s];
            g_diffSumWindow[s] += diff[s];
            if (g_cmpWindow[s] == 1 || diff[s] < g_diffMinWindow[s]) g_diffMinWindow[s] = diff[s];
            if (g_cmpWindow[s] == 1 || diff[s] > g_diffMaxWindow[s]) g_diffMaxWindow[s] = diff[s];
            g_lastDiff[s] = diff[s];
            if (diff[s] < sameBelow) { ++g_samePairs[s]; ++g_samePairsWindow[s]; if (firstSame < 0) firstSame = s; }
        }
    }
    ++g_pairs; ++g_pairsWindow;
    if (floorBb >= 0.0f) { g_floorSumWindow += floorBb; ++g_floorN; }
    float c5d = -1.0f;
    if (l.c5Ok && r.c5Ok) {
        const float dx = r.c5[0] - l.c5[0], dy = r.c5[1] - l.c5[1], dz = r.c5[2] - l.c5[2];
        c5d = sqrtf(dx * dx + dy * dy + dz * dz);
        g_c5SumWindow += c5d; ++g_c5N;
    }
    char sums[2][kStages][12];
    for (int e = 0; e < 2; ++e)
        for (int s = 0; s < kStages; ++s) {
            const Record& rr = e ? r : l;
            if ((rr.mask >> s) & 1u) _snprintf(sums[e][s], sizeof(sums[e][s]), "%08x", rr.sum[s]);
            else strcpy(sums[e][s], (rr.tried >> s) & 1u ? "busy" : "-");
        }
    char d[kStages][12];
    for (int s = 0; s < kStages; ++s) {
        if (have[s]) _snprintf(d[s], sizeof(d[s]), "%.1f", diff[s]); else strcpy(d[s], "-");
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                     "stereo: frameid #%u [-1] c5=(%.1f %.1f %.1f) slot%d bb=%s slot=%s out=%s sc%d=%s | #%u [+1] "
                     "c5=(%.1f %.1f %.1f) slot%d bb=%s slot=%s out=%s sc%d=%s | L-R diff bb=%s slot=%s out=%s sc=%s "
                     "(c5 |d| %.2f uu) | same-eye floor bb=%.1f (this L vs the previous L; one picture = below %.1f) "
                     "| first one-picture stage: %s",
                     l.serial, l.c5[0], l.c5[1], l.c5[2], l.slot, sums[0][0], sums[0][1], sums[0][2], l.scTarget, sums[0][3],
                     r.serial, r.c5[0], r.c5[1], r.c5[2], r.slot, sums[1][0], sums[1][1], sums[1][2], r.scTarget, sums[1][3],
                     d[0], d[1], d[2], d[3], c5d, floorBb, sameBelow,
                     firstSame < 0 ? "none (two pictures at every stage read)" : kStageName[firstSame]);
    // the state CHANGE at stage bb (or the first stage both had, when bb is not there)
    int judgeStage = have[kBb] ? kBb : have[kSlot] ? kSlot : have[kOut] ? kOut : have[kSc] ? kSc : -1;
    if (judgeStage >= 0) {
        const bool one = diff[judgeStage] < sameBelow;
        if (one) { ++g_oneStreak; g_twoStreak = 0; } else { ++g_twoStreak; g_oneStreak = 0; }
        if (!g_onePicture && g_oneStreak >= 10) {
            g_onePicture = true;
            DVR_WARN("stereo: frameid THE EYES BECAME ONE PICTURE at stage %s (10 pairs in a row below the floor; "
                     "diff %.1f vs floor %.1f at serial %u) - the stages after it %s", kStageName[judgeStage],
                     diff[judgeStage], floorBb, l.serial,
                     firstSame == judgeStage ? "inherit it" : "are the ones to read");
        } else if (g_onePicture && g_twoStreak >= 10) {
            g_onePicture = false;
            DVR_WARN("stereo: frameid the eyes became TWO PICTURES again at stage %s (10 pairs in a row above the floor; "
                     "diff %.1f vs floor %.1f at serial %u)", kStageName[judgeStage], diff[judgeStage], floorBb, l.serial);
        }
    }
}

} // namespace

void set_enabled(bool on) {
    if (on == g_enabled) return;
    g_enabled = on;
    DVR_INFO("stereo: frameid %s (the four-stage thumbnail trace; [Perf] FrameId=%d for the next launch)", on ? "ON" : "off", on ? 1 : 0);
}
bool enabled() { return g_enabled; }

void note_c5(const float c5[3], bool ok) {
    g_pendingC5Ok = ok && c5 != nullptr;
    if (g_pendingC5Ok) memcpy(g_pendingC5, c5, sizeof(g_pendingC5));
}

void stage_backbuffer(IDirect3DDevice9* dev, IDirect3DSurface9* bb, uint32_t serial, int tag) {
    if (!g_enabled) return;
    Record& r = rec_for(serial);
    r.used = true; r.serial = serial; r.tag = tag;
    r.c5Ok = g_pendingC5Ok; memcpy(r.c5, g_pendingC5, sizeof(r.c5));
    r.slot = -1; r.scTarget = -1; r.scIndex = 0; r.mask = 0; r.tried = 0;
    g_pendingC5Ok = false;
    if (g_bbDead || !dev || !bb || !ensure9(dev)) return;
    read9(serial);
    const uint32_t k = serial % kRing;
    HRESULT hr = dev->StretchRect(bb, nullptr, g_rt9[k], nullptr, g_bbLinear ? D3DTEXF_LINEAR : D3DTEXF_NONE);
    if (FAILED(hr) && g_bbLinear) {
        g_bbLinear = false;
        DVR_INFO("stereo: frameid stage bb - the filtered StretchRect was refused (0x%08lx), point sampling instead", (unsigned long)hr);
        hr = dev->StretchRect(bb, nullptr, g_rt9[k], nullptr, D3DTEXF_NONE);
    }
    if (FAILED(hr)) {
        DVR_WARN("stereo: frameid stage bb OFF - StretchRect backbuffer -> 64x64 refused (0x%08lx)", (unsigned long)hr);
        g_bbDead = true;
        return;
    }
    hr = dev->GetRenderTargetData(g_rt9[k], g_sys9[k]);
    if (FAILED(hr)) {
        DVR_WARN("stereo: frameid stage bb OFF - GetRenderTargetData 64x64 refused (0x%08lx)", (unsigned long)hr);
        g_bbDead = true;
        return;
    }
    g_serial9[k] = serial; g_issued9[k] = true;
}

void note_delivery(uint32_t serial, int tag, int slot, const char* modeName) {
    g_curSerial = serial; g_curValid = g_enabled; g_curTag = tag;
    if (modeName) { strncpy(g_modeName, modeName, sizeof(g_modeName) - 1); g_modeName[sizeof(g_modeName) - 1] = 0; }
    if (Record* r = rec_get(serial)) { r->slot = slot; if (r->tag == 0) r->tag = tag; }
    if (slot >= 0 && slot == g_lastSlot) ++g_slotRepeats;
    g_lastSlot = slot;
}

void stage_slot(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* slotSrv) { draw11(dev, ctx, kSlot, slotSrv); }
void stage_out(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* outSrv) { draw11(dev, ctx, kOut, outSrv); }

void stage_swapchain(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* image, int target, uint32_t index) {
    if (!g_enabled || !g_curValid || !dev || !ctx || !image || g_dead11[kSc]) return;
    if (g_dev11 != dev) { release11(); g_dev11 = dev; }
    D3D11_TEXTURE2D_DESC id; image->GetDesc(&id);
    if (id.Width < kThumb || id.Height < kThumb) return;
    if (g_scFormat != id.Format) {
        for (uint32_t i = 0; i < kRing; ++i) { if (g_staging[kSc][i]) { g_staging[kSc][i]->Release(); g_staging[kSc][i] = nullptr; } g_issued11[kSc][i] = false; }
        if (!make_staging(dev, kSc, id.Format)) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn, "stereo: frameid stage sc OFF - a 64x64 staging texture in the swapchain format %d could not be created", (int)id.Format);
            g_dead11[kSc] = true;
            return;
        }
        g_scFormat = id.Format;
    }
    read11(ctx, kSc, g_curSerial);
    if (Record* r = rec_get(g_curSerial)) { r->scTarget = target; r->scIndex = index; }
    if (g_curTag != 0) { if (target == g_lastScTarget) ++g_scRepeats; g_lastScTarget = target; }
    const uint32_t k = g_curSerial % kRing;
    D3D11_BOX box;
    box.left = (id.Width - kThumb) / 2; box.right = box.left + kThumb;
    box.top = (id.Height - kThumb) / 2; box.bottom = box.top + kThumb;
    box.front = 0; box.back = 1;
    ctx->CopySubresourceRegion(g_staging[kSc][k], 0, 0, 0, 0, image, 0, &box);
    g_serial11[kSc][k] = g_curSerial; g_issued11[kSc][k] = true;
}

void present_tick() {
    if (!g_enabled || !g_curValid) return;
    g_curValid = false;
    // Judge every serial old enough for all four reads to have been attempted
    // (the sc read for serial e happens at delivered serial e + kReadBack).
    if (g_curSerial < kReadBack + 2) return;
    const uint32_t upTo = g_curSerial - kReadBack - 1;
    if (!g_lastEvalInit) { g_lastEval = upTo > 0 ? upTo - 1 : 0; g_lastEvalInit = true; }
    if (upTo <= g_lastEval) return;
    if (upTo - g_lastEval > kRecords / 2) g_lastEval = upTo - kRecords / 2;   // a stall: skip what the ring lost
    for (uint32_t e = g_lastEval + 1; e <= upTo; ++e) {
        const Record* r = rec_get(e);
        if (!r || r->tag <= 0 || e == 0) continue;
        const Record* l = rec_get(e - 1);
        if (!l || l->tag >= 0) continue;
        judge_pair(*l, *r, e - 1);
    }
    g_lastEval = upTo;
    const uint64_t now = GetTickCount64();
    if (g_summaryMs == 0) g_summaryMs = now;
    else if (now - g_summaryMs >= 3000) { summary("3s:"); window_reset(); g_summaryMs = now; }
}

void on_reset() { release9(); }

void shutdown() { release9(); release11(); }

void log_status() {
    DVR_INFO("stereo: frameid %s - lifetime pairs=%u, one-picture pairs bb=%u slot=%u out=%u sc=%u, last L-R diff bb=%.1f slot=%.1f "
             "out=%.1f sc=%.1f, state=%s (frameid on|off|status)", g_enabled ? "ON" : "off", g_pairs,
             g_samePairs[0], g_samePairs[1], g_samePairs[2], g_samePairs[3], g_lastDiff[0], g_lastDiff[1], g_lastDiff[2], g_lastDiff[3],
             g_onePicture ? "ONE PICTURE" : "two pictures");
    summary("window so far:");
}

void status(dvr::status::Writer& w) {
    w.kv("enabled", g_enabled);
    w.kv("pairs", (unsigned long)g_pairs);
    w.kv("onePicture", g_onePicture);
    for (int s = 0; s < kStages; ++s) {
        char key[24];
        _snprintf(key, sizeof(key), "same_%s", kStageName[s]); w.kv(key, (unsigned long)g_samePairs[s]);
        _snprintf(key, sizeof(key), "diff_%s", kStageName[s]); w.kv(key, (double)g_lastDiff[s]);
        _snprintf(key, sizeof(key), "busy_%s", kStageName[s]); w.kv(key, (unsigned long)g_busy[s]);
    }
    w.kv("slotRepeats", (unsigned long)g_slotRepeats);
    w.kv("scRepeats", (unsigned long)g_scRepeats);
}

} // namespace dvr::frameid
