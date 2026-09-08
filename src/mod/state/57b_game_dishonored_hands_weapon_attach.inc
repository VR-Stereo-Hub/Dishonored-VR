// State chunk 57b for game/dishonored/hands/weapon_attach - included by
// src/mod/dishonoredvr.cpp.
//
// ===========================================================================
// VR-33 W2/W3: MOVE THE WEAPON WITH THE HAND, NOT TO A NEW GRIP
//
// REVISED 2026-09-08 after review. The first implementation was never reached
// (adoption depended on a sweep that attributed nothing), and the review found
// that had it been reached it would still have been wrong. What changed:
//
//   - IDENTIFICATION no longer comes from the whole-scene hide sweep. It comes
//     from a COORDINATE BRIDGE built on a draw we already trust, and a full
//     transform match against the components the game itself names.
//
//   - THE CORRECTION is no longer a per-weapon grip solve. The game already
//     authors the hand-to-crossbow and crossbow-to-bolt relationships, so the
//     shortest correct move is to carry the whole assembly through the SAME
//     transform the hand took. No weapon pivot, no second calibration, and the
//     bolt keeps its animated relationship to the stock for free.
//
// THE COMMON CORRECTION. The hand path computes D_hand_local, which moves the
// hand's own animated palette into the tracked pose. In its own local space
// that is not portable, so the hand publishes it conjugated into the draw's
// camera-relative world:
//
//     D_common = L_hand * D_hand_local * inverse(L_hand)
//
// Any other member drawn in the SAME view then takes
//
//     D_member_local = inverse(L_member) * D_common * L_member
//     newPalette     = D_member_local * originalPalette
//
// Both L are the DRAW transforms, so both sides are in one space by
// construction and the CPU-side transforms are needed only to identify which
// draw is which. Every bone of a member gets one common transform, so internal
// animation survives; and because the crossbow and the bolt take the SAME
// D_common rather than each independently targeting the palm, their relative
// geometry survives too. That last point is what the previous design got
// wrong: two members each solved onto one palm do not stay attached to each
// other.
//
// THE MODEL SCALE is folded into D_common exactly once, on the hand side,
// because that is where it is already applied. WaDraw must not apply it again.
//
// GENERATION. D_common is only valid for the Present, pose generation and EYE
// it was built in. A correction from the previous eye is a whole IPD wrong, so
// the match is exact and a miss REFUSES rather than borrowing. The previous
// design's bare boolean could not express this and would have consumed the
// last eye's target.
//
// THE COORDINATE BRIDGE. The shader's LocalToWorld and the component's own
// native matrix describe the same placement, but the shader's is rebased on
// the view. Rather than declare a candidate's own residual to be the camera
// position - which cannot validate the candidate that produced it - the offset
// is taken from a component we have ALREADY identified by other means: the
// body mesh the split locks and draws.
//
//     K                       = L_draw_ref * inverse(L_cpu_ref)
//     predicted L_draw_member = K * L_cpu_member
//
// A candidate is then checked on the WHOLE predicted transform - orientation,
// position and scale - and must be unique by a margin. Rotation alone is a
// filter, never an identity: a bolt lying in the crossbow's channel shares its
// orientation, and so do plenty of world props.
//
// CONVENTION. Native rows and shader registers are converted into the math
// helper's column-vector representation. A reference fitting its own bridge
// is tautological; the independent member matches are the useful evidence.
// Native scale is retained rather than normalised away.
// ===========================================================================

enum {
    WA_MAX_COMP  = 16,     // components tracked from the script lane
    WA_MAX_MESH  = 12,     // matched draw contracts
    WA_MAX_REGS  = 256     // registers we are willing to patch
};

// ---- the hand's correction, published for the view it belongs to ------------
// ---- the components, snapshotted on the script lane -------------------------
struct WaComp {
    uint8_t*      obj;         // identity and lifetime, never dereferenced here
    char          asset[64];
    char          name[64];
    dvr::hf::Mat3 R;           // native basis, same extraction as the shader's
    float         t[3];
    float         scale[3];    // column lengths, RETAINED not divided out
    bool          ok;
    bool          isRef;       // the body mesh: the bridge's anchor
    bool          isMember;    // a weapon member we are willing to attach
    int           hand;
};
struct WaCommon {
    dvr::hf::Xform D;          // camera-relative world space
    dvr::hf::Xform L_hand;     // the hand draw's own transform, for the bridge
    uint32_t present;          // dvr::frame::count() when it was built
    uint32_t poseGen;          // the pose snapshot it used
    int      eye;              // g_mpEyeState at that draw
    WaComp components[WA_MAX_COMP];
    int componentCount;
    uint32_t componentGen;
    bool     ok;
};
static WaCommon g_waCommon[2];        // per hand


static SRWLOCK g_waCompLock = SRWLOCK_INIT;
static WaComp   g_waComp[WA_MAX_COMP];
static int      g_waCompN   = 0;
static uint32_t g_waCompGen = 0;       // bumped on every republish
static double   g_waCompMs  = 0.0;

// ---- a matched draw contract ------------------------------------------------
//
// A vertex/index pair is NOT one geometry: shared and dynamic buffers hold
// several ranges. The contract keeps the discriminators that actually separate
// one member's draw from another's in the same buffers.
struct WaMesh {
    void*    vb, *ib, *decl, *vs;
    UINT     stride, startIndex, numVerts, primCount, streamOffset, minIndex;
    D3DPRIMITIVETYPE type;
    INT      baseVertex;
    int      comp;             // index into g_waComp
    int      hand;
    char     asset[64];
    int      boneReg;          // the REFLECTED register, never assumed to be 6
    UINT     regs;             // registers actually patched
    double   lastVerifyMs;     // revalidated on every draw, also used for eviction
    LONG     placed, refused;
    // THE CORRECTION THIS CONTRACT LAST APPLIED, and the Present it was built
    // in. A sibling pass of the same geometry in the same Present is the same
    // mesh at the same place, so it takes the same delta - and requiring the
    // Present to match exactly is what stops a stale one being reused.
    dvr::hf::Xform dm;
    uint32_t dmPresent;
    bool     dmOk;
    LONG     ghosts;           // sibling passes corrected through this contract
};
static WaMesh   g_waMesh[WA_MAX_MESH];
static int      g_waMeshN = 0;

// ---- levers -----------------------------------------------------------------
static bool     g_waOn         = false;   // [Hands] AttachWeapons
static int      g_waSwordHand  = 1;      // [Hands] AttachSwordHand
static int      g_waXbowHand   = 0;      // [Hands] AttachCrossbowHand
static float    g_waAngTolDeg  = 0.25f;  // [Hands] AttachAngleTol - PROVISIONAL
static float    g_waPosTolUU   = 1.0f;   // [Hands] AttachPosTol   - PROVISIONAL
static float    g_waMarginX    = 4.0f;   // runner-up must be this much worse
static int      g_waMaxTry     = 3000;   // diagnostic watermark, never skips placement

// ---- routing counters, BEFORE every gate ------------------------------------
//
// Zero attachment must produce an explicit reason. The previous build printed
// nothing at all when nothing was adopted, so three runs could not distinguish
// "never entered" from "entered and refused".
static LONG g_waSeen        = 0;   // indexed draws the router looked at
static LONG g_waNoCommon    = 0;   // no valid hand correction for this view
static LONG g_waCandChecked = 0;   // full transform comparison performed
static LONG g_waMatched     = 0;   // a unique candidate won
static LONG g_waAmbiguous   = 0;   // two candidates within the margin
static LONG g_waNoCandidate = 0;   // nothing within tolerance

// ---- THE GHOST PASS ---------------------------------------------------------
//
// A weapon mesh is drawn more than once per frame: a colour pass, and at least
// one depth or shadow pass. Only a pass whose shader declares LocalToWorld can
// be identified by transform, so the others were refused and drawn where the
// engine put them - which is a dark copy of the weapon standing at the native
// position while the real one follows the hand. The tester reports exactly
// that: locked to where the weapon used to be, flickering many times a second.
//
// A refused draw that shares a MATCHED contract's geometry - same buffers,
// same range, same primitive count, different shader - is that pass. It does
// not need identifying again; it needs the correction its sibling already
// computed this Present.
//
// The cheap pre-filter is the primitive count, which arrives as a call
// argument and costs nothing to test. Only a draw whose triangle count matches
// a live contract pays for the device reads.
static bool g_waGhostFix    = true;   // [Hands] AttachGhostFix
static LONG g_waGhostSeen   = 0;   // refused draws sharing a contract's geometry
static LONG g_waGhostFixed  = 0;   // of those, corrected
static LONG g_waGhostNoBone = 0;   // no BoneMatrices declaration at all
static LONG g_waGhostNoDelta = 0;  // sibling had no correction this Present
static LONG g_waGhostRange  = 0;   // the palette range failed its bounds check

// ---- THE PROBE --------------------------------------------------------------
//
// The first ghost detector reported ZERO and that reading is trustworthy: no
// refused draw shares a matched contract's exact geometry. It also exposed its
// own flaw. Its cheap pre-filter was the PRIMITIVE COUNT, so a pass drawing a
// different number of triangles from the same mesh - a shadow LOD, a separate
// section, a different index range - was rejected before anything looked at
// it. A filter that can only find what it already assumed is not evidence.
//
// The probe drops that assumption. On a refused draw it compares the VERTEX
// BUFFER against the contracts, which is the loosest useful key, and reports
// what DIFFERS about anything that hits: index buffer, range, shader,
// primitive count. That distinguishes the three live possibilities - the same
// buffers at another range, another mesh entirely, or a pass that never
// reaches this router at all - which have so far been indistinguishable.
//
// Bounded per Present so it cannot cost the frame, and it reports the budget
// it used so a truncated probe is never read as an absence.
static bool g_waProbe       = true;   // [Hands] AttachProbe
static int  g_waProbeBudget = 400;    // device reads per Present
static LONG g_waProbeRan    = 0;
static LONG g_waProbeVbHit  = 0;   // shares a contract's vertex buffer
static LONG g_waProbeIbHit  = 0;   // ... and its index buffer
static LONG g_waProbeMiss   = 0;   // not our geometry at all
static LONG g_waProbeCapped = 0;   // refused draws the budget did not reach
static LONG g_waNonIndexed  = 0;   // draws that never reach this router

// ---- READINESS --------------------------------------------------------------
//
// A run on 2026-09-08 attached nothing and the only sign of why was a
// no-bridge counter buried in the beat. The cause was upstream and simple: the
// component list held no weapon at all that run, so there was never anything
// to attach. The build before it, on the same binary path, held six components
// including the crossbow and attached fine.
//
// "The weapons did not attach" and "the weapons were never there to attach"
// are different answers and the log has to separate them WITHOUT being read
// counter by counter. These say, in one line, whether the attachment has what
// it needs and what is missing if not.
static int  g_waRefN        = 0;   // components that can anchor the bridge
static int  g_waMemberN     = 0;   // weapon members eligible to attach
static int  g_waDroppedN    = 0;   // candidates that went away before snapshot
static LONG g_waNotReady    = 0;   // ticks with nothing to attach
static LONG g_waCacheHit    = 0;   // matched by a cached contract
static LONG g_waNoLayout    = 0;   // the bound shader has no usable layout
static LONG g_waNoSource    = 0;   // palette unreadable or not skinned
static LONG g_waAttempted   = 0;   // patched and submitted a draw
static LONG g_waSucceeded   = 0;   // that draw returned success
static LONG g_waRestoreFail = 0;   // constants could not be put back
static LONG g_waEarlyMember = 0;   // a member drew BEFORE the hand this view
static const char* g_waWhy  = "no weapon draw has been routed yet";

// The best and runner-up residuals of the most recent comparison, so a refusal
// can say HOW close it came instead of only that it failed.
static float g_waBestAng = 0.0f, g_waBestPos = 0.0f;
static float g_waNextAng = 0.0f, g_waNextPos = 0.0f;
static char  g_waBestName[64] = "none";

static LONG g_waHandCompared[2] = {};
static LONG g_waStaleComp = 0, g_waBudgetSkip = 0, g_waNoBridge = 0;
static float g_waNearestScore[2] = { FLT_MAX, FLT_MAX };
static float g_waNearestAngle[2] = {}, g_waNearestPos[2] = {}, g_waNearestScale[2] = {};
static char g_waNearestName[2][64] = {};
