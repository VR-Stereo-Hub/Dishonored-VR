// ---- VR-62: WHAT SCREEN IS ACTUALLY UP, ASKED RATHER THAN INFERRED ----------
//
// Three attempts to shorten the mono window after a load were falsified in a
// headset, and ENGINE_NOTES records why all three failed the same way: each
// replaced a slow conservative test with a fast one, and the fast signals do not
// SEPARATE the states that matter. Dispatch flow cannot tell a main menu from
// gameplay (the menu's 3D background dispatches). The camera upload serial
// cannot tell a pause menu from a load (the world renders behind the menu). A
// live pawn cannot tell any of them apart (the main menu has one).
//
// Every one of those signals is a SIDE EFFECT of a screen being up. This module
// asks about the screen itself.
//
// The mod's whole menu vocabulary is event history: g_menuOpen, g_inMenu and
// g_mainMenu are set and cleared by named ProcessEvent dispatches. That is why a
// ghost exists at all - `Dis_OpenPauseMenu` fires during a load with no pause
// menu on screen, and `OnFocusLost` clears the main-menu flag without the main
// menu having closed. An event says something HAPPENED; it never says what is
// true now, and nothing in the mod can currently read what is true now.
//
// Scaleform screens in this game are GFxMoviePlayer objects, and a movie player
// carries its own open/closed state as a property. If that property exists and
// tracks, it is the direct discriminator all three attempts lacked: it separates
// a main menu from gameplay by IDENTITY (which movie), and an open menu from a
// load by STATE (open or not), without reference to any dispatch rate.
//
// THIS BUILD DOES NOT USE IT FOR ANYTHING. It is default-on observation:
// discover the vocabulary, watch it change, and print the verdict it WOULD have
// produced beside the one the mod actually used. Nothing here writes engine
// memory, clears a flag, gates a draw or triggers a collection.
//
// WHAT WOULD FALSIFY IT, stated before the run:
//   * If no movie-player class carries a readable open/closed bool, the premise
//     is dead and the log says UNKNOWN rather than inventing a "closed".
//   * If the bit is open at the main menu AND open in plain gameplay, it does not
//     separate them and it is worth nothing.
//   * If the bit only flips seconds after the screen visibly changes, it is no
//     earlier than what we already have.
// Any of those three ends this line of attack, and the run reports which.

#define UI_CLS_MAX   24    // distinct movie-player CLASSES
#define UI_PROP_MAX  96    // properties declared across those classes
#define UI_INST_MAX  48    // live movie-player INSTANCES tracked
#define UI_OPENSET   4     // classes named in a rollup line

// A movie-player class discovered in GObjects. `obj` is the UClass itself, which
// is what an instance's +kClassOff points at, so identity is a pointer compare
// and never a string compare on a hot path.
struct UiCls {
    uint8_t* obj;
    char     name[64];
    int      instances;    // counted during the scan that found it
};

// One property declared on one of those classes. Bools carry a bitmask; anything
// else is recorded for the log only, because a name we did not predict is the
// most useful thing a first run can hand back.
struct UiProp {
    uint8_t* owner;        // the declaring UClass
    char     name[48];
    char     kind[24];     // "BoolProperty", "IntProperty", ...
    uint32_t off;
    uint32_t mask;         // bools only
};

// A live instance we are watching. `bits` is the last sampled value of the open
// bit; -1 means never sampled.
struct UiInst {
    uint8_t* obj;
    uint8_t* cls;          // its UClass, cached so the poll never re-reads a name
    char     name[64];     // ObjectName at latch time
    int      open;         // -1 unknown, 0 closed, 1 open
    int      gen;          // the load generation it was first seen in
    double   seenMs;
};

static bool     g_uiOn        = true;    // [Menu] UiProbe - ships ON, see above
static bool     g_uiReady     = false;   // discovery has published; poll may read
static bool     g_uiRescan    = true;    // a scan is wanted (set again per load)
static int      g_uiScans     = 0;
static int      g_uiGen       = 0;       // bumped per load, stamped on instances

static UiCls    g_uiCls[UI_CLS_MAX];
static int      g_uiClsN      = 0;
static UiProp   g_uiProp[UI_PROP_MAX];
static int      g_uiPropN     = 0;
static UiInst   g_uiInst[UI_INST_MAX];
static LONG     g_uiInstN     = 0;       // published last, appended to only

// THE OPEN BIT. Chosen by discovery from the bools it actually found, never
// assumed. Zero mask means "no such property exists", and every consumer below
// has to treat that as UNKNOWN rather than as closed.
static uint32_t g_uiOpenOff   = 0;
static uint32_t g_uiOpenMask  = 0;
static char     g_uiOpenName[48]  = "";
static char     g_uiOpenOwner[64] = "";

// The rollup, recomputed on the present thread and logged only when it changes.
static int      g_uiOpenCount = 0;     // movies reading open right now
static bool     g_uiMainMenu  = false; // one of them is a main-menu movie
static char     g_uiOpenList[256] = "";
static bool     g_uiPropOld   = false; // the mod's own "a menu is up"
static bool     g_uiPropNew   = false; // what the movie state says instead
static double   g_uiPollMs    = 0.0;
static LONG     g_uiPolls     = 0;
static LONG     g_uiDropped   = 0;     // instances that stopped reading as objects
static LONG     g_uiReadyFlag = 0;     // the publish barrier for the tables above

// Published for the scoreboard, which scores the PROPOSED verdict beside the
// real one. Nothing gates on either of these.
static bool     g_uiVerdictNew    = false;
static bool     g_uiVerdictNoView = false;
