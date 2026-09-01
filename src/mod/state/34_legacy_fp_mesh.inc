// State chunk 34 for legacy/fp_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// WEAPON-MESH EXPEDITION (Stage 8.x) - move the first-person weapon with the
// hand. Groundwork: the first-person weapon/arms in UE3 are drawn with their
// OWN view-projection (a separate "weapon FOV" so the gun never clips walls),
// distinct from the world VP. Both land at register c0 (the non-affine matrix
// stereo already shears). If we can pick the weapon's VP out of the c0 stream,
// we can post-multiply a hand rotation into ONLY those draws → the weapon
// visibly points where the controller points.
//
// Stage 8.0 (this build) = DIAGNOSTIC ONLY: cluster the distinct non-affine c0
// matrices seen per frame and log them, so we can identify the weapon VP (a
// minority cluster with a different FOV, i.e. different m[0]/m[5]) vs the world
// VP (dominant). Nothing is modified. Gated by [Weapon] Diag=1.
// ============================================================================
struct VpCluster { float rep[16]; uint32_t hits; uint32_t lastFrame; };
static VpCluster g_vpClusters[12];
static int       g_vpClusterN = 0;

// ============================================================================
// Stage 10 - DECOUPLING THE FP MESH FROM THE HEAD (per-draw LocalToWorld)
//
// The 8.x hunt failed because I looked for the weapon's own VIEW-PROJECTION;
// Dishonored shares one VP for everything. But the VS-constant scan showed a
// second matrix stream: register c6 receives an AFFINE row-major matrix ~730
// times per frame - that is the per-draw LocalToWorld (row 3 = the object's
// world position). Every drawn object announces where it is, per draw.
//
// The first-person arms/weapon are the objects whose world position sits a
// few dozen units from the camera, every single frame. So we don't need the
// engine to separate them for us: we identify them geometrically at draw time
// and rotate their LocalToWorld about the camera by the hand delta. That is
// the decoupling - the mesh stops inheriting only the head and follows the
// controller, without touching the camera or the engine.
// ============================================================================
static uint32_t g_waFrame   = 0xffffffff;
static bool     g_waHaveCam = false, g_waOk = false;
static float    g_waCamPos[3] = {0,0,0};
static float    g_waA[16];                 // T(-cam) * Rdelta * T(cam)
static float    g_waD[9];                  // Rdelta alone (row-vector conv)
static float    g_waL[9];                  // hand rotation in MESH-LOCAL axes
static bool     g_waLOk = false;

// Per-hand transforms, rebuilt each frame: rotation in mesh-local axes plus a
// 6DoF translation (hand movement relative to the head, in game units). The
// translation is what makes it feel like VR instead of a turret.
static float    g_handL[2][9];
static float    g_handPos[2][3];
static bool     g_handOk[2] = {false,false};
static float    g_restPos[2][3];
static bool     g_haveRest[2] = {false,false};

// What each mesh block should do. Blocks are addressed by catalogue index.
enum { BM_OFF = 0, BM_LEFT = 1, BM_RIGHT = 2, BM_HIDE = 3 };
// (reg,count) alone is too coarse a name for a mesh: ANY skeleton with the
// same bone count shares it, which is why random world objects started
// turning with you. So we also fingerprint the skeleton's own proportions - 
// the summed bone offsets - and only touch meshes that match the one you
// actually assigned.
struct BlockState { uint8_t mode; uint64_t hideMask; float fp; };
static BlockState g_blk[32];
static int      g_selBone = -1;            // bone picked inside the target block
static float    g_blkLastFp[32] = {0};     // most recent fingerprint per signature

// Assignments persist in the ini, keyed by the block's signature (register +
// size) rather than its discovery order - so a layout you dial in once comes
// back every launch instead of being re-hunted with the keys.
struct CatRec { uint32_t reg, count; };
static CatRec   g_cat[32];
static int      g_catN = 0;

struct SavedBlk { uint32_t reg, count; uint8_t mode; uint64_t hideMask; float fp; };
static SavedBlk g_saved[32];
static int      g_savedN = 0;
// Persistent catalogue of upload shapes, so a block can be targeted by index.
static int      g_wpnTarget = -1;          // -1 = every block
static uint32_t g_waHits = 0, g_waAffine = 0;

struct NearRec { float d; float t[3]; uint32_t reg; uint32_t n; };
static NearRec  g_near[12];
static int      g_nearN = 0;

// ---------------------------------------------------------------------------
// Skinned meshes don't upload a LocalToWorld - they upload a BONE PALETTE: a
// contiguous block of 3-register (3x4, transposed) bone matrices. The arms and
// weapon are skeletal, which is exactly why the c6 rigid-mesh stream never
// showed anything near the camera. So: catalogue every large upload block by
// (register, size), and read the first matrix's translation under BOTH
// conventions, so we can see which stream carries player-attached bones.
// ---------------------------------------------------------------------------
struct ShapeRec {
    uint32_t reg, count, n;
    float dRow, tRow[3];      // row-major 4x4  -> translation = row 3
    float dCol, tCol[3];      // transposed 3x4 -> translation = 4th column
    float magCol;             // |t| for the col form (local-space bones = small)
};
static ShapeRec g_shapes[28];
static int      g_shapeN = 0;

// ============================================================================
// Stage 20 - ProcessEvent: talking to the engine in its own language
//
// Everything before this poked raw memory offsets and hoped. UE3 has a proper
// front door and we never used it: every gameplay function is a named UFunction
// object, and they ALL flow through one virtual method - 
//     UObject::ProcessEvent(UFunction* Function, void* Parms, void* Result)
// Hook that and we get a callback on the GAME THREAD, at the right moment in
// the frame, with a human-readable name for what is happening. Camera updates,
// weapon fire, movement - all named, all interceptable.
//
// Located by static analysis of Dishonored.exe rather than guesswork: the only
// UObject virtual present in all 2396 class vtables that takes three stack
// arguments (ret 0xC) with a 1.6KB body. That is ProcessEvent.
//
// This build only LISTENS: it records the distinct function names that pass
// through and logs them once. No parameters are modified.
// ============================================================================
// 0x0048a0e0 was the wrong pick - it has the same shape but no UFunction in
// any argument. The real one reads UFunction::FunctionFlags at +0x80 from its
// first argument, which is the fingerprint that settles it.
static const uintptr_t kProcessEvent = 0x00470640;
