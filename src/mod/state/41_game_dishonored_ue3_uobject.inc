// State chunk 41 for game/dishonored/ue3/uobject - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ==== 32.95: measure the collision cylinder =============================
// The under-table facts, from the user: Corvo crouches with the button, STAYS
// crouched, and still cannot fit under tables. So the question is no longer
// input - it is whether the B-toggle crouch actually SHRINKS the pawn's
// collision cylinder, or only plays the animation and lowers the camera.
// Read it from the engine's reflection (the skc/prop technique): find the
// UProperty named CollisionHeight declared on CylinderComponent and
// CollisionComponent declared on Actor, then chase pawn -> component ->
// height. Logged whenever the stance flips; standing vs crouched numbers
// settle it. If the height does NOT change on crouch, the toggle is not a
// full UE crouch and the fix moves to whatever Dishonored uses instead.
static uint32_t g_cylHOff = 0, g_cylCompOff = 0;
static int      g_cylTried = 0;
// 33.0: while the cylinder is readable, it is the ONLY stance writer. The
// button-style detector mis-measured "hold" this session, which made the
// press-tracker assert "standing" EVERY TICK against the cylinder sync - the
// churn in the log (cyl corrects to CROUCHED 4x/s, trim flaps). Prediction
// made sense when there was no ground truth; now it only adds noise.
static double   g_cylOkMs = 0.0;
