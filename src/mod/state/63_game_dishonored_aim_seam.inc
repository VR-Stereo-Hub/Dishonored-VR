// state/63_game_dishonored_aim_seam.inc - VR-57: the fire-seam probe's state.
// Read-only: nothing here is written into the game.
static const int kAsMax = 8;
static bool     g_asOn = false;       // [Aim] SeamProbe
static bool     g_asVerbose = false;  // [Aim] SeamVerbose: log the quiet instances too
static double   g_asLastMs = 0.0, g_asScanMs = 0.0;
static int      g_asN = 0;
static uint8_t* g_asObj[kAsMax] = {};
static int32_t  g_asTag[kAsMax] = {};
static int32_t  g_asPawnTag = 0x7fffffff;
static bool     g_asLiveSeen = false;  // a context whose tick tag advanced with a real direction

// VR-57 step 3: DRIVING the cache from the controller ray ([Aim] DriveFromHand).
static bool     g_asDrive = false;      // the lever; ships 0
static float    g_asDriveDistUU = 800.0f;   // how far along the ray the aim point sits
static uint8_t* g_asCtx = NULL;         // the equipped weapon's projectile context
static uint32_t g_asCtxOff = 0;         // its cache offset
static double   g_asCtxMs = 0.0;        // when it was last revalidated
static long     g_asWrites = 0, g_asWriteRefused = 0;
static double   g_asWriteLogMs = 0.0;
static const char* g_asWriteWhy = "not attempted";
static float    g_asLastDir[3] = {};
static long     g_asOverwritten = 0;
static const int kAsWhyMax = 8;
static const char* g_asWhyStr[kAsWhyMax] = {};
static long     g_asWhyCnt[kAsWhyMax] = {};
static int      g_asWhyN = 0;
static float    g_asProjMine[2] = {}, g_asProjGame[2] = {};
static float    g_asProjErr = -1.0f;
static long     g_asProjSamples = 0;
static double   g_asPoseLogMs = 0.0;
