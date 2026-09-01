// State chunk 12 for game/dishonored/hands/skelcontrol - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static const uint32_t kSkcStr     = 0x64;   // float ControlStrength
static const uint32_t kSkcBools   = 0xb8;   // bApply/bAdd/bRemove bitfield
static const uint32_t kSkcTrans   = 0xbc;   // FVector BoneTranslation
static const uint32_t kSkcTSpace  = 0xc8;   // BYTE  BoneTranslationSpace
static const uint32_t kSkcRSpace  = 0xc9;   // BYTE  BoneRotationSpace
// The three live controls that hang off pMesh (the player's own component).
static uint8_t* g_skcPlayer[8];
static int      g_skcPlayerN = 0;
// 35.7: graft-experiment state (machinery lives with the skc probe code)
static uint32_t g_graftOffNext = 0, g_graftOffStr = 0;
static uint32_t g_graftOffSTgt = 0, g_graftOffBTG = 0;
static uint8_t* g_graftDonor[3] = { NULL, NULL, NULL };
static int      g_graftDonorN = 0;
static bool     g_graftOn = false;
static uint8_t  g_graftSave[3][0x100];      // donor bytes before we touched it
static uint8_t* g_graftHost[3] = { NULL, NULL, NULL };   // live ctl we linked
// 35.8: the REAL drive through the donors. 35.7 proved the channel (+45 deg
// through a grafted donor VISIBLY rotated the arm - user confirmed with
// screenshots), so the fixed test rotation is gone and the 33.7 mailbox
// (g_skcWantRot -> SkcRotApply) now delivers controller rotation to the
// grafted donors instead of the dead host channel.
static uint32_t g_graftDonorIdx[3] = { 0, 0, 0 };  // GObjects index (liveness)
static void*    g_graftDonorCls[3] = { NULL, NULL, NULL };
static int      g_graftHand[3] = { -1, -1, -1 };   // hand each graft serves
static bool     g_graftWant = false;   // the user's wish - survives rig reloads
                                       // (g_graftOn is merely "engaged now")
static int      g_graftRotSpace = 0;   // BoneRotationSpace stamped by the
                                       // drive: 0 world .. 4 bone (overlay dial;
                                       // the math is world-frame, but 35.7 only
                                       // PROVED bone space applies - measure)
// 35.9 tried "subtract the LIVE head instead of the snap" - WRONG THEORY,
// measured (no change): the snap is republished every frame as a matched
// pair with g_viewYawRad/g_viewPitchRad, so it already WAS live. 36.0's
// reading of the same symptom: BCS_WorldSpace (rot space 0) is TRUE world -
// the engine converts the given rotation via the component's LocalToWorld
// and does NOT compose the camera in for us. A head-relative command in a
// world slot leaves the weapon head-coupled by construction. The world
// command must be ABSOLUTE: injected view rotation + (controller - head),
// every term taken from the same matched pair the injector published.
static bool     g_graftHeadComp = true;   // [Hands] GraftHeadComp: view compose
static void GraftTestSet(bool on);
static void SkcRotZeroNeutral(const char* why);
static bool     g_skcDrive = false;
static float    g_skcTrans[3] = { 0.0f, 0.0f, 60.0f };
static int      g_skcSpace = 3;        // 0 world 1 actor 2 comp 3 parent 4 bone
static uint32_t g_skcBoolMask = 0xFF;
static float    g_skcStrength = 1.0f;
// ---- 30.96: ARKANE AUTHORED PER-HAND CONTROLS ------------------------------
//   #0 LookAtControl_Camera      #1 LookAtControl_LeftHand
//   #2 LookAtControl_RightHand
// Separate left and right, already at strength 1.0, already evaluated every
// frame inside the animation pass. That is per-hand separation for free - no
// filtering by upload size, no bone-index guessing, and the engine keeps
// animating everything else around them.
//
// Their bools read 0x0a. UE3 assigns bitfield masks in declaration order
// (bApplyTranslation 1, bApplyRotation 2, bAddTranslation 4, bAddRotation 8),
// so 0x0a = bApplyRotation | bAddRotation: Arkane uses them as ROTATION-only
// look-at controls. Position needs bApplyTranslation turned on, which is why
// the translation sliders did nothing until the mask went in.
static const uint32_t kSkcApplyTrans = 0x01;
static const uint32_t kSkcApplyRot   = 0x02;
static const uint32_t kSkcAddTrans   = 0x04;
static const uint32_t kSkcAddRot     = 0x08;
// 31.4: map by NAME, never by index. GObjects iteration order is not stable -
// two probes in one session returned the controls as
//   #0 RightHand #1 LeftHand #2 Camera   and   #0 Camera #1 LeftHand #2 RightHand
// so a hardcoded index map silently drove the CAMERA look-at with hand data on
// one load and left a hand undriven. That alone would explain hands that stop
// responding after a reload and "movement that follows the head".
static int      g_skcHandOf[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
static bool     g_skcLive = false;      // drive from the CONTROLLERS, not sliders
static bool     g_skcDoTrans = true, g_skcDoRot = false;
static bool     g_skcAddMode = true;    // add to the animated pose vs replace it
static float    g_skcScaleUU = 50.0f;   // uu per metre of hand travel
static float    g_skcMax = 120.0f;
static float    g_skcNeutral[2][3];
static bool     g_skcHaveNeutral[2] = { false, false };
// ---- 32.12: the neutral is PERSISTED, so calibration is a one-time act -----
// Until now the neutral was "wherever your controllers happened to be at the
// instant the poses first went valid after launch". Every hand position is
// measured from it, so a different resting pose at startup moved the whole rig
// and the only knob left was the trim - which is exactly why the trim needed a
// nudge every session. 32.11 made recentre recapture it, which helps but still
// asks for a ritual every launch.
// A saved neutral removes the problem instead of managing it: capture once,
// write it to the ini, load it at boot. Same numbers every launch, so the trim
// values keep meaning the same thing forever.
// The first run under this build captures as before and IMMEDIATELY saves, so
// the neutral it persists is the one the user's existing trims were tuned
// against - nothing to re-tune.
static bool     g_skcNeutralSaved = false;
// ---- 32.25: PER-STANCE TRIM ------------------------------------------------
// Reported: crouching with the button puts the right arm/sword above where the
// real arm is, while the left stays fine. Asymmetric, so it is not a camera
// height mismatch - both hands would move together for that. Rather than chase
// the cause through the animation rig, correct it empirically, which is what
// the user asked for and is the right shape of fix either way: a second trim
// set that applies while crouched and hands back to the standing one when you
// stand. The sliders edit whichever set is live, so there is no extra button
// and no mode to remember.
// The crouch signal has to be the GAME's, not ours - the physical-crouch
// detector fires but the user crouches with a button. UE3 Pawn carries
// bIsCrouched; its offset and bitmask are read from the engine's own
// reflection, never guessed (guessing property layout is what crashed 30.41).
// 32.39: this is an OFFSET ADDED TO THE STANDING TRIM, not a replacement set.
// 32.25-32.38 swapped one set for the other, so the moment the stance flipped
// the standing calibration was thrown away wholesale - and since this set had
// never been tuned it was all zeros, which is exactly why standing "felt off"
// once the crouch detector started firing. A zero offset now means "identical
// to standing", so the worst case is the behaviour we had before per-stance
// trim existed, and re-calibrating standing automatically carries into crouch.
static float    g_skcTrimCrouch[2][3] = { {0,0,0}, {0,0,0} };
static bool     g_skcCrouchTrimOn = true;
// 34.9: standing-BLOCK trim. Blocking while standing pushes the rendered arm
// too far forward (crouched block is right), so blocking gets its own additive
// offset, exactly the crouch-trim pattern. The signal is the controller's
// m_bBlockButton byte (found by the 34.7 blockhunt walk at +0x62e; the offset
// is re-resolved from reflection at runtime, never hardcoded).
static float    g_skcTrimBlock[2][3] = { {0,0,0}, {0,0,0} };
static bool     g_skcBlockTrimOn = true;
static volatile bool g_blockHeld = false;
static bool     g_pawnCrouched = false;
static uint32_t g_bIsCrouchOff  = 0;
static uint32_t g_bIsCrouchMask = 0;
static bool     g_bIsCrouchFound = false;
static double   g_crouchPropNext = 0.0;
// 32.38: Pawn.bIsCrouched is MEASURABLY the wrong signal. The log shows it
// reading true for 156-172 ms bursts while the user holds crouch for whole
// seconds, and there is exactly one candidate and it IS declared on Pawn - so
// preferring the Pawn one (32.37) could not have helped and did not. Rather
// than guess a third property name, measure the thing that cannot lie: how far
// the game camera sits above the pawn's own origin. Crouching lowers the eye,
// walking downstairs lowers BOTH, so the difference is stance and nothing else.
// Standing height self-heals the same way the HMD baseline does: it is simply
// the highest eye height seen, and crouching can only ever lower it.
static int      g_crouchSrc     = 1;      // [Hands] CrouchSource 0=bIsCrouched 1=eye
static uint32_t g_actorLocOff   = 0;      // Actor.Location byte offset
static bool     g_actorLocFound = false;
static double   g_locPropNext   = 0.0;
static float    g_eyeNowUU      = 0.0f;   // camZ - pawnZ, this frame
static float    g_eyeStandUU    = 0.0f;   // slow estimate of the STANDING eye
static float    g_eyeCrouchUU   = 0.0f;   // slow estimate of the CROUCHED eye
static bool     g_eyeHaveRef    = false;
static double   g_eyeLastMs     = 0.0;
static float    g_eyeDropUU     = 20.0f;  // [Hands] CrouchDropUU (min separation)
static bool     g_eyeCrouched   = false;
// 32.40 -------------------------------------------------------------------
// Two INDEPENDENT signals - bIsCrouched and camZ-minus-pawnZ - both blip for
// 30-500 ms and then fall back, at the same moments, while a crouch is held
// for seconds. Two unrelated measurements failing identically is not two
// coincidences; the thing they share is the POINTER. g_pePawn is latched from
// the event stream by class name, so any actor of that class re-points it.
// Read the pawn the PlayerController actually possesses (+0x248) instead, and
// dump every input to the decision at 5 Hz so the next fix is measured rather
// than reasoned about. Also locate bWantsToCrouch, the input-intent flag,
// which is sustained by construction - all three get logged side by side.
static uint32_t g_bWantsCrOff   = 0;
static uint32_t g_bWantsCrMask  = 0;
static bool     g_bWantsCrFound = false;
static bool     g_crouchDiag    = true;   // [Hands] CrouchDiag
static uint8_t* g_crouchPawnWas = NULL;
static double   g_crouchDiagNext = 0.0;
// 38.25 crawlbox: raw movement stick as the CONTROLLER sent it, captured in
// both pad compose paths BEFORE any menu/shaping logic, so the crouch/raw
// line can print raw-vs-delivered side by side.
static float    g_dbgRawMx = 0.0f;
static float    g_dbgRawMy = 0.0f;
// ...and the DELIVERED movement stick, mirrored from the final synthesized
// pad AFTER menu shaping (g_padState lives further down the file and its
// lock with it, so the diag block reads these lock-free mirrors instead).
static SHORT    g_dbgOutLx = 0;
static SHORT    g_dbgOutLy = 0;
static bool     g_crouchForce   = false;  // overlay: pretend we are crouched
static bool     g_crouchPend    = false;  // debounce: candidate state
static double   g_crouchPendMs  = 0.0;
static float    g_crouchHoldMs  = 250.0f; // [Hands] CrouchHoldMs
// 32.41 -------------------------------------------------------------------
// 1995 samples with the POSSESSED pawn settle it. The pointer was rock steady
// (one value, zero changes), so 32.40's hypothesis was wrong. And:
//   * camZ - pawnZ sat at 76-78 uu in 1858 of 1995 samples - dead flat. The
//     pawn's Location drops WITH the camera when you crouch, so the
//     difference is not a stance signal at all and never was.
//   * bIsCrouched was true in 2 samples out of 1995. bWantsToCrouch: zero.
//     Dishonored does not use UE3's stock crouch state.
// Which leaves the one signal that cannot be wrong: WE synthesize the pad.
// The crouch/sneak button the game receives is a bit we set ourselves, so
// read it back instead of trying to infer its effect from the world.
// Whether that button is hold-style or toggle-style is then measured, not
// assumed: watch the camera height across a press/release cycle and see
// whether the view stays low after the button comes up.
// 32.42: 731 samples with btn=0 throughout, while the pad log shows the user's
// crouch presses arriving as xbtn=0x2000. 0x2000 is B - which is what Dishonored
// binds crouch/sneak to on a gamepad - and 32.41 was watching LEFT_THUMB (0x40),
// the bit the PHYSICAL crouch detector synthesizes and which this user's
// bindings never set. Same class of mistake as the three before it: an assumed
// constant standing in for a measurement. So the mask is now configurable, it
// defaults to B|LEFT_THUMB, AND it learns: any button whose press is followed
// by the camera dropping is a crouch button, whatever it happens to be bound to.
static volatile LONG g_padBtnsPub = 0;    // the whole synthesized button mask
// 32.44: the default is B ALONE. 32.42 also seeded LEFT_THUMB because the
// physical-crouch detector synthesizes it - but physical crouch is off, the
// user's bindings put something else on that bit, and seeding it gave the
// learner a free pass to adopt a non-crouch button. Physical crouch can add
// the bit back itself when it is re-enabled.
static uint32_t g_crouchBtnMask = 0x2000; // [Hands] CrouchButtonMask  (B)
// 36.6: long-press interact (left A) = drink a health elixir. The game owns
// the mechanic already - the user's DishonoredInput.ini binds R ->
// GBA_HealthElixir (Dis_ConsumeElixir ElixirType_Health) - so this
// synthesizes that keypress, the same proven route as the voice-command J.
static bool   g_elixirOn = true;        // [Input] HealthElixirLongPress
static float  g_elixirHoldMs = 700.0f;  // [Input] HealthElixirHoldMs
static int    g_elixirVk = 'R';         // [Input] HealthElixirKey
// 36.7: finisher-capture toggle - logs every distinct event dispatched on
// the player pawn/controller while on (the substring net drowned in
// particle noise; the choreography must run on the player).
static bool   g_finCapOn = false;
static int      g_crouchBitScore[16] = {0};
static double   g_crouchBitMs[16]    = {0};
static float    g_crouchBitZ[16]     = {0};
static float    g_crouchBitX[16]     = {0};
static float    g_crouchBitY[16]     = {0};
static bool     g_crouchBitPend[16]  = {false};
static bool     g_crouchBitQuiet[16] = {false};
// Camera-quiet tracking: a crouch is a ~20 uu vertical move from a STILL
// camera that does not go anywhere horizontally. Blinks, jumps, falls and
// stairs all move the view too, which is exactly how 32.43 talked itself into
// adopting 0x0040 - a blink landed inside the 400 ms window and the learner
// credited it to the last button pressed.
static float    g_camZLast      = 0.0f;
static double   g_camMoveMs     = 0.0;
static uint32_t g_crouchBtnPrev = 0;
static bool     g_crouchBindArm = false;  // overlay: adopt the next button
static volatile LONG g_sneakBtn = 0;      // the crouch bit we send the game
static bool     g_crouchToggle  = true;   // [Hands] CrouchToggle
static bool     g_crouchTogLock = false;  // true once measured
static int      g_crouchTogVote = 0;      // +1 toggle-ish, -1 hold-ish
static bool     g_crouchBtnSt   = false;  // stance derived from the button
static bool     g_sneakWas      = false;
static float    g_crouchZPress  = 0.0f;
static float    g_crouchXPress  = 0.0f;
static float    g_crouchYPress  = 0.0f;
static bool     g_crouchQuietP  = false;
static bool     g_crouchChkPend = false;
static double   g_crouchChkMs   = 0.0;
static double   g_crouchRelMs   = 0.0;
static bool     g_crouchRelPend = false;
static volatile LONG g_skcCalReq = 0;      // overlay: start a 3 s calibration
static bool     g_skcCalGo = false;
static double   g_skcCalUntil = 0.0;
static double   g_skcCalSum[2][3];
static uint32_t g_skcCalN[2];
static void SkcSaveNeutral(const char* why);
static volatile LONG g_skcRecap = 0;
// ---- 30.97: killing the head coupling --------------------------------------
// Space 3 is PARENT BONE space, so the offset we write is measured along the
// forearm - which is animated AND camera-locked. That is exactly why the hands
// still drift with your head: the direction of our offset rotates with the
// arm, and the bone's base position rides the camera regardless.
//
// The fix is WORLD space with ADD OFF, i.e. state the bone's absolute world
// position rather than nudging it. Then nothing upstream of us can move it.
// The target is built from the game's own camera basis (g_camObj +0x50/+0x60/
// +0x70 = fwd/right/up, +0x80 = position) applied to the controller's offset
// from your head: the head rotation appears once in each and cancels exactly,
// instead of being approximated away.
// 30.99: world-space absolute placement measured "worse" in the headset, so it
// is OFF again. Honest reading of why: stating an ABSOLUTE world position for
// the hand bone throws away everything the animation was doing to it, and any
// error in the game-camera basis or the uu/metre scale shows up as the hand
// sitting in the wrong place entirely rather than merely drifting. The offset
// lane keeps the authored pose and only nudges it, which reads as far more
// solid even though it drifts with the head.
static bool     g_skcWorld = false;
static float    g_skcTrim[2][3] = { {0,0,0}, {0,0,0} };
static volatile LONG g_skcHits = 0;
// 31.2: the bitmasks came back EXACTLY as assumed (bApplyTranslation 0x01,
// bApplyRotation 0x02, bAddTranslation 0x04, bAddRotation 0x08), read from
// +0x6c of each BoolProperty. So "replace the translation" really was being
// requested, and the remaining unverified assumption is that space 0 means
// world. This pins the hand to ONE FIXED WORLD POINT: if space 0 is really
// world space the hand should hang in the air as you walk and turn. If it
// follows you, index 0 is not world and the whole world lane was mislabelled.
static bool     g_skcPinTest = false;
static float    g_skcPinAt[3] = { 0, 0, 0 };
static bool     g_skcPinHave = false;
// ---- 31.6: WORLD ROTATION ---------------------------------------------------
// The pin test settled it: with space 0 the user could WALK AWAY from their
// hands, so translation really is world-space and really is absolute. What kept
// following the head was the ROTATION, which we were never driving - the bone
// kept whatever orientation the animation gave it on a camera-locked component.
// So drive BoneRotation too, in world space, from the controller's own
// orientation. FRotator is three INTs (Pitch, Yaw, Roll) at 65536 per turn.
static const uint32_t kSkcRot = 0xd4;      // FRotator BoneRotation
// 32.2: rotation drive OFF by default. Measured: 9,000 writes/second into
// BoneRotation, comfortably outrunning the per-tick recompute (that was the
// FOV lever's whole trick), and the hands STILL rotate with the head. So the
// write is not being lost - BoneRotation on this control simply does not
// govern the bone's world orientation. Position works and is worth keeping;
// rotation is not ours through this lever and pretending otherwise just made
// the hands feel worse.
static bool     g_skcWorldRot = false;
static float    g_skcRollGain = 1.0f;
// 31.7: world space is OFF again, and this time I can say why it puts the
// hands in your face. In replace mode the target is camera position + your
// controller's offset from your head - which is a point about 30 cm in front
// of your eyes, because it throws away where the hand NATURALLY sits (arm's
// length forward, down at your side). Offset mode keeps that authored resting
// pose and only nudges it, which is why it reads as far more solid.
//
// The residual head rotation gets a direct counter instead: yaw the bone back
// by however far your head has turned since neutral. Same idea that failed at
// the GPU upload, except this input is one the engine RESPECTS rather than
// recomputes, so it has a real chance here.
static float    g_skcCounterYaw = 0.0f;    // 0 = off, 1 = full counter-rotation
static float    g_skcYaw0[2] = { 0, 0 };
// 31.8: BoneScale, offset 0xa0 on SkelControlBase - straight off the property
// dump. Scales the controlled bone and everything under it, so shrinking the
// hand takes the held weapon with it. Animation is unaffected: this is a
// property the anim pass consumes, not a transform fighting it.
static const uint32_t kSkcScaleProp = 0xa0;
static float    g_skcHandSize = 1.0f;
// ---- 32.0 -------------------------------------------------------------------
// The user is getting motion sick, and the cause is a CONFLICT, not a bug in
// either half: position follows the controller while orientation follows the
// head. Two contradictory motion cues on one object is worse than either alone
// being wrong, which is why this feels worse than the stock arms.
//
// F9 kills the whole drive instantly - relief must not require finding a
// checkbox in a panel while feeling ill.
//
// And the rotation question gets the same treatment that settled position:
// F7 pins the hands to a FIXED WORLD ROTATION. If they hold their orientation
// while you turn, world rotation works and our controller->rotator maths is
// wrong. If they keep turning with you, the game is overwriting BoneRotation
// every tick - these ARE its look-at controls - and rotation is not ours to
// take without stopping whatever drives them.
static bool     g_skcRotPin = false;
// 33.6: the rotation drive - controller orientation relative to the head,
// written into the hand SkelControls (camera-parented rig => head-relative is
// the ONLY frame that yields world-stable weapons). F7 cycles off/DRIVE/pin.
static bool     g_skcRotDrive = false;
static int      g_skcRotSignY = 1, g_skcRotSignP = 1;  // [Hands] RotSignYaw/Pitch
static float    g_skcRotNeu[2][2] = {{0,0},{0,0}};     // yaw,pitch at END
static bool     g_skcRotNeuOk[2] = { false, false };
// 36.4: MEASURED (36.3 telemetry + the user's "opposite" report): once the
// donor replaces the hosts' look-at output, the hand bones' frame follows
// the head NOT AT ALL - subtracting the head from the command was itself
// the remaining leak, in exact mirror image. So the right command is the
// controller's orientation RAW, relative only to its captured neutral.
static bool     g_graftAimAbs = true;                  // [Hands] GraftAimAbs
static float    g_skcRotNeuAbs[2][2] = {{0,0},{0,0}};  // raw yaw,pitch at zero
// 36.5: the residual head coupling is UPSTREAM of our bone (parent chain /
// pawn yaw / aim-offset animations follow the view by some fraction).
// 36.3+36.4 telemetry bracketed the coefficient but adjectives cannot pin
// it - so it is a SLIDER now, per axis: the user turns their head and drags
// until the weapon holds still. cmd -= HC * (hmdNow - hmdAtZero).
static float    g_graftHCY = 1.5f;   // [Hands] GraftHeadFollowYaw
static float    g_graftHCP = 1.5f;   // [Hands] GraftHeadFollowPitch
static float    g_skcRotNeuHmd[2][2] = {{0,0},{0,0}};  // hmd yaw,pitch at zero
// ---- 32.1: OUTRUN THE RECOMPUTE ---------------------------------------------
// F7 pinned the hands to a fixed world rotation and they STILL turned with the
// head. So the game rewrites BoneRotation every tick - these are its own
// LookAtControl nodes and it drives them - and our once-per-frame write loses
// the race exactly like the FOV fields did before the lever.
//
// The fix is this project's own hardest-won lesson (handoff sec.10): do not
// write engine values once per frame and conclude writes do not work. Enforce
// them on a dispatch that fires thousands of times a second. That is the
// ProcessEvent hook, which is what made the FOV lever hold. Same trick, same
// place, applied to the hand rotators.
static int32_t  g_skcWantRot[2][3];       // pitch, yaw, roll per hand
static bool     g_skcWantRotOk[2] = { false, false };
static volatile LONG g_skcRotWrites = 0;
// The only rotation-related flag we have never set. In UE3 it decides whether
// the mesh's own rotation is factored OUT when a control applies a rotation in
// another space - which is exactly the "should this be absolute" question. No
// claim that it works; it is simply the last untried bit.
static bool     g_skcRemoveMeshRot = false;
// ---- 32.3: the control I kept skipping ---------------------------------------
// There are three: LookAtControl_LeftHand, _RightHand, and _Camera. I routed
// the two hands and deliberately left the camera one untouched, treating it as
// none of our business - for a dozen builds, while the reported symptom was
// "the arms rotate with my camera".
//
// A LookAt control exists to make bones orient toward a target. If that target
// is the view, then THAT is the thing turning the arms with your head, and no
// amount of writing BoneRotation on the hand controls will ever beat it,
// because it is a different control doing exactly what it was authored to do.
// ControlStrength 0 switches a SkelControl off - so this is a dial from
// "stock" to "gone".
static float    g_skcCamStrength = 1.0f;   // LookAtControl_Camera: 1 stock, 0 off
static float    g_skcHandCtlStr[2] = { 1.0f, 1.0f };
static int      g_skcCamIdx = -1;
// ---- 32.4: the probe was the freeze -----------------------------------------
// The auto-probe walks all ~103,000 UObjects calling ObjClassName on each, and
// it re-armed every 3 s for as long as no controls were found - which is the
// entire duration of a level load. Reload quickly and you stack full object-
// table sweeps onto the game thread: hitches, then a freeze, and head tracking
// starving because the thread never gets back. Exactly the class of mistake
// sec.10 warns about ("a stray per-frame GObjects diff once cost 120 ms/frame").
//
// Three brakes: cache the class pointer so re-probes compare pointers instead
// of strings, never probe while a menu/load is up, and back off after repeated
// failures instead of hammering.
static void*    g_skcClsCache = NULL;
static int      g_skcProbeFails = 0;
// ---- 32.6: LIVENESS, not "does it still look like a control" --------------
// Crash trace decoded from the user's Fatal error dialog reads, innermost
// first: pool free-list unlink (0x420ef0) <- FMalloc free (0x4246f8) <-
// UObject property destruction (0x469561/0x47d2ad) <- CollectGarbage
// (0x47db7f - the function carrying the "RF_DisregardForGC assumption" string)
// <- LoadMap (0x641311) <- save-game browse "%s\Save%i.usa?load" (0x637ba6).
// So: load a save -> GC -> destroy objects -> fault inside the allocator.
// That is heap corruption, and we hold cached UObject pointers that we write
// floats into on EVERY ProcessEvent dispatch.
//
// The old guard was worthless for this: it ran once every 2000 ms, checked
// only slot 0, and asked "does the class name still say SkelControl?" - freed
// memory keeps its old contents, so that answers YES right up until the block
// is reused. Two seconds of writes into a freed AnimTree is exactly the
// corruption the allocator then trips over.
//
// The one check that fails at the right moment is GObjects: ~UObject nulls
// GObjObjects(Index) BEFORE the memory goes back to the pool. Remember each
// control's index and class at probe time, and compare both before every
// write. Two loads and two compares - affordable in the ProcessEvent lane.
static uint32_t g_skcObjIdx[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
static void*    g_skcObjCls[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static volatile LONG g_skcStale = 0;
