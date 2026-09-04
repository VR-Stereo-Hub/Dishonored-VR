// core/gfx/hud_capture.cpp - see hud_capture.h.
#define DVR_CAT ::dvr::log::Cat::hud
#include "core/gfx/hud_capture.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/util/log.h"
#include "core/vr/hud_stub.h"
#include "core/vr/openxr_runtime.h"
#include "game/dishonored/patterns.h"

#include <d3d11.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::hudcap {
namespace {

// ---- the lever ------------------------------------------------------------
bool  g_wanted = false;
bool  g_on = false;
float g_slotScale = 0.5f;
bool  g_gameGate = false;
bool  g_armed = false;          // the per-present verdict the draw path reads
bool  g_refusedSaid = false;

// ---- the private D3D9 target ----------------------------------------------
IDirect3DSurface9* g_hudRt = nullptr;
uint32_t           g_rtW = 0, g_rtH = 0;
bool               g_rtFailed = false;
uint32_t           g_redirected = 0;      // draws redirected this present
bool               g_inRedirect = false;  // re-entrancy guard

// ---- the shared hand-off (the eye path's shape, at the panel's size) -------
// Two slots so the D3D9 blit never lands under a D3D11 read, and a fence in
// BOTH directions: the eye path paid for that lesson in session 8.
IDirect3DSurface9*  g_slotRt[2] = {};
IDirect3DQuery9*    g_blitFence[2] = {};
ID3D11Texture2D*    g_slotTex[2] = {};
ID3D11ShaderResourceView* g_slotSrv[2] = {};
ID3D11Query*        g_readFence[2] = {};
bool                g_blitIssued[2] = {}, g_readIssued[2] = {}, g_slotValid[2] = {};
int                 g_cur = 0, g_delivered = -1;
uint32_t            g_slotW = 0, g_slotH = 0;

ID3D11Texture2D*        g_outTex = nullptr;
ID3D11RenderTargetView* g_outRtv = nullptr;
ID3D11DeviceContext*    g_lastCtx = nullptr;
dvr::gfx::BlitQuad      g_blit;
bool                    g_deliveredThisPresent = false;

// ---- counters -------------------------------------------------------------
uint32_t g_winPresents = 0, g_winRedirected = 0, g_winDelivered = 0;
uint32_t g_blitWaits = 0, g_blitTimeouts = 0, g_readWaits = 0, g_readTimeouts = 0;
uint32_t g_restoreFails = 0;
unsigned long g_winStartMs = 0;

const char* g_offReason = "the lever is off";

void release_slots() {
    if (g_lastCtx) {
        ID3D11ShaderResourceView* nul = nullptr;
        g_lastCtx->PSSetShaderResources(0, 1, &nul);
    }
    for (int i = 0; i < 2; ++i) {
        if (g_readFence[i]) { g_readFence[i]->Release(); g_readFence[i] = nullptr; }
        if (g_slotSrv[i]) { g_slotSrv[i]->Release(); g_slotSrv[i] = nullptr; }
        if (g_slotTex[i]) { g_slotTex[i]->Release(); g_slotTex[i] = nullptr; }
        if (g_blitFence[i]) { g_blitFence[i]->Release(); g_blitFence[i] = nullptr; }
        if (g_slotRt[i]) { g_slotRt[i]->Release(); g_slotRt[i] = nullptr; }
        g_blitIssued[i] = g_readIssued[i] = g_slotValid[i] = false;
    }
    g_cur = 0; g_delivered = -1;
    if (g_outRtv) { g_outRtv->Release(); g_outRtv = nullptr; }
    if (g_outTex) { g_outTex->Release(); g_outTex = nullptr; }
    g_slotW = g_slotH = 0;
}

// The clear rule from the archaeology: an element that stops being drawn has to
// be black by the next copy, or it stays on the panel for minutes. ColorFill is
// the cheap way and it is what the proxy used in 38.55, but a driver may refuse
// it on a non-lockable render target, and a clear that fails quietly leaves a
// stale frame on the panel forever - so the refusal is caught, named once, and
// answered by binding the target and clearing it, which a render target must
// support.
void clear_rt(IDirect3DDevice9* dev) {
    if (!dev || !g_hudRt) return;
    static int mode = 0;   // 0 = try ColorFill, 1 = ColorFill works, 2 = bind+Clear
    if (mode != 2) {
        const HRESULT hr = dev->ColorFill(g_hudRt, nullptr, D3DCOLOR_ARGB(0, 0, 0, 0));
        if (SUCCEEDED(hr)) { mode = 1; return; }
        mode = 2;
        DVR_WARN("hud: ColorFill on the panel's target was refused (0x%08lx) - clearing by binding "
                 "it and calling Clear instead. A clear that failed quietly would leave whatever "
                 "was in that memory on the panel for the whole run", (unsigned long)hr);
    }
    IDirect3DSurface9* prev = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &prev))) prev = nullptr;
    if (SUCCEEDED(dvr::frame::orig_set_render_target(dev, 0, g_hudRt))) {
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        if (prev) dvr::frame::orig_set_render_target(dev, 0, prev);
    }
    if (prev) prev->Release();   // released inside the call
}

void release_rt() {
    if (g_hudRt) { g_hudRt->Release(); g_hudRt = nullptr; }
    g_rtW = g_rtH = 0;
}

// The private target, at the backbuffer's size and multisample type so whatever
// depth-stencil is legal for the backbuffer is legal here too. Created from the
// present path, never from inside a draw: CreateRenderTarget can re-enter, and
// the fork needed a busy flag for exactly that.
bool ensure_rt(IDirect3DDevice9* dev) {
    if (g_hudRt || g_rtFailed || !dev) return g_hudRt != nullptr;
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    D3DSURFACE_DESC d = {};
    const HRESULT dh = bb->GetDesc(&d);
    bb->Release();                       // released inside the call, always
    if (FAILED(dh) || !d.Width || !d.Height) return false;
    const HRESULT hr = dev->CreateRenderTarget(d.Width, d.Height, D3DFMT_A8R8G8B8,
                                               d.MultiSampleType, d.MultiSampleQuality,
                                               FALSE, &g_hudRt, nullptr);
    if (FAILED(hr) || !g_hudRt) {
        g_rtFailed = true;
        g_hudRt = nullptr;
        DVR_ERROR("hud: the panel's render target %ux%u A8R8G8B8 (ms=%d/%u) was refused (0x%08lx) - "
                  "the panel is off for this run and the HUD stays in the frame",
                  d.Width, d.Height, (int)d.MultiSampleType, d.MultiSampleQuality,
                  (unsigned long)hr);
        return false;
    }
    g_rtW = d.Width; g_rtH = d.Height;
    clear_rt(dev);
    DVR_INFO("hud: the panel's render target is %ux%u A8R8G8B8 (the backbuffer's size and "
             "multisample type, so its depth-stencil stays legal)", g_rtW, g_rtH);
    return true;
}

bool ensure_slots(IDirect3DDevice9* dev, ID3D11Device* dev11) {
    uint32_t w = (uint32_t)(g_rtW * g_slotScale + 0.5f);
    uint32_t h = (uint32_t)(g_rtH * g_slotScale + 0.5f);
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    if (g_slotRt[0] && g_slotRt[1] && g_outTex && w == g_slotW && h == g_slotH) return true;
    release_slots();
    for (int i = 0; i < 2; ++i) {
        HANDLE shared = nullptr;
        HRESULT hr = dev->CreateRenderTarget(w, h, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &g_slotRt[i], &shared);
        if (FAILED(hr) || !g_slotRt[i] || !shared) {
            DVR_ERROR("hud: shared slot %d %ux%u refused (0x%08lx) - %s", i, w, h, (unsigned long)hr,
                      hr == D3DERR_INVALIDCALL ? "the device does not share ([Device] Ex=1?)"
                                               : "out of memory or an unshareable format");
            release_slots();
            return false;
        }
        if (FAILED(dev11->OpenSharedResource(shared, __uuidof(ID3D11Texture2D), (void**)&g_slotTex[i])) ||
            !g_slotTex[i] ||
            FAILED(dev11->CreateShaderResourceView(g_slotTex[i], nullptr, &g_slotSrv[i]))) {
            DVR_ERROR("hud: D3D11 could not open shared slot %d (another adapter?)", i);
            release_slots();
            return false;
        }
        if (FAILED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &g_blitFence[i])) || !g_blitFence[i]) {
            DVR_ERROR("hud: the blit fence for slot %d was refused - no fence, no panel", i);
            release_slots();
            return false;
        }
        D3D11_QUERY_DESC qd = {};
        qd.Query = D3D11_QUERY_EVENT;
        if (FAILED(dev11->CreateQuery(&qd, &g_readFence[i])) || !g_readFence[i]) {
            DVR_ERROR("hud: the read fence for slot %d was refused - no fence, no panel", i);
            release_slots();
            return false;
        }
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // the runtime swapchain's family
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev11->CreateTexture2D(&td, nullptr, &g_outTex)) || !g_outTex ||
        FAILED(dev11->CreateRenderTargetView(g_outTex, nullptr, &g_outRtv))) {
        DVR_ERROR("hud: the panel texture %ux%u R8G8B8A8 was refused", w, h);
        release_slots();
        return false;
    }
    g_slotW = w; g_slotH = h;
    DVR_INFO("hud: the panel's hand-off is live: %ux%u shared (2 slots, fenced both ways) -> "
             "%ux%u R8G8B8A8 on the runtime's device, scale %.2f of the %ux%u frame",
             w, h, w, h, g_slotScale, g_rtW, g_rtH);
    return true;
}

// GetTickCount cannot measure a 10 ms budget (its tick is about 15 ms), so a
// budget written that way expires on the first read and the fence is not really
// waited on. These use the performance counter, like the eye path's.
long long qpc_now() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
long long qpc_freq() {
    static long long f = 0;
    if (!f) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = q.QuadPart ? q.QuadPart : 1; }
    return f;
}
bool past_us(long long t0, long long us) { return (qpc_now() - t0) * 1000000 / qpc_freq() > us; }

void blit_wait(int i) {
    if (!g_blitIssued[i] || !g_blitFence[i]) return;
    HRESULT hr = g_blitFence[i]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
        ++g_blitWaits;
        const long long t0 = qpc_now();
        while (hr == S_FALSE && !past_us(t0, 10000)) {
            Sleep(0);
            hr = g_blitFence[i]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
        }
        if (hr == S_FALSE) ++g_blitTimeouts;
    }
    g_blitIssued[i] = false;
}

void read_wait(int i) {
    if (!g_readIssued[i] || !g_readFence[i] || !g_lastCtx) return;
    HRESULT hr = g_lastCtx->GetData(g_readFence[i], nullptr, 0, 0);
    if (hr == S_FALSE) {
        ++g_readWaits;
        const long long t0 = qpc_now();
        while (hr == S_FALSE && !past_us(t0, 10000)) {
            Sleep(0);
            hr = g_lastCtx->GetData(g_readFence[i], nullptr, 0, 0);
        }
        if (hr == S_FALSE) ++g_readTimeouts;
    }
    g_readIssued[i] = false;
}

ID3D11Texture2D* the_provider(ID3D11DeviceContext* ctx) { return provider_texture(ctx); }

void apply_wanted(const char* why) {
    if (g_wanted == g_on) return;
    if (g_wanted) {
        if (!kHudFingerprintMeasured) {
            DVR_WARN("hud: the panel is REFUSED (%s) - patterns.h has no measured HUD fingerprint "
                     "(kHudFingerprintMeasured=0). Run `draws on` in gameplay and read the VERDICT "
                     "line; a guessed rule puts world geometry on the panel or holes in the world",
                     why);
            g_wanted = false;
            return;
        }
        dvr::vr::set_hud_texture_provider(the_provider);
    }
    g_on = g_wanted;
    DVR_INFO("hud: the panel is %s (%s)%s", g_on ? "ON" : "off", why,
             g_on ? " - the HUD leaves the frame and the eye textures, and appears on the "
                    "head-locked quad. The desktop window loses it too, by construction"
                  : " - the HUD is back in the frame from the next draw");
    if (!g_on) g_deliveredThisPresent = false;
}

} // namespace

// ---------------------------------------------------------------------------

bool enabled() { return g_on; }
void set_enabled(bool on) { g_wanted = on; apply_wanted("asked"); }

void set_slot_scale(float s) {
    if (s < 0.1f) s = 0.1f;
    if (s > 1.0f) s = 1.0f;
    if (s == g_slotScale) return;
    g_slotScale = s;
    DVR_INFO("hud: panel scale %.2f - the slots rebuild on the next present (one stall)", s);
}
float slot_scale() { return g_slotScale; }

void set_game_gate(bool on) { g_gameGate = on; }

bool armed() { return g_armed; }

bool begin(IDirect3DDevice9* dev, const D3DVIEWPORT9& vp) {
    if (!g_armed || g_inRedirect || !g_hudRt || !dev) return false;
    if (FAILED(dvr::frame::orig_set_render_target(dev, 0, g_hudRt))) return false;
    // SetRenderTarget resets the viewport to the whole target; the game's own
    // viewport has to go back. The device is PURE, so there is no GetViewport
    // to read it from - it comes from the classifier's shadow.
    dev->SetViewport(&vp);
    g_inRedirect = true;
    ++g_redirected;
    return true;
}

void end(IDirect3DDevice9* dev, IDirect3DSurface9* gameRt, const D3DVIEWPORT9& vp) {
    if (!g_inRedirect || !dev) return;
    g_inRedirect = false;
    HRESULT hr;
    if (gameRt) {
        hr = dvr::frame::orig_set_render_target(dev, 0, gameRt);
    } else {
        // A null shadow means the implicit backbuffer (nothing has set a target
        // since the device came up). Ask for it, use it, release it here.
        IDirect3DSurface9* bb = nullptr;
        hr = dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb);
        if (SUCCEEDED(hr) && bb) {
            hr = dvr::frame::orig_set_render_target(dev, 0, bb);
            bb->Release();
        }
    }
    if (FAILED(hr)) {
        ++g_restoreFails;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 3,
                        "hud: could not put the game's render target back (0x%08lx) - the panel "
                        "disarms for this run rather than draw the game into its own target",
                        (unsigned long)hr);
        g_wanted = false;
        apply_wanted("a restore failed");
    }
    dev->SetViewport(&vp);
}

void end_frame(IDirect3DDevice9* dev9, ID3D11Device* dev11, ID3D11DeviceContext* ctx11) {
    // The gate, recomputed for the NEXT present's draws. Both halves must hold:
    // the runtime's own (a projection present carrying an eye tag, which menus,
    // loading screens and the cinematic quad all drop) and the game side's
    // strict-gameplay verdict.
    const bool xrGate = dvr::hud::gate();
    const bool wantArm = g_on && xrGate && g_gameGate;

    if (dev9 && g_on) ensure_rt(dev9);

    if (!g_winStartMs) g_winStartMs = GetTickCount();
    ++g_winPresents;
    g_winRedirected += g_redirected;
    g_deliveredThisPresent = false;

    if (g_on && g_hudRt && dev9 && dev11 && ctx11) {
        g_lastCtx = ctx11;
        if (ensure_slots(dev9, dev11) && g_blit.init(dev11)) {
            {
                read_wait(g_cur);
                RECT src = {0, 0, (LONG)g_rtW, (LONG)g_rtH};
                const HRESULT sr = dev9->StretchRect(g_hudRt, &src, g_slotRt[g_cur], nullptr,
                                                     D3DTEXF_LINEAR);
                if (SUCCEEDED(sr)) {
                    if (g_blitFence[g_cur]) { g_blitFence[g_cur]->Issue(D3DISSUE_END); g_blitIssued[g_cur] = true; }
                    g_slotValid[g_cur] = true;
                } else {
                    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 3,
                                    "hud: the copy of the panel's target into slot %d was refused "
                                    "(0x%08lx) - the panel would show whatever the slot last held",
                                    g_cur, (unsigned long)sr);
                }
                // Unconditionally, every present: a present with NO HUD draw is
                // exactly the case the fork got wrong (a dropped body's icon sat
                // on the wrist for minutes because nothing drew to trigger its
                // lazy clear).
                clear_rt(dev9);
            }
            // Deliver the OTHER slot: a whole present has passed since its blit,
            // so its fence is almost always already signalled.
            const int other = g_cur ^ 1;
            if (g_slotValid[other]) {
                blit_wait(other);
                g_blit.draw(ctx11, g_slotSrv[other], g_outRtv, g_slotW, g_slotH, true);
                if (g_readFence[other]) {
                    ctx11->End(g_readFence[other]);
                    ctx11->Flush();   // an event query does not complete until the work is submitted
                    g_readIssued[other] = true;
                }
                g_delivered = other;
                g_deliveredThisPresent = true;
                ++g_winDelivered;
            }
            g_cur ^= 1;
        }
    } else if (g_hudRt && dev9 && !g_on) {
        dev9->ColorFill(g_hudRt, nullptr, D3DCOLOR_ARGB(0, 0, 0, 0));
    }
    g_redirected = 0;
    g_armed = wantArm;

    // One line every 3 s while the lever is on, naming the owner of every term.
    if (g_on && GetTickCount() - g_winStartMs >= 3000) {
        DVR_INFO("hud: 3s: presents=%u redirected=%.1f draws/present delivered=%u | slot %ux%u of "
                 "%ux%u (scale %.2f) | fences: blit waits %u timeouts %u, read waits %u timeouts "
                 "%u | restore failures %u | gate: panel=%d xr=%d game=%d -> %s",
                 g_winPresents, g_winPresents ? (double)g_winRedirected / g_winPresents : 0.0,
                 g_winDelivered, g_slotW, g_slotH, g_rtW, g_rtH, g_slotScale,
                 g_blitWaits, g_blitTimeouts, g_readWaits, g_readTimeouts, g_restoreFails,
                 (int)g_on, (int)xrGate, (int)g_gameGate, wantArm ? "ARMED" : "idle");
        if (!wantArm) {
            g_offReason = !xrGate ? "the runtime's gate is down (no projection present with an eye "
                                    "tag: a menu, a loading screen, the cinematic quad, or the mono "
                                    "screen - the HUD stays in the frame there by design)"
                                  : "the game side is not in strict gameplay (a menu, no pawn, a "
                                    "cinematic, or the power wheel is held)";
            DVR_INFO("hud: not redirecting - %s", g_offReason);
        } else {
            g_offReason = "armed";
        }
        g_winStartMs = GetTickCount();
        g_winPresents = g_winRedirected = g_winDelivered = 0;
    }
}

ID3D11Texture2D* panel_texture() { return g_outTex; }

ID3D11Texture2D* provider_texture(ID3D11DeviceContext*) {
    if (!g_on || !g_deliveredThisPresent) return nullptr;
    return g_outTex;
}

void on_reset() {
    release_slots();
    release_rt();
    g_rtFailed = false;
    g_inRedirect = false;
    g_redirected = 0;
    g_armed = false;
}

void shutdown() {
    g_armed = false;
    g_on = false;
    g_wanted = false;
    g_blit.shutdown();
    release_slots();
    release_rt();
}

void log_status() {
    DVR_INFO("hud: panel=%s scale=%.2f rt=%ux%u slot=%ux%u | gate: xr=%d game=%d -> %s | "
             "fingerprint measured=%d | %s",
             g_on ? "on" : "off", g_slotScale, g_rtW, g_rtH, g_slotW, g_slotH,
             (int)dvr::hud::gate(), (int)g_gameGate, g_armed ? "ARMED" : "idle",
             (int)kHudFingerprintMeasured, g_offReason);
}

void status(dvr::status::Writer& w) {
    w.kv("on", g_on);
    w.kv("armed", g_armed);
    w.kv("xrGate", dvr::hud::gate());
    w.kv("gameGate", g_gameGate);
    w.kv("scale", (double)g_slotScale);
    w.kv("slotW", (int)g_slotW);
    w.kv("slotH", (int)g_slotH);
    w.kv("blitTimeouts", (unsigned long)g_blitTimeouts);
    w.kv("readTimeouts", (unsigned long)g_readTimeouts);
    w.kv("restoreFails", (unsigned long)g_restoreFails);
    w.kv("reason", g_offReason);
}

bool command(const char* args) {
    if (!strcmp(args, "on"))  { set_enabled(true);  return true; }
    if (!strcmp(args, "off")) { set_enabled(false); return true; }
    if (!strncmp(args, "scale", 5)) {
        const char* a = args + 5;
        while (*a == ' ') ++a;
        const double v = atof(a);
        if (v <= 0.0) { DVR_WARN("hud: scale wants a fraction, e.g. `hud scale 0.5`"); return true; }
        set_slot_scale((float)v);
        return true;
    }
    log_status();
    return true;
}

} // namespace dvr::hudcap
