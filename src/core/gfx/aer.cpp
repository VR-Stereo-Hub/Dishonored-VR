// core/gfx/aer.cpp - rung 2 of the stereo ladder: AlternateEye rendering.
//
// THE IDEA. The game renders one frame per tick. Alternate which EYE that
// frame is rendered for: the camera seam offsets the camera by +/- IPD/2 along
// view-right on the script lane, and each present carries one eye's image.
//
// WHICH AER. There are two shapes and they are not equally good:
//
//   (a) HELD EYE - submit every present, the compositor keeps showing the
//       other eye's previous image and reprojects it. Half the temporal rate
//       per eye and it leans on reprojection to hide a genuine mismatch.
//       The runtime layer has this (g_aerEnabled / current_eye_sign) and this
//       method deliberately does NOT use it.
//
//   (b) PAIRED - the left present is HELD OPEN, the right present closes the
//       pair, and ONE projection layer carrying BOTH eyes is submitted per
//       pair. No stale eye, no reprojected sibling, both images in the same
//       xrEndFrame. This is what BioShock Remastered VR does (XR_SubmitPair:
//       "eye 0 stashes, eye 1 submits the pair - one XR cycle per two
//       Presents") and it is why that mod fuses cleanly.
//
// We take (b), and it costs no new runtime code: the layer's SequentialReentry
// tag ring is exactly the mechanism. sr_push_eye() tags a present with an eye,
// the render thread pops one tag per present and captures into that eye's
// swapchain, and the pair pacing holds the left frame open until the right
// arrives. The ring does not care whether the two frames came from one game
// tick (rung 3) or two (this rung) - only that they arrive tagged, in order.
//
// THE ONE PRESENT OF LAG. begin_frame publishes the eye for the frame the game
// is ABOUT to render; that frame reaches us at the NEXT present. So the tag
// attached at end_frame is the sign published at the PREVIOUS begin_frame, not
// this one. Getting this wrong swaps the eyes and inverts depth - which reads
// as "stereo works but everything is inside out", not as a crash.
//
// FRESH FRAMES ONLY. dvr::capture reports whether the backbuffer actually
// moved. A present that re-shows the previous image must NOT be tagged: the
// pair would then carry the same picture in both eyes (zero disparity, which
// the eye reads as infinitely far away) or, worse, one eye's picture tagged as
// the other's. A stale present emits nothing, the ring pops 0, and the last
// good pair stays up.
//
// ACCEPTANCE (ROADMAP S2a): `stereo aer` accepted; the beat line reads
// L/s == R/s == out/s / 2; xrsim-shot shows two projection views with
// DIFFERENT content and EyeSeparationM ~= the IPD; the [pair] probe reports no
// untagged presents and no staleL; then the headset - fusion at the measured
// IPD, and no swim on head turns.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/gfx/blit_quad.h"
#include "core/gfx/capture.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>
#include <d3d11.h>

namespace dvr::stereo {
namespace {

class AlternateEye : public IStereo {
public:
    const char* name() const override { return "aer"; }
    bool implemented() const override { return true; }
    const char* note() const override {
        return "aer renders one eye per game frame (the camera seam offsets +/- IPD/2) and "
               "submits both eyes as one paired projection layer; it needs `camera eyetest` "
               "to have found an honoured eye field first ([Camera] EyeField=).";
    }

    void begin_frame(const FrameInput& in) override {
        // Projection mode is what makes the runtime submit a projection layer
        // instead of the mono quad. Idempotent; cleared again in shutdown().
        dvr::vr::set_camera_mode(true);

        frame_ = in.frame;
        ipdM_  = in.ipdM;

        // The frame we are about to capture at THIS present was rendered by the
        // tick that followed the PREVIOUS begin_frame, so it carries that sign.
        inFlight_ = published_;
        published_ = (published_ >= 0) ? -1 : +1;   // -1 left, +1 right
        if (!armedLogged_) {
            armedLogged_ = true;
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                    "aer: armed - one eye per game frame, paired submit. ipd %.4f m, "
                    "recommended eye %ux%u. The headset rate is half the game's present "
                    "rate BY DESIGN (one XR frame per L/R pair).",
                    in.ipdM, in.eyeW, in.eyeH);
        }
    }

    int eye_for_next_frame() const override { return published_; }

    bool end_frame(const FrameDevices& d, FrameOutput& out) override {
        if (!d.dev9 || !d.dev11 || !d.ctx11) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                         "aer: no D3D11 device - the game runs flat, nothing reaches the headset");
            return false;
        }
        if (!blit_.init(d.dev11)) return false;

        const bool fresh = dvr::capture::grab(d.dev9, d.dev11, d.ctx11);
        if (!fresh) {
            // A repeat of the previous image. Tagging it would put one eye's
            // picture into the other eye. Skip the present entirely; the pair
            // on screen stays up and the ring pops 0.
            ++skippedStale_;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 5000,
                             "aer: %lu stale presents skipped (the game re-showed a frame; "
                             "tagging it would cross the eyes)", skippedStale_);
            return false;
        }
        // The pipeline needs one present to fill: the very first frame after
        // arming was rendered before any eye was asked for.
        if (inFlight_ == 0) { ++primed_; return false; }

        ID3D11ShaderResourceView* src = dvr::capture::srv();
        if (!src) return false;
        const uint32_t w = dvr::capture::width(), h = dvr::capture::height();
        if (!ensure_target(d.dev11, w, h)) return false;

        blit_.draw(d.ctx11, src, rtv_, w, h);
        if (OverlayDrawFn ov = overlay_draw()) ov(d.ctx11, rtv_, w, h);

        if (inFlight_ < 0) ++left_; else ++right_;

        // THE INSTRUMENT THAT WAS MISSING (41.1). Tagging both eyes correctly
        // is not the same as both eyes REACHING the headset, and the first
        // headset run proved it: the beat line read a perfect L/s == R/s while
        // the runtime was quietly showing the mono quad, because the cinematic
        // fallback had no gameplay verdict to read and a never-published
        // verdict is indistinguishable from a cutscene. What the player saw
        // was the alternating camera as a side-to-side FLICKER. So say, here,
        // whether the eyes we are tagging are actually being used - and say
        // what to do about it, because the answer is never "AER is broken".
        if (!dvr::vr::vr_camera_mode()) {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                "aer: tagging eyes but the runtime is NOT in projection mode - the quad "
                "screen is what reaches the headset, so this looks like MONO THAT FLICKERS "
                "side to side, not like stereo. The eye tags are fine; the layer is the "
                "problem (session/projection not ready yet, or no hfov claim).");
        } else if (dvr::vr::cinematic_active()) {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                "aer: tagging eyes but the CINEMATIC QUAD is up - the projection layer is "
                "being dropped for the flat screen. In gameplay that means the game-state "
                "verdict reads MENU/CINEMATIC/NO_PAWN or has gone stale; `vrcine off` is "
                "the live A/B that proves it.");
        }
        if (ipdM_ <= 0.0f) {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                "aer: the runtime has published no eye separation yet (ipd 0) - the camera "
                "offset is eye * ipd/2 * scale, so the eyes are being tagged with ZERO "
                "disparity. That is mono with extra latency until the first xrLocateViews.");
        }

        out.tex = tex_;
        out.eyeSign = inFlight_;
        out.w = w; out.h = h;
        return true;
    }

    void on_reset() override { dvr::capture::on_reset(); }

    void shutdown() override {
        dvr::vr::set_camera_mode(false);
        release_target();
        blit_.shutdown();
        published_ = 0;
        inFlight_ = 0;
        armedLogged_ = false;
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                "aer: stood down - camera mode off, the quad screen takes over");
    }

    void status(dvr::status::Writer& w) override {
        w.kv("aerLeft", (unsigned long)left_);
        w.kv("aerRight", (unsigned long)right_);
        w.kv("aerStaleSkipped", (unsigned long)skippedStale_);
        w.kv("aerPriming", (unsigned long)primed_);
        w.kv("aerIpdM", (double)ipdM_);
        // The downstream truth: are the tagged eyes actually being submitted?
        w.kv("aerProjection", dvr::vr::vr_camera_mode());
        w.kv("aerCineQuad", dvr::vr::cinematic_active());
        // Left and right must track each other. A gap means presents are being
        // dropped on one side, which is the eye-crossing failure mode.
        const long gap = (long)left_ - (long)right_;
        w.kv("aerLRGap", (int)gap);
        const dvr::capture::Bbox b = dvr::capture::bbox();
        w.obj("bbox");
        w.kv("valid", b.valid);
        w.kv("pctW", (double)b.pctW); w.kv("pctH", (double)b.pctH);
        w.kv("nonBlackPct", (double)b.nonBlackPct);
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
            DVR_ERROR("aer: output texture %ux%u failed", w, h);
            release_target();
            return false;
        }
        w_ = w; h_ = h;
        DVR_INFO("aer: output texture %ux%u (RGBA) - one eye per present goes through this", w, h);
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
    float    ipdM_ = 0.0f;
    int      published_ = 0;   // the eye the NEXT game frame renders
    int      inFlight_  = 0;   // the eye baked into the frame we capture now
    unsigned long left_ = 0, right_ = 0, skippedStale_ = 0, primed_ = 0;
    bool     armedLogged_ = false;
};

AlternateEye g_aer;

} // namespace

IStereo* create_aer() { return &g_aer; }

} // namespace dvr::stereo
