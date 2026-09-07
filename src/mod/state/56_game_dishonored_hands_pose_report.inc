// State chunk 56 for game/dishonored/hands/pose_report - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-33 step 1: the READ-ONLY pose, socket and item correspondence report.
//
// WHY THIS EXISTS AND WHY IT WRITES NOTHING. Four attempts at placing the
// hands failed, and the review of them found that every one had been built on
// a quantity nobody had checked. Three examples, all of them mine:
//
//   * "the pivot is at the shoulder" came from distances between skinning
//     matrix TRANSLATIONS. A skinning matrix commonly folds in the inverse
//     bind transform, so its translation places the bind mesh origin and not
//     the joint. The number was real and the reading of it was invented.
//   * g_msHandBone and g_msBoneHand were treated as one idea. They are two
//     unrelated heuristics - maximum degree in a co-influence graph, and a
//     sphere around a vertex centroid - and neither is anatomy.
//   * 210 units a metre was derived from where a resting hand OUGHT to be,
//     which assumes the answer.
//
// So before anything is placed anywhere, resolve what the engine itself says:
// the live skeletal component, its sockets, each socket's parent BONE and
// local transform, the equipped item and the socket it actually uses. Those
// are semantic facts the asset states, rather than shapes guessed from a
// vertex cloud.
//
// Every name below is taken from the decompiled scripts under
// tools/uscript/dishonored, quoted exactly:
//
//   DishonoredInventory.m_Slots, .m_pOwner
//   DishonoredInventoryItem.m_pOwningInventory, .m_CurSocket, .m_pMesh,
//                           .m_pPlayerMesh, .m_pPostUpdateTickComponent
//   SkeletalMeshSocket.SocketName, .BoneName, .RelativeLocation,
//                      .RelativeRotation, .RelativeScale
//
// A name that does not resolve is REPORTED AS UNKNOWN and nothing is inferred
// from its absence. That is the whole discipline this module exists to
// restore: the plan's gate is "if a relationship is unknown, print it as
// unknown", and a report that quietly substitutes a guess for a missing field
// is worse than no report.
//
// LANE: the script lane, from the same place arm_follow's probe runs, where
// the pawn is already proven live. It reads engine memory through the same
// RangeReadable guards and never writes.
// ===========================================================================

#define PR_MAX_FIELDS 24

struct PrField {
    const char* cls;
    const char* prop;
    uint32_t    off;      // 0 = did not resolve
};

// The fields this report needs, by class and name. Kept as a table so the log
// can say exactly which ones the running build could and could not find,
// rather than failing at the first miss.
static PrField g_prFields[PR_MAX_FIELDS] = {
    { "DishonoredInventory",     "m_Slots",                   0 },
    { "DishonoredInventory",     "m_pOwner",                  0 },
    { "DishonoredInventoryItem", "m_pOwningInventory",        0 },
    { "DishonoredInventoryItem", "m_CurSocket",               0 },
    { "DishonoredInventoryItem", "m_pMesh",                   0 },
    { "DishonoredInventoryItem", "m_pPlayerMesh",             0 },
    { "DishonoredInventoryItem", "m_pPostUpdateTickComponent",0 },
    { "SkeletalMeshSocket",      "SocketName",                0 },
    { "SkeletalMeshSocket",      "BoneName",                  0 },
    { "SkeletalMeshSocket",      "RelativeLocation",          0 },
    { "SkeletalMeshSocket",      "RelativeRotation",          0 },
    { "SkeletalMeshSocket",      "RelativeScale",             0 },
    { "SkeletalMeshComponent",   "Sockets",                   0 },
    { "SkeletalMeshComponent",   "SkeletalMesh",              0 },
    { "SkeletalMesh",            "Sockets",                   0 },
    { "SkeletalMesh",            "RefSkeleton",               0 },
    { "DishonoredPlayerPawn",    "m_pLookAtControl_LeftHand", 0 },
    { "DishonoredPlayerPawn",    "m_pLookAtControl_RightHand",0 },
    { "DishonoredPlayerPawn",    "m_pLookAtControl_Camera",   0 },
    { "DishonoredPawn",          "Mesh",                      0 },
    { NULL, NULL, 0 },
};

static bool     g_prOn = true;          // [Hands] PoseReport - read-only
static bool     g_prTried = false;
static uint32_t g_prNextTryMs = 0;
static int      g_prFound = 0, g_prMissing = 0;
static volatile int g_prDumpReq = 0;    // one full dump on request
static double   g_prNextReport = 0.0;
