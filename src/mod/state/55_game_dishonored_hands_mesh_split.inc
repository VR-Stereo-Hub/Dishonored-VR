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
