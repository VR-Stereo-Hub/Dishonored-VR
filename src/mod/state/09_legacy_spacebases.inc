// State chunk 09 for legacy/spacebases - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static volatile LONG g_armReq   = 0;
// 30.62: bone-bank wiggle test. The probe found two FBoneAtom banks on every
// skeletal component: +0x208 (component-space accumulation = SpaceBases) and
// +0x214 (parent-relative = LocalAtoms). This proves which one the RENDERER
// consumes: rotate every bone of the WEAPON component rigidly and see which
// window visibly tilts the weapon. Written on every script dispatch, because
// the engine recomputes these each tick and a once-per-frame write loses the
// race (the lesson the FOV lever taught us).
static volatile bool g_boneRtGo = false;
// 30.69: SIZE SWEEP. The 42-register (14-bone) guess offset 1736 uploads with
// no visible effect, so that rig is not the weapon. The census listed every
// size the game uploads; step through them one at a time, 2 s each, and the
// window in which the hands/weapon jump identifies the rig for good.
static const UINT kBrtSizes[] = {42, 36, 54, 72, 93, 96, 99, 144, 159, 165, 204};
static const int  kBrtSizeN = 11;
static volatile UINT g_brtTarget = 0;
static int    g_brtIdx = -1;
static double g_brtUntil = 0.0;
static float  g_camPosC5[3] = {0, 0, 0};
static bool   g_haveC5 = false;
static volatile LONG g_brtHits = 0;

// ============================================================================
// 30.70 - THE RENDER-TIME HAND/WEAPON DRIVE.
//
// Everything upstream of the GPU was a dead end: the in-memory bone banks
// (+0x208 SpaceBases-like, +0x214 LocalAtoms-like) can be written at any rate
// and nothing changes on screen (30.62-65), because the renderer keeps its own
// copy of the pose. The 30.69 sweep found where that copy lands and proved a
// write there reaches the eye: the skinned bone matrices arrive as vertex
// constants at c6, 3 registers per bone, and offsetting them VISIBLY MOVED
// THE SWORD.
//
// Measured rig signatures (30.69, the only sizes on screen):
//   c6 x36  = 12 bones = the SWORD  (bone0 translation a constant (0,0,3.0))
//   c6 x144 = 48 bones = the ARMS/HANDS rig
//   c6 x204 = 68 bones = an NPC - never touched
//
// We apply ONE shared rigid transform to both, so the hand stays welded to the
// weapon. The transform is built from the controller pose RELATIVE TO THE HEAD,
// which is the whole trick: the rig's component space is glued to the camera,
// so a head-relative pose cancels head coupling ANALYTICALLY instead of the old
// drive's EMA approximation. Turn your head and the hand stays in the room.
//
// Bone matrix layout: row i = (m[i][0], m[i][1], m[i][2], t[i]), so applying
// D = (Rc, tc) on the OUTPUT side is a pre-multiply:
//   new_row[i] = Rc[i][0]*row0 + Rc[i][1]*row1 + Rc[i][2]*row2 ; new_row[i].w += tc[i]
// ============================================================================
static bool           g_rtdEnable   = false;   // RETIRED - SkelControl replaces it
