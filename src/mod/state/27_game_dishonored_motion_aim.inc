// State chunk 27 for game/dishonored/motion_aim - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static double   g_maimArmMs = 0;          // when the current window was armed
static bool     g_maimWindowCaught = false;
static bool     g_maimWindowLogged = true;
static void**   g_maimSnap = NULL;        // GObjects pointer snapshot @ trigger
static uint32_t g_maimSnapN = 0, g_maimSnapCap = 0;
static bool     g_maimNeedSnap = false;
static bool     g_maimNeedDiscover = true;
static double   g_maimLastDiscover = -1e9;

struct MaimHit {                          // projectiles we're actively steering
    uint8_t* ptr; double firstMs;
    void* cls; uint32_t nameIdx;          // identity check vs. object reuse
    float viewYaw, viewPitch;             // the shot's ORIGINAL view basis
    float lastDir[3];                     // what we last wrote (velocity match)
    float viewF[3];                       // original spawn forward (diagnostics)
    float lastPos[3]; int trackN;         // flight-path tracker
};
static MaimHit  g_maimHits[16];
static int      g_maimHitN = 0;

// Pooled-projectile watch list: Dishonored REUSES projectile objects (same
// pointer, same GObjects slot — proven by 7.2/7.3 logs where repeat shots
// never re-appeared), so a pointer-diff alone misses every shot after the
// first. Watched objects are caught by POSITION TELEPORT instead (pool reuse
// snaps the object to the muzzle).
struct MaimWatch { uint8_t* ptr; void* cls; uint32_t nameIdx; float pos[3]; };
static MaimWatch g_maimWatch[64];
static int       g_maimWatchN = 0;

// PlayerCamera instances (plural — stale ones exist; DishonoredPlayerCamera_1
// latched by FindLiveCamera had a FROZEN basis in the 7.3 test, which is why
// every "hand" direction came out ~world +X). Cameras are now used ONLY for
// the distance filter (min over instances); the aim basis comes from the
// projectile itself.
struct MaimCam { uint8_t* ptr; uint32_t nameIdx; };
static MaimCam  g_maimCams[4];
static int      g_maimCamN = 0;

static void MaimHaptic(int hand, float amp, float durSec); // defined w/ pad code

// ---------------------------------------------------------------------------
// Fire-function finder (7.3.2): the flight logs say the engine flies the bolt
// along its ORIGINAL aim even after we rewrite every actor-side field — the
// trajectory almost certainly lives in a physics body (PhysX). So the real
// redirect point is the CODE that initializes the shot. We arm a WRITE-ONLY
// hardware watchpoint (DR0) on the pooled projectile's rotation yaw (+0xa0)
// between shots; when the next shot re-initializes the pooled object, the
// VEH captures the writer's EIP + likely return addresses = the fire function
// chain, which we disassemble offline and detour in 7.4.
// ---------------------------------------------------------------------------
static uintptr_t g_awAddr = 0;            // watched address (0 = disarmed)
