// State chunk 32 for game/dishonored/head_track - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// Stage 12 — NATIVE HEAD TRACKING (the thing that separates a VR mod from a
// vorpX-style port)
//
// Everything so far renders the game's OWN view and then post-processes it.
// Head look goes in as emulated mouse deltas, which is why it swims, lags, and
// has no roll — the engine is always a step behind your neck, and no amount of
// mesh polish fixes that feeling.
//
// The fix every real VR mod uses: stop asking the mouse and write the view
// rotation into the engine directly. UE3 stores that as an FRotator (3 int32,
// 65536 = one full turn) on the camera POV. We locate it by finding the
// rotator whose angles reproduce the camera's known forward vector, then write
// our HMD orientation into it every frame.
//
// Turning still works: before each write we read what the game changed since
// our last write (that's your stick turn) and fold it into a body yaw, so head
// and body compose instead of fighting.
// ============================================================================
static int      g_povN = 0;
static uint32_t g_povOff[8];
static int      g_headInjMode = 0;          // 0 off, 1 fixed-yaw test, 2 live HMD
static int32_t  g_lastWriteYaw = 0;
static bool     g_haveLastWrite = false;
static float    g_bodyYawU = 0.0f;          // accumulated body (stick) yaw, UE units
static float    g_injRefYaw = 0.0f;
static bool     g_haveInjRef = false;
static const float kUEPerRad = 10430.378f;  // 65536 / 2pi

// ---------------------------------------------------------------------------
// Stage 13 — FIND THE VIEW ROTATION EMPIRICALLY
//
// Writing the camera object does nothing: confirmed twice now, from two
// different angles. The renderer clearly reads the view from somewhere else,
// and guessing which UE3 field that is has burned enough test cycles.
//
// So stop guessing and follow the one path that demonstrably works. Mouse
// movement DOES turn the view. Therefore some memory location changes when a
// mouse delta arrives, and that location is the real view rotation. We find it
// by experiment: snapshot the player objects, inject a known mouse delta,
// snapshot again, and report what moved — with a control pass first so fields
// that change on their own every frame get filtered out.
//
// Whatever survives is the address the engine actually renders from, and we
// can write our head pose straight into it.
// ---------------------------------------------------------------------------
#define VP_OBJS   10
#define VP_BYTES  0x800
static uint8_t* g_vpObj[VP_OBJS];
static const char* g_vpName[VP_OBJS];
static uint8_t  g_vpSnapA[VP_OBJS][VP_BYTES];
static uint8_t  g_vpSnapB[VP_OBJS][VP_BYTES];
static uint32_t g_vpNoise[VP_OBJS][VP_BYTES/4];   // how much each field moves on its own
static bool     g_vpProbing = false;             // suppress head-mouse while probing
static int      g_vpN = 0;
static int      g_vpPhase = 0;      // 0 idle, 1 control, 2 inject, 3 done
static int      g_vpTimer = 0;

static bool FindPlayerController();   // defined below, with the injection code
static bool BuildLiveSet();           // live-object table (graph-walk section)
static bool IsLiveObject(uint8_t* p);

// Rotator fields the probe proved the mouse drives — candidates to write into.
struct RotSite { uint8_t* obj; uint32_t off; const char* cls; };
static RotSite  g_rot[16];
static int      g_rotN = 0;
static float    g_rotBodyYaw = 0.0f;
static int32_t  g_rotLastYaw = 0;
static bool     g_rotHaveLast = false;
static float    g_rotRefYaw = 0.0f;
static bool     g_rotHaveRef = false;

// The probe pinned it down: on BOTH the camera and the controller the view
// rotation is an FRotator at +0x9c (pitch, yaw, roll), with a cached copy at
// +0xd0 — mouse movement drives the controller's copy, and the camera's copies
// are recomputed from it. That is why writing the camera alone kept sliding
// back. So we write the controller first (the source of truth) and the camera
// caches too (what this frame renders from).
//
// NB: the probe reported the YAW offset (+0xa0), because pitch barely moves
// during a horizontal turn — the rotator BASE is four bytes earlier. Writing
// from +0xa0 put pitch into yaw and yaw into roll, which is exactly the
// scrambled feel from the last build.
static uint8_t* g_pcObj = NULL;
// Only the fields we have actually VERIFIED as the view rotation. The probe
// also flagged +0x33c/+0x35c/+0x380/+0x454 on the camera, and I wrote head
// angles into all of them — but "changes when the mouse moves" does not mean
// "is the view". Those are almost certainly other actors' rotations parked on
// the camera (weapon, pawn, view target), which is exactly why world objects
// began turning with your head, and likely why it hitches.
static const uint32_t kPcRotBase[]  = { 0x9c, 0xd0 };   // controller: the source
static const uint32_t kCamRotBase[] = { 0x9c, 0xd0 };   // camera POV + its cache

// Last build swung too far the other way: re-scanning every frame meant
// dereferencing ~240 arbitrary pointers per frame through ObjClassName, and
// RangeReadable only proves a page is committed — not that it still holds a
// live object. Sooner or later one of those was garbage, and the render thread
// died inside our DLL. That is the crash in the stack trace.
//
// Now: remember WHICH camera field held the controller, and on the fast path
// just re-read that one field. Only when it changes do we rescan, and then
// every candidate is checked against the engine's own object table first.
static uint32_t g_pcOff = 0;
static double   g_pcNextScanMs = 0;

// ============================================================================
// Stage 17 — WALK THE OBJECT GRAPH (find the hands properly)
//
// Every previous attempt at the first-person mesh guessed: "this shader upload
// is probably bones", "this class name is probably the weapon". That guessing
// is what corrupted world geometry. We have something far better now — a
// verified anchor. The camera hands us DishonoredPlayerController, and UE3
// objects reference each other by pointer, so from that anchor we can WALK to
// the pawn, its components and the weapon, reading real class names at every
// step instead of inferring anything.
//
// READ-ONLY. This build only maps the graph and prints it.
// ============================================================================
// The first cut followed ANY pointer that looked plausible and dereferenced it.
// During a level load the engine is creating and destroying objects, so some of
// those pointers were already dead — which is what crashed the game. Now we
// build a sorted set of the addresses GObjects currently holds and follow a
// pointer ONLY if it is in that set: every object we touch is provably live.
static void**   g_liveSet = NULL;
