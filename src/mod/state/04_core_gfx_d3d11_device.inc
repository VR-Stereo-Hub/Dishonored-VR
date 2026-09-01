// State chunk 04 for core/gfx/d3d11_device - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*,
    void*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

static HMODULE              g_d3d11mod    = NULL;
static ID3D11Device*        g_dev11       = NULL;
static ID3D11DeviceContext* g_ctx11       = NULL;
static bool                 g_d3d11Failed = false;

// Two game-frame textures — one per eye — for alternate-eye stereo. Each frame
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
static bool          g_presentNamed = false;
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
// 37.6: rigid single-screen construction on the OPENVR path too, keyed to
// the HMD model - Quest-family lenses are strongly canted and per-eye
// frustum centering hands each eye a differently placed screen (the Steam
// Link "two different images"). Vive/Index never take this branch, so
// their tuned feel is untouched. [Screen] RigidScreen: -1 auto (detect
// Quest), 0 never, 1 always.
static bool     g_rigidScreen = false;
static int      g_rigidScreenCfg = -1;
// 37.9: OVERLAY-SCENE for Quest. The remaining "warp" is the compositor
// REPROJECTING a head-locked screen as if it were world geometry - correct
// for 3D scenes, exactly wrong for a screen that rides the head, and worst
// over Steam Link (double reprojection: SteamVR + on-device). The native
// cure is the same one OpenXR offers as head-locked layers (the fix from
// the user's Stardew mod): a compositor OVERLAY - composited directly,
// reprojection-EXEMPT, SideBySide-stereo capable, dashboard-stable on
// Quest. Scene submits continue as BLACK frames so the compositor stays
// fed and WaitGetPoses timing is untouched. Wrist HUD + settings panel
// are not drawn in this mode yet (ported once the warp is confirmed dead).
// [Screen] OverlayScene: 0 never (default; architecture rejected 38.0),
// 1 force-on for experiments only.
static int      g_ovlSceneCfg = 0;
// [Screen] EyeCant: 1 apply the eye-to-head ROTATION in the quad projection
// (exact math; identity on Vive/Index so those are bit-unchanged), 0 discard
// it (the 37.8-and-earlier behavior, kept as the rollback lever).
// 38.1 MEASURED both eyes identity on Quest/Steam Link - cant is NOT the warp.
static int      g_eyeCantCfg = 1;
// [Screen] WorldScreen: -1 auto (Quest-family/rigid only), 0 never, 1 always.
// 38.2: THE WARP FIX, per the bioshock-vr research + the Stardew data point.
// A HEAD-LOCKED screen is the one thing pose-predictive reprojection cannot
// render correctly: SteamVR reprojects our frame from render pose to display
// pose (treating content as world-locked), and on Quest the streaming
// client's OWN timewarp does it a second time with poses our TextureWithPose
// stamp can never reach. Every hop warps a head-locked quad; none warps
// world-locked geometry - that is literally what reprojection is FOR. So on
// Quest the screen anchors in ROOM space (level, yaw-only, at your head
// position when anchored - recenter re-anchors it), the quads are rebuilt
// each frame from the CURRENT WaitGetPoses pose (the pose the compositor
// assumes), and the submit is a plain Submit like every normal VR game.
// OpenXR "fixed" Stardew for exactly this reason: XR quad layers live in
// LOCAL (world) space. Vive/Index keep the tuned head-locked rig untouched.
// 38.3: WorldScreen shipped auto-on for Quest (38.2) and the headset verdict
// was immediate: geometrically correct, experientially WRONG - a fixed cinema
// screen instead of the Vive's head-locked window-into-the-world. Auto is
// dead; 1 keeps it available as an explicit experiment.
static int      g_worldScreenCfg = -1;
static bool     g_worldScreen    = false;
static volatile LONG g_wsReanchor = 1;    // anchor on first tracked frame
// [Input] ClickFallback: 1 (default) also read the legacy stick-click bit and
// OR it into sprint - insurance for controller profiles (Quest reported
// "omnione" via VD) whose thumbstick click doesn't reach our action binding.
static int      g_clickFallback = 1;
static bool     g_ovlScene = false;
static VROverlayHandle_t g_ovlSceneH = 0;
static float    g_ovlSceneW = 0.0f;
// 38.0: WORLD-ANCHORED with lazy follow, not head-locked. Head-locked +
// reprojection-exempt was stable but cannot be motion-smoothed (45 fps
// head-look shown raw - "doesn't feel smooth"). A world-anchored overlay
// is re-rendered by the compositor against the TRUE head pose every
// display frame: exact geometry (no warp) AND native-rate head motion.
// The anchor eases toward the head with time constant OverlayFollowTau
// ([Screen], seconds; 0 = hard head-lock = the 37.9 behavior).
// OverlayColor ([Screen]): 1 gamma (default), 2 linear, 0 auto - the
// "lighting looks bad" knob (overlay-path gamma handling differs from the
// scene path on some stacks).
static float    g_ovlFollowTau = 0.35f;
static int      g_ovlColor = 1;
static float    g_ovlYawS = 0.0f, g_ovlPitchS = 0.0f;
static float    g_ovlPosS[3] = { 0, 0, 0 };
static bool     g_ovlFollowInit = false;
static bool GetFnTable(const char* version, void** out);
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
