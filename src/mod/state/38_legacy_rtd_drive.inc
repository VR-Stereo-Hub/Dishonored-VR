// State chunk 38 for legacy/rtd_drive - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static int    g_gtPhase  = 0;
static int    g_gtFrame  = 0;
static float  g_gtB0[24][9];             // per-candidate world basis at zero
static bool   g_gtB0Ok[24];
static float  g_gtRefB0[9];              // pMesh basis at zero (noise check)
static bool   g_gtRefOk = false;
static float  g_gtC0[2][9];              // controller bases at head-test start
static bool   g_gtC0Ok[2] = { false, false };
static float  g_gtQMax[2] = { 0.0f, 0.0f };
static float  g_gtT0[24][3];             // world positions at zero (trans ref)
static bool   g_gtT0Ok[24];
static float  g_gtHR0[2][3];             // hand ROOM pos at head-test start
static bool   g_gtHR0Ok[2] = { false, false };
static float  g_gtRel0[2][3];            // hand HEAD-REL pos at head-test start
static bool   g_gtRel0Ok[2] = { false, false };
static float  g_gtHmd0[3] = { 0, 0, 0 };
static bool   g_gtHmd0Ok = false;
static float  g_gtPawn0 = 0.0f;          // pawn yaw at head-test start
static bool   g_gtPawn0Ok = false;

static const char* kGtName[11] =
    { "zero", "yaw+30", "zero", "pitch+30", "zero", "roll+30",
      "zero", "tx+30uu", "ty+30uu", "tz+30uu", "yaw30+tx30" };

// The measured global change of basis between the rotator's frame and the
// world-side delta (ground truth run 1): M_engine = T * M_ue3 * T^T.
static const float kGtT[9] = { 0,-1, 0,   0, 0,-1,   1, 0, 0 };
