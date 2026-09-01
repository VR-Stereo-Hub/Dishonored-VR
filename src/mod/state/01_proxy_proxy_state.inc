// State chunk 01 for proxy/proxy_state - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static FILE*   g_log      = NULL;
static bool    g_disabled = false;
static char    g_dir[MAX_PATH] = {0};
static CRITICAL_SECTION g_logLock;

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

typedef HRESULT (__stdcall *PFN_CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND,
                                              DWORD, D3DPRESENT_PARAMETERS*,
                                              IDirect3DDevice9**);
typedef HRESULT (__stdcall *PFN_Present)(IDirect3DDevice9*, const RECT*,
                                         const RECT*, HWND, const RGNDATA*);
typedef HRESULT (__stdcall *PFN_Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT (__stdcall *PFN_SetVSConstF)(IDirect3DDevice9*, UINT,
                                             const float*, UINT);

typedef HRESULT (__stdcall *PFN_SetRenderTarget)(IDirect3DDevice9*, DWORD,
                                                 IDirect3DSurface9*);

static PFN_CreateDevice    g_origCreateDevice = NULL;
static PFN_Present         g_origPresent      = NULL;
static PFN_Reset           g_origReset        = NULL;
static PFN_SetVSConstF     g_origSetVSConstF  = NULL;
static PFN_SetRenderTarget g_origSetRT        = NULL;
static HWND                g_gameWnd          = NULL;

// current render-target dims (from SetRenderTarget) — used to shear ONLY the
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
