// core/gfx/dlss.cpp - see dlss.h. The present thread calls run(); starting the helper and
// building its features block for hundreds of milliseconds, so that happens on a worker
// thread that touches only the device and the pipe. The state machine below never lets the
// worker and the present thread use the client at the same time: the present thread uses it
// only in Ready, the worker only outside it.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/dlss.h"
#include "core/gfx/dlss_client.h"
#include "core/gfx/dlss_gpu.h"
#include "core/gfx/dlss_jitter.h"
#include "core/gfx/clarity.h"
#include "core/gfx/capture.h"
#include "core/gfx/depth_probe.h"

#include "core/util/log.h"
#include "core/util/paths.h"
#include "game/dishonored/camera.h"

#include <windows.h>
#include <d3d11.h>
#include <atomic>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>

namespace dvr::dlss {
namespace {

enum State { Idle = 0, Working, Ready, Failed };

std::atomic<int> g_mode{ModeOff};
std::atomic<int> g_backend{BackendDlss};
std::atomic<int> g_fsrVersion{0};
std::atomic<bool> g_restart{false};   // the backend changed: the present thread stops the helper
char g_runtime[64] = "";
char g_offered[128] = "";
uint64_t g_lastEyeQpc[2] = {};
std::atomic<int> g_preset{0};
std::atomic<int> g_quality{QDlaa};
std::atomic<int> g_model{0};
std::atomic<bool> g_audit{false};
std::atomic<uint32_t> g_outW{0}, g_outH{0};
const float kRatio[QCount] = {1.0f, 1.5f, 1.7241f, 2.0f, 3.0f, 1.3f};
const char* const kQualityName[QCount] = {"DLAA", "Quality", "Balanced", "Performance", "Ultra Performance", "Ultra Quality"};
// FSR's name for the full-resolution mode; the others are the same words.
const char* mode_name(int q) { return (q == QDlaa && g_backend.load() == BackendFsr) ? "Native AA" : kQualityName[q]; }
const char* const kQualityWord[QCount] = {"dlaa", "quality", "balanced", "performance", "ultraperformance", "ultraquality"};
std::atomic<bool> g_mask{false};
std::atomic<bool> g_fgBias{true};   // `dlss fgbias on|off`: the hands and weapon always trust the current colour
std::atomic<float> g_maskLo{0.03f}, g_maskHi{0.12f};
std::atomic<int> g_state{Idle};
std::atomic<bool> g_retry{false};
Client g_client;
GuideGpu g_guides;
std::thread g_worker;
char g_why[512] = "";          // the last refusal, for the log and F10
char g_summary[256] = "off";
// What the present thread last asked for (read by the worker once it is joined-for).
uint32_t g_wantW = 0, g_wantH = 0, g_wantOw = 0, g_wantOh = 0;
DXGI_FORMAT g_wantFmt = DXGI_FORMAT_UNKNOWN;
ID3D11Device* g_dev = nullptr;
bool g_helperStarted = false;
bool g_guideFailed = false;

struct Window { uint64_t eyes[2] = {}, fallback = 0, resets = 0, masked = 0, fgMasked = 0; };
Window g_win;
uint64_t g_winMs = 0;

void client_log(int level, const char* line) {
    if (level >= 2) DVR_ERROR("%s", line);
    else if (level == 1) DVR_WARN("%s", line);
    else DVR_INFO("%s", line);
}

void join_worker() { if (g_worker.joinable()) g_worker.join(); }

// Worker: start the helper if needed, then build both eyes at the wanted size.
void work(uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, DXGI_FORMAT fmt, int preset) {
    char why[512] = "";
    bool ok = true;
    if (!g_client.running()) {
        wchar_t exe[MAX_PATH], data[MAX_PATH];
        _snwprintf_s(exe, _TRUNCATE, L"%hs\\dvr_dlss\\dvr_dlss_host64.exe", dvr::paths::game_dir());
        _snwprintf_s(data, _TRUNCATE, L"%hs\\dlss", dvr::paths::data_dir());
        StartParams sp;
        sp.hostExe = exe; sp.dataDir = data; sp.log = client_log;
        sp.backend = (uint32_t)g_backend.load();
        sp.fsrVersion = (uint32_t)g_fsrVersion.load();
        ok = g_client.start(g_dev, sp, why, sizeof(why));
        if (ok) { g_helperStarted = true; strcpy_s(g_runtime, g_client.runtime()); strcpy_s(g_offered, g_client.offered()); }
    }
    for (int e = 0; ok && e < 2; ++e) ok = g_client.build(e, w, h, ow, oh, fmt, preset, why, sizeof(why));
    if (ok && g_backend.load() == BackendFsr) {
        DVR_INFO("dlss: AMD %s %s ready on %s for both eyes, %ux%u -> %ux%u (%.2fx per axis, %.0f%% of the output's pixels "
                 "rendered; colour format %d), %.1f MiB shared with the helper - FSR now owns the eye image; the custom "
                 "temporal AA stands down", g_client.runtime(), (w == ow && h == oh) ? "native AA" : "upscaling",
                 g_client.adapter(), w, h, ow, oh, (double)ow / w, 100.0 * w * h / ((double)ow * oh), (int)fmt,
                 g_client.bytes() / (1024.0 * 1024.0));
        g_state.store(Ready);
    } else if (ok) {
        DVR_INFO("dlss: %s ready on %s for both eyes, %ux%u -> %ux%u (%.2fx per axis, %.0f%% of the output's pixels "
                 "rendered; colour format %d, preset %d%s), %.1f MiB shared with the helper - DLSS now owns the eye "
                 "image; the custom temporal AA stands down",
                 (w == ow && h == oh) ? "DLAA" : "DLSS Super Resolution", g_client.adapter(), w, h, ow, oh,
                 (double)ow / w, 100.0 * w * h / ((double)ow * oh), (int)fmt, preset,
                 preset == 11 ? " = transformer K" : (preset == 5 || preset == 6) ? " = fast CNN"
                 : preset == 10 ? " = transformer J" : preset == 12 ? " = transformer 2 L" : preset == 13 ? " = transformer 2 M"
                 : preset == kPresetPerMode ? " = NVIDIA per mode K/M/L" : " (raw DlssPreset)",
                 g_client.bytes() / (1024.0 * 1024.0));
        g_state.store(Ready);
    } else {
        strcpy_s(g_why, why);
        DVR_WARN("dlss: %s unavailable (%s) - the normal path runs; `dlss retry` or toggling it tries again", backend_name(), why);
        if (!g_client.running()) g_helperStarted = false;
        g_state.store(Failed);
    }
}

void kick(uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, DXGI_FORMAT fmt) {
    join_worker();
    g_wantW = w; g_wantH = h; g_wantOw = ow; g_wantOh = oh; g_wantFmt = fmt;
    g_state.store(Working);
    // The raw preset wins; otherwise the model: K (11), or the CNN E (5) for SR / F (6) for DLAA.
    int preset = g_preset.load();
    if (!preset) preset = g_model.load() == 1 ? ((w == ow && h == oh) ? 6 : 5) : 11;
    g_worker = std::thread([w, h, ow, oh, fmt, preset] { work(w, h, ow, oh, fmt, preset); });
}

void status_tick() {
    const uint64_t now = GetTickCount64();
    if (!g_winMs) { g_winMs = now; return; }
    if (now - g_winMs < 5000) return;
    const double s = (now - g_winMs) / 1000.0;
    Stats& st = g_client.stats;
    const double gl = st.gpuN[0] ? st.gpuMsSum[0] / st.gpuN[0] : -1, gr = st.gpuN[1] ? st.gpuMsSum[1] / st.gpuN[1] : -1;
    const double cl = st.frames[0] ? st.cpuMsSum[0] / (st.frames[0] + st.refused[0]) : 0;
    const double cr = st.frames[1] ? st.cpuMsSum[1] / (st.frames[1] + st.refused[1]) : 0;
    DVR_INFO("dlss: %s %ux%u -> %ux%u: %.0f/s L %.0f/s R, fallback %.0f/s, history resets %llu | helper GPU evaluate L %.2f R %.2f ms "
             "(-1 = not sampled), present-thread cost L %.2f R %.2f ms max %.2f | refused L %llu R %llu | %.1f MiB shared + %.1f MiB guides "
             "| camera-only vectors, projection jitter %s | bias mask on %.0f/s (hands/weapon mask on %.0f/s: 0 with DLSS "
             "on = no foreground copy - the hands then smear when they move)",
             g_backend.load() == BackendFsr ? g_runtime
                 : (g_wantW == g_wantOw && g_wantH == g_wantOh) ? "DLAA" : kQualityName[g_quality.load()],
             g_wantW, g_wantH, g_wantOw, g_wantOh, g_win.eyes[0] / s, g_win.eyes[1] / s, g_win.fallback / s, (unsigned long long)g_win.resets, gl, gr, cl, cr,
             st.cpuMsMax[0] > st.cpuMsMax[1] ? st.cpuMsMax[0] : st.cpuMsMax[1],
             (unsigned long long)st.refused[0], (unsigned long long)st.refused[1],
             g_client.bytes() / (1024.0 * 1024.0), g_guides.bytes() / (1024.0 * 1024.0), jitter::summary(),
             g_win.masked / s, g_win.fgMasked / s);
    {   // The vector audit (dlss_gpu.h): per depth band, how much of the frame-to-frame change the
        // camera vectors explain. vec well below zero = explained; vec near zero = not (it smears).
        char t[512]; int m = 0;
        for (int b = 0; b < kAuditBins; ++b) {
            const AuditBin& a = g_guides.bins[b];
            if (!a.n) continue;
            m += _snprintf_s(t + m, sizeof(t) - m, _TRUNCATE, " %s: vec %.4f zero %.4f mask %.2f (%llu)", kAuditBinNames[b],
                             a.vecErr / a.n, a.zeroErr / a.n, a.masked / a.n, (unsigned long long)a.n);
        }
        if (m)
            DVR_INFO("dlss/audit: luminance error, previous image moved by the camera vectors (vec) vs not moved (zero), "
                     "by depth band in depth units, mean anti-smear mask; a band whose vec is near its zero is where the "
                     "vectors are wrong:%s | mask %s %.2f..%.2f", t, g_mask.load() ? "ON" : "off", g_maskLo.load(), g_maskHi.load());
        for (auto& b : g_guides.bins) b = AuditBin{};
        const FlowStats& f = g_guides.flow;
        if (f.n)
            DVR_INFO("dlss/flow: %llu textured points (%llu rejected as flat/ambiguous) | vector error mean %.2f px, bias x %+.2f "
                     "y %+.2f px, over 1 px %.0f%% | true/predicted gain x %.2f y %.2f (1 = right; 0.5 or 2 = a frame off) | "
                     "mean predicted motion %.2f px, true %.2f px | arms band: %llu points, error %.2f px | per image: whole-image "
                     "shift %.2f px, scatter around it %.2f px (%llu images; shift = pose, scatter = depth/projection/noise)",
                     (unsigned long long)f.n, (unsigned long long)f.rejected, f.eabs / f.n, f.ex / f.n, f.ey / f.n,
                     100.0 * f.big / f.n, f.mm[0] > 0 ? f.mt[0] / f.mm[0] : 0.0, f.mm[1] > 0 ? f.mt[1] / f.mm[1] : 0.0,
                     f.pabs / f.n, f.tabs / f.n, (unsigned long long)f.armsN, f.armsN ? f.armsAbs / f.armsN : 0.0,
                     f.frames ? f.frameShift / f.frames : 0.0, f.frames ? f.frameScatter / f.frames : 0.0,
                     (unsigned long long)f.frames);
        if (f.n || f.jitFrames)
            DVR_INFO("dlss/flow jitter: %llu images with a jitter change | expected shift %.3f px, uncorrected whole-image shift "
                     "%.3f px, jitter gain %.2f (1 = the image moved by exactly the recorded offset: jitter applied, sign and "
                     "record right; 0 = it did not move; -1 = the other way) | the errors above are after subtracting it",
                     (unsigned long long)f.jitFrames, f.jitFrames ? f.jitExpect / f.jitFrames : 0.0,
                     f.jitFrames ? f.jitShiftRaw / f.jitFrames : 0.0, f.jitNorm > 0 ? f.jitDot / f.jitNorm : 0.0);
        if (f.n) {
            char t[400]; int m = 0;
            for (int b = 0; b < kAuditBins; ++b)
                if (f.bandN[b] > 200)
                    m += _snprintf_s(t + m, sizeof(t) - m, _TRUNCATE, " %s: gain %.2f err %.2f px (%llu)", kAuditBinNames[b],
                                     f.bandMm[b] > 0 ? f.bandMt[b] / f.bandMm[b] : 0.0, f.bandErr[b] / f.bandN[b],
                                     (unsigned long long)f.bandN[b]);
            DVR_INFO("dlss/flow bands: true/predicted gain and error per depth band (depth units):%s", t);
        }
        g_guides.flow = FlowStats{};
    }
    _snprintf_s(g_summary, _TRUNCATE, "%s %s %ux%u -> %ux%u, L %.0f/s R %.0f/s, GPU %.2f/%.2f ms per eye, fallback %.0f/s",
                g_backend.load() == BackendFsr ? g_runtime : "DLSS",
                (g_wantW == g_wantOw && g_wantH == g_wantOh) ? mode_name(QDlaa) : kQualityName[g_quality.load()], g_wantW, g_wantH,
                g_wantOw, g_wantOh, g_win.eyes[0] / s, g_win.eyes[1] / s, gl, gr, g_win.fallback / s);
    g_win = Window{};
    st = Stats{};
    g_winMs = now;
}

} // namespace

void set_mode(int m, const char* who) {
    m = m == ModeDlaa ? ModeDlaa : ModeOff;
    const int was = g_mode.exchange(m);
    if (was == m) return;
    DVR_INFO("dlss: mode %s -> %s (live, %s)", was ? "DLAA" : "off", m ? "DLAA" : "off", who ? who : "?");
    if (m) g_retry.store(true);
}
int mode() { return g_mode.load(); }

void set_backend(int b, const char* who) {
    b = b == BackendFsr ? BackendFsr : BackendDlss;
    const int was = g_backend.exchange(b);
    if (was == b) return;
    DVR_INFO("dlss: upscaler %s -> %s (%s)%s", was == BackendFsr ? "FSR" : "DLSS", b == BackendFsr ? "FSR" : "DLSS",
             who ? who : "?", g_mode.load() ? " - the helper restarts on the other backend" : "");
    g_restart.store(true);
}
int backend() { return g_backend.load(); }
const char* backend_name() { return g_backend.load() == BackendFsr ? "FSR" : "DLSS"; }
void set_fsr_version(int v, const char* who) {
    if (v < 0 || v > 16) v = 0;
    if (g_fsrVersion.exchange(v) == v) return;
    DVR_INFO("dlss: FSR version -> %d (%s; 0 = the runtime's default)%s", v, who ? who : "?",
             g_backend.load() == BackendFsr ? " - the helper restarts" : "");
    if (g_backend.load() == BackendFsr) g_restart.store(true);
}
int fsr_version() { return g_fsrVersion.load(); }
const char* runtime_name() { return g_backend.load() == BackendFsr ? g_runtime : ""; }
const char* offered_versions() { return g_backend.load() == BackendFsr ? g_offered : ""; }

const ModelChoice kModelChoices[] = {
    {"Transformer K (default)", 0, 0,
     "NVIDIA's best all-round model: sharp, stable, little ghosting. About 2 ms per eye at 2750x2850, "
     "the same in every mode."},
    {"Transformer J", 0, 10,
     "A sibling of K: slightly less ghosting behind moving things, slightly more flicker on fine "
     "detail. Same cost as K. NVIDIA recommends K over J."},
    {"Transformer 2 M", 0, 13,
     "The newer (DLSS 4.5) model NVIDIA uses for Performance: the least ghosting and cleaner fine "
     "patterns, best when the render is small. Heavy at high resolution: about 3 ms per eye in "
     "Performance, 4 in Quality, 8 in DLAA."},
    {"Transformer 2 L", 0, 12,
     "The newer model NVIDIA uses for Ultra Performance: rebuilds the most from a very small render. "
     "The heaviest: about 3 ms per eye in Performance, 5 in Quality, 10 in DLAA."},
    {"NVIDIA recommended per mode", 0, kPresetPerMode,
     "What NVIDIA picks for each mode: K for DLAA, Ultra Quality, Quality and Balanced; M for "
     "Performance; L for Ultra Performance."},
    {"Fast (older CNN)", 1, 0,
     "The older, lighter models (E for the smaller modes, F for DLAA): under 1 ms per eye, a little "
     "softer, more ghosting and shimmer. Pick it when frame rate matters more than detail."},
};
const int kModelChoiceCount = (int)(sizeof(kModelChoices) / sizeof(kModelChoices[0]));

int model_choice() {
    const int m = g_model.load(), p = g_preset.load();
    for (int i = 0; i < kModelChoiceCount; ++i) {
        // With a raw preset set, the model switch does not matter (the preset overrides it).
        if (p ? kModelChoices[i].preset == p : (kModelChoices[i].preset == 0 && kModelChoices[i].model == m)) return i;
    }
    return -1;
}
void set_model_choice(int i, const char* who) {
    if (i < 0 || i >= kModelChoiceCount) return;
    const bool changed = g_model.load() != kModelChoices[i].model || g_preset.load() != kModelChoices[i].preset;
    g_model.store(kModelChoices[i].model);
    g_preset.store(kModelChoices[i].preset);
    if (!changed) return;
    DVR_INFO("dlss: model -> %s (DlssModel=%d DlssPreset=%d, %s); the features rebuild", kModelChoices[i].name,
             kModelChoices[i].model, kModelChoices[i].preset, who ? who : "?");
    g_retry.store(true);
}

void set_preset(int p, const char* who) {
    if (p < 0 || p > kPresetPerMode) p = 0;
    if (g_preset.exchange(p) == p) return;
    DVR_INFO("dlss: preset -> %d%s (%s); the features rebuild", p, p ? "" : " (the helper's model K)", who ? who : "?");
    g_retry.store(true);
}
int preset() { return g_preset.load(); }

void set_model(int m, const char* who) {
    m = m == 1 ? 1 : 0;
    if (g_model.exchange(m) == m) return;
    DVR_INFO("dlss: model -> %s (%s); the features rebuild", m ? "fast (CNN presets E/F)" : "transformer (preset K)", who ? who : "?");
    g_retry.store(true);
}
int model() { return g_model.load(); }
void set_audit(bool on, const char* who) {
    if (g_audit.exchange(on) == on) return;
    DVR_INFO("dlss: vector audit and flow check %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - costs GPU time every eye image; for diagnosis only" : "");
}
bool audit_on() { return g_audit.load(); }

void set_quality(int q, const char* who) {
    if (q < 0 || q >= QCount) q = QDlaa;
    const int was = g_quality.exchange(q);
    if (was == q) return;
    DVR_INFO("dlss: quality %s -> %s (%.2fx per axis, %s); the game resizes to match", kQualityName[was], kQualityName[q],
             kRatio[q], who ? who : "?");
}
int quality() { return g_quality.load(); }
float ratio() { return kRatio[g_quality.load()]; }
const char* quality_name(int q) { return (q >= 0 && q < QCount) ? mode_name(q) : "?"; }
void set_output(uint32_t w, uint32_t h, const char* who) {
    if (w && h && (w < 640 || h < 480 || w > 16384 || h > 16384)) return;
    if (g_outW.load() == w && g_outH.load() == h) return;
    g_outW.store(w); g_outH.store(h);
    DVR_INFO("dlss: Super Resolution output %ux%u (%s)%s", w, h, who ? who : "?", w ? "" : " - none recorded");
}
bool output(uint32_t* w, uint32_t* h) {
    *w = g_outW.load(); *h = g_outH.load();
    return *w && *h;
}
void render_for(uint32_t ow, uint32_t oh, uint32_t* w, uint32_t* h) {
    const float r = kRatio[g_quality.load()];
    uint32_t rw = (uint32_t)(ow / r + 0.5f) & ~1u, rh = (uint32_t)(oh / r + 0.5f) & ~1u;
    if (rw < 640) rw = 640;
    if (rh < 480) rh = 480;
    *w = rw; *h = rh;
}
bool sr_output_for(uint32_t w, uint32_t h, uint32_t* ow, uint32_t* oh) {
    if (g_mode.load() == ModeOff || g_quality.load() == QDlaa || g_state.load() == Failed) return false;
    uint32_t tw = g_outW.load(), th = g_outH.load();
    if (!tw || !th) return false;
    uint32_t rw = 0, rh = 0;
    render_for(tw, th, &rw, &rh);
    if (w + 2 < rw || w > rw + 2 || h + 2 < rh || h > rh + 2) return false;
    *ow = tw; *oh = th;
    return true;
}
bool failed() { return g_state.load() == Failed; }

void set_mask(bool on, const char* who) {
    if (g_mask.exchange(on) == on) return;
    DVR_INFO("dlss: anti-smear mask %s (live, %s)", on ? "ON" : "off", who ? who : "?");
}
bool mask_on() { return g_mask.load(); }
void set_fg_bias(bool on, const char* who) {
    if (g_fgBias.exchange(on) == on) return;
    DVR_INFO("dlss: the hands and weapon always trust the current colour %s (live, %s)", on ? "ON" : "off", who ? who : "?");
}
bool fg_bias() { return g_fgBias.load(); }
void set_mask_range(float lo, float hi, const char* who) {
    if (!(lo >= 0.0f && lo < 1.0f)) lo = 0.03f;
    if (!(hi > lo && hi <= 1.0f)) hi = lo + 0.09f;
    g_maskLo.store(lo); g_maskHi.store(hi);
    DVR_INFO("dlss: anti-smear mask range %.3f..%.3f (%s)", lo, hi, who ? who : "?");
}
float mask_lo() { return g_maskLo.load(); }
float mask_hi() { return g_maskHi.load(); }

bool active() { return g_mode.load() != ModeOff && g_state.load() == Ready; }

ID3D11ShaderResourceView* run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
                              uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, int eye, const GuideParams& gp, bool reset) {
    if (g_mode.load() == ModeOff) {
        if (g_state.load() != Idle && g_state.load() != Working) shutdown();
        return nullptr;
    }
    if (!dev || !ctx || !src || (eye != 0 && eye != 1)) return nullptr;
    if (g_restart.exchange(false) && g_state.load() != Idle) {
        DVR_INFO("dlss: stopping the helper to restart it as %s", backend_name());
        shutdown();
    }
    status_tick();
    ID3D11Resource* res = nullptr;
    src->GetResource(&res);
    ID3D11Texture2D* color = nullptr;
    if (res) { res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&color); res->Release(); }
    if (!color) { ++g_win.fallback; return nullptr; }
    D3D11_TEXTURE2D_DESC cd = {};
    color->GetDesc(&cd);
    struct Hold { ID3D11Texture2D* t; ~Hold() { t->Release(); } } hold{color};

    if (g_dev && g_dev != dev) { DVR_WARN("dlss: the D3D11 device changed - restarting the helper"); shutdown(); }
    g_dev = dev;
    const int state = g_state.load();
    if (state == Working) { ++g_win.fallback; return nullptr; }
    const bool retry = g_retry.exchange(false);
    if (state == Idle || (retry && state != Working) ||
        (state == Ready && !g_client.built(eye, w, h, ow, oh, cd.Format))) {
        if (state == Ready && !retry)
            DVR_INFO("dlss: eye image %ux%u -> %ux%u format %d differs from the built %ux%u -> %ux%u format %d - rebuilding "
                     "both eyes", w, h, ow, oh, (int)cd.Format, g_wantW, g_wantH, g_wantOw, g_wantOh, (int)g_wantFmt);
        kick(w, h, ow, oh, cd.Format);
        ++g_win.fallback;
        return nullptr;
    }
    if (state != Ready) { ++g_win.fallback; return nullptr; }

    char why[256] = "";
    g_guides.bodyDepth = gp.bodyDepth;
    if (!g_guides.run(dev, ctx, gp, why, sizeof(why))) {
        if (!g_guideFailed) DVR_ERROR("dlss: the guide pass failed (%s) - DLAA is inert, the normal path runs", why);
        g_guideFailed = true;
        ++g_win.fallback;
        return nullptr;
    }
    // The overlap: the scene depth is read only by the guide pass and the capture slot only by
    // the input copy (and the optional mask/audit before it). Releasing them there instead of
    // after the DLSS wait lets the game's next frame reuse them while DLSS still runs; before,
    // the CPU waited ~2.6 ms per eye image on the capture slot for the previous DLSS.
    EyeInputs in;
    in.color = color; in.depth = g_guides.depth(); in.motion = g_guides.motion();
    in.afterCopy = [](ID3D11DeviceContext* c) { dvr::capture::read_done(c); };
    in.reset = reset;
    // The offset this image was drawn with, as the backend reads it (the sign the host test proved).
    const bool fsr = g_backend.load() == BackendFsr;
    in.jitterX = (fsr ? jitter::kFsrReportX : jitter::kReportX) * gp.jitter[0];
    in.jitterY = (fsr ? jitter::kFsrReportY : jitter::kReportY) * gp.jitter[1];
    in.mvSign = fsr ? jitter::kFsrMvSign : 1.0f;
    {   // FSR's depth reconstruction: the time since this eye's last image, the render's vertical FOV
        // and one depth unit in metres (gp.depthScale uu per unit, world_scale uu per metre).
        LARGE_INTEGER t, f; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f);
        const uint64_t prev = g_lastEyeQpc[eye];
        g_lastEyeQpc[eye] = (uint64_t)t.QuadPart;
        in.frameTimeMs = prev ? (float)((double)((uint64_t)t.QuadPart - prev) * 1000.0 / (double)f.QuadPart) : 0.0f;
        if (in.frameTimeMs > 100.0f) in.frameTimeMs = 100.0f;
        in.fovY = gp.tanV > 0.0f ? 2.0f * atanf(gp.tanV) : 0.0f;
        const float uuPerM = dvr::camera::world_scale();
        in.metersPerUnit = uuPerM > 1.0f ? gp.depthScale / uuPerM : 0.0f;
    }
    if (reset) ++g_win.resets;
    // The mask, the audit and the previous-image copy they read cost GPU time on every eye image,
    // so they run only when the mask or the audit is on.
    const bool wantMask = g_mask.load(), wantAudit = g_audit.load();
    // VR-39: the hands and weapon always "trust the current colour" (their vectors are the camera's only).
    const bool wantFg = g_fgBias.load() && gp.preFg && gp.sceneDepth;
    if (wantFg) ++g_win.fgMasked;
    if (wantMask || wantAudit || wantFg) {
        const bool masked = g_guides.mask(dev, ctx, eye, src, gp.historyValid, g_maskLo.load(), g_maskHi.load(), why, sizeof(why),
                                          wantMask, wantFg ? gp.sceneDepth : nullptr, wantFg ? gp.preFg : nullptr);
        if (masked && (wantMask || wantFg)) { in.bias = g_guides.bias(); ++g_win.masked; }
        if (wantAudit)
            g_guides.audit(dev, ctx, eye, src, gp.jitterKnown ? gp.jitter[0] - gp.prevJitter[0] : 0.0f,
                           gp.jitterKnown ? gp.jitter[1] - gp.prevJitter[1] : 0.0f);
        g_guides.keep(dev, ctx, eye, color);
    } else {
        g_guides.forget(eye);
    }
    dvr::depthprobe::read_done(ctx);   // the scene depth's (and the foreground copy's) last reader was the mask above
    if (!g_client.evaluate(ctx, eye, in, why, sizeof(why))) {
        ++g_win.fallback;
        if (!g_client.running()) {
            strcpy_s(g_why, why);
            g_state.store(Failed);
            DVR_ERROR("dlss: the helper is gone (%s) - the normal path runs; `dlss retry` restarts it", why);
        } else {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "dlss: eye %d evaluate refused (%s) - this image takes the normal path", eye, why);
        }
        return nullptr;
    }
    ++g_win.eyes[eye];
    return g_client.output(eye);
}

void idle() {
    if (g_state.load() == Idle) return;
    DVR_INFO("dlss: off - stopping the helper and releasing %.1f MiB shared + %.1f MiB guides",
             g_client.bytes() / (1024.0 * 1024.0), g_guides.bytes() / (1024.0 * 1024.0));
    shutdown();
}

void shutdown() {
    join_worker();
    if (g_client.running() || g_helperStarted) g_client.stop();
    g_helperStarted = false;
    g_guides.shutdown();
    g_guideFailed = false;
    g_dev = nullptr;
    g_state.store(Idle);
    strcpy_s(g_summary, "off");
}

const char* summary() {
    if (g_mode.load() == ModeOff) return "off";
    switch (g_state.load()) {
    case Working: return g_backend.load() == BackendFsr ? "starting the FSR helper..." : "starting the DLSS helper...";
    case Failed: return g_why;
    case Idle: return "waiting for the first eye image";
    default: return g_summary;
    }
}

bool command(const char* args) {
    char sub[24] = "", val[24] = "";
    const int n = args ? sscanf(args, "%23s %23s", sub, val) : 0;
    if (n >= 1 && (!_stricmp(sub, "on") || !_stricmp(sub, "dlaa"))) { set_mode(ModeDlaa, "the seam"); return true; }
    if (n >= 1 && !_stricmp(sub, "off")) { set_mode(ModeOff, "the seam"); return true; }
    if (n >= 1 && !_stricmp(sub, "retry")) { g_retry.store(true); DVR_INFO("dlss: retry requested (the seam)"); return true; }
    if (n >= 2 && !_stricmp(sub, "backend")) { set_backend(!_stricmp(val, "fsr") || !strcmp(val, "1") ? BackendFsr : BackendDlss, "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "fsrversion")) { set_fsr_version(atoi(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "preset")) { set_preset(atoi(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "model")) {
        const char* names[] = {"k", "j", "m", "l", "permode", "fast"};
        for (int i = 0; i < kModelChoiceCount && i < 6; ++i)
            if (!_stricmp(val, names[i])) { set_model_choice(i, "the seam"); return true; }
        set_model(!strcmp(val, "1") ? 1 : 0, "the seam"); return true;
    }
    if (n >= 2 && !_stricmp(sub, "audit")) { set_audit(!_stricmp(val, "on") || !strcmp(val, "1"), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "quality")) {
        int q = atoi(val);
        for (int k = 0; k < QCount; ++k) if (!_stricmp(val, kQualityWord[k])) q = k;
        set_quality(q, "the seam"); return true;
    }
    if (n >= 2 && !_stricmp(sub, "output")) {
        // Only while Super Resolution runs: with SR off, a recorded output is what the game side
        // RESTORES, so setting one here resized the game natively to it (found in the simulator).
        unsigned ow = 0, oh = 0;
        if (sscanf(args, "%*s %u %u", &ow, &oh) == 2) {
            if (g_mode.load() == ModeOff || g_quality.load() == QDlaa) {
                DVR_WARN("dlss: output %ux%u refused - Super Resolution is not on (dlss on, dlss quality 1..4 first); "
                         "with SR off the resolution control sets the render size directly", ow, oh);
                return true;
            }
            set_output(ow, oh, "the seam"); return true;
        }
    }
    if (n >= 2 && !_stricmp(sub, "taxis")) {
        float f = 1, r = 1, u = 1;
        if (sscanf(args, "%*s %f %f %f", &f, &r, &u) == 3) { dvr::clarity::set_translation_axes(f, r, u, "the seam"); return true; }
    }
    if (n >= 2 && !_stricmp(sub, "vp")) { dvr::clarity::set_use_vp(!_stricmp(val, "on") || !strcmp(val, "1"), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "body")) { dvr::clarity::set_body_depth((float)atof(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "pos")) { dvr::clarity::set_pos_source(!_stricmp(val, "render") ? 1 : 0, "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "jitter")) {
        char w3[16] = "";
        if (!_stricmp(val, "wide") && sscanf(args, "%*s %*s %15s", w3) == 1) {
            jitter::set_wide(!_stricmp(w3, "on") || !strcmp(w3, "1"), "the seam"); return true;
        }
        jitter::set_enabled(!_stricmp(val, "on") || !strcmp(val, "1"), "the seam"); return true;
    }
    if (n >= 2 && !_stricmp(sub, "mask")) { set_mask(!_stricmp(val, "on") || !strcmp(val, "1"), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "fgbias")) { set_fg_bias(!_stricmp(val, "on") || !strcmp(val, "1"), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "maskrange")) {
        float lo = 0, hi = 0;
        if (sscanf(args, "%*s %f %f", &lo, &hi) == 2) { set_mask_range(lo, hi, "the seam"); return true; }
    }
    DVR_INFO("dlss: mode %s, preset %d, state %d (0 idle 1 working 2 ready 3 failed) | %s | words: dlss on|off, retry, backend dlss|fsr, fsrversion <0..n>, model k|j|m|l|permode|fast, preset <0..16>, quality <0..5|dlaa|ultraquality|quality|balanced|performance|ultraperformance>, output <w> <h>, "
             "audit on|off, mask on|off, jitter on|off, jitter wide on|off, "
             "maskrange <lo> <hi> | mask %s %.3f..%.3f | jitter %s",
             g_mode.load() ? "DLAA" : "off", g_preset.load(), g_state.load(), summary(), g_mask.load() ? "on" : "off",
             g_maskLo.load(), g_maskHi.load(), jitter::summary());
    return true;
}

} // namespace dvr::dlss
