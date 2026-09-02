// State chunk 53 for core/input/pad_bridge - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE*);
typedef DWORD (WINAPI *XInputSetState_t)(DWORD, XINPUT_VIBRATION*);
static XInputGetState_t g_realXIGetState = NULL;
static XInputSetState_t g_realXISetState = NULL;
static bool     g_padHooked   = false;
static bool     g_padHookTried= false;
static volatile bool g_padActive = false;  // VR controllers currently feeding it
static CRITICAL_SECTION g_padLock;         // guards g_padState
static XINPUT_STATE g_padState;            // the synthesized pad (slot 0)
static DWORD    g_padPacket = 0;
static volatile LONG g_padPolls = 0;       // game polls this window (heartbeat)
// (g_ctrlIdx moved up with the pose globals - Stage 7.3 needs it earlier)
