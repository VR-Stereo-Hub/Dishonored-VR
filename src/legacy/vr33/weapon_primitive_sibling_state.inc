// ---- THE GHOST PASS ---------------------------------------------------------
//
// A weapon mesh is drawn more than once per frame: a colour pass, and at least
// one depth or shadow pass. Only a pass whose shader declares LocalToWorld can
// be identified by transform, so the others were refused and drawn where the
// engine put them - which is a dark copy of the weapon standing at the native
// position while the real one follows the hand. The tester reports exactly
// that: locked to where the weapon used to be, flickering many times a second.
//
// A refused draw that shares a MATCHED contract's geometry - same buffers,
// same range, same primitive count, different shader - is that pass. It does
// not need identifying again; it needs the correction its sibling already
// computed this Present.
//
// The cheap pre-filter is the primitive count, which arrives as a call
// argument and costs nothing to test. Only a draw whose triangle count matches
// a live contract pays for the device reads.
static LONG g_waGhostSeen   = 0;   // refused draws sharing a contract's geometry
static LONG g_waGhostFixed  = 0;   // of those, corrected
static LONG g_waGhostNoBone = 0;   // no BoneMatrices declaration at all
static LONG g_waGhostNoDelta = 0;  // sibling had no correction this Present
static LONG g_waGhostRange  = 0;   // the palette range failed its bounds check
