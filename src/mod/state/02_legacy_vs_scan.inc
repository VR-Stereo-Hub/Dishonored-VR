// State chunk 02 for legacy/vs_scan - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static uint32_t g_vsCallCount[VS_REGS];    // matrix uploads seen at this reg
static uint32_t g_vsNonAffine[VS_REGS];    // of those, how many non-affine
static uint32_t g_vsChanged[VS_REGS];      // frame-to-frame value changes
static float    g_vsLast[VS_REGS][16];     // last matrix seen (for dump/compare)
static float    g_vsSum[VS_REGS];          // last checksum (change detection)
static bool     g_vsHave[VS_REGS];
static bool     g_scanEnabled = false;  // per-upload matrix scanner (costs fps)
static bool     g_forceNoVSync = true;  // uncap the game from the monitor
static int      g_scanDumps   = 0;
static bool     g_wpnDiag    = false;   // Stage 8.0: weapon-VP cluster diag
static bool     g_wpnEnabled = false;   // Stage 8.1: weapon mesh follows hand
static bool     g_wpnFindMesh= false;   // Stage 8.3: find FP weapon mesh comp
static bool     g_wpnAttach  = false;   // Stage 10: FP mesh follows the hand
static float    g_wpnRadius  = 60.0f;   // uu from camera = "attached to me"
static bool     g_wpnShowNear= true;    // log the nearest-to-camera draws
static int      g_wpnMaxBones= 20;      // ALL-mode: skip big skeletons (= NPCs)
static float    g_wpnPosScale= 55.0f;   // game units per metre of hand travel
static float    g_wpnPosMax  = 140.0f;  // clamp, game units
static float    g_wpnFpTol   = 0.35f;   // how far a skeleton may differ
// Master switch for the whole first-person-mesh experiment. It is OFF now.
// The premise was that any constant upload of 9+ registers is a bone palette - 
// but that is simply not true: material arrays, light data and other per-draw
// tables land in the same stream. Writing rotations into those corrupts world
// geometry, which is why walls started warping and why hiding a "bone" broke
// the picture. Until meshes can be identified reliably, this stays off.
static bool     g_wpnMaster  = false;
static bool     g_wpnAutoSmall = false; // move every small skeleton (old 10.x way)
static bool     g_rotInject = false;    // native head tracking (F3)
static bool     g_rotRoll   = false;    // inject head roll too (fights lean)
// 41.1 `headroll` A/B. Under a projection layer the roll write is FORCED on
// (the compositor rotates the image for head roll, so the game camera has to
// follow it). If tilt still reads wrong, the question is three-way and not a
// sign: the write matches the compositor (+1), fights it (-1), or should not
// happen at all because the compositor already did it (off). This forces the
// third case so all three can be swept in ONE run instead of three restarts.
static bool     g_rollForceOff = false;
static bool     g_scriptHeadOK = false; // engine view hook is driving the view
// 32.96: ...and WHEN it last drove it. The flag used to be latched forever,
// so when a save-load killed the script event, g_scriptHeadOK stayed true,
// the viewinject fallback stayed politely disabled, and NOBODY drove the
// camera - the user's "loaded a save while in a save and my head stopped".
// The claim now expires: fresh means a script write in the last 750 ms.
static double   g_scriptHeadMs = 0.0;
// 38.68 THE BOAT DEATH, root cause. The direct fallback below is a RESCUE
// for save-loads - but it seized the controller whenever script writes were
// quiet, and "quiet" is exactly what a keyhole seat-in or cutscene looks
// like. At a new game the script path has never claimed the camera at all,
// so the fallback wrote controller rotation straight through the intro
// boat's seat-in: Corvo was never seated, rode the deck standing (measured:
// eye 78, ExitKeyhole a no-op), and when the arrival script hid the boat he
// free-fell at frozen XY into the water. The fallback now HOLDS OFF while a
// cinematic is announced, while the script path has not yet claimed the
// current pawn, and for a grace window after a pawn latch (spawn/load).
static double        g_fbPawnMs   = 0.0;  // when the current pawn latched
static volatile LONG g_fbPvrSince = 0;    // script head write landed since latch
static double        g_fbBackoffMs = 0.0; // 38.75: watchdog back-off (fallback only)
static float    g_wpnTestYaw = 0.0f;    // debug: fixed yaw instead of hand
static int      g_wpnFlipX = 0, g_wpnFlipY = 0;

