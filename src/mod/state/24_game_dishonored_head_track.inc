// State chunk 24 for game/dishonored/head_track - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 32.6: the cheap half of validity - is this object still IN GObjects at the
// index we found it? ~UObject nulls that slot before the block goes back to
// the pool, so this is the check that flips at the moment writing becomes
// heap corruption. The name/class test below cannot do that: freed memory
// keeps its old contents until it is reused. Two loads, safe to call on the
// ProcessEvent hot path.
static uint32_t g_camObjIdx = 0;

// ============================================================================
// IN-PROCESS DEBUGGER: camera-write tracer
//
// Sets a CPU hardware breakpoint (debug register DR0, break-on-write) on the
// live camera's world-matrix, then a vectored exception handler catches the
// exact instruction that writes it each frame. That instruction lives inside
// the engine's camera-calc function — the BioShock hook target. We record the
// writing instruction pointers (+ a few caller return addresses) so we can
// locate the function offline and hook it. READ-ONLY: we observe, never modify.
// ============================================================================
static PVOID    g_veh          = NULL;
