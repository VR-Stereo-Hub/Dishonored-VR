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
// build 30.20: supersampling. The game renders at its WINDOW size (~1003
// lines) and the headset blows that up to 2240 lines per eye - the fuzz.
// The game ignores oversize requests in its own ini, but WE hand out the
// device: override the backbuffer, let D3D scale the window blit, and the
// capture picks up the full-size image automatically. Windowed only.
// 30.21: default OFF. Forcing the backbuffer alone makes the game render
// into the top-left corner of it (the engine keeps its own idea of the
// resolution) - off-centre view, broken menus. The honest route to
// sharpness is driver-side DSR/VSR: fake high-res desktop, game and cursor
// and window all agree, and this override stays retired.
static UINT   g_forceResW = 0;           // [Screen] RenderWidth  (0 = off)
static UINT   g_forceResH = 0;           // [Screen] RenderHeight
static UINT   g_wantClientW = 0, g_wantClientH = 0;   // the client size we drive
static bool   g_fsRescue = false;  // game asked fullscreen; window needs rescuing
// 32.79: the hold must not start at load time. The game's window is born at
// 160x120 and grows; 32.78 blocked that growth and the window stayed 154x91 -
// unreachable overlay, and a 154x91 render. The hold is only about stopping
// the game from UNDOING our size, so it arms after our size has been applied.
static bool   g_holdWindow = false;
static void   ApplyRenderWindowSize(const char* where);
// 30.22: the CONSISTENT version of supersampling. 30.20 grew the backbuffer
// while the game still believed 1783x1003 - corner render, broken menus.
// Instead, spoof the desktop size the game sees (GetSystemMetrics /
// GetMonitorInfo via its own import table) so its windowed-resolution clamp
// lets the user's DishonoredEngine.ini ResX/ResY through. Window, viewport,
// UI and cursor space all agree at the big size; the capture just follows.
// 38.91: the live backbuffer size, so the setres console script can tell
// whether the game is ALREADY where we want it and skip itself entirely.
// The script exists as a safety net for installs that came up at the wrong
// resolution; on every machine that ran setup_resolution.bat the engine ini
// already puts the game at the target, and running it anyway fires a burst
// of device Resets a few seconds into play - measured on a user's rig going
// 4032x1431 (aspect 2.818!) and 1600x900 before settling. A wrong aspect is
// a wrong vertical FOV, which is exactly the "loads in looking right, then
// switches to zoomed a couple of seconds later" report, and it happens on
// every backend because this path is shared.
static UINT   g_liveBbW = 0, g_liveBbH = 0;
static UINT   g_spoofW = 0;              // [Screen] SpoofDesktopW (0 = off)
static UINT   g_spoofH = 0;              // [Screen] SpoofDesktopH
// 36.1: the REAL desktop window. The render never depended on it (the game
// reads its size through hkGetClientRect, which lies), but on the user's
// machine the game began creating its real window at desktop-spanning size -
// with NO binary change involved (A/B against STABLE_35.1 showed the same),
// so the cause is environmental and the cure is to stop caring: shrink the
// real window ourselves once the hold is armed, and rewrite WM_SIZE in the
// overlay subclass so the game never learns. Cosmetic only. 0 = leave alone.
static UINT   g_deskWinW = 1600;         // [Screen] DesktopWindowW
static UINT   g_deskWinH = 900;          // [Screen] DesktopWindowH
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

// DELIBERATELY still 9 for 30.70. Bumping this makes LoadConfig rewrite the
// whole ini from WriteDefaultIni, which would have silently thrown away the
// user's tuned FovLever=130, the 2560x1440 desktop spoof, convergence, fill -
// everything calibrated over the last few builds - to ship a documentation
// comment. The [HandRender] section is appended to the live ini instead, and
// every key falls back to the same defaults through IniFloat if it is absent.
static const char* kBuildTag = "38.92";  // ALWAYS bump with the deploy
static const int kConfigVersion = 9; // bump to refresh users' ini with new defaults

// Load config exactly once, lazily, from OUTSIDE the loader lock (first export
// call). Safe to call repeatedly; only the first does work.
static bool g_configLoaded = false;
