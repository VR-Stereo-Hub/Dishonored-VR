// state/63_game_dishonored_aim_seam.inc - VR-57: the fire-seam probe's state.
// Read-only: nothing here is written into the game.
static const int kAsMax = 8;
static bool     g_asOn = false;       // [Aim] SeamProbe
static bool     g_asVerbose = false;  // [Aim] SeamVerbose: log the quiet instances too
static double   g_asLastMs = 0.0, g_asScanMs = 0.0;
static int      g_asN = 0;
static uint8_t* g_asObj[kAsMax] = {};
static int32_t  g_asTag[kAsMax] = {};
static int32_t  g_asPawnTag = 0x7fffffff;
static bool     g_asLiveSeen = false;  // a context whose tick tag advanced with a real direction

// VR-57 step 3: DRIVING the cache from the controller ray ([Aim] DriveFromHand).
static bool     g_asDrive = false;      // the lever; ships 0
static float    g_asDriveDistUU = 800.0f;   // how far along the ray the aim point sits
static uint8_t* g_asCtx = NULL;         // the equipped weapon's projectile context
static uint32_t g_asCtxOff = 0;         // its cache offset
static double   g_asCtxMs = 0.0;        // when it was last revalidated
static long     g_asWrites = 0, g_asWriteRefused = 0;
static double   g_asWriteLogMs = 0.0;
static const char* g_asWriteWhy = "not attempted";
static float    g_asLastDir[3] = {};
static long     g_asOverwritten = 0;
static const int kAsWhyMax = 8;
static const char* g_asWhyStr[kAsWhyMax] = {};
static long     g_asWhyCnt[kAsWhyMax] = {};
static int      g_asWhyN = 0;
static float    g_asProjMine[2] = {}, g_asProjGame[2] = {};
static float    g_asProjErr = -1.0f;
static long     g_asProjSamples = 0;
static double   g_asPoseLogMs = 0.0;

// ---- VR-57 Phase 1: the SHOT PROBE. Read-only, [Aim] ShotProbe ---------------
//
// What the bolt actually did, against what we asked for. The acceptance test is
// NOT the angle between the bolt and the controller ray: a bolt launched parallel
// to that ray from a muzzle offset from the controller misses the visible dot by
// the whole transverse offset, at zero angle. What must be measured is the MISS at
// the plane of the dot the player can see.
//
// The observation seam is the ProcessEvent dispatch ON the projectile, which the
// MotionAim work already proved reaches a live object: the engine is inside a
// synchronous call on it, so scalars can be copied and nothing is retained. This
// probe never writes and never keeps the pointer past the call.
//
// The field offsets come from that same work, derived against this build:
//   +0x050 spawn forward (float3)    +0x080 position (float3)
//   +0x09c rotator (int32 p,y,r)     +0x1b4 velocity (float3), arrow speed 40000
// They are re-checked for plausibility on every read rather than trusted, and a
// read that fails is counted and named instead of dropped.
//
// TIMING, already recorded by that work and the reason there are two stages: the
// engine fills velocity in AFTER it announces the projectile. So the first sight
// has a valid position and forward and NO velocity, and the velocity arrives a
// dispatch or two later. Stage 0 reports the spawn forward as a labelled PROXY for
// direction; stage 1 reports the measured velocity. Only stage 1 is evidence about
// where the bolt went, and every record says which stage it came from.
static bool     g_shOn = false;         // [Aim] ShotProbe; ships 0
static const int kShRayHist = 24;       // bounded history, ~0.4 s of script ticks
struct ShRay {
    double  ms;                         // when this solve ran, script lane
    uint32_t gen;                       // the published ray's generation
    uint64_t sampleMs;                  // the INPUT sample's own stamp
    bool    ok;
    float   H[3], d[3], P[3], T[3];     // game space: origin, dir, written point, dot
    float   viewF[3];                   // the VIEW forward at the same instant, so a
                                        // bolt can be tested against the head as well
                                        // as against the controller
    float   distP, distT;               // uu
    bool    wrote;                      // did the drive write this solve
};
// Phase A0: the solve association has to carry an AGE BOUND. Picking the newest
// solve at or before a sighting is an association and not the input the shot
// consumed, and without a cap it will happily marry a bolt to a solve from
// seconds earlier. 24 slots is a count of script dispatches, not a duration.
static const double kShRayMaxAgeMs = 150.0;
static ShRay    g_shRay[kShRayHist] = {};
static int      g_shRayN = 0;           // monotonic; index is (n-1) % kShRayHist
static const int kShMax = 6;            // in-flight projectiles tracked
struct ShRec {
    uint8_t* obj;                       // identity ONLY; never dereferenced off-call
    uint32_t nameIdx, clsName;
    int      id;
    double   firstMs, lastMs;
    int      sights;
    bool     haveRay;
    ShRay    ray;                       // the solve matched to this shot
    double   rayAgeMs;
    float    S[3], fwd0[3], vel[3];
    float    speed;
    int      stage;                     // 0 = forward proxy only, 1 = velocity measured
    bool     reported;
    // Phase A0 identity. An address alone can merge two shots or suppress the
    // second as already reported, because the engine pools projectiles.
    uint32_t epoch;          // gameplay epoch; an address from a previous one is void
    float    spawnLoc[3];    // first-sight position, part of the identity
    bool     ambiguous;      // identity could not be established; counted, not scored
    // The candidate rotations, sampled at first sight AND again at the velocity
    // observation, because comparing a fresh rotation against an older bolt and
    // calling them simultaneous is how a frame question gets answered wrongly.
    bool     candOk[4];
    float    candFwd[4][3];
    int32_t  candRaw[4][3];
    bool     candOk2[4];
    float    candFwd2[4][3];
    int      writerAtShot;   // which head writer was live: 0 none, 1 script, 2 fallback
};
// The candidates, in one order used everywhere so a log column cannot drift.
enum ShCand { kShCamPov = 0, kShCamCache = 1, kShPcRot = 2, kShPawnRot = 3, kShCandN = 4 };
static const char* const kShCandName[kShCandN] = {
    "cameraPOV(0x9c, WRITTEN by the mod from the head)",
    "cameraCache(0xd0, also written)",
    "playerController Actor.Rotation (resolved by name)",
    "pawn Actor.Rotation (the body, resolved by name)"
};
static ShRec    g_shRec[kShMax] = {};
static int      g_shRecN = 0;
static int      g_shNextId = 1;
// Populations. A counter is not evidence until its population is known, and the
// beat prints every one of these even when they are zero.
static long     g_shDispatches = 0;     // projectile-class dispatches seen
static long     g_shFirstSights = 0;    // distinct projectiles newly recorded
static long     g_shSecondSights = 0;   // a recorded projectile seen again
static long     g_shCompleted = 0;      // records that reached stage 1 and reported
static long     g_shNoRay = 0;          // no usable solve to match
static long     g_shBadRead = 0;        // offsets failed plausibility
static long     g_shEvicted = 0;        // pushed out of the ring before stage 1
static long     g_shMissExcluded = 0;   // the miss metric refused its own inputs
static double   g_shBeatMs = 0.0;
static long     g_shStaleRay = 0;       // association refused on age
static long     g_shForeign = 0;        // not the player's ordinary crossbow bolt
static long     g_shAmbiguous = 0;      // pooled-address identity unresolved
static uint32_t g_shEpoch = 1;          // bumped on load/possession/weapon change
static uint32_t g_shRotOffPc = 0, g_shRotOffPawn = 0;   // Actor.Rotation, by name
static bool     g_shRotResolved = false;
static long     g_shCandRefused[kShCandN] = {};
