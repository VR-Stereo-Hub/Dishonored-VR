// State chunk 20 for core/window/game_window - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

// 30.38 - in-game settings overlay (F10). Mouse and keyboard reach it through
// the WndProc subclass on the game window; while the overlay wants the mouse,
// those messages are swallowed so buttons don't leak clicks into the game.
static bool     g_ovlVisible = false;
static bool     g_ovlInit    = false;
static WNDPROC  g_ovlOldWndProc = NULL;

static void InstallWindowSubclass(const char* who);   // 38.92 (fwd)
