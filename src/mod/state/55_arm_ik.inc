// IK configuration is edited by the overlay and read on the render lane.
// No engine object or engine bone pointer is retained by this feature.
#include "game/dishonored/hands/arm_rig.h"
static std::atomic<bool> g_ikOn{false},g_ikReload{false};
static std::atomic<float> g_ikForward{-6},g_ikRight{0},g_ikUp{-20},g_ikWidth{36};
static std::atomic<float> g_ikLength{1},g_ikElbowOut{.6f};
static std::atomic<const char*> g_ikStatus{"Disabled"};
static std::atomic<unsigned> g_ikSolves{0},g_ikRefusals{0};
static void IkDropGeometry();
static void IkResetPose();
