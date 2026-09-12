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
