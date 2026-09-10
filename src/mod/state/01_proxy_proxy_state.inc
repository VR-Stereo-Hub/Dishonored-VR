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
// VR-69: [Stereo] P1EyeWrite. Pass 2 writes its eye into the camera field
// explicitly; pass 1 never did - it trusted the eye the world tick's
// ProcessViewRotation dispatch left there. That asymmetry is the shape of the
// reported fault: a tick whose dispatch does not land leaves the field holding
// the PREVIOUS tick's pass-2 value, so the engine's own draw renders the LEFT
// eye from the RIGHT eye's camera - the whole picture, world geometry included,
// displaced by one full IPD, in a fixed direction, with the head standing still.
// 1 = assert the left eye before pass 1, the mirror of what pass 2 already does.
static bool     g_sdP1EyeWrite = true;
// The measurement, and it keeps working while the write is ON: how far the field
// MOVED when pass 1 asserted the left eye. Zero means the dispatch had already
// put the left eye there and nothing was wrong; a full ipd*scale means the field
// held the other eye and pass 1 was about to draw from it.
static uint32_t g_sdP1Same = 0, g_sdP1Moved = 0, g_sdP1Other = 0, g_sdP1NoRead = 0;
static uint32_t g_sdBeatP1Same = 0, g_sdBeatP1Moved = 0, g_sdBeatP1Other = 0;
static uint32_t g_sdP1WriteRefused = 0, g_sdBeatP1Refused = 0;
static float    g_sdP1MovedMaxUu = 0.0f;
static uint32_t g_sdLastSubmits = 0;   // the runtime's stereo submit total at the last beat
