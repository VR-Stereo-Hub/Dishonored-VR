// State chunk 20 for core/window/game_window - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool     g_ovlInit    = false;
static WNDPROC  g_ovlOldWndProc = NULL;

static void InstallWindowSubclass(const char* who);   // 38.92 (fwd)
