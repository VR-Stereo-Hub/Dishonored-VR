// core/gfx/desktop_eye.cpp - see the header for why this exists.
//
// THE FAULT THIS FIXES, measured 2026-09-06 and confirmed in review.
//
// `hkPresent` calls the game's original Present for EVERY eye draw, and the
// runtime layer's `mirror_present()` only ever incremented counters - its
// comment said the D3D9 pin was unimplemented, and it was. So with a healthy
// sequential-stereo stream the game window shows
//
//     L(k), R(k), L(k+1), R(k+1), ...
//
// while the headset correctly receives [L(k),R(k)], [L(k+1),R(k+1)]. A
// recording of the window therefore alternates between two camera positions one
// IPD apart, frame by frame - which is exactly what alternate-eye rendering
// looks like, and it is what a tester reported for months. The headset was
// never doing AER; the desktop had no eye policy at all.
//
// Note what this does NOT claim. It does not explain headset ghosting, frame
// pacing, or the pause-transition failure - those are separate and are tracked
// separately. It makes the WINDOW show one view.
//
// THE ORDER MATTERS. The snapshot is taken on the left eye's present and the
// re-blit happens on the right eye's present, both from the runtime layer's
// existing call sites, and the re-blit runs only AFTER that eye's XR capture.
// Blitting before the capture would feed the left image to the right eye and
// silently destroy the stereo the headset receives.
//
// THE RESET LAW. The surface is D3DPOOL_DEFAULT because StretchRect's source
// and destination must both be device-resident. It is released in on_reset(),
// before the game's Reset runs; a default-pool object still held there makes
// the game's Reset fail forever.

#include "core/gfx/desktop_eye.h"
#include "core/util/log.h"
#include "core/framework/status.h"

#define DVR_CAT ::dvr::log::Cat::present

namespace dvr::desktop_eye {
namespace {

IDirect3DDevice9*  g_dev = nullptr;      // identity only, never AddRef'd
IDirect3DSurface9* g_held = nullptr;     // the pinned eye, DEFAULT pool
UINT     g_w = 0, g_h = 0;
D3DFORMAT g_fmt = D3DFMT_UNKNOWN;
bool     g_on = true;                    // [VR] DesktopEye
uint32_t g_snaps = 0, g_blits = 0, g_failSnap = 0, g_failBlit = 0, g_recreate = 0;
uint32_t g_beatSnaps = 0, g_beatBlits = 0;
bool     g_refused = false;              // a hard refusal: stop trying, say why

// The surface must match the backbuffer exactly or StretchRect refuses.
bool ensure_surface(IDirect3DSurface9* bb) {
    D3DSURFACE_DESC d;
    if (FAILED(bb->GetDesc(&d))) return false;
    if (g_held && (d.Width != g_w || d.Height != g_h || d.Format != g_fmt)) {
        g_held->Release();
        g_held = nullptr;
        ++g_recreate;
        DVR_INFO("desktopeye: backbuffer changed %ux%u -> %ux%u (fmt %d -> %d), "
                 "surface recreated", g_w, g_h, d.Width, d.Height, (int)g_fmt, (int)d.Format);
    }
    if (g_held) return true;
    // Lockable=FALSE: nothing reads this on the CPU, and a lockable render
    // target is slower and more restricted.
    const HRESULT hr = g_dev->CreateRenderTarget(d.Width, d.Height, d.Format,
                                                 D3DMULTISAMPLE_NONE, 0, FALSE,
                                                 &g_held, nullptr);
    if (FAILED(hr) || !g_held) {
        g_held = nullptr;
        g_refused = true;
        DVR_WARN("desktopeye: REFUSED - CreateRenderTarget(%ux%u fmt %d) -> 0x%08lx. "
                 "The desktop keeps alternating eyes; the headset is unaffected. "
                 "[VR] DesktopEye=0 silences this.",
                 d.Width, d.Height, (int)d.Format, (unsigned long)hr);
        return false;
    }
    g_w = d.Width; g_h = d.Height; g_fmt = d.Format;
    DVR_INFO("desktopeye: pinning the game window to ONE eye - %ux%u fmt %d. The "
             "window showed L,R,L,R because every eye draw reaches the game's own "
             "Present; the headset's pairs are untouched by this.",
             g_w, g_h, (int)g_fmt);
    return true;
}

}  // namespace

void set_device(IDirect3DDevice9* dev) {
    if (dev == g_dev) return;
    if (g_held) { g_held->Release(); g_held = nullptr; }
    g_dev = dev;
    g_w = g_h = 0;
    g_fmt = D3DFMT_UNKNOWN;
    g_refused = false;
}

void on_present(int eyeSign) {
    if (!g_on || g_refused || !g_dev || eyeSign == 0) return;
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(g_dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return;
    if (ensure_surface(bb)) {
        // StretchRect between two DEFAULT-pool surfaces of identical size and
        // format is the supported combination; no filtering is wanted or needed.
        if (eyeSign < 0) {
            if (SUCCEEDED(g_dev->StretchRect(bb, nullptr, g_held, nullptr, D3DTEXF_NONE))) {
                ++g_snaps; ++g_beatSnaps;
            } else {
                ++g_failSnap;
            }
        } else {
            // Only ever put back an eye we actually captured, so the first
            // right-eye present of a session does not blit an empty surface.
            if (g_snaps == 0) {
                ++g_failBlit;
            } else if (SUCCEEDED(g_dev->StretchRect(g_held, nullptr, bb, nullptr, D3DTEXF_NONE))) {
                ++g_blits; ++g_beatBlits;
            } else {
                ++g_failBlit;
            }
        }
    }
    bb->Release();
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 15000,
        "desktopeye: %u snapshot(s) and %u re-blit(s) in the last window "
        "(lifetime %u/%u, %u failed snapshot(s), %u failed blit(s), %u surface "
        "rebuild(s)). Healthy is the two window counts EQUAL and non-zero - one "
        "left eye held, one right eye overwritten, per pair. Blits far below "
        "snapshots means right-eye presents are not reaching the pin and the "
        "window will still alternate.",
        g_beatSnaps, g_beatBlits, g_snaps, g_blits, g_failSnap, g_failBlit,
        g_recreate);
    if (g_beatSnaps > 100000u) { g_beatSnaps = 0; g_beatBlits = 0; }
}

void on_reset() {
    if (g_held) { g_held->Release(); g_held = nullptr; }
    g_w = g_h = 0;
    g_fmt = D3DFMT_UNKNOWN;
    g_refused = false;
}

void shutdown() { on_reset(); g_dev = nullptr; }

void set_enabled(bool on) {
    if (on == g_on) return;
    g_on = on;
    if (!on) on_reset();
    DVR_INFO("desktopeye: %s - the game window %s", on ? "ON" : "off",
             on ? "shows one eye" : "shows whichever eye each Present carried "
                  "(it will alternate under a sequential stereo method)");
}
bool enabled() { return g_on; }

void status(dvr::status::Writer& w) {
    w.kv("desktopEye", g_on);
    w.kv("desktopEyeSnaps", (unsigned long)g_snaps);
    w.kv("desktopEyeBlits", (unsigned long)g_blits);
    w.kv("desktopEyeFailed", (unsigned long)(g_failSnap + g_failBlit));
}

void log_status() {
    DVR_INFO("desktopeye: %s | %u snapshot(s) %u re-blit(s) | %u failed snapshot(s) "
             "%u failed blit(s) | %ux%u fmt %d | %u rebuild(s)%s",
             g_on ? "ON" : "off", g_snaps, g_blits, g_failSnap, g_failBlit,
             g_w, g_h, (int)g_fmt, g_recreate,
             g_refused ? " | REFUSED, see the warning above" : "");
}

}  // namespace dvr::desktop_eye
