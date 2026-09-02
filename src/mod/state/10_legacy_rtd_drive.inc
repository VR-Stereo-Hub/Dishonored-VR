// State chunk 10 for legacy/rtd_drive - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static bool           g_rtdDoArms   = true;    // drive the arms rig
static bool           g_rtdDoWpn    = true;    // drive the weapon rigs
static UINT           g_rtdSizeWpn  = 36;      // registers that identify weapon rig A
static UINT           g_rtdSizeWpn2 = 0;       // ... and weapon rig B (left hand)
static UINT           g_rtdSizeArms = 144;     // ... and the arms
// 30.71: TWO INDEPENDENT HANDS. The first build applied one shared transform to
// every driven rig, which welded the sword to the hand but also dragged the
// LEFT arm around with the right controller. The arms rig is 48 bones, not one,
// so it can be split: a contiguous bone range goes to the right controller and
// everything else to the left. UE3 lists a skeleton depth-first, so each arm
// chain is a contiguous run - the overlay's range picker wiggles the candidate
// run live so the boundary can be seen rather than computed.
static int            g_rtdArmsHand = 1;       // whole-rig hand when no split
static int            g_rtdWpnHand  = 1;       // weapon rig A -> right (sword)
static int            g_rtdWpn2Hand = 0;       // weapon rig B -> left (crossbow/magic)
static int            g_rtdSplitLo  = 0;       // right-hand bone range, [lo, hi)
static int            g_rtdSplitHi  = 0;       // hi <= lo = no split, whole rig
// ---- 30.72: WHICH FRAME does the rig actually live in? --------------------
// The playtest says the weapons follow the controller AND still ride the head,
// and that trimming only slides them around inside the view. That is the
// signature of the rig NOT living in the camera-aligned space this drive
// assumed. Two hypotheses fit the ABI equally well:
//   (a) component space  - axes rotate with the camera  -> follow = 1
//   (b) camera-RELATIVE WORLD space (UE3 subtracts the camera position from
//       LocalToWorld but leaves the axes world-aligned) -> follow = 0
// Rather than guess a third time, the frame the drive projects through is now
// tunable: yawEff = yaw0 + Follow*(yawNow - yaw0). Follow=1 reproduces the old
// behaviour exactly; Follow=0 freezes the frame at the neutral pose, which is
// what (b) needs. Whichever value nulls the head swim IS the answer, and it
// also absorbs a mouse-injection gain error, which would land in between.
static float          g_rtdFollowYaw   = 1.0f;
static float          g_rtdFollowPitch = 1.0f;
static float          g_rtdYaw0[2] = { 0, 0 }, g_rtdPitch0[2] = { 0, 0 };
// component index a takes head component MapSrc[a] (0 right, 1 up, 2 fwd),
// times MapSgn[a]. Defaults reproduce the old fixed (fwd,right,up) guess.
static int            g_rtdMapSrc[3] = { 2, 0, 1 };
static float          g_rtdMapSgn[3] = { 1.0f, 1.0f, 1.0f };
static float          g_rtdNudge[3]  = { 0, 0, 0 };   // axis-probe offset, uu
// ---- 30.76: THE DECIDING TEST --------------------------------------------
// Five attempts at "controller pose -> camera-space transform" have all left
// the weapons riding the head, and each attempt cost a play session. So stop
// attempting and ask the only question that matters, with one variable:
//
//   CAN a transform applied to these bone constants hold the weapon still in
//   the room while the head turns - at all, by any gain?
//
// LockTest ignores the controllers completely and applies nothing but a yaw
// counter-rotation of Gain * (head yaw since neutral). Sweep Gain:
//   * if some value holds the weapon still in the world, camera-space
//     counter-rotation WORKS and everything left is my compose maths;
//   * if no value does - it drifts at every setting - then bone constants
//     cannot decouple this rig and the render-time path is finished, with no
//     sixth guess needed.
// Gain -1 is the value theory predicts.
static bool           g_rtdLockTest = false;
static float          g_rtdLockGain = -1.0f;
// ---- 30.72: the two arms are separate DRAWS, not two halves of one rig -----
// Wiggling bone sub-ranges moved BOTH arms and never one, which rules out a
// bone-index split: the left and right arm are drawn separately and each
// uploads its own 48 bones, so a size filter necessarily catches both. The
// discriminator has to be the draw, so uploads of a size are numbered within
// the frame and routed per ordinal. -1 = don't drive, 0 = left, 1 = right.
static int            g_rtdOrdHand[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static bool           g_rtdUseOrdinals = false;
static UINT           g_rtdOrdSeen = 0;        // ordinals observed last frame
static int            g_rtdIdOrd = -1;         // identifier: wiggle only this ordinal
static float          g_rtdPivotMix = 1.0f;    // 0 = spin about the viewpoint, 1 = about the hand
static float          g_rtdPosMax   = 120.0f;  // uu clamp on hand travel
static float          g_rtdScaleUU  = 0.0f;    // uu per metre; 0 = follow world scale
static float          g_rtdSmooth   = 0.0f;    // 0..1 EMA on the pose (0 = raw)
static float          g_rtdTrim[2][3] = { {0,0,0}, {0,0,0} };  // per hand: X fwd, Y right, Z up
static float          g_rtdWpnYPR[3] = { 0, 0, 0 };  // weapon-only basis correction, degrees
// Two escape hatches for the things this build cannot verify from outside the
// headset. RotInvert transposes the delta - the single fix if the weapon turns
// the WRONG WAY (a handedness/transpose slip is the likeliest way the basis
// change is wrong, and it cannot be tuned out with angles). RotScale=0 kills
// rotation entirely, leaving pure translation, which isolates the two halves of
// the drive so a fault can be attributed instead of guessed at.
static bool           g_rtdRotInvert = false;
static float          g_rtdRotScale  = 1.0f;
// Where the viewpoint sits along the rig's own up axis. The pivot is derived
// from the hand's position relative to the HEAD, which is only the same thing
// as its position relative to the RIG ORIGIN if that origin is at the eye. If
// it turns out to be at the pawn's feet the rotation will swing the arms; this
// slides the pivot back. The armed upload log prints the rig centroid so the
// value can be measured rather than hunted.
static float          g_rtdPivotUp   = 0.0f;
// Published under a seqlock: written on the submit thread (pose time), read on
// the render thread inside hkSetVSConstF. Odd sequence = a write in flight.
static volatile LONG  g_rtdSeq = 0;
static float          g_rtdR[2][9], g_rtdT[2][3];    // [hand] plain transform
static float          g_rtdRw[2][9], g_rtdTw[2][3];  // [hand] weapon-basis corrected
static bool           g_rtdHandOk[2] = { false, false };
static volatile bool  g_rtdOn = false;         // at least one hand live THIS frame
// The neutral orientation is stored in ROOM axes, NOT head axes. Storing it
// head-relative looks natural and is wrong: the delta then comes out as
// H*C*C0'*H0', and that trailing H0' is the head's orientation at capture time,
// so every degree you turned your head since pressing END would ride along in
// the weapon's rotation - the exact drift this whole feature exists to kill.
// Room axes make the rotation delta head-independent by construction; the head
// frame is applied fresh each frame when converting into rig space.
// (Position is different and IS stored head-relative - see below.)
static float          g_rtdC0[2][9];           // neutral controller axes in HEAD coords (H0*C0)
static float          g_rtdP0[2][3];           // neutral hand position, HEAD space, metres
static volatile bool  g_rtdHaveNeutral[2] = { false, false };
static volatile LONG  g_rtdReq = 0;            // bit0 = recapture left, bit1 = right
// ---- in-headset alignment markers (30.71) --------------------------------
// The calibration is "press END when your controller is where the game draws
// its hand", and in a headset you cannot see your controller - which is why
// trimming by feel was miserable. These are SteamVR overlays pinned to each
// controller and billboarded at the HMD, so the controller becomes visible as
// a bright ring. Park the ring on the in-game hand, press END, done.
static bool           g_rtdMarkers = false;   // the blue/orange rings - retired with the VR-hands calibration
static bool           g_rtdMarkVis[2] = { false, false };
static float          g_rtdMarkSize = 0.045f;
static volatile LONG  g_rtdHitsWpn = 0, g_rtdHitsArms = 0;
static float          g_rtdSmR[2][9], g_rtdSmP[2][3];
static bool           g_rtdSmOk[2] = { false, false };
static volatile LONG  g_rtdLogReq = 0;         // N uploads left to log (overlay button)
static bool           g_handMesh = false;      // HOME toggles (master hands switch)
// ---- rig IDENTIFIER (replaces the 30.69 timed sweep) ----------------------
// The timed sweep asked the user to remember which 2-second window they were in
// out of eleven, and the answer it produced ("x36 is the sword") turned out to
// be the reasoning, not the observation - the user actually saw the sword move
// in the x144 window. So: no timer, no hardcoded size list. Census what the
// game is ACTUALLY uploading right now, let the user step through those sizes
// one at a time with the picture in front of them, and OSCILLATE the chosen one
// (movement is far easier to spot than a static offset) until they say "that
// one". Then one button assigns it. Nothing has to be remembered.
static UINT           g_rtdCensus[86] = { 0 };  // uploads per size, render thread, plain adds
static UINT           g_rtdCensusSnap[86] = { 0 };
static volatile bool  g_rtdIdGo   = false;      // wiggle the selected size
static volatile UINT  g_rtdIdSize = 0;          // registers of the size under test
static float          g_rtdIdAmp  = 30.0f;      // uu
static float          g_rtdIdPeriodMs = 1200.0f;
// wiggle only bones [lo, hi) of the selected size - this is how the left/right
// boundary inside the 48-bone arms rig gets FOUND instead of guessed
static volatile int   g_rtdIdLo = 0, g_rtdIdHi = 0;   // hi <= lo = whole rig
// ---- 30.72: the crossbow's OTHER half ------------------------------------
// "c6 x30 moves one part of the crossbow, the other part stays put." A count
// of 4 at c6 is not a bone set at all - it is a plain per-draw LocalToWorld
// for a STATIC mesh, which is how attachments like the crossbow's body are
// drawn. Those translations ARE world positions, so unlike bones they can be
// filtered by distance to the camera (c5) - the filter that was hopeless for
// rig-local bone data is exactly right here. Diagnostic first: log what sits
// near the camera, then drive it in the next build once the space is known.
// ---- 30.74: FIND THE RIG'S WORLD MATRIX ----------------------------------
// The probe proved the bone constants are COMPONENT space: they hold the pose,
// not the placement. Something else maps that component into the world, and
// for a skinned draw UE3 has to upload it as a constant too - we have simply
// never looked for it. That matrix is the lever this feature actually needs:
// write it and the rig lands where we say in WORLD space, with no camera in
// the path at all, so head coupling becomes structurally impossible instead of
// something to cancel.
// Instrument: keep a ring of the most recent small uploads, and when an arms
// upload goes by, flush the ring plus the calls that follow. The matrix set
// just before or just after the bones is the candidate.
struct VcEnt { UINT reg, count; float v[16]; };
static VcEnt          g_vcRing[48];
static int            g_vcRingN = 0;
static volatile LONG  g_vcArmed = 0;      // 1 = flush at the next arms upload
static volatile LONG  g_vcAfter = 0;      // calls still to log after the flush
static volatile bool  g_rtdProbeOn = false;
static double         g_rtdProbeUntil = 0.0, g_rtdProbeNext = 0.0;
static volatile LONG  g_rtdStaticLog = 0;
static float          g_rtdStaticRadius = 250.0f;

// ============================================================================
// 30.77 - VR HANDS: our own weapons, drawn in VR space.
//
// The render-time drive is finished. The deciding test - a pure yaw counter-
// rotation, controllers out of the loop, swept across every gain - never held
// the weapon still while the head physically turned. Arkane draws the view
// model in camera space with no world matrix anywhere in the constant map, and
// the renderer offers no seam to get between the arms and the camera.
//
// So stop asking the engine. Hide its arms (we cannot place them, but we
// certainly can collapse them) and draw our own geometry at the controller
// pose, in the eye render targets we already own. The engine is no longer in
// the loop, so tracking is 1:1 by construction and there is nothing to
// calibrate away.
//
// Vertices are transformed to clip space on the CPU using the same
// GetProjectionRaw composition the eye quads use, and triangles are sorted
// back-to-front, so this needs no constant buffers and no depth buffer.
// ============================================================================
struct HandVert { float pos[4]; float col[4]; float uv[2]; };
