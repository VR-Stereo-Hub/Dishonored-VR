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
static bool   g_cineNow   = false;
static double g_cineOnMs  = 0.0;
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
static bool  g_eyeCanted    = false;  // any eye measured non-identity
static bool  g_eyeFrOk = false;
// 38.3 XR-3: the screen geometry BuildEyeQuads last authored, exported for
// the quad-layer submit (metres, head space): size W x H centered (cx, cy)
// at -screenDist.
static float g_bqW = 0, g_bqH = 0, g_bqCx = 0, g_bqCy = 0;
static float    g_ipdM = 0.064f;           // real IPD from OpenVR (meters)
static uint8_t* g_camObj       = NULL;     // cached live camera pointer
static uint32_t g_camNameIdx   = 0;        // its Name index (for revalidation)
static int      g_camRefindIn  = 0;        // frames until we re-find the camera

static uint32_t g_gameFrames = 0;          // game frames rendered this window
static uint32_t g_submits    = 0;          // compositor submits this window
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
