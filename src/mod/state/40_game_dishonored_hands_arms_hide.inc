// State chunk 40 for game/dishonored/hands/arms_hide - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 30.14: the +0x288 byte poke was wrong - that array turned out to be some
// per-bone animation control (arms froze to the view and rode the head).
// Do it the engine's way instead: UE3 ships native HideBoneByName /
// UnHideBoneByName on skeletal components. Call them through the engine's
// own ProcessEvent - the engine allocates BoneVisibilityStates itself, so
// the "never point engine arrays at our memory" trap never comes up.
typedef void (__fastcall *PeCall_t)(void* self, void* edxDummy,
                                    void* fn, void* parms, void* result);

// ---- 41.2 (VR-31) route (b) step 1: the DRAW census -----------------------
// Ties each c6 bone-palette upload to the DrawIndexedPrimitive that consumes
// it, so "are the arms and hands separate geometry" stops depending on upload
// SIZE - two chunks can share a size, and several draws can reuse one upload.
// See game/dishonored/hands/draw_census.cpp for the reasoning and the lane and
// D3D-reference rules it follows.
#define DC_MAX 96
// How many draws after a c6 write still count as fed by it. Constants persist
// until overwritten, so this bounds the reuse window instead of assuming one
// draw per upload (which silently hid every reusing draw) or none (which would
// swallow the whole world).
#define DC_REUSE_WINDOW 12
struct DcDraw {
    uint32_t bones;        // the palette that fed it (registers / 3)
    void*    vb;           // stream-0 vertex buffer - IDENTITY ONLY, never read
    uint32_t stride;
    uint32_t streamOff;    // stream byte offset - was DISCARDED, see below
    void*    ib;           // index buffer            - identity only
    void*    decl;         // vertex declaration      - identity only
    void*    vs;           // vertex shader           - identity only
    int32_t  baseVertex;   // BaseVertexIndex - was IGNORED, see below
    uint32_t primType;
    uint32_t minIndex, numVerts, startIndex, prims;
    // Does the DECLARATION actually carry skin influences? bones >= 2 is a
    // plausibility filter, not proof of skinning: a non-palette c6 write of the
    // right length would pass it. Read once per row from the declaration.
    int8_t   hasBlendIdx, hasBlendWt, declRead;
    uint32_t ordAfterUpload;  // draws between the c6 write and this one (not identity)
    uint32_t hits, frames, lastFrame;
    uint32_t hitsPrev;     // hits in the PREVIOUS window - "is this row live?"
};
static DcDraw   g_dcDraw[DC_MAX];
static volatile int g_dcN = 0;           // published only after a row is filled
static bool     g_dcOn = true;           // [Hands] DrawCensus - ON for the test sessions
static volatile uint32_t g_dcPendingBones = 0;
static volatile int      g_dcPendingSerial = 0;
static volatile int      g_dcHideAt = -1;    // the row the cycler is hiding
static volatile uint32_t g_dcHidden = 0;     // draws dropped since the press
static volatile int      g_dcCycleReq = 0;
static volatile uint32_t g_dcSinceUpload = DC_REUSE_WINDOW;   // starts closed
// MESH LOCK. Hiding the three palette-fed passes over the arm mesh left a faint
// arm still on screen, so something draws it that the palette-gated census
// never saw. Identity by BUFFER PAIR answers that without depending on the
// gate at all: any draw bound to this vertex+index buffer IS this mesh,
// whatever the constants did and whichever draw entry point it used.
// g_dcUnseen is the evidence - drops that the census had no row for.
static void* volatile g_dcHideVb = NULL;
static void* volatile g_dcHideIb = NULL;
static volatile uint32_t g_dcDropIdx = 0;    // indexed draws dropped
static volatile uint32_t g_dcDropPrim = 0;   // NON-indexed draws dropped
static volatile uint32_t g_dcUnseen = 0;     // dropped while no palette was current
static volatile uint32_t g_dcSkippedFlat = 0; // bones==1 draws kept out of the table
// Did THIS code arm the lock, or did the tester? An automatic lock fails soft
// (a mesh it cannot split is drawn exactly as the game asked) and releases
// itself when it goes stale; a hand-armed one drops what it is told to drop.
static volatile int      g_dcAutoArmed = 0;
// SLICE MODE. The mesh is one triangle list, so a sub-range of it can be drawn
// by calling the original with a shifted startIndex and a smaller primCount -
// no index data has to be read, captured or replaced to find out WHICH
// triangles are the arms. Hide one slice at a time and the mesh says where its
// parts are. DC_SLICES presses cover the whole thing.
// 32, not 8. At eighths the tester got close twice - "most of the left arm
// except a few pieces of sleeve", and "the right arm past halfway up the
// forearm" - but never both at once, because THE TWO ARMS SIT IN DIFFERENT,
// NON-ALIGNED REGIONS of the one triangle list. A single slice can never be
// right for both, however it is placed.
//
// So: finer slices, and a MASK instead of a single hidden slice. Walk the
// cursor, mark the slices that are arm, and the marks accumulate. The result is
// an arbitrary set of ranges - which is what a real arm/hand split needs, since
// left arm, right arm and stray sleeve pieces are not one contiguous run.
#define DC_SLICES 32
static volatile int g_dcHideSlice = -1;        // cursor; -1 = whole mesh
// The tester's mask from the 2026-09-06 session, 0x3001D237, kept as the record
// of where the arms sit in the triangle order. NOT the default - it was tuned
// for the LEFT hand and takes too much of the right, and the slices are
// PERCENTAGES of one mesh's primCount, so it is only meaningful for the asset
// it was measured on. Restore it with `dc mask 3001D237` rather than retyping
// twelve key presses.
#define DC_MASK_S19 0x3001D237u
static volatile uint32_t g_dcSliceMask = 0;    // slices marked hidden for good
static volatile int g_dcSliceReq = 0;
static double   g_dcNextReport = 0.0;
