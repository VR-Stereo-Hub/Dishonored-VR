// State chunk 03 for core/vr/openvr_backend - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

typedef void        (*PFN_VR_ShutdownInternal)(void);
typedef intptr_t    (*PFN_VR_GetGenericInterface)(const char*, EVRInitError*);
typedef const char* (*PFN_VR_GetVRInitErrorAsEnglishDescription)(EVRInitError);

static HMODULE g_openvr = NULL;
static PFN_VR_InitInternal        g_VR_InitInternal        = NULL;
static PFN_VR_ShutdownInternal    g_VR_ShutdownInternal    = NULL;
static PFN_VR_GetGenericInterface g_VR_GetGenericInterface = NULL;
static PFN_VR_GetVRInitErrorAsEnglishDescription g_VR_ErrDesc = NULL;

static struct VR_IVRSystem_FnTable*     g_sys  = NULL;
static struct VR_IVRCompositor_FnTable* g_comp = NULL;
static struct VR_IVROverlay_FnTable*    g_ov   = NULL;

enum VRMode { MODE_NONE, MODE_SCENE, MODE_THEATER };
static VRMode g_mode     = MODE_NONE;
static bool   g_vrReady  = false;
static bool   g_vrFailed = false;
static VROverlayHandle_t g_overlay = 0;

// --- Stage 8 (build 30.5): left-hand aim reticle -----------------------------
// A small SteamVR overlay anchored TO THE CONTROLLER DEVICE, offset down the
// same tilted ray the aiming pipeline uses (g_maimPitchOff). Device-relative
// anchoring means SteamVR moves it with the render pose - zero lag, zero
// per-frame math, and it cannot touch the weapon-drive code at all.
static VROverlayHandle_t g_retOverlay = 0;
static bool  g_retEnabled = true;      // [Reticle] Enabled
static float g_retDist    = 2.5f;      // [Reticle] DistanceMeters
static float g_retSize    = 0.030f;    // [Reticle] SizeMeters
static int   g_retDevIdx  = -1;        // device currently anchored to
static float g_retPitch   = -999.0f;   // pitchoff the anchor was built with
static bool  g_retVisible = false;

// ----------------------------------------------------------------------------
// Globals: D3D11 renderer
// ----------------------------------------------------------------------------
typedef HRESULT (WINAPI *PFN_D3D11CreateDevice)(IDXGIAdapter*, D3D_DRIVER_TYPE,
    HMODULE, UINT, const D3D_FEATURE_LEVEL*, UINT, UINT,
    ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
