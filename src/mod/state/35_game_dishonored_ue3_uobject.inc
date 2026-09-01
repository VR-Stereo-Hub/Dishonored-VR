// State chunk 35 for game/dishonored/ue3/uobject - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool      g_peInstalled = false;
static uint8_t*  g_peStub = NULL;
static volatile LONG g_peCalls = 0;
static uint32_t  g_peSeen[512];        // name indices already logged
static volatile LONG g_peSeenN = 0;
static bool      g_peLogging = true;

// ---------------------------------------------------------------------------
// ProcessViewRotation is UE3's official "adjust where the player is looking"
// hook - the engine calls it every frame with the view rotation as an OUT
// parameter, precisely so that things like camera shake can modify it. That is
// the sanctioned way in, on the game thread, at the right moment in the frame.
// We add our head DELTA to it, so stick turning still composes normally and
// there is no feedback loop to run away.
//
// Parms layout: float DeltaTime | FRotator OutViewRotation | FRotator OutDeltaRot
//                    +0                  +4                        +16
// ---------------------------------------------------------------------------
static uint32_t g_idxViewRot = 0xffffffffu;
// 30.37b: telemetry - how often the view-rotation fast path actually fires.
// Zero per heartbeat during gameplay = the engine is not dispatching
// ProcessViewRotation (cutscene/scripted state), NOT a proxy failure.
static volatile LONG g_pvrHits = 0;
// 30.57: dispatches vs writes that actually landed. If head tracking ever dies
// again the heartbeat says which stage stopped: hits=0 (engine not
// dispatching), hits>0/writes=0 (our parms gate rejected it), or both>0 (the
// write landed and something downstream overrode it).
static volatile LONG g_pvrWrites = 0;
// Keyword filtering assumed Dishonored names its fire events the way stock UE3
// does. Rather than assume, do it differentially: learn every event that
// happens while you stand still, then report only what is NEW while you fire.
// Whatever appears in that second window IS the firing pipeline, whatever it
// happens to be called.
static int      g_wwPhase = 0;         // 0 idle, 1 baseline, 2 firing
static double   g_wwPhaseEnd = 0.0;
static uint32_t g_wwBase[1024];
static volatile LONG g_wwBaseN = 0;
static volatile LONG g_wwLines = 0;
// 30.18: frequency histogram over the fire window. Blink's targeting arc is
// recomputed every frame while the aim is held, so its event sits near the
// top of this chart - novelty alone can't find it (it also fires in menus).
static uint32_t g_wwHistN[256];
static uint32_t g_wwHistC[256];
static uint32_t g_wwHistCls[256];
static volatile LONG g_wwHistLen = 0;
static double   g_wwUntil = 0.0;


// ============================================================================
// Stage 26 - THE FIRST-PERSON MESH FOLLOWS THE HAND
//
// pMesh (DishonoredPlayerSkeletalComponent, pawn +0x3dc) carries its world
// transform as a row-major FMatrix at +0x060: three orthonormal basis rows at
// +0x060 / +0x070 / +0x080, world position at +0x090. The field dump proved
// it - rows (0.736,0.677,0) / (0,0,-1) / (-0.677,0.736,0) and a translation
// equal to the player's world position.
//
// We do NOT replace that basis. We do not know which local axis the artist
// called "forward", and guessing is precisely the mistake that warped world
// geometry in the shader-constant builds. Instead we apply a DELTA: the
// rotation that carries where you are LOOKING onto where you are POINTING.
// Whatever the mesh's own convention is, that delta swings it from the
// camera's forward to the hand's forward, and when your hand is aligned with
// your head the delta is identity and the matrix is left exactly as the engine
// wrote it.
//
// Runaway guard: if the engine does not rebuild this matrix every frame, a
// per-frame delta would compound and the arms would spin. So we remember the
// matrix as we found it AND as we left it; if we come back and find our own
// values untouched, we restore the original before applying the fresh delta.
// ============================================================================
// (g_handMesh moved up with the 30.70 hand-drive globals - the F10 overlay,
// which is compiled long before this point, reports whether HOME is on)
static int      g_fpFlipYaw[2]   = { 1, 1 };   // PageUp flips the trimmed hand
static int      g_fpFlipPitch[2] = { 1, 1 };   // PageDown flips the trimmed hand
static uint8_t* g_fpMesh     = NULL;
static uint8_t* g_fpPawn     = NULL;
static double   g_fpNextMs   = 0.0;
static float    g_fpBase[12];              // basis as the engine left it
static float    g_fpAfter[12];             // basis as we left it
static bool     g_fpHaveBase = false;
static long     g_fpWrites   = 0;
static long     g_fpRestores = 0;
static uint32_t g_fpMeshOff  = 0x3dc;      // pawn -> pMesh (re-derived if wrong)

// 26.0 walked camera -> controller -> pawn (camera was stale). 26.1 scanned
// the object table for a pawn (found a config asset). 26.2 scanned harder and
// the census returned the verdict: 103,217 objects, ZERO PlayerControllers,
// and every "pawn" it found was a DisTweaks_* tweak asset - while the very
// next log line was a script event ON DishonoredPlayerController. The runtime
// actors are simply not where my scan was looking.
//
// So stop looking. ProcessEvent hands us the object every event fires on, on
// the game thread, guaranteed live - that is how hand-aim catches projectiles
// and it has never missed. Latch the controller and the pawn straight out of
// that stream. Level travel heals itself, because the engine keeps calling
// events on whatever the current actors are.
static void FpDumpMatrix(const char* tag);
static void FpRawDump();
static bool FindFpMesh();

static uint8_t* g_peCtrl  = NULL;
static uint8_t* g_pePawn  = NULL;
static void*    g_clsCtrl = NULL;      // their UClass pointers, for cheap re-checks
static void*    g_clsPawn = NULL;
