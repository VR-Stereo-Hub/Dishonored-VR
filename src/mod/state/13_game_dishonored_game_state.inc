// State chunk 13 for game/dishonored/game_state - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

// The "hands in your face" in world mode was a SCALE error, not a design flaw:
// at 50 uu/m a 0.6 m arm reach becomes 30 uu, which in a game built at roughly
// 1 uu per cm is 30 cm - right at your eyes. World placement wants ~100.
static float    g_skcWorldScale = 100.0f;
static void CameraTraceTick();           // 32.20: defined much later
static void BoneWigBuildMask();          // defined after FindRefSkel
static volatile bool g_boneWigGo = false;
static int    g_bwPhase = -1;
static double g_bwNext  = 0.0;
// 30.54: the game ignores input when its window is not focused, and SteamVR's
// status window steals focus without the user (who is in the headset) ever
// seeing it - controllers just go dead. Take focus back, but ONLY from
// SteamVR's own windows: an alt-tab to a browser is deliberate and is left
// alone. [Input] AutoFocus, plus a manual button on the overlay.
static bool     g_autoFocus  = true;
static volatile bool g_focusNow = false;
static volatile bool g_fovHuntGo = false;  // 30.48: scan ctrl/pawn/cam for the
                                           // LIVE FOV field (overlay button)
static volatile bool g_fwigGo   = false;   // 30.47: FOV test runs ON DEMAND
                                           // (overlay button) - the auto-start
                                           // trigger fired at the MAIN MENU
                                           // (background pawn), so both tests
                                           // completed before the user loaded in
static bool     g_csRecon       = true;    // 30.40: [CamSeam] Recon - camera
                                           // seam recon hooks (read-only)
// 38.69 INTRO BOAT SKIP. The intro's boat->dock handoff never runs in VR
// (measured across five runs: the player is never seated in the ride
// keyhole, PVR fires free-look from second one, and the arrival script
// hides the boat under a standing pawn - straight into the water). Until
// the seat-in breakage is found, the shipped game's OWN developer debug
// transitions (DefaultDebugMenu.ini, Kismet 'ce' remote events) jump past
// the broken arrival with mission state set up by the devs themselves.
//   [Debug] IntroSkip = 0 off
//     1 = 'ce ChangeLvl_fromPrison_toTower'  (Dunwall Tower entry - the
//         dev "Skip to Dunwall Tower"; prologue content preserved if it
//         lands past the dock)
//     2 = 'ce ChangeLvl_fromTower_toPrison'  (prison cell - guaranteed
//         past the prologue, loses the tower scenes)
// Armed only by a real New Game click; fires once, after the intro pawn
// has been up for IntroSkipDelayMs.
static int    g_introSkip        = 0;      // [Debug] IntroSkip
static int    g_introSkipDelayMs = 8000;   // [Debug] IntroSkipDelayMs
static bool   g_introSkipDone    = false;
// 38.71: transition 1 half-fired from the boat: the Kismet event exists but
// its receiver is wired for a prison->tower trip, so it raised cinematic
// mode (63 ms later, measured), never changed the level, and OUR cine latch
// then parked the user's controllers for the ride ("I lost access to my
// controller inputs"). If a fired skip produces no new pawn within 15 s it
// is declared failed and any cine latch it raised is cleared.
static double g_introSkipFiredMs = 0.0;
static bool   g_introSkipJudged  = false;
static int    g_introSkipTries   = 0;      // 38.72: fires, max 3
static double g_introSkipRetryMs = 0.0;
static int g_swarmAim    = 1;         // 38.55: [MotionAim] SwarmAim
static double g_lastVersusMs = 0.0;        // 38.51: last block-input event
// 38.65 THE BOAT DEATH. The intro cutscene killed a fresh new game: the
// player settling into their chair fired a physical-crouch B pulse, and 16ms
// later Dis_ExitKeyhole ejected Corvo from the scripted boat view - loose
// pawn during the script, dropped in the water, dead on a first impression.
// The game ANNOUNCES cinematics (OnToggleCinematicMode, a toggle on the
// player controller); while one is running, every synthesized gameplay
// input goes quiet: buttons except START (pause must always work), sticks,
// physical-crouch pulses, roomscale. Safety: the latch self-expires after
// 8 minutes in case an off-toggle is ever missed.
// ---- VR-62: DOUBLE ON SCENE LIVENESS, NOT ON A FIVE-TERM VERDICT -----------
//
// The scene-draw doubling gate asked DvrGameplayVerdict(), which is five terms
// wide. It only needs one thing to be safe: that the scene is being drawn and
// presented one-to-one, because the hazard it guards against is a MENU, whose
// draws outnumber its presents and so break the pair schedule.
//
// Two of those five terms are slow by construction, and both were measured
// holding the picture mono after a load:
//
//   menuOpen - set by a Dis_OpenPauseMenu dispatch during a load when no menu is
//              open at all. A ghost.
//   viewLive - leaving LOADING deliberately requires a full second of continuous
//              dispatches, so it cannot clear sooner than 1 s however ready the
//              scene is. Measured at +1.52 s and +1.72 s on two loads.
//
// Neither says anything about whether the scene is drawing. The downstream
// gates already test that honestly - a c5 camera upload since the previous
// draw, and a present since the previous draw - and those two together ARE the
// pairing precondition.
//
// So with this on, a verdict refusal is overruled when the scene is
// demonstrably live. The guards that remain are the ones that name a real menu
// rather than a slow one: the main menu, a cinematic, and a live pawn.
// FALSIFIED IN A HEADSET, 2026-09-08, and default OFF because of it. With this
// on, the hands and weapons FLASHED in the background of the pause menu. The
// camera upload serial keeps moving while a menu is up - the world is still
// being rendered behind it - so "the scene is drawing" cannot tell a pause menu
// from a load, and doubling during a menu is exactly the hazard the verdict was
// guarding: a menu's draws outnumber its presents, so the pair schedule breaks.
//
// The idea is not dead, the SIGNAL was wrong. The code already records the right
// discriminator elsewhere: a pause menu silences the view-rotation dispatches,
// while a load keeps dispatching about once a second. A gate built on dispatch
// recency rather than on c5 movement would separate them - but it needs a window
// wider than the 1 s load rate, which means a pause would stay doubled for that
// window, so it trades one artifact for another and needs measuring before it
// ships. Kept as a lever so that measurement is one ini key away.
// VR-16/VR-62: THE AUTHORITATIVE EYE FOR THE DRAW THAT IS RUNNING RIGHT NOW.
//
// -1 during pass 1, +1 during pass 2, 0 when the tick is not being doubled. The
// re-entry method draws the world twice per game tick with the camera moved
// between them, so inside a draw the eye is not something to infer: it is a
// decision this code made microseconds earlier on this thread.
//
// It exists because the hand and weapon correction infers the eye instead, from
// a jump in the draw's own LocalToWorld translation, keyed on the PRESENT count.
// That clock is wrong twice over - there are about 160 presents per second
// against 81 doubled draws - and when the inference misses, every hand and
// weapon draw in that frame takes a full inter-pupillary offset in the wrong
// direction. That is a one-frame jump, on both weapons at once, mirrored between
// the eyes, which is exactly the reported flicker. Published here so the audit
// can compare the two without either one moving.
static int   g_sdEyeNow = 0;
// ... and the two facts that make it ATTRIBUTABLE rather than merely current.
//
// The first audit read g_sdEyeNow as a bare global, and a bare global cannot say
// whether the draw asking is inside the pass that set it or merely running after
// it. That matters: 89% of the corrected draws in the first run had no pass at
// all to compare against, so the population the disagreement was measured over
// was never established. A counter is not evidence until you know its
// population, and this is the missing half.
//
// g_sdDoubleTick says the current game tick is being drawn twice; the depth
// counter and the draw thread say the caller is lexically INSIDE one of those
// two draws. Together with the camera seam's per-thread second-pass latch, a
// draw can name its own eye with no inference and no race.
// The THREAD that is currently inside a doubled viewport draw, or 0. A draw hook
// that finds its own thread id here is lexically inside one of the two passes;
// anything else is a pass we do not own and must stay UNKNOWN.
static volatile LONG g_sdInDrawTid = 0;

// IS THE METHOD PRODUCING PAIRS AT ALL? Readable from any thread, which the
// marker above is not.
//
// This is the guard the alternation needed and did not have. In MONO there is
// one camera, so the sideways step between presents is zero every time - which
// the eye inference reads as "too small to tell", on EVERY present. Holding was
// harmless there; alternating was not, and it flipped the hands by a full
// inter-pupillary distance every single frame. Measured: four windows of 565,
// 396, 350 and 293 consecutive unreadable presents, all of them mono.
//
// A mono frame has no eye. The correction must apply no eye offset and must not
// alternate anything.
static volatile LONG g_sdDoublingNow = 0;
// VR-65: the pair both passes of this tick belong to. Minted at pass 1 so a
// left and a right image can be proved to be the same tick rather than assumed
// to be by their order in a queue.
static uint32_t g_sdPairId = 0;
static bool  g_sdGateSceneLive = false;    // [Stereo] GateOnSceneLive
static float g_sdSceneQuietMs  = 400.0f;   // [Stereo] SceneQuietMs
static LONG  g_sdGatedByScene  = 0;   // doubled on scene liveness, not the verdict
static double g_sdC5LastMoveMs = 0.0;
static uint32_t g_sdC5LastSerial = 0;

// VR-62: clear a ghost menu flag on dispatch RECENCY rather than a cumulative
// count. OFF restores the count, which is what held the picture mono for 24 s
// on a measured load, so the two compare directly in a headset.
// FALSIFIED IN A HEADSET, 2026-09-08, and default OFF because of it. Clearing on
// dispatch recency does collapse the ghost from 24 s to about 1.5 s - but it
// also clears at the MAIN MENU, which then goes stereo after a few seconds.
//
// ENGINE_NOTES already recorded this exact failure at 38.17: the main menu's 3D
// background keeps dispatching view rotations, so "dispatches still flowing"
// does not separate it from gameplay, and the clearer firing there swept the
// menu UI onto the wrist panel. The cumulative count was slow ENOUGH to hide
// that; recency is not. The 24 s clear that looked like a bug was partly the
// clearer being correctly slow at a main menu.
//
// So the count stays until something separates a main menu from a load without
// relying on dispatch flow at all. CylTruthLive was supposed to be that
// discriminator and the run shows it is not sufficient.
static bool   g_menuGhostByRate  = false;   // [Menu] GhostClearByRate
static double g_menuGhostQuietMs = 400.0;   // [Menu] GhostQuietMs
static bool   g_cineNow   = false;
static double g_cineOnMs  = 0.0;
// VR-73: the engine's own input locks. The latch above is a parity flip of
// OnToggleCinematicMode, but that action has Enable, Disable and Toggle inputs,
// and at the Dunwall Tower arrival the flip left the headset on the mono quad
// (and the pad parked) until a pause. Every toggle ends in SetCinematicMode, so
// after one has run these fields ARE the game's state; they may only CLEAR the
// latch. Read on the object the toggle was dispatched on.
static uint32_t g_cineFlagOff = 0, g_cineFlagMask = 0;     // PlayerController.bCinematicMode
static uint32_t g_cineMoveOff = 0;                         // PlayerController.bIgnoreMoveInput (byte counter)
static uint32_t g_cineDisMoveOff = 0, g_cineDisMoveMask = 0; // PlayerController.bCinemaDisableInputMove
static uint32_t g_cineDisIgnOff = 0, g_cineDisIgnMask = 0; // DishonoredPlayerController.m_bInputIgnoreInput_Cinematic
static int      g_cineFlagState = 0;         // 0 unresolved, 1 at least one field resolved, -1 none
static uint8_t* g_cineCtrl      = NULL;      // the object the last toggle was dispatched on
static bool     g_cineFlagSeen1 = false;     // a lock read set since the latch last went ON
static double   g_cineFlag0Ms   = 0.0;       // when every lock first read clear after that
static int      g_cineLastMask  = -1;        // the lock set last logged, for change-only lines
static bool CylTruthLive();              // fwd
// per-eye frustum + eye offsets, cached by BuildEyeQuads for the panel math
static float g_eyeFr[2][4];           // l, r, t, b
static float g_eyeOffs[2][3];         // ex, ey, ez
// 38.1: full eye-to-head ROTATION (row-major 3x3), identity until measured.
// 37.8 and earlier silently DISCARDED this (the validity check tolerated up
// to ~36 deg of cant while keeping only the translation) - if a runtime
// reports canted per-eye transforms, that is a static per-eye skew in every
// scene frame: the prime suspect for the Quest/Steam Link warp.
static float g_eyeRot[2][9] = {{1,0,0, 0,1,0, 0,0,1},{1,0,0, 0,1,0, 0,0,1}};
static bool  g_eyeFrOk = false;
static float    g_ipdM = 0.064f;           // real IPD from OpenVR (meters)
static uint8_t* g_camObj       = NULL;     // cached live camera pointer
static uint32_t g_camNameIdx   = 0;        // its Name index (for revalidation)
static int      g_camRefindIn  = 0;        // frames until we re-find the camera

static uint32_t g_gameFrames = 0;          // game frames rendered this window
static long long g_qpcFreq   = 0;          // QueryPerformanceCounter frequency
// 38.14: [VR] FpsCap - even-cadence frame limiter (0 = off). Set 72 with
// VD at 72 Hz, or 45 with VD at 90 Hz. Works on any backend but exists for
// the Quest stutter (game fps wandering 66-80 vs a fixed display rate).
static float     g_fpsCap    = 0.0f;

// --- Stage 5.0 positional head tracking (lean/peek/crouch) -------------------
// Your head's real position offset (vs a re-centerable reference) is folded
// into the SAME c0 view-projection we already shear for stereo, as a pure
// view-space translation. No rotation math (that's what went wild in the old
// F3 experiments), no camera-object writes - just the proven machinery fed
// with live head position. F4 toggles, F5 re-centers.
static bool  g_posTrack   = true;     // ini [PosTrack] Enabled, F4 toggle
static float g_posScaleUU = 50.0f;    // game units per meter (UE3 ~1uu=2cm)
static bool  g_posFlipX   = false;    // ini escape hatch if lean X is inverted
static float g_posMaxM    = 0.80f;    // clamp radius in meters (safety)
// 41.1 [Neck]: the pitch pivot lever (docs/KNOWN_ISSUES: looking up or down
// pitched about a point behind the eyes). A real head pitches about a point
// below and behind the eyes, so pitching translates the eye on an arc; the
// term models that arc from the tracked head ROTATION. Mode 0 off (the tracked
// displacement alone), 1 add (the arc added: a rig without positional tracking,
// or an arc the tracking is not supplying), 2 cancel (the arc SUBTRACTED from
// the tracked displacement: the engine pitches about its own neck, so the
// tracked arc must not be applied twice - the pivot then holds the ENGINE's
// numbers from `camera pitchtest`). Under a projection layer only.
static int   g_neckMode    = 0;
// MEASURED 2026-09-03 (camera pitchtest, simulator, the sewers): the ENGINE
// pitches its camera about a point 0.321 m below and 0.062 m behind the eyes
// (17 cm of backward travel at +30 deg), consistency 0.3 uu; `neck cancel`
// with these numbers held the render camera still (< 0.5 uu) while the head
// pitched at a fixed position. A human neck for `add` is about 0.11 / 0.09.
static float g_neckBelowM  = 0.321f;  // the pivot below the eyes, metres (the engine's, measured)
static float g_neckBehindM = 0.062f;  // the pivot behind the eyes, metres (the engine's, measured)
static volatile float g_neckArcUu[3] = {0, 0, 0};   // the last term: right, up, forward (uu)
// VR-78 [Neck] CrouchPivotBelowM / CrouchPivotBehindM: the ENGINE's pivot while
// plainly crouched. MEASURED 2026-09-12 (the VR-78 accounting probe, headset,
// one 23 s crouched sweep): crouched, the engine's camera height moved 0.0 uu
// across +-33 deg of pitch where the standing pivot predicts 9.5, so the engine
// has no neck arc crouched and `cancel` with the standing numbers subtracted an
// arc that was not there. -1 = the standing numbers (the pre-VR-78 behaviour).
static float g_neckCrouchBelowM  = -1.0f;
static float g_neckCrouchBehindM = -1.0f;
static float g_neckStanceBlendMs = 150.0f;   // NOT measured: the ease between the two pivots on a stance change
static float g_neckStanceW       = 0.0f;     // 0 standing pivot .. 1 crouched pivot, eased
static int   g_neckStanceTarget  = 0;
static float g_neckEffBelowM = 0.321f, g_neckEffBehindM = 0.062f;   // the pivot the last arc used
// 38.46 ROOMSCALE. Positional tracking was LEAN only: your real-world offset
// slid the CAMERA, clamped to MaxMeters, and the pawn never moved - so every
// real step drifted you off-centre until you recentred by hand. Real
// roomscale has to move the PAWN, and collision-correctly: raising the camera
// clamp would just walk your view through walls.
// So walking pushes the STICK. The head's horizontal offset from the
// reference, in the head's own yaw frame, is added to the movement stick and
// the game walks Corvo there through its own collision. Meanwhile the
// reference BLEEDS toward your head, consuming the offset - that bleed is the
// auto-recenter. Under the deadzone nothing is injected, so leaning to peek
// still works exactly as before.
static int   g_roomScaleCfg = 1;      // [PosTrack] RoomScale
static float g_roomDeadM    = 0.14f;  // [PosTrack] RoomDeadM   lean stays lean
static float g_roomGain     = 2.4f;   // [PosTrack] RoomGain    stick per metre
static float g_roomMaxStick = 0.90f;  // [PosTrack] RoomMaxStick
static float g_roomBleedMS  = 0.90f;  // [PosTrack] RoomBleedMS m/s ref follows
static float g_roomFwdM = 0.0f, g_roomRightM = 0.0f;   // raw, head-yaw frame
// 30.79: REAL crouching. Positional tracking only lowers the VIEW - the pawn
// stays standing, so ducking under a table put your eyes below it while Corvo's
// collision was still upright and you could not actually get under. Physically
// dropping below the standing reference now holds the game's own sneak/crouch,
// so the body goes down with you.
static bool  g_crouchOn = true;          // [Tracking] PhysicalCrouch
static float g_crouchDropM = 0.22f;      // metres below the reference to trigger
static float g_crouchReleaseM = 0.14f;   // rise back above this to stand (hysteresis)
static volatile bool g_crouchHeld = false;
static float g_crouchRef = 0.0f;
static bool  g_crouchRefOk = false;
static bool  g_posHaveRef = false;    // captured a reference origin yet?
// 38.15 CROUCH EYE HEIGHT. Root cause (user-measured + 32.94 reinterpreted):
// the camera OBJECT's height never changes with stance (camZ-pawnZ flat
// 76-78 both stances - filed as a "dead signal" when it was the bug), and
// our view injection rebuilds the view from that object, so the game's own
// downstream eye-drop never reaches the headset - you crouch and your eyes
// stay at standing height, clipping into whatever you ducked under. Fix:
// drive the drop ourselves from the stance oracle (collision cylinder,
// 87.5 standing / 65 crouched / 33 vents): drop = (87.5 - cyl) uu, smoothed
// ~300 ms like the game's own transition, folded into the positional lean.
// [PosTrack] CrouchEyeDrop=0 disables; CrouchEyeScale trims the depth.
static int   g_crouchEyeCfg   = 1;
static float g_crouchEyeScale = 1.0f;
static float g_crouchDropUU   = 0.0f; // smoothed current drop
// 38.16 DEEP CROUCH - the user's "body won't move forward": the game's
// crouched capsule is 2x65=130uu tall and vanilla refuses under-furniture
// spots outright (vents pass because the GAME drops the cylinder to 33 -
// proof the engine tolerates small capsules). While the game itself says
// crouched (50..76), we shrink the capsule to DeepCrouchUU. State machine
// never fights the game: >76 = standing (hands off), <50 not-ours = vent
// (the game's forced-low owns it), ours = keep. The game's own uncrouch
// headroom check prevents standing into a ceiling, and physical-crouch
// pulses already stand down below 50 (the vent rule). Fall-through guard:
// value clamped >= 33 (the game's own vent height).
static int   g_deepCrouchCfg = 1;     // [PosTrack] DeepCrouch
static float g_deepCrouchUU  = 45.0f; // [PosTrack] DeepCrouchUU (half-height)
static float g_dcOurs        = 0.0f;  // last value WE wrote (0 = not ours)
static float g_cylLast = -1.0f;       // 38.19: last good cylinder read
static int   g_crouchHideCfg   = 1;      // [Hands] CrouchHideArms
static float g_crouchHideCyl   = 76.0f;  // [Hands] CrouchHideCyl
static float g_crouchHideScale = 0.02f;  // [Hands] CrouchHideScale
static bool  g_armsHidden      = false;
// VR-31 route (a): per-bone hiding through the engine's own BoneVisibilityStates.
// Back to OFF: the verdict is recorded (route (a) is closed on three
// instruments, ENGINE_NOTES "VERDICT (2026-09-06, run 4)"), so the lever has
// nothing left to prove and its diagnostics cost a GObjects walk and an
// FpCollect per launch. `arms vis status` runs the whole diagnostic on demand.
static int   g_boneVisCfg      = 0;      // [Hands] BoneVisHide
// VR-31 route (d): the material census. Read-only, and it is the evidence the
// route decision is waiting on, so it ships ON.
static int   g_matCensusCfg    = 1;      // [Hands] MatCensus
// The automatic A/B. Ships ON: the tester is not sending commands during a run,
// so the mod runs the experiment on itself and restores everything after.
// Route (d) is PROVEN, so the timed sweep goes back off - it fought the
// cycler and its job is done. The numpad cycler is the way to identify a
// section now, at a human pace.
static int   g_matAutoCfg      = 0;      // [Hands] MatAuto
static int   g_matCycleCfg     = 1;      // [Hands] MatCycle (Numpad 1/2/3)
static int   g_handFloorCfg = 1;      // 38.27: [Hands] HandFloor
static float g_handFloorMargin = 4.0f;// 38.27: uu above the capsule bottom
static int   g_crawlTuckCfg = 1;      // 38.19: [Hands] CrawlTuck
static int   g_eyeClampCfg = 1;       // 38.24: [PosTrack] EyeClamp
static float g_eyeClampMargin = 8.0f; // 38.24: uu below the capsule top
static float g_eyeClampRate = 300.0f; // 38.26: [PosTrack] EyeClampRate, uu/s
static float g_ecCeil       = 0.0f;   // the eased clamp ceiling (world Z)
static double g_ecCeilMs    = 0.0;
static bool  g_ecCeilOn     = false;
// 38.22 SLIDE ASSIST. Measured chain: getting under tight spots (the boat,
// per the user's flat-game memory) is a SLIDE - B while sprinting
// (Dis_SneakOrSlide -> StartCrouch -> cylinder 33). Trivial on keyboard
// (Shift+CTRL), a finger-twister in VR (stick-click sprint + B while
// moving) - which is the whole "hit or miss". Fix: B pressed at FULL
// forward stick = hold the sprint bit and deliver the crouch a beat later,
// so the game reads sprint+crouch = slide. In flat play that exact input
// already produces a slide, so there are no false positives by definition.
// [Input] SlideAssist=0 disables.
static int    g_slideAssist = 1;

// 38.28 -------------------------------------------------------------------
// THE CROUCH SAGA'S ACTUAL ROOT CAUSE, measured in dishonored_vr_3825crawl:
//   18908421  L-stick click (sprint) DOWN -> script: DisToggleSprint
//   18909062  B                           -> Dis_SneakOrSlide -> cyl 33
// Dishonored's sprint is a TOGGLE (DisToggleSprint). One click latches it ON
// and it stays on - there is not a second click anywhere in the rest of that
// log. So sprint was latched for the whole session and EVERY crouch press was
// producing a SLIDE, not a crouch. That single fact explains the entire saga:
// the cylinder sitting at 33 (slide) instead of 65 (crouch); full stick with
// spd=0 (a slide is a scripted move that ignores the stick - "I get stuck");
// the camera inside the table (eye at crouch height, capsule at slide
// height); and the arms underground (the clamp drops 53 uu at cyl 33 versus
// 21 uu at cyl 65). Nothing was ever wrong with crouch.
// It is VR-specific because pushing a thumbstick to full deflection - exactly
// what crawling under furniture requires - is how you click it by accident,
// and a latched toggle gives no feedback that you did.
// THE FIX: hold-to-sprint. Pulse the game's toggle on the PRESS edge and
// again on the RELEASE edge, so sprint lasts exactly as long as you hold the
// click and an accidental click can never latch. [Input] SprintHold=0 = the
// old raw pass-through.
static bool   g_sprintHoldCfg = false;  // [Input] SprintHold - 38.29: DEFAULT OFF (rolled back)
static float  g_sprintPulseMs = 130.0f; // [Input] SprintPulseMs
static bool   g_spWas         = false;
static double g_spPulseUntil  = 0.0;
static bool   g_spTold        = false;
static float g_posRefX = 0, g_posRefY = 0, g_posRefZ = 0; // tracking-space ref (m)
static volatile float g_leanRightUU = 0;  // view-space right offset, game units
static volatile float g_leanUpUU    = 0;  // view-space up offset, game units
static volatile float g_leanFwdUU   = 0;  // 30.35: view-space forward offset -
                                          // the missing third axis. Forward
                                          // head translation shrinks clip w/z
                                          // (near objects grow), completing
                                          // 3-axis positional parallax.
static bool  g_f4Was = false, g_f5Was = false;

// --- Stage 6.0 virtual-gamepad config (read in LoadConfig; logic lives later)
static bool     g_padEnabled  = true;      // ini [Controllers] Enabled
static bool     g_padHaptics  = true;      // ini [Controllers] Haptics
static float    g_padDeadzone = 0.12f;     // ini [Controllers] Deadzone
static volatile bool g_inMenu = false;     // cursor visible = menu open (auto)
static bool     g_cursorGhost = false;     // 32.47: cursor stuck on in gameplay
static bool     g_f9Was = false;
// 32.9: is the game in a WINDOW rather than exclusive fullscreen? Set from
// D3DPRESENT_PARAMETERS at CreateDevice and every Reset. It matters because
// the menu detector's cursor half is only meaningful in fullscreen: with the
// game in a window the desktop cursor reads CURSOR_SHOWING forever, the
// detector concludes "menu open" forever, and three things break at once.
static volatile bool g_gameWindowed = false;
static volatile bool g_wheelHeld = false;  // power wheel held open (pad action)

// --- Stage 7.0 fire-aim tracer config (read in LoadConfig) -------------------
static bool g_fireTraceEnabled = true;     // ini [Debug] FireTrace

// ----------------------------------------------------------------------------
// Logging
// ----------------------------------------------------------------------------
// 38.90 THE MICROSTUTTER. This used to fflush EVERY line - a synchronous
// disk write, holding the log lock, on the game thread. Measured on the dev
// rig: 54 lines inside ONE frame (NPC volume/BaseChange bursts), i.e. 54
// blocking disk flushes in a single frame. That is a guaranteed hitch, and
// it scales with how many actors move at once - which is why it reads as
// random microstutter. Now the stream is buffered and flushed at most 5x a
// second; LogFlush() forces one where losing the tail would matter (crash
// handler, shutdown). Worst case on a hard kill: the last <200 ms of log.
