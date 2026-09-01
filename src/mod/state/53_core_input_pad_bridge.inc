// State chunk 53 for core/input/pad_bridge - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static const uintptr_t kXISetSlot = 0x00f946c0; // IAT slot: xinput1_3 ord 3
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE*);
typedef DWORD (WINAPI *XInputSetState_t)(DWORD, XINPUT_VIBRATION*);
static XInputGetState_t g_realXIGetState = NULL;
static XInputSetState_t g_realXISetState = NULL;
static bool     g_padHooked   = false;
static bool     g_padHookTried= false;
static volatile bool g_padActive = false;  // VR controllers currently feeding it
static CRITICAL_SECTION g_padLock;         // guards g_padState
static XINPUT_STATE g_padState;            // the synthesized pad (slot 0)
static DWORD    g_padPacket = 0;
static volatile LONG g_padPolls = 0;       // game polls this window (heartbeat)
// (g_ctrlIdx moved up with the pose globals — Stage 7.3 needs it earlier)
static uint64_t g_padBtnLast[2] = {0, 0};  // for raw-mask change logging

#define IDXBTN(id) (1ull << (id))

// --- Stage 6.4: MODERN SteamVR input (action manifest) -----------------------
// The legacy API emulates a Vive wand: no A buttons, trackpad and thumbstick
// merged, stick "clicks" firing at full tilt. The action system exposes every
// Index input separately, and lets the user rebind in SteamVR's controller
// settings UI. We write our manifest + default bindings next to the DLL at
// startup and read actions instead of GetControllerState. Legacy stays as the
// automatic fallback (non-Index controllers, old SteamVR).
static struct VR_IVRInput_FnTable* g_input = NULL;
static bool g_useActions = false;
static VRActionSetHandle_t g_actSet = 0;
static VRActionHandle_t g_actMove = 0, g_actTurn = 0, g_actHandR = 0,
    g_actHandL = 0, g_actJump = 0, g_actStealth = 0, g_actInteract = 0,
    g_actPause = 0, g_actWheel = 0, g_actZoom = 0, g_actChoke = 0,
    g_actLeanY = 0, g_actSneak = 0, g_actRecenter = 0,
    g_actJournal = 0, g_actHealth = 0,
    g_actHapL = 0, g_actHapR = 0;

static const char* kActionManifest = R"JSON({
  "default_bindings": [
    { "controller_type": "knuckles", "binding_url": "vr_bindings_knuckles.json" },
    { "controller_type": "oculus_touch", "binding_url": "vr_bindings_touch.json" }
  ],
  "action_sets": [ { "name": "/actions/main", "usage": "leftright" } ],
  "actions": [
    { "name": "/actions/main/in/move",     "type": "vector2" },
    { "name": "/actions/main/in/turn",     "type": "vector2" },
    { "name": "/actions/main/in/hand_r",   "type": "vector1" },
    { "name": "/actions/main/in/hand_l",   "type": "vector1" },
    { "name": "/actions/main/in/jump",     "type": "boolean" },
    { "name": "/actions/main/in/stealth",  "type": "boolean" },
    { "name": "/actions/main/in/interact", "type": "boolean" },
    { "name": "/actions/main/in/pause",    "type": "boolean" },
    { "name": "/actions/main/in/wheel",    "type": "boolean" },
    { "name": "/actions/main/in/zoom",     "type": "boolean" },
    { "name": "/actions/main/in/choke",    "type": "boolean" },
    { "name": "/actions/main/in/lean_y",   "type": "boolean" },
    { "name": "/actions/main/in/sneak",    "type": "boolean" },
    { "name": "/actions/main/in/recenter", "type": "boolean" },
    { "name": "/actions/main/in/journal",  "type": "boolean" },
    { "name": "/actions/main/in/health",   "type": "boolean" },
    { "name": "/actions/main/out/haptic_l", "type": "vibration" },
    { "name": "/actions/main/out/haptic_r", "type": "vibration" }
  ],
  "localization": [
    { "language_tag": "en_US",
      "/actions/main": "Dishonored VR",
      "/actions/main/in/move": "Move",
      "/actions/main/in/turn": "Turn Body",
      "/actions/main/in/hand_r": "Right Hand (Sword)",
      "/actions/main/in/hand_l": "Left Hand (Power/Gun)",
      "/actions/main/in/jump": "Jump / Menu Confirm",
      "/actions/main/in/stealth": "Stealth / Menu Back",
      "/actions/main/in/interact": "Interact",
      "/actions/main/in/pause": "Pause Menu",
      "/actions/main/in/wheel": "Power Wheel",
      "/actions/main/in/zoom": "Zoom",
      "/actions/main/in/choke": "Choke / Attack Alt",
      "/actions/main/in/lean_y": "Adrenaline",
      "/actions/main/in/sneak": "Sneak Toggle",
      "/actions/main/in/recenter": "Recenter Lean Tracking",
      "/actions/main/in/journal": "Journal / Powers",
      "/actions/main/in/health": "Health Elixir (hold)",
      "/actions/main/out/haptic_l": "Left Haptic",
      "/actions/main/out/haptic_r": "Right Haptic" }
  ]
}
)JSON";

static const char* kBindingsKnuckles = R"JSON({
  "controller_type": "knuckles",
  "description": "Dishonored VR defaults for Index controllers",
  "name": "Dishonored VR (Index)",
  "bindings": {
    "/actions/main": {
      "sources": [
        { "path": "/user/hand/left/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/main/in/move" },
                      "click":    { "output": "/actions/main/in/sneak" } } },
        { "path": "/user/hand/right/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/main/in/turn" },
                      "click":    { "output": "/actions/main/in/health" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/main/in/hand_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/main/in/hand_l" } } },
        { "path": "/user/hand/right/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/jump" } } },
        { "path": "/user/hand/right/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/stealth" } } },
        { "path": "/user/hand/left/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/interact" } } },
        { "path": "/user/hand/left/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/pause" } } },
        { "path": "/user/hand/left/input/trackpad", "mode": "trackpad",
          "inputs": { "click": { "output": "/actions/main/in/wheel" } } },
        { "path": "/user/hand/right/input/trackpad", "mode": "trackpad",
          "inputs": { "click": { "output": "/actions/main/in/zoom" } } },
        { "path": "/user/hand/right/input/grip", "mode": "trigger",
          "parameters": { "click_activate_threshold": "0.9",
                          "click_deactivate_threshold": "0.7" },
          "inputs": { "click": { "output": "/actions/main/in/choke" } } }
      ],
      "haptics": [
        { "output": "/actions/main/out/haptic_l", "path": "/user/hand/left/output/haptic" },
        { "output": "/actions/main/out/haptic_r", "path": "/user/hand/right/output/haptic" }
      ]
    }
  }
}
)JSON";

// 37.6: Quest Touch defaults - the knuckles layout translated to hardware
// with no trackpads. Wheel moves to the LEFT GRIP (hard squeeze); zoom,
// journal, recenter and adrenaline ship unbound (SteamVR bindings UI).
// Health elixir keeps its 36.7 home: right stick click, short hold.
static const char* kBindingsTouch = R"JSON({
  "controller_type": "oculus_touch",
  "description": "Dishonored VR defaults for Quest/Touch controllers",
  "name": "Dishonored VR (Touch)",
  "bindings": {
    "/actions/main": {
      "sources": [
        { "path": "/user/hand/left/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/main/in/move" },
                      "click":    { "output": "/actions/main/in/sneak" } } },
        { "path": "/user/hand/right/input/thumbstick", "mode": "joystick",
          "inputs": { "position": { "output": "/actions/main/in/turn" },
                      "click":    { "output": "/actions/main/in/health" } } },
        { "path": "/user/hand/right/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/main/in/hand_r" } } },
        { "path": "/user/hand/left/input/trigger", "mode": "trigger",
          "inputs": { "pull": { "output": "/actions/main/in/hand_l" } } },
        { "path": "/user/hand/right/input/a", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/jump" } } },
        { "path": "/user/hand/right/input/b", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/stealth" } } },
        { "path": "/user/hand/left/input/x", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/interact" } } },
        { "path": "/user/hand/left/input/y", "mode": "button",
          "inputs": { "click": { "output": "/actions/main/in/pause" } } },
        { "path": "/user/hand/left/input/grip", "mode": "trigger",
          "parameters": { "click_activate_threshold": "0.9",
                          "click_deactivate_threshold": "0.7" },
          "inputs": { "click": { "output": "/actions/main/in/wheel" } } },
        { "path": "/user/hand/right/input/grip", "mode": "trigger",
          "parameters": { "click_activate_threshold": "0.9",
                          "click_deactivate_threshold": "0.7" },
          "inputs": { "click": { "output": "/actions/main/in/choke" } } }
      ],
      "haptics": [
        { "output": "/actions/main/out/haptic_l", "path": "/user/hand/left/output/haptic" },
        { "output": "/actions/main/out/haptic_r", "path": "/user/hand/right/output/haptic" }
      ]
    }
  }
}
)JSON";

// digital action pressed? (also ORs its bActive into *activeOut)
static void InitActionInput();        // defined below; re-runnable on failure
