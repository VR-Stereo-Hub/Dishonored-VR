// State chunk 19 for core/gfx/d3d11_device - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// 30.38 - in-game settings overlay (F10). One ImGui frame per VR submit; the
// draw list is rendered into BOTH eye RTs (after the game quad, before the
// compositor submit), so the panel floats in view in the headset. Mouse and
// keyboard reach it through a WndProc subclass on the game window; while the
// overlay wants the mouse, those messages are swallowed so buttons don't
// leak clicks into the game's window path.
// ============================================================================
static bool     g_ovlVisible = false;
