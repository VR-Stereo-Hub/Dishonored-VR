// Desktop pin: the runtime's delivered tag can lag the live D3D9 backbuffer.
// All copies run after XR capture. DEFAULT-pool surfaces obey the Reset law.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/desktop_eye.h"
#include "core/util/log.h"
#include "core/framework/status.h"
#include "core/util/clock.h"
#include <atomic>
#include <string.h>

namespace dvr::desktop_eye {
namespace {
IDirect3DDevice9* g_dev = nullptr; // identity only, never AddRef'd
IDirect3DSurface9* g_held = nullptr;
UINT g_w = 0, g_h = 0;
D3DFORMAT g_fmt = D3DFMT_UNKNOWN;
bool g_on = true, g_refused = false;
bool g_reduce = false, g_callbackPending = false;
bool g_mirrorOff = true, g_submitRefused = false;
bool g_strictOff = false; // Opt-in: no desktop refresh for capture/callback gaps in a running XR session.
IDirect3DQuery9* g_submitQuery = nullptr;
int g_callbackTag = 0;
uint32_t g_leftPresented = 0;
bool g_ppChecked = false, g_ppSupported = false;
struct PresentWindow {
    uint32_t hooks = 0, calls = 0, skips = 0, offSkips = 0, errors = 0, queryFailed = 0, strictSkips = 0;
    uint32_t context = 0, parameters = 0, pin = 0, predecessor = 0, notRight = 0;
    double nativeMs = 0, mirrorMs = 0, flushMs = 0, nativeMaxMs = 0;
} g_presentWindow;
ULONGLONG g_presentBeatMs = 0;
Source g_source = Source::Draw; // VR-76: headset-confirmed 2026-09-11; tag is the A/B
Policy g_policy, g_old;
int g_oldShown = 0;
uint32_t g_present = 0;
int g_draw = 0;
Record g_records[1024];
uint32_t g_snaps = 0, g_blits = 0, g_failSnap = 0, g_failBlit = 0, g_recreate = 0;
std::atomic<uint32_t> g_singles{0};
struct Window {
    uint32_t draws[3] = {}, same = 0, opposite = 0, zero = 0;
    uint32_t snaps = 0, blits = 0, oldChanges = 0, oldRawLeaks = 0;
    uint32_t shownRight = 0, shownUnknown = 0, warmupRight = 0, failures = 0;
} g_window;
ULONGLONG g_beatMs = 0;

void invalidate() {
    g_policy.reset(); g_old.reset(); g_oldShown = 0;
    g_leftPresented = 0;
}

bool supported_parameters() {
    if (g_ppChecked) return g_ppSupported;
    g_ppChecked = true;
    IDirect3DSwapChain9* swap = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    HRESULT hr = g_dev->GetSwapChain(0, &swap);
    if (SUCCEEDED(hr) && swap) hr = swap->GetPresentParameters(&pp);
    else hr = E_FAIL;
    if (swap) swap->Release();
    g_ppSupported = hr == D3D_OK && pp.Windowed && pp.SwapEffect == D3DSWAPEFFECT_DISCARD &&
        pp.BackBufferCount == 1 && pp.MultiSampleType == D3DMULTISAMPLE_NONE &&
        pp.PresentationInterval == D3DPRESENT_INTERVAL_IMMEDIATE;
    DVR_INFO("desktoppresent: parameters supported=%d hr=0x%08lx windowed=%d swap=%u buffers=%u msaa=%u interval=0x%08x; unsupported uses original Present",
        g_ppSupported, (unsigned long)hr, (int)pp.Windowed, (unsigned)pp.SwapEffect,
        pp.BackBufferCount, (unsigned)pp.MultiSampleType, pp.PresentationInterval);
    return g_ppSupported;
}

// Without any desktop Present, explicitly submit the D3D9 command buffer.
// S_FALSE is a pending event, not completion; no wait or resource reuse relies
// on this query. The capture bridge retains its own ownership fences.
bool submit_without_present() {
    if (g_submitRefused) return false;
    HRESULT hr = D3D_OK;
    if (!g_submitQuery) hr = g_dev->CreateQuery(D3DQUERYTYPE_EVENT, &g_submitQuery);
    if (SUCCEEDED(hr) && g_submitQuery) {
        // Always cover THIS frame. Polling an already-completed older event
        // can return S_OK without flushing newly queued commands (native host
        // test failed on frame 2). END may abandon a pending event; this query
        // owns no resource/fence completion, so its old result is not needed.
        hr = g_submitQuery->Issue(D3DISSUE_END);
        if (hr == D3D_OK) {
            hr = g_submitQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH);
            if (hr == D3D_OK || hr == S_FALSE) return true;
        }
    }
    g_submitRefused = true;
    DVR_WARN("desktoppresent: OFF refused submit query hr=0x%08lx query=%p; original Present until reset or mode toggle",
        (unsigned long)hr, (void*)g_submitQuery);
    return false;
}

bool ensure_surface(IDirect3DSurface9* bb) {
    D3DSURFACE_DESC d;
    if (FAILED(bb->GetDesc(&d))) return false;
    if (g_held && (d.Width != g_w || d.Height != g_h || d.Format != g_fmt)) {
        g_held->Release(); g_held = nullptr;
        invalidate(); ++g_recreate;
        DVR_INFO("desktopeye: surface changed %ux%u -> %ux%u, invalidating held pixels",
                 g_w, g_h, d.Width, d.Height);
    }
    if (g_held) return true;
    invalidate();
    const HRESULT hr = g_dev->CreateRenderTarget(d.Width, d.Height, d.Format,
        D3DMULTISAMPLE_NONE, 0, FALSE, &g_held, nullptr);
    if (FAILED(hr) || !g_held) {
        g_held = nullptr; g_refused = true;
        DVR_WARN("desktopeye: REFUSED CreateRenderTarget(%ux%u fmt %d) -> 0x%08lx; "
                 "desktop unpinned, headset unaffected", d.Width, d.Height, (int)d.Format, (unsigned long)hr);
        return false;
    }
    g_w = d.Width; g_h = d.Height; g_fmt = d.Format;
    DVR_INFO("desktopeye: surface ready %ux%u fmt %d source=%s (no valid snapshot yet)",
             g_w, g_h, (int)g_fmt, source_name());
    return true;
}

void beat() {
    const ULONGLONG now = GetTickCount64();
    if (!g_beatMs) g_beatMs = now;
    if (now - g_beatMs < 15000) return;
    const Window& w = g_window;
    DVR_INFO("desktopeye: source=%s window=%llu ms draw L/R/0=%u/%u/%u "
             "tag same/opposite/zero=%u/%u/%u singleTicks=%u snap/blit=%u/%u "
             "oldShadowChanges=%u oldShadowRawLeaks=%u shownR=%u shownUnknown=%u "
             "warmupR=%u failures=%u; eyes inferred from copy provenance, not pixel verification. "
             "Draw pass needs bursts and oldShadowRawLeaks>0, shownR=0 after warmup, failures=0.",
             source_name(), now - g_beatMs, w.draws[0], w.draws[2], w.draws[1],
             w.same, w.opposite, w.zero, g_singles.exchange(0), w.snaps, w.blits,
             w.oldChanges, w.oldRawLeaks, w.shownRight, w.shownUnknown, w.warmupRight, w.failures);
    g_window = Window{}; g_beatMs = now;
}
} // namespace

void set_device(IDirect3DDevice9* dev) {
    if (dev == g_dev) return;
    on_reset(); g_dev = dev;
}
void begin_present(uint32_t present) {
    g_present = present; g_draw = 0;
    g_callbackPending = false;
    Record& r = g_records[present % 1024];
    r = Record{}; r.present = present;
    r.source = g_source == Source::Draw ? 'd' : 't';
}
void note_drawn_eye(int eye) {
    g_draw = eye < 0 ? -1 : eye > 0 ? 1 : 0;
    g_records[g_present % 1024].draw = g_draw;
}
void note_single_draw() { g_singles.fetch_add(1, std::memory_order_relaxed); }
bool record_for(uint32_t present, Record& out) {
    out = g_records[present % 1024];
    return present != 0 && out.present == present;
}

static void apply_pin(int eyeSign, bool allowSkip) {
    const double start = dvr::clock::now_ms();
    Record& r = g_records[g_present % 1024];
    r.tag = eyeSign; r.shown = g_draw; r.action = 'N';
    ++g_window.draws[g_draw + 1];
    if (!g_draw || !eyeSign) ++g_window.zero;
    else if (g_draw == eyeSign) ++g_window.same;
    else ++g_window.opposite;
    if (g_on && !g_refused && g_dev) {
        IDirect3DSurface9* bb = nullptr;
        const HRESULT got = g_dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        if (SUCCEEDED(got) && bb && ensure_surface(bb)) {
            // Counterfactual old policy assumes successful copies. It uses the
            // current draw as provenance, never the delayed tag as pixel identity.
            const Action oldAction = g_old.decide(Source::Tag, g_draw, eyeSign);
            const int oldShown = oldAction == Action::Blit ? g_old.heldEye : g_draw;
            if (oldAction == Action::None && g_old.valid && g_draw &&
                g_old.heldEye && g_draw != g_old.heldEye) ++g_window.oldRawLeaks;
            if (oldShown && g_oldShown && oldShown != g_oldShown) ++g_window.oldChanges;
            g_oldShown = oldShown;
            g_old.copied(oldAction, g_draw, true);

            const Action action = g_policy.decide(g_source, g_draw, eyeSign);
            if (g_reduce) {
                r.reduceReason = !allowSkip ? 'C' : g_source != Source::Draw ? 'I' :
                    !supported_parameters() ? 'P' : g_draw != 1 ? 'R' :
                    action != Action::Blit || !g_policy.valid || g_policy.heldEye != -1 ? 'I' :
                    !g_leftPresented || g_leftPresented + 1 != g_present ? 'L' : 'K';
            }
            if (g_source == Source::Draw && !g_policy.valid && g_draw > 0)
                ++g_window.warmupRight;
            bool success = true;
            if (action == Action::Snapshot) {
                success = SUCCEEDED(g_dev->StretchRect(bb, nullptr, g_held, nullptr, D3DTEXF_NONE));
                if (success) { ++g_snaps; ++g_window.snaps; r.action = 'S'; }
                else ++g_failSnap;
            } else if (r.reduceReason == 'K') {
                // Both XR capture/submission and the prior successful left
                // desktop Present have already happened. Keep the desktop's
                // displayed pixels; do not write the current right backbuffer.
                r.action = 'K'; r.shown = -1;
            } else if (action == Action::Blit) {
                success = SUCCEEDED(g_dev->StretchRect(g_held, nullptr, bb, nullptr, D3DTEXF_NONE));
                if (success) { ++g_blits; ++g_window.blits; r.action = 'B'; r.shown = g_policy.heldEye; }
                else ++g_failBlit;
            }
            g_policy.copied(action, g_draw, success);
            if (!success) { r.action = 'F'; ++g_window.failures; }
        } else { r.action = 'F'; ++g_window.failures; }
        if (bb) bb->Release();
    }
    if (g_on && g_refused && r.action != 'F') { r.action = 'F'; ++g_window.failures; }
    if (r.shown > 0) ++g_window.shownRight;
    if (!r.shown) ++g_window.shownUnknown;
    r.mirrorMs += dvr::clock::now_ms() - start;
    beat();
}

void on_present(int eyeSign) {
    if (g_reduce || g_mirrorOff) {
        // The runtime may fail/end its session after this callback. Decide at
        // the host tail using its final live state, never a provisional skip.
        g_callbackTag = eyeSign; g_callbackPending = true;
        return;
    }
    apply_pin(eyeSign, false);
}

HRESULT present(PresentFn native, IDirect3DDevice9* dev, const RECT* src,
                const RECT* dst, HWND wnd, const RGNDATA* dirty, bool stereoReady, bool xrReady, bool xrRunning) {
    Record& r = g_records[g_present % 1024];
    const bool standard = !src && !dst && !wnd && !dirty && dev == g_dev;
    const bool context = stereoReady && standard;
    if (g_reduce || g_mirrorOff) r.reduceReason = 'I'; // missing callback or pin
    const bool strictReady = g_mirrorOff && g_strictOff && xrRunning;
    if (g_callbackPending || strictReady) {
        if (!g_callbackPending) g_callbackTag = 0;
        g_callbackPending = false;
        bool offReady = false;
        if (g_mirrorOff) {
            r.reduceReason = !((xrReady || strictReady) && standard) ? 'C' : !supported_parameters() ? 'P' : 'O';
            if (r.reduceReason == 'O') {
                const double start = dvr::clock::now_ms();
                offReady = submit_without_present();
                r.flushMs = dvr::clock::now_ms() - start;
                if (!offReady) r.reduceReason = 'Q';
            }
        }
        if (offReady) {
            r.tag = g_callbackTag; r.action = 'O'; r.shown = 0; // retained desktop pixels, no new image
            invalidate(); // a later fallback must not restore an old pre-off snapshot
        } else {
            const char refusal = r.reduceReason;
            apply_pin(g_callbackTag, context && !g_mirrorOff);
            if (g_mirrorOff) r.reduceReason = refusal;
        }
    }
    const bool skip = (g_reduce && r.action == 'K') || (g_mirrorOff && r.action == 'O');
    HRESULT hr = D3D_OK;
    g_leftPresented = 0; // a token permits at most one adjacent omitted call
    if (!skip) {
        const double start = dvr::clock::now_ms();
        hr = native(dev, src, dst, wnd, dirty);
        r.nativeMs = dvr::clock::now_ms() - start; r.nativeCalled = true;
        if (g_reduce && !g_mirrorOff && context && hr == D3D_OK && r.action == 'S' && r.draw == -1 &&
            g_source == Source::Draw && g_policy.valid && g_policy.heldEye == -1)
            g_leftPresented = g_present;
    }
    auto& w = g_presentWindow;
    ++w.hooks;
    if (skip) ++w.skips; else ++w.calls;
    if (r.action == 'O') { ++w.offSkips; if (strictReady) ++w.strictSkips; }
    if (hr != D3D_OK) ++w.errors;
    w.nativeMs += r.nativeMs; w.mirrorMs += r.mirrorMs; w.flushMs += r.flushMs;
    if (r.nativeMs > w.nativeMaxMs) w.nativeMaxMs = r.nativeMs;
    switch (r.reduceReason) {
    case 'C': ++w.context; break; case 'P': ++w.parameters; break;
    case 'I': ++w.pin; break; case 'L': ++w.predecessor; break; case 'R': ++w.notRight; break;
    case 'Q': ++w.queryFailed; break;
    }
    const ULONGLONG now = GetTickCount64();
    if (!g_presentBeatMs) g_presentBeatMs = now;
    if (now - g_presentBeatMs >= 3000) {
        DVR_INFO("desktoppresent: reduced=%d off=%d window=%llu ms hooks=%u actual=%u skipped=%u offSkips=%u nonOK=%u "
            "fallback context/parameters/pin/predecessor/notRight/query=%u/%u/%u/%u/%u/%u "
            "cpu native=%.3f ms/hook %.3f ms/call max=%.3f ms mirror=%.3f ms/hook flush=%.3f ms/hook; "
            "strict=%d strictSkips=%u; not a fresh-XR-pair count or GPU time; compare total frame tails for moved waits",
            g_reduce, g_mirrorOff, now - g_presentBeatMs, w.hooks, w.calls, w.skips, w.offSkips, w.errors,
            w.context, w.parameters, w.pin, w.predecessor, w.notRight, w.queryFailed,
            w.nativeMs / w.hooks, w.calls ? w.nativeMs / w.calls : 0, w.nativeMaxMs,
            w.mirrorMs / w.hooks, w.flushMs / w.hooks, g_strictOff, w.strictSkips);
        w = PresentWindow{}; g_presentBeatMs = now;
    }
    return hr;
}

void set_reduced_present(bool on) {
    if (on != g_reduce) {
        // Do not release the valid left snapshot on a live switch. Only the
        // Present permission expires; off restores the accepted copy policy.
        g_reduce = on; g_leftPresented = 0; g_callbackPending = false;
        g_presentWindow = PresentWindow{}; g_presentBeatMs = GetTickCount64();
    }
    DVR_INFO("desktoppresent: ReduceDesktopPresent=%d; live A/B desktoppresent full|reduced|off", g_reduce);
}
bool reduced_present() { return g_reduce; }

void set_mirror_off(bool on) {
    if (on != g_mirrorOff) {
        g_mirrorOff = on; invalidate(); g_callbackPending = false;
        if (g_submitQuery) { g_submitQuery->Release(); g_submitQuery = nullptr; }
        g_submitRefused = false;
        g_presentWindow = PresentWindow{}; g_presentBeatMs = GetTickCount64();
    }
    DVR_INFO("desktoppresent: DesktopMirrorOff=%d; OFF freezes desktop updates while XR capture remains live, guarded fallback still presents", g_mirrorOff);
}
bool mirror_off() { return g_mirrorOff; }
void set_strict_off(bool on) {
    if (on != g_strictOff) { g_strictOff = on; on_reset(); }
    DVR_INFO("desktoppresent: DesktopMirrorStrictOff=%d; running XR suppresses capture/callback-gap desktop refresh; stopped XR, unsupported parameters or submit failure still fall back", g_strictOff);
}
bool strict_off() { return g_strictOff; }

void on_reset() {
    if (g_submitQuery) { g_submitQuery->Release(); g_submitQuery = nullptr; }
    g_submitRefused = false;
    if (g_held) { g_held->Release(); g_held = nullptr; }
    g_w = g_h = 0; g_fmt = D3DFMT_UNKNOWN; g_refused = false;
    invalidate();
    g_ppChecked = g_ppSupported = false; g_callbackPending = false;
}
void shutdown() { on_reset(); g_dev = nullptr; }
void set_enabled(bool on) {
    if (on == g_on) return;
    g_on = on; on_reset();
    DVR_INFO("desktopeye: %s source=%s", on ? "ON" : "off", source_name());
}
bool enabled() { return g_on; }
const char* source_name() { return g_source == Source::Draw ? "draw" : "tag"; }
bool set_source(const char* name, const char* origin) {
    Source next;
    if (!_stricmp(name, "draw")) next = Source::Draw;
    else if (!_stricmp(name, "tag")) next = Source::Tag;
    else {
        DVR_WARN("desktopeye: invalid source '%s' from %s; keeping %s", name, origin, source_name());
        return false;
    }
    if (next != g_source) {
        g_source = next; on_reset(); g_window = Window{};
        g_singles.exchange(0); g_beatMs = GetTickCount64();
    }
    DVR_INFO("desktopeye: DesktopEyeSource=%s from %s; live A/B: desktopeye draw|tag", source_name(), origin);
    return true;
}
void status(dvr::status::Writer& w) {
    w.kv("reduceDesktopPresent", g_reduce);
    w.kv("desktopMirrorOff", g_mirrorOff);
    w.kv("desktopMirrorStrictOff", g_strictOff);
    w.kv("desktopEye", g_on);
    w.kv("desktopEyeSource", source_name());
    w.kv("desktopEyeValid", g_policy.valid);
    w.kv("desktopEyeSnaps", (unsigned long)g_snaps);
    w.kv("desktopEyeBlits", (unsigned long)g_blits);
    w.kv("desktopEyeFailed", (unsigned long)(g_failSnap + g_failBlit));
}
void log_status() {
    DVR_INFO("desktoppresent: reduced=%d off=%d supported=%d checked=%d submitRefused=%d lastLeft=%u window hooks/actual/skipped=%u/%u/%u",
        g_reduce, g_mirrorOff, g_ppSupported, g_ppChecked, g_submitRefused, g_leftPresented,
        g_presentWindow.hooks, g_presentWindow.calls, g_presentWindow.skips);
    DVR_INFO("desktopeye: %s source=%s valid=%d heldEye=%d snaps=%u blits=%u "
             "failedSnap=%u failedBlit=%u rebuilds=%u refused=%d",
             g_on ? "ON" : "off", source_name(), g_policy.valid, g_policy.heldEye,
             g_snaps, g_blits, g_failSnap, g_failBlit, g_recreate, g_refused);
}
} // namespace dvr::desktop_eye
