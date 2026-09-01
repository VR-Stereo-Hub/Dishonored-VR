// State chunk 17 for core/gfx/eye_quads - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// --- geometry, built once, in controller-local metres -----------------------
// OpenVR controller axes: +X right, +Y up, -Z out the front. The grip sits at
// the origin, so a held weapon runs down -Z.
static int   g_hmTexSlot = 0;              // which skin the next triangles use
