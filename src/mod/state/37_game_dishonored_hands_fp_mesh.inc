// State chunk 37 for game/dishonored/hands/fp_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


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

// VR-62: NOBODY OWNED THE REBUILD, and that is why the weapons never came back
// after a second load.
//
// The candidate list is dropped when the game leaves gameplay - correctly, the
// pointers are dead. The rebuild trigger everywhere is `if (!g_fpCandN)
// FpCollect()`, and every one of those call sites is a ONE-SHOT that has already
// fired by then: the material census (once per session, g_matCensusDone), the
// bone-vis scan (once, g_bvScanned), the numpad cycler (a keypress), and
// skelcontrol's per-frame refresh, which is behind `if (!g_handMesh) return` and
// so never runs at all in the shipped GamepadOnly configuration.
//
// Measured in the 12a880f7 log: ONE `handmesh: ==== 6 view model(s) ====` for
// the whole session, at the material census. The list is dropped at the second
// load and FpCollect is never called again - not once, in the remaining 55
// seconds. Every downstream symptom follows from that and none of them is a
// separate bug: 0 refs forever, contracts stuck at 0/64, and a snapshot showing
// 2 weapon members because those come from the VR-61 equipped-item path, which
// reads the inventory TArray and needs no candidate list at all.
//
// The recovery that was supposed to cover this could not work either: the
// no-anchor branch in WaCompTick calls FpInvalidateCandidates, whose first line
// is `if (!g_fpCandN) return`. With the list already empty it is a no-op that
// logs as though it acted, which is why the log repeats "Forcing a rescan" once
// a second and nothing happens.
//
// So this is not a retry budget, a delay or a relaxed check. It is the missing
// owner: while the game is in gameplay and the list is empty, something has to
// call the collector on a cadence, and nothing did.
static double   g_fpEnsureMs    = 0.0;    // next allowed rebuild attempt
static int      g_fpEnsureTries = 0;      // attempts since the list went empty
static LONG     g_fpEnsureBuilt = 0;      // rebuilds that produced a usable list
static bool     g_fpAutoRecollect = true; // [Hands] AttachAutoRecollect
static int      g_fpNoAnchorTries = 0;    // FpCollect's anchor retry, resettable

// ---- REFRESH ON A SWAP, NOT ONLY ON AN EMPTY LIST ---------------------------
//
// The load fix above gave the list an owner for one state: EMPTY. A weapon swap
// does not empty it, it makes it STALE, and the measured fault is exactly that.
//
// Headset run da1d760d. With the crossbow out the list held the bolt:
//     handmesh:   [3] 'pArrowMesh_HighRes' asset=bolt_01
// The pistol was equipped, a rebuild ran while it was out, and the list came
// back with the pistol's mesh where the bolt had been. Swapping back to the
// crossbow triggered nothing, because the list was not empty, so the bolt never
// returned. Its draws kept arriving and kept failing to find anything to verify
// against:
//     wa: NEAREST MISS hand 0, closest to 'crossbow_01': 169.241 deg / 11.00 uu
//         | prim 310 verts 257 start 0 stride 32
// which is the bolt's own geometry signature from the wa/census line. The
// crossbow itself still attached, because it comes from the equipped-item path,
// which supplies the item's own mesh and none of its CHILDREN.
//
// TWO THINGS ARE NEEDED AND ONE OF THEM IS NOT OBVIOUS.
//
// The first is the trigger: refresh when the equipment revision moves, not only
// when the list empties.
//
// The second is that ONE REBUILD IS NOT ENOUGH. The equipment event and the
// creation of the loaded bolt component are not promised to happen in the same
// tick, and a collect that wins the race returns a list with no bolt in it and
// declares success. So a revision change opens a SETTLE WINDOW: a few more
// collects over the next couple of seconds, at that revision, so a child that
// appears late is picked up. Bounded, and it reports what each pass found.
static uint32_t g_fpEquipRevSeen = 0;     // the revision the published list is for
static bool     g_fpDirty        = false; // a refresh is wanted
static char     g_fpDirtyWhy[96] = "";
static int      g_fpSettleLeft   = 0;     // extra collects owed to this revision
static double   g_fpSettleMs     = 0.0;   // next one due
static uint32_t g_fpCandRev      = 0;     // bumped when the published SET changes
static LONG     g_fpSwapRefresh  = 0;     // refreshes caused by a swap
static bool     g_fpEquipRoots   = true;  // [Hands] AttachCollectEquippedRoots
// How many extra collects a revision change is worth, and how far apart. Five at
// 400 ms covers two seconds, which is longer than any swap animation measured.
static int      g_fpSettleTries  = 5;     // [Hands] AttachSwapSettleTries
static double   g_fpSettleGapMs  = 400.0; // [Hands] AttachSwapSettleGapMs

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
