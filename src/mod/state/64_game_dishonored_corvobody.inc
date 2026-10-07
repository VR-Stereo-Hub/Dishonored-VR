// state/64_game_dishonored_corvobody.inc - the CorvoBody body mod (Nexus 453) meets
// the VR hands at the shoulder (game/dishonored/corvobody.cpp, docs/dishonored/CORVOBODY.md).
// Written on the script lane only; read by the overlay, the seam and status.json.
static bool     g_cbEnabled   = true;    // [CorvoBody] Enabled: drive the body's arm IK when the mod is present
static bool     g_cbHideHands = true;    // [CorvoBody] HideBodyHands: hide the body's own hand bones (our cut hands show)
static float    g_cbStrength  = 1.0f;    // [CorvoBody] ArmStrength: the two arm controls' ControlStrength
static float    g_cbReach     = 0.98f;   // [CorvoBody] ReachClamp: effector kept within this fraction of the arm's length (0 = off)
static int      g_cbOverlayVk = 0;       // [Overlay] Key, parsed; 0 = default (F10, or Insert while CorvoBody is present)
static bool     g_cbPresent   = false;   // dinput8.dll exporting BlinkBootstrap_Register AND CorvoBody.ini next to the exe
static bool     g_cbTheirRigid = false;  // their ini: RigidCamera=1 (the neck-pivot detour; must be 0 in VR)
static bool     g_cbTheirHideArms = false; // their ini: HideBodyArms=1 (shoulders vanish with gear; must be 0 for the attach)
static bool     g_cbTheirEnabled = true; // their ini: Enabled
static double   g_cbDetectMs  = 0.0;     // next detection pass
static double   g_cbScanMs    = 0.0;     // next GObjects walk for the body
static uint8_t* g_cbPawn      = NULL;    // the pawn the body was found on
static uint8_t* g_cbBody      = NULL;    // their visible SkeletalMeshComponent (outer = pawn, mesh *skm_Corvo_Body, not HiddenGame)
static uint8_t* g_cbCtl[2]    = { NULL, NULL };   // SkelControlLimb hand_L_jnt / hand_R_jnt on that body
static float    g_cbArmLen[2] = { 0.0f, 0.0f };   // upper arm + forearm segment lengths (uu), measured once per body
static bool     g_cbHandsHidden = false; // HideBoneByName(hand_*_jnt) was issued on g_cbBody
static int      g_cbShadowSeen = 0;      // HiddenGame copies that matched the mesh name (their shadow body)
// The hand point the SkelControl drive just handed the first-person hand bone, per hand
// (0 = left, 1 = right): the identical world point drives the body's arm. Captured in
// skelcontrol.cpp at the write; space 0 = BCS_WorldSpace, anything else refuses here.
static float    g_cbHandTgt[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
static uint8_t  g_cbHandSpace[2]  = { 0xff, 0xff };
static double   g_cbHandMs[2]     = { 0.0, 0.0 };
// Resolved offsets (names first, SDK numbers as the cross-check; 0 = unresolved)
static uint32_t g_cbOffEff = 0, g_cbOffEffSpace = 0, g_cbOffStrT = 0, g_cbOffSkelMesh = 0;
static uint32_t g_cbOffHidden = 0, g_cbMaskHidden = 0;
static uint8_t* g_cbFnFind = NULL, *g_cbFnHide = NULL, *g_cbFnUnhide = NULL, *g_cbFnBoneLoc = NULL;
static bool     g_cbRefused   = false;   // an offset or function did not resolve as the SDK says: the module refuses for the session
static long     g_cbWrites    = 0;       // effector writes that landed
static long     g_cbClamped   = 0;       // writes shortened by the reach clamp
static double   g_cbLogMs     = 0.0;
static char     g_cbWhy[96]   = "not started";
