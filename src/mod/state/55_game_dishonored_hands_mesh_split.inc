// State chunk 55 for game/dishonored/hands/mesh_split - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-31 step 2: DERIVE the hand/arm cut from BONE INFLUENCE, per arm.
//
// Step 1 (draw_census.cpp) proved the arms and hands are one skinned triangle
// list and cut it by eye into 32 percentage slices. The tester's mask,
// 0x3001D237, works on the LEFT hand and takes too much of the right, and the
// reason is structural: the two arms occupy different, non-aligned regions of
// the one triangle list, so no single set of percentage ranges can be right
// for both. Eyeballing a second mask would only move the problem.
//
// So classify each TRIANGLE by the bones that actually move it. That is the
// mesh's own answer to "which arm, and hand or forearm", it is per-arm by
// construction, it is exact rather than a percentage, and it picks up the
// stray sleeve shards that survived the slice mask because they were never a
// question of ranges.
//
// WHAT THIS READS, AND WHY THAT IS A CHANGE. Step 1 read nothing: it re-issued
// the game's own draw with a shifted startIndex. Classification cannot work
// that way - it needs the index list and the per-vertex blend indices and
// weights. So this locks the game's vertex and index buffers ONCE, copies the
// draw's range out, and unlocks. Nothing is written back, nothing is kept
// beyond the copy, and no engine D3D object is referenced outside the call
// that read it. The copy never leaves memory and is never written to disk.
//
// A LOCK ON A WRITE-ONLY BUFFER CAN RETURN GARBAGE. D3DUSAGE_WRITEONLY says
// the application will not read, and a driver is within its rights to hand
// back an uninitialised staging copy. That is why every read is validated
// against facts the data must satisfy - indices inside the draw's own vertex
// window, bone indices below the palette size, weights summing to 1 - and why
// a failed validation REFUSES and logs the numbers instead of drawing a mesh
// carved up from noise. The descriptor (usage and pool) is logged too, so the
// first run says which world we are in rather than leaving it to be guessed.
//
// LANE: everything here runs on the render thread, inside the draw detour,
// because that is the only place the right buffers are bound. The hotkeys post
// requests; nothing acts on them off-lane.
// ===========================================================================

#define MS_MAX_VERTS  8192      // the mesh measures 2771
#define MS_MAX_TRIS   16384     // the mesh measures 4448
#define MS_MAX_BONES  128       // the palette measures 48
#define MS_MAX_STRIDE 64        // the mesh measures 32
// A clipped triangle contributes at most two new vertices, and only triangles
// that actually straddle the plane are clipped - a thin ring around one arm.
#define MS_MAX_CLIPV  8192
// The boundary segments the clip produced, one per straddling triangle: the
// ring the cap is built on.
#define MS_MAX_CAPSEG 8192
// A triangle cut by a plane becomes at most three (one on the near side, two on
// the far side), and BOTH halves are emitted so "all" still rebuilds the mesh.
#define MS_MAX_OUT    (MS_MAX_TRIS * 3)

// Triangle classes, and the ORDER the rebuilt index list is emitted in. Hands
// first and adjacent, so the common case - both hands, no arms - is ONE draw
// call over a contiguous range rather than a run per fragment.
#define MS_CLS_HAND_A 0
#define MS_CLS_HAND_B 1
#define MS_CLS_ARM_A  2
#define MS_CLS_ARM_B  3
#define MS_CLS_OTHER  4         // no recognised influence at all
#define MS_CLS_N      5

// What the draw path is allowed to emit. Cycled live with Numpad 0.
#define MS_MODE_OFF   0         // our split does nothing; step 1's behaviour
#define MS_MODE_HANDS 1         // hands only - the point of the exercise
#define MS_MODE_ALL   2         // every class, through OUR buffer (the A/B)
#define MS_MODE_ARMS  3         // the inverse cut: arms only, hands gone
#define MS_MODE_OTHER 4         // only the unclassified triangles
#define MS_MODE_N     5

struct MsVert {
    float   p[3];
    uint8_t bi[4];
    float   bw[4];
};

// THE EDGE IS CLIPPED, not rounded to whole triangles.
//
// A plane fixed the chunky jumps but not the outline: a triangle that straddles
// the cut was still kept or dropped whole, so the boundary was a sawtooth one
// triangle high, and on the coarse cuff geometry that reads as spikes hanging
// off the wrist. Keeping only triangles entirely past the plane trades the
// spikes for notches; neither is a clean edge.
//
// So cut the triangles themselves. A triangle crossing the plane is split into
// the part on each side, with new vertices interpolated along the two crossing
// edges - the standard plane clip, and the only construction that gives an
// exactly circular boundary. Both halves are emitted, into their own class, so
// "arms" is still the exact inverse of "hands" and "all" still rebuilds the
// original mesh.
//
// That needs a vertex buffer of ours as well as an index buffer, because the
// new vertices do not exist in the game's. The raw bytes are copied through
// unchanged and only the elements that can be interpolated are touched; blend
// INDICES are taken from the nearer parent vertex rather than blended, because
// a bone index is a name, not a quantity.
//
// It also needs stream 0 to be the ONLY stream, since re-basing the indices
// would desynchronise any second stream the game had bound. That is checked,
// and a mesh with more streams falls back to the whole-triangle rule with the
// reason in the log.

// THE CUT END IS CAPPED, because a cut surface is a hole.
//
// The mesh is a shell: an arm is a tube of triangles with nothing inside it.
// Clipping it at the plane gives a clean circular boundary and leaves that
// boundary OPEN, so at the wrist you look straight down the inside of the
// sleeve and out through the far wall - the "you can see through it" the ring
// walk shows at every step.
//
// So close it. The clip already produces exactly the ring that has to be
// closed - one boundary segment per straddling triangle - and a fan from the
// ring's centre to those segments is the disc that fills it. Both halves get
// their own cap, wound and normalled the opposite way, so the hand's stump is
// closed when the hand is drawn and the sleeve's stump is closed in the
// inverse view. In "all" the two caps are coincident, face away from each
// other and sit inside a closed mesh, so they are invisible - the A/B still
// rebuilds what the game drew, as seen.
//
// THE COLOUR IS THE RING'S OWN, NOT A CONSTANT. The cap has no texture of its
// own to sample and inventing one would be a guess about the asset. Instead
// every cap vertex is given the SAME texture coordinate as the most common one
// around the ring - the mode of the ring's UVs, not their mean, because a mean
// lands between two islands of an atlas and samples whatever happens to be
// there. One UV on every vertex means the whole disc interpolates to a single
// texel, so the cap comes out flat in the colour that dominates the cut: the
// sleeve's colour when the ring is on the sleeve, skin when it is past the
// cuff. It re-derives on every knob press, so it tracks the ring by
// construction rather than being told where the cuff is.
//
// Position and SKINNING come from the ring vertex instead, so the cap deforms
// with the arm and cannot part from the rim it is sewn to; only the appearance
// is borrowed.
static uint8_t  g_msRaw[MS_MAX_VERTS * MS_MAX_STRIDE];     // the window, verbatim
static uint8_t  g_msClipRaw[MS_MAX_CLIPV * MS_MAX_STRIDE]; // the vertices we made
static int      g_msClipN = 0;
static uint32_t g_msStride = 0;
static D3DVERTEXELEMENT9 g_msEl[MAXD3DDECLLENGTH];
static int      g_msNel = 0;
static uint32_t g_msOutIdx[MS_MAX_OUT * 3];   // the rebuilt triangle list
static uint8_t  g_msOutCls[MS_MAX_OUT];
static int      g_msOutN = 0;
struct MsCapSeg { uint32_t a, b; uint8_t sd; };   // oriented as the HAND side walks it
static MsCapSeg g_msCapSeg[MS_MAX_CAPSEG];
static int      g_msCapSegN = 0;
static bool     g_msCap = true;               // [Hands] CutCap - close the cut end
static bool     g_msCapTwo = true;            // [Hands] CutCapTwoSided
static int      g_msCapTris = 0;              // cap triangles in the last build
static int      g_msCapUv[3] = { -1, -1, -1 };// the ring vertex each cap took its colour from
static IDirect3DVertexBuffer9* g_msVb = NULL;
static bool     g_msOwnVb = false;            // stream 0 is the only stream
static int      g_msExtraStream = -1;         // which stream vetoed the clip
// A stream can be BOUND without the declaration referencing it. Leftover state
// from an earlier draw is not data this draw consumes, and vetoing the clip on
// it costs the clipped edge and the caps for no reason. Only a stream the
// declaration actually names counts.
static uint32_t g_msDeclStreams = 0;          // bitmask of streams the decl uses

// THE SAME MESH IS DRAWN BY MORE THAN ONE PASS, AND ONLY SOME OF THEM CAN BE
// CLIPPED. Measured 2026-09-07: one run locked the arm mesh twice, from the
// identical vertex and index buffers, and got different answers. The first
// draw reported "stream 0 is the only stream" and produced 78 clipped
// triangles and 312 cap triangles; the second reported "stream 2 is also
// bound", which vetoes owning stream 0, so the clip fell back to whole
// triangles and the caps vanished. The tester saw the wrist caps disappear
// between runs with no config change, twice, and this is why.
//
// A lock released after the mesh stops being drawn re-arms on whichever pass
// draws it next, and that is a coin toss. So a draw that cannot be clipped is
// SKIPPED rather than accepted, and the split waits for a pass it can own -
// which the first build proves exists. The skips are bounded: after enough of
// them the degraded split is taken anyway, loudly, because no hands at all is
// worse than hands with a rougher edge.
// WHAT THE SPLIT WAS BUILT FROM, checked again on every draw that tries to
// use it.
//
// Choosing a better BUILD pass does not make later draws compatible. MsDraw
// used to accept any draw with a matching primitive count and then bind our
// re-based index buffer and generated vertices to it. A draw of the same
// buffer pair with a different vertex window, start index, base vertex, stream
// offset or declaration would consume a split that does not describe it, and
// re-based indices with generated vertices are meaningless against another
// stream's data. Nothing about waiting for a clippable pass fixes that.
//
// Worse, the caller drops what MsDraw declines once a split is ready
// (draw_census.cpp:276) - the auto-arm fail-soft only covers !g_msReady - so
// an incompatible draw was being SUPPRESSED rather than passed through. So
// MsDraw now says WHICH of three things happened, and the caller acts on it.
static INT      g_msBuiltBaseVertex = 0;
static UINT     g_msBuiltMinIndex = 0, g_msBuiltNumVerts = 0;
static UINT     g_msBuiltStartIndex = 0, g_msBuiltStream0Off = 0;
static void*    g_msBuiltDecl = NULL;
static volatile uint32_t g_msIncompat = 0;    // our geometry, wrong draw contract
static bool     g_msPassThrough = false;      // MsDraw declined: DRAW IT NORMALLY

// A split that WANTED the clip and did not get it. It still draws - a
// sawtooth edge and no caps - but it is a degraded result, not a finished one,
// so a later pass that can be clipped is allowed to replace it. Without this
// the first degraded build was permanent: g_msReady gates the build path, so
// nothing could ever upgrade it.
static bool     g_msDegraded = false;

static int      g_msStreamSkips = 0;
static bool     g_msRetryLater = false;       // declined, but not refused
#define MS_MAX_STREAM_SKIPS 120

// The knob's step, as a fraction of the arm's length. Three sizes because the
// right one is not knowable in advance: coarse to find the region, fine to
// place the ring, ultrafine to settle a boundary a millimetre wide in the
// headset. Numpad . cycles them.
static const float kMsStep[3] = { 0.02f, 0.005f, 0.001f };
static volatile int g_msStepMode = 0;
static volatile int g_msStepReq = 0;

// The copied mesh. Static rather than allocated: a detour is the wrong place
// to be calling the heap, and this is bounded by the asset.
static MsVert   g_msVert[MS_MAX_VERTS];
static uint32_t g_msIdx[MS_MAX_TRIS * 3];
static uint8_t  g_msTriCls[MS_MAX_TRIS];

// Per-bone facts derived from the copy.
static float    g_msBoneW[MS_MAX_BONES];       // total influence weight
static float    g_msBoneCen[MS_MAX_BONES][3];  // weighted centroid, mesh space
static uint8_t  g_msBoneSide[MS_MAX_BONES];    // 0 = unused, 1 = side A, 2 = side B
static uint8_t  g_msBoneHand[MS_MAX_BONES];    // 1 = at or past the wrist
static uint8_t  g_msAdj[MS_MAX_BONES][MS_MAX_BONES / 8];   // co-influence graph

// THE CUT IS A PLANE, not a sphere around the hand bone.
//
// The sphere came first and it worked, but it moves in JUMPS: a bone is inside
// it or outside it, so every triangle that bone dominates flips at once. The
// tester's walk shows it exactly - nothing changed at all from scale 0.50 to
// 0.67, then 156 triangles per arm flipped in one press. The edge that leaves
// is a blobby outline of a bone's influence region, not a cut.
//
// A plane perpendicular to the forearm cuts a cylinder in a CIRCLE, and it
// moves continuously: a press moves it a couple of percent of the arm's length
// and the ring travels smoothly up or down. The axis comes from the mesh (the
// dominant direction of the forearm's own vertices), and the plane's starting
// position reproduces what the sphere was keeping, so the look the tester
// settled on survives the change of shape.
static float    g_msAxis[3][3];        // unit limb axis per side, pointing distally
static float    g_msCutRel[3] = { 0.0f, 0.0f, 0.0f };   // plane, relative to the hand bone
static float    g_msAxialLen[3] = { 0.0f, 0.0f, 0.0f }; // the side's extent along its axis
static int      g_msSeedN[3] = { 0, 0, 0 };   // triangles the sphere kept, per side
// Has the plane been placed by hand (ini or knob)? A derived seed is recomputed
// on every rebuild, which is right for a first run and wrong once the tester has
// chosen a position - a level load would otherwise silently undo the choice.
static uint8_t  g_msCutSet[3] = { 0, 0, 0 };
static uint8_t  g_msTriSide[MS_MAX_TRIS];     // which arm each triangle belongs to
static bool     g_msPlane = true;             // [Hands] WristPlane - the plane, not the sphere
static int      g_msEdge = 3;                 // 0 centroid, 1 all, 2 any, 3 CLIP

static int      g_msVerts = 0, g_msTris = 0, g_msBones = 0;
static uint32_t g_msMinIndex = 0;              // the draw's vertex window
static int      g_msClsStart[MS_CLS_N];        // triangle offsets in OUR buffer
static int      g_msClsCount[MS_CLS_N];

// ---- VR-33: the draw-scoped bone palette ------------------------------------
//
// The native lane is shut (ENGINE_NOTES: no SkelControl on this build is ever
// evaluated), so the hands are moved where the draw consumes them: the skinning
// matrices uploaded to c6. One common rigid delta D is applied to EVERY matrix
// of one hand class, and that class is drawn under its own palette.
//
// Finger animation survives, which is the whole reason this is not a static
// hand: sum_i w_i (D M_i) v = D (sum_i w_i M_i v). The engine keeps animating
// M_i; D only moves the result.
//
// What it CANNOT do, stated here so it is not rediscovered: this is a GPU-side
// edit. The engine's own transform is untouched, so weapons, muzzle effects and
// firing aim do not follow. The weapon half of VR-33 needs its own mechanism.
//
// Rung 1, which is what ships here, is a pure TRANSLATION on one hand class
// with the other class as the control. The rotation half needs the controller
// pose resolved into this palette's space, and putting an unverified delta and
// an unverified space in the same build is how a null result becomes
// unreadable.
static bool     g_mpOn      = false;   // [Hands] Palette
#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_axis_state.inc"
#endif
// The block the game last uploaded to c6. Cached at the upload rather than read
// back from the device: a D3D9 constant read-back is a pipeline stall, and the
// data is already in our hands for free.
static float    g_mpCache[4 * 256];
static UINT     g_mpCacheN  = 0;
static LONG     g_mpDraws   = 0;       // per-class draws issued under a palette
static LONG     g_mpNoCache = 0;       // per-class draw wanted, no c6 seen yet

// THE AXIS PROBE. The palette's three axes were a basis nobody had measured,
// and this is what measured them: a delta on one hand class with the other left
// alone as the reference, stepped by the TESTER so there is no phase to infer.
//
// A timed sweep came first and gave two readings - left/down/forward and
// down/forward/left. Those are one cycle entered a step apart, but nothing in
// either run could prove that rather than a changed basis; the tester said so
// before it was acted on, and the timed variant is gone. The stepped probe
// settled it in a single run.
//
// RESULT, recorded in ENGINE_NOTES: axis 0 LEFT, axis 1 DOWN, axis 2 FORWARD.
// Kept because it is what would re-measure the basis if the asset or the engine
// path ever changed.

// ---- the controller offset the placement consumes -----------------------------
//
// The present thread publishes only frame-free scalars: the controller's offset
// from the head, resolved into the head's own right/up/forward, in metres. The
// draw turns those into a world vector using the basis it reads from that
// draw's own constants, so no assumption about the game's axes lives here.
//
// THREE APPROACHES DIED BEFORE THIS ONE, and the reasons are worth keeping:
//
//  * A RELATIVE drive moved each hand by its controller's travel from a
//    captured neutral. Refuted on paper before it was re-run - the cancellation
//    argument covered the added displacement and omitted the animated hand
//    already underneath it, so a still controller left the original hand still
//    following the camera.
//  * A YAW RESIDUAL correction assumed the camera carried a body yaw the head
//    did not. The run measured phi constant at 144 deg across a 145 deg head
//    swing: the camera tracks the head 1:1 and phi is the UE/XR yaw-origin
//    offset, not a body yaw.
//  * ABSOLUTE placement with a CALIBRATED origin folded the coordinate-origin
//    difference, the grip offset and any anchor error into one 138 uu constant
//    applied in a rotating frame - a 1.4 m lever pivoting on the head.
//
// All three are in docs/dishonored/VR-33-HANDS-AND-WEAPONS.md, and in git under the tags
// vr33-placement-works and vr33-scale-correct.
static float    g_mpCtlRUF[2][3];
static bool     g_mpCtlRUFOk[2] = { false, false };
static float    g_mpDriveGain = 1.0f;   // [Hands] PaletteDriveGain - scale trim

// ---- the palm anchor ---------------------------------------------------------
//
// Placement needs to know where the visible palm IS this frame, and it finds out
// by skinning real vertices with the game's own unmodified palette rather than
// trusting an assumption about where the palette puts things.
//
// A fixed set of vertex identities chosen once at build time: the deduplicated
// vertices nearest the hand class's bind-pose centroid, so fingers fall out as
// the extremities of that cloud without needing to identify a joint. A moving
// anchor would make the residual meaningless.
enum { MP_ANCHOR_N = 12 };
static uint32_t g_mpAnchorIdx[MS_CLS_N][MP_ANCHOR_N];
static int      g_mpAnchorN[MS_CLS_N] = { 0, 0, 0, 0, 0 };

// ---- BUILD A2: PLACEMENT THROUGH THE MEASURED CHAIN --------------------------
//
// The shader capture ended the guessing. The vertex path is
//
//     p_local = (sum_i w_i M_i) * (v0 * MeshExtension + MeshOrigin)
//     p_cam   = LocalToWorld * p_local          (camera-relative world, uu)
//     clip    = ViewProjectionMatrix * p_cam
//
// so the palette's output is the component's LOCAL space and LocalToWorld -
// present in the constants, not conjecturable - takes it to a camera-relative
// world frame. To put the palm at the controller:
//
//     target_local = Rl^T * (d_cam - t),   T = target_local - q_local
//
// where [Rl | t] is LocalToWorld and d_cam is the controller's position
// relative to the camera in that frame. No calibrated offset, no neutral, and
// nothing left underneath: q_local is re-measured every frame from the game's
// own palette, so the animated baseline is subtracted rather than hoped away.
//
// d_cam is built from the camera's own basis, recovered from the
// ViewProjectionMatrix: rows 0 and 1 normalised give right and up (their
// lengths are the focal scales), and the w row is the forward axis and is
// exactly unit for a standard perspective. Checked against a captured packet
// before any of this was written: the left palm came out 15.3 uu left, 26.1 uu
// below and 44.1 uu in front of the camera, which is where a left hand is.
//
// WHAT IS STILL ASSUMED, and is the one number not measured here: the metres
// to unreal-units scale. [Hands] WorldScaleUU and [PosTrack] Scale disagree
// (100 vs 108) and neither has been validated for THIS conversion at two
// depths. A scale error shows up as the hand tracking with a gain error, not
// as drift, so it is distinguishable from a frame error in the next run.
static bool     g_mpWorld     = false;  // [Hands] PaletteWorld
// THE VIEW MODEL IS DEPTH-CRUSHED, measured not guessed: the hand draw runs
// with viewport MinZ 0.0 / MaxZ 0.001, the nearest thousandth of the depth
// buffer, while other draws in the same frame use the full 0..1. That is the
// standard first-person trick which guarantees the weapon and hands can never
// be occluded by world geometry - and it is exactly why correctly placed hands
// still composite on top of everything.
//
// Placement cannot fix it. The draw's depth range can, and we own the draw:
// restore the full range for the hand draws only, and put the game's own back
// immediately after. Whether that actually yields occlusion depends on the
// depth buffer still holding world depths at that point, which is NOT known -
// if the pass runs after a depth clear there is nothing to occlude against and
// the hands will look identical. That is the experiment.
//
// A render lever, so it ships default OFF with a live A/B.
static bool     g_mpDepth     = false;  // [Hands] PaletteDepthRange
static LONG     g_mpDepthUsed = 0;
static float    g_mpDepthSeen[2] = { 0.0f, 0.0f };   // the game's MinZ/MaxZ

// The stereo eye offset. A render lever, so it ships default OFF with an A/B -
// but it is the difference between hands that read as hand-sized and hands that
// read as giants, so the A/B is what proves the diagnosis rather than a
// preference.
static bool     g_mpEyeOffset = false;  // [Hands] PaletteEyeOffset
static LONG     g_mpEyeSeen[2] = { 0, 0 };   // draws attributed to pass 1 / pass 2
static LONG     g_mpEyeUnclassified = 0;
// THE EYE. Four mechanisms were tried and three failed; the reasons are in
// docs/dishonored/VR-33-HANDS-AND-WEAPONS.md and are worth not repeating:
//
//  1. The game-thread second-pass flag. UE3 queues render commands, so it reads
//     false by the time the draw runs on the render thread. The counter caught
//     it: pass2 0 draws against pass1's 92,800.
//  2. A learned MIDPOINT of LocalToWorld's right-axis projection. Not
//     head-invariant - turning the head drifts the projection for reasons
//     unrelated to the eye - so draws crossed a stale midpoint and took a full
//     IPD of error, which read as the hands teleporting.
//  3. The draw's ORDINAL within a frame. Sound in principle, but the sampling
//     was per HAND, so its "pair" was the left and right hand of ONE draw whose
//     constants are identical by construction. 24,376 zero comparisons that
//     measured nothing.
//
// What works, and what the corrected sampling measured: successive ORIGINAL
// draws differ on the right axis either by ~0 or by about one IPD - 6.76 uu
// against 6.31 predicted. The eye is constant within a Present and changes
// between them, so it is decided once per Present and the SIGN of the jump
// gives left or right absolutely. No vote, no ordinal, no hand side.
// The draw-to-draw comparison that found the eye, kept as the instrument that
// would find it again if the engine path changed.
static bool     g_mpEyeHunt = false;    // [Hands] PaletteEyeHunt

// The corrected sampling: one context per ORIGINAL draw, and counts that say
// what was sampled rather than only what was found.
static uint32_t g_mpDrawSeq = 0;
static LONG     g_mpDrawsEntered = 0, g_mpDrawsSampled = 0, g_mpDrawsRejected = 0;
static const char* g_mpDrawRejectWhy = "none";
static float    g_mpPrevVp[16], g_mpPrevL2w[16], g_mpPrevProj = 0.0f;
static uint32_t g_mpPrevDrawId = 0;
static bool     g_mpPrevDrawOk = false;
static float    g_mpCmpMaxVp = 0.0f, g_mpCmpMaxL2w = 0.0f, g_mpCmpMaxProj = 0.0f;
static LONG     g_mpCmpCount = 0;
// The eye, decided once per Present from the right-axis jump in LocalToWorld's
// translation. -1 left, +1 right, 0 unknown (no offset applied).
static int      g_mpEyeState = 0;
static uint32_t g_mpEyePresent = 0xFFFFFFFFu;
static bool     g_mpEyeHavePrev = false;
static float    g_mpEyePrevFirst = 0.0f;
static LONG     g_mpEyeToggles = 0, g_mpEyeSame = 0, g_mpEyeAmbiguous = 0;

// VR-16: THE EYE AUDIT. Read-only by default.
//
// The correction infers the eye per PRESENT from a jump in LocalToWorld. The
// re-entry method KNOWS it per DRAW, because it is the thing that moved the
// camera. These counters compare the two without changing what is applied, so
// one run says whether the inference is the flicker or is innocent.
//
// The tester's description is the hypothesis: both weapons jump together for a
// single frame, displaced right in the right eye and left in the left eye. A
// full inter-pupillary offset applied with the wrong sign looks exactly like
// that, and the inference has two documented ways to produce one - it holds the
// previous present's answer whenever the jump is too small to read, and it is
// keyed on a present count that runs at roughly twice the doubled-draw rate.
//
// WHAT WOULD FALSIFY IT: if agree/disagree comes back with no disagreements
// while the tester still sees the flicker, the eye inference is innocent and the
// displacement is coming from somewhere else - the correction's own camera, or
// the draw being routed to the wrong pass.
static LONG     g_mpEyeAgree    = 0;   // inference matched the pass that drew it
static LONG     g_mpEyeDisagree = 0;   // it did not - a full IPD in the wrong way
static LONG     g_mpEyeNoTruth  = 0;   // not a doubled tick, nothing to compare
static LONG     g_mpEyeSaid     = 0;   // disagreements printed in full
// FALSIFIED, AND THE MEASUREMENT IS THE POINT. Taking the eye from the drawing
// pass never executed once: 0 of 83,400 corrected draws found a doubled pass to
// read, because the passes and the draws are on DIFFERENT THREADS. The re-entry
// method re-draws the world on the game thread (76156 in the measured run) and
// the engine replays those commands, and therefore every palette draw, on the
// render thread (79476). There is no stack to look up.
//
// That also retracts the 39% disagreement this lever was turned on for. It came
// from reading a bare global across those two threads, which is not a
// measurement of anything. The lever stays, default OFF and inert, because the
// finding is worth keeping and the code is the proof of it.
static bool     g_mpEyeFromPass = false;  // [Hands] PaletteEyeFromPass

// WHAT TO DO WHEN THE JUMP IS TOO SMALL TO READ.
//
// The inference decides the eye from a sideways step of about one IPD between
// presents. When the step is too small to be sure, the current code KEEPS THE
// PREVIOUS PRESENT'S ANSWER - and that is the one answer guaranteed to be wrong,
// because the method presents the eyes alternately: pass 1 pushes -1, pass 2
// pushes +1, and the run measured 148 tags against 147 presents.
//
// So "I cannot tell" should alternate, not hold. Measured population: 991 such
// presents against 7350 clean ones in the flicker run, which is 12% - the right
// order for an intermittent one-frame jump on both weapons at once.
//
// It is NOT unconditionally right. A present that delivered no tag, or a held
// frame, really does repeat an eye, and there were a few hundred of those. This
// trades a certain error on the majority for a possible error on the minority,
// which is why it is a lever and why the run reports the count either way.
static bool     g_mpEyeAlternate = true;  // [Hands] PaletteEyeAlternate
static LONG     g_mpEyeFlipped   = 0;     // presents alternated instead of held

// THE FLICKER, COUNTED AS A WINDOW RATHER THAN A TOTAL.
//
// A lifetime counter cannot be matched against "it flickered while I faced the
// door and stopped when I turned away". These record the START and END of every
// stretch where the eye could not be read cleanly, with the head yaw on the line,
// so a spoken report lands on a timestamp without the tester timing anything.
static bool     g_mpFlickOn      = false;
static double   g_mpFlickStartMs = 0.0;
static LONG     g_mpFlickCount   = 0;
static double   g_mpFlickLastMs  = 0.0;
static float    g_mpFlickYaw0    = 0.0f;
static int      g_mpFlickWindows = 0;
static LONG     g_mpTickRan   = 0;      // times the pose tick has run at all
static LONG     g_mpWorldOk   = 0;      // draws placed through the measured chain
static LONG     g_mpWorldRefused = 0;   // and draws that refused, with a reason
static const char* g_mpWorldWhy = "not run yet";
static float    g_mpLastTargetLocal[2][3];
static float    g_mpLastPCam[2][3];

static float    g_mpResid[2][3];        // target - corrected anchor, MEASURED
static LONG     g_mpResidTick[2] = { 0, 0 };  // PER HAND - a shared counter only ever
                                              // sampled the right hand in the normal
                                              // left-then-right draw order
static bool     g_mpResidOk[2]  = { false, false };
static uint32_t g_mpResidGen[2] = { 0, 0 };
static uint32_t g_mpCacheGen  = 0;      // bumped on every accepted palette write

// THE PALETTE IS AN INTERVAL WITH PER-REGISTER VALIDITY, not a length.
//
// Tracking only a count could not express what the engine actually does. An
// update starting INSIDE the block (c9 x3) failed the outer test entirely; a
// wide block covering c6 could top up an existing cache but never bootstrap an
// empty one; a short c6 x4 left the previous claimed length standing; and an
// over-long upload was copied whole, after which MpBuild happily treated every
// following triplet as another bone.
//
// So: a fixed register interval [MP_PAL_LO, MP_PAL_LO + 3*bones), a valid flag
// per register, and every upload contributing its INTERSECTION with that
// interval. The palette is usable only when every register in it is valid,
// which is a statement about state rather than about the last write's size.
enum { MP_PAL_LO = 6, MP_PAL_MAX = 256 };
static uint8_t  g_mpValid[MP_PAL_MAX];  // per-register, indexed from MP_PAL_LO
static UINT     g_mpPalN      = 0;      // registers in the interval (3 * bones)
static UINT     g_mpValidN    = 0;      // how many of them are currently valid
// How far the anchor's skin weights may stray from summing to 1 before the
// anchor is refused outright. The shader's own weight arithmetic has NOT been
// read yet; until it has, a sum far from 1 means the CPU point and the point
// the GPU renders are different points, and the placement would be built on
// the difference.
static float    g_mpWsumTol   = 0.02f;  // [Hands] PaletteWeightTol
static float    g_mpWsumWorst = 0.0f;   // the sum that last caused a refusal

// ---------------------------------------------------------------------------
// VR-33 ROTATION AND GRIP
// ---------------------------------------------------------------------------
//
// THE COHERENT POSE SNAPSHOT.
//
// The pose tick runs on the present thread (present_tick.cpp -> DcTick ->
// MsTick) and the draws run on whichever thread the renderer draws on. Writing
// a group of floats and then setting a validity flag is NOT publication: the
// flag is already true from the previous sample, so a reader can combine new
// rows with old ones and never know. The whole snapshot is therefore copied
// under a lock, in one piece, and each ORIGINAL DRAW takes one copy that its
// hands - and later its weapon - all share, so two draws of one view can never
// disagree about where the controller was.
//
// The lock is uncontended in the normal case (about five original draws per
// present), and `g_mpLaneSame` records whether the two lanes turned out to be
// one thread after all, so the contract is measured rather than assumed.
struct MpPoseSnap {
    dvr::hf::Mat3 inHead[2];     // F * transpose(R_head) * R_ctl, per hand
    float         ruf[2][3];     // F * transpose(R_head) * (p_ctl - p_head), metres
    bool          ok[2];         // per hand: head AND that hand tracked
    bool          headOk;
    uint32_t      gen;           // bumped on every publication
};
static MpPoseSnap        g_mpPosePub;          // the published snapshot
static CRITICAL_SECTION  g_mpPoseCs;
static bool              g_mpPoseCsOk = false;
static uint32_t          g_mpPoseGen  = 0;
static DWORD             g_mpTickTid  = 0;     // the lane that publishes
static DWORD             g_mpDrawTid  = 0;     // the lane that draws
static int               g_mpLaneSame = -1;    // -1 unknown, 0 different, 1 same
static LONG              g_mpPoseStale = 0;    // draws that took a snapshot older
                                               // than the previous draw's

// THE SOURCE FRAME. Orientation comes from ONE palette slot - the slot
// carrying the greatest total weight across the anchor patch - because a
// weighted BLEND of rotations is not a rotation (measured on the 28 saved
// packets: det 0.970, Gram off by 2.6 percent), while a single slot is a
// rotation times a uniform scale to six decimals.
//
// It is a PALETTE SLOT, not a named joint. Dominant weight does not prove it
// follows the palm rather than a finger or the forearm, and a skinning matrix
// can carry an inverse-bind rotation, so its axes are not anatomical axes. Any
// constant bind orientation is absorbed into the grip transform G. What must be
// established by measurement is only that the slot follows the PALM rigidly;
// `ms/palette/frame:` reports what is needed to judge that.
//
// Frozen per class AND per source generation: a rebuild, a device reset or a
// palette remap invalidates it, because an index still inside the palette does
// not prove it still names the same transform.
static int      g_mpDomSlot[MS_CLS_N] = { -1, -1, -1, -1, -1 };
static float    g_mpDomWeight[MS_CLS_N] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static uint32_t g_mpSrcGen   = 0;       // bumped by every split rebuild/reset
// THE FRAME-CONVENTION FINGERPRINT that a grip transform was solved against.
// A generation number alone is too blunt: a rebuild that re-derives the SAME
// slot and the SAME anchor has not changed what G means, and invalidating
// there costs a headset run for nothing. A generation number is also not
// enough on its own, because an index still inside the palette does not
// prove it names the same transform. So record what was actually used - the
// slot, the anchor's size and its first identity - and invalidate only when
// one of them differs. That is a check, not a hope.
struct MpFrameId { int slot; int anchorN; uint32_t anchor0; };
static MpFrameId g_mpGripId[2] = { { -1, 0, 0 }, { -1, 0, 0 } };
static bool      g_mpGripFromIni[2] = { true, true };

// The last measured source frame, per hand, for the instrument.
static dvr::hf::Mat3 g_mpSrcR[2];
static bool     g_mpSrcOk[2]   = { false, false };
static float    g_mpSrcScale[2] = { 0.0f, 0.0f };   // the slot's own uniform scale
static float    g_mpSrcAniso[2] = { 0.0f, 0.0f };
static float    g_mpSrcOrtho[2] = { 0.0f, 0.0f };
// Angles for the instrument: how far each measured frame has moved from the
// first one seen, so "does it track the palm?" is a number the tester can be
// asked about rather than an impression.
static dvr::hf::Mat3 g_mpSrcRef[2];
static bool     g_mpSrcRefOk[2] = { false, false };
static float    g_mpSrcMoved[2] = { 0.0f, 0.0f };   // deg from the reference
static float    g_mpSrcMovedMax[2] = { 0.0f, 0.0f };
static float    g_mpCtlMoved[2] = { 0.0f, 0.0f };   // the controller, same measure
static float    g_mpCtlMovedMax[2] = { 0.0f, 0.0f };
static dvr::hf::Mat3 g_mpCtlRef[2];
static bool     g_mpCtlRefOk[2] = { false, false };
static float    g_mpOutMoved[2] = { 0.0f, 0.0f };   // the APPLIED palm, when rotating
static bool     g_mpOutOk[2] = { false, false };

// THE LEVER AND THE GRIP TRANSFORM.
static bool     g_mpRotate   = false;   // [Hands] PaletteRotate

// THE CALIBRATION RECORD, versioned, with the reflection stored EXPLICITLY.
//
// G inherits the parity of the pose mapping and is improper on this game, so
// three Euler angles cannot carry it: a product of three proper rotations is
// always proper. The record therefore stores a parity sign and a PROPER
// rotation, and `G = P * R_saved` rebuilds it exactly because P*P = I.
//
// GripVersion is what tells an old three-angle record apart from a complete
// one. A record without it is NOT read as a calibration - the angles in it
// were written by the build that dropped the reflection, and loading them
// would restore a mirrored hand while looking like a valid calibration.
enum { MP_GRIP_VERSION = 2 };
static int      g_mpGripVer[2]    = { 0, 0 };   // 0 = no usable record
static int      g_mpGripParity[2] = { 0, 0 };   // +1/-1, stored beside it
static bool     g_mpGripHave[2]   = { false, false };
static volatile LONG g_mpGripSaveReq = 0;       // draw -> the present thread
static float    g_mpGripSaveDeg[2][3];          // the PROPER rotation, degrees
static int      g_mpGripSaveParity[2] = { 0, 0 };

// THE HAND TRIM: a small rigid nudge expressed IN THE CALIBRATED PALM FRAME,
// so "5 mm towards the fingers" stays towards the fingers however the wrist is
// held.
//
// PER HAND since the numpad scheme landed. It used to be one shared trim on
// the reasoning that each palm is already calibrated to its own G, so palm
// space means the same thing on both sides. The headset disagreed: the two
// grips are solved from two separate poses and carry their own residual, so
// the leftover error is not common and one trim cannot cancel both.
//
// It moves the hand AND anything held in it by the same common transform, so a
// later hand adjustment never invalidates a weapon profile. A weapon-only trim
// is separate and belongs to the weapon stage.
// THE MODEL SCALE. One uniform factor on the hand AND anything held in it,
// applied about the target palm so the grip stays where tracking put it and
// the model shrinks around the controller rather than sliding off it.
//
// This is NOT the world scale. PageUp/PageDown drive g_posScaleUU, which sets
// the stereo separation - a property of the projection, so it changes the
// apparent size of everything in the frame at once and can never make the
// hands smaller relative to the room. Only the geometry can do that, which is
// what this is.
//
// It is also not the hand TRAVEL (WorldScaleUU / PaletteDriveGain): those set
// how far the hand moves per metre of controller, and the tester reports that
// as already correct.
//
// One lever for both hands and both weapons on purpose. The weapon path takes
// the same palm target and the same factor, so the two cannot drift apart the
// way two independent knobs would.
static float    g_mpModelScale = 1.0f;      // [Hands] ModelScale

static float    g_mpTrimT[2][3] = { { 0.0f, 0.0f, 0.0f },   // metres, palm frame
                                    { 0.0f, 0.0f, 0.0f } };
static float    g_mpTrimR[2][3] = { { 0.0f, 0.0f, 0.0f },   // degrees, XYZ
                                    { 0.0f, 0.0f, 0.0f } };

// ===========================================================================
// THE NUMPAD ADJUST (VR-33). Adopted key for key from the maintainer's own
// BioShock Remastered VR mod, because these are the keys the tester already
// has in his fingers and a headset is the worst possible place to learn a new
// binding. Four modes cycled with Numpad 9:
//
//   0  LEFT hand POSITION      1  LEFT hand ROTATION
//   2  RIGHT hand POSITION     3  RIGHT hand ROTATION
//
//   Numpad 8 / 2   along the fingers, forward / back  |  pitch
//   Numpad 6 / 4   across the palm, right / left      |  yaw
//   Numpad 0 / 5   out of the palm, up / down         |  roll
//   Numpad 7       cycles the step size
//   Numpad 9       cycles the mode, and NAMES it in the log
//
// THE CLAIM. Every one of those keys is already read by another block in
// hotkeys.cpp - the material cycler (1/2/3), the draw census and its
// eighth-cutter (4..9) and the mesh split (0). One key firing two features has
// cost this project a session already (7099c3b0, 0e1ccbb0), so this is not
// another reader added alongside them: with [Hands] Adjust=1 the adjust block
// takes those keys and the other blocks are suppressed FOR THOSE KEYS ONLY,
// with one log line at startup saying the numpad is claimed and by what.
// The mesh split's mode cycle moves from Numpad 0 to Numpad 1 to keep it
// reachable.
//
// The rotation steps go down to a tenth of a degree because a hand that is
// two degrees out is visible and one degree of correction overshoots it.
static bool     g_mpAdjOn    = true;    // [Hands] Adjust
static int      g_mpAdjMode  = 0;       // 0 Lpos, 1 Lrot, 2 Rpos, 3 Rrot
static int      g_mpAdjStepT = 1;       // index into kMpAdjStepT
static int      g_mpAdjStepR = 3;       // index into kMpAdjStepR
// bits 0..5 = the six directional presses, 6 = step cycle, 7 = mode cycle
static volatile LONG g_mpAdjReq = 0;
static const float kMpAdjStepT[3] = { 0.005f, 0.02f, 0.05f };          // metres
static const float kMpAdjStepR[7] = { 0.1f, 0.25f, 0.5f, 1.0f,
                                      2.0f, 5.0f, 15.0f };             // degrees

// The target palm each hand was placed at, in the draw's camera-relative
// frame. Published so the weapon stage can consume the SAME target rather than
// a hand's delta - and so a mismatch between the two is visible.
static dvr::hf::Xform g_mpPalmTarget[2];
static bool     g_mpPalmTargetOk[2] = { false, false };
static float    g_mpGripDeg[2][3] = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
static dvr::hf::Mat3 g_mpGrip[2];       // built from g_mpGripDeg at config time
static volatile LONG g_mpGripCapReq = 0;   // hotkey -> the next qualified draw
static LONG     g_mpGripCapDone = 0;
static LONG     g_mpRotOk = 0, g_mpRotRefused = 0;
static const char* g_mpRotWhy = "not run yet";
// Tolerances. Deliberately loose enough for a real palette and tight enough to
// reject a frame that is not a rotation: the measured slot residuals are around
// 1e-6, three orders of magnitude inside these.
static float    g_mpFrameTolOrtho = 0.02f;   // [Hands] PaletteFrameTol
static float    g_mpFrameTolAniso = 0.02f;
static int      g_mpBasisImproper = 0;  // draws refused: the basis was not orthonormal
static int      g_mpParitySeen    = 0;  // +1/-1 once measured, 0 until then
static int      g_mpSelfTestFailed = -1;   // -1 not run, else the failure count

// The derived wrist: distance from the hand bone's centroid, per side, and the
// tester's live multiplier on it. 1.0 means "exactly where the bone spacing
// says the wrist is"; the knob exists because a derived number still has to
// survive a human looking at it.
static float    g_msWristR[3] = { 0.0f, 0.0f, 0.0f };   // [1]=side A, [2]=side B
// 0.70 is the tester's measured value from the 2026-09-06 headset run, not a
// guess: it is where the cut was left after walking it, and it is what the
// plane's starting position is seeded from.
static float    g_msWristScale[3] = { 0.70f, 0.70f, 0.70f };
static int      g_msHandBone[3] = { -1, -1, -1 };
static int      g_msSideAxis = -1;             // the axis that separated the arms
static float    g_msSideSign[3] = { 0.0f, 0.0f, 0.0f };  // mean coord per side

// Our index buffer. D3DPOOL_MANAGED on purpose: a default-pool object still
// held at Reset makes the game's Reset fail forever (the hkReset LAW), and a
// managed buffer survives a Reset with no release/recreate dance at all.
static IDirect3DIndexBuffer9* g_msIb = NULL;
static uint32_t g_msIbFmt = 0;                 // D3DFMT_INDEX16 / INDEX32
static IDirect3DDevice9* g_msIbDev = NULL;     // identity only, never released by us

static volatile int  g_msMode = MS_MODE_HANDS; // [Hands] SplitMode - ON by default
static volatile int  g_msModeReq = 0;          // +1 next mode, -1 previous
static volatile int  g_msWristReq = 0;         // +1 wider hand, -1 narrower
static volatile int  g_msSideReq = 0;          // cycle which side the knob moves
static volatile int  g_msRebuildReq = 0;       // re-read the buffers from scratch
static volatile int  g_msReclassReq = 0;       // reclassify the copy we already hold
static volatile int  g_msRederiveReq = 0;      // redo the axis and the plane, no re-read
// Which direction the ring is square to: 0 = the forearm BONE (hand bone minus
// the bone it hangs off), 1 = the longest direction of the arm's triangles.
// They can disagree, and a ring square to the wrong one is slanted across the
// forearm - which is what "it takes too much on the other side" looks like.
static volatile int  g_msAxisMode = 0;
static volatile int  g_msKnobSide = 0;         // 0 = both, 1 = side A, 2 = side B
static bool     g_msOn = true;                 // [Hands] ArmSplit
static volatile int  g_msReady = 0;            // a validated split exists
static volatile int  g_msRefused = 0;          // a read was tried and rejected
static volatile uint32_t g_msDraws = 0;        // draws served from our buffer
static volatile uint32_t g_msFallback = 0;     // draws that fell through to the game's

// AUTO-ARM. The mesh lock was armed by hand from a censused row, so a level
// load that recreates the buffers left it pointing at freed pointers and
// silently drawing everything - a listed weakness of the instrument, and the
// reason this ships with an identity instead of a key press. The signature is
// the mesh's own measured shape; anything else is not this mesh and is left
// alone. A run that never matches says so in the log.
static bool     g_msAuto = true;               // [Hands] ArmSplitAuto
static uint32_t g_msWantPrims = 4448;          // [Hands] ArmMeshPrims (measured)
static uint32_t g_msWantVerts = 2771;          // [Hands] ArmMeshVerts (measured)
static volatile uint32_t g_msLockFrame = 0;    // last frame the lock matched a draw
static volatile int      g_msAutoArmed = 0;    // this lock came from the signature
static double   g_msNextReport = 0.0;
static bool g_mpEyeFromMeasured = true;   // VR-69: [Hands] PaletteEyeFromMeasured
static LONG g_msMeasAgree = 0, g_msMeasDisagree = 0, g_msMeasNone = 0;   // VR-69 audit
static int g_mpEyeMeasSign = -1;   // VR-69: [Hands] PaletteEyeMeasSign - the method tags pass 1/2, the palette left/right
static LONG g_msMeasAgreeEye[2] = {0,0}, g_msMeasDisagreeEye[2] = {0,0}, g_msMeasNoneEye[2] = {0,0};   // VR-69: left/right
static LONG g_msPubStep1 = 0, g_msPubStepOther = 0, g_msPubRepeat = 0;   // VR-69 build 1: publication staleness
// VR-69: [Hands] PaletteEyeFromMatrix - take the eye offset from the draw's own
// ViewProjection instead of deciding which eye the draw is. This is the root of
// the weapon flickers: the decision is what has been wrong, in three different
// ways, and the decision does not need to exist.
// VR-69 phase A. The BEHAVIOUR lever is gone - there is nothing for it to do
// until Phase B establishes the head-centre reference, and leaving a dead lever
// that looks live is how the next reader ships it. What remains is a DIAGNOSTIC
// that records the recovered camera and cannot touch placement.
static bool g_mpEyeMatrixDiag = true;
static LONG g_mpEyeMeasUsed = 0, g_mpEyeMeasAgree = 0, g_mpEyeMeasDiffer = 0, g_mpEyeMeasRefused = 0;
// VR-69 phase B: where the eye translation lives. Counted over pairs of draws of
// the SAME object, one per eye. All four outcomes are answers.
static LONG g_mpPbPairs = 0, g_mpPbVpOnly = 0, g_mpPbL2wOnly = 0, g_mpPbBoth = 0, g_mpPbNeither = 0;
// VR-69: the validated eye-pair observer. It pairs on g_mpDrawSeq, which ALREADY
// EXISTED and is already stamped once per original draw at the end of
// MpAcquireCtx - the identity the broken probe needed was in the context the
// whole time and it used a quantised position instead.
static dvr::eyeobs::Observer g_mpEyeObs;
