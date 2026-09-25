// ============================================================================
//  ovrshim_input.cpp - OpenXR actions -> SteamVR input (IVRInput).
//
//  Adapted from BioVRDev/Bioshock-Remastered-VR OpenXRShim/src/shim_input.cpp
//  with the author's permission (see THIRD_PARTY_NOTICES.md). Deltas vs donor:
//    - binding manifests rewritten for THIS mod's action set ("gameplay", 19
//      actions) - SteamVR binds by ACTION NAME, so none of the donor's names
//      survive
//    - NEW: xrGetActionStatePose (the mod's locate_hand gates on it)
//    - float reads fall back to GetDigitalActionData when the analog read is
//      inactive, so a button bound to a vector1 action (Vive/WMR grip) still
//      drives the mod's FLOAT grip actions as 0/1 - the donor documented this
//      as the "WMR radials never open" hazard and left it open
//    - Index menu = left trackpad CLICK; the donor's collision (their trackpad
//      TOUCH was also their d-pad modifier) does not apply here because this
//      mod's modifier is the thumbrest, which Index lacks - thumbrest stays
//      unbound and the mod's stick-click fallback covers the ammo radial
//
//  The mod creates one action set ("gameplay") with named actions and suggests
//  bindings for oculus/touch_controller etc. SteamVR's input system wants a
//  JSON action manifest plus per-controller binding files instead, so at
//  attach time we generate them next to the DLL and hand them to
//  SetActionManifestPath. Users can rebind anything in SteamVR's controller
//  UI - that UI is the escape hatch for every gap below.
//
//  Pose components: the mod's aim_l/aim_r -> SteamVR "tip", pose_l/pose_r
//  (grip) -> "handgrip". EXCEPT Index: the shim rebuilds all four from the
//  raw device pose (IndexPoseCorrection below), so an Index hand reports the
//  same grip/aim frames a Quest hand reports under VDXR (where every
//  per-weapon offset was tuned), whatever SteamVR binding is live.
// ============================================================================
#include "ovrshim.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <direct.h>
#include <string>
#include <vector>

extern const char* ShimModuleDir();

static ActionSetRec* g_theSet = nullptr;
static std::vector<ActionRec*> g_setActions;
static bool g_inputReady = false;

// ---------------------------------------------------------------- Index poses
// Every hand, weapon and aim offset in the mod was tuned on Quest controllers
// under VDXR, i.e. against Meta's OpenXR grip/aim frames. SteamVR's Index
// components are not those frames: "handgrip" sits ~5.9 cm and ~6 deg from
// where a Quest grip pose sits in the same hand, and "tip" is ~4.2 cm from the
// Quest aim origin. Correcting the frame here, once, carries every per-weapon
// offset over unchanged (they are all expressed relative to the grip/aim pose).
//
// Derivation, from SteamVR's own render-model JSONs: both controllers define
// "openxr_handmodel", the frame SteamVR aligns its skeletal hand to, so it is
// the hand-equivalent link between the two devices. Target (Index raw space)
//   = IndexHandModel * inv(TouchHandModel) * TouchOpenXR{Grip,Aim}
// (Touch openxr_grip/openxr_aim are identical on Quest 1/2/3/Pro). Index "tip"
// has the SAME rotation as Index openxr_handmodel, so binding to "tip" and
// applying the residual inv(tip) * target leaves a rotation of exactly
// +60 deg pitch for grip and none for aim; SteamVR resolves "tip" itself, so
// the JSON's Euler-order ambiguity only touches the translation (2.5 mm, the
// two conventions' midpoint is used). Left/right mirror in X. At runtime the
// chain is raw * tip * correction, with tip read from SteamVR's render model.
enum HandPose { HP_NONE = 0, HP_GRIP_L, HP_GRIP_R, HP_AIM_L, HP_AIM_R };

// VR-224: everything Index-specific below (the frame correction, the hold and
// sword trims, the force-sensor grip binding) runs only when the mod says so.
// The mod resolves [Controllers] IndexTuning against the launcher's headset in
// its config load and hands the verdict over in DVR_INDEX_TUNING, set in this
// process before the loader loads the shim. Absent or "0" = off: the shim then
// behaves exactly as it did before, handgrip/tip as bound.
static bool IndexTuningOn()
{
    static int on = -1;
    if (on < 0)
    {
        char v[8] = {};
        const DWORD n = GetEnvironmentVariableA("DVR_INDEX_TUNING", v, sizeof(v));
        on = (n > 0 && n < sizeof(v) && v[0] == '1') ? 1 : 0;
        SLOG("input: Index tuning %s (DVR_INDEX_TUNING=%s, set by the mod from [Controllers] "
             "IndexTuning and the launcher headset)", on ? "ON" : "off", n ? v : "<unset>");
    }
    return on == 1;
}

static int HandPoseFromName(const char* n)
{
    if (!strcmp(n, "pose_l")) return HP_GRIP_L;
    if (!strcmp(n, "pose_r")) return HP_GRIP_R;
    if (!strcmp(n, "aim_l"))  return HP_AIM_L;
    if (!strcmp(n, "aim_r"))  return HP_AIM_R;
    return HP_NONE;
}

// tip -> target, q = x,y,z,w, p in metres (tip frame: +X right, +Y up, -Z out
// of the tip). Grip: +60 deg about X (sin30, cos30).
static M34 IndexPoseCorrection(int hp)
{
    const float sx = (hp == HP_GRIP_L || hp == HP_AIM_L) ? 1.0f : -1.0f;
    if (hp == HP_GRIP_L || hp == HP_GRIP_R)
    {
        const float q[4] = { 0.5f, 0.0f, 0.0f, 0.8660254f };
        const float p[3] = { sx * 0.00580f, -0.07116f, 0.08556f };
        return M34_FromQuatPos(q, p);
    }
    const float q[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float p[3] = { sx * 0.00580f, -0.04116f, -0.00944f };
    return M34_FromQuatPos(q, p);
}

// Hold trim (Index tuning), applied to grip AND aim together (so the drawn weapon and
// the shot stay on one ray), pivoting about the grip point so the hand does not
// move. Measured on this rig with the arm extended and the fingers OPEN, where
// an Index controller swings on its strap: the aim ray read 17-25 deg above and
// 9-13 deg right of the forearm (left hand, two samples). Pitch applies to both
// hands; the yaw only to the left, the one hand with data. A GRIPPED hand will
// now read about this much low - the trade the player chose.
static const float kHoldPitchDeg = -21.0f;      // negative = rotate down
static const float kHoldYawDegLeft = 11.0f;     // positive = rotate left
// Wrist roll about the (already trimmed) pointing axis, so the aim direction is
// unchanged. Positive = counter-clockwise as seen from behind the hand looking
// down the arm. Tuned in the headset: right 60 overshot to 10 o'clock (the drawn
// forearm swings ~2x the roll, it sits off this axis), so 30; left 30 -> 20.
static const float kHoldRollDegLeft = -20.0f;   // clockwise 20
static const float kHoldRollDegRight = 30.0f;   // counter-clockwise 30

static M34 IndexHoldTrim(int hp)
{
    const bool left = (hp == HP_GRIP_L || hp == HP_AIM_L);
    const float sx = left ? 1.0f : -1.0f;
    const float pg[3] = { sx * 0.00580f, -0.07116f, 0.08556f }; // grip point, tip frame
    const float yaw = (left ? kHoldYawDegLeft : 0.0f) * 0.01745329f;
    const float pit = kHoldPitchDeg * 0.01745329f;
    const float qy[4] = { 0.0f, sinf(yaw * 0.5f), 0.0f, cosf(yaw * 0.5f) };
    const float qx[4] = { sinf(pit * 0.5f), 0.0f, 0.0f, cosf(pit * 0.5f) };
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    const float neg[3] = { -pg[0], -pg[1], -pg[2] };
    const float rol = (left ? kHoldRollDegLeft : kHoldRollDegRight) * 0.01745329f;
    const float qz[4] = { 0.0f, 0.0f, sinf(rol * 0.5f), cosf(rol * 0.5f) };
    const M34 R = M34_Mul(M34_Mul(M34_FromQuatPos(qy, zero), M34_FromQuatPos(qx, zero)),
                          M34_FromQuatPos(qz, zero));
    const float id[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    return M34_Mul(M34_Mul(M34_FromQuatPos(id, pg), R), M34_FromQuatPos(id, neg));
}

// Right-hand SWORD trim: the grip (hand + sword) only - the right aim ray is
// left alone. Pivots about the grip point in the hold-trimmed tip frame (-Z
// along the arm, +Y up, so the blade axis is about +Y with the arm straight).
// Tuned in the headset: blade edge faced 11 o'clock on a ceiling clock (12 =
// straight ahead) -> turn it 30 deg right; the tip leaned forward off straight
// up -> tilt it back a little toward the player.
static const float kSwordTurnRightDeg = 30.0f;  // about the blade axis
static const float kSwordTiltBackDeg = 8.0f;    // tip toward the player

static M34 IndexSwordTrim(int hp)
{
    const float id[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float zero[3] = { 0.0f, 0.0f, 0.0f };
    if (hp != HP_GRIP_R) return M34_FromQuatPos(id, zero);
    const float pg[3] = { -0.00580f, -0.07116f, 0.08556f };     // right grip point, tip frame
    const float neg[3] = { -pg[0], -pg[1], -pg[2] };
    const float yaw = -kSwordTurnRightDeg * 0.01745329f;        // +Y rotation turns left
    const float tlt = kSwordTiltBackDeg * 0.01745329f;          // +X rotation tips +Y toward +Z (back)
    const float qy[4] = { 0.0f, sinf(yaw * 0.5f), 0.0f, cosf(yaw * 0.5f) };
    const float qx[4] = { sinf(tlt * 0.5f), 0.0f, 0.0f, cosf(tlt * 0.5f) };
    const M34 R = M34_Mul(M34_FromQuatPos(qx, zero), M34_FromQuatPos(qy, zero));
    return M34_Mul(M34_Mul(M34_FromQuatPos(id, pg), R), M34_FromQuatPos(id, neg));
}

// SteamVR's "tip" transform in the controller's raw space, from the render
// model itself so SteamVR's own Euler convention applies. The fallback is the
// same component read from valve_controller_knu_1_0_{left,right}.json (the two
// Euler readings of it differ by 3.4 deg, so the live query is preferred).
static M34 IndexTipFallback(int hp)
{
    const float s = (hp == HP_GRIP_L || hp == HP_AIM_L) ? 1.0f : -1.0f;
    const float q[4] = { -0.341695f, s * -0.040989f, s * 0.014919f, 0.938798f };
    const float p[3] = { s * 0.006f, -0.015f, 0.020f };
    return M34_FromQuatPos(q, p);
}

// True when the device behind a pose action's current origin is an Index
// controller. Cached per action by origin handle; logged once per change.
// Also resolves the device index and its raw -> tip transform, because the
// correction is built from the RAW device pose, never from the bound pose
// component: SteamVR can keep an autosaved binding from an older shim (it did,
// with grip on "handgrip") and reports no component name to tell them apart.
static bool OriginIsIndex(ActionRec* a, uint64_t origin)
{
    if (!origin) return false;
    if (origin == a->lastOrigin) return a->lastOriginIsIndex;
    a->lastOrigin = origin;
    a->lastOriginIsIndex = false;

    vr::InputOriginInfo_t info = {};
    if (g_vr.input->GetOriginTrackedDeviceInfo(origin, (InputOriginInfo_t*)&info,
            sizeof(info)) != EVRInputError_VRInputError_None)
        return false;
    char type[64] = {};
    ETrackedPropertyError perr = ETrackedPropertyError_TrackedProp_Success;
    g_vr.sys->GetStringTrackedDeviceProperty(info.trackedDeviceIndex,
        ETrackedDeviceProperty_Prop_ControllerType_String, type, sizeof(type), &perr);
    if (perr != ETrackedPropertyError_TrackedProp_Success || strcmp(type, "knuckles"))
    {
        SLOG("input: %s bound to device %u type='%s' -> Index pose correction off",
             a->name, info.trackedDeviceIndex, type);
        return false;
    }
    a->deviceIndex = info.trackedDeviceIndex;

    char model[128] = {};
    g_vr.sys->GetStringTrackedDeviceProperty(info.trackedDeviceIndex,
        ETrackedDeviceProperty_Prop_RenderModelName_String, model, sizeof(model), &perr);
    vr::RenderModel_ControllerMode_State_t mode = {};
    vr::RenderModel_ComponentState_t cs = {};
    const bool live = g_vr.rm && model[0] &&
        g_vr.rm->GetComponentStateForDevicePath(model, (char*)"tip", info.devicePath,
            (RenderModel_ControllerMode_State_t*)&mode, (RenderModel_ComponentState_t*)&cs);
    a->rawToTip = live ? M34_FromVr(cs.mTrackingToComponentLocal)
                       : IndexTipFallback(a->handPose);
    a->lastOriginIsIndex = true;

    float q[4], p[3];
    M34_ToQuatPos(a->rawToTip, q, p);
    SLOG("input: %s bound to device %u type='knuckles' model='%s' -> Index pose "
         "correction ON, raw->tip from %s: q=(%.4f %.4f %.4f %.4f) p=(%.4f %.4f %.4f)",
         a->name, info.trackedDeviceIndex, model, live ? "SteamVR" : "built-in fallback",
         q[0], q[1], q[2], q[3], p[0], p[1], p[2]);
    return true;
}

// ---------------------------------------------------------------- creation
OVRSHIM_FN(shim_CreateActionSet)(
    XrInstance, const XrActionSetCreateInfo* info, XrActionSet* out)
{
    if (!info || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionSetRec* s = new ActionSetRec();
    strncpy_s(s->name, info->actionSetName, _TRUNCATE);
    g_theSet = s;
    *out = (XrActionSet)s;
    SLOG("xrCreateActionSet '%s'", s->name);
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_CreateAction)(
    XrActionSet set, const XrActionCreateInfo* info, XrAction* out)
{
    if (!set || !info || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = new ActionRec();
    a->set = (ActionSetRec*)set;
    strncpy_s(a->name, info->actionName, _TRUNCATE);
    a->type = info->actionType;
    a->handPose = HandPoseFromName(a->name);
    g_setActions.push_back(a);
    *out = (XrAction)a;
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_SuggestInteractionProfileBindings)(
    XrInstance, const XrInteractionProfileSuggestedBinding* sb)
{
    if (!sb) return XR_ERROR_VALIDATION_FAILURE;
    SLOG("xrSuggestInteractionProfileBindings profile='%s' count=%u (noted; shim "
         "authors its own SteamVR bindings)", PathToString(sb->interactionProfile),
         sb->countSuggestedBindings);
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_CreateActionSpace)(
    XrSession, const XrActionSpaceCreateInfo* info, XrSpace* out)
{
    if (!info || !out) return XR_ERROR_VALIDATION_FAILURE;
    SpaceRec* s = new SpaceRec();
    s->kind = SPACE_ACTION;
    s->action = (ActionRec*)info->action;
    *out = (XrSpace)s;
    return XR_SUCCESS;
}

// ---------------------------------------------------------------- manifest
// Index controllers. Grip via the grip FORCE SENSOR (squeeze pressure, analog;
// mode force_sensor, input force) rather than trigger-mode "pull" - the binding an
// Index player chose in SteamVR's binding UI. VR-224: only with the Index tuning
// on; otherwise KnucklesBindings() puts the trigger-mode pull back. Menu on a firm left
// trackpad click. Thumbrest is deliberately ABSENT (Index has none) - the
// mod's stick-click fallback keeps the ammo radial reachable.
static const char* kBindingsKnuckles = R"JSON({
  "bindings": {
    "/actions/gameplay": {
      "sources": [
        { "path": "/user/hand/left/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/move" },
                      "click":    { "output": "/actions/gameplay/in/stick_l" } } },
        { "path": "/user/hand/right/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/look" },
                      "click":    { "output": "/actions/gameplay/in/stick_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/plasmid" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/fire" } } },
        { "path": "/user/hand/left/input/grip", "mode": "force_sensor",
          "inputs": { "force": { "output": "/actions/gameplay/in/grip_l" } } },
        { "path": "/user/hand/right/input/grip", "mode": "force_sensor",
          "inputs": { "force": { "output": "/actions/gameplay/in/grip_r" } } },
        { "path": "/user/hand/right/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_a" } } },
        { "path": "/user/hand/right/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_b" } } },
        { "path": "/user/hand/left/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_x" } } },
        { "path": "/user/hand/left/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_y" } } },
        { "path": "/user/hand/left/input/trackpad", "mode": "trackpad",
          "inputs": { "click": { "output": "/actions/gameplay/in/menu" } } }
      ],
      "poses": [
        { "output": "/actions/gameplay/in/aim_l",  "path": "/user/hand/left/pose/tip" },
        { "output": "/actions/gameplay/in/aim_r",  "path": "/user/hand/right/pose/tip" },
        { "output": "/actions/gameplay/in/pose_l", "path": "/user/hand/left/pose/handgrip" },
        { "output": "/actions/gameplay/in/pose_r", "path": "/user/hand/right/pose/handgrip" }
      ]
    }
  },
  "controller_type": "knuckles",
  "description": "DishonoredVR shim default bindings for Index controllers",
  "name": "DishonoredVR (shim) Index bindings"
}
)JSON";

// The knuckles binding as written: the force-sensor grip with the Index tuning,
// the trigger-mode pull (the binding every Index player had before VR-224)
// without. Each grip source's mode and input are swapped in place, matched one
// line at a time so the source file's line endings cannot matter.
static std::string KnucklesBindings()
{
    std::string b = kBindingsKnuckles;
    if (IndexTuningOn()) return b;
    for (const char* hand : { "left", "right" })
    {
        char src[96];
        _snprintf_s(src, sizeof(src), _TRUNCATE, "\"/user/hand/%s/input/grip\", \"mode\": \"force_sensor\"", hand);
        const size_t at = b.find(src);
        const size_t in = at == std::string::npos ? at : b.find("\"force\":", at);
        if (in == std::string::npos) {
            SLOG("input: knuckles binding: %s grip source not found - left as force_sensor", hand);
            continue;
        }
        b.replace(in, 8, "\"pull\":");                              // input first: `at` stays valid
        b.replace(b.find("\"force_sensor\"", at), 14, "\"trigger\"");
    }
    return b;
}

// Vive wands. Trackpads stand in for both sticks; grip is a digital squeeze
// bound to the FLOAT grip actions (the shim's digital fallback read converts
// it to 0/1). Right menu doubles as A (use/interact); B/X/Y have no physical
// home - the SteamVR binding UI is the remedy.
static const char* kBindingsVive = R"JSON({
  "bindings": {
    "/actions/gameplay": {
      "sources": [
        { "path": "/user/hand/left/input/trackpad", "mode": "trackpad",
          "inputs": { "position": { "output": "/actions/gameplay/in/move" },
                      "click":    { "output": "/actions/gameplay/in/stick_l" } } },
        { "path": "/user/hand/right/input/trackpad", "mode": "trackpad",
          "inputs": { "position": { "output": "/actions/gameplay/in/look" },
                      "click":    { "output": "/actions/gameplay/in/stick_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/plasmid" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/fire" } } },
        { "path": "/user/hand/left/input/grip", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/grip_l" } } },
        { "path": "/user/hand/right/input/grip", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/grip_r" } } },
        { "path": "/user/hand/left/input/application_menu", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/menu" } } },
        { "path": "/user/hand/right/input/application_menu", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_a" } } }
      ],
      "poses": [
        { "output": "/actions/gameplay/in/aim_l",  "path": "/user/hand/left/pose/tip" },
        { "output": "/actions/gameplay/in/aim_r",  "path": "/user/hand/right/pose/tip" },
        { "output": "/actions/gameplay/in/pose_l", "path": "/user/hand/left/pose/handgrip" },
        { "output": "/actions/gameplay/in/pose_r", "path": "/user/hand/right/pose/handgrip" }
      ]
    }
  },
  "controller_type": "vive_controller",
  "description": "DishonoredVR shim bindings for Vive wands (rebind B/X/Y in the SteamVR controller UI)",
  "name": "DishonoredVR (shim) Vive wand bindings"
}
)JSON";

// Touch under SteamVR (Quest via Link/Steam Link etc). Mirrors the mod's
// native touch table 1:1 including the thumbrest ammo modifier.
static const char* kBindingsTouch = R"JSON({
  "bindings": {
    "/actions/gameplay": {
      "sources": [
        { "path": "/user/hand/left/input/joystick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/move" },
                      "click":    { "output": "/actions/gameplay/in/stick_l" } } },
        { "path": "/user/hand/right/input/joystick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/look" },
                      "click":    { "output": "/actions/gameplay/in/stick_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/plasmid" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/fire" } } },
        { "path": "/user/hand/left/input/grip", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/grip_l" } } },
        { "path": "/user/hand/right/input/grip", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/grip_r" } } },
        { "path": "/user/hand/right/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_a" } } },
        { "path": "/user/hand/right/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_b" } } },
        { "path": "/user/hand/left/input/x", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_x" } } },
        { "path": "/user/hand/left/input/y", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_y" } } },
        { "path": "/user/hand/left/input/application_menu", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/menu" } } },
        { "path": "/user/hand/left/input/thumbrest", "mode": "button",
          "inputs": { "touch": { "output": "/actions/gameplay/in/thumbrest_l" } } },
        { "path": "/user/hand/right/input/thumbrest", "mode": "button",
          "inputs": { "touch": { "output": "/actions/gameplay/in/thumbrest_r" } } }
      ],
      "poses": [
        { "output": "/actions/gameplay/in/aim_l",  "path": "/user/hand/left/pose/tip" },
        { "output": "/actions/gameplay/in/aim_r",  "path": "/user/hand/right/pose/tip" },
        { "output": "/actions/gameplay/in/pose_l", "path": "/user/hand/left/pose/handgrip" },
        { "output": "/actions/gameplay/in/pose_r", "path": "/user/hand/right/pose/handgrip" }
      ]
    }
  },
  "controller_type": "oculus_touch",
  "description": "DishonoredVR shim bindings for Touch controllers",
  "name": "DishonoredVR (shim) Touch bindings"
}
)JSON";

// Windows Mixed Reality / Reverb G2. No face buttons: trackpad clicks feed A
// (right) and X (left), matching the mod's native WMR profile; B/Y unbound.
// UNTESTED on hardware - treat as provisional (donor's warning carried).
static const char* kBindingsWmr = R"JSON({
  "bindings": {
    "/actions/gameplay": {
      "sources": [
        { "path": "/user/hand/left/input/joystick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/move" },
                      "click":    { "output": "/actions/gameplay/in/stick_l" } } },
        { "path": "/user/hand/right/input/joystick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/gameplay/in/look" },
                      "click":    { "output": "/actions/gameplay/in/stick_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/plasmid" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/gameplay/in/fire" } } },
        { "path": "/user/hand/left/input/grip", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/grip_l" } } },
        { "path": "/user/hand/right/input/grip", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/grip_r" } } },
        { "path": "/user/hand/right/input/trackpad", "mode": "trackpad",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_a" } } },
        { "path": "/user/hand/left/input/trackpad", "mode": "trackpad",
          "inputs": { "click": { "output": "/actions/gameplay/in/btn_x" } } },
        { "path": "/user/hand/left/input/application_menu", "mode": "button",
          "inputs": { "click": { "output": "/actions/gameplay/in/menu" } } }
      ],
      "poses": [
        { "output": "/actions/gameplay/in/aim_l",  "path": "/user/hand/left/pose/tip" },
        { "output": "/actions/gameplay/in/aim_r",  "path": "/user/hand/right/pose/tip" },
        { "output": "/actions/gameplay/in/pose_l", "path": "/user/hand/left/pose/handgrip" },
        { "output": "/actions/gameplay/in/pose_r", "path": "/user/hand/right/pose/handgrip" }
      ]
    }
  },
  "controller_type": "holographic_controller",
  "description": "DishonoredVR shim bindings for Windows Mixed Reality controllers (provisional, untested)",
  "name": "DishonoredVR (shim) WMR bindings"
}
)JSON";

static bool WriteTextFile(const char* path, const char* text)
{
    FILE* f = nullptr;
    fopen_s(&f, path, "w");
    if (!f) { SLOG("!!! input: cannot write %s", path); return false; }
    fputs(text, f);
    fclose(f);
    return true;
}

static const char* XrTypeToManifestType(XrActionType t)
{
    switch (t)
    {
    case XR_ACTION_TYPE_BOOLEAN_INPUT:  return "boolean";
    case XR_ACTION_TYPE_FLOAT_INPUT:    return "vector1";
    case XR_ACTION_TYPE_VECTOR2F_INPUT: return "vector2";
    case XR_ACTION_TYPE_POSE_INPUT:     return "pose";
    case XR_ACTION_TYPE_VIBRATION_OUTPUT: return "vibration";
    default: return "boolean";
    }
}

bool InputShim_Attach(ActionSetRec* set, ActionRec** actions, int actionCount)
{
    char dir[MAX_PATH];
    _snprintf_s(dir, MAX_PATH, _TRUNCATE, "%s\\openvr_input", ShimModuleDir());
    _mkdir(dir);

    // ---- actions.json (generated from the live action list) ----------------
    // Regenerated every launch: hand-edits never stick, but a moved game
    // folder self-heals. The binding files are the user-visible surface;
    // SteamVR's own controller UI can override them per-user.
    char p[MAX_PATH];
    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\actions.json", dir);
    FILE* f = nullptr;
    fopen_s(&f, p, "w");
    if (!f) { SLOG("!!! input: cannot write %s", p); return false; }

    fprintf(f, "{\n  \"default_bindings\": [\n"
        "    { \"controller_type\": \"knuckles\", \"binding_url\": \"bindings_knuckles.json\" },\n"
        "    { \"controller_type\": \"vive_controller\", \"binding_url\": \"bindings_vive_controller.json\" },\n"
        "    { \"controller_type\": \"oculus_touch\", \"binding_url\": \"bindings_oculus_touch.json\" },\n"
        "    { \"controller_type\": \"holographic_controller\", \"binding_url\": \"bindings_holographic_controller.json\" }\n"
        "  ],\n  \"actions\": [\n");
    for (int i = 0; i < actionCount; ++i)
    {
        const bool out = actions[i]->type == XR_ACTION_TYPE_VIBRATION_OUTPUT;
        fprintf(f, "    { \"name\": \"/actions/%s/%s/%s\", \"type\": \"%s\", \"requirement\": \"optional\" }%s\n",
            set->name, out ? "out" : "in", actions[i]->name,
            XrTypeToManifestType(actions[i]->type),
            (i + 1 < actionCount) ? "," : "");
    }
    fprintf(f, "  ],\n  \"action_sets\": [\n"
        "    { \"name\": \"/actions/%s\", \"usage\": \"leftright\" }\n"
        "  ],\n  \"localization\": [\n    { \"language_tag\": \"en_US\"", set->name);
    for (int i = 0; i < actionCount; ++i)
    {
        const bool out = actions[i]->type == XR_ACTION_TYPE_VIBRATION_OUTPUT;
        fprintf(f, ",\n      \"/actions/%s/%s/%s\": \"%s\"",
            set->name, out ? "out" : "in", actions[i]->name, actions[i]->name);
    }
    fprintf(f, "\n    }\n  ]\n}\n");
    fclose(f);

    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\bindings_knuckles.json", dir);
    WriteTextFile(p, KnucklesBindings().c_str());
    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\bindings_vive_controller.json", dir);
    WriteTextFile(p, kBindingsVive);
    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\bindings_oculus_touch.json", dir);
    WriteTextFile(p, kBindingsTouch);
    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\bindings_holographic_controller.json", dir);
    WriteTextFile(p, kBindingsWmr);

    _snprintf_s(p, MAX_PATH, _TRUNCATE, "%s\\actions.json", dir);
    EVRInputError ie = g_vr.input->SetActionManifestPath((char*)p);
    if (ie != EVRInputError_VRInputError_None)
    {
        SLOG("!!! input: SetActionManifestPath('%s') -> %d", p, (int)ie);
        return false;
    }
    SLOG("input: action manifest set: %s", p);

    char handlePath[160];
    _snprintf_s(handlePath, sizeof(handlePath), _TRUNCATE, "/actions/%s", set->name);
    ie = g_vr.input->GetActionSetHandle(handlePath, &set->vrHandle);
    if (ie != EVRInputError_VRInputError_None || !set->vrHandle)
    {
        SLOG("!!! input: GetActionSetHandle -> %d", (int)ie);
        return false;
    }

    for (int i = 0; i < actionCount; ++i)
    {
        const bool out = actions[i]->type == XR_ACTION_TYPE_VIBRATION_OUTPUT;
        _snprintf_s(handlePath, sizeof(handlePath), _TRUNCATE, "/actions/%s/%s/%s",
                  set->name, out ? "out" : "in", actions[i]->name);
        ie = g_vr.input->GetActionHandle(handlePath, &actions[i]->vrHandle);
        if (ie != EVRInputError_VRInputError_None)
            SLOG("!!! input: GetActionHandle('%s') -> %d", handlePath, (int)ie);
    }

    set->attached = true;
    g_inputReady = true;
    SLOG("input: %d actions attached to SteamVR input", actionCount);
    return true;
}

OVRSHIM_FN(shim_AttachSessionActionSets)(
    XrSession, const XrSessionActionSetsAttachInfo* info)
{
    if (!info || info->countActionSets < 1) return XR_ERROR_VALIDATION_FAILURE;
    ActionSetRec* set = (ActionSetRec*)info->actionSets[0];
    if (!set) return XR_ERROR_HANDLE_INVALID;
    if (!InputShim_Attach(set, g_setActions.data(), (int)g_setActions.size()))
    {
        // Attach "succeeds" so the mod keeps running; every action just reads
        // inactive, which the mod already handles by publishing a neutral pad.
        SLOG("!!! input: attach degraded - controllers will be inactive");
    }
    return XR_SUCCESS;
}

// ---------------------------------------------------------------- sync/state
OVRSHIM_FN(shim_SyncActions)(XrSession, const XrActionsSyncInfo* info)
{
    if (!info) return XR_ERROR_VALIDATION_FAILURE;
    if (!g_inputReady) return XR_SUCCESS;

    if (g_vr.ovl && g_vr.ovl->IsDashboardVisible())
        return XR_SESSION_NOT_FOCUSED;

    vr::VRActiveActionSet_t as = {};
    as.ulActionSet = ((ActionSetRec*)info->activeActionSets[0].actionSet)->vrHandle;
    as.ulRestrictedToDevice = vr::k_ulInvalidInputValueHandle;
    as.nPriority = 0;
    const EVRInputError ie = g_vr.input->UpdateActionState(
        (VRActiveActionSet_t*)&as, sizeof(vr::VRActiveActionSet_t), 1);
    if (ie != EVRInputError_VRInputError_None)
    {
        static EVRInputError last = EVRInputError_VRInputError_None;
        if (ie != last) { last = ie; SLOG("!!! input: UpdateActionState -> %d", (int)ie); }
    }
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_GetActionStateBoolean)(
    XrSession, const XrActionStateGetInfo* gi, XrActionStateBoolean* out)
{
    if (!gi || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = (ActionRec*)gi->action;
    out->isActive = XR_FALSE;
    out->currentState = XR_FALSE;
    out->changedSinceLastSync = XR_FALSE;
    out->lastChangeTime = 0;
    if (!a || !a->vrHandle || !g_inputReady) return XR_SUCCESS;

    vr::InputDigitalActionData_t d = {};
    if (g_vr.input->GetDigitalActionData(a->vrHandle, (InputDigitalActionData_t*)&d,
            sizeof(d), vr::k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None
        && d.bActive)
    {
        out->isActive = XR_TRUE;
        out->currentState = d.bState ? XR_TRUE : XR_FALSE;
        out->changedSinceLastSync = d.bChanged ? XR_TRUE : XR_FALSE;
        out->lastChangeTime = g_st.lastPredictedTime;
    }
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_GetActionStateFloat)(
    XrSession, const XrActionStateGetInfo* gi, XrActionStateFloat* out)
{
    if (!gi || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = (ActionRec*)gi->action;
    out->isActive = XR_FALSE;
    out->currentState = 0.f;
    out->changedSinceLastSync = XR_FALSE;
    out->lastChangeTime = 0;
    if (!a || !a->vrHandle || !g_inputReady) return XR_SUCCESS;

    vr::InputAnalogActionData_t d = {};
    if (g_vr.input->GetAnalogActionData(a->vrHandle, (InputAnalogActionData_t*)&d,
            sizeof(d), vr::k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None
        && d.bActive)
    {
        out->isActive = XR_TRUE;
        out->currentState = d.x;
        out->changedSinceLastSync = (d.deltaX != 0.f) ? XR_TRUE : XR_FALSE;
        out->lastChangeTime = g_st.lastPredictedTime;
        return XR_SUCCESS;
    }

    // Digital fallback (delta vs donor): a button bound to a vector1 action
    // (Vive/WMR grip -> the mod's FLOAT grip actions) reads inactive through
    // the analog path on some SteamVR versions. Convert 0/1 here so the
    // weapon/plasmid radials still open - the donor documented this as the
    // "WMR radials never open" hazard and shipped it open.
    vr::InputDigitalActionData_t dd = {};
    if (g_vr.input->GetDigitalActionData(a->vrHandle, (InputDigitalActionData_t*)&dd,
            sizeof(dd), vr::k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None
        && dd.bActive)
    {
        out->isActive = XR_TRUE;
        out->currentState = dd.bState ? 1.f : 0.f;
        out->changedSinceLastSync = dd.bChanged ? XR_TRUE : XR_FALSE;
        out->lastChangeTime = g_st.lastPredictedTime;
    }
    return XR_SUCCESS;
}

OVRSHIM_FN(shim_GetActionStateVector2f)(
    XrSession, const XrActionStateGetInfo* gi, XrActionStateVector2f* out)
{
    if (!gi || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = (ActionRec*)gi->action;
    out->isActive = XR_FALSE;
    out->currentState.x = out->currentState.y = 0.f;
    out->changedSinceLastSync = XR_FALSE;
    out->lastChangeTime = 0;
    if (!a || !a->vrHandle || !g_inputReady) return XR_SUCCESS;

    vr::InputAnalogActionData_t d = {};
    if (g_vr.input->GetAnalogActionData(a->vrHandle, (InputAnalogActionData_t*)&d,
            sizeof(d), vr::k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None
        && d.bActive)
    {
        out->isActive = XR_TRUE;
        out->currentState.x = d.x;
        out->currentState.y = d.y;
        out->changedSinceLastSync = (d.deltaX != 0.f || d.deltaY != 0.f) ? XR_TRUE : XR_FALSE;
        out->lastChangeTime = g_st.lastPredictedTime;
    }
    return XR_SUCCESS;
}

// NEW vs donor: the mod's locate_hand refuses to locate a hand whose pose
// action is not active, so without this the hands never appear. Called four
// times per frame - GetPoseActionDataForNextFrame is a cheap local read.
OVRSHIM_FN(shim_GetActionStatePose)(
    XrSession, const XrActionStateGetInfo* gi, XrActionStatePose* out)
{
    if (!gi || !out) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = (ActionRec*)gi->action;
    out->isActive = XR_FALSE;
    if (!a || !a->vrHandle || !g_inputReady) return XR_SUCCESS;

    vr::InputPoseActionData_t pd = {};
    if (g_vr.input->GetPoseActionDataForNextFrame(a->vrHandle,
            ETrackingUniverseOrigin_TrackingUniverseStanding,
            (InputPoseActionData_t*)&pd, sizeof(pd),
            vr::k_ulInvalidInputValueHandle) == EVRInputError_VRInputError_None
        && pd.bActive)
        out->isActive = XR_TRUE;
    return XR_SUCCESS;
}

// ---------------------------------------------------------------- haptics
// The mod does not call this today, but the GIPA-based dispatch makes it free
// to keep (in the donor's static-import model a MISSING export here stopped
// the whole mod from loading - error 127, 2026-08-12).
//
// OpenXR gives nanoseconds and may ask for XR_FREQUENCY_UNSPECIFIED; SteamVR
// wants seconds and a real frequency. 160 Hz is the mid-band both Touch and
// Index reproduce well. XR_MIN_HAPTIC_DURATION (-1) -> 40 ms.
OVRSHIM_FN(shim_ApplyHapticFeedback)(
    XrSession, const XrHapticActionInfo* hi, const XrHapticBaseHeader* header)
{
    if (!hi || !header) return XR_ERROR_VALIDATION_FAILURE;
    ActionRec* a = (ActionRec*)hi->action;
    if (!a || !a->vrHandle || !g_inputReady) return XR_SUCCESS;
    if (header->type != XR_TYPE_HAPTIC_VIBRATION) return XR_ERROR_VALIDATION_FAILURE;

    const XrHapticVibration* v = (const XrHapticVibration*)header;

    float seconds = (v->duration == XR_MIN_HAPTIC_DURATION)
        ? 0.04f : (float)((double)v->duration / 1e9);
    if (seconds < 0.01f) seconds = 0.01f;
    if (seconds > 2.0f)  seconds = 2.0f;      // a stuck buzz is worse than none

    float freq = v->frequency;
    if (freq == XR_FREQUENCY_UNSPECIFIED || freq <= 0.f) freq = 160.0f;

    float amp = v->amplitude;
    if (amp < 0.f) amp = 0.f;
    if (amp > 1.f) amp = 1.f;

    const EVRInputError ie = g_vr.input->TriggerHapticVibrationAction(
        a->vrHandle, 0.0f, seconds, freq, amp, vr::k_ulInvalidInputValueHandle);

    static bool told = false;
    if (!told)
    {
        told = true;
        SLOG("input: first haptic pulse -> %d  (%.0f Hz, %.2f amp, %.0f ms). "
             "Result 0 means SteamVR ACCEPTED it; if nothing is felt the binding "
             "for /actions/gameplay/out/* is not live on this controller.",
             (int)ie, freq, amp, seconds * 1000.0f);
    }
    return XR_SUCCESS;
}

// ---------------------------------------------------------------- locate
static bool SpacePoseInOrigin(SpaceRec* s, M34* out)
{
    switch (s->kind)
    {
    case SPACE_REF_LOCAL:
        *out = M34_Identity();
        return true;
    case SPACE_REF_VIEW:
        if (!g_st.hmdValid) return false;
        *out = g_st.hmd;
        return true;
    case SPACE_ACTION:
    {
        if (!s->action || !s->action->vrHandle || !g_inputReady || !g_st.haveOrigin)
            return false;
        vr::InputPoseActionData_t pd = {};
        const EVRInputError ie = g_vr.input->GetPoseActionDataForNextFrame(
            s->action->vrHandle, ETrackingUniverseOrigin_TrackingUniverseStanding,
            (InputPoseActionData_t*)&pd, sizeof(pd), vr::k_ulInvalidInputValueHandle);
        if (ie != EVRInputError_VRInputError_None || !pd.bActive || !pd.pose.bPoseIsValid)
            return false;
        *out = M34_Mul(g_st.originInv, M34_FromVr(pd.pose.mDeviceToAbsoluteTracking));
        // Index: rebuild from the raw device pose (same WaitGetPoses
        // prediction as the action read), whatever component is bound.
        M34 raw;
        if (s->action->handPose && IndexTuningOn() && OriginIsIndex(s->action, pd.activeOrigin) &&
            Shim_RenderPose(s->action->deviceIndex, &raw))
            *out = M34_Mul(g_st.originInv,
                M34_Mul(M34_Mul(M34_Mul(M34_Mul(raw, s->action->rawToTip),
                                        IndexHoldTrim(s->action->handPose)),
                                IndexSwordTrim(s->action->handPose)),
                        IndexPoseCorrection(s->action->handPose)));
        return true;
    }
    }
    return false;
}

OVRSHIM_FN(shim_LocateSpace)(
    XrSpace space, XrSpace base, XrTime, XrSpaceLocation* out)
{
    if (!space || !base || !out) return XR_ERROR_VALIDATION_FAILURE;
    out->locationFlags = 0;
    out->pose.orientation = { 0, 0, 0, 1 };
    out->pose.position = { 0, 0, 0 };

    M34 t, b;
    if (!SpacePoseInOrigin((SpaceRec*)space, &t)) return XR_SUCCESS;
    if (!SpacePoseInOrigin((SpaceRec*)base, &b)) return XR_SUCCESS;

    const M34 rel = M34_Mul(M34_InvRigid(b), t);
    float q[4], p[3];
    M34_ToQuatPos(rel, q, p);
    out->pose.orientation = { q[0], q[1], q[2], q[3] };
    out->pose.position = { p[0], p[1], p[2] };
    out->locationFlags = XR_SPACE_LOCATION_ORIENTATION_VALID_BIT |
                         XR_SPACE_LOCATION_POSITION_VALID_BIT |
                         XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT |
                         XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    return XR_SUCCESS;
}
