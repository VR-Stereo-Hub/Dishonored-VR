// State chunk 05 for core/gfx/d3d9_capture - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

// 41.0: the capture lives in core/gfx/capture and the present counter in
// core/framework/frame_hooks (real modules). g_frame stays the name every
// per-frame stamp in the unity build uses.
#define g_frame (dvr::frame::count())

// ----------------------------------------------------------------------------
// Globals: head tracking -> mouse
// ----------------------------------------------------------------------------
static bool  g_trackingEnabled = true;   // ini + F8 toggle
