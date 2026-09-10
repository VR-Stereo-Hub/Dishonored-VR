// State chunk 06 for game/dishonored/head_track - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool  g_f8WasDown = false;
static float g_yawCounts   = 11.5f;      // mouse counts per degree (ini)
static float g_pitchCounts = 11.5f;
static bool  g_invertPitch = false;
static float g_screenDist  = 1.8f;       // meters (ini)
static float g_screenWidth = 3.6f;       // meters (ini, legacy fixed-size mode)
// 40.3: [Mode] GamepadOnly - motion controls off, plain gamepad only,
// while the render is being fitted. Head tracking and the FOV lever stay on.
static bool  g_gamepadOnly  = true;

static bool  g_haveLastPose = false;
static float g_lastYaw = 0, g_lastPitch = 0;
static float g_carryX = 0, g_carryY = 0;

// HMD orientation (radians), updated each frame from the pose; used by the
// camera-matrix injection (F3) and its sign-flip config.
static float g_hmdYaw = 0, g_hmdPitch = 0, g_hmdRoll = 0;
// 32.86: the HMD orientation AS OF the last camera write. The blink direction
// is decomposed against the head and recomposed against the game camera; if
// those two are sampled at different times, the gap between them is head
// rotation, and it leaks straight into the aim. The gap grows with frame time
// - which is why blink "broke again" the moment 4032x2268 halved the frame
// rate, and why it also broke during SEQ (same halving). Snapshot the pair
// together and the lag can never enter, at any frame rate.
// VR-68: the head history the HAND normalisation selects from, and the lever
// that picks the generation. MEASURED: the rendered camera moves with the head
// from two generations back (bv/lag, 0.119 deg at lag 2 against 1.19 at lag 0
// over 4085 moving frames), while MpDriveTick normalised against the FRESH
// head - so the hand was expressed relative to one head and planted in a view
// built from another, leaving two generations of head rotation in the frame.
//
// This is NOT the submission lag applied twice. The compositor reprojects the
// whole image from the tagged pose; the world is correct because it was
// rendered from that pose, and the hand is correct only if it was placed
// against the same one. Both consume the same generation; neither is delayed
// again.
//
// Ported onto the pre-VR-69 base 2026-09-09. The A/B that shipped with the
// original is deliberately NOT here: it walks the lever 0/2/0/2 on fifteen
// second segments, and an experiment left armed contaminates every measurement
// after it (docs/TRAPS.md section 1). The value it decided ships as the
// default instead.
#define DVR_HEAD_HIST 4
static float g_headHist[DVR_HEAD_HIST][3][4];
static bool  g_headHistOk[DVR_HEAD_HIST] = { false, false, false, false };
static int   g_headHistIdx = 0;
static int   g_headHistN = 0;
static int   g_mpPoseLag = 2;          // [Hands] PoseLag - the measured, headset-confirmed answer

static bool  g_blkProbeForce = false;   // [Blink] BlinkProbe=1: 2 Hz aim log
static float g_injHmdYawSnap = 0, g_injHmdPitchSnap = 0;
static bool  g_injSnapOk = false;
// 41.1: the POSE-LANE instrument's game side. g_hmdGen is the runtime layer's
// locate generation as of the sample in g_hmdYaw (stamped in TrackHead, on the
// present thread); g_injHmdGen carries it forward with g_injHmdYawSnap so the
// submission site can say how many locates back the sample the camera write
// actually used sits. g_injHmdSeq only ever increments, and a 0 at the audit
// means the adapter has never published a camera write at all - which the
// instrument prints instead of a delta, because "no sample" and "no
// disagreement" must never read the same. See openxr_runtime.h's pose-lane
// seam for the sign convention (published negated, once, at the seam).
static uint32_t g_hmdGen = 0;
static uint32_t g_injHmdGen = 0, g_injHmdSeq = 0;
// The camera frame blink recomposes against. 32.92: THIS WAS THE WHOLE BUG.
// Only the script-event camera writer published these; the direct-rotator
// fallback (viewinject) drove the camera without publishing, so on launches
// where the script event failed to latch - the long-known intermittent
// startup bug - the camera worked, the game LOOKED normal, and blink aimed
// against a frozen zero frame, rotating with the head 1:1. Per-launch coin
// flip, which is why it "broke" and "un-broke" across builds that never
// touched the aim. INVARIANT: every path that writes the game's view
// rotation MUST publish these four values at the moment it writes.
static float g_viewPitchRad = 0.0f;        // the view we just wrote, radians
static float g_viewYawRad   = 0.0f;

// Stage 7.3: raw device poses (3x4 device-to-tracking matrices) captured at
// WaitGetPoses time - [0] is the HMD; controllers found via g_ctrlIdx.
static float g_devPose[16][3][4];
static bool  g_devPoseOk[16] = {false};
static int   g_ctrlIdx[2] = {-1, -1};   // tracked-device index: 0=left 1=right

// Stage 7.3: MotionAim config (hand-aimed projectiles)
static bool  g_maimEnabled  = true;
static int   g_maimHand     = 0;      // 0 = left controller, 1 = right
static float g_maimPitchOff = 40.0f;  // deg, tilts ray toward natural point
static float g_maimWindowMs = 1200.0f;
static float g_maimMaxDist  = 900.0f; // uu from camera = "freshly spawned"
static int   g_maimFlipR    = 0, g_maimFlipU = 0;
static float g_refYaw = 0;        // recenter reference (F5)
static bool  g_haveRef = false;
static int   g_flipYaw = 1, g_flipPitch = 1, g_flipRoll = 1; // ini sign flips (+/-1)

// ----------------------------------------------------------------------------
// Globals: STEREO (experimental - writes the game camera)
// Confirmed UE3 layout from probe v3 (DishonoredPlayerCamera live instance):
//   +0x50 FMatrix (rows 0x50/0x60/0x70 = basis X/Y/Z, row3 0x80 = world pos)
//   +0x80, +0x90, +0xc4 = cached camera world location (FVector, 3 floats)
//   right-vector = basis-Y row @ +0x60
// ----------------------------------------------------------------------------
