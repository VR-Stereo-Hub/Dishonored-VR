// State chunk 56 for game/dishonored/hands/palette_drive - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-33: THE STATIC HAND, driven at the BONE PALETTE.
//
// WHY HERE AND NOWHERE ELSE. Three earlier lanes are closed, each on a
// measurement, and this one is chosen because it is downstream of all of them:
//
//   * SpaceBases / LocalAtoms writes in memory do nothing - the renderer keeps
//     its own copy of the pose (30.62-65).
//   * SkelControl world-space TRANSLATION works; world-space ROTATION does not.
//     Writes into BoneRotation lose to the engine's recompute, measured in
//     38.x at 9,000 writes a second and reproduced again on 2026-09-06.
//   * Moving vertices in our own buffer cannot work either, and for a reason
//     that is easy to miss: the vertex shader skins by THIS palette, so a
//     position written in bind space is still moved wherever the palette says.
//
// The palette arrives as vertex constants at c6, three float4 rows per bone,
// and the draw consumes them immediately. Nothing recomputes after it. That is
// the whole argument: this is the last word before the GPU.
//
// It is also already proven to move geometry - the legacy `rtd_drive` lane
// moved the sword from exactly here. What it could never do was DECOUPLE the
// sword, because it was trying to correct a transform that stayed parented to
// the camera. This does not correct anything. It REPLACES the hand's placement
// outright with one derived from the controller, so there is nothing left to
// fight.
//
// WHICH SLOTS ARE THE HAND. This is what the arm/hand split bought, beyond the
// cut itself. Blend indices in the vertex data name THIS palette, so the bones
// the split classified are palette slots directly: g_msHandBone[side] is the
// hand bone's slot, g_msBoneSide[] says which arm a slot belongs to, and
// g_msBoneHand[] says whether it is at or past the wrist. No name lookup, no
// guessed index, and no chance of dragging a forearm along - the arms are cut
// away in the same pass that identified them.
//
// THE TRANSFORM IS A RIGID DELTA, NOT A REBUILD. We never learn the bind pose,
// and we do not need it. For one side, take the hand bone's CURRENT origin out
// of the palette as a reference, and apply one rigid delta to every hand bone
// on that side:
//
//     D = T(controller - R*P) . R . T(-reference)
//
// Every hand bone gets the same D, so the hand's internal arrangement is
// preserved exactly and the whole hand moves and turns as one piece. That is
// what makes it a STATIC hand: correct placement, no animation, no gripping
// fingers. Animation would mean skinning it ourselves against this palette,
// which is the better method and is deliberately not attempted here.
//
// P is the grip offset in the hand's own frame, so the hand turns about the
// GRIP rather than about the bone origin at the wrist. Rotating about a point
// is rotating about the origin and then placing the result so that point lands
// where it belongs - the same subtraction as before, applied where nothing can
// recompute it away.
//
// THE SPACE IS A QUESTION, AND IT IS ASKED RATHER THAN ASSUMED. ENGINE_NOTES
// records that Arkane draws the first-person view model in CAMERA space, and
// that is the default. It is not taken on faith: the probe already in
// vs_const_hook answers it (a camera-aligned bone's translation is unchanged
// as you turn on the spot; a world-aligned one swings with your yaw), the
// answer is logged, and `[Hands] PaletteSpace` switches live. If the hand
// swings with the head, that is this switch and one press settles it.
//
// LANE: the render thread, inside the constant upload, which is the only place
// the palette exists. Nothing here writes engine memory - the buffer is a copy
// and the original data is never modified.
// ===========================================================================

#define PD_MAX_REG 256          // 3 registers a bone, the arms measured 144

// [Hands] PaletteDrive. ON, which is the stated exception to default-OFF: this
// IS the experiment the test sessions run, exactly like the arm split. It
// fails soft in every direction - an unrecognised draw, a split that is not
// ready, a missing controller pose all pass the game's own constants through
// untouched.
static bool g_pdOn = true;
static volatile int g_pdToggleReq = 0;

// 0 = the palette is camera-relative (ENGINE_NOTES: Arkane draws the FP view
// model in camera space), 1 = world. Wrong choice reads as a hand that swings
// with the head, which is the same symptom the last two lanes died of, so it
// is a switch and not a constant.
static int  g_pdSpace = 0;
static volatile int g_pdSpaceReq = 0;

// The grip offset in the hand's own frame, unreal units: forward along the
// limb, right across the palm, up out of the back of the hand. Seeded from the
// mesh - the distance from the wrist ring to the hand's centroid measured
// 12.26 uu on this asset - because the split owns the geometry and can say.
static float g_pdGrip[2][3] = { { 12.26f, 0.0f, 0.0f }, { 12.26f, 0.0f, 0.0f } };
static uint8_t g_pdGripSet[2] = { 0, 0 };
static volatile int g_pdAxis = 0;          // which axis the knob moves
static volatile int g_pdSide = 0;          // 0 both, 1 left, 2 right
static volatile int g_pdAxisReq = 0, g_pdNudgeReq = 0, g_pdSideReq = 0;
static const float kPdStep = 0.5f;

// Scale from metres of controller travel to unreal units. The world measured
// 108 uu/m on this build; it is a key because a hand that moves too far is a
// scale error and not a tracking error, and telling those apart by eye is
// impossible.
static float g_pdScaleUU = 108.0f;

// What actually happened, per 15 s. Every one of these can print the unwelcome
// answer: a draw that never matched, a split that never armed, a pose that was
// never good.
static volatile uint32_t g_pdDraws    = 0;   // palette uploads we rewrote
static volatile uint32_t g_pdSeen     = 0;   // c6 uploads of any size
static volatile uint32_t g_pdNoSplit  = 0;   // matched, but no split to name bones
static volatile uint32_t g_pdNoPose   = 0;   // matched, but no controller pose
static volatile uint32_t g_pdNoBones  = 0;   // matched, but no hand bone on a side
static volatile uint32_t g_pdBonesHit = 0;   // bone matrices actually replaced
static double g_pdNextReport = 0.0;
static uint32_t g_pdLastCount = 0;           // the last c6 size we saw
