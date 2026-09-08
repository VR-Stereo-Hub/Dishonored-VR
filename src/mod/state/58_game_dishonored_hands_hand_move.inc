// State chunk 58 for game/dishonored/hands/hand_move - included by
// src/mod/dishonoredvr.cpp. Original order is preserved across chunks.
//
// ===========================================================================
// VR-33 PHASE 1: move one hand by a constant, and look.
//
// The first thing in this ticket that WRITES to the skeleton. It is bounded,
// reversible, and it answers a question that three rounds of static analysis
// could not: WHICH BONE does LookAtControl_LeftHand actually drive?
//
// A control's variable name is not that evidence - ENGINE_NOTES records the
// camera control aiming the arms at the view while the hand controls were the
// ones under study. Moving it ten units and looking costs one run.
//
// THE SEQUENCE IS off -> additive zero -> +10 -> -10 -> off, and the zero step
// is the important one. Writing a constant REPLACES whatever translation input
// the control already had; bAddTranslation does not preserve it. So the zero
// step applies the entire write path with an offset of nothing: if the hand
// moves at THAT step, the movement is the write path clobbering the control's
// own input, not our offset, and every later reading would have been wrong.
//
// EVERY OWNED FIELD IS SAVED AND RESTORED. BoneTranslation, its space byte and
// the bool dword are captured on the first write and put back on OFF, so the
// experiment cannot leave the rig altered.
//
// WHAT IT DOES NOT ESTABLISH, stated because the last four attempts each
// claimed more than they had:
//
//   * Visible hand movement proves INFLUENCE, not that the control targets the
//     hand bone - an upstream arm bone would move the hand too. The acceptance
//     test is what the other hand and the camera do, not what this one does.
//   * Three distinct control pointers do not prove independent effects. That
//     is a result of this test, not an input to it.
//   * ControlTickTag is logged as an observation only. A tag that changed does
//     NOT establish when the field was consumed, nor that no later writer
//     replaced it, so no timing conclusion is drawn from it.
//
// LANE: the script lane, from PeHandler, beside the read-only reports.
// ===========================================================================

#define HM_OFF   0
#define HM_ZERO  1      // the write path, with an offset of nothing
#define HM_PLUS  2
#define HM_MINUS 3
#define HM_STATES 4

static bool     g_hmOn = false;          // [Hands] HandMoveTest - WRITES to the rig
static volatile int g_hmState = HM_OFF;
static volatile int g_hmStepReq = 0;
static float    g_hmAmount = 10.0f;      // [Hands] HandMoveUU
static int      g_hmAxis = 0;            // [Hands] HandMoveAxis - 0 X, 1 Y, 2 Z

// What the control looked like before we touched it. Restored on OFF.
static bool     g_hmSaved = false;
static float    g_hmOrigTrans[3] = { 0.0f, 0.0f, 0.0f };
static uint8_t  g_hmOrigSpace = 0;
static uint32_t g_hmOrigBools = 0;
static uint8_t* g_hmCtl = NULL;          // the control we own, while we own it

static volatile uint32_t g_hmWrites = 0;
static int      g_hmLastTag = -1;
static double   g_hmNextReport = 0.0;
static const char* const kHmStateName[HM_STATES] = {
    "OFF (original values restored)",
    "ADDITIVE ZERO - the whole write path, offset of nothing",
    "PLUS", "MINUS"
};
