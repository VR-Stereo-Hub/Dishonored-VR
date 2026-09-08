// State chunk 57b for game/dishonored/hands/weapon_attach - included by
// src/mod/dishonoredvr.cpp.
//
// ===========================================================================
// VR-33 W2/W3: PUT THE WEAPON IN THE TRACKED HAND
//
// The hands are placed by applying ONE rigid transform D to every matrix in
// the draw's bone palette: the weighted skinning blend commutes with a common
// rigid transform, so the engine goes on animating and D only moves the
// result. The weapon is the same operation on a different mesh. Nothing new is
// derived here - the target palm, the draw conversion and the delta are the
// helpers the hand path already uses and the self-test already pins.
//
// WHICH DRAWS. A weapon is a separate SkeletalMeshComponent drawn from its own
// buffers, so it never reaches the mesh split's own draw path and has to be
// caught in the general detour. The key is the VERTEX AND INDEX BUFFER PAIR,
// learned by the component sweep in weapon_id.cpp: hide 'crossbow_01', see
// which buffer pair stops arriving and comes back, and that pair IS the
// crossbow. That is identification by making the picture move, which is this
// project's own rule, and it is the same key the mesh lock uses.
//
// WHY NOT A POINTER OR A NAME AT THE DRAW. The draw call carries no component
// pointer and no name. Palette size, draw order and proximity were all tried
// on the hands and all three produced confident wrong answers.
//
// THE SOURCE FRAME. For the hand, the source is an anchor averaged over
// several vertices because a hand is deformable and its palm has to be found.
// A weapon assembly is RIGID: every bone of the crossbow moves together, so
// the assembly frame is simply the palette's first bone - rotation from its
// 3x3, position from its .w column. Taking one bone rather than an average is
// not a shortcut here, it is the correct reading of a rigid body, and it is
// what keeps the loaded bolt's own animated relationship to the stock intact:
// the whole palette gets the same D, so anything the engine animates within
// the assembly still animates.
//
// LANES AND STATE. The patch happens inside the draw detour on the present
// thread. c6 is CURRENT DEVICE STATE, not a one-shot - draw_census.cpp paid
// for that once - so the game's own block is put back immediately after the
// draw or every later draw inherits the weapon's delta. No engine D3D object
// is referenced beyond the call.
//
// FAIL SOFT. Any refusal draws the weapon exactly where the engine wanted it,
// and says why. A weapon that is not attached is the old build; a weapon
// attached from a bad frame is a weapon somewhere behind the player's head.
// ===========================================================================

enum { WA_MAX_MESH = 12, WA_MAX_REGS = 256 };

struct WaMesh {
    void*    vb;             // identity only, never dereferenced
    void*    ib;
    int      hand;           // 0 left, 1 right
    char     asset[64];
    uint32_t bones;          // largest palette seen for this pair
    LONG     placed, refused;
};

static bool     g_waOn = true;              // [Hands] AttachWeapons
static WaMesh   g_waMesh[WA_MAX_MESH];
static int      g_waMeshN = 0;
static const char* g_waWhy = "no weapon draw seen yet";
static LONG     g_waDraws = 0;

// Which hand each asset belongs in. Dishonored holds the sword in the LEFT
// hand and the crossbow (and the bolt loaded in it) in the RIGHT. Overridable
// from the ini rather than compiled in, because a left-handed profile and the
// other games in this family do not agree with that.
static int      g_waSwordHand = 0;          // [Hands] AttachSwordHand
static int      g_waXbowHand  = 1;          // [Hands] AttachCrossbowHand
