// State chunk 04 for core/gfx/d3d11_device - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*,
    void*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE,
    HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

static HMODULE              g_d3d11mod    = NULL;
static ID3D11Device*        g_dev11       = NULL;
static ID3D11DeviceContext* g_ctx11       = NULL;
static bool                 g_d3d11Failed = false;

// 40.1 (the author's 39.3 fix): the adapter the OpenXR runtime REQUIRES.
// xrGetD3D11GraphicsRequirementsKHR hands back a LUID; creating our D3D11
// device on any other adapter means the shared eye textures cannot be read by
// the runtime, every call still returns S_OK, and nothing logs an error. That
// is the leading explanation for "works only on his PC" on Quest + VD. The
// backend publishes the LUID here BEFORE it asks for a device; EnsureD3D11
// then selects that adapter instead of passing NULL (= "whatever DXGI
// enumerates first"). Fail soft: an unmatched LUID logs loudly and falls back
// to the default adapter, because a flat game beats no game.
static LUID  g_wantAdapterLuid   = { 0, 0 };
static bool  g_wantAdapterLuidOk = false;
// What we actually landed on, read back FROM THE DEVICE (not from what we
// asked for) so the instrument can fail its own hypothesis.
static LUID  g_gotAdapterLuid    = { 0, 0 };
static bool  g_gotAdapterLuidOk  = false;
static char  g_gotAdapterName[128] = "";

// Two game-frame textures - one per eye - for alternate-eye stereo. Each frame
// the game renders one eye's view; we update only that eye's texture and keep
// the other eye showing its last frame (the compositor reprojects the gap).
static ID3D11Texture2D*          g_texGame[2] = {NULL, NULL};
static ID3D11ShaderResourceView* g_srvGame[2] = {NULL, NULL};
static UINT                      g_texW = 0, g_texH = 0;

static ID3D11Texture2D*        g_eyeTex[2] = {NULL, NULL};
static ID3D11RenderTargetView* g_eyeRTV[2] = {NULL, NULL};
// 30.81: a real depth buffer for the hand pass. The first version sorted
// triangles back-to-front on the CPU, which is fine for a few boxes but falls
// apart on a modelled mesh (and made texture batching awkward). Depth testing
// is both more correct and simpler: draw order stops mattering, so triangles
// can be grouped by texture instead of by distance.
static ID3D11Texture2D*        g_eyeDS[2] = {NULL, NULL};
static ID3D11DepthStencilView* g_eyeDSV[2] = {NULL, NULL};
static ID3D11DepthStencilState* g_dsOff = NULL;   // the game quad: no test, no write
static ID3D11DepthStencilState* g_dsOn  = NULL;   // the hands: test and write
static UINT                    g_eyeW = 0, g_eyeH = 0;
// ---- the runtime-facing state that outlives the old backends (41.0) ------
// The runtime layer (core/vr/openxr_runtime, next commits) replaces the mod's
// own loader negotiation, pace thread and action layer. These are the
// handles the rest of the mod keeps reading meanwhile.
static char     g_xrJsonIni[2 * MAX_PATH] = "";   // [VR] XrRuntimeJson
static int      g_xrHaptics = 1;          // [VR] XrHaptics=0 kills them entirely
static DWORD    g_presentTid = 0;
static bool     g_xrOn = false;           // session live and frames flowing
// 38.78: while a headset is served, the game never learns the desktop window
// lost focus (see OverlayWndProc). [Screen] KeepAliveUnfocused=0 restores the
// stock pause-on-focus-loss behavior.
static bool     g_vrKeepAlive = true;
// 38.88: [HeadTrack] ChainStamp - 1 (code default) = stamp every modifier
// dispatch in the chain pass with the same values (the 38.86 order-proof
// fix for rigs where the surviving modifier shuffles per load). 0 = the
// EXACT pre-38.86 behavior: one write to the first dispatch per presented
// frame, later stages (bob/sway/shake) untouched.
static int      g_chainStamp = 1;
// 38.79: set when the game announces shutdown (PreExit on a real exit);
// every VR path stands down so teardown can't crash under us.
static volatile LONG g_gameExiting = 0;
static uint32_t g_xrEyeW = 0, g_xrEyeH = 0;   // runtime-recommended per eye
static bool     g_vrReady = false;        // a headset is being served
// [Screen] EyeCant: the hand mesh applies the eye-to-head rotation when 1.
// Measured identity on every runtime so far; kept as the hand pass's lever.
static int      g_eyeCantCfg = 1;
static ID3D11VertexShader*     g_vs = NULL;
static ID3D11PixelShader*      g_ps = NULL;
static ID3D11InputLayout*      g_layout = NULL;
static ID3D11SamplerState*     g_sampler = NULL;
static bool                    g_pipelineReady = false;

// ----------------------------------------------------------------------------
// Globals: capture
// ----------------------------------------------------------------------------
static IDirect3DSurface9* g_sysmem = NULL;
