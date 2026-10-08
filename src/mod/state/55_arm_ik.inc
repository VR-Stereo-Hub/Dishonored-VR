// IK configuration is edited by the overlay and read on the render lane.
// No engine object or engine bone pointer is retained by this feature.
#include "game/dishonored/hands/arm_rig.h"
static std::atomic<bool> g_ikOn{false},g_ikReload{false};
static std::atomic<float> g_ikForward{-16},g_ikRight{0},g_ikUp{-25},g_ikWidth{38.1f};
static std::atomic<float> g_ikLength{1.27f},g_ikElbowOut{.6f};
// [Hands] ArmIKGameArmInAnim: while a game animation owns a hand, that side's IK arm blends to
// the game's own arm by the hand-back weight (full game arm when the game fully owns it).
// Off = the IK arm follows the animated wrist from the tracked shoulder (the original design).
// Default on since the 2026-10-04 headset run accepted it.
static std::atomic<bool> g_ikGameArmInAnim{true};
// [Hands] ArmIKGameArmShoulder: that game arm is re-seated so its shoulder sits on the IK shoulder
// (the wrist stays the game's). 0 = off, the game's arm exactly as drawn for its own camera;
// 1 = every game animation except the choke (judged right as it was in the 2026-10-04 run);
// 2 = the choke as well. Default 1 since the same run accepted it.
static std::atomic<int> g_ikGameArmShoulder{1};
// [Hands] ArmIKGameArmMaxStretch: how far the re-seat may lengthen the game's arm to reach the IK
// shoulder. 1.25 left the shoulder in front in most takedowns (they needed 1.25..1.84 on
// 2026-10-05/06); 1.9 reaches it in every one measured.
static std::atomic<float> g_ikGameArmMaxStretch{1.9f};
static float g_ikGameShare[2]={0,0};   // render lane: the game's share of each arm in the last IK draw (log only)
static std::atomic<const char*> g_ikStatus{"Waiting for the first gameplay arm draw"};
static std::atomic<unsigned> g_ikSolves{0},g_ikRefusals{0};
static void IkDropGeometry();
static void IkResetPose();
