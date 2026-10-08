// state/64_game_dishonored_corvobody.inc - the CorvoBody body mod (Nexus 453) meets
// the VR hands at the shoulder (game/dishonored/corvobody.cpp, docs/dishonored/CORVOBODY.md).
// Written on the script lane (the ProcessEvent hook and the post-Tick chain) except the
// two IK publications, which the render lane stores and the script lane loads.
static bool     g_cbEnabled   = true;    // [CorvoBody] Enabled
static int      g_cbArmMode   = 0;       // [CorvoBody] ArmMode: 0 auto (vr while [Hands] ArmIK is on, else body), 1 vr, 2 body
static bool     g_cbHideHands = true;    // body mode: hide the body's hand bones (our cut hands show at the wrist)
static float    g_cbStrength  = 1.0f;    // body mode: the arm controls' ControlStrength
static float    g_cbReach     = 0.98f;   // body mode: effector kept within this fraction of the arm's length (0 = off)
static bool     g_cbHeadAnchor = true;   // [CorvoBody] HeadAnchor: the body's shoulders sit on the arm IK's shoulder centre (head-relative, body yaw)
static bool     g_cbAnchorZ   = true;    // [CorvoBody] AnchorZ: the height from the head too (measured 2026-10-07: CorvoBody's own placement put the shoulder joints AT eye height); 0 = CorvoBody's own height
static bool     g_cbTorsoYaw  = true;    // [CorvoBody] TorsoYaw: the body faces the arm IK's body yaw (25 deg deadzone to the head), 0 = the head
static float    g_cbBodyFwdCm = 0.0f;    // [CorvoBody] BodyForwardCm / BodyRightCm / BodyUpCm: trims on top of the IK shoulder centre
static float    g_cbBodyRightCm = 0.0f;
static float    g_cbBodyUpCm  = 0.0f;
static bool     g_cbMatchWidth = true;   // [CorvoBody] MatchShoulderWidth: set the IK's ArmShoulderWidthCm to the body's measured spacing, once per body
static int      g_cbOverlayVk = 0;       // [Overlay] Key, parsed; 0 = default (F10, or Insert while CorvoBody is present)
static bool     g_cbPresent   = false;   // dinput8.dll exporting BlinkBootstrap_Register AND CorvoBody.ini next to the exe
static HMODULE  g_cbModule    = NULL;    // their module (for the post-Tick chain's target check)
static bool     g_cbTheirRigid = false;  // their ini: RigidCamera=1 (the neck-pivot detour; must be 0 in VR)
static bool     g_cbTheirHideArms = false; // their ini: HideBodyArms=1
static bool     g_cbTheirEnabled = true; // their ini: Enabled
static double   g_cbDetectMs  = 0.0;     // next detection pass
static double   g_cbScanMs    = 0.0;     // next GObjects walk for the body
static uint8_t* g_cbPawn      = NULL;    // the pawn the body was found on
static uint8_t* g_cbBody      = NULL;    // their visible SkeletalMeshComponent (outer = pawn, mesh *skm_Corvo_Body, not HiddenGame)
static uint32_t g_cbBodyName  = 0;       // its name index (identity, with IsLiveObject)
static uint8_t* g_cbCtl[2]    = { NULL, NULL };   // SkelControlLimb hand_L_jnt / hand_R_jnt on that body
static float    g_cbArmLen[2] = { 0.0f, 0.0f };   // upper arm + forearm segment lengths (uu), measured once per body
static int      g_cbBoneIdx[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };   // hand_L, hand_R, shoulder_L, shoulder_R, upper_arm_L, upper_arm_R, lower_arm_L, lower_arm_R (MatchRefBone)
static uint32_t g_cbNameIdx[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };          // the same eight, as GNames indices (for the render lane's RefSkeleton read)
// The body's arms cut at draw time (hands/body_cut.cpp, render lane)
static bool     g_bcOn = true;           // [CorvoBody] ArmCut
static float    g_bcRadius = 12.0f;      // [CorvoBody] ArmCutRadiusUu: a vertex this close to the arm chain is arm geometry (PSK census: all within 11.2)
static float    g_bcStartUu = 11.0f;     // [CorvoBody] ArmCutStartUu: the first part of the upper arm stays (the deltoid; torso vertices sit there too)
static int      g_bcMinArm = 2;          // [CorvoBody] ArmCutMinVerts: arm vertices a triangle needs to be dropped (1..3)
static int      g_bcN = 0;               // contracts built for the current body
static bool     g_bcRefused = false;
static bool     g_bcChainOk = false;     // the reference-pose chains were composed and agreed with the vertex buffer
static int      g_bcChainVariant = -1;   // which quaternion convention won
static float    g_bcChain[2][4][3] = {};
static uint8_t* g_bcBodyFor = NULL;      // the body the contracts belong to
static long     g_bcDraws = 0, g_bcMisses = 0, g_bcIdentTries = 0;
static int      g_bcDropped = 0, g_bcKept = 0, g_bcArmVerts = 0;
static char     g_bcWhy[96] = "waiting for the body";
static int      g_cbAppliedMode = 0;     // 1 vr (shoulders hidden), 2 body (hands hidden or not), 0 nothing applied yet
static long     g_cbRehides   = 0;       // times a hide had to be re-issued (their UnHide on a weapon change)
static int      g_cbShadowSeen = 0;      // HiddenGame copies that matched the mesh name (their shadow body)
static bool     g_cbWidthDone = false;   // the IK width was matched for this body
// The hand point the SkelControl drive just handed the first-person hand bone, per hand
// (0 = left, 1 = right), as a WORLD point whatever space the drive itself wrote in.
static float    g_cbHandTgt[2][3] = { { 0, 0, 0 }, { 0, 0, 0 } };
static uint8_t  g_cbHandSpace[2]  = { 0xff, 0xff };
static double   g_cbHandMs[2]     = { 0.0, 0.0 };
// The arm IK's body frame, published from the render lane each IK draw: body yaw minus head
// yaw in the tracking frame (radians, clockwise positive like UE yaw) and when.
static std::atomic<float>              g_cbIkYawDelta{0.0f};
static std::atomic<unsigned long long> g_cbIkYawMs{0};
// Resolved offsets (names first, SDK numbers as the cross-check; 0 = unresolved)
static uint32_t g_cbOffEff = 0, g_cbOffEffSpace = 0, g_cbOffStrT = 0, g_cbOffSkelMesh = 0;
static uint32_t g_cbOffHidden = 0, g_cbMaskHidden = 0, g_cbOffBoneVis = 0, g_cbOffLoc = 0, g_cbOffRot = 0;
static uint8_t* g_cbFnFind = NULL, *g_cbFnHide = NULL, *g_cbFnUnhide = NULL, *g_cbFnBoneLoc = NULL;
static uint8_t* g_cbFnSetTrans = NULL, *g_cbFnSetRot = NULL, *g_cbFnMatchBone = NULL;
static bool     g_cbRefused   = false;   // an offset or function did not resolve as the SDK says: the module refuses for the session
// The post-Tick chain: our 5-byte jump over CorvoBody's own MinHook jump at UGameEngine::Tick, so
// our placement runs AFTER their per-tick body update and before the draw.
static uint8_t* g_cbTickStub  = NULL;
static uintptr_t g_cbTickTheirs = 0;     // where their jump went (their detour)
static int      g_cbTickState = 0;       // 0 not tried, 1 waiting for their hook, 2 installed, 3 refused
static double   g_cbTickRetryMs = 0.0;
static uint8_t  g_cbTickSaved[5] = { 0, 0, 0, 0, 0 };
static bool     g_cbChain     = false;   // [CorvoBody] PostTickChain: the retired route (a second jump after theirs; it double-wrote the transform each tick and the eyes disagreed)
static float    g_cbShoulderOff[3] = { 0, 0, 0 };   // the upper-arm midpoint in the body's own yaw frame, refreshed on the script lane
static double   g_cbShoulderOffMs = 0.0;
static bool     g_cbShoulderOffOk = false;
static long     g_cbRewrites  = 0;       // their SetTranslation/SetRotation calls that carried our placement
static double   g_cbRewriteLogMs = 0.0;
static long     g_cbPlacements = 0;      // post-Tick placements that landed
static long     g_cbWrites    = 0;       // body-mode effector writes that landed
static long     g_cbClamped   = 0;       // writes shortened by the reach clamp
static double   g_cbLogMs     = 0.0, g_cbPlaceLogMs = 0.0;
static float    g_cbLastC[3]  = { 0, 0, 0 };   // the last shoulder-centre target (world) and the measured mid, for status
static float    g_cbLastS[3]  = { 0, 0, 0 };
static float    g_cbLastYawDeg = 0.0f, g_cbLastDeltaDeg = 0.0f, g_cbShoulderCm = 0.0f;
static char     g_cbWhy[96]   = "not started";
