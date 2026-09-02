// core/vr/openxr_input.h - the OpenXR action layer (adopted from the BioShock
// trilogy VR mod in 41.0): one action set ("gameplay") with stick/trigger/
// grip/button/pose/haptic actions, suggested bindings for Touch, Index, Vive
// wands, WMR and the khr/simple fallback, and once per frame a raw controller
// SNAPSHOT plus located hand poses.
//
// Where BioShock composed an XInput pad here, Dishonored keeps its own
// composition in core/input/pad_bridge.cpp (sprint toggle, health hold, slide
// assist, the crouch pulses) and reads the snapshot: raw sticks, triggers,
// grips and buttons, no shaping. Haptics are queued from any thread and
// applied on the present thread inside input_sync - the thread that owns
// every runtime call (the 38.10 rule).
//
// This header is included only inside openxr_runtime.cpp's DVR_WITH_OPENXR
// block and by the pad bridge; the runtime passes its private handles at the
// lifecycle points below.

#pragma once

#include <openxr/openxr.h>

namespace dvr::vr {

// The raw controller state the pad bridge composes from. `active` false =
// no data this frame (no session, not FOCUSED, actions unbound).
struct InputSnapshot {
    float mv[2] = {0, 0};      // left thumbstick x, y (XR: +y forward)
    float lk[2] = {0, 0};      // right thumbstick
    float trigL = 0, trigR = 0;
    float gripL = 0, gripR = 0;
    bool  a = false, b = false, x = false, y = false;
    bool  clkL = false, clkR = false;   // stick clicks
    bool  menu = false;                 // left menu button, or the X+Y chord
    bool  active = false;
};

// After xrCreateInstance: create the action set + actions, suggest bindings.
void input_create(XrInstance instance);

// After session + reference spaces exist: create the action spaces (session
// children) and attach the action set (once per session).
void input_on_session_created(XrSession session, XrSpace baseSpace);

// From session teardown: destroy action spaces, clear the attach flag.
void input_on_session_teardown();

// Once per frame (Present-head, after xrWaitFrame): xrSyncActions, read,
// publish the snapshot, locate the hands, apply queued haptics.
// XR_SESSION_NOT_FOCUSED is a success code (actions read inactive) - it
// publishes an inactive snapshot and must never tear the session down.
void input_sync(XrSession session, XrTime predictedDisplayTime);

// The latest snapshot (copied under a lock; any thread).
void input_snapshot(InputSnapshot* out);

// True once the action set is attached to a live session.
bool input_attached();

// Queue a haptic pulse for a hand (0 = left, 1 = right); any thread. Applied
// on the present thread by input_sync.
void input_haptic(int hand, float amp, float durSec);

// Both stick clicks pressed together = the recenter chord (one edge per
// chord, re-armed when both release). Returns true once per chord.
bool take_recenter_chord();

// One status line inside vr::draw_debug_ui().
void input_draw_debug_ui();

// Latest predicted pose of a hand (0 = left, 1 = right), located in
// input_sync against the app space at the same predicted display time as the
// head pose. `aimPose` picks the runtime's AIM pose ("where this controller
// points") over the GRIP pose (the handle, what a hand model wants). False
// while that hand is not tracked. Meters + quaternion, XR convention.
bool input_get_hand_pose(int hand, bool aimPose, float* pos3, float* quat4);

// Session-20 vrrec (BioShock): a sim overlay on the funnel above. While any
// slot is armed, input_get_hand_pose serves the injected poses to ALL
// consumers; clear restores the live slots. Game-thread writers.
void input_set_sim_hand(int hand, bool aimPose, bool valid, const float pos3[3],
                        const float quat4[4]);
void input_clear_sim_hands();

} // namespace dvr::vr
