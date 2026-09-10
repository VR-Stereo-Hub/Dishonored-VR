// mod/state/54_game_dishonored_arm_follow.inc - VR-30, the arm-follow probe.
//
// The arms and weapon follow the view because Arkane draws the first-person
// view model in camera space. Six recorded attempts (ENGINE_NOTES, "Head
// coupling of the arms") all fought the RESULT of that - bone banks, the c6
// palette, SkelControl strength, a donor graft. The decompiled scripts say
// the game has its own controls UPSTREAM of all of it:
//
//   DishonoredPlayerCamera.m_DishonoredVTSettings  (a DishonoredVTSettings,
//   declared in DishonoredCameraInfluenceGroup) ships every weight at 1.0 -
//   arm follow fully ON - with m_ArmFollowWeight_Rot_Primary/_Secondary the
//   rotational follow weights this ticket is about.
//
//   Four DishonoredCameraInfluence objects exist purely to turn it off:
//   m_pDisableArmFollow_Primary/_Secondary_Influence and the ArmOffset pair,
//   each carrying m_Weight / m_TargetWeight / m_bActive / m_TransitionSpeed.
//
// NOTHING here writes. This is the read-only step: resolve the fields by
// name once, then log what they actually do while the player equips, aims,
// sheathes and swaps. The question it answers is whether these are DRIVEN at
// runtime or left at their defaults, and it must be able to print either
// answer - "resolved, never moved" is the finding that kills the lane.

// ---- resolution (one-shot; FindPropOffset is two GNames walks plus a full
// GObjects sweep per call, so it is latched and never re-run on a cadence) --
static bool     g_afTried      = false;   // resolution attempted
static bool     g_afReady      = false;   // ... and it produced usable offsets
static uint32_t g_afVtOff      = 0;       // DishonoredPlayerCamera.m_DishonoredVTSettings
static uint32_t g_afNextTryMs  = 0;       // retry gate: GNames is empty early

// Members WITHIN DishonoredVTSettings (add to g_afVtOff for the address).
// 0 is FindPropOffset's failure sentinel, but it is also a legitimate offset
// for the first member of a struct, so each has its own found flag.
enum { kAfWeight = 0, kAfRotPrim, kAfRotSec, kAfOffWPrim, kAfOffWSec, kAfMeshFov, kAfFieldCount };
static const char* const kAfFieldNames[kAfFieldCount] = {
    "m_ArmFollowWeight",
    "m_ArmFollowWeight_Rot_Primary",
    "m_ArmFollowWeight_Rot_Secondary",
    "m_ArmFollowOffset_Weight_Primary",
    "m_ArmFollowOffset_Weight_Secondary",
    "m_MeshSpecificFOVWeight",
};
static uint32_t g_afFieldOff[kAfFieldCount]   = {0};
static bool     g_afFieldFound[kAfFieldCount] = {false};
static float    g_afFieldLast[kAfFieldCount]  = {0};
static bool     g_afFieldSeen[kAfFieldCount]  = {false};
static uint32_t g_afFieldMoves[kAfFieldCount] = {0};   // times the value CHANGED

// ---- the four influences --------------------------------------------------
// Named pointers on the camera. The offsets come from the camera class, the
// member offsets from DishonoredCameraInfluence.
enum { kAfInfFollowPrim = 0, kAfInfFollowSec, kAfInfOffsetPrim, kAfInfOffsetSec, kAfInfCount };
static const char* const kAfInfNames[kAfInfCount] = {
    "m_pDisableArmFollow_Primary_Influence",
    "m_pDisableArmFollow_Secondary_Influence",
    "m_pDisableArmOffset_Primary_Influence",
    "m_pDisableArmOffset_Secondary_Influence",
};
static uint32_t g_afInfPtrOff[kAfInfCount] = {0};
static bool     g_afInfFound[kAfInfCount]  = {false};
static float    g_afInfWLast[kAfInfCount]  = {0};   // m_Weight
static float    g_afInfTLast[kAfInfCount]  = {0};   // m_TargetWeight
static bool     g_afInfSeen[kAfInfCount]   = {false};
static uint32_t g_afInfMoves[kAfInfCount]  = {0};

// Members of DishonoredCameraInfluence itself.
static uint32_t g_afInfWeightOff = 0, g_afInfTargetOff = 0, g_afInfSpeedOff = 0;
static uint32_t g_afInfActiveOff = 0, g_afInfActiveMask = 0;
static bool     g_afInfMembersOk = false;

// ---- the write lever (VR-30, measured 2026-09-05) -------------------------
// The probe's first run settled how this has to work. m_ArmFollowWeight and
// both _Rot_ weights are RECOMPUTED every frame - they oscillate 0.982..1.000
// and never leave that band - so a single write is overwritten immediately.
// The influence route is worse: the game does drive DisableArmFollow itself,
// but it yanks the target back to 0 about 60 ms after raising it, and the
// blend only ever reached weight 0.117. Setting m_fDefaultWeight=1.0 in the
// game's own DishonoredCamera.ini changed nothing at all.
//
// So the write goes where the FOV lever's does: straight into the settings
// struct, on EVERY ProcessEvent dispatch, which is the cadence that outruns
// the engine's per-tick recompute (fov_lever.cpp's 30.50 note).
//
// -1 = off, do not write (ships). 0..1 = force the rotational follow weights
// to this value. 0 is full decoupling.
static float    g_afForceWeight = -1.0f;
static uint32_t g_afWrites      = 0;
static bool     g_afSaidWriting = false;

// ---- the influence lever (VR-30, measured 2026-09-05, second run) ---------
// Forcing the settings struct WORKS but only half the time: the log shows the
// weights oscillating 1.000 <-> 0.000 between dispatches - we write 0, the
// engine recomputes 1 straight back, and the arms end up with roughly half
// authority, which is seen as the arms TRAILING the view instead of tracking
// it. Winning that race outright is not possible from this lane.
//
// So stop fighting the recompute and feed it. The recompute derives the arm
// follow weight from the DisableArmFollow influences; if THOSE read 1, it
// computes an arm follow of 0 by itself and holds it, because that is the
// engine's own arithmetic rather than a value we are pasting over the top.
// The earlier finding stands - the game pulls m_TargetWeight back to 0 about
// 60 ms after raising it - so m_Weight, m_TargetWeight and m_bActive are all
// written every dispatch, which leaves the cancel nothing to act on.
//
// -1 = off (ships). 0..1 = force the two DisableArmFollow influences.
static float    g_afForceInf    = -1.0f;
static uint32_t g_afInfWrites   = 0;
static bool     g_afSaidInfWrite = false;
// The influence pointers, fetched and range-checked on the slow tick so the
// per-dispatch write is a store through a known-good pointer rather than two
// dereferences and two RangeReadable calls. Cleared the moment the camera
// stops being live, which is the only way these can go stale.
static uint8_t* g_afInfCache[kAfInfCount] = {0};

// ---- the camera look-at lever (VR-30, the YAW candidate) ------------------
// The influence route took PITCH and left YAW untouched, and forcing the
// settings weights on top of it added nothing - both act on the same channel.
// The remaining suspect is LookAtControl_Camera: one of three Arkane look-at
// controls on the player rig (with _LeftHand and _RightHand), and the one
// attempt 5 described as aiming the arms at the view. Its ControlStrength is
// already written every dispatch by SkcRotApply from [Hands]
// CameraLookAtStrength - the right cadence - but that path is dead on a
// shipped build: SkcRotApply returns on !g_skcDrive, and g_skcPlayer[] is
// only filled by SkelControlProbe, which sits below ApplyHandToMesh's
// `if (!g_handMesh) return`.
//
// So this finds the control itself and writes it itself, independent of the
// hands subsystem, and can therefore be judged on a stock install.
//
// THE HAZARD, and it is why this is guarded the way it is: 32.6 records this
// class of store as "the single most dangerous in the DLL" - writes into a
// freed AnimTree after CollectGarbage on a save load corrupted the heap. So
// the object's GObjects slot AND its class pointer are both re-checked before
// every write, exactly as SkcAlive does, and anything unexpected drops the
// pointer rather than writing through it.
static float    g_afLookStrength = -1.0f;   // -1 = off (ships), 0..1 forces it
static uint8_t* g_afLookObj      = NULL;    // the LookAtControl_Camera node
static uint32_t g_afLookIdx      = 0;       // its GObjects slot, for liveness
static void*    g_afLookCls      = NULL;    // and its class pointer
static float    g_afLookWasStr   = -1.0f;   // what it read before we touched it
static uint32_t g_afLookWrites   = 0;
static uint32_t g_afLookNextFindMs = 0;
static bool     g_afSaidLook     = false;

// ---- the counter-yaw lever (VR-30, the YAW answer) ------------------------
// Reading the scripts end to end says there is NO yaw driver for the arms
// anywhere: every arm/aim/look mechanism the game exposes is pitch-only or
// NPC-only. The arms yaw because the PAWN ACTOR yaws - the arms are bones of
// the pawn's mesh, the mesh rides the actor, and DishonoredPlayerPawn's
// native FaceRotation override sets the actor's yaw from the controller
// every frame. There is no weight to switch off, because the camera never
// needed to ADD yaw in the first place.
//
// But DishonoredVTSettings carries something the pitch hunt walked straight
// past: m_ArmFollowOffset_Rot_Primary and _Secondary are ROTATORS, not
// floats, with a live Yaw field, scaled by m_ArmFollowOffset_Weight_* (both
// shipped 1.0) and gated by a SECOND influence pair - DishonoredCamera_
// DisableArmOffset - which this session left at 0 the whole time while
// testing its DisableArmFollow sibling. So the offset path has been alive
// and unused throughout.
//
// That makes the fix a SUBTRACTION rather than a switch: feed the offset a
// yaw equal and opposite to how far the head has turned since the reference,
// and the arms hold still in the world while the view turns. Stick turning
// moves g_bodyYawU, not g_hmdYaw, so it is untouched and composes normally -
// the body comes round and the arms come with it.
//
// -1 = off, 1 = full counter-rotation, values between scale it (0.5 = the
// arms follow at half rate, which is the useful thing to tune by feel).
static float    g_afCounterYaw   = -1.0f;
static uint32_t g_afCntWrites    = 0;
static bool     g_afSaidCounter  = false;
static float    g_afCntRefYaw    = 0.0f;    // head yaw when the lever armed
static bool     g_afCntHaveRef   = false;
static float    g_afCntLastDeg   = 0.0f;    // what we last wrote, for the report
static float    g_afCntMaxDeg    = 0.0f;    // the largest counter we ever wrote (the ceiling hunt)
static float    g_afCntHeadDeg   = 0.0f;    // the raw head delta that produced it
static float    g_afCntHeadMax   = 0.0f;    // and its extreme
// The viewmodel's own lens. m_bUseFOV / m_FOV live on
// DishonoredPlayerSkeletalComponent (the pMesh that carries arms AND body), and
// the camera blends them by m_MeshSpecificFOVWeight. A narrower arms lens means
// more pixels per degree there than in the world, so an angular counter-rotation
// has to be scaled by armsFOV/worldFOV or it over-corrects. 1.0 until resolved,
// which is the behaviour that shipped.
static float    g_afFovRatio     = 1.0f;
static float    g_afArmsFov      = 0.0f;    // as read from the component
static uint32_t g_afFovOff = 0, g_afUseFovOff = 0, g_afUseFovMask = 0;
static uint8_t* g_afVmComp       = NULL;    // a live DishonoredPlayerSkeletalComponent
static uint32_t g_afVmIdx        = 0;
static void*    g_afVmCls        = NULL;
static uint32_t g_afVmNextFindMs = 0;
static uint32_t g_afVmScan       = 1;    // incremental search cursor - a full sweep per try was the lag spike
static bool     g_afSaidFov      = false;

// The body-yaw hold is GONE (VR-30, 2026-09-05). It wrote the pawn's own
// Rotation.Yaw every dispatch and was measured futile: 4 of 186 writes
// survived to the next dispatch, because the engine re-derives the pawn's
// heading from the controller every tick. Its replacement does not write the
// field at all - it intercepts FaceRotation and changes the heading the engine
// is ASKED for. See the FaceRotation intercept below.

// Actor.Rotation, by reflection - the census reads it on both the controller
// and the pawn, and both are Actors so one lookup serves both.
static uint32_t g_afActorRotOff  = 0;

// The two Rotator offsets, resolved alongside the float weights.
static uint32_t g_afRotOffPrim = 0, g_afRotOffSec = 0;

// ---- the yaw census (VR-30): four quantities, one coherent sample ---------
// The body-lock A/B measured that ONE writer suppresses both the wanted stick
// turn and the unwanted arm follow. Telling those apart needs the yaw chain
// sampled together on one line, under head-only and stick-only input, with the
// writer on and off. It runs whatever BodyYawLock is set to - a probe that only
// runs on the configuration under suspicion cannot produce the control cell.
//
// The delta is what carries the answer: a field that does not MOVE is the
// finding here, and a raw value hides that.
static double   g_afCenNextMs   = 0.0;   // next sample boundary
static double   g_afCenLastMs   = 0.0;   // when the previous sample was taken
static bool     g_afCenHave     = false;
static float    g_afCenHead     = 0.0f;  // g_hmdYaw, radians
static float    g_afCenView     = 0.0f;  // g_viewYawRad, radians
static int32_t  g_afCenCtrl     = 0;     // PlayerController Rotation.Yaw, UE units
static int32_t  g_afCenPawn     = 0;     // pawn Actor.Rotation.Yaw, UE units
static bool     g_afSaidCensus  = false;

// ---- strip the mesh rotation from the hand bones (VR-30, option A) --------
// MEASURED 2026-09-05: writing the pawn's yaw cannot win. Across 186 logged
// writes our value survived to the next dispatch 4 times (2.2%); 70% of the
// time the engine had already restamped the pawn back to the view yaw. The
// body is re-derived from the controller every tick and the controller's yaw
// is where the head injection has to go, so every lever that held the BODY was
// fighting a recompute it cannot outrun.
//
// So stop holding the body. The requirement was only ever about the arms.
// DishonoredPlayerPawn carries m_pLookAtControl_LeftHand/_RightHand
// (SkelControlSingleBone), and SkelControlBase declares bRemoveMeshRotation -
// "strip the component's own rotation from this bone". If the arms rotate
// because the component they hang off rotates, that bool removes the cause at
// the bone instead of chasing it at the body. Never tested: its only writer
// sits in SkcRotApply behind the hand-drive gate, which GamepadOnly=1 clears.
//
// This lever is independent of the whole hands subsystem: it finds the two
// controls itself, resolves the bit by REFLECTION (the 0x10 in skelcontrol.cpp
// is a guess and is not reused here), and forces it every dispatch because the
// engine restamps these nodes continuously. Ships OFF.
static uint8_t* g_asrCtl[2]   = { NULL, NULL };   // 0 = left, 1 = right
static uint32_t g_asrIdx[2]   = { 0, 0 };         // GObjects slot, for liveness
static void*    g_asrCls[2]   = { NULL, NULL };
static uint32_t g_asrOff      = 0;                // bRemoveMeshRotation
static uint32_t g_asrMask     = 0;
static bool     g_asrTriedOff = false;
static bool     g_asrWasSet[2]= { false, false }; // what it read before we touched it
static float    g_asrWant     = -1.0f;            // -1 off, 1 strip, 0 force-clear
static uint32_t g_asrWrites   = 0;
static uint32_t g_asrNextFindMs = 0;
static bool     g_asrSaid     = false;

// ---- the native-facing probe (VR-30) --------------------------------------
// The body target is CORRECT - it holds at -85.6 deg while the view swings 35 -
// and it does not survive to the next dispatch (4 of 186, 2.2%). The open
// question is WHO overwrites it and WHEN, and the answer is not derivable from
// the image: RTTI for the UE3 classes is stripped, and the native name strings
// sit in a blob loaded into GNames at runtime, so there is no name->function
// table and no vtable to find statically (checked 2026-09-05).
//
// It is also NOT established that our write is even late enough to matter. The
// ProcessEvent hook is a PRE-hook: PeHandler runs, then the original engine
// function proceeds. So writing "after ApplyHeadToViewRotation" is still before
// ProcessViewRotation's own body finishes, let alone the update that encloses
// it. That ordering is the thing to measure, not assume.
//
// This probe resolves the FaceRotation UFunction, reports every .text pointer
// in its tail (the exec thunk is among them) with RVAs, dumps the head of the
// pawn's vtable, and counts whether FaceRotation is EVER dispatched through
// ProcessEvent during gameplay. A count of zero is the expected answer and is
// the proof that gameplay reaches it native-to-native - which is exactly why
// hooking the script exec wrapper would catch nothing.
static bool     g_nfpDone     = false;
static uint32_t g_nfpFaceIdx  = 0xffffffffu;   // GNames index of "FaceRotation"
static uint32_t g_nfpFaceHits = 0;             // dispatches seen through ProcessEvent

// ---- the FaceRotation intercept (VR-30) -----------------------------------
// The body target is correct and does not survive a raw field assignment (4 of
// 186 writes). So stop assigning the field and change what the engine is ASKED
// to face instead: intercept FaceRotation, replace its Yaw argument with the
// separated body target, and let the engine's own function run normally so all
// its dependent bookkeeping happens as usual.
//
// Only for the validated possessed pawn, only with a fresh snapshot, and only
// the yaw - pitch, roll and delta time pass through untouched, as does every
// other actor. Ships OFF.
static float    g_frWant      = -1.0f;   // -1 off, 1 replace, 0..1 blends
static uint32_t g_frSeen      = 0;       // calls into FaceRotation at all
static uint32_t g_frOurs      = 0;       // ... on our validated pawn
static uint32_t g_frReplaced  = 0;       // ... where we replaced the yaw
static uint32_t g_frStale     = 0;       // ... declined, no fresh snapshot
static bool     g_frSaid      = false;
static dvr::hooks::Detour g_frDet;
static uint8_t* g_frStub      = NULL;

// ---- the report -----------------------------------------------------------
static uint32_t g_afTicks       = 0;   // script-lane passes that read a camera
static uint32_t g_afNoCam       = 0;   // ... and passes that had none
static uint32_t g_afNextLogMs   = 0;   // the periodic line
static bool     g_afSaidReady   = false;
