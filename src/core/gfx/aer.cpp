// core/gfx/aer.cpp - rung 2 of the stereo ladder: AlternateEye rendering.
// DESIGN STUB (41.0): registered so `stereo aer` names it and refuses with
// the note below; nothing here renders. The design is written down so the
// developer taking this rung starts from the same page as the re-entry one.
//
// THE IDEA. The game renders one frame per tick. Alternate which EYE that
// frame is rendered for: on even ticks the camera sits at the left eye, on
// odd ticks at the right, and each present carries one fresh eye while the
// compositor keeps showing the other eye's previous image (reprojected by
// the runtime for the head motion since). Cheap - the game's cost is
// unchanged - and geometrically real stereo, at half the temporal rate per
// eye. BioShock's rung 2 (docs/ARCHITECTURE.md, the ladder) shipped this
// before re-entry replaced it; the runtime layer still carries its machinery.
//
// WHAT THIS METHOD DOES PER PRESENT:
//   begin_frame   pick the eye the NEXT game frame renders: alternate the
//                 sign (-1, +1, -1, ...) and publish it through
//                 eye_for_next_frame(). The camera seam reads it on the
//                 script lane (dvr::camera::set_eye is called from the game
//                 tick each present) and writes +/- IPD/2 along the camera's
//                 right row into [Camera] EyeField - so this rung is gated on
//                 the eyetest having found an honoured field (camera.h). With
//                 no field, refuse: an AER without an eye offset is a mono
//                 screen with extra latency.
//   end_frame     capture the game's frame (dvr::capture) into THIS eye's
//                 texture (two RGBA targets, one per eye) and hand it out with
//                 eyeSign = the eye that was requested at the PREVIOUS
//                 begin_frame - the frame the game just presented was rendered
//                 during the tick that followed that request (one present of
//                 lag; the BioShock pipeline's lockstep note). The other eye's
//                 texture keeps its last content; the runtime layer holds the
//                 other eye's released swapchain image (its AER mode:
//                 dvr::vr::current_eye_sign / the g_aerEnabled path in
//                 openxr_runtime.cpp) and submits a projection layer with both.
//
// THE TAG AUDIT (why an eye tag can lie). The runtime's pair probe
// (openxr_runtime.cpp, `[pair]`, BioShock s43) exists because an UNTAGGED
// present - one the game side did not attribute - is captured into the LEFT
// swapchain by default, which is the "stale left eye" mechanism: the left
// image stops updating while the right keeps flowing, and every viewer reads
// it as a black or frozen left eye. The simulator's per-view source stats
// (tools/xrsim-shot.ps1, VERIFICATION) are the instrument: both eyes'
// nonBlackPct must move, and eye-check.ps1 leg 0 (the pairing leg) must see
// L/s and R/s equal on the stereo beat line (`stereo: beat method=aer ...`).
//
// ACCEPTANCE (ROADMAP S2a): eyetest HONOURED on some field; `stereo aer`
// accepted; beat line L/s == R/s == presents/2; xrsim-shot both eyes non-black
// with DIFFERENT content (the bbox and the mean luma differ by the parallax);
// eye-check.ps1 legs 0-5 on the simulator; then the headset: fusion at the
// measured IPD, no swim when turning (the tag lag is the first suspect if
// there is).
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/util/log.h"

namespace dvr::stereo {
namespace {

class AlternateEye : public IStereo {
public:
    const char* name() const override { return "aer"; }
    bool implemented() const override { return false; }
    const char* note() const override {
        return "aer is a design stub (core/gfx/aer.cpp): alternate the eye the camera seam "
               "renders each tick and tag each present; needs `camera eyetest` to have found "
               "an honoured eye field first (ROADMAP S2a).";
    }
    void begin_frame(const FrameInput&) override {}
    int  eye_for_next_frame() const override { return 0; }
    bool end_frame(const FrameDevices&, FrameOutput&) override {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn, "stereo: aer end_frame reached - %s", note());
        return false;
    }
    void on_reset() override {}
    void shutdown() override {}
    void status(dvr::status::Writer& w) override { w.kv("aer", "design stub"); }
};

AlternateEye g_aer;

} // namespace

IStereo* create_aer() { return &g_aer; }

} // namespace dvr::stereo
