// State chunk 15 for core/config/config - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static float  g_autoHandDelay = 4.0f;    // [HandTracking] DelaySec
static bool   g_autoHandDone  = false;
static double g_autoHandArmMs = 0.0;
// build 30.8: hand-tracking feature toggles moved off the keyboard
static bool   g_fpPosOn  = true;         // [HandTracking] Depth
static bool   g_fpRollOn = false;        // [HandTracking] WristRoll
// build 30.8: diagnostics are scheduled from the ini, not keys.
// [Debug] Probe=bones|census|graph|ue3|view  runs once, ~2 s after weapon
// tracking comes up, then logs. Empty = nothing.
static char   g_dbgProbe[24] = "";
static uint8_t g_armVal = 2;             // [HandTracking] ArmHideValue
// build 30.19: motion melee - a real right-hand swing fires the sword attack.
// Gesture-triggered, the standard flat2VR melee: your swing times the game's
// attack animation. Physics-collision melee would need the engine's sealed
// hit detection; this is the honest version that actually ships.
static bool   g_meleeOn     = true;      // [Melee] Enabled
static float  g_meleeSpeed  = 1.8f;      // [Melee] SwingSpeed (m/s)
// 35.0: A SWING IS SUSTAINED, NOT A PEAK. The old detector fired the frame
// smoothed speed crossed the gate, so a wrist flick attacked. Now the speed
// must STAY above the gate for SwingMs while the hand travels SwingDistM -
// a flick has the peak but never the time or the distance. Both 0 = the old
// instant behavior.
static float  g_meleeSwingMs   = 120.0f; // [Melee] SwingMs (sustain time)
static float  g_meleeSwingDist = 0.25f;  // [Melee] SwingDistM (travel)
static float  g_meleeHoldMs = 220.0f;    // [Melee] HoldMs (attack press length)
static float  g_meleeCoolMs = 300.0f;    // [Melee] CooldownMs (combo pacing)
static double g_meleeUntil  = 0.0;
static double g_meleeNext   = 0.0;
static long   g_meleeCount  = 0;
static double g_meleeLastMs = 0.0;       // for spike correlation (30.24)
static bool   g_meleeHaptic = true;      // [Melee] Haptic
// 30.26: menus fire recognizable script events; any of them mutes swing
// injection for a few seconds. Auto-start runs at the main menu (its
// background scene has a pawn), so hand gestures there were injecting
// attack presses into the UI - the "main menu glitch".
static volatile double g_uiEventMs = 0.0;
// 30.30: real menu state from the game's own script events. Starts TRUE -
// the game boots into the main menu. Dis_OpenPauseMenu / MessageBox /
// save-load queries flip it on; OnMenuClosed / OnResumeGameClicked /
// LoadGameClicked flip it off. While a menu is open (or the powers wheel is
// held, or the DXVK fork reports a frame with no stereo splices - loading
// screens, videos), SBS presentation drops to mono so flat UI is readable.
static volatile bool g_menuOpen = true;
// 41.1: the title screen / main menu movie is up (DisGFxMoviePlayerMainMenu events,
// process_event.cpp); its own flag because the ghost-clear owns g_menuOpen.
static volatile bool g_mainMenu = false;
// 36.2: the panel places itself ONCE (FirstUseEver), and 36.1's shrink can
// fire after that - the panel's latched position then sits outside both the
// new logical space (invisible) and the mouse's reachable range (dead).
// Any real-window resize we cause must therefore re-center the panel.
static volatile LONG g_ovlRecenter = 0;
// 30.23: the hand-position neutral is relative to a SLOW-FOLLOWING body
// anchor (heavily low-passed HMD position), not a fixed room point. Turn
// around or walk and the anchor follows within seconds - no recalibrating
// toward your original facing. Head bob and lean are too fast for it to
// track, so the anti-head-leak property of room anchoring survives.
static float  g_bodyAnchor[3] = { 0, 0, 0 };
static bool   g_bodyAnchorOk = false;
static float  g_anchorTau = 2.5f;        // [HandTracking] AnchorTauSec

static void BlockCfgLoad();          // defined with the mesh-block machinery

// Bumping this makes LoadConfig rewrite the whole ini from WriteDefaultIni, so
// it costs the user every tuned value: bump it only when the keys themselves
// change. 10 = 41.0, the native-stereo foundation: the wide-window, side-by-
// side, OpenVR and [Stereo] shear keys are gone and the mono screen's
// DistanceMeters/WidthMeters defaults changed (docs/RELEASE_NOTES.md).
// 41.1 [Screen] RenderWidth/RenderHeight/RenderFullscreen/VirtualMode: the
// render-resolution picker's ask (0 = the game's own size). The mechanism and
// what the game will take: core/window/render_size.cpp.
static uint32_t g_resWantW = 0, g_resWantH = 0;
static bool     g_resWantFull = true;
static bool     g_resVirtual = false;
static uint32_t g_resModes[64][2];                  // the display modes the picker lists (render_size.cpp)
static int      g_resModeN = 0;
static char     g_resLastLine[200] = "no ask yet";  // the picker's last verdict, for F10
static const char* kBuildTag = "41.0";
static const int kConfigVersion = 10; // bump to refresh users' ini with new defaults

// Load config exactly once, lazily, from OUTSIDE the loader lock (first export
// call). Safe to call repeatedly; only the first does work.
static bool g_configLoaded = false;
