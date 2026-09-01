// State chunk 58 for core/framework/frame_hooks - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// 37.0 - XR-1 BENCH: OpenXR bring-up, end to end, inside this 32-bit process.
//
// The port's first measurable step. Armed ONLY by the environment variable
// DISHONORED_VR_XR_BENCH=1 (the xr_bench.bat sets it) - a normal launch
// never runs a line of this. What it proves when it passes: runtime loads
// via the negotiation interface (no loader DLL), instance + system come up,
// D3D11 session is created on the runtime's required adapter, swapchains
// allocate, the session reaches READY, and 30 frames run through
// xrWaitFrame/xrBeginFrame/xrEndFrame. Every step logs an "xrb:" line;
// the first failure names itself and the thread exits without touching
// the game (fail-soft, the bioshock-vr rule).
// ============================================================================
static PFN_xrGetInstanceProcAddr g_xrGipa = NULL;
