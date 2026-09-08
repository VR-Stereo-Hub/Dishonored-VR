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

enum { WI_MAX_SIG = 128, WI_MAX_PHASE = 26 };

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
static double   g_wiUntil     = 0.0;
static int      g_wiPhaseN    = 0;
static bool     g_wiDone      = false;
static bool     g_wiHiding    = false;
static float    g_wiPhaseMs   = 1500.0f;    // [Hands] WeaponIdMs
static int      g_wiTries     = 0;
