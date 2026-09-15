// core/gfx/hud_capture.cpp - see hud_capture.h.
#define DVR_CAT ::dvr::log::Cat::hud
#include "core/gfx/hud_capture.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/gfx/hud_layout.h"
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
bool  g_menuOverride = false;
bool  g_armed = false;          // the per-present verdict the draw path reads
bool  g_handoffReady = false;   // at least one sink's slots + the blit up
bool  g_failed = false;         // a D3D failure latched this session

// ---- a sink: one private target and its hand-off ---------------------------
struct Sink {
    IDirect3DSurface9*  rt = nullptr;
    IDirect3DSurface9*  slotRt[2] = {};
    IDirect3DQuery9*    blitFence[2] = {};
    ID3D11Texture2D*    slotTex[2] = {};
    ID3D11ShaderResourceView* slotSrv[2] = {};
    ID3D11Query*        readFence[2] = {};
    bool blitIssued[2] = {}, readIssued[2] = {}, slotValid[2] = {};
    int  cur = 0;
    ID3D11Texture2D*        outTex = nullptr;
    ID3D11RenderTargetView* outRtv = nullptr;
    uint32_t slotW = 0, slotH = 0;
    bool     ready = false;          // slots + out texture are up at the current size
    bool     delivered = false;      // this present
    uint32_t redirected = 0;         // draws this present
    uint32_t winRedirected = 0, winDelivered = 0, winEmpty = 0;
};
Sink g_sink[dvr::hudlayout::kMaxSinks];
uint32_t g_rtW = 0, g_rtH = 0;      // the private targets' size (= the backbuffer's)
bool     g_rtFailed = false;
int      g_inRedirect = -1;         // the sink bound right now, -1 = none

ID3D11DeviceContext* g_lastCtx = nullptr;
dvr::gfx::BlitQuad   g_blit;

// ---- counters -------------------------------------------------------------
uint32_t g_winPresents = 0, g_winArmedPresents = 0, g_winEmptyArmed = 0;
uint32_t g_winEmptyEven = 0, g_winEmptyOdd = 0;
uint32_t g_blitWaits = 0, g_blitTimeouts = 0, g_readWaits = 0, g_readTimeouts = 0;
uint32_t g_restoreFails = 0;
uint32_t g_presentNo = 0;
unsigned long g_winStartMs = 0;
unsigned long g_lastRedirectMs = 0;
const char* g_offReason = "the lever is off";

long long qpc_now() { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
long long qpc_freq() {
    static long long f = 0;
    if (!f) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = q.QuadPart ? q.QuadPart : 1; }
    return f;
}
bool past_us(long long t0, long long us) { return (qpc_now() - t0) * 1000000 / qpc_freq() > us; }

void release_slots(Sink& s) {
    if (g_lastCtx) {
        ID3D11ShaderResourceView* nul = nullptr;
        g_lastCtx->PSSetShaderResources(0, 1, &nul);
    }
    for (int i = 0; i < 2; ++i) {
        if (s.readFence[i]) { s.readFence[i]->Release(); s.readFence[i] = nullptr; }
        if (s.slotSrv[i]) { s.slotSrv[i]->Release(); s.slotSrv[i] = nullptr; }
        if (s.slotTex[i]) { s.slotTex[i]->Release(); s.slotTex[i] = nullptr; }
        if (s.blitFence[i]) { s.blitFence[i]->Release(); s.blitFence[i] = nullptr; }
        if (s.slotRt[i]) { s.slotRt[i]->Release(); s.slotRt[i] = nullptr; }
        s.blitIssued[i] = s.readIssued[i] = s.slotValid[i] = false;
    }
    s.cur = 0;
    if (s.outRtv) { s.outRtv->Release(); s.outRtv = nullptr; }
    if (s.outTex) { s.outTex->Release(); s.outTex = nullptr; }
    s.slotW = s.slotH = 0;
    s.ready = false;
    s.delivered = false;
}

void release_rt(Sink& s) {
    if (s.rt) { s.rt->Release(); s.rt = nullptr; }
}

// The clear rule from the archaeology: an element that stops being drawn has to
// be black by the next copy, or it stays on the panel for minutes. ColorFill is
// the cheap way (38.55); a driver may refuse it on a non-lockable target, and a
// clear that fails quietly leaves a stale frame forever, so the refusal is
// caught, named once, and answered by binding the target and clearing it.
void clear_rt(IDirect3DDevice9* dev, Sink& s) {
    if (!dev || !s.rt) return;
    static int mode = 0;   // 0 = try ColorFill, 1 = ColorFill works, 2 = bind+Clear
    if (mode != 2) {
        const HRESULT hr = dev->ColorFill(s.rt, nullptr, D3DCOLOR_ARGB(0, 0, 0, 0));
        if (SUCCEEDED(hr)) { mode = 1; return; }
        mode = 2;
        DVR_WARN("hud: ColorFill on a sink target was refused (0x%08lx) - clearing by binding it and "
                 "calling Clear instead. A clear that failed quietly would leave whatever was in that "
                 "memory on the panel for the whole run", (unsigned long)hr);
    }
    IDirect3DSurface9* prev = nullptr;
    if (FAILED(dev->GetRenderTarget(0, &prev))) prev = nullptr;
    if (SUCCEEDED(dvr::frame::orig_set_render_target(dev, 0, s.rt))) {
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
        if (prev) dvr::frame::orig_set_render_target(dev, 0, prev);
    }
    if (prev) prev->Release();   // released inside the call
}

// The private target, at the backbuffer's size AND multisample type so the
// depth-stencil that is legal for the backbuffer is legal here. Created from
// the present path, never from inside a draw (CreateRenderTarget can re-enter).
bool ensure_rt(IDirect3DDevice9* dev, int i) {
    Sink& s = g_sink[i];
    if (s.rt || g_rtFailed || !dev) return s.rt != nullptr;
    IDirect3DSurface9* bb = nullptr;
    if (FAILED(dev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)) || !bb) return false;
    D3DSURFACE_DESC d = {};
    const HRESULT dh = bb->GetDesc(&d);
    bb->Release();                       // released inside the call, always
    if (FAILED(dh) || !d.Width || !d.Height) return false;
    const HRESULT hr = dev->CreateRenderTarget(d.Width, d.Height, D3DFMT_A8R8G8B8,
                                               d.MultiSampleType, d.MultiSampleQuality,
                                               FALSE, &s.rt, nullptr);
    if (FAILED(hr) || !s.rt) {
        g_rtFailed = true; g_failed = true;
        s.rt = nullptr;
        DVR_ERROR("hud: sink %d's render target %ux%u A8R8G8B8 (ms=%d/%u) was refused (0x%08lx) - the "
                  "redirect is off for this run and the HUD stays in the frame",
                  i, d.Width, d.Height, (int)d.MultiSampleType, d.MultiSampleQuality, (unsigned long)hr);
        return false;
    }
    g_rtW = d.Width; g_rtH = d.Height;
    clear_rt(dev, s);
    DVR_INFO("hud: sink %d's target is %ux%u A8R8G8B8 (the backbuffer's size and multisample type, "
             "so its depth-stencil stays legal)", i, g_rtW, g_rtH);
    return true;
}

bool ensure_slots(IDirect3DDevice9* dev, ID3D11Device* dev11, int i) {
    Sink& s = g_sink[i];
    uint32_t w = (uint32_t)(g_rtW * g_slotScale + 0.5f);
    uint32_t h = (uint32_t)(g_rtH * g_slotScale + 0.5f);
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    if (s.ready && w == s.slotW && h == s.slotH) return true;
    release_slots(s);
    for (int k = 0; k < 2; ++k) {
        HANDLE shared = nullptr;
        HRESULT hr = dev->CreateRenderTarget(w, h, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &s.slotRt[k], &shared);
        if (FAILED(hr) || !s.slotRt[k] || !shared) {
            DVR_ERROR("hud: sink %d shared slot %d %ux%u refused (0x%08lx) - %s", i, k, w, h, (unsigned long)hr,
                      hr == D3DERR_INVALIDCALL ? "the device does not share ([Device] Ex=1?)"
                                               : "out of memory or an unshareable format");
            release_slots(s); g_failed = true;
            return false;
        }
        if (FAILED(dev11->OpenSharedResource(shared, __uuidof(ID3D11Texture2D), (void**)&s.slotTex[k])) ||
            !s.slotTex[k] ||
            FAILED(dev11->CreateShaderResourceView(s.slotTex[k], nullptr, &s.slotSrv[k]))) {
            DVR_ERROR("hud: D3D11 could not open sink %d's shared slot %d (another adapter?)", i, k);
            release_slots(s); g_failed = true;
            return false;
        }
        if (FAILED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &s.blitFence[k])) || !s.blitFence[k]) {
            DVR_ERROR("hud: the blit fence for sink %d slot %d was refused - no fence, no panel", i, k);
            release_slots(s); g_failed = true;
            return false;
        }
        D3D11_QUERY_DESC qd = {};
        qd.Query = D3D11_QUERY_EVENT;
        if (FAILED(dev11->CreateQuery(&qd, &s.readFence[k])) || !s.readFence[k]) {
            DVR_ERROR("hud: the read fence for sink %d slot %d was refused - no fence, no panel", i, k);
            release_slots(s); g_failed = true;
            return false;
        }
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // the runtime swapchain's family (CopyResource)
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev11->CreateTexture2D(&td, nullptr, &s.outTex)) || !s.outTex ||
        FAILED(dev11->CreateRenderTargetView(s.outTex, nullptr, &s.outRtv))) {
        DVR_ERROR("hud: sink %d's texture %ux%u R8G8B8A8 was refused", i, w, h);
        release_slots(s); g_failed = true;
        return false;
    }
    s.slotW = w; s.slotH = h;
    s.ready = true;
    DVR_INFO("hud: sink %d's hand-off is live: %ux%u shared (2 slots, fenced both ways) -> %ux%u "
             "R8G8B8A8 on the runtime's device, scale %.2f of the %ux%u frame (%.1f MB copied per present)",
             i, w, h, w, h, g_slotScale, g_rtW, g_rtH, (double)w * h * 4 / 1048576.0);
    return true;
}

void blit_wait(Sink& s, int k) {
    if (!s.blitIssued[k] || !s.blitFence[k]) return;
    HRESULT hr = s.blitFence[k]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
    if (hr == S_FALSE) {
        ++g_blitWaits;
        const long long t0 = qpc_now();
        while (hr == S_FALSE && !past_us(t0, 10000)) {
            Sleep(0);
            hr = s.blitFence[k]->GetData(nullptr, 0, D3DGETDATA_FLUSH);
        }
        if (hr == S_FALSE) ++g_blitTimeouts;
    }
    s.blitIssued[k] = false;
}

void read_wait(Sink& s, int k) {
    if (!s.readIssued[k] || !s.readFence[k] || !g_lastCtx) return;
    HRESULT hr = g_lastCtx->GetData(s.readFence[k], nullptr, 0, 0);
    if (hr == S_FALSE) {
        ++g_readWaits;
        const long long t0 = qpc_now();
        while (hr == S_FALSE && !past_us(t0, 10000)) {
            Sleep(0);
            hr = g_lastCtx->GetData(s.readFence[k], nullptr, 0, 0);
        }
        if (hr == S_FALSE) ++g_readTimeouts;
    }
    s.readIssued[k] = false;
}

void apply_wanted(const char* why) {
    if (g_wanted == g_on) return;
    if (g_wanted && !kHudFingerprintMeasured) {
        DVR_WARN("hud: the redirect is REFUSED (%s) - patterns.h has no measured HUD fingerprint "
                 "(kHudFingerprintMeasured=0). Run `draws on` in gameplay and read the VERDICT line; a "
                 "guessed rule puts world geometry on the panel or holes in the world", why);
        g_wanted = false;
        return;
    }
    g_on = g_wanted;
    DVR_INFO("hud: the redirect is %s (%s)%s", g_on ? "ON" : "off", why,
             g_on ? " - the HUD leaves the frame and the eye textures and appears on its anchors "
                    "(the window, the hand). The desktop window loses it too, by construction"
                  : " - the HUD is back in the frame from the next draw");
    if (!g_on) for (Sink& s : g_sink) s.delivered = false;
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
    DVR_INFO("hud: slot scale %.2f - the slots rebuild on the next present (one stall)", s);
}
float slot_scale() { return g_slotScale; }

void set_game_gate(bool arm, bool menuOverride) { g_gameGate = arm; g_menuOverride = menuOverride; }
bool armed() { return g_armed; }

bool begin(IDirect3DDevice9* dev, const D3DVIEWPORT9& vp, int sink) {
    if (!g_armed || g_inRedirect >= 0 || !dev) return false;
    if (sink < 0 || sink >= dvr::hudlayout::kMaxSinks) return false;
    Sink& s = g_sink[sink];
    if (!s.rt) return false;
    if (FAILED(dvr::frame::orig_set_render_target(dev, 0, s.rt))) return false;
    // SetRenderTarget resets the viewport to the whole target; the game's own
    // viewport goes back. The device is PURE, so it comes from the shadow.
    dev->SetViewport(&vp);
    g_inRedirect = sink;
    ++s.redirected;
    return true;
}

void end(IDirect3DDevice9* dev, IDirect3DSurface9* gameRt, const D3DVIEWPORT9& vp) {
    if (g_inRedirect < 0 || !dev) return;
    g_inRedirect = -1;
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
        g_failed = true;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 3,
                        "hud: could not put the game's render target back (0x%08lx) - the redirect "
                        "disarms for this run rather than draw the game into its own target",
                        (unsigned long)hr);
        g_wanted = false;
        apply_wanted("a restore failed");
    }
    dev->SetViewport(&vp);
}

void end_frame(IDirect3DDevice9* dev9, ID3D11Device* dev11, ID3D11DeviceContext* ctx11) {
    // The gate, recomputed for the NEXT present's draws. Both halves must hold:
    // the runtime's own presentation MODE (a projection layer is up; the mono
    // screen, a loading screen and the cinematic quad all drop it) or the menu
    // override, and the game side's scene verdict. NOT the per-present eye tag:
    // re-entry leaves 6 to 21 presents a second untagged by design, and a gate
    // that followed the tag drew the HUD into the frame on each of them - the
    // window/frame flicker of the first headset run.
    const bool xrGate = dvr::hud::projection_mode() || g_menuOverride;
    const bool wantArm = g_on && xrGate && g_gameGate && g_handoffReady && !g_failed;

    if (!g_winStartMs) g_winStartMs = GetTickCount();
    ++g_winPresents;
    ++g_presentNo;
    if (g_armed) {
        ++g_winArmedPresents;
        uint32_t any = 0;
        for (const Sink& s : g_sink) any += s.redirected;
        if (!any) { ++g_winEmptyArmed; if (g_presentNo & 1) ++g_winEmptyOdd; else ++g_winEmptyEven; }
        else g_lastRedirectMs = GetTickCount();
    }

    bool anyReady = false;
    if (g_on && dev9 && dev11 && ctx11 && !g_failed) {
        g_lastCtx = ctx11;
        const bool blitOk = g_blit.init(dev11);
        for (int i = 0; i < dvr::hudlayout::kMaxSinks; ++i) {
            Sink& s = g_sink[i];
            s.delivered = false;
            const bool inUse = dvr::hudlayout::sink_element(i) >= 0;
            if (!inUse) {
                // A sink nobody routes to holds no default-pool memory and
                // costs no copy; a stale target is cleared once and released.
                if (s.rt) { clear_rt(dev9, s); release_rt(s); }
                if (s.ready) release_slots(s);
                s.redirected = 0;
                continue;
            }
            if (!ensure_rt(dev9, i)) { s.redirected = 0; continue; }
            if (!blitOk || !ensure_slots(dev9, dev11, i)) { s.redirected = 0; continue; }
            anyReady = true;
            s.winRedirected += s.redirected;
            if (!s.redirected && g_armed) ++s.winEmpty;
            {
                read_wait(s, s.cur);
                RECT src = {0, 0, (LONG)g_rtW, (LONG)g_rtH};
                const HRESULT sr = dev9->StretchRect(s.rt, &src, s.slotRt[s.cur], nullptr, D3DTEXF_LINEAR);
                if (SUCCEEDED(sr)) {
                    if (s.blitFence[s.cur]) { s.blitFence[s.cur]->Issue(D3DISSUE_END); s.blitIssued[s.cur] = true; }
                    s.slotValid[s.cur] = true;
                } else {
                    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Error, 3,
                                    "hud: the copy of sink %d's target into slot %d was refused (0x%08lx) - "
                                    "the panel would show whatever the slot last held", i, s.cur, (unsigned long)sr);
                }
                // Unconditionally, every present: a present with NO HUD draw is
                // exactly the case the fork got wrong.
                clear_rt(dev9, s);
            }
            // Deliver the OTHER slot: a whole present has passed since its blit.
            const int other = s.cur ^ 1;
            if (s.slotValid[other]) {
                blit_wait(s, other);
                g_blit.draw(ctx11, s.slotSrv[other], s.outRtv, s.slotW, s.slotH, true);
                if (s.readFence[other]) {
                    ctx11->End(s.readFence[other]);
                    ctx11->Flush();   // an event query does not complete until the work is submitted
                    s.readIssued[other] = true;
                }
                s.delivered = true;
                ++s.winDelivered;
            }
            s.cur ^= 1;
            s.redirected = 0;
        }
    } else {
        for (Sink& s : g_sink) {
            s.delivered = false;
            if (s.rt && dev9 && !g_on) clear_rt(dev9, s);
            s.redirected = 0;
        }
    }
    g_handoffReady = anyReady;
    if (g_on && !g_handoffReady)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                         "hud: the redirect is ON but no sink's hand-off to D3D11 is ready, so it stays OFF "
                         "and the HUD keeps drawing into the frame - losing the HUD entirely would be worse "
                         "than not having a panel. The lines above say which step refused (a plain device "
                         "instead of 9Ex, a refused shared surface, or no d3dcompiler for the blit)%s",
                         g_failed ? "; a failure has LATCHED this session (`hud off`, `hud on` retries)" : "");
    g_armed = wantArm;

    // The beat, every 3 s while the lever is on. It states its prediction and
    // can print both unwelcome answers: empty presents while armed are a HUD
    // tail that landed in ONE re-entry pass (all on one parity) or a rule that
    // matched nothing (every present).
    if (g_on && GetTickCount() - g_winStartMs >= 3000) {
        uint32_t redir = 0, deliv = 0;
        char per[160] = "";
        for (int i = 0; i < dvr::hudlayout::kMaxSinks; ++i) {
            const Sink& s = g_sink[i];
            if (dvr::hudlayout::sink_element(i) < 0) continue;
            redir += s.winRedirected; deliv += s.winDelivered;
            char one[40];
            _snprintf(one, sizeof(one), " s%d[%s]=%.1f", i, dvr::hudlayout::element_name(dvr::hudlayout::sink_element(i)),
                      g_winPresents ? (double)s.winRedirected / g_winPresents : 0.0);
            one[39] = 0;
            strncat(per, one, sizeof(per) - strlen(per) - 1);
        }
        DVR_INFO("hud/beat: presents=%u armed=%u redirected=%.1f/present (%s) delivered=%u empty-while-armed=%u "
                 "(even %u, odd %u) | slots %ux%u of %ux%u (scale %.2f) | fences: blit waits %u timeouts %u, "
                 "read waits %u timeouts %u | restore failures %u | gate: on=%d xr=%d menu=%d game=%d handoff=%d "
                 "failed=%d -> %s. Prediction while armed: empty=0; empty==armed/2 all on one parity = the HUD "
                 "tail lands in ONE re-entry pass; empty==armed = the rule matched nothing",
                 g_winPresents, g_winArmedPresents, g_winPresents ? (double)redir / g_winPresents : 0.0,
                 per[0] ? per + 1 : "no sink in use", deliv, g_winEmptyArmed, g_winEmptyEven, g_winEmptyOdd,
                 g_sink[0].slotW, g_sink[0].slotH, g_rtW, g_rtH, g_slotScale,
                 g_blitWaits, g_blitTimeouts, g_readWaits, g_readTimeouts, g_restoreFails,
                 (int)g_on, (int)dvr::hud::projection_mode(), (int)g_menuOverride, (int)g_gameGate, (int)g_handoffReady,
                 (int)g_failed, wantArm ? "ARMED" : "idle");
        if (!wantArm) {
            g_offReason = g_failed ? "a D3D failure latched this session (the lines above name it)"
                        : !g_handoffReady ? "the hand-off to D3D11 is not ready, so the redirect is held off "
                                            "rather than take the HUD away with nowhere to put it"
                        : !xrGate ? "the runtime is not in projection mode (the mono screen, a loading "
                                    "screen, the cinematic quad) and no menu is riding the window - the HUD "
                                    "stays in the frame there by design"
                                  : "the game side's scene verdict is down (no pawn, a load)";
            DVR_INFO("hud: not redirecting - %s", g_offReason);
        } else {
            g_offReason = "armed";
        }
        g_winStartMs = GetTickCount();
        g_winPresents = g_winArmedPresents = g_winEmptyArmed = g_winEmptyEven = g_winEmptyOdd = 0;
        for (Sink& s : g_sink) s.winRedirected = s.winDelivered = s.winEmpty = 0;
        dvr::hudlayout::log_status();
    }
}

ID3D11Texture2D* sink_texture(int sink, ID3D11DeviceContext*) {
    if (!g_on || sink < 0 || sink >= dvr::hudlayout::kMaxSinks) return nullptr;
    const Sink& s = g_sink[sink];
    return s.delivered ? s.outTex : nullptr;
}
ID3D11Texture2D* panel_texture(int sink) {
    if (sink < 0 || sink >= dvr::hudlayout::kMaxSinks) return nullptr;
    return g_sink[sink].outTex;
}

bool redirect_healthy() {
    // The recent-redirect window alone: g_armed is recomputed every present and
    // drops on any untagged present (the ring drains, none/s=1 in the beat), and
    // a health check that blinks with it cancelled the ride's open-gap stand-in
    // on the first pause measured (2026-09-15, the sewers on the simulator).
    if (!g_on || !g_handoffReady || g_failed) return false;
    return (GetTickCount() - g_lastRedirectMs) < 500;
}
bool redirect_failed() { return g_failed; }

void on_reset() {
    g_handoffReady = false;
    for (Sink& s : g_sink) { release_slots(s); release_rt(s); s.redirected = 0; }
    g_rtFailed = false;
    g_inRedirect = -1;
    g_armed = false;
}

void shutdown() {
    g_armed = false;
    g_handoffReady = false;
    g_on = false;
    g_wanted = false;
    g_blit.shutdown();
    for (Sink& s : g_sink) { release_slots(s); release_rt(s); }
}

void log_status() {
    DVR_INFO("hud: redirect=%s scale=%.2f rt=%ux%u slot=%ux%u | gate: projection=%d tag=%d menu=%d game=%d handoff=%d "
             "failed=%d -> %s | fingerprint measured=%d | %s",
             g_on ? "on" : "off", g_slotScale, g_rtW, g_rtH, g_sink[0].slotW, g_sink[0].slotH,
             (int)dvr::hud::projection_mode(), (int)dvr::hud::gate(), (int)g_menuOverride, (int)g_gameGate, (int)g_handoffReady, (int)g_failed,
             g_armed ? "ARMED" : "idle", (int)kHudFingerprintMeasured, g_offReason);
    dvr::hudlayout::log_status();
}

void status(dvr::status::Writer& w) {
    w.kv("on", g_on);
    w.kv("armed", g_armed);
    w.kv("xrGate", dvr::hud::projection_mode());
    w.kv("eyeTag", dvr::hud::gate());
    w.kv("menuOverride", g_menuOverride);
    w.kv("gameGate", g_gameGate);
    w.kv("handoff", g_handoffReady);
    w.kv("failed", g_failed);
    w.kv("healthy", redirect_healthy());
    w.kv("scale", (double)g_slotScale);
    w.kv("slotW", (int)g_sink[0].slotW);
    w.kv("slotH", (int)g_sink[0].slotH);
    w.kv("blitTimeouts", (unsigned long)g_blitTimeouts);
    w.kv("readTimeouts", (unsigned long)g_readTimeouts);
    w.kv("restoreFails", (unsigned long)g_restoreFails);
    w.kv("reason", g_offReason);
    w.obj("layout"); dvr::hudlayout::status(w); w.end_obj();
}

bool command(const char* args) {
    if (!strcmp(args, "on"))  { g_failed = false; set_enabled(true);  return true; }
    if (!strcmp(args, "off")) { set_enabled(false); return true; }
    if (!strncmp(args, "scale", 5)) {
        const char* a = args + 5;
        while (*a == ' ') ++a;
        const double v = atof(a);
        if (v <= 0.0) { DVR_WARN("hud: scale wants a fraction, e.g. `hud scale 0.5`"); return true; }
        set_slot_scale((float)v);
        return true;
    }
    if (!args[0] || !strcmp(args, "status")) { log_status(); return true; }
    if (dvr::hudlayout::command(args)) return true;
    DVR_WARN("hud: unknown `hud %s` - on|off|status|scale <f>|regions on|off|anchor <element> <anchor>|window ...|hand ...|place ...|region ...|menu ...|reset|layout", args);
    return true;
}

} // namespace dvr::hudcap
