// IK configuration is edited by the overlay and read on the render lane.
// No engine object or engine bone pointer is retained by this feature.
#include "game/dishonored/hands/arm_rig.h"
static std::atomic<bool> g_ikOn{false},g_ikReload{false};
static std::atomic<float> g_ikForward{-6},g_ikRight{0},g_ikUp{-20},g_ikWidth{36};
static std::atomic<float> g_ikLength{1},g_ikElbowOut{.6f};
// [Hands] ArmIKGameArmInAnim: while a game animation owns a hand, that side's IK arm blends to
// the game's own arm by the hand-back weight (full game arm when the game fully owns it).
// Off = the IK arm follows the animated wrist from the tracked shoulder (the original design).
static std::atomic<bool> g_ikGameArmInAnim{false};
static float g_ikGameShare[2]={0,0};   // render lane: the game's share of each arm in the last IK draw (log only)
static std::atomic<const char*> g_ikStatus{"Waiting for the first gameplay arm draw"};
static std::atomic<unsigned> g_ikSolves{0},g_ikRefusals{0};
static void IkDropGeometry();
static void IkResetPose();
