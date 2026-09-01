// State chunk 26 for legacy/fire_tracer - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static uint32_t g_capEips[64];            // EIPs seen in the current sample
static uint32_t g_capCount[64];
static volatile LONG g_capN = 0;
static uint32_t g_bgEips[256];            // learned background readers
static int      g_bgN = 0;
static bool     g_fireArmed = false;      // a sample window is open
static bool     g_fireIsShot = false;     // this window is a shot (vs background)
static long long g_fireArmQpc = 0;
static long long g_lastBgQpc = 0;
static uintptr_t g_fireWatch[4] = {0,0,0,0};
static int      g_fireRounds = 0;

// ============================================================================
// Stage 7.2 - PROJECTILE-SPAWN TRACER (motion-controls expedition, step 2)
//
// The hardware tracer proved the aim = the view (fire reads the same camera
// POV the renderer uses). So the redirect point is the FIRE itself, not the
// aim: catch the projectile/bolt the instant it spawns and read the aim it was
// given. On a trigger pull we scan GObjects for freshly-created projectile-ish
// actors and dump their FVector/FRotator fields next to the camera's forward
// vector - so we can find the rotation/velocity offset to overwrite with the
// hand ray. Cheap (a couple object scans per shot; no CPU breakpoints).
// READ-ONLY for now.
// ============================================================================
static bool     g_spawnActive = false;
static int      g_spawnDelay  = 0;        // frames to wait after trigger
static int      g_spawnRounds = 0;
static uint8_t* g_spawnLogged[32];        // dedup recently-logged objects
static int      g_spawnLogN   = 0;

// ============================================================================
// Stage 7.3 - MOTION AIM (hand-aimed projectiles)
//
// The 7.2 tracer proved every ranged weapon spawns a real projectile actor
// (crossbow = DisProjectile_Arrow, pistol = DisBullet) with a shared DisActor
// layout:  FMatrix @ +0x50 (row X = forward = the aim), world pos @ +0x80,
// FRotator @ +0x9c with a cached copy @ +0xd0, and a velocity FVector further
// up (bullet: +0x1b4, parallel to forward). So: on a fire-trigger pull we arm
// a short window; each ~90 Hz tick we scan GObjects for a projectile-class
// actor within MaxDist of the camera and rewrite its aim - matrix rows,
// both rotators, and any world-scale vector parallel to its old forward
// (velocity) - to the controller's ray, converted into the game's world frame
// via the head-relative transform (hand-vs-HMD in tracking space, re-based
// onto the camera's basis, which we can READ reliably; only writes were dead).
// ============================================================================
static double   g_maimArmedUntil = 0;     // QPC ms deadline of the shot window
