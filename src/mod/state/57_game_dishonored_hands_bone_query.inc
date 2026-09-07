// State chunk 57 for game/dishonored/hands/bone_query - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-33 step 1b: the SEMANTIC bone queries.
//
// The question this exists to answer, and it decides the architecture:
// IS handAttachment_L_jnt A DESCENDANT OF hand_L_jnt? If it is, moving the
// wrist in the CPU skeleton can carry the weapon through the engine's own
// attachment path. If they are siblings, it cannot, and the placement design
// changes. Nobody has asked the engine; the socket dump names the bone that
// OWNS each socket and says nothing about that bone's parent.
//
// TWO CORRECTIONS FROM REVIEW, both of which invalidated the first proposal:
//
// 1. THE MOD ALREADY CALLS OUT THROUGH ProcessEvent. The proposal claimed this
//    would be the first outbound engine call in the codebase and built a whole
//    contract around that premise. False: hands/mat_hide.cpp calls
//    GetNumElements and GetMaterial on live components through
//    PFN_ProcessEventCall, with a parameter frame and a NULL Result, and
//    console.cpp uses the same path. The grep that "proved" otherwise did not
//    match the cast-through-kProcessEvent form the code actually uses. So this
//    file HARDENS a proven path rather than inventing an ABI.
//
// 2. GetParentBone IS NAME-TO-NAME, NOT INDEX-TO-INDEX. The declaration is
//    `native final function name GetParentBone(name BoneName)`
//    (Engine/SkeletalMeshComponent.uc:302). The proposed first call passed two
//    integers and then checked `parent < index` as a plausibility test. Both
//    halves were wrong, and the check was meaningless anyway: a returned
//    FName's global name-table index is not a skeletal index.
//
// THE ORDER IS SMALLEST-RISK-FIRST, and every step gates the next:
//
//   GetNumElements()          no inputs, integer return, an existing tested
//                             caller. If this does not work nothing else will.
//   MatchRefBone(name) -> int the first call that passes a value in
//   GetBoneName(int) -> name  round-tripped against it; both FName words must
//                             match, not just the index
//   GetParentBone(name)->name the actual walk, by NAME, with a visited set
//   BoneIsChildOf(n,n)->bool  a semantic cross-check, last, because a boolean
//                             return needs its own encoding validated
//
// FNAMES CARRY TWO WORDS. An FName is { int32 Index; int32 Number; } and this
// codebase has only ever compared the index. A name is copied whole from the
// socket snapshot and passed whole; it is never rebuilt from printed text.
//
// LANE AND RE-ENTRY. Called only from PeHandler's script-lane pass. Our call
// re-enters ProcessEvent and therefore re-enters PeHandler, and the existing
// g_peReentry flag does NOT guard the whole hook - several mod actions run
// before any event-specific filtering. So this uses its own scoped depth
// counter, checked at the top of PeHandler, and mod-originated calls are
// excluded from any evidence about the game's natural update order.
//
// Default OFF, under its own key, independent of the read-only report - the
// socket dump keeps working if the calls are disabled.
// ===========================================================================

// An FName, both words. Never reduced to its index.
struct BqName { uint32_t idx; uint32_t num; };

#define BQ_MAX_CHAIN 64          // a parent walk longer than this is a cycle
#define BQ_MAX_TARGETS 8

static bool     g_bqOn = false;          // [Hands] BoneQuery - makes engine calls
static volatile int g_bqRunReq = 0;      // posted by the command, consumed on-lane
static bool     g_bqDone = false;
static int      g_bqDepth = 0;           // our own calls, in flight
static uint32_t g_bqThread = 0;          // the lane we were called on

// The UFunctions, resolved with an OWNER check - FindFunctionObj returns the
// first match of a name with no regard for its declaring class, so a
// same-named function on an unrelated class could be called on our receiver.
static uint8_t* g_bqFnNumElements = NULL;
static uint8_t* g_bqFnMatchRefBone = NULL;
static uint8_t* g_bqFnGetBoneName = NULL;
static uint8_t* g_bqFnGetParentBone = NULL;
static uint8_t* g_bqFnBoneIsChildOf = NULL;

// UStruct::SuperField, DERIVED rather than guessed: the offset at which
// DishonoredPlayerPawn's class chain reaches Pawn, then Actor, then Object.
// A three-link chain of correctly-named classes is evidence; a single
// plausible pointer is not.
static uint32_t g_bqSuperOff = 0;

// What the queries found. Names, not indices, because the engine answers in
// names and converting early is how the last four attempts went wrong.
static const char* const kBqTargets[BQ_MAX_TARGETS] = {
    "hand_L_jnt", "hand_R_jnt",
    "handAttachment_L_jnt", "handAttachment_R_jnt",
    "camera_jnt", NULL, NULL, NULL
};
static BqName   g_bqSocketBone[16];      // full FNames kept from the socket dump
static int      g_bqSocketBoneN = 0;
