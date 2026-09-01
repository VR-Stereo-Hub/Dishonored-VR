// State chunk 42 for game/dishonored/crouch - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


struct ArmBone { uint32_t idx, num; char nm[40]; };
static ArmBone  g_armBones[40];
static int      g_armBoneCnt = 0;
static uint8_t* g_armComp = NULL;
static uint8_t* g_fnHide = NULL;
static uint8_t* g_fnUnhide = NULL;
static bool     g_armHidden = false;
