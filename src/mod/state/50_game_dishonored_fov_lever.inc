// State chunk 50 for game/dishonored/fov_lever - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// 30.62: rotate every bone of the weapon component in one bank, rigidly.
// FQuat is (x,y,z,w); left-multiplying by r spins the whole mesh in that
// bank's own space, which is unmistakable on screen if the bank is live.
// 30.64: the 30.62/63 test excluded 'Skm_Player' to protect the camera - but
// that IS the first-person arms+hands rig (79 bones), and the weapon rides it
// (its own 14-bone skeleton is just the prop). So include it, selected BY BONE
// NAME: only arm/hand/finger/weapon bones move, never spine/neck/head, so the
// camera cannot be swung by the test. The mask is built once on the game
// thread (resolving names costs far too much to redo per dispatch).
static uint8_t* g_bwObj[8];
static int      g_bwBoneN[8];
static uint8_t  g_bwSel[8][192];
static int      g_bwObjN = 0;
