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
// ---- 37.3 / XR-2: OpenXR backend state visible to the render path ----------
// Selected by env DISHONORED_VR_BACKEND=openxr (play_quest_vd.bat) or ini
// [VR] Backend=openxr. When live, the SAME pipeline renders the SAME eye
// textures - only the pose source (xrLocateViews instead of WaitGetPoses)
// and the delivery (swapchain copy + xrEndFrame instead of IVRCompositor::
// Submit) change. Definitions live in the XR section before the exports.
static bool     g_xrBackend = false;
static bool     g_xrQuad = true;          // 38.3: XR-3 quad-layer presentation
// 38.4: runtime manifest from the ini, so a NORMAL STEAM LAUNCH can run the
// OpenXR backend (no bat, no env var - "the Stardew shape": launch the game
// like always, the mod does the rest). Order: env > ini > 32-bit registry.
static char     g_xrJsonIni[2 * MAX_PATH] = "";
// 38.6: [Screen] XrScreenY - metres added to the XR quad's vertical centre
// (default 0; the centre itself now lands on the measured frustum centre).
static float    g_xrScreenY = 0.0f;
// 38.7: CYLINDER screen (XR_KHR_composition_layer_cylinder). The gameplay
// warp on the 38.6 flight was measured to be geometry, not reprojection:
// the FOV lever renders 130 deg and the FLAT quad resized to subtend ~125
// deg - rectilinear content that wide has >2x edge stretch, and head-locked
// it SWEEPS with your head (objects change shape as you look around). The
// Vive never shows it because its ~100 deg lenses only display the mild
// central region. A cylinder holds one angular scale across the width -
// the same shape VD's own cinema screen uses. [Screen] XrCylinder:
// -1 auto (use when the runtime has it), 0 flat quad, 1 force.
static int      g_xrCylCfg = -1;
static bool     g_xrCylOn  = false;   // extension enabled on the instance
// 38.8: [VR] XrLayer = proj (default) | cyl | quad. PROJ is the Vive-parity
// mode: the SAME BuildEyeQuads virtual-screen projection the Vive rig uses
// (hands + wrist panel included), submitted as a projection layer stamped
// with the render pose. Our content is world-consistent (head-look drives
// the camera 1:1), so reprojection treats it as what it is - a first-person
// view of the world - instead of fighting a screen shape. XR-2 was this
// mode; it never had a fair test (t/b fov bug fixed 38.6, blocking loop
// fixed by the pace thread, Steam-context crash fixed 38.4/38.5).
// cyl ("really fucking weird wrapped around") and quad (flat, 130-deg edge
// stretch) stay as fallbacks.
static int      g_xrLayerMode = 0;    // 0 proj, 1 cyl, 2 quad
// 38.9: [VR] XrPoseDelay (submit ticks, 0-3, default 1) - stamp the
// projection layer with the pose matching the GAME CONTENT's age instead of
// the freshest one. Content lags the pose by the capture pipeline; stamping
// fresh poses makes the compositor "correct" by the difference, which
// wobbles with the game's uneven fps = the 38.8 warp + head stutter.
static int      g_xrPoseDelay = 1;
// 38.9 OpenXR INPUT (ported from bioshock-vr openxr_input): the pace thread
// syncs actions + locates the grip poses each frame and publishes this
// snapshot under g_xrCs; the game thread composes the virtual pad from it
// (same Touch semantics as our SteamVR bindings) and feeds the hand poses
// into g_devPose[3]/[4], so hands, blink aim, melee, wrist panel and
// haptics light up with zero changes to any of them.
struct XrInpState {
    float mv[2], lk[2];
    float trigL, trigR, gripL, gripR;
    bool  a, b, x, y, clkL, clkR, menu;
    bool  handOk[2];
    float hand[2][3][4];
    bool  active;
};
static XrInpState g_xrInp;
static bool g_xrInpAttached = false;
static CRITICAL_SECTION g_xrCs;      // moved up from the XR-3 block (38.9)
static bool g_xrCsInit = false;
static void XrInpHaptic(int hand, float amp, float durSec);
// 38.10: haptics are QUEUED and applied by the pace thread. 38.9 called
// xrApplyHapticFeedback from the game's render thread while the pace thread
// was inside the runtime's frame calls - VDXR-32 raced, jumped through a
// trashed pointer (EIP=0 at the ARM-TIME haptic, both crash runs) and left
// heap damage FaceFX's destructor found seconds later. The pace thread owns
// EVERY runtime call, no exceptions - now including this one.
static float         g_xrHapAmp[2] = { 0, 0 };
static float         g_xrHapDur[2] = { 0, 0 };
static volatile LONG g_xrHapPend[2] = { 0, 0 };
static int           g_xrHaptics = 1;   // [VR] XrHaptics=0 kills them entirely
static DWORD         g_xrPaceTid = 0;   // 38.11: crash-report thread identity
static DWORD         g_presentTid = 0;
// 38.13: [Screen] XrFrustumFill=1 (default) - XR proj mode fills the whole
// frustum at 1:1 angular scale (no draggable screen border = the residual
// warp's anchor removed; exact tan-linear content mapping).
static int           g_xrFrustumFill = 1;
static bool     g_xrOn = false;           // session live and frames flowing
// 38.78: while a headset is served, the game never learns the desktop window
// lost focus (see OverlayWndProc). [Screen] KeepAliveUnfocused=0 restores the
// stock pause-on-focus-loss behavior.
static bool     g_vrKeepAlive = true;
// 38.84 STAMP WHAT YOU RENDERED - the pitch fix for machines where the
// engine ignores our rotator pitch (render never pitches although the
// rotator follows the head; measured on several Quest+VD machines across
// three runtimes). The XR layer stamp is corrected so it equals the pose
// the content was RENDERED from: renderPitch comes from the fork's new
// dxvk_vr_view export (the VP's w-row = unit world forward; UE3 world is
// Z-up so pitch = asin(fwd.z)). Where render == head the correction is
// identically zero - healthy machines see no change. [VR] StampFix:
// 0 = off (default), 1 = apply delta, 2 = apply flipped delta (sign
// conventions are the untestable-from-here risk; the overlay cycles all
// three LIVE so one session answers it).
static int g_stampFix = 0;
// 38.88: [HeadTrack] ChainStamp - 1 (code default) = stamp every modifier
// dispatch in the chain pass with the same values (the 38.86 order-proof
// fix for rigs where the surviving modifier shuffles per load). 0 = the
// EXACT pre-38.86 behavior: one write to the first dispatch per presented
// frame, later stages (bob/sway/shake) untouched - the path proven smooth
// for months on the dev rig, which stuttered under chain stamping.
static int g_chainStamp = 1;
// 38.89: [VR] StampLive - 1 (default) = stamp the XR projection layer with
// the head pose located THIS submit, not the pose the content was rendered
// from. Why: our content already carries the head rotation (the game camera
// follows the head 1:1), so a stamp that disagrees with the current head
// makes the compositor warp the image back by the difference - it cancels
// exactly the head motion the render just applied ("I can look up and down
// for a few seconds after load, then it locks" = the seconds before head
// injection starts, measured in the user's log: hw=0 while it works,
// hw=200 the moment it breaks). Stamping the live pose cannot cancel:
// worst case the image is a few ms old. 0 = the old render-pose stamp
// (marginally smoother reprojection on rigs where it was already right).
static int g_stampLive = 1;
// 38.79: set when the game announces shutdown (PreExit/GameSessionEnded on
// a real exit); every VR path stands down so teardown can't crash under us.
static volatile LONG g_gameExiting = 0;
// (moved up from the XR block so the PreExit handler can stop the pace thread)
static volatile LONG g_xrRun = 0;
static uint32_t g_xrEyeW = 0, g_xrEyeH = 0;   // runtime-recommended per eye
static bool     g_vrReady = false;        // a headset is being served
// [Screen] EyeCant: the hand mesh applies the eye-to-head rotation when 1.
// Measured identity on every runtime so far; kept as the hand pass's lever.
static int      g_eyeCantCfg = 1;
static void XrRtTryInit(void);
static bool XrRtFrameBegin(void);
static void XrRtSubmitEye(int eye);
static void XrRtFrameEnd(void);
static void XrRtPublish(void);               // 38.3 XR-3
static DWORD WINAPI XrPaceThread(LPVOID);    // 38.3 XR-3
static ID3D11VertexShader*     g_vs = NULL;
static ID3D11PixelShader*      g_ps = NULL;
static ID3D11InputLayout*      g_layout = NULL;
static ID3D11Buffer*           g_vb[2] = {NULL, NULL}; // per-eye quad, clip space
static ID3D11SamplerState*     g_sampler = NULL;
static bool                    g_pipelineReady = false;
static float                   g_quadAspect = 0.0f; // aspect the VBs were built for

// ----------------------------------------------------------------------------
// Globals: capture
// ----------------------------------------------------------------------------
static IDirect3DSurface9* g_sysmem = NULL;
