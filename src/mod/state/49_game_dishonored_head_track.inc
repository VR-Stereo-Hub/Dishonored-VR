// State chunk 49 for game/dishonored/head_track - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 30.50: enforce the FOV lever on every script dispatch (see the block comment
// at g_fovLever). Cheap by construction: an early-out when the lever is off,
// plain float stores otherwise, and the expensive camera identity check runs
// only once per 256 writes.
// 30.51: the lever is a RATIO, not an absolute. Forcing a fixed number every
// dispatch (30.50) also stomped the engine's OWN fov intent - the spyglass
// zoom and cutscene framing both died. Now each field is scaled by
// R = lever / natural, and engine-authored changes are detected and honoured:
// if a field no longer holds what we last wrote, the engine deliberately
// changed it, so that new value becomes the base and gets scaled in turn. A
// 32 deg spyglass zoom stays a zoom (32 x R), cutscenes keep their framing,
// and the steady-state view is exactly the FOV the user asked for.
static const uint32_t kLevCtrl[3] = {0x3ac, 0x3b0, 0x3b4};   // FOVAngle/Desired/Default
static const uint32_t kLevCam[7]  = {0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564};
static float g_levLast[10] = {0};
static float g_levBase[10] = {0};
static float g_fovNatural  = 0.0f;   // the game's own base FOV, captured once
