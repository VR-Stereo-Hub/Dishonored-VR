// State chunk 11 for core/gfx/hand_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static ID3D11VertexShader* g_hmVS = NULL;
static ID3D11PixelShader*  g_hmPS = NULL;
static ID3D11InputLayout*  g_hmLayout = NULL;
static ID3D11Buffer*       g_hmVB = NULL;
static bool   g_hmReady = false;
static const char* kHandShaderSrc =
    "struct VSIn  { float4 pos : POSITION; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
    "struct VSOut { float4 pos : SV_Position; float4 col : COLOR0; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(VSIn i) { VSOut o; o.pos=i.pos; o.col=i.col; o.uv=i.uv; return o; }\n"
    "Texture2D  skin : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "float4 psmain(VSOut i) : SV_Target {\n"
    "    float4 t = skin.Sample(samp, i.uv);\n"
    "    if (t.a < 0.35) discard;\n"          // alpha-cut, for straps and belts
    "    return float4(t.rgb * i.col.rgb * 2.0, 1.0);\n"
    "}\n";

// 41.2 (VR-31): route (d) ruled the engine-side path out for HANDS - arms and
// hands are one material section on Skm_Player and no finer cut exists in the
// asset - so our own hands are the path being tested, and the lever ships ON
// for the test sessions. Reverts to OFF once the verdict is recorded.
static bool   g_hmEnable = false;        // [VRHands] Enabled - the fallback
// OFF with it on purpose: collapsing the game's arms in the same build as the
// first draw of ours would be two changes at once, and the game's arms are the
// reference our hands get judged against on the first run.
static bool   g_hmHideGame = false;      // [VRHands] HideGameArms
static float  g_hmScale = 1.0f;
static float  g_hmPos[2][3] = { {0,0,0}, {0,0,0} };      // metres, extra per-hand nudge
static float  g_hmRot[2][3] = { {0,0,0}, {0,0,0} };      // deg: yaw, pitch, roll
// 0 none, 1 sword, 2 crossbow, 3 empty hand, 4 pistol, 5 grenade, 6 springrazor,
// 7 heart. Chosen automatically from the equipped weapon's MESH ASSET NAME -
// the same identification FpCollect already refreshes every 750 ms, so swapping
// weapons in game swaps the model in your hand.
enum { HM_NONE=0, HM_SWORD=1, HM_XBOW=2, HM_HAND=3, HM_PISTOL=4,
       HM_GRENADE=5, HM_RAZOR=6, HM_HEART=7, HM_COUNT=8 };
static int    g_hmModel[2] = { 2, 1 };
static bool   g_hmAuto = true;           // follow the equipped weapon
static volatile LONG g_hmRebuild = 0;    // model changed; rebuild on the render thread
// Per-MODEL hold offsets, not per-hand: a pistol and a sword want different
// angles in the same hand, and auto-switching would otherwise fight the trim.
static float  g_hmMRot[HM_COUNT][3];
static float  g_hmMPos[HM_COUNT][3];
// Every first-person rig we want gone, by upload size. The left-hand weapons
// have their own sizes (the user measured x30 crossbow, x6 gun, x3 grenades)
// so hiding only the arms and the sword left them floating. 0 ends the list.
static UINT   g_hmHideSize[12] = { 3, 6, 30, 36, 144, 0, 0, 0, 0, 0, 0, 0 };
// The crossbow's body is a STATIC mesh (c6 x4 = a plain LocalToWorld), so it
// has no bones to collapse - but its translation is a real world position, so
// it can be caught by distance from the camera. View models ride the camera
// and are always close; keep the radius small so real props are not culled.
static float  g_hmHideStaticUU = 70.0f;
static bool   g_hmHideStatic = true;
// 30.79: a plain distance test was far too blunt - a doorway you are standing
// in front of is well inside 110 uu, so the cull ate real world geometry (the
// invisible doors in Campbell's level). View-model statics are not merely
// close, they are drawn in the same cluster as the first-person rigs, right
// after them. So the cull only stays open for a few draws after a rig has been
// collapsed, which world geometry elsewhere in the frame can never enter.
static volatile LONG g_hmStaticWindow = 0;
static int    g_hmStaticDraws = 6;
// model geometry, built once in controller-local metres
struct HmTri { float p[3][3]; float c[4]; float uv[3][2]; int tex; };
#define HM_MAXTRI 6000
static HmTri* g_hmGeo[2] = { NULL, NULL };
static int    g_hmGeoN[2] = { 0, 0 };
static float  g_hmObjScale = 0.01f;      // OBJ units -> metres (uu are ~1 cm)
static bool   g_hmObjLoaded[2] = { false, false };
// Hot reload: modelling is an iterate-and-look loop, and taking the headset off
// to restart the game between every tweak would make it unbearable. Save the
// OBJ and it reappears in your hand a second later.
static bool   g_hmHotReload = true;
static FILETIME g_hmObjTime[2][2];       // [hand][0 = weapon, 1 = hand mesh]
static char   g_hmObjPath[2][2][MAX_PATH];

static void RtdBuildYPR(const float* ypr, float* out);
static inline void V3Cross(const float* a, const float* b, float* o);
static inline float V3Norm(float* v);
static volatile bool g_sbGo = false;
static int      g_sbWritePoint = 1;          // 0 = ProcessEvent, 1 = the draw
static uint32_t g_sbBank = 0x208;
static float    g_sbOff = 40.0f;
static volatile LONG g_sbWrites = 0, g_sbBanks = 0;
static float    g_sbRead[3] = {0,0,0};
static float    g_sbWrote[3] = {0,0,0};
static float    g_sbC6[3] = {0,0,0};
static double   g_sbUntil = 0.0, g_sbNext = 0.0, g_sbFlip = 0.0;
static bool     g_sbOnPhase = true;
static volatile LONG g_sbInv = 0;   // 1 = reset ranges + log the inventory
// ---- 30.89: BANK HUNT ------------------------------------------------------
// The inventory finally showed why matching by bone count kept failing:
//   Skm_Player      79 bones -> would be c6 x237
//   Wpn_PlySword01  14 bones -> would be c6 x42
// but the uploads on screen are x144 (48 bones) and x36 (12). UE3 splits a
// skeletal mesh into CHUNKS and uploads only the bones each chunk references,
// so 79 bones can be drawn as a 48-bone chunk. The counts were never going to
// line up, and nothing is wrong with the components - but it also means we have
// no confirmation that +0x208 is the array the renderer feeds from. Both
// +0x208 and +0x214 report num=79; a third array of the same size could be the
// real SpaceBases. So: find EVERY bone-sized array on the component and sweep
// them one at a time, instead of trusting two offsets found by an early probe.
static int      g_sbHunt[24];         // candidate bank offsets
static int      g_sbHuntN = 0;
static int      g_sbSweepIdx = -1;
static bool     g_sbSweep = false;
static double   g_sbSweepNext = 0.0;
static float    g_sbSweepBase[86];    // bone0.x at the start of this slot
// The overlay grew an R&D wing while we were hunting the hands problem. Split
// it: the top level is what a PLAYER tunes, everything experimental lives
// behind one switch. Off by default, remembered in the ini.
static bool     g_ovlDev = false;
static float    g_heightOffsetM = 0.0f;   // [Tracking] HeightOffsetM
// ---- 31.0: point at the overlay with a controller -------------------------
// Computed where the action system lives (it needs the analog trigger) and
// consumed by OverlayFrame, which runs long before that code. The panel is
// head-locked and centred, so the controller ray's offset from your view
// direction maps straight onto it - no plane intersection needed.
static float    g_ovlRayX = 0.0f, g_ovlRayY = 0.0f;   // ray offset / forward
static bool     g_ovlPtrDown = false, g_ovlPtrValid = false;
// 31.3: mouse only. Pointing a controller at the panel read as fiddly in
// practice - it fights your aim, the trigger doubles as weapon fire, and a
// panel you open for ten seconds does not justify learning a new input. The
// code stays behind an ini flag for anyone who wants it; nothing in the
// overlay advertises it any more.
static bool     g_ovlPtrEnable = false;   // [Overlay] ControllerPointer
static int      g_ovlPtrHand = 1;        // which controller points
static float    g_ovlPtrGain = 2.2f;
static volatile LONG g_skcReq = 0;   // 30.90 SkelControl probe request
// 32.8 blink probe: declared up here because the overlay (which arms it) is
// compiled long before the probe itself.
static bool          g_bpGo  = false;
static volatile LONG g_bpReq = 0;
// 32.14 blink write test - same reason as g_bpGo: the overlay that drives
// these is compiled well before the code that uses them.
static uint8_t* g_blkObj = NULL;
static uint32_t g_blkIdx = 0;
static void*    g_blkCls = NULL;
static bool     g_blkTest = false;
static bool     g_blkHookOnUI = false;   // mirror for the overlay (32.21)
// 38.52 MAGIC AIMING. The 32.26 detour was left gated to blink on purpose
// ("whether the same detour serves other powers" - the doc's own open
// question). The hook demonstrably FIRES for every power (g_blkHookHits
// counts them all); it just declined to redirect anything whose component
// was not PowerBlink. Answer the open question: any live
// ActivePowerComponent gets its aim direction swapped to the controller
// ray - possession, devouring swarm, windblast alike. Direction ONLY: the
// engine's own traced distance, range check and pull-back stay intact,
// exactly as they do for blink. Powers with no aim trace never reach this
// code path, and Dark Vision / Bend Time are excluded outright as
// insurance. [Blink] AimAllPowers=0 reverts to blink-only.
static int g_aimAllPowers = 1;           // [Blink] AimAllPowers
static bool     g_blkDstOnUI  = false;   // 32.26
static bool     g_blkTraceAim = true;    // 32.31 [Blink] RedirectTrace
static double   g_blkTrcLastMine = 0.0;  // 32.36: last real redirect
// ---- 32.37: DECOUPLE THE REACH ------------------------------------------
// The trace hook at 0xbf5d1a reported "INSTALLED" and then never fired once -
// same as 0xbf595f. Two hook sites picked by reading a multi-path function
// statically, two that sit on branches the aim path does not take. I am not
// picking a third that way.
// So decouple the distance where we already stand, at the proven-executed
// store site. Today the direction is the controller's but the LENGTH is
// whatever the engine's view-ray happened to hit - look at the floor and the
// landing point rushes toward you, look at the sky and it flies off. Instead
// use a fixed reach along the controller ray. The engine still range-checks
// and validates the destination (0x00bfc9b7), so an over-long reach into a
// wall is the engine's problem to resolve, exactly as it already is.
// Auto-calibrates: remember the longest ray the engine has produced this
// session, which IS the power's max range when you look at open space.
// 32.38: fixed reach did decouple the head, but it replaced one thing the
// user could not control with another: "it's just locked to being far away I
// can't get the blink closer". Distance has to come from the CONTROLLER, the
// same place the direction comes from. Mode 2 maps the controller's pitch to
// reach - point down at your feet for a short hop, level out for max range -
// which is the standard VR teleport idiom and needs no geometry we do not
// already have. Modes 0/1 stay so a regression can be A/B'd in place.
//   0 = engine (head) distance   1 = fixed reach   2 = controller pitch
static int      g_blkReachMode  = 2;      // [Blink] ReachMode
static float    g_blkReachUU    = 0.0f;   // 0 = use the auto-learned max
static float    g_blkReachSeen  = 0.0f;   // longest engine distance observed
static float    g_blkNearUU     = 150.0f; // reach with the hand pointed down
static float    g_blkPitchNear  = -55.0f; // hand pitch (deg) that means "near"
static float    g_blkPitchFar   =  -5.0f; // hand pitch (deg) that means "far"
static float    g_blkPitchNow   = 0.0f;   // last measured, for the overlay
// 32.34: arm a frame dump that fires FROM INSIDE the aim. Asking the user to
// press Scroll Lock at the moment the marker is on screen cannot work when the
// marker is invisible - the last capture came back with no marker-shaped draw
// in 6031 draws because there was no way to know it was up. The destination
// detour only runs while an aim is live, so let IT pull the trigger.
static volatile LONG g_blkAutoDump = 0;
static float    g_blkMarkRadUU = 160.0f; // [Blink] LocatorRadiusUU
// 32.49: the engine traces along the VIEW and paints its marker there, while
// we patch the destination afterwards - so you land where your hand points but
// a second marker sits where you are looking. Redirecting the trace is the
// real fix and both candidate detour sites turned out to be dead branches, so
// until a live one is found, the renderer simply withholds that draw. It is
// identified by position, not by appearance: the fork compares the draw's
// object position against the head-aimed point we hand it.
// 32.51: OFF by default. Suppressing the engine's marker is a workaround, and
// shipping it switched on in the same build as the source-aim fix meant that
// when the result looked bad there was no way to tell which change did it.
// Source aim is supposed to make this unnecessary; if it works, this stays off
// forever.

static bool     g_blkDirOn    = false;   // 32.50 source-aim detour installed
static bool     g_blkDirAim   = true;    // [Blink] AimAtSource
static bool     g_blkAimOnCfg = true;    // 32.27 [Blink] ControllerAim
// 32.28: the game's own Blink marker is a small dim decal and it sits exactly
// where your eyes converge, which is the worst place to read a small dim decal
// (a frame dump ruled out any stereo fault - it is just hard to see). We
// already know the landing DISTANCE from the detour, and the destination is now
// along the controller ray by construction, so the reticle overlay we already
// have can be parked right on the landing spot: bright, depth-correct, and
// sized to stay readable at range.
static volatile float g_blkAimDistUU = 0.0f;
static volatile double g_blkAimSeen  = 0.0;
static bool     g_blkMarker = true;      // [Blink] Marker
// 32.29: "visible at the sides and looking down, but not straight ahead" is
// not an FOV problem - it is the marker being CO-LOCATED with the surface it
// is aimed at. The destination is a point ON the geometry (pulled back off it
// by only ~15 uu), so at a wall dead ahead the dot lands inside the wall's own
// pixels and loses; aimed at a floor or a distant side wall the surface angle
// changes and it pokes clear. Move it toward the player along the ray.
static float    g_blkMarkerBackUU = 60.0f;   // [Blink] MarkerPullbackUU
volatile LONG g_blkDstReqUI = 0;
static bool     g_blkDriveUI  = false;
static float    g_blkTestDeg = 25.0f;
static bool     g_blkWrite060 = true, g_blkWrite0d0 = true;
static volatile LONG g_blkWrites = 0;
static double   g_blkNextFind = 0.0;
volatile LONG   g_blkHookReq = 0;        // 32.21, driven from the overlay
static volatile LONG g_blkReadReq = 0;   // 32.22: arm a READ watch on +0x060
// 32.18: point the EXISTING hardware-breakpoint tracer at PowerBlink+0x060.
// The write race proved the field is an output, so the thing worth finding is
// the native code that PRODUCES it. A DR0 break-on-write plus the vectored
// handler already in this file names the writing instruction and its callers;
// Dishonored.exe has no ASLR, so those addresses disassemble straight off the
// file. This is how the camera function was located in 30.4x - same tool,
// new target.
static volatile LONG g_blkTraceReq = 0;
static bool     g_traceIsBlink = false;
// 32.15: the 32.14 test rotated the field's OWN PREVIOUS OUTPUT. Read v,
// rotate 25 deg, write it back - then next dispatch read the already-rotated
// value and rotate it again, 130 times a second. The target did not move 25
// deg right, it orbited the camera, which is why the blink landed BACKWARDS.
// Exactly the runaway the FOV lever hit in 30.51, and the fix is the same one
// its comment already spells out: never derive a write from a field you write.
// Keep what we last wrote; if the engine's value differs from it, the engine
// has re-authored the aim and THAT is the source. Rotate the source, never
// the result.
static float    g_blkWrote[2][3];
static bool     g_blkHaveWrote[2] = { false, false };
static float    g_blkSrc[2][3];
static bool     g_blkHaveSrc[2]   = { false, false };
// 32.17: the race telemetry. "It goes to head aim" and "it went backwards"
// cannot both be explained by the arithmetic, so measure the thing neither
// test could see: how often the ENGINE re-authors the field between our
// writes. If it restamps on essentially every dispatch then our value exists
// for microseconds and losing the race is the whole story - and the target
// stops being this field and becomes whatever feeds it.
static volatile LONG g_blkApplies = 0;
static volatile LONG g_blkEngine  = 0;
static double   g_blkNextTell = 0.0;
static inline bool BlkAlive();
// ---- 30.94: SkelControlSingleBone layout, READ FROM UE3's OWN REFLECTION ----
// Property Offset lives at +0x5c in each UProperty. Every size lines up with
// the next offset (FName 8 -> float at +8, FVector 12 -> byte at +12) and the
// bools share bitfield offsets, which is what UE3 does. Nothing here is
// inferred from a byte pattern - that is the 30.41 mistake.
