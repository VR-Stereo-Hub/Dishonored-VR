// State chunk 37 for game/dishonored/hands/fp_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static const uint32_t kMeshRot   = 0x19c;
static const uint32_t kMeshScale = 0x1a8;
static const uint32_t kMeshScl3D = 0x1ac;

// Writing pMesh's relative Rotation WORKED - d-cmd and d-world tracked each
// other all the way down the log. But pMesh is what the camera rides on, so
// the hand turned the view instead of the weapon. The mechanism is right; the
// target is wrong.
//
// Rather than guess the next component and spend a build per guess, enumerate
// every component reachable from the pawn that has a writable relative
// transform (the Scale/Scale3D fingerprint IS the test - if a component does
// not have that block we cannot drive it anyway) and let INSERT cycle through
// them live until the weapon moves.
struct FpCand { uint8_t* obj; char name[40]; char cls[52]; char owner[52]; char asset[40];
                float pivot[3]; bool havePivot;
                int yawSign, pitchSign;      // measured, not guessed
                float calY0, calP0;          // world angles at rest
                float basis0[9];             // parent basis at rest
                float parentY, parentP;      // parent frame at rest
                float lastCmdY, lastCmdP, lastCmdR;
                float plive[9];              // live animated parent, recovered
                bool  haveLive;
                float calT0[3];              // world position at rest (cal)
                float emap[9];               // written T -> world motion, per uu
                float einv[9];               // world motion -> written T
                bool  haveE;
                float ddSave[2];             // draw distances before culling
                bool  ddCulled; }; 
static FpCand   g_fpCand[24];
static int      g_fpCandN = 0;
static void FpNoBlock(uint8_t* comp, const char* nm);   // 38.23, defined below
static int      g_fpSel   = -1;
static uint8_t* g_fpWritten = NULL;      // last object we rotated
static uint8_t* g_fpWritten2 = NULL;
static int      g_fpDualA = -1, g_fpDualB = -1;
static bool     g_fpDual = false;        // Shift+PageUp: one view model per hand
static int      g_fpSwap = 0;            // which view model gets the left hand
// (g_fpPosOn / g_fpRollOn moved up with the config globals in 30.8)
static float    g_fpNeutral[2][3];       // resting controller pose, per hand
static bool     g_fpHaveNeutral[2] = { false, false };
static float    g_fpNeutralRoom[2][3];   // hand-minus-bodyanchor at neutral
static bool     g_fpHaveNeutralRoom[2] = { false, false };
// (30.23 body-anchor globals live up with the config block)
static int      g_fpCalPhase = 0, g_fpCalFrame = 0;
static uint8_t* g_fpRef = NULL;          // a component we never write to
static float    g_fpRefY0 = 0.0f, g_fpRefP0 = 0.0f;
static bool     g_fpHaveRef = false;
static float    g_fpRefBas0[9];          // reference basis at calibration
static float    g_fpHandBas0[2][9];      // controller basis at calibration
static bool     g_fpHaveHandBas[2] = { false, false };
static float    g_fpHandNeutY[2] = { 0, 0 }, g_fpHandNeutP[2] = { 0, 0 };
static float    g_fpNeutralYaw[2]   = { 0.0f, 0.0f };   // resting hand ANGLES
static float    g_fpNeutralPitch[2] = { 0.0f, 0.0f };
static float    g_fpPosScale = 50.0f;    // unreal units per metre
static float    g_fpPosMax   = 45.0f;    // never fling it further than this
static int      g_fpPosSign  = 1;        // DELETE flips if this reads backwards
static float    g_fpPivotMix2[2] = { 1.0f, 1.0f };  // Shift+Up/Down per hand
// Trim, in actor space: X forward, Y right, Z up. Arrow keys nudge it live so
// a weapon that sits too far out can be pulled back toward the middle of your
// view without another build.
// right-hand values are the ones you trimmed to by hand in play
static float    g_fpBias[2][3] = { { 0, 0, 0 }, { 30, -31, 0 } };
static int      g_fpTrimHand = 1;        // arrows trim this hand; HOME+arrows swaps
static int      g_fpPivotPend = 0;       // frames to hold still while measuring
static uint8_t* g_fpLastPawn = NULL;
static double   g_fpCollectMs = 0.0;

static const char* FpAssetName(uint8_t* comp);
static uint8_t* FpPawn();
static void FpComputePivots();
static bool FpIsViewModel(const FpCand* k);
static bool FpPawnYaw(float* out);
static float FpWrapPi(float a);

// The spinning crossbow was my fault, and the mistake is a classic one:
// 28.0 recovered the parent frame by reading the component's world matrix and
// subtracting the rotation we had just written to it. That is a closed loop
// around a value we ourselves drive, and with one frame of lag between write
// and read the gain passes 1 and it runs away - which on screen is a weapon
// spinning.
//
// Break the loop by measuring the frame from something we NEVER write to.
// pMesh hangs off the same player and is not a view model, so its world
// matrix is pure parent motion with none of our own output folded back in.
static bool FpRefAngles(float* yaw, float* pitch);

// ---------------------------------------------------------------------------
// GROUND TRUTH self-test (Shift+F9), build 30.0.
//
// Every "fix" so far has been inferred from a user report of which way the
// weapon felt wrong, and the handoff records that inference being wrong at
// least three times. This test takes the hands out of the loop entirely:
// command a KNOWN rotator on the view models only (never pMesh - the old
// calibration commanded pMesh too, and if the weapons hang off pMesh's hand
// bones every probe measurement was contaminated by pMesh's own commanded
// rotation), wait for the engine to compose it, and compare what actually
// happened to the world basis against the commanded matrix - both ways:
//
//   dL = b1 * b0^T   (engine applied the rotator BEFORE the parent:  b1 = M*b0)
//   dR = b0^T * b1   (engine applied the rotator AFTER the parent:   b1 = b0*M)
//
// errL / errR are degrees between that delta and the commanded UE3 rotation
// matrix; errLi / errRi are against its inverse. Whichever of the four reads
// ~0 for all three probes on BOTH weapons IS the engine's composition law,
// and the drive formula follows from it with no signs left to guess.
//
// Phase 6 then watches the hand basis with the controllers at rest while the
// user moves their head: |Q| staying ~0 proves (or refutes) the 29.1 claim
// that head motion can no longer leak into FpHandBasisWorld.
// ---------------------------------------------------------------------------
static bool   g_gtActive = false;
