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
// 30.53: how far the presented image may shrink before it magnifies instead.
// 30.55: default 1.0 - anything less leaves BLACK BORDERS during spyglass
// zooms and cutscene/dialogue framing, which is what the user actually sees
// and dislikes. At 1.0 the presentation never shrinks: a narrower render is
// shown across the same lens area, i.e. it magnifies, exactly like the flat
// game. Lower values trade lens coverage for true angular scale.
static float g_zoomFillFloor = 1.0f;
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
static bool     g_sbsMonoNow    = false;   // 30.30: SBS presenting mono (menu/
                                           // wheel/2D frame) - full frame to
                                           // both eyes so flat UI is readable
// 34.6: wrist HUD wiring. The fork (M6.0) redirects UI draws into an
// offscreen HUD texture whenever dxvk_vr_hudwrist is nonzero; the proxy owns
// the policy: on in gameplay, off in menus (so menu screens stay in view).
// dxvk_vr_hudtex is the fork's exported texture pointer - the panel renderer
// reads it in the build after this one.
static volatile unsigned int* g_dxvkHudWrist = NULL;
static IDirect3DTexture9* volatile* g_dxvkHudTex = NULL;
// 35.2: live shadow-fix mode dial (fork M6.5 isolation modes, 0-4). The
// export is read per draw fork-side, so overlay changes apply INSTANTLY -
// the user stands at a broken light and flips modes while looking at it.
static volatile unsigned int* g_dxvkShadowFix = NULL;
// 35.3: planar reflections kill-switch (fork M6.6). The marker capture
// identified the per-eye "light pool" artifacts as the game's mirrored-
// camera reflection pass, rendered once from the mono eye - impossible to
// composite correctly into two eyes. Off = the pass is skipped.
static volatile unsigned int* g_dxvkReflect = NULL;
// 35.4: the artifact bisector (fork M6.7). Diagnostic only, never saved.
static volatile unsigned int* g_dxvkKillMask = NULL;
// 38.49: the artifact bisector, from the INI instead of the F10 overlay.
// The overlay checkboxes have existed since 35.4, but wrestling an ImGui
// panel inside a Quest headset to run an A/B is miserable, and this test is
// two launches. [Debug] KillMask kills whole draw classes live in the fork:
//   1 = samples-rt (the mono full-screen composites - THE lighting suspect)
//   2 = DUP (UI-like duplicated draws)     4 = modulate/darken (sb=DESTCOLOR)
//   8 = additive light draws              16 = mirrored-vp (reflection pass)
//  32 = SPLICE (all world FX - drastic)
// Whichever value makes the artifact VANISH names the draw class that draws
// it. That is the same bisection that found the additive light class before.
static int g_killMaskIni = 0;         // [Debug] KillMask
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
static bool g_wristHud = true;                      // [Hud] WristHud
// 38.47: the user's own idea, and a better one than the front-board and the
// region-split both. Conversation choices are drawn by the same Scaleform
// batch as the rest of the HUD, so they ride the wrist redirect and end up
// tiny on your arm exactly when you have to read them. Rather than move them,
// just STOP REDIRECTING while a conversation is up: the whole UI then draws
// in view at its normal size, like the flat game, and goes back to the wrist
// when the conversation ends. It is one extra condition on the gate that
// already handles menus, mono frames and the power wheel - the same gate the
// main-menu fix hangs off.
static int    g_dlgHudOff  = 1;        // [Hud] DialogHudOff
static volatile LONG g_hudRedirLive = 0;  // 38.50: the redirect gate's verdict
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
static float  g_dlgHoldMs  = 12000.0f; // [Hud] DialogHoldMs
static double g_dlgUntilMs = 0.0;      // armed until (re-armed per event)
// 34.7: the wrist PANEL. The fork's HUD texture is GPU-downscaled
// (StretchRect) to a small RT, read back to the CPU (2.3 MB/frame - the main
// capture is 36 MB, so this is noise), uploaded to a D3D11 texture, and
// composited per eye as a controller-anchored billboard with ADDITIVE blend:
// the HUD RT clears to black each frame, so black = invisible and the HUD
// elements float like a hologram. No alpha correctness needed.
#define HUDRB_W 1008
#define HUDRB_H 568
static IDirect3DSurface9* g_hudSmall = NULL;   // POOL_DEFAULT downscale RT
static IDirect3DSurface9* g_hudSys   = NULL;   // SYSTEMMEM readback surface
static uint8_t*           g_hudPix   = NULL;   // CPU copy, HUDRB_W*HUDRB_H*4
static volatile bool      g_hudPixNew  = false;
static volatile bool      g_hudPixEver = false;
static bool               g_hudRbFail  = false;
static int   g_hudPanelHand = 0;      // [Hud] PanelHand   0=left 1=right
static float g_hudPanelSize = 0.16f;  // [Hud] PanelSize   metres wide
static float g_hudPanelUp   = 0.06f;  // [Hud] PanelUp     metres above ctrl
static ID3D11Texture2D*          g_hudTex11 = NULL;
static ID3D11ShaderResourceView* g_hudSrv11 = NULL;
static ID3D11Buffer*             g_vbHud    = NULL;
static ID3D11BlendState*         g_bsAdd    = NULL;
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
// 38.2 world-anchored screen state: the quad corners as BuildEyeQuads
// authored them (anchor space = "head space when facing the screen"), the
// per-eye uvs, and the room<-anchor pose (yaw-level, set at recenter).
static float g_wsCorn[2][4][3];       // per-eye corner positions
static float g_wsUv[2][4][2];         // per-eye uvs (SBS halves)
static bool  g_wsCornOk = false;
static float g_wsA[9]   = {1,0,0, 0,1,0, 0,0,1};   // anchor rotation (room<-anchor)
static float g_wsPos[3] = {0,0,0};                 // anchor position (room)
// 38.3 XR-3: the screen geometry BuildEyeQuads last authored, exported for
// the quad-layer submit (metres, head space): size W x H centered (cx, cy)
// at -screenDist.
static float g_bqW = 0, g_bqH = 0, g_bqCx = 0, g_bqCy = 0;
static volatile unsigned int* g_dxvkSplices = NULL; // fork's per-frame stereo
                                           // draw count (dxvk_vr_splices)
static volatile float* g_dxvkProj = NULL;  // 30.36: fork's live projection
                                           // scales [xs, ys] of the main VP
static volatile float* g_dxvkView = NULL;  // p53: main camera world FORWARD
                                           // [0..2] + freshness counter [3]
static volatile float* g_dxvkSep  = NULL;  // 30.37: fork's live stereo
static volatile float* g_dxvkConv = NULL;  //        separation / convergence
// 32.6/M3.8: write 1 and the fork dumps the NEXT frame's draws with a splice
// VERDICT per draw ("-> SPLICE" or the condition that failed). The old dump
// armed at frame 1200/6000/12000, which cannot catch "stand in front of the
// fire and look at it".
static volatile unsigned int* g_dxvkDumpReq = NULL;
// 32.45: hand the fork BOTH blink points, camera-relative, so it can name the
// draw that renders each one instead of trying to recognise it by render
// state. [0..2] hand-aimed, [3..5] head-aimed, [6] aim live, [7] match radius.
static volatile float* g_dxvkMark = NULL;
// 32.46: F8 cycles which matched draws the fork withholds. 0 = draw everything,
// 1 = drop the draws sitting on the HEAD-aimed point, 2 = drop the ones on the
// HAND-aimed point. Whatever disappears is what those draws render.
static volatile unsigned int* g_dxvkMarkKill = NULL;
static volatile unsigned int* g_dxvkUvFix = NULL;   // 32.48: Pause key A/B
static bool     g_uvKeyWas = false;
static float    g_ipdM = 0.064f;           // real IPD from OpenVR (meters)
static float    g_liveFovX = 0.0f;         // horizontal FOV derived from xs;
                                           // sizes the screen from what the
                                           // game ACTUALLY renders (kills
                                           // cutscene zoom + FOV-config warp)
static bool     g_dxvkSplicesTried = false;
// 32.71: SEQ - frame-sequential per-eye, the answer to "full res in each eye,
// not split". The side-by-side frame gives each eye half the width, and the
// game's frame is only ~1000 lines tall against a ~2240-line eye texture, so
// BOTH axes were being upscaled about 2.2x. Splitting one frame between two
// eyes cannot fix that; not splitting it can. In SEQ the fork shifts the whole
// frame for one eye, we store that whole frame as that eye's image, and the
// eyes alternate - which is exactly what the decoupled-AER path below was
// already built to do. Per-eye source resolution goes from (W/2 x H) to
// (W x H) at no GPU cost, because the fork now draws once where it drew twice.
static bool     g_seqMode       = false;   // dxvk_stereo.txt contains seq=1
static bool     g_seqMonoNow    = false;   // menus/loading: same frame both eyes
static volatile unsigned int* g_dxvkSeq     = NULL;
static volatile float*        g_dxvkEyeSign = NULL;
static bool     g_sbsMode       = false;   // 30.29: dxvk_stereo.txt present ->
                                           // the DXVK fork renders true SBS
                                           // stereo per frame; proxy stops AER
                                           // shear/alternation and feeds each
                                           // eye its half of the backbuffer
static float    g_sepUU        = 4.0f;     // half-separation in Unreal units
static bool     g_f7WasDown    = false;
static uint8_t* g_camObj       = NULL;     // cached live camera pointer
static uint32_t g_camNameIdx   = 0;        // its Name index (for revalidation)
static int      g_camRefindIn  = 0;        // frames until we re-find the camera
static int      g_curEye       = 0;        // eye the current game frame is for
static int      g_writtenEye   = -1;       // eye we last offset the camera for
static bool     g_stereoDiagDone = false;
static float    g_prevWriteLoc[3] = {0,0,0};
static bool     g_havePrevWrite = false;

// --- Stage 3.3 render-time stereo (shear the view-projection constant) -------
// Scan found the VP matrix at register c0 (row-major, translation in row 3).
// Per eye we apply the Nvidia-3D-Vision separation+convergence shear to a COPY
// of the constants (never game memory). AFR: left on even frames, right on odd.
static int      g_stereoReg    = 0;        // VP constant register (ini)
static float    g_sepClip      = 0.020f;   // separation in clip/NDC units (ini/keys)
static float    g_converge     = 120.0f;   // convergence depth in clip.w units (ini)
static bool     g_stereoTranspose = false; // try if row-major shear looks wrong (ini)
static int      g_drawEye      = 0;        // eye the current frame's draws target
static bool     g_f10Was = false, g_f11Was = false, g_f12Was = false;
static uint32_t g_shearHits = 0;           // how many draws we sheared this window
static uint32_t g_gameFrames = 0;          // game frames rendered this window
static uint32_t g_submits    = 0;          // compositor submits this window
static long long g_qpcFreq   = 0;          // QueryPerformanceCounter frequency
// 38.14: [VR] FpsCap - even-cadence frame limiter (0 = off). Set 72 with
// VD at 72 Hz, or 45 with VD at 90 Hz. Works on any backend but exists for
// the Quest stutter (game fps wandering 66-80 vs a fixed display rate).
static float     g_fpsCap    = 0.0f;
static long long g_lastSubmitQpc = 0;      // timestamp of last compositor submit
static bool      g_haveSubmitted = false;

// --- Stage 5.0 positional head tracking (lean/peek/crouch) -------------------
// Your head's real position offset (vs a re-centerable reference) is folded
// into the SAME c0 view-projection we already shear for stereo, as a pure
// view-space translation. No rotation math (that's what went wild in the old
// F3 experiments), no camera-object writes — just the proven machinery fed
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
// 38.62 SPECTATOR MIRROR - the streamer feature. The desktop window shows
// the raw SBS pair, which no viewer wants and which OBS layers can never
// capture on this game anyway (Dishonored is 32-bit; the OpenXR capture
// layers ship 64-bit, and our loaderless XR has no layer seam to begin
// with). We own the backbuffer, so after the VR path has consumed it the
// window content is rewritten to ONE eye, centre-cropped to 16:9 - a Game
// Capture of Dishonored in OBS is the entire streamer setup.
// [Screen] MirrorMode: 0=sbs (unchanged), 1=left eye, 2=right eye.
static int g_mirrorMode = 0;          // [Screen] MirrorMode
// 38.64: the eye is PORTRAIT (2016x2268) - the 16:9 crop threw away a third
// of the view ("it's cut off"). Default now shows the WHOLE eye pillarboxed
// on black; MirrorAspect crops for taste (1.78 = the old behaviour).
// MirrorHud stamps the wrist-HUD texture into the corner so streams show
// health/mana - the panel is drawn into the HEADSET's eye textures and was
// never in the game backbuffer at all.
static float g_mirrorAspect = 0.0f;   // [Screen] MirrorAspect (0 = full eye)
static int   g_mirrorHud    = 0;      // [Screen] MirrorHud (user: off)
static IDirect3DSurface9* g_specTmp = NULL;
static UINT g_specW = 0, g_specH = 0;
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
