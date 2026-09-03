// core/gfx/reentry.cpp - rung 3 of the stereo ladder: SequentialReentry.
// DESIGN STUB (41.0): registered so `stereo reentry` names it and refuses
// with the note below; nothing here renders. The notes are the starting point
// for the developer taking this rung - the big bet of the two.
//
// THE IDEA. Call the engine's scene draw a SECOND time per tick with the
// other eye's camera, so every present carries a fresh eye and every per-eye
// effect (reflections, screen-space post, the viewmodel) is native to its
// eye. BioShock Infinite's rung 3 (bioshock-1-vr-mod: src/game/infinite/
// scenedraw.cpp and its ENGINE_NOTES "the render root") reached 2x presents
// per tick with the second call at 80-170 us; AER's compositor-held eye is
// gone and so is its half-rate flicker on fast motion.
//
// TASK ONE: the scene-draw entry. Nothing in patterns.h names it yet; the
// method that found it on Infinite, adapted to this engine (UE3 9099, D3D9):
//   1. Caller census at the camera write. ApplyHeadToViewRotation runs inside
//      the ProcessEvent hook on the script lane; log the return addresses of
//      the dispatches that carry the surviving rotation for one level, and
//      keep the ones that repeat once per tick.
//   2. One-shot stack scrape from that dispatch for call-preceded image
//      addresses (the 5-byte E8 test), to find the viewport draw root the
//      tick calls right after the camera is final. Static walking failed twice
//      on Infinite; do it LIVE.
//   3. Identify the pass by making it MOVE: `camera eyetest` (the c5
//      readback) tells whether a camera write BEFORE a candidate call moves
//      the draw; a call that does not move c5 is not the root.
//   4. Deny-by-default caller gate: the second call fires only from the ONE
//      return address measured in 1-3, never from a load screen, a cinematic
//      or a menu tick (game_state.h owns those verdicts).
//   5. The second call is SEH-guarded; a fault poisons the method for the
//      session (log, fall back to mono, never retry) - the game must keep
//      running flat (fail soft is the whole contract).
// Every address found goes to patterns.h with its derivation in ENGINE_NOTES.
//
// WHAT THIS METHOD DOES PER PRESENT, once the root is found:
//   begin_frame   nothing to choose: both eyes render every tick.
//   the game tick (script lane, in the ProcessEvent hook's camera pass):
//                 camera::set_eye(-1), apply the offset, the FIRST draw is
//                 the engine's own; then camera::set_eye(+1), apply, call the
//                 root a SECOND time (dvr::vr::sr_push_eye(-1) before the
//                 first present, sr_push_eye(+1) before the second - the
//                 runtime's eye-tag ring pairs the two presents into one XR
//                 frame: pair pacing in openxr_runtime.cpp).
//   end_frame     called twice per tick (the engine presents after each
//                 draw): capture into the eye texture the tag names and hand
//                 it out; the runtime's SR path submits the pair.
//   Present-rate is 2x the tick rate; the beat line must read L/s == R/s ==
//   out/s / 2, and draws/s == 2nd/s (the Infinite acceptance numbers).
//
// ACCEPTANCE (ROADMAP S2b): the root found and byte-verified; `stereo
// reentry` accepted; beat line L/s == R/s; second draw cost logged per
// present; xrsim-shot both eyes non-black with parallax; eye-check.ps1 legs
// 0-5; the headset: fusion, no flicker on fast motion, per-eye reflections.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/util/log.h"

namespace dvr::stereo {
namespace {

class SequentialReentry : public IStereo {
public:
    const char* name() const override { return "reentry"; }
    bool implemented() const override { return false; }
    bool wants_projection() const override { return true; }   // per-eye renders: the projection layer
    const char* note() const override {
        return "reentry is a design stub (core/gfx/reentry.cpp): the engine's scene draw is "
               "called a second time per tick with the other eye's camera; task one is "
               "finding that draw root (ROADMAP S2b).";
    }
    void begin_frame(const FrameInput&) override {}
    int  eye_for_next_frame() const override { return 0; }
    bool end_frame(const FrameDevices&, FrameOutput&) override {
        DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn, "stereo: reentry end_frame reached - %s", note());
        return false;
    }
    void on_reset() override {}
    void shutdown() override {}
    void status(dvr::status::Writer& w) override { w.kv("reentry", "design stub"); }
};

SequentialReentry g_reentry;

} // namespace

IStereo* create_reentry() { return &g_reentry; }

} // namespace dvr::stereo
