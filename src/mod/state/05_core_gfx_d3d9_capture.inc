// State chunk 05 for core/gfx/d3d9_capture - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static UINT      g_capW = 0, g_capH = 0;
static D3DFORMAT g_capFmt = D3DFMT_UNKNOWN;
static uint8_t*  g_pixels = NULL;
static uint32_t  g_frame = 0;
static bool      g_warnedFormat = false;
static bool      g_warnedRTD    = false;

// ----------------------------------------------------------------------------
// Globals: head tracking -> mouse
// ----------------------------------------------------------------------------
static bool  g_trackingEnabled = true;   // ini + F8 toggle
