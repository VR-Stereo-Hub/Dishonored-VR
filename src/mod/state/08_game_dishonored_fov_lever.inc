// State chunk 08 for game/dishonored/fov_lever - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static volatile LONG  g_fovLeverWrites = 0;
// 30.9b/30.61: probe requests are flags, serviced on the GAME thread (the
// first F3 attempt dumped from the render thread while the candidate list was
// being rebuilt underneath it, and died instantly).
static volatile LONG g_bonesReq = 0;
