// core/gfx/mono_screen.cpp - rung 1 of the stereo ladder: the mono screen.
//
// The game's frame, captured off the D3D9 backbuffer, blitted into one
// R8G8B8A8 texture at the window size with the F10 overlay drawn on top, and
// handed to the runtime layer with eyeSign 0: the runtime shows it on a
// head-locked quad ([Screen] DistanceMeters / WidthMeters), the same image in
// both eyes. No depth, no head-driven render - the camera is the game's own,
// turned by the head-tracking write on the script lane as always.
//
// This is the rung that proves the path: capture -> D3D11 -> swapchain ->
// compositor -> both eyes, with head tracking and the gamepad on top. Every
// higher rung reuses the capture and the blit; only the tag and the camera
// change.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/gfx/capture.h"
#include "core/util/log.h"

#include <windows.h>
#include <d3d11.h>

namespace dvr::stereo {
namespace {

class MonoScreen : public IStereo {
public:
    const char* name() const override { return "mono"; }
    bool implemented() const override { return true; }

    void begin_frame(const FrameInput& in) override { frame_ = in.frame; }
    int eye_for_next_frame() const override { return 0; }

    bool end_frame(const FrameDevices& d, FrameOutput& out) override {
        if (!d.dev9 || !d.dev11 || !d.ctx11) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                         "mono: no D3D11 device - the game runs flat, nothing reaches the headset");
            return false;
        }
        if (!blit_.init(d.dev11)) return false;
        const bool fresh = dvr::capture::grab(d.dev9, d.dev11, d.ctx11);
        ID3D11ShaderResourceView* src = dvr::capture::srv();
        if (!src) return false;
        const uint32_t w = dvr::capture::width(), h = dvr::capture::height();
        if (!ensure_target(d.dev11, w, h)) return false;
        if (fresh || !drawnOnce_) {
            blit_.draw(d.ctx11, src, rtv_, w, h);
            if (OverlayDrawFn ov = overlay_draw()) ov(d.ctx11, rtv_, w, h);
            drawnOnce_ = true;
            ++frames_;
        } else {
            ++stale_;   // the last good frame goes out again
        }
        out.tex = tex_;
        out.eyeSign = 0;
        out.w = w; out.h = h;
        return true;
    }

    void on_reset() override { dvr::capture::on_reset(); }

    void shutdown() override {
        release_target();
        blit_.shutdown();
        drawnOnce_ = false;
    }

    void status(dvr::status::Writer& w) override {
        w.kv("monoFrames", (unsigned long)frames_);
        w.kv("monoStale", (unsigned long)stale_);
        const dvr::capture::Bbox b = dvr::capture::bbox();
        w.obj("bbox");
        w.kv("valid", b.valid);
        w.kv("x0", (int)b.x0); w.kv("y0", (int)b.y0); w.kv("x1", (int)b.x1); w.kv("y1", (int)b.y1);
        w.kv("pctW", (double)b.pctW); w.kv("pctH", (double)b.pctH);
        w.kv("nonBlackPct", (double)b.nonBlackPct);
        w.end_obj();
        const dvr::capture::Cost c = dvr::capture::cost();
        w.obj("captureCost");
        w.kv("rtdUs", (int)c.rtdUs); w.kv("copyUs", (int)c.copyUs);
        w.kv("uploadUs", (int)c.uploadUs); w.kv("totalUs", (int)c.totalUs);
        w.kv("grabs", (int)c.grabsInWindow);
        w.end_obj();
    }

private:
    bool ensure_target(ID3D11Device* dev, uint32_t w, uint32_t h) {
        if (tex_ && w_ == w && h_ == h) return true;
        release_target();
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = w; td.Height = h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // the runtime swapchain's family
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex_)) ||
            FAILED(dev->CreateRenderTargetView(tex_, nullptr, &rtv_))) {
            DVR_ERROR("mono: output texture %ux%u failed", w, h);
            release_target();
            return false;
        }
        w_ = w; h_ = h;
        drawnOnce_ = false;
        DVR_INFO("mono: output texture %ux%u (RGBA) - the head-locked screen shows this", w, h);
        return true;
    }
    void release_target() {
        if (rtv_) { rtv_->Release(); rtv_ = nullptr; }
        if (tex_) { tex_->Release(); tex_ = nullptr; }
        w_ = h_ = 0;
    }

    dvr::gfx::BlitQuad      blit_;
    ID3D11Texture2D*        tex_ = nullptr;
    ID3D11RenderTargetView* rtv_ = nullptr;
    uint32_t w_ = 0, h_ = 0;
    uint32_t frame_ = 0;
    uint32_t frames_ = 0, stale_ = 0;
    bool     drawnOnce_ = false;
};

MonoScreen g_mono;

} // namespace

IStereo* create_mono_screen() { return &g_mono; }

} // namespace dvr::stereo
