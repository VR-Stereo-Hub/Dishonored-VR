// State chunk 31 for legacy/camera_hook - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool     g_camHookInstalled = false;
static bool     g_injectHead       = false;   // F3: live HMD -> engine camera
static uint8_t* g_camStub          = NULL;
static uint8_t  g_camOrigEpi[5]    = {0};
static int      g_camRefresh       = 0;

// Called from the asm stub with the camera 'this' pointer, AFTER the engine
// built the base matrix. We ROTATE the existing camera basis (proven in 4.1 to
// steer the view) by the head's yaw/pitch/roll — NOT rebuild it — so the engine
// renders where your head looks, in the engine's own convention.
//   rows: +0x50 = forward, +0x60 = right, +0x70 = up (world space)
static volatile uint32_t g_injTotal = 0, g_injMatch = 0, g_injMiss = 0;

// ---- head-look injection into the c0 view-projection (the matrix that renders)
static float g_viewA[16];          // world rotation-about-camera to inject
static bool  g_haveA = false;
static float g_refHmdYaw = 0, g_refGameYaw = 0;
