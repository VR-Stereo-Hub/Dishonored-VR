// State chunk 56 for game/dishonored/hands/grip_pivot - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-33 step 1: ROTATE THE HAND ABOUT THE GRIP, not about the bone's origin.
//
// THE FAULT THIS FIXES. The rotation drive writes the controller's world
// rotation into `SkelControlSingleBone.BoneRotation`, and the translation
// drive writes the controller's world position into `BoneTranslation`. Both
// are replace-mode in world space, so the bone's ORIGIN lands on the
// controller and the mesh turns about that origin.
//
// A UE3 hand bone's origin is at the WRIST. A real hand turns about the
// middle of the palm, which is where the controller's grip sits inside your
// closed fingers. Those are several centimetres apart, so every rotation of
// the controller swings the hand through an arc it should not travel: the
// hand stays put when you turn your wrist and slides when you roll it, and a
// weapon attached to it slides further because it is further out the lever.
// The same fault was seen and named on the BioShock mod.
//
// THE FIX IS ONE SUBTRACTION, and it is exact rather than a correction
// factor. Rotating about a point P instead of the origin O is the same as
// rotating about O and then moving the result so that P lands where it
// belongs. In the world-placement form the drive already uses, that is
//
//     bone_origin_world = controller_world - R * P_local
//
// where `P_local` is the vector from the bone's origin to the grip point,
// expressed in the HAND's own frame, and R is the rotation already being
// commanded. No new rotation, no second write, no fight with the existing
// lanes: the same `w[]` the translation drive already computes, with one
// vector subtracted in the controller's basis.
//
// WHERE P COMES FROM. Not from a guessed constant. The mesh split already
// owns the hand's geometry (`mesh_split.cpp` reads and validates the vertex
// window to cut it), so the distance from the WRIST RING - the cut plane
// itself, which is at the wrist by construction - to the CENTROID of the
// hand-class triangles is a measured property of this asset. That distance
// along the limb axis is what seeds the forward component, and the log prints
// it. The lateral and vertical components seed at zero, because nothing in
// the mesh says where inside the palm a controller's grip sits - that is a
// fact about the controller and the player's hand, not about the asset, and
// it is what the knob is for.
//
// This is a SEED, not an answer. The rigorous derivation would read the hand
// bone's own matrix out of the vertex-shader constant palette and express the
// palm centroid in it exactly; that is a bigger change and it is not needed to
// find out whether the pivot is the fault. If the knob converges somewhere
// far from the seed, the seed is wrong and the palette read is the next step.
//
// LANE: the values are read on the script lane inside the existing hand write.
// The hotkeys post requests and nothing acts on them off-lane, same as the
// mesh split's knobs.
// ===========================================================================

// The grip point in the HAND's own frame, in unreal units: forward along the
// limb axis, right across it, up out of the back of the hand. Per side, so a
// left and a right controller can differ - they are mirror images and the
// lateral term is the one most likely to need opposite signs.
static float g_gripP[2][3] = { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
static uint8_t g_gripSet[2] = { 0, 0 };     // placed by hand (ini or knob)?

// The measured seed: wrist ring to hand centroid along the limb axis, per
// side, in mesh units. Zero until the split has been built at least once.
static float g_gripSeed[2] = { 0.0f, 0.0f };
static uint8_t g_gripSeedOk[2] = { 0, 0 };

// [Hands] GripPivot. ON is the fix; OFF reproduces the pre-VR-33 behaviour
// exactly - the bone origin goes to the controller and the hand turns about
// the wrist - which is the live A/B for "is the pivot the fault at all".
static bool g_gripOn = true;

// The knob. HOME cycles the axis, INSERT / DELETE nudge it, END toggles the
// whole compensation so the difference can be felt in one press. The numpad
// is fully spoken for by the mesh split.
static volatile int g_gripAxisReq = 0;      // cycle which axis the knob moves
static volatile int g_gripNudgeReq = 0;     // -1 / +1
static volatile int g_gripToggleReq = 0;
static volatile int g_gripSideReq = 0;      // both / left / right
static volatile int g_gripAxis = 0;         // 0 forward, 1 right, 2 up
static volatile int g_gripSide = 0;         // 0 both, 1 left, 2 right
static const float kGripStep = 0.5f;        // unreal units per press
static const char* const kGripAxisName[3] = { "FORWARD (out the arm)",
                                              "RIGHT (across the palm)",
                                              "UP (out of the back of the hand)" };
static double g_gripNextReport = 0.0;
static volatile uint32_t g_gripApplied = 0;   // writes that carried the offset
static volatile uint32_t g_gripSkipped = 0;   // writes that could not
