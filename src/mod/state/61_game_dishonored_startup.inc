// ---- VR-62: SCORE THE STARTUP, PHASE BY PHASE -------------------------------
//
// The tester reports 2-15 s of mono after a load, with the hands already
// tracking and the weapons still in their default positions. Every attempt to
// explain it so far has been a wall-clock impression, and the one hard
// measurement that existed falsified the leading hypothesis: the gameplay
// verdict flips in the SAME millisecond as `[game] state: GAMEPLAY`, and the
// first stereo tag follows immediately, so nothing after the verdict is holding
// the picture.
//
// ```
// 21.88s  reentry: gates -> SINGLE draw (state not GAMEPLAY)
// 37.31s  [game] state: GAMEPLAY
// 37.31s  gameplay verdict: TRUE (all clear)
// 37.31s  reentry: gates -> DOUBLE draw after 1040 single tick(s)
// ```
//
// So the wait is upstream, in OUR state machine deciding the game is in
// gameplay. And there is evidence it decides late: the equipment reader pulled a
// REAL inventory off the player pawn (sword, crossbow, pistol - 6 of 9 slots) at
// 22.91 s, and that read calls FpPawn() and refuses without a live pawn, while
// `CylTruthLive()` stayed false until 34.00 s and a menu flag held on 3.3 s more.
//
// A NOTE ON WHAT COUNTS AS EVIDENCE, because the first draft of this got it
// wrong. The `pose: 28 of 28 name(s) resolved` line at 21.86 s looks like proof
// of a live pawn and is not: it resolves property OFFSETS out of GObjects by
// name, which works as soon as GNames is populated and needs no pawn at all.
// Only two signals here are genuine live-player evidence - a pawn pointer and an
// inventory that actually read - and the summary is built on those alone.
//
// That is roughly fifteen seconds between "we are reading this pawn's live
// inventory" and "we admit this is gameplay", which brackets the reported
// window. But WHICH term is late, and by how much, is exactly what nothing
// measures. VR-49 says so in its own words: without an instrument, "make
// startup instant" has no scoreboard.
//
// THIS IS THAT SCOREBOARD. It samples the terms of the state machine
// independently, records when each FIRST became true, and prints one line per
// load naming the term that cleared LAST - the one that owns the wait.
//
// IT MUST BE ABLE TO PRINT THE UNWELCOME ANSWER. If every signal clears
// together, the line says the wait is the game's own load and not ours. That is
// a real possible outcome and the instrument is worthless if it cannot report
// it.

// One milestone. `ms` is 0 until the signal is first seen true in this load.
struct SuPhase {
    const char* key;
    const char* what;    // what being true actually means, for the log
    double      ms;
};

// The order here is the order they are printed, not an expected order. A
// milestone arriving out of the expected order is itself a finding.
enum {
    SU_LOAD = 0,     // a load began (state left GAMEPLAY): the clock for this run
    SU_PAWN_PTR,     // a player pawn POINTER exists
    SU_NAMES,        // GNames populated. A floor, NOT pawn evidence.
    SU_INV_OK,       // its inventory reads - a real, equipped player
    SU_CYL,          // CylTruthLive(): the term that gates NO_PAWN
    SU_NOMENU,       // no menu flag set: the term that gates MENU
    SU_VIEW,         // DvrScriptViewLive(): the term that gates LOADING
    SU_NOCINE,       // no cinematic latch
    SU_VERDICT,      // DvrGameplayVerdict(): all five, together
    SU_UI_VERDICT,   // VR-62: the same verdict with the movie state substituted
    SU_UI_NOVIEW,    // ... and without the one-second view hold as well
    SU_STEREO,       // the first DOUBLE draw - the mono window ends HERE
    SU_COUNT
};

static SuPhase g_su[SU_COUNT] = {
    { "load",     "a load began (the clock starts here)",                 0.0 },
    { "pawn-ptr", "a player pawn pointer exists",                         0.0 },
    { "gnames",   "GNames populated (a floor on name lookups, NOT pawn evidence)", 0.0 },
    { "inventory","the pawn's inventory reads - it is a live equipped player", 0.0 },
    { "cyl",      "CylTruthLive - the term that gates NO_PAWN",           0.0 },
    { "no-menu",  "no menu flag set - the term that gates MENU",          0.0 },
    { "view",     "the view pipeline dispatches - the term that gates LOADING", 0.0 },
    { "no-cine",  "no cinematic latch",                                   0.0 },
    { "verdict",  "the gameplay verdict is TRUE - all five terms",        0.0 },
    { "ui-vrd",   "the PROPOSED verdict (movie state instead of the menu flags)", 0.0 },
    { "ui-novw",  "the proposed verdict without the 1 s view hold either", 0.0 },
    { "stereo",   "the first DOUBLE draw - the mono window ENDS here",    0.0 },
};

static bool   g_suArmed    = false;   // a load is being scored right now
static bool   g_suReported = false;   // this load has printed its summary
static double g_suT0       = 0.0;     // when the load began
static int    g_suLoads    = 0;       // loads scored this session
static bool   g_suOn       = true;    // [Startup] Timing

// Set from the scene-draw decision when it first doubles, because that module
// owns the transition and this one must not re-derive it.
static bool g_suStereoSeen = false;
