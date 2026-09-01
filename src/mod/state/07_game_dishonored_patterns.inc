// State chunk 07 for game/dishonored/patterns - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

// UE3 UObject: ... Outer, FName Name (0x28), UClass* Class (0x30). Outer sits
// one pointer before the name. Used to answer "who OWNS this SkelControl".

static bool     g_stereoEnabled = false;   // ini [Stereo] Enabled + F7 toggle
static bool     g_povProbe      = true;    // 30.43: [CamSeam] PovProbe - scan
                                           // the live camera for its POV block
static bool     g_povWiggle     = true;    // 30.45: [CamSeam] Wiggle - which
                                           // POV block does the renderer obey
// ============================================================================
// 30.50 - THE FOV LEVER (ported from bioshock-vr's I6 rung 1)
//
// Their UE3 notes match our null results EXACTLY: every FOV holder "snapped
// back within a tick of being poked ... the value is RECOMPUTED each tick
// from the option upstream: no memory address is the source". Their fix is
// not a better address - it is a faster WRITE RATE: enforce the value on
// every camera-detour dispatch so the write outruns the recompute.
//
// We don't have their native detour, but we have something with the same
// property: the ProcessEvent hook, which fires thousands of times per second,
// spread through the whole frame - including after the camera update that was
// eating our once-per-frame writes.
//
// Targets (from the 30.48 hunt):
//   ctrl +0x3ac/+0x3b0/+0x3b4 = 85,85,85 -> the classic UE3 PlayerController
//        FOVAngle / DesiredFOV / DefaultFOV triple
//   cam  +0x254,+0x348,+0x368,+0x38c,+0x53c,+0x540,+0x564 = 95 -> the camera's
//        own copies, which tracked the RENDERED value through the zoom
// Verification is objective, not visual: the fork exports the FOV the renderer
// actually drew (dxvk_vr_proj), and the heartbeat prints lever vs rendered.
// 0 = off (the engine's own recompute is the undo - disarming self-restores).
// ============================================================================
static volatile float g_fovLever = 0.0f;
