// State chunk 57 for game/dishonored/hands/weapon_id - included by
// src/mod/dishonoredvr.cpp.
//
// ===========================================================================
// VR-33 W1: WHICH DRAWS BELONG TO WHICH WEAPON COMPONENT
//
// To move the crossbow we have to know which DrawIndexedPrimitive calls are
// the crossbow. The two ends of that question already exist in this tree and
// have never been joined:
//
//   - the SCRIPT side knows the components by name. `mat_hide.cpp` resolves
//     Skm_Player, Wpn_PlySword01, crossbow_01 and bolt_01 as live components
//     and can hide any one of them with ShowMaterialSection, which the tester
//     confirmed in the headset on 2026-09-06;
//   - the DRAW side knows draws by identity - vertex buffer, index buffer,
//     declaration, shader, range - which is what draw_census.cpp established.
//
// Neither can name the other. So JOIN THEM BY MAKING THE PICTURE MOVE, which
// is this project's own rule for identifying a render pass: hide one component
// at a time and record which draw signatures stop being submitted. A signature
// that is present in the baseline and absent exactly while `crossbow_01` is
// hidden IS the crossbow, and nothing about palette size, draw order or
// proximity was guessed to get there.
//
// WHY THIS AND NOT A GUESS. The inherited lore says "c6 x36 = the sword". A
// size is not an identity: several components can share a bone count, one
// upload can feed several draws, and a size list built by observation only
// contains what somebody happened to see. The same reasoning closed the
// arm/hand question, and it applies here unchanged.
//
// COST. Everything is behind `g_wiOn`. Off, this costs one predictable branch
// per draw. Armed, it takes the four identity Get calls per SKINNED draw only -
// the same gate the census uses - and each is released immediately, so no
// reference to an engine D3D object outlives the detour.
//
// LANES. Signatures are recorded on the draw lane. The hides are dispatched
// through ProcessEvent on the SCRIPT lane, beside the material cycler, because
// that is the lane rule this project has already paid for. The two communicate
// through a phase index and plain counters.
//
// WHAT IT DOES NOT ESTABLISH. A draw signature identifies a geometry and pass
// combination, not a unique game instance: the same buffers can draw a dropped
// copy or an NPC's weapon, and a different shader can draw the same mesh in
// another pass. This narrows the field to a named component's draws and says
// so; qualifying a live owned instance is the next step, not this one.
// ===========================================================================

// WHAT THE FIRST RUN GOT WRONG, and what each number here is for.
//
// The 2026-09-07 run reported 'Skm_Player OWNS ...' and every part of it was
// worthless. Three separate defects, all visible in its own output:
//
//  1. IT SWEPT BEFORE THE WEAPONS EXISTED. It fired about 17 s after the
//     config loaded, when the only resolved component was Skm_Player - no
//     crossbow_01, no bolt_01, no Wpn_PlySword01. A sweep whose plan cannot
//     contain the thing it is there to identify must REFUSE, not run. It now
//     waits for a component that is not the player body and holds the plan
//     until the candidate list has stopped changing (WI_SETTLE_MS), because
//     the list is rebuilt live and planning mid-rebuild plans against a
//     half-built list.
//
//  2. THE TABLE SATURATED. 128 signatures held, 55,272 draws did not fit. Once
//     full the table cannot record a new signature, so every later phase is
//     partly blind and the orphan count - which is the report's only control -
//     is measuring the table's capacity rather than the world. An overflow now
//     VOIDS the report instead of appearing as a footnote under attributions
//     it silently broke. The table is also far bigger, because the population
//     is every skinned draw in the scene and not just the view model.
//
//  3. ONE DISAPPEARANCE IS NOT EVIDENCE. 'this draw stopped during a 1.5 s
//     window' is produced just as well by a camera move, an NPC walking out of
//     frame, or an LOD switch. The sweep now runs HIDE, SHOW, HIDE, SHOW per
//     component and requires a signature to vanish on BOTH hides and come back
//     on BOTH shows. That is a four-times-repeated prediction that the world
//     alone will almost never satisfy, and it is the difference between an
//     instrument that can fail its own hypothesis and one that cannot.
//
// COST OF THE BIGGER TABLE: WI_MAX_SIG * (sizeof(WiSig) + WI_MAX_PHASE*4),
// about 2048 * (64 + 84) = 300 KB of static data, paid whether or not the
// sweep is armed. That is the price of the control being trustworthy.
enum {
    WI_MAX_SIG   = 2048,
    WI_MAX_PHASE = 42,     // 1 baseline + 4 per component
    WI_PER_COMP  = 4,      // hide, show, hide, show
    WI_MAX_COMP  = 10,     // (WI_MAX_PHASE - 1) / WI_PER_COMP
    WI_SETTLE_MS = 3000    // the candidate list must be unchanged this long
};

struct WiSig {
    uint64_t h;              // the identity hash
    void*    vb;             // kept for the report, never dereferenced
    void*    ib;
    void*    vs;
    void*    decl;
    UINT     stride, primCount, numVerts, startIndex;
    INT      baseVertex;
    uint32_t bones;          // registers/3 of the palette current at the draw
};

static bool     g_wiOn        = false;  // [Hands] WeaponId
static WiSig    g_wiSig[WI_MAX_SIG];
static int      g_wiSigN      = 0;
// Draws of each signature seen in each phase. Phase 0 is the baseline with
// nothing hidden; phase 1+n is "component n hidden".
static uint32_t g_wiSeen[WI_MAX_SIG][WI_MAX_PHASE];
static volatile LONG g_wiPhase = -1;    // -1 = not collecting
static LONG     g_wiOverflow  = 0;      // signatures that did not fit the table
static LONG     g_wiDraws     = 0;      // skinned draws inspected

// The sweep, driven on the script lane.
static int      g_wiStep      = -1;     // -1 not started, else the phase index
static int      g_wiCompOf[WI_MAX_PHASE];   // phase -> g_fpCand index
static bool     g_wiHidden[WI_MAX_PHASE];   // was the component hidden then?
static double   g_wiUntil     = 0.0;
static int      g_wiPhaseN    = 0;
static bool     g_wiDone      = false;
static bool     g_wiHiding    = false;
static float    g_wiPhaseMs   = 1500.0f;    // [Hands] WeaponIdMs
static int      g_wiTries     = 0;

// The wait for the assets to exist. The sweep cannot be planned against a
// candidate list that is still being built, and it must not be planned at all
// against one that contains no weapon - both produce a report that answers a
// question nobody asked.
static int      g_wiSeenN     = -1;     // last observed g_fpCandN
static void*    g_wiSeenFirst = NULL;   // and the identity of its first entry
static double   g_wiStable    = 0.0;    // when the list last changed
static bool     g_wiWaitSaid  = false;  // the "waiting for" line, printed once

// A phase whose hide DID NOT TAKE cannot attribute anything, and a phase whose
// restore did not take poisons the phase after it. Recorded rather than
// assumed, and reported.
static bool     g_wiPhaseBad[WI_MAX_PHASE];
