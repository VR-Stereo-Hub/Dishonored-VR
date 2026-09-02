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
// the same asymmetric-frustum composition as the eye quads, then a painter's
// sort so no depth buffer is needed.
static const float* g_hmSortKey = NULL;
