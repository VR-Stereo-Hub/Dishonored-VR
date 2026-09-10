// State chunk 18 for core/gfx/hand_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static int g_hmTexSlot = 0;   // (was chunk 17) the skin slot the hand pass binds


// Load an extracted mesh if one is present. Meshes go in a "vrhands" folder
// beside the exe, named for the model: sword.obj, crossbow.obj, pistol.obj,
// grenade.obj, razor.obj, heart.obj, hand.obj. Anything missing falls back to
// the built-in shape, so this can be filled in one weapon at a time.
//
// Deliberately loads at RUNTIME from the user's own files rather than shipping
// any art: extracting from a copy you own is one thing, redistributing Arkane's
// meshes inside a mod is quite another.
// Skins load through WIC, which is part of Windows - so PNG, JPG, BMP and TIFF
// all work with no extra dependency and no bundled decoder. <name>.png beside
// <name>.obj is picked up automatically; a 1x1 white fallback keeps untextured
// models rendering as plain shaded colour.
static ID3D11ShaderResourceView* g_hmSkin[8] = { NULL };
static char g_hmSkinName[8][64];
static char g_hmSkinPath[8][MAX_PATH];
static FILETIME g_hmSkinTime[8];
static int  g_hmSkinN = 1;                 // slot 0 is reserved for plain white

// Draw both hands into one eye. Room -> head -> eye -> clip on the CPU, using
// the same asymmetric-frustum composition the eye quads used.
//
// The painter's sort this comment used to describe is GONE - the pass batches
// by skin and leans on a depth buffer instead (one Draw per texture beats one
// per triangle run). g_hmSortKey and HmSortCmp survive only as the sort's
// carcass; the live requirement is that a DEPTH-STENCIL VIEW is bound when
// HmRenderEye runs, or the back of a hand paints over its front.
static const float* g_hmSortKey = NULL;

// 41.2 (VR-31): the hand pass's own depth buffer. The per-eye render targets
// that used to carry one went with the side-by-side pipeline (cc2fa936), so
// the pass now owns a D32 buffer sized to the stereo method's output texture -
// ONE, shared by both eyes, cleared at the head of every draw. Both eyes never
// share a frame: a present carries one eye's picture and one eye's hands.
static ID3D11Texture2D*        g_hmDepthTex = NULL;
static ID3D11DepthStencilView* g_hmDSV = NULL;
static uint32_t g_hmDepthW = 0, g_hmDepthH = 0;

// The beat's population. A hand pass that draws nothing looks exactly like a
// hand pass that is not being called, so the counters separate the reasons and
// the beat line prints the unwelcome ones by name.
static uint32_t g_hmCalls = 0;       // the seam called us
static uint32_t g_hmDraws = 0;       // we reached HmRenderEye
static uint32_t g_hmTris  = 0;       // triangles SUBMITTED to the GPU
static int      g_hmLastWhy = 0;     // the last HmRenderEye return code
static double   g_hmNextBeat = 0.0;

// 41.2 (VR-31, second pass): SUBMITTED is not VISIBLE, and the first version of
// this beat could not tell them apart - it reported 252 triangles a present
// while the screen stayed empty. Three distinct ways a triangle can be counted
// and still produce no pixels, each now counted on its own:
//
//   non-finite - a NaN coordinate. It does NOT trip the near-plane test,
//                because `NaN > -0.03f` is FALSE, so it used to be submitted.
//   degenerate - zero projected area. Perfectly ordinary bounding box, no
//                pixels.
//   off-screen - NDC bounds outside the viewport. Rasterised into nothing.
//
// The NDC bounding box is the number that says WHERE they went, so "the hands
// are invisible" becomes an axis and a magnitude instead of a hypothesis.
static uint32_t g_hmNonFinite = 0;
static uint32_t g_hmNearRej   = 0;
static uint32_t g_hmDegenerate = 0;
static uint32_t g_hmOnScreen  = 0;
static float    g_hmNdcMin[2] = { 1e30f, 1e30f };
static float    g_hmNdcMax[2] = { -1e30f, -1e30f };
static float    g_hmDistMin = 1e30f, g_hmDistMax = -1e30f;   // metres from the eye

// The near plane, and the depth mapping that replaces the constant 0.5.
static const float kHmNearM = 0.03f;
static const float kHmFarM  = 20.0f;

static inline bool HmFinite(float v)
{
    return v == v && v > -3.0e38f && v < 3.0e38f;   // v != v catches NaN
}

// 41.2: the calibration triangle. One fixed-NDC triangle through the SAME
// shader, vertex buffer, states and target as the hands, so "the draw path
// works" and "the geometry is right" stop being one question. Default OFF; it
// is the fallback if the hands are still invisible once the geometry is sound.
static bool g_hmCalib = false;       // [VRHands] CalibTriangle
