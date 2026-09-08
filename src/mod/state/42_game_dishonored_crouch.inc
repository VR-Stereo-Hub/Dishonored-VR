// State chunk 42 for game/dishonored/crouch - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// VR-31: `bone` is the ref-skeleton INDEX, which is the index space
// BoneVisibilityStates uses. 30.13 only ever collected the FName, which is why
// the array write of the time had to be aimed by name and could not be checked
// against the skeleton's own bone count.
struct ArmBone { uint32_t idx, num; int bone; char nm[40]; };
static ArmBone  g_armBones[40];
static int      g_armBoneCnt = 0;
static uint8_t* g_armComp = NULL;
static uint8_t* g_fnHide = NULL;
static uint8_t* g_fnUnhide = NULL;
static bool     g_armHidden = false;

// VR-31 route (a): the per-bone visibility lever and its write-survival census.
static bool     g_bvResolved  = false;   // reflection asked once
static uint32_t g_bvOffVis    = 0;       // SkeletalMeshComponent::BoneVisibilityStates
static uint32_t g_bvOffCtl    = 0;       // ::SkelControlIndex
static uint32_t g_bvOffReq    = 0;       // ::RequiredBones
static int      g_bvBoneTotal = 0;       // ref-skeleton bone count of the rig we prepped
static bool     g_bvOn        = false;   // the write is currently applied
static uint8_t  g_bvSave[40];            // the exact bytes we overwrote
static uint8_t* g_bvSaveComp  = NULL;    // the component those bytes came from
static int      g_bvHeld = 0, g_bvReverted = 0, g_bvOther = 0, g_bvWrites = 0;
static double   g_bvNextLogMs = 0.0;
static int      g_bvArmTries  = 0;       // ini-armed attempts, capped
static double   g_bvNextTryMs = 0.0;
static bool     g_bvClosedOut = false;   // the name-only search ran once
static bool     g_bvScanned   = false;   // the rig scan ran once
static int      g_bvCollects  = 0;       // self-served FpCollect attempts

// VR-31 route (d): material-section hiding (hands/mat_hide.cpp)
static bool     g_matResolved = false;
static uint32_t g_matOffLodInfo = 0, g_matOffMaterials = 0;
static uint32_t g_matOffUsed = 0, g_matOffToBody = 0, g_matOffAssetMats = 0;
static uint8_t* g_fnShowMatSection = NULL;
static bool     g_matCensusDone = false;
static int      g_matTries = 0;
static double   g_matNextTryMs = 0.0;
// wasHidden is the flag the GAME had before we touched it: 1 = it was already
// hidden and showing it would be a change, not a restore. -1 = unreadable.
struct MatHid { uint8_t* comp; int id; int lod; int wasHidden; };
static MatHid   g_matHid[64];
static int      g_matHidN = 0;
static uint8_t* g_fnGetNumElements = NULL;
static uint8_t* g_fnGetMaterial = NULL;
// the automatic sweep: phase -1 = finished, 0 = build the plan, 1 = stepping
static int      g_matAutoPhase = 0;
static double   g_matAutoUntil = 0.0;
static double   g_matStepMs = 5000.0;    // [Hands] MatStepMs
struct MatPlan { uint8_t* comp; int id; int lod; int cand; };
static MatPlan  g_matPlan[96];
static int      g_matPlanN = 0, g_matPlanAt = 0;
// the cycler: the hotkey (present thread) posts, the script lane consumes
static volatile int g_matCycleReq = 0;
static int      g_matCycleAt = -1;
static double   g_bvNextCollectMs = 0.0;
