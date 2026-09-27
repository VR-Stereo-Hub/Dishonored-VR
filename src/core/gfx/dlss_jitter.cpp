// core/gfx/dlss_jitter.cpp - see dlss_jitter.h. Every function here runs on the D3D9 thread
// (the vertex-constant hook, SetRenderTarget and the present path are all that device's calls),
// except set_enabled/enabled/summary, which the seam and F10 call; those touch only atomics and a
// string the present path rewrites whole.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/dlss_jitter.h"
#include "core/gfx/dlss.h"
#include "core/vr/pose_record.h"
#include "core/util/clock.h"
#include "core/util/log.h"

#include <windows.h>
#include <atomic>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::dlss::jitter {
namespace {

std::atomic<bool> g_enabled{false};
// Also shift perspective draws into ANY colour target the size of the eye image, whatever depth
// surface is bound (none, or another one). The headset run of the depth-keyed build still showed
// ~570 such uploads/s unshifted and black speckles flickering on textures in the left eye only -
// the signature of one depth-writing or depth-testing pass carrying a different shift. Live A/B.
std::atomic<bool> g_wide{true};

// The bound colour target and depth-stencil surface (identity only: never dereferenced, never
// AddRef'd).
const void* g_rt = nullptr;
uint32_t g_rtW = 0, g_rtH = 0;
const void* g_ds = nullptr;

// Depth surfaces the world pass was observed testing against, with its viewport and colour
// target. THE DEPTH SURFACE IS THE IDENTITY, not the colour target: the first simulator run keyed
// on the colour target and the left eye lost whole lit surfaces, because a depth-writing pass
// drew into ANOTHER eye-size colour target unshifted and later passes depth-tested against it.
// Every pass that shares the scene depth must carry the same shift. An entry not seen for kStale
// presents is dropped, so a freed surface whose address is reused cannot stay a world target for
// long - and it must also still have the confirmed viewport to be shifted.
struct World { const void* ds; const void* rt; uint32_t vw, vh; uint32_t seen; };
World g_world[4] = {};
const uint32_t kStale = 120;
uint32_t g_presents = 0;

bool g_live = false;            // applied to uploads until the next present
bool g_confirmed = false;
uint32_t g_confW = 0, g_confH = 0;
float g_sx = 0, g_sy = 0, g_ax = 0, g_ay = 0;
uint32_t g_drawsThis = 0;       // uploads shifted since the last present
// This image's uploads by class, attributed to its eye at the present (the draws do not know
// their eye; the present that follows them does): shifted on the scene depth, shifted by the
// wide rule, left unshifted into an eye-size target, and how many of those had no depth bound.
uint32_t g_iDepth = 0, g_iWide = 0, g_iEyeLeft = 0, g_iNoDs = 0;
uint32_t g_phase = 0, g_phasePair = 0, g_phaseUses = 0, g_phases = 8;
double g_lastPresentMs = 0;

// The 5 s census: what was shifted and what was deliberately not.
struct Census {
    uint64_t shifted = 0, shiftedOtherRt = 0, affine = 0, other = 0, images = 0, imagesJittered = 0, untagged = 0,
             pairs = 0, gaps = 0;
    struct Size { uint32_t w, h; uint64_t n; } sizes[6] = {};
    uint64_t eyeImages[2] = {}, eyeDepth[2] = {}, eyeWide[2] = {}, eyeLeft[2] = {}, eyeNoDs[2] = {};
};
Census g_c;
double g_censusMs = 0;
char g_summary[200] = "off";
char g_refusal[200] = "";

World* find_world(const void* ds) {
    for (auto& w : g_world) if (w.ds == ds && ds) return &w;
    return nullptr;
}

void note_other_size(uint32_t w, uint32_t h) {
    for (auto& s : g_c.sizes) if (s.n && s.w == w && s.h == h) { ++s.n; return; }
    for (auto& s : g_c.sizes) if (!s.n) { s.w = w; s.h = h; s.n = 1; return; }
}

uint32_t phases_for_mode() {
    // NVIDIA's DLSS guide: about 8 * (output / render)^2 phases. DLAA 8, Quality 18, Performance 32.
    const float r = dvr::dlss::quality() == dvr::dlss::QDlaa ? 1.0f : dvr::dlss::ratio();
    const uint32_t n = (uint32_t)(8.0f * r * r + 0.5f);
    return n < 8 ? 8 : n;
}

void census_tick() {
    const double now = dvr::clock::now_ms();
    if (!g_censusMs) { g_censusMs = now; return; }
    if (now - g_censusMs < 5000) return;
    const double s = (now - g_censusMs) / 1000.0;
    char others[200] = ""; int m = 0;
    for (const auto& z : g_c.sizes)
        if (z.n) m += _snprintf_s(others + m, sizeof(others) - m, _TRUNCATE, " %ux%u %.0f/s", z.w, z.h, z.n / s);
    DVR_INFO("dlss/jitter: %s | %.0f images/s, %.0f/s jittered (%llu untagged) | c0..c3 uploads shifted %.0f/s (%.0f/s of them "
             "into another colour target on the scene depth); NOT shifted: "
             "affine (2D/post/HUD) %.0f/s, perspective on other targets %.0f/s:%s | world target %ux%u, %u phases, pairs %.0f/s "
             "(%llu partner gaps) | last offset %+.3f %+.3f px",
             g_live ? "LIVE" : (g_enabled.load() ? "REFUSED" : "off"), g_c.images / s, g_c.imagesJittered / s,
             (unsigned long long)g_c.untagged, g_c.shifted / s, g_c.shiftedOtherRt / s, g_c.affine / s, g_c.other / s,
             m ? others : " none",
             g_confW, g_confH, g_phases, g_c.pairs / s, (unsigned long long)g_c.gaps, g_sx, g_sy);
    {
        auto per = [](uint64_t v, uint64_t n) { return n ? (double)v / n : 0.0; };
        DVR_INFO("dlss/jitter per eye, uploads per image: L %llu images: scene depth %.1f, wide %.1f, eye-size UNSHIFTED %.1f "
                 "(no depth bound %.1f) | R %llu images: scene depth %.1f, wide %.1f, eye-size UNSHIFTED %.1f (no depth bound "
                 "%.1f) | wide rule %s. An eye whose passes differ from the other's is where one pass can carry a different "
                 "shift; UNSHIFTED > 0 with the wide rule on would mean a draw still escapes it",
                 (unsigned long long)g_c.eyeImages[0], per(g_c.eyeDepth[0], g_c.eyeImages[0]), per(g_c.eyeWide[0], g_c.eyeImages[0]),
                 per(g_c.eyeLeft[0], g_c.eyeImages[0]), per(g_c.eyeNoDs[0], g_c.eyeImages[0]),
                 (unsigned long long)g_c.eyeImages[1], per(g_c.eyeDepth[1], g_c.eyeImages[1]), per(g_c.eyeWide[1], g_c.eyeImages[1]),
                 per(g_c.eyeLeft[1], g_c.eyeImages[1]), per(g_c.eyeNoDs[1], g_c.eyeImages[1]), g_wide.load() ? "ON" : "off");
    }
    g_c = Census{};
    g_censusMs = now;
}

} // namespace

void set_enabled(bool on, const char* who) {
    if (g_enabled.exchange(on) == on) return;
    DVR_INFO("dlss: projection jitter %s (live, %s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - applied only while DLSS runs and the world target is confirmed; the next presents say which" : "");
}
bool enabled() { return g_enabled.load(); }
void set_wide(bool on, const char* who) {
    if (g_wide.exchange(on) == on) return;
    DVR_INFO("dlss: projection jitter wide rule %s (live, %s) - %s", on ? "ON" : "off", who ? who : "?",
             on ? "every perspective draw into an eye-size target is shifted, whatever depth is bound"
                : "only draws on the observed scene depth surface are shifted");
}
bool wide() { return g_wide.load(); }

void note_render_target(const void* rt, uint32_t w, uint32_t h) { g_rt = rt; g_rtW = w; g_rtH = h; }
void note_depth_stencil(const void* ds) { g_ds = ds; }

void note_world_pass(uint32_t vw, uint32_t vh) {
    if (!g_ds || vw < 320 || vh < 240) {
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
                         "dlss/jitter: world pass NOT learned - depth surface %p, viewport %ux%u (no depth surface has been "
                         "bound through SetDepthStencilSurface, or the viewport is too small)", g_ds, vw, vh);
        return;
    }
    if (World* w = find_world(g_ds)) { w->rt = g_rt; w->vw = vw; w->vh = vh; w->seen = g_presents; return; }
    World* slot = &g_world[0];
    for (auto& w : g_world) if (!w.ds || g_presents - w.seen > g_presents - slot->seen) slot = &w;
    const bool fresh = !slot->ds;
    *slot = World{g_ds, g_rt, vw, vh, g_presents};
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
                     "dlss/jitter: world pass observed on depth surface %p (colour target %p %ux%u, viewport %ux%u)%s", g_ds,
                     g_rt, g_rtW, g_rtH, vw, vh, fresh ? "" : " - replaced the least recently seen entry");
}

bool shift_for_upload(bool perspective, float* ax, float* ay) {
    if (!g_live) return false;
    // Only the reentry present path advances and records the jitter. If it stops calling in (the
    // method switched to mono, a movie, a menu hold), stop shifting: nothing would record or
    // resolve the offset. Checked every 64th upload to keep the clock off the per-draw path.
    static uint32_t tick = 0;
    if ((++tick & 63) == 0 && dvr::clock::now_ms() - g_lastPresentMs > 250.0) {
        g_live = false;
        DVR_WARN("dlss/jitter: stopped - no stereo present for %.0f ms (another method or a hold owns the frame)",
                 dvr::clock::now_ms() - g_lastPresentMs);
        return false;
    }
    if (!perspective) { ++g_c.affine; return false; }
    const World* w = find_world(g_ds);
    if (w && w->vw == g_confW && w->vh == g_confH && g_presents - w->seen <= kStale) {
        *ax = g_ax; *ay = g_ay;
        ++g_drawsThis; ++g_c.shifted; ++g_iDepth;
        if (g_rt != w->rt) ++g_c.shiftedOtherRt;
        return true;
    }
    const bool eyeSize = g_rtW == g_confW && g_rtH == g_confH;
    if (eyeSize && g_wide.load()) {
        *ax = g_ax; *ay = g_ay;
        ++g_drawsThis; ++g_c.shifted; ++g_iWide;
        return true;
    }
    if (eyeSize) { ++g_iEyeLeft; if (!g_ds) ++g_iNoDs; }
    ++g_c.other;
    note_other_size(g_rtW, g_rtH);
    return false;
}

void on_present(uint32_t recId, uint32_t capW, uint32_t capH) {
    ++g_presents;
    g_lastPresentMs = dvr::clock::now_ms();
    // 1. The image just presented: store the offset its draws carried (0 when none were shifted).
    ++g_c.images;
    if (g_drawsThis) ++g_c.imagesJittered;
    if (recId) dvr::pose::note_render_jitter(recId, g_sx, g_sy, g_drawsThis);
    else ++g_c.untagged;
    const bool wasJittered = g_drawsThis != 0;
    g_drawsThis = 0;
    dvr::pose::Record rec = {};
    const bool haveRec = recId && dvr::pose::copy(recId, &rec);
    if (haveRec && (rec.eye == -1 || rec.eye == 1)) {
        const int e = rec.eye < 0 ? 0 : 1;
        ++g_c.eyeImages[e]; g_c.eyeDepth[e] += g_iDepth; g_c.eyeWide[e] += g_iWide;
        g_c.eyeLeft[e] += g_iEyeLeft; g_c.eyeNoDs[e] += g_iNoDs;
    }
    g_iDepth = g_iWide = g_iEyeLeft = g_iNoDs = 0;

    // 2. Is it allowed for the next image? The world target must have been seen recently with a
    //    viewport the size of the image the capture delivers.
    const bool want = g_enabled.load();
    static uint32_t wantPresents = 0;   // the world pass is only watched while enabled: give it a few presents
    wantPresents = want ? wantPresents + 1 : 0;
    bool confirmed = false;
    uint32_t seenW = 0, seenH = 0;
    for (const auto& w : g_world) {
        if (!w.ds || g_presents - w.seen > kStale) continue;
        seenW = w.vw; seenH = w.vh;
        if (w.vw == capW && w.vh == capH) { confirmed = true; break; }
    }
    const bool dlssOn = dvr::dlss::active();
    const bool live = want && dlssOn && confirmed;
    if (want) {
        char why[200] = "";
        if (!dlssOn) _snprintf_s(why, _TRUNCATE, "DLSS is not running (%s)", dvr::dlss::summary());
        else if (!confirmed && !seenW)
            _snprintf_s(why, _TRUNCATE, "no world pass observed in the last %u presents (capture %ux%u)", kStale, capW, capH);
        else if (!confirmed)
            _snprintf_s(why, _TRUNCATE, "the world pass viewport %ux%u is not the captured image %ux%u", seenW, seenH, capW, capH);
        if (wantPresents <= 4 && !confirmed && !seenW) why[0] = 0;   // just switched on: not a refusal yet
        if (strcmp(why, g_refusal) != 0) {
            strcpy_s(g_refusal, why);
            if (why[0]) DVR_WARN("dlss/jitter: REFUSED - %s; images are drawn unjittered", why);
        }
    } else {
        g_refusal[0] = 0;
    }
    if (live != g_live || (live && (capW != g_confW || capH != g_confH)))
        DVR_INFO("dlss/jitter: %s (world target %ux%u = the captured image, %u phases)", live ? "LIVE" : "stopped", capW, capH,
                 phases_for_mode());
    g_live = live;
    g_confirmed = confirmed;
    if (confirmed) { g_confW = capW; g_confH = capH; }

    // 3. The phase: both eyes of one stereo pair share it; it advances after the pair's second image.
    if (live && recId) {
        const uint32_t pair = haveRec ? rec.pairId : 0;
        if (wasJittered && g_phaseUses && pair == g_phasePair) {
            ++g_phase; g_phaseUses = 0; ++g_c.pairs;
        } else {
            if (wasJittered && g_phaseUses) ++g_c.gaps;   // a pair lost its partner: keep the phase for this one's
            g_phasePair = pair; g_phaseUses = wasJittered ? 1 : 0;
        }
    }
    g_phases = phases_for_mode();
    if (live) {
        phase_offset(g_phase, g_phases, &g_sx, &g_sy);
        ndc_shift(g_sx, g_sy, capW, capH, &g_ax, &g_ay);
    } else {
        g_sx = g_sy = g_ax = g_ay = 0;
        g_phaseUses = 0;
    }
    if (want) census_tick();
    if (!want) strcpy_s(g_summary, "off");
    else if (live) _snprintf_s(g_summary, _TRUNCATE, "on, %u phases at %ux%u", g_phases, capW, capH);
    else _snprintf_s(g_summary, _TRUNCATE, "waiting: %s", g_refusal[0] ? g_refusal : "?");
}

const char* summary() { return g_summary; }

} // namespace dvr::dlss::jitter
