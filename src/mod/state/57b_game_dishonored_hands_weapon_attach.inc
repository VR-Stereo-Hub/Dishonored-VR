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
    // THE CONTRACT TABLE WAS EXACTLY THE SIZE OF THE ANSWER, WHICH IS WHY IT
    // THRASHED. Three meshes drawn by four shaders each is twelve contracts,
    // and every healthy run reported "contracts 12" - pegged at the cap. One
    // more distinct draw then evicts the oldest, which is a contract still in
    // use; it has to be identified again, is uncorrected until it is, and the
    // copy reappears. That is the tester's report exactly: the copies went
    // away for two or three seconds and then came back flickering, and
    // flickering in a stance that used to be steady, because eviction does not
    // care about stance.
    //
    // A cap that is also the expected answer cannot absorb a weapon swap, a
    // second bolt type, an extra pass or a level change. Sixty-four costs a
    // few kilobytes and leaves room for all of them.
    WA_MAX_MESH  = 64,     // matched draw contracts
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
    dvr::hf::Xform palm;
    float forward[3];
    float unitsPerMeter;
    void* target; D3DVIEWPORT9 viewport;
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
    // VR-59: the COMPONENT this contract was matched to. A contract names a
    // geometry; this names the instance, and it is the thing every draw on
    // these buffers is verified against.
    void*    compObj;
    // Where this contract last verified AS THE HELD ITEM, in the draw space of
    // that verification, refreshed on EVERY verified draw. This is lastL2W
    // done properly: lastL2W is written only where the transform matcher adopts
    // a contract, which happened 4 times in 13 million draws in one run, so any
    // test against it is comparing with a reference that is almost always
    // stale. That single fact is why the pass-radius gate both misfired on held
    // weapons and missed fired bolts.
    // When this contract's component was last FOUND in a fresh snapshot. A
    // component that has gone means the contract is about something that no
    // longer exists, and it must be retired rather than refusing forever.
    uint32_t compSeenPresent;
    bool     compSeenOk;
    float    heldAt[3];
    uint32_t heldPresent;
    bool     heldOk;
    int      hand;
    char     asset[64];
    int      boneReg;          // the REFLECTED register, never assumed to be 6
    UINT     regs;             // registers actually patched
    double   lastVerifyMs;     // revalidated on every draw, also used for eviction
    LONG     placed, refused;
    bool     wasLive;          // for the lock/unlock transition line
    // THE CORRECTION THIS CONTRACT LAST APPLIED, and the Present it was built
    // in. A sibling pass of the same geometry in the same Present is the same
    // mesh at the same place, so it takes the same delta - and requiring the
    // Present to match exactly is what stops a stale one being reused.
    dvr::hf::Xform dm;
    uint32_t dmPresent;
    bool     dmOk;
    // WHICH PREDICTION WON when this contract was identified. A member is
    // offered twice - once in the reference draw's rebased space and once in
    // world space - and the two take different corrections. A sibling pass of
    // the same geometry belongs to the same one, and guessing would put it
    // somewhere new rather than where its twin went.
    bool     useNative;
    // WHERE THIS CONTRACT WAS LAST DRAWN, and when. Used only to decide
    // IDENTITY for another pass of the same geometry, never to position
    // anything: a world instance of the same mesh - a bolt standing in the
    // ground - shares these buffers and must not be swept into the hand.
    float    lastL2W[3];
    uint32_t lastL2WPresent;
    bool     lastL2WOk;
    LONG     ghosts;           // sibling passes corrected through this contract
};
// VR-64: retire a contract whose component is DEAD, on a candidate refresh.
// Absence from the refreshed list is not death - a stowed weapon's component is
// still alive - and retiring there cost a second of flicker on every swap back.
static bool     g_waRetireDead = true;    // [Hands] AttachRetireDeadOnRefresh
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

static bool g_waGhostFix    = true;   // [Hands] AttachGhostFix
#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_primitive_sibling_state.inc"
#endif

#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_refused_probe_state.inc"
#endif

static LONG g_waNonIndexed = 0;  // non-indexed draw population

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

// ---- BUFFER IDENTITY, AHEAD OF THE LAYOUT GATE ------------------------------
//
// THIS IS THE ARM FIX, APPLIED TO THE WEAPON. Dropping the three palette-fed
// passes over the arm mesh left a faint arm still drawn, and the note in
// draw_census.cpp says exactly why: "a pass whose bone constants land outside
// the reuse window, or at a register other than c6, is invisible to it.
// Buffer identity does not care. Any draw bound to this vertex and index
// buffer IS this mesh, whatever the constants did."
//
// The weapon router had the same blindness in a different costume. It demands
// a full shader layout - ViewProjection, LocalToWorld and BoneMatrices - before
// it will look at a draw at all, and refuses about 5.97 million per run on that
// gate alone. A weapon pass whose shader declares no LocalToWorld is invisible
// to it in precisely the way the extra arm pass was invisible to the census.
//
// The layout is needed to IDENTIFY a draw by comparing transforms. It is not
// needed to RECOGNISE one whose buffers we have already identified. So the
// buffer check now runs first and unbudgeted, and a hit is the weapon whatever
// its constants look like.
//
// AND THE PROBE'S ZERO WAS AN ARTEFACT. It reported 0 of 2.8 million refused
// draws sharing a weapon buffer, but it also skipped 3.7 million more on a
// per-Present budget that runs out on world geometry long before the frame
// reaches the view model, which is drawn last. A budget that expires before
// the target is drawn cannot see the target. That is the same error as the
// primitive-count filter before it, and it is why the buffer check below has
// no budget at all.
static LONG g_waIdSeen      = 0;   // draws recognised by buffer identity
static LONG g_waIdCorrected = 0;   // ... corrected through the sibling delta
static LONG g_waIdNoBone    = 0;   // ... with no BoneMatrices declared
static LONG g_waIdNoDelta   = 0;   // ... whose sibling had no delta this Present

// ---- THE NON-INDEXED POPULATION ---------------------------------------------
//
// The weapon router lives on DrawIndexedPrimitive only, so a pass drawn with
// DrawPrimitive has never been compared against anything - about 31,000 per
// run, roughly four a frame, which is the scale of a few view-model passes
// rather than of world geometry. draw_census.cpp hooks this entry for exactly
// this reason: "a pass that draws without an index buffer would otherwise
// survive every suppression this file performs and look like a rendering
// mystery."
static LONG g_waPrimSeen    = 0;
static LONG g_waPrimVbHit   = 0;
static LONG g_waPrimFixed   = 0;
static LONG g_waPrimNoBone  = 0;
static LONG g_waPrimNoDelta = 0;


// ---- THE NEAREST MISS -------------------------------------------------------
//
// The copy stays at the native position AND ANIMATES CORRECTLY - the bow fires,
// the sword flips on crouch. A copy that animates is being skinned by the
// game's own animated palette, so it IS the weapon mesh, drawn from buffers
// nothing here has ever seen: every draw touching a contract's buffers is one
// of the twelve contracts, and the probe found no other.
//
// It must therefore be among the draws that WERE compared and matched nothing
// - tens of thousands a run. The existing 'interval nearest' cannot show it,
// because that statistic is saturated by the exact matches at 0.0000 deg and
// 0.0005 uu; the winner drowns out everything behind it.
//
// This records the best residual among draws that MISSED, with the geometry
// that produced it. If the copy is the same mesh at the same animated pose, it
// will sit at a small, characteristic residual and these buffers name it - and
// naming its buffers is all the correction needs, because recognition by
// buffer identity is already built.
struct WaMiss {
    float angle, position, scale, score;
    void* vb, *ib, *vs;
    UINT  primCount, numVerts, startIndex, stride;
    char  asset[64];
    bool  ok;
};
static WaMiss g_waMiss[2];        // per hand, reset each reporting interval

// ---- THE VIEW MODEL CENSUS --------------------------------------------------
//
// Every contract is c6 x225 - one seventy-five bone palette shared by the body,
// the sword, the crossbow and the bolt. Components on one view-model rig share
// a LocalToWorld, so a transform match cannot tell those meshes apart: the
// identity is in the BUFFERS, and the transform only says where the rig is.
// That is consistent with everything measured. The weapons attach because the
// matcher picks a component whose transform is right; a mesh it assigns to the
// wrong component, or to none, is simply left where the engine drew it, and it
// animates because the game's own palette is still driving it.
//
// So stop asking which component a draw belongs to and just list what is
// actually drawn on this rig. Every distinct skinned geometry that reaches the
// matcher is recorded once, with its buffers, size, LocalToWorld translation,
// nearest component and residual, and whether it was corrected. Twenty or so
// lines, printed once each, and between them they account for every piece of
// the view model - including whichever piece is still standing at the native
// position.
//
// This is a census, not a filter. It cannot exclude the thing it is looking
// for, which is the failure mode of the last four attempts: a primitive-count
// pre-filter, a palette gate, a per-Present budget and a tolerance band each
// hid the target before anything could see it.
enum { WA_MAX_CENSUS = 48 };
struct WaCensus {
    void*  vb, *ib, *vs;
    UINT   primCount, numVerts, startIndex, stride;
    float  l2w[3];
    float  angle, position;
    char   nearest[64];
    bool   corrected;
    LONG   draws;
};
static WaCensus g_waCensus[WA_MAX_CENSUS];
static int      g_waCensusN = 0;
static bool     g_waCensusOn = true;   // [Hands] AttachCensus

// ---- KEEPING WORLD INSTANCES OUT OF THE HAND --------------------------------
//
// A fired bolt standing in the ground is the SAME MESH as the loaded one and is
// drawn from the same buffers, so both of this build's identity routes can
// reach it: the component snapshot will happily hold a bolt_01 that is no
// longer in the crossbow, and buffer identity does not distinguish instances
// at all. The tester found one attached to the hand and hanging in the sky.
//
// Two gates, both measured rather than tuned. A view-model rig's components sit
// together - in the reference run the body, sword, crossbow and bolt spanned
// about sixty units - so a member far from the bridge anchor is not on the rig
// and is not a member. And another pass of a contract is drawn at the same
// place as the contract itself: the census measured the uncorrected crossbow
// pass 0.3 units from its corrected twin, while a world instance is metres
// away.
static float g_waRigRadiusUU = 200.0f;   // [Hands] AttachRigRadius
static float g_waPassRadiusUU = 60.0f;   // [Hands] AttachPassRadius
static LONG  g_waOffRig      = 0;   // members rejected as not on the rig
static LONG  g_waOffPass     = 0;   // same-geometry draws rejected as elsewhere

// ---- THE VIEW MODEL IS A PLACE, AND IT SEPARATES CLEANLY --------------------
//
// The census settled what the tolerance band could not. Every view-model draw
// sits within about 170 units of the camera; the nearest world draw is 1880
// and most are three to nine thousand. That is better than a tenfold gap,
// measured, and it is a far stronger instance test than any angular band.
//
// Inside it, the ASSIGNMENT was correct every single time and only the
// RESIDUAL was too large: the sword's draw named Wpn_PlySword01 at 11.1
// degrees, the bolt's named bolt_01 at 12.2, the crossbow's named crossbow_01
// at 0.000. The sword and the bolt hang off sockets, so their component
// transform does not track their render transform the way the crossbow's does
// - and a band tight enough to keep world geometry out was never going to
// admit them. That is why the sword attached only intermittently: sometimes
// its residual happened to dip under the band.
//
// So the two jobs are separated. PROXIMITY decides that a draw is on the view
// model at all, which is what keeps a fired bolt in the ground out. A RELAXED
// band then picks which member it is, wide enough for a socket-mounted weapon
// and still narrow enough to reject the body mesh, which named a member at
// 175 degrees and 143 units. The strict band still governs everything outside.
//
// This is not widening a tolerance to manufacture a match. The strict band is
// unchanged where it was doing real work; what replaces it inside the view
// model is a different and better-separated test.
static float g_waViewModelUU  = 500.0f;  // [Hands] AttachViewModelUU
static float g_waNearAngDeg   = 20.0f;   // [Hands] AttachNearAngle
static float g_waNearPosUU    = 30.0f;   // [Hands] AttachNearPos
static LONG  g_waNearAccepted = 0;   // accepted on the relaxed band
// A pass we RECOGNISE but cannot place is suppressed rather than drawn where
// the engine put it. It is a duplicate of geometry the same frame draws
// correctly, so the cost is at most one pass of depth or shadow and the saving
// is the copy itself.
static bool  g_waDropUncorrected = true;  // [Hands] AttachDropUncorrected
static LONG  g_waIdDropped    = 0;
// How often the weapon draw's own pose snapshot had a different generation from
// the hand's. This USED to refuse the correction; it is now tolerated, and the
// counter is what makes that decision checkable rather than asserted.
static LONG  g_waPoseGenDiff  = 0;

// ---- SNAPSHOT AGE IS THE VIEW-MODEL'S SWAY --------------------------------
//
// The tester falsified the frame-rate story: a locked spot stays locked
// whatever the frame rate does, but ROTATING away loses the lock and rotating
// back regains it. That is a camera-POSE effect, and it points at one thing.
//
// The bridge cancels camera motion algebraically. K * native_member expands to
// L_draw_ref * (inverse(L_native_ref) * L_native_member), and the bracketed
// term is the member RELATIVE TO THE RIG - it has no camera in it at all, while
// L_draw_ref is this frame's. So neither walking nor turning can move the
// prediction through the bridge.
//
// What the bracket DOES carry is the member's pose relative to the rig at the
// moment the snapshot was taken. Turning the view makes Dishonored's view model
// SWAY on its own inertia, so that relative pose is exactly what changes while
// you rotate - and a snapshot up to 100 ms old then predicts where the weapon
// WAS, not where it is. Hold still and the sway settles onto the snapshot
// again, which is the relock.
//
// The bound was 100 ms: nine frames of sway at 90 Hz, and the observed misses
// were 11-12 degrees. Tightening it is the direct test of this, and the age is
// now reported next to every residual so the correlation can be read off a log
// rather than argued.
static float g_waSnapMaxMs    = 20.0f;   // [Hands] AttachSnapshotMaxMs
static float g_waSnapAgeAtUse = 0.0f;    // age of the snapshot the match used

// ---- SUPPRESS WHAT WE CANNOT PLACE ------------------------------------------
//
// The tester asked the question that should have been asked four builds ago:
// why reposition the copy at all? It is a DUPLICATE of geometry the same frame
// already draws correctly, so placing it correctly and not drawing it are
// visually identical - and one of those needs a coordinate bridge, a fresh
// component snapshot, a same-view correction and a band wide enough for a
// socket-mounted member, while the other needs none of it.
//
// So the rule becomes blunt and route-independent: a draw on a weapon's OWN
// buffers that we did not place ourselves is not drawn. It does not matter
// whether it failed for want of a layout, a view, a delta or a match; if we
// know those buffers are a weapon's and we did not correct this draw, the only
// thing drawing it can produce is the copy.
//
// WHAT THIS COSTS. That pass is most likely a depth or shadow contribution.
// Losing it can mean the weapon no longer primes depth or no longer casts from
// this pass - if the weapon starts z-fighting with the world, or its shadow
// goes missing, this lever is why. It cannot make the weapon vanish outright:
// the corrected pass is a different draw and still runs.
//
// Kept as a lever rather than a hard-coded behaviour because it trades a known
// artifact for a possible one.
static bool  g_waSuppressUnplaced = true;   // [Hands] AttachSuppressUnplaced
static LONG  g_waSuppressed       = 0;
static float g_waNearMargin   = 1.5f;    // [Hands] AttachNearMargin
static LONG  g_waNearRejected = 0;   // on the view model and still refused

// ---- VR-59: A FIRED BOLT IS A DIFFERENT INSTANCE OF THE LOADED ONE ---------
//
// A contract is keyed on BUFFERS, and buffer identity cannot tell two
// instances of one mesh apart - that is by design, and it is what lets one
// identified pass recognise the others. A crossbow bolt standing in the world
// is the same mesh drawn from the same buffers as the loaded one, so every
// route in the identity block can reach it, and no radius separates the two:
// a bolt fired into a wall a metre away is genuinely near the camera and
// genuinely near where the loaded bolt draws.
//
// The defect was not the radii. It was that a contract OUTLIVES the weapon
// being stowed - it is evicted only when the table fills - and the instance
// gate ran only when a fresh reference existed. With the crossbow put away the
// reference goes stale, the gate is skipped, and the hand for that contract is
// still publishing a fresh correction every frame because the HAND is always
// drawn. So the absence of evidence was read as permission, and a world bolt
// was corrected onto whatever is now in that hand.
//
// Two symptoms, one cause. Some passes of the fired bolt were CORRECTED (the
// copy that follows the hand) and the rest were SUPPRESSED as unplaced passes
// of a weapon (the dark stub left standing in the ground). The second is the
// documented cost of AttachSuppressUnplaced landing on a world instance: its
// colour and lighting passes are its own, not duplicates of anything.
//
// Four preconditions, each a lever, each with a counter that can print the
// unwelcome answer.
static bool g_waReqFreshRef   = true;   // [Hands] AttachRequireFreshRef
static bool g_waReqLiveMember = true;   // [Hands] AttachRequireLiveMember
static bool g_waVetoFrees     = true;   // [Hands] AttachVetoReleasesBuffers
static bool g_waVetoRelaxed   = true;   // [Hands] AttachInstanceVetoRelaxed
static int  g_waRefPresents   = 2;      // [Hands] AttachRefMaxPresents
// What would make each of these move, so a zero is readable:
//   NoFreshRef  - a draw on a contract whose mesh has not drawn on the view
//                 model for AttachRefMaxPresents presents. Reads 0 while the
//                 weapon is held and drawing every frame, which is normal.
//   Stowed      - a draw on a contract whose asset is not a live member of
//                 that hand in the current snapshot. Reads 0 until a weapon
//                 is put away with its geometry still on screen.
//   VetoFreed   - vetoed draws handed back to the engine untouched, which is
//                 what keeps a world bolt visible instead of suppressed.
//   VetoRelaxed - vetoed draws additionally refused the relaxed view-model
//                 band. Only the two STRONG vetoes do this; "no reference"
//                 never does, or a re-equipped weapon could not relock.
static LONG g_waNoFreshRef    = 0;
static LONG g_waStowed        = 0;
static LONG g_waVetoFreed     = 0;
static LONG g_waVetoedRelaxed = 0;

// ---- VR-59 attempt 2: verify every draw, correct none on faith -------------
//
// The lever is default ON. With it OFF the build reverts to trusting buffer
// identity, which is the behaviour every run up to this one had, so the two are
// directly comparable in a headset.
static bool g_waVerifyInstance = true;   // [Hands] AttachVerifyInstance
static int  g_waHeldMaxPresents = 2;     // [Hands] AttachHeldMaxPresents
// What moves each of these, so a zero is readable:
//   VerifiedHeld  - draws positively identified as the held item. This should be
//                   the overwhelming majority while a weapon is out; if it is 0
//                   and weapons are flat, verification is failing, not idle.
//   VerifiedAway  - draws on weapon buffers that are a DIFFERENT instance.
//                   Moves the moment a bolt exists in the world. A fired bolt
//                   that stays put with this counting up is the fix working.
//   VerifyNoRef   - draws that could not be verified either way, and were
//                   therefore left exactly as the engine drew them.
//   RefComponent / RefRecent - which reference answered, so "it works" can be
//                   told apart from "it works because the cheap path happened
//                   to agree". If RefComponent is 0 the engine-read route is
//                   not running at all and only the cache is holding it up.
static LONG g_waVerifiedHeld  = 0;
static LONG g_waVerifiedAway  = 0;
static LONG g_waVerifyNoRef   = 0;
static LONG g_waRefComponent  = 0;
static LONG g_waRefRecent     = 0;
// Draws returned to the engine rather than dropped, because they did not verify
// as the held item. Dropping claims a draw is a DUPLICATE; for a world instance
// that claim is false and the drop deletes the object. This counter moving is
// what a visible fired bolt looks like from the log side.
static LONG g_waHandedBack    = 0;

// ---- VR-60: THE EQUIPPED WEAPON SUPPLIES ITS OWN COMPONENT ------------------
//
// The pistol is not in FpCollect's component snapshot and never has been. That
// walk reads each 4-byte field as a UObject pointer, so it cannot traverse the
// TArray every inventory item lives in, and it sees only what happens to be
// reachable by direct pointer. Measured: all 19 snapshots in a run were
// identical, six components, no pistol, while the pistol was in hand.
//
// With no candidate of its own the pistol reaches a contract only through the
// buffer lookup and is then verified against ANOTHER weapon's component - one
// fixed in the world while the pistol orbits the head. That is the 45-degree
// detach cone: AttachPassRadius converted into an angle by geometry, not a
// rotation gate. Any threshold would produce some angle, because the identity is
// what is wrong.
//
// So the equipped item supplies its own component. VR-61 reads which item is in
// which hand from the engine (DishonoredInventoryItem::m_EquipUsage and
// m_CurSocket, per object), and that item's m_pPlayerMesh is the component its
// draws should be checked against.
//
// This also retires an assumption. The hand for these members comes from the
// engine's own equip usage rather than from asset-name substrings in WaHandFor,
// which the record has flagged as inherited guesswork since VR-33.
static bool g_waEquippedMembers = true;   // [Hands] AttachEquippedMembers
static LONG g_waEquipAdded   = 0;   // components injected from the equipped item
static LONG g_waEquipDup     = 0;   // already present from the pointer walk
static LONG g_waEquipNoMesh  = 0;   // equipped item with no readable player mesh

// ---- A CONTRACT MUST NOT OUTLIVE THE LEVEL IT WAS MATCHED IN ---------------
//
// A contract holds compObj, a pointer to the component it was matched to. A
// level load destroys that component, and the contract table is evicted only
// when it FILLS (64 entries), so after a reload every contract points at a dead
// object. WaVerifyDraw then finds no component reference, the recent-verification
// cache is stale too, and every draw returns NO_REF.
//
// That is not merely a refusal, it is a LOCKOUT: a refused draw returns before
// reaching the transform matcher, so the contract can never be re-adopted and
// the weapons never attach again for the rest of the session. Measured on the
// second load of a session: unverifiable climbing past 45,000 while corrected sat
// frozen at 16,245 and only 3 contracts had ever matched.
//
// Two defences, because either alone leaves a hole:
//
//   1. Drop every contract when the game leaves gameplay. Cheap, exact, and it
//      covers the level load that caused this.
//   2. Retire a contract whose component has been missing from a FRESH snapshot
//      for a while. This covers the cases a load hook cannot see - an item
//      destroyed mid-level, a weapon removed from inventory - and it is what
//      stops any future stale pointer becoming permanent.
static LONG g_waDroppedLoad  = 0;   // contracts dropped because a load began
static LONG g_waDroppedStale = 0;   // contracts retired: component long gone
static int  g_waStaleMaxPresents = 90;   // [Hands] AttachContractStalePresents


static float g_waAwayFarthest = 0.0f;   // the largest offset we refused
static float g_waHeldWorst    = 0.0f;   // the largest offset we still accepted


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

// VR-57: the measured model axis, handed from the DRAW that measures it to the
// script lane that writes it to the ini. Declared here because both mesh_split.cpp
// (which does the writing, beside the grip save) and bolt_model_ray.cpp (which does
// the measuring) are compiled into the unity build in that order.
static volatile LONG g_brSaveReq = 0;
static float g_brSaveOrigin[2][3] = {}, g_brSaveDir[2][3] = {};
