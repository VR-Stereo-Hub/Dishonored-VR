// State chunk 01 for proxy/proxy_state - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool    g_disabled = false;
static char    g_dir[MAX_PATH] = {0};

typedef IDirect3D9* (WINAPI *PFN_Direct3DCreate9)(UINT);
typedef HRESULT     (WINAPI *PFN_Direct3DCreate9Ex)(UINT, IDirect3D9Ex**);
typedef int         (WINAPI *PFN_D3DPERF_BeginEvent)(D3DCOLOR, LPCWSTR);
typedef int         (WINAPI *PFN_D3DPERF_EndEvent)(void);
typedef void        (WINAPI *PFN_D3DPERF_SetOptions)(DWORD);
typedef DWORD       (WINAPI *PFN_D3DPERF_GetStatus)(void);
typedef void        (WINAPI *PFN_D3DPERF_SetMarker)(D3DCOLOR, LPCWSTR);
typedef void        (WINAPI *PFN_D3DPERF_SetRegion)(D3DCOLOR, LPCWSTR);
typedef BOOL        (WINAPI *PFN_D3DPERF_QueryRepeatFrame)(void);

static PFN_Direct3DCreate9   g_realCreate9    = NULL;
static PFN_Direct3DCreate9Ex g_realCreate9Ex  = NULL;
static PFN_D3DPERF_BeginEvent g_realBeginEvent = NULL;
static PFN_D3DPERF_EndEvent   g_realEndEvent   = NULL;
static PFN_D3DPERF_SetOptions g_realSetOptions = NULL;
static PFN_D3DPERF_GetStatus  g_realGetStatus  = NULL;
static PFN_D3DPERF_SetMarker  g_realSetMarker  = NULL;
static PFN_D3DPERF_SetRegion  g_realSetRegion  = NULL;
static PFN_D3DPERF_QueryRepeatFrame g_realQueryRepeatFrame = NULL;

// 41.0: the device hooks and their originals live in core/framework/frame_hooks.
static HWND                g_gameWnd          = NULL;

// current render-target dims (from SetRenderTarget) - used to shear ONLY the
// main scene pass (wide aspect, full size), not square shadow maps or small
// reflection/UI targets.
static UINT g_curRTw = 0, g_curRTh = 0;

// ----------------------------------------------------------------------------
// Vertex-shader-constant SCAN (Stage 3.2, read-only): find the view-projection
// matrix register. World/bone matrices are affine (4th column = 0,0,0,1); a
// view-projection matrix is not. We tally, per constant register, how often a
// non-affine 4x4 matrix lands there and how often it changes frame-to-frame
// (camera responsiveness). No game memory is written; we only observe.
// ----------------------------------------------------------------------------
#define VS_REGS 256

// [VR] DisableBadApiLayers. An implicit OpenXR API layer whose library is
// 64-bit only cannot load into this 32-bit game and fails xrCreateInstance for
// every runtime; ON, the guard switches such a layer off for this process using
// the opt-out its own manifest declares. OFF, they are reported and left alone -
// which means no VR, and is only useful for confirming that a layer really is
// the cause. See core/vr/apilayer_guard.cpp.
static bool g_algGuard = true;

// [Stereo] ReentryStandDown. A second scene draw that never becomes a second
// PRESENT cannot pair; it can only overwrite pass 1 in the same backbuffer, and
// the one untagged present then carries whichever draw finished last - so the
// frame alternates between the eyes. ON, the re-entry beat stands the doubling
// down when it measures that; OFF, it is left running for work on the rung.
// Lives here rather than in scene_draw.cpp because config.cpp is compiled
// before it in the unity build. See SceneDrawBeat.
static bool g_sdStandDown = true;
static uint32_t g_sdLastSubmits = 0;   // the runtime's stereo submit total at the last beat
