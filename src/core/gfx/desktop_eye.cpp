// Desktop pin: the runtime's delivered tag can lag the live D3D9 backbuffer.
// All copies run after XR capture. DEFAULT-pool surfaces obey the Reset law.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/desktop_eye.h"
#include "core/util/log.h"
#include "core/framework/status.h"
#include <atomic>
#include <string.h>

namespace dvr::desktop_eye {
namespace {
IDirect3DDevice9* g_dev = nullptr; // identity only, never AddRef'd
IDirect3DSurface9* g_held = nullptr;
UINT g_w = 0, g_h = 0;
D3DFORMAT g_fmt = D3DFMT_UNKNOWN;
bool g_on = true, g_refused = false;
Source g_source = Source::Tag; // new render lever defaults off
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

void on_present(int eyeSign) {
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
            if (g_source == Source::Draw && !g_policy.valid && g_draw > 0)
                ++g_window.warmupRight;
            bool success = true;
            if (action == Action::Snapshot) {
                success = SUCCEEDED(g_dev->StretchRect(bb, nullptr, g_held, nullptr, D3DTEXF_NONE));
                if (success) { ++g_snaps; ++g_window.snaps; r.action = 'S'; }
                else ++g_failSnap;
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
    beat();
}

void on_reset() {
    if (g_held) { g_held->Release(); g_held = nullptr; }
    g_w = g_h = 0; g_fmt = D3DFMT_UNKNOWN; g_refused = false;
    invalidate();
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
    w.kv("desktopEye", g_on);
    w.kv("desktopEyeSource", source_name());
    w.kv("desktopEyeValid", g_policy.valid);
    w.kv("desktopEyeSnaps", (unsigned long)g_snaps);
    w.kv("desktopEyeBlits", (unsigned long)g_blits);
    w.kv("desktopEyeFailed", (unsigned long)(g_failSnap + g_failBlit));
}
void log_status() {
    DVR_INFO("desktopeye: %s source=%s valid=%d heldEye=%d snaps=%u blits=%u "
             "failedSnap=%u failedBlit=%u rebuilds=%u refused=%d",
             g_on ? "ON" : "off", source_name(), g_policy.valid, g_policy.heldEye,
             g_snaps, g_blits, g_failSnap, g_failBlit, g_recreate, g_refused);
}
} // namespace dvr::desktop_eye
