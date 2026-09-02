#if DVR_WITH_OPENXR

#include "core/vr/openxr_input.h"

#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>

#include <imgui.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

#define XRLOG(...) DVR_LOG(::dvr::log::Cat::xrinput, ::dvr::log::Level::Info, __VA_ARGS__)

namespace dvr::vr {
namespace {

// Grip squeeze -> bumper with hysteresis (press/release thresholds).
constexpr float kGripPress = 0.70f;
constexpr float kGripRelease = 0.55f;
// Left menu button: short press -> START, hold -> BACK (Touch has no second
// system-legal menu button). START is emitted as a short pulse on release so
// the game's per-tick edge detection cannot miss it.
constexpr uint64_t kMenuLongMs = 500;
constexpr uint64_t kStartPulseMs = 150;
// Ammo-slot select (session 19, headset-revised): while the right-stick
// CLICK is held, stick directions past kFlickPress select the ammo slot
// (dpad up/down/left pulses); re-arm inside +-kFlickRearm, cooldown against
// machine-gunning. Zoom is removed - RS-click never reaches the game.
constexpr float kFlickPress = 0.65f;
constexpr float kFlickRearm = 0.30f;
constexpr uint64_t kFlickPulseMs = 150;
constexpr uint64_t kFlickCooldownMs = 300;


XrActionSet g_actionSet = XR_NULL_HANDLE;

XrAction g_move = XR_NULL_HANDLE;      // VECTOR2F left thumbstick
XrAction g_look = XR_NULL_HANDLE;      // VECTOR2F right thumbstick
XrAction g_fire = XR_NULL_HANDLE;      // FLOAT right trigger
XrAction g_plasmid = XR_NULL_HANDLE;   // FLOAT left trigger
XrAction g_gripR = XR_NULL_HANDLE;     // FLOAT right squeeze
XrAction g_gripL = XR_NULL_HANDLE;     // FLOAT left squeeze
XrAction g_btnA = XR_NULL_HANDLE;
XrAction g_btnB = XR_NULL_HANDLE;
XrAction g_btnX = XR_NULL_HANDLE;
XrAction g_btnY = XR_NULL_HANDLE;
XrAction g_stickClickL = XR_NULL_HANDLE;
XrAction g_stickClickR = XR_NULL_HANDLE;
XrAction g_menu = XR_NULL_HANDLE;
XrAction g_hapL = XR_NULL_HANDLE;      // 41.0: vibration outputs
XrAction g_hapR = XR_NULL_HANDLE;
// Capacitive thumbrest pads (session 23). BOOLEAN touch, part of the core
// oculus/touch_controller profile - no extension needed. read_bool() returns
// false when a binding is inactive, so a runtime that does not expose them
// simply never fires the modifier. g_thumbrestSeen logs the first real touch,
// which is how we prove the runtime actually reports it.
XrAction g_thumbrestL = XR_NULL_HANDLE;
XrAction g_thumbrestR = XR_NULL_HANDLE;
bool g_thumbrestSeen[2] = {false, false};

// s50 (Infinite): the FLOURISH CHORD - left thumbrest touched + A pressed.
// Armed by the adapter only (default off - BS1/BS2 composers byte-identical).
// While the rest is touched, A is consumed (jump never fires under the
// chord) and each rising A edge bumps the counter the adapter polls on the
// game thread. Left thumbrest for the same reason the flick modifier uses
// it: the chord is necessarily cross-hand (A sits under the right thumb).
std::atomic<bool> g_flourishChordArmed{false};
// s52: cinematic-scoped suspension - while set, the chord neither consumes A
// nor counts edges, so an interactive prompt's confirm press reaches the
// game (the raffle lesson). Set/cleared by the adapter's cinematic gate.
std::atomic<bool> g_flourishChordSuspended{false};
std::atomic<uint32_t> g_flourishChordEdges{0};
bool g_chordAWasDown = false;
XrAction g_poseL = XR_NULL_HANDLE;     // grip poses - hand position (M7 hands)
XrAction g_poseR = XR_NULL_HANDLE;
XrAction g_aimL = XR_NULL_HANDLE;      // AIM poses - where the controller POINTS.
XrAction g_aimR = XR_NULL_HANDLE;      // The runtime's own pointing ray; the grip
                                       // pose runs along the handle and reads
                                       // tens of degrees low as an aim vector.

XrSpace g_gripSpaceL = XR_NULL_HANDLE; // session children
XrSpace g_gripSpaceR = XR_NULL_HANDLE;
XrSpace g_aimSpaceL = XR_NULL_HANDLE;
XrSpace g_aimSpaceR = XR_NULL_HANDLE;
XrSpace g_baseSpace = XR_NULL_HANDLE;  // app space, owned by the runtime
bool g_attached = false;
bool g_created = false;
bool g_loggedAttachFail = false;

// Render-thread state for the composers.
bool g_gripLatchedL = false;
bool g_gripLatchedR = false;
uint64_t g_menuDownMs = 0;
uint64_t g_startPulseUntilMs = 0;
bool g_flickArmed = true;
uint16_t g_flickPulseBit = 0;
uint64_t g_flickPulseUntilMs = 0;
uint64_t g_flickCooldownMs = 0;
bool g_rsClickWasDown = false;

// M6 hand poses. Located on the render thread in input_sync; read from the
// GAME thread by the adapter's aim path, so publish through atomics-guarded
// snapshots (relaxed is fine: a group torn by one frame is invisible at
// 90 Hz, and the valid flag is what gates use of the numbers).
struct HandSlot {
    std::atomic<bool> valid{false};
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
};
HandSlot g_hands[2]; // grip pose: 0 = left, 1 = right
HandSlot g_aims[2];  // aim pose, same indexing

// Session 20 vrrec: sim overlay ON the funnel. While armed, EVERY consumer of
// input_get_hand_pose (fire ray, viewmodel, laser) reads the injected poses
// instead of the runtime's - replay only works if all three see one
// consistent world. Written from the game thread (replay tick / drive
// command), read game+render side - the same atomics discipline as the live
// slots.
std::atomic<bool> g_simHandsActive{false};
HandSlot g_simHands[2];
HandSlot g_simAims[2];

// Telemetry for the overlay (render thread writes, overlay reads same thread).
std::atomic<uint32_t> g_syncOk{0};
std::atomic<uint32_t> g_syncNotFocused{0};
std::atomic<uint32_t> g_syncFailed{0};
std::atomic<bool> g_lastActive{false};

// 41.0 (Dishonored): the raw snapshot the pad bridge composes from, the
// haptic queue (any thread -> applied on the present thread in input_sync)
// and the recenter-chord edge.
std::mutex g_snapMutex;
InputSnapshot g_snap;
std::atomic<float> g_hapAmp[2] = {};
std::atomic<float> g_hapDur[2] = {};
std::atomic<bool> g_hapPend[2] = {};
std::atomic<bool> g_recenterChord{false};
std::atomic<bool> g_attachedAtomic{false};

void publish_snapshot(const InputSnapshot& s) {
    std::lock_guard<std::mutex> lk(g_snapMutex);
    g_snap = s;
}
void publish_inactive() { publish_snapshot(InputSnapshot{}); }

XrPath path(XrInstance inst, const char* s) {
    XrPath p = XR_NULL_PATH;
    xrStringToPath(inst, s, &p);
    return p;
}

bool make_action(const char* name, const char* localized, XrActionType type,
                 XrAction* out) {
    XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
    aci.actionType = type;
    strcpy_s(aci.actionName, name);
    strcpy_s(aci.localizedActionName, localized);
    XrResult r = xrCreateAction(g_actionSet, &aci, out);
    if (XR_FAILED(r)) {
        XRLOG("xr-input: xrCreateAction(%s) failed (%d)", name,
                static_cast<int>(r));
        *out = XR_NULL_HANDLE;
        return false;
    }
    return true;
}

int16_t axis_to_thumb(float v) {
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return static_cast<int16_t>(v * 32767.0f);
}


float read_float(XrSession session, XrAction action) {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = action;
    XrActionStateFloat st{XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_FAILED(xrGetActionStateFloat(session, &gi, &st)) || !st.isActive)
        return 0.0f;
    return st.currentState;
}

bool read_bool(XrSession session, XrAction action) {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = action;
    XrActionStateBoolean st{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_FAILED(xrGetActionStateBoolean(session, &gi, &st)) || !st.isActive)
        return false;
    return st.currentState == XR_TRUE;
}

// Locate one grip space against the app space at the frame's predicted display
// time - the same instant the head pose is located, so hand and head belong to
// the same moment and the aim ray cannot lag the camera.
void locate_hand(XrSession session, XrAction poseAction, XrSpace space, XrTime when,
                 HandSlot& slot) {
    if (space == XR_NULL_HANDLE || g_baseSpace == XR_NULL_HANDLE) {
        slot.valid.store(false, std::memory_order_relaxed);
        return;
    }
    // The action must be active (controller present + bound) before its space
    // is meaningful.
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = poseAction;
    XrActionStatePose ps{XR_TYPE_ACTION_STATE_POSE};
    if (XR_FAILED(xrGetActionStatePose(session, &gi, &ps)) || !ps.isActive) {
        slot.valid.store(false, std::memory_order_relaxed);
        return;
    }
    XrSpaceLocation sl{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(space, g_baseSpace, when, &sl)) ||
        !(sl.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
        !(sl.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
        slot.valid.store(false, std::memory_order_relaxed);
        return;
    }
    slot.px = sl.pose.position.x;
    slot.py = sl.pose.position.y;
    slot.pz = sl.pose.position.z;
    slot.qx = sl.pose.orientation.x;
    slot.qy = sl.pose.orientation.y;
    slot.qz = sl.pose.orientation.z;
    slot.qw = sl.pose.orientation.w;
    slot.valid.store(true, std::memory_order_relaxed);
}

void invalidate_hand_slots() {
    for (int i = 0; i < 2; ++i) {
        g_hands[i].valid.store(false, std::memory_order_relaxed);
        g_aims[i].valid.store(false, std::memory_order_relaxed);
    }
}

bool read_vec2(XrSession session, XrAction action, float* x, float* y) {
    XrActionStateGetInfo gi{XR_TYPE_ACTION_STATE_GET_INFO};
    gi.action = action;
    XrActionStateVector2f st{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_FAILED(xrGetActionStateVector2f(session, &gi, &st)) || !st.isActive) {
        *x = *y = 0.0f;
        return false;
    }
    *x = st.currentState.x;
    *y = st.currentState.y;
    return true;
}

} // namespace

void input_create(XrInstance instance) {
    XrActionSetCreateInfo asci{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(asci.actionSetName, "gameplay");
    strcpy_s(asci.localizedActionSetName, "Gameplay");
    XrResult r = xrCreateActionSet(instance, &asci, &g_actionSet);
    if (XR_FAILED(r)) {
        XRLOG("xr-input: xrCreateActionSet failed (%d) - controller input off",
                static_cast<int>(r));
        return;
    }

    int made = 0;
    made += make_action("move", "Move", XR_ACTION_TYPE_VECTOR2F_INPUT, &g_move);
    made += make_action("look", "Look", XR_ACTION_TYPE_VECTOR2F_INPUT, &g_look);
    made += make_action("fire", "Fire weapon", XR_ACTION_TYPE_FLOAT_INPUT, &g_fire);
    made += make_action("plasmid", "Fire plasmid", XR_ACTION_TYPE_FLOAT_INPUT, &g_plasmid);
    made += make_action("grip_r", "Right grip", XR_ACTION_TYPE_FLOAT_INPUT, &g_gripR);
    made += make_action("grip_l", "Left grip", XR_ACTION_TYPE_FLOAT_INPUT, &g_gripL);
    made += make_action("btn_a", "A", XR_ACTION_TYPE_BOOLEAN_INPUT, &g_btnA);
    made += make_action("btn_b", "B", XR_ACTION_TYPE_BOOLEAN_INPUT, &g_btnB);
    made += make_action("btn_x", "X", XR_ACTION_TYPE_BOOLEAN_INPUT, &g_btnX);
    made += make_action("btn_y", "Y", XR_ACTION_TYPE_BOOLEAN_INPUT, &g_btnY);
    made += make_action("stick_l", "Left stick click", XR_ACTION_TYPE_BOOLEAN_INPUT,
                        &g_stickClickL);
    made += make_action("stick_r", "Right stick click", XR_ACTION_TYPE_BOOLEAN_INPUT,
                        &g_stickClickR);
    made += make_action("menu", "Menu", XR_ACTION_TYPE_BOOLEAN_INPUT, &g_menu);
    made += make_action("thumbrest_l", "Left thumbrest touch", XR_ACTION_TYPE_BOOLEAN_INPUT,
                        &g_thumbrestL);
    made += make_action("thumbrest_r", "Right thumbrest touch", XR_ACTION_TYPE_BOOLEAN_INPUT,
                        &g_thumbrestR);
    made += make_action("pose_l", "Left grip pose", XR_ACTION_TYPE_POSE_INPUT, &g_poseL);
    made += make_action("pose_r", "Right grip pose", XR_ACTION_TYPE_POSE_INPUT, &g_poseR);
    made += make_action("aim_l", "Left aim pose", XR_ACTION_TYPE_POSE_INPUT, &g_aimL);
    made += make_action("aim_r", "Right aim pose", XR_ACTION_TYPE_POSE_INPUT, &g_aimR);
    made += make_action("haptic_l", "Left haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, &g_hapL);
    made += make_action("haptic_r", "Right haptic", XR_ACTION_TYPE_VIBRATION_OUTPUT, &g_hapR);

    // Quest 3 Touch. Never bind .../input/system/click - reserved by runtimes.
    XrActionSuggestedBinding touch[] = {
        {g_move, path(instance, "/user/hand/left/input/thumbstick")},
        {g_look, path(instance, "/user/hand/right/input/thumbstick")},
        {g_fire, path(instance, "/user/hand/right/input/trigger/value")},
        {g_plasmid, path(instance, "/user/hand/left/input/trigger/value")},
        {g_gripR, path(instance, "/user/hand/right/input/squeeze/value")},
        {g_gripL, path(instance, "/user/hand/left/input/squeeze/value")},
        {g_btnA, path(instance, "/user/hand/right/input/a/click")},
        {g_btnB, path(instance, "/user/hand/right/input/b/click")},
        {g_btnX, path(instance, "/user/hand/left/input/x/click")},
        {g_btnY, path(instance, "/user/hand/left/input/y/click")},
        {g_stickClickL, path(instance, "/user/hand/left/input/thumbstick/click")},
        {g_stickClickR, path(instance, "/user/hand/right/input/thumbstick/click")},
        {g_thumbrestL, path(instance, "/user/hand/left/input/thumbrest/touch")},
        {g_thumbrestR, path(instance, "/user/hand/right/input/thumbrest/touch")},
        {g_menu, path(instance, "/user/hand/left/input/menu/click")},
        {g_poseL, path(instance, "/user/hand/left/input/grip/pose")},
        {g_poseR, path(instance, "/user/hand/right/input/grip/pose")},
        {g_aimL, path(instance, "/user/hand/left/input/aim/pose")},
        {g_aimR, path(instance, "/user/hand/right/input/aim/pose")},
        {g_hapL, path(instance, "/user/hand/left/output/haptic")},
        {g_hapR, path(instance, "/user/hand/right/output/haptic")},
    };
    XrInteractionProfileSuggestedBinding sb{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    sb.interactionProfile = path(instance, "/interaction_profiles/oculus/touch_controller");
    sb.suggestedBindings = touch;
    sb.countSuggestedBindings = static_cast<uint32_t>(sizeof(touch) / sizeof(touch[0]));
    r = xrSuggestInteractionProfileBindings(instance, &sb);
    if (XR_FAILED(r))
        XRLOG("xr-input: touch_controller binding suggestion failed (%d)",
                static_cast<int>(r));

    // Minimal khr/simple_controller fallback so unknown runtimes still give
    // menu-walk capability (select = A, menu = START).
    XrActionSuggestedBinding simple[] = {
        {g_btnA, path(instance, "/user/hand/right/input/select/click")},
        {g_menu, path(instance, "/user/hand/left/input/menu/click")},
        {g_poseL, path(instance, "/user/hand/left/input/grip/pose")},
        {g_poseR, path(instance, "/user/hand/right/input/grip/pose")},
        {g_aimL, path(instance, "/user/hand/left/input/aim/pose")},
        {g_aimR, path(instance, "/user/hand/right/input/aim/pose")},
        {g_hapL, path(instance, "/user/hand/left/output/haptic")},
        {g_hapR, path(instance, "/user/hand/right/output/haptic")},
    };
    sb.interactionProfile = path(instance, "/interaction_profiles/khr/simple_controller");
    sb.suggestedBindings = simple;
    sb.countSuggestedBindings = static_cast<uint32_t>(sizeof(simple) / sizeof(simple[0]));
    xrSuggestInteractionProfileBindings(instance, &sb); // best-effort

    // SteamVR-family profiles (s62). All best-effort like touch: a runtime that
    // rejects a profile only loses that hardware, never the session. Boolean
    // inputs (squeeze/click on Vive/WMR) bound to FLOAT actions are converted
    // to 0.0/1.0 by the runtime per spec 11.4 - the grip hysteresis
    // (0.70/0.55) latches at 1.0 and releases at 0.0, so no read path changes.
    // Unbound actions simply read inactive (read_float/read_bool return 0).

    // Valve Index. A/B on BOTH hands (left has no x/y paths), analog squeeze,
    // no menu button (system/click is reserved - never bind), no thumbrest:
    // the noThumbrestYet fallback keeps RS-click as the ammo modifier, and
    // menu goes to a firm left-trackpad press (boolean action on the float
    // force component - the runtime thresholds it).
    XrActionSuggestedBinding index[] = {
        {g_move, path(instance, "/user/hand/left/input/thumbstick")},
        {g_look, path(instance, "/user/hand/right/input/thumbstick")},
        {g_fire, path(instance, "/user/hand/right/input/trigger/value")},
        {g_plasmid, path(instance, "/user/hand/left/input/trigger/value")},
        {g_gripR, path(instance, "/user/hand/right/input/squeeze/value")},
        {g_gripL, path(instance, "/user/hand/left/input/squeeze/value")},
        {g_btnA, path(instance, "/user/hand/right/input/a/click")},
        {g_btnB, path(instance, "/user/hand/right/input/b/click")},
        {g_btnX, path(instance, "/user/hand/left/input/a/click")},
        {g_btnY, path(instance, "/user/hand/left/input/b/click")},
        {g_stickClickL, path(instance, "/user/hand/left/input/thumbstick/click")},
        {g_stickClickR, path(instance, "/user/hand/right/input/thumbstick/click")},
        {g_menu, path(instance, "/user/hand/left/input/trackpad/force")},
        {g_poseL, path(instance, "/user/hand/left/input/grip/pose")},
        {g_poseR, path(instance, "/user/hand/right/input/grip/pose")},
        {g_aimL, path(instance, "/user/hand/left/input/aim/pose")},
        {g_aimR, path(instance, "/user/hand/right/input/aim/pose")},
        {g_hapL, path(instance, "/user/hand/left/output/haptic")},
        {g_hapR, path(instance, "/user/hand/right/output/haptic")},
    };
    sb.interactionProfile = path(instance, "/interaction_profiles/valve/index_controller");
    sb.suggestedBindings = index;
    sb.countSuggestedBindings = static_cast<uint32_t>(sizeof(index) / sizeof(index[0]));
    r = xrSuggestInteractionProfileBindings(instance, &sb);
    if (XR_FAILED(r))
        XRLOG("xr-input: index_controller binding suggestion failed (%d)",
                static_cast<int>(r));

    // HTC Vive wands: trigger, squeeze CLICK only, menu on both hands, and a
    // trackpad standing in for both sticks. No face buttons exist, so jump/
    // heal/reload (btn_b/x/y) stay unbound natively - the SteamVR shim's
    // binding JSONs plus SteamVR's own binding UI are the full-coverage path.
    // stick_r = right trackpad click is eaten as the ammo modifier on BS1/BS2,
    // which suits a trackpad: press the pad, touch direction picks the slot.
    XrActionSuggestedBinding vive[] = {
        {g_move, path(instance, "/user/hand/left/input/trackpad")},
        {g_look, path(instance, "/user/hand/right/input/trackpad")},
        {g_fire, path(instance, "/user/hand/right/input/trigger/value")},
        {g_plasmid, path(instance, "/user/hand/left/input/trigger/value")},
        {g_gripR, path(instance, "/user/hand/right/input/squeeze/click")},
        {g_gripL, path(instance, "/user/hand/left/input/squeeze/click")},
        {g_btnA, path(instance, "/user/hand/right/input/menu/click")},
        {g_stickClickL, path(instance, "/user/hand/left/input/trackpad/click")},
        {g_stickClickR, path(instance, "/user/hand/right/input/trackpad/click")},
        {g_menu, path(instance, "/user/hand/left/input/menu/click")},
        {g_poseL, path(instance, "/user/hand/left/input/grip/pose")},
        {g_poseR, path(instance, "/user/hand/right/input/grip/pose")},
        {g_aimL, path(instance, "/user/hand/left/input/aim/pose")},
        {g_aimR, path(instance, "/user/hand/right/input/aim/pose")},
        {g_hapL, path(instance, "/user/hand/left/output/haptic")},
        {g_hapR, path(instance, "/user/hand/right/output/haptic")},
    };
    sb.interactionProfile = path(instance, "/interaction_profiles/htc/vive_controller");
    sb.suggestedBindings = vive;
    sb.countSuggestedBindings = static_cast<uint32_t>(sizeof(vive) / sizeof(vive[0]));
    r = xrSuggestInteractionProfileBindings(instance, &sb);
    if (XR_FAILED(r))
        XRLOG("xr-input: vive_controller binding suggestion failed (%d)",
                static_cast<int>(r));
    else
        XRLOG("xr-input: vive wands have no face buttons - jump/heal/reload "
                "unbound natively; use the SteamVR shim + binding UI for full "
                "wand support");

    // WMR motion controllers: thumbstick AND trackpad, squeeze click, menu on
    // both hands, no face buttons. Trackpad clicks stand in for A (use) and X;
    // B/Y stay unbound (same advisory as Vive). Never hardware-tested - the
    // SteamVR binding UI is the correction mechanism.
    XrActionSuggestedBinding wmr[] = {
        {g_move, path(instance, "/user/hand/left/input/thumbstick")},
        {g_look, path(instance, "/user/hand/right/input/thumbstick")},
        {g_fire, path(instance, "/user/hand/right/input/trigger/value")},
        {g_plasmid, path(instance, "/user/hand/left/input/trigger/value")},
        {g_gripR, path(instance, "/user/hand/right/input/squeeze/click")},
        {g_gripL, path(instance, "/user/hand/left/input/squeeze/click")},
        {g_btnA, path(instance, "/user/hand/right/input/trackpad/click")},
        {g_btnX, path(instance, "/user/hand/left/input/trackpad/click")},
        {g_stickClickL, path(instance, "/user/hand/left/input/thumbstick/click")},
        {g_stickClickR, path(instance, "/user/hand/right/input/thumbstick/click")},
        {g_menu, path(instance, "/user/hand/left/input/menu/click")},
        {g_poseL, path(instance, "/user/hand/left/input/grip/pose")},
        {g_poseR, path(instance, "/user/hand/right/input/grip/pose")},
        {g_aimL, path(instance, "/user/hand/left/input/aim/pose")},
        {g_aimR, path(instance, "/user/hand/right/input/aim/pose")},
        {g_hapL, path(instance, "/user/hand/left/output/haptic")},
        {g_hapR, path(instance, "/user/hand/right/output/haptic")},
    };
    sb.interactionProfile =
        path(instance, "/interaction_profiles/microsoft/motion_controller");
    sb.suggestedBindings = wmr;
    sb.countSuggestedBindings = static_cast<uint32_t>(sizeof(wmr) / sizeof(wmr[0]));
    r = xrSuggestInteractionProfileBindings(instance, &sb);
    if (XR_FAILED(r))
        XRLOG("xr-input: motion_controller binding suggestion failed (%d)",
                static_cast<int>(r));

    g_created = true;
    XRLOG("xr-input: action set ready (%d actions; touch + simple + index + "
            "vive + wmr bindings suggested)", made);
}

void input_on_session_created(XrSession session, XrSpace baseSpace) {
    if (!g_created) return;
    g_baseSpace = baseSpace; // M6: hand poses locate against the app space

    XrActionSpaceCreateInfo asci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    asci.poseInActionSpace.orientation.w = 1.0f;
    asci.action = g_poseL;
    if (XR_FAILED(xrCreateActionSpace(session, &asci, &g_gripSpaceL)))
        g_gripSpaceL = XR_NULL_HANDLE;
    asci.action = g_poseR;
    if (XR_FAILED(xrCreateActionSpace(session, &asci, &g_gripSpaceR)))
        g_gripSpaceR = XR_NULL_HANDLE;
    asci.action = g_aimL;
    if (XR_FAILED(xrCreateActionSpace(session, &asci, &g_aimSpaceL)))
        g_aimSpaceL = XR_NULL_HANDLE;
    asci.action = g_aimR;
    if (XR_FAILED(xrCreateActionSpace(session, &asci, &g_aimSpaceR)))
        g_aimSpaceR = XR_NULL_HANDLE;

    XrSessionActionSetsAttachInfo sai{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    sai.countActionSets = 1;
    sai.actionSets = &g_actionSet;
    XrResult r = xrAttachSessionActionSets(session, &sai);
    if (XR_FAILED(r)) {
        if (!g_loggedAttachFail) {
            XRLOG("xr-input: xrAttachSessionActionSets failed (%d) - display "
                    "continues without controller input", static_cast<int>(r));
            g_loggedAttachFail = true;
        }
        return;
    }
    g_attached = true;
    g_attachedAtomic.store(true, std::memory_order_relaxed);
    g_loggedAttachFail = false;
    XRLOG("xr-input: action set attached to session");
}

void input_on_session_teardown() {
    if (g_gripSpaceL != XR_NULL_HANDLE) { xrDestroySpace(g_gripSpaceL); g_gripSpaceL = XR_NULL_HANDLE; }
    if (g_gripSpaceR != XR_NULL_HANDLE) { xrDestroySpace(g_gripSpaceR); g_gripSpaceR = XR_NULL_HANDLE; }
    if (g_aimSpaceL != XR_NULL_HANDLE) { xrDestroySpace(g_aimSpaceL); g_aimSpaceL = XR_NULL_HANDLE; }
    if (g_aimSpaceR != XR_NULL_HANDLE) { xrDestroySpace(g_aimSpaceR); g_aimSpaceR = XR_NULL_HANDLE; }
    g_baseSpace = XR_NULL_HANDLE;
    invalidate_hand_slots();
    g_attached = false;
    g_attachedAtomic.store(false, std::memory_order_relaxed);
    g_gripLatchedL = g_gripLatchedR = false;
    g_menuDownMs = 0;
    g_startPulseUntilMs = 0;
    publish_inactive();
}

void input_sync(XrSession session, XrTime predictedDisplayTime) {
    if (!g_attached) return;

    XrActiveActionSet active{g_actionSet, XR_NULL_PATH};
    XrActionsSyncInfo si{XR_TYPE_ACTIONS_SYNC_INFO};
    si.countActiveActionSets = 1;
    si.activeActionSets = &active;
    XrResult r = xrSyncActions(session, &si);
    if (r == XR_SESSION_NOT_FOCUSED) {
        g_syncNotFocused.fetch_add(1, std::memory_order_relaxed);
        g_lastActive.store(false, std::memory_order_relaxed);
        invalidate_hand_slots();
        publish_inactive();
        return;
    }
    if (XR_FAILED(r)) {
        // Never tear the session down from the input path - display first.
        g_syncFailed.fetch_add(1, std::memory_order_relaxed);
        g_lastActive.store(false, std::memory_order_relaxed);
        invalidate_hand_slots();
        publish_inactive();
        return;
    }
    g_syncOk.fetch_add(1, std::memory_order_relaxed);

    // M6: hand grip poses for the aim ray (never fatal - a hand that fails to
    // locate just leaves its slot invalid and the aim falls back to the view).
    locate_hand(session, g_poseL, g_gripSpaceL, predictedDisplayTime, g_hands[0]);
    locate_hand(session, g_poseR, g_gripSpaceR, predictedDisplayTime, g_hands[1]);
    locate_hand(session, g_aimL, g_aimSpaceL, predictedDisplayTime, g_aims[0]);
    locate_hand(session, g_aimR, g_aimSpaceR, predictedDisplayTime, g_aims[1]);

    // 41.0 (Dishonored): publish the RAW state. Shaping (deadzone, grip
    // hysteresis, sprint toggle, health hold, slide assist, crouch pulses)
    // lives in core/input/pad_bridge.cpp, where it always did.
    InputSnapshot s;
    read_vec2(session, g_move, &s.mv[0], &s.mv[1]);
    read_vec2(session, g_look, &s.lk[0], &s.lk[1]);
    s.trigR = read_float(session, g_fire);
    s.trigL = read_float(session, g_plasmid);
    s.gripL = read_float(session, g_gripL);
    s.gripR = read_float(session, g_gripR);
    s.a = read_bool(session, g_btnA);
    s.b = read_bool(session, g_btnB);
    // s62c (BioShock): LEFT X+Y together = the menu button, for the Steam Link
    // overlay that swallows the real one. Latched until both release, so the
    // button released second cannot fire its own action on the way out.
    const bool btnXDown = read_bool(session, g_btnX);
    const bool btnYDown = read_bool(session, g_btnY);
    const bool menuChord = btnXDown && btnYDown;
    static bool s_menuChordLatch = false;
    if (menuChord) s_menuChordLatch = true;
    else if (!btnXDown && !btnYDown) s_menuChordLatch = false;
    if (!s_menuChordLatch) { s.x = btnXDown; s.y = btnYDown; }
    s.menu = read_bool(session, g_menu) || menuChord;
    // Both stick clicks together = the recenter chord (one edge per chord,
    // re-armed when both release); while held neither click reaches the game.
    const bool clickL = read_bool(session, g_stickClickL);
    const bool clickR = read_bool(session, g_stickClickR);
    const bool chordHeld = clickL && clickR;
    static bool s_chordArmed = true;
    if (chordHeld && s_chordArmed) {
        s_chordArmed = false;
        g_recenterChord.store(true, std::memory_order_relaxed);
    } else if (!clickL && !clickR) {
        s_chordArmed = true;
    }
    if (!chordHeld) { s.clkL = clickL; s.clkR = clickR; }
    s.active = true;
    g_lastActive.store(true, std::memory_order_relaxed);
    publish_snapshot(s);

    // Queued haptics, applied on THIS thread - the one that owns every runtime
    // call (38.10: a haptic from another thread while the pace lane was inside
    // the runtime trashed the heap on VDXR).
    for (int h = 0; h < 2; ++h) {
        if (!g_hapPend[h].exchange(false, std::memory_order_acq_rel)) continue;
        XrAction act = h ? g_hapR : g_hapL;
        if (act == XR_NULL_HANDLE) continue;
        XrHapticVibration hv{XR_TYPE_HAPTIC_VIBRATION};
        hv.amplitude = g_hapAmp[h].load(std::memory_order_relaxed);
        hv.duration = static_cast<XrDuration>(g_hapDur[h].load(std::memory_order_relaxed) * 1.0e9f);
        hv.frequency = XR_FREQUENCY_UNSPECIFIED;
        XrHapticActionInfo hai{XR_TYPE_HAPTIC_ACTION_INFO};
        hai.action = act;
        xrApplyHapticFeedback(session, &hai, reinterpret_cast<const XrHapticBaseHeader*>(&hv));
    }
}

void input_snapshot(InputSnapshot* out) {
    if (!out) return;
    std::lock_guard<std::mutex> lk(g_snapMutex);
    *out = g_snap;
}

bool input_attached() { return g_attachedAtomic.load(std::memory_order_relaxed); }

void input_haptic(int hand, float amp, float durSec) {
    if (hand < 0 || hand > 1) return;
    if (amp < 0.0f) amp = 0.0f;
    if (amp > 1.0f) amp = 1.0f;
    if (durSec < 0.005f) durSec = 0.005f;
    if (durSec > 2.0f) durSec = 2.0f;
    g_hapAmp[hand].store(amp, std::memory_order_relaxed);
    g_hapDur[hand].store(durSec, std::memory_order_relaxed);
    g_hapPend[hand].store(true, std::memory_order_release);
}

bool take_recenter_chord() {
    return g_recenterChord.exchange(false, std::memory_order_relaxed);
}

bool input_get_hand_pose(int hand, bool aimPose, float* pos3, float* quat4) {
    if (hand < 0 || hand > 1 || !pos3 || !quat4) return false;
    const bool sim = g_simHandsActive.load(std::memory_order_relaxed);
    const HandSlot& s = sim ? (aimPose ? g_simAims[hand] : g_simHands[hand])
                            : (aimPose ? g_aims[hand] : g_hands[hand]);
    if (!s.valid.load(std::memory_order_relaxed)) return false;
    pos3[0] = s.px; pos3[1] = s.py; pos3[2] = s.pz;
    quat4[0] = s.qx; quat4[1] = s.qy; quat4[2] = s.qz; quat4[3] = s.qw;
    return true;
}

void input_set_sim_hand(int hand, bool aimPose, bool valid, const float pos3[3],
                        const float quat4[4]) {
    if (hand < 0 || hand > 1) return;
    HandSlot& s = aimPose ? g_simAims[hand] : g_simHands[hand];
    if (pos3) { s.px = pos3[0]; s.py = pos3[1]; s.pz = pos3[2]; }
    if (quat4) { s.qx = quat4[0]; s.qy = quat4[1]; s.qz = quat4[2]; s.qw = quat4[3]; }
    s.valid.store(valid, std::memory_order_relaxed);
    // Arming ANY slot flips the whole funnel to sim - unset slots read
    // invalid, which is what a faithful replay of an untracked hand wants.
    g_simHandsActive.store(true, std::memory_order_relaxed);
}

void input_clear_sim_hands() {
    g_simHandsActive.store(false, std::memory_order_relaxed);
    for (int h = 0; h < 2; ++h) {
        g_simHands[h].valid.store(false, std::memory_order_relaxed);
        g_simAims[h].valid.store(false, std::memory_order_relaxed);
    }
}

void input_draw_debug_ui() {
    ImGui::Text("xr hands: grip L %s R %s | aim L %s R %s",
                g_hands[0].valid.load(std::memory_order_relaxed) ? "ok" : "-",
                g_hands[1].valid.load(std::memory_order_relaxed) ? "ok" : "-",
                g_aims[0].valid.load(std::memory_order_relaxed) ? "ok" : "-",
                g_aims[1].valid.load(std::memory_order_relaxed) ? "ok" : "-");
    ImGui::Text("xr actions: %s | sync ok %u nf %u fail %u | %s",
                g_attached ? "attached" : (g_created ? "created" : "off"),
                g_syncOk.load(std::memory_order_relaxed),
                g_syncNotFocused.load(std::memory_order_relaxed),
                g_syncFailed.load(std::memory_order_relaxed),
                g_lastActive.load(std::memory_order_relaxed) ? "ACTIVE" : "idle");
}

} // namespace dvr::vr


#endif // DVR_WITH_OPENXR
