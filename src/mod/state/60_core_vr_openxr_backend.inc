// State chunk 60 for core/vr/openxr_backend - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static XrSystemId  g_xriSys   = XR_NULL_SYSTEM_ID;
static XrSession   g_xriSess  = XR_NULL_HANDLE;
static XrSpace     g_xriSpace = XR_NULL_HANDLE;
static XrSwapchain g_xriSwc[2] = { XR_NULL_HANDLE, XR_NULL_HANDLE };
static ID3D11Texture2D* g_xriImg[2][8];
static uint32_t    g_xriImgN[2] = { 0, 0 };
static bool        g_xriBegun = false;
static bool        g_xriFrameOpen = false;
static XrView      g_xriViews[2];
static XrTime      g_xriDispTime = 0;
static char        g_xriRuntimeName[128] = "";
static struct {
    PFN_xrPollEvent               pollEvent;
    PFN_xrBeginSession            beginSession;
    PFN_xrEndSession              endSession;
    PFN_xrWaitFrame               waitFrame;
    PFN_xrBeginFrame              beginFrame;
    PFN_xrEndFrame                endFrame;
    PFN_xrLocateViews             locateViews;
    PFN_xrAcquireSwapchainImage   acquire;
    PFN_xrWaitSwapchainImage      wait;
    PFN_xrReleaseSwapchainImage   release;
    // 38.9 input
    PFN_xrStringToPath            stringToPath;
    PFN_xrCreateActionSet         createActionSet;
    PFN_xrCreateAction            createAction;
    PFN_xrSuggestInteractionProfileBindings suggestBindings;
    PFN_xrCreateActionSpace       createActionSpace;
    PFN_xrAttachSessionActionSets attachActionSets;
    PFN_xrSyncActions             syncActions;
    PFN_xrGetActionStateFloat     getFloat;
    PFN_xrGetActionStateBoolean   getBool;
    PFN_xrGetActionStateVector2f  getVec2;
    PFN_xrLocateSpace             locateSpace;
    PFN_xrApplyHapticFeedback     applyHaptic;
} g_xrf;

// ---- 38.3 XR-3: detached pacing + quad-layer presentation ------------------
// The XR-2 crash fingerprint (NULL read in the game's device-management code,
// ~6 s in, SAFE mode included) matches the failure class bioshock-vr measured
// and fixed on the same runtime: xrWaitFrame on the game's present thread
// stalls it for tens of ms (or indefinitely while the session isn't FOCUSED),
// and the game's own code falls over. Their shipped shape, adopted here: a
// pace thread OWNS the whole XR frame loop (events, waitFrame, beginFrame,
// swapchain copies, endFrame) and the game thread never calls anything that
// can block - it renders eye textures and publishes them. Presentation is two
// QUAD layers in VIEW space (eyeVisibility L/R): head-locked BY THE
// COMPOSITOR, so no reprojection ever touches the screen - the same
// architecture that fixed the identical warp in the user's Stardew mod, and
// the Vive-parity experience (head-locked stereo window + injected head-look).
static XrSpace          g_xriViewSpace = XR_NULL_HANDLE;
static HANDLE           g_xrThread = NULL;
// (g_xrRun moved to the top globals for the 38.79 PreExit shutdown hook)
// (g_xrCs/g_xrCsInit live in the top XR block since 38.9 - the pad composer
// needs them long before this point in the file)
static ID3D11Texture2D* g_xrpTex[2] = { NULL, NULL };  // published content
static volatile LONG    g_xrpSeq = 0;                  // content generation
static LONG             g_xrpShown = 0;                // last gen copied out
static bool             g_xrShownOnce[2] = { false, false };
// pace -> game thread: freshest head pose + frustums (under g_xrCs)
static float            g_xrpM[3][4];
static bool             g_xrpValid = false;
static float            g_xrpFr[2][4];
static float            g_xrpIpd = 0.064f;
// 38.8 projection mode: the located views travel with the content so the
// layer is stamped with the pose the frame was RENDERED from (bioshock
// fovaudit rule: stamp what you rendered, not what is freshest).
static XrView           g_xrpLocViews[2];      // pace -> render (under CS)
static bool             g_xrpLocOk = false;
// 38.9: render-thread RING of consumed views, so the publish can stamp the
// snapshot from XrPoseDelay ticks ago (matching the content's age).
static XrView           g_xrViewRing[4][2];
static int              g_xrRingHead = 0;
static bool             g_xrConsOk = false;
static XrView           g_xrpPubViews[2];      // render -> pace, with content
static bool             g_xrpPubOk = false;
// 38.83: freshness travels with the views. If the locate->ring->publish
// chain stalls anywhere (session state blips, tracking-invalid frames, a
// timing race - all machine-dependent), g_xrpPubOk used to stay true with
// FROZEN views, and the layer was stamped with a stale pose forever - a
// compositor fed frozen stamps pins the image ("fixed in place and moves
// with me", "zoomed"). Stamps older than 250 ms are now replaced at submit
// with the pace thread's own freshly located views for THIS frame.
static double           g_xrRingMs = 0.0;      // when the ring views were located
static double           g_xrpPubMs = 0.0;      // age carried by the published stamp
// game -> pace thread: quad geometry (under g_xrCs)
static float            g_xrpQuadW = 0, g_xrpQuadH = 0,
                        g_xrpQuadCx = 0, g_xrpQuadCy = 0, g_xrpQuadD = 1.8f;
// minimal ID3D10Multithread (works on a D3D11 immediate context) so the pace
// thread's CopyResource may share the context with the render thread; the
// MinGW headers don't ship d3d11_4.
typedef ID3D10Multithread XrMtItf;   // works on a D3D11 immediate context
#define kIID_D3D10Mt __uuidof(ID3D10Multithread)

// ======================== 38.9: OpenXR INPUT =================================
// Ported from bioshock-vr's shipped openxr_input (staged /root/xr): one
// action set, oculus/touch_controller suggested bindings, grip-pose action
// spaces located against the app space, synced on the pace thread. Never
// bind .../input/system/click (reserved by runtimes).
static XrActionSet g_xiSet = XR_NULL_HANDLE;
static XrAction g_xiMove = XR_NULL_HANDLE, g_xiLook = XR_NULL_HANDLE,
    g_xiTrigL = XR_NULL_HANDLE, g_xiTrigR = XR_NULL_HANDLE,
    g_xiGripL = XR_NULL_HANDLE, g_xiGripR = XR_NULL_HANDLE,
    g_xiA = XR_NULL_HANDLE, g_xiB = XR_NULL_HANDLE,
    g_xiX = XR_NULL_HANDLE, g_xiY = XR_NULL_HANDLE,
    g_xiClkL = XR_NULL_HANDLE, g_xiClkR = XR_NULL_HANDLE,
    g_xiMenu = XR_NULL_HANDLE,
    g_xiPoseL = XR_NULL_HANDLE, g_xiPoseR = XR_NULL_HANDLE,
    g_xiHapL = XR_NULL_HANDLE, g_xiHapR = XR_NULL_HANDLE;
