// State chunk 28 for legacy/aim_watch - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static uint8_t*  g_awObj  = NULL;         // object the watch belongs to
static PVOID     g_awVeh  = NULL;
struct AwRec { uint32_t eip; uint32_t ret[3]; uint32_t n; };
static AwRec     g_awRecs[8];
static volatile LONG g_awRecN = 0;
static volatile LONG g_awTotal = 0;       // flood guard (freed+reused memory)
static uintptr_t g_awSelfLo = 0, g_awSelfHi = 0;   // our own DLL range (skip)
