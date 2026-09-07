// state/59 - VR-33 step 2: the draw capture (palette_capture.cpp).
//
// WHAT THIS IS FOR. Build A placed the palm at a target and the palm landed
// there, but nobody knows what that target MEANS in the world, because the
// conversion from palette coordinates to the rendered image has never been
// measured. Every attempt to infer it - a head-space neutral, a yaw residual,
// a calibrated offset - produced a number that absorbed the origin, the scale,
// the grip and the anchor error into one constant and could not be read.
//
// So capture the draw and measure the conversion instead:
//
//     clip = Q * [q_palette, 1],   Q = P * F
//
// F is what placement needs. The optical centre recoverable from Q supplies a
// POSITION only; it does not supply F's linear part, axis convention or scale,
// and algebra alone permits Q = (P*G^-1)(G*F) for many G. Deriving F is a
// separate deliverable and is not done here - this module's whole job is to
// produce honest inputs for it.
//
// LANE. The copy happens on the draw's own thread at the capture point, into
// an immutable bounded packet. The worker gets owned bytes: never a live D3D
// object, never a pointer into mutable mesh or global storage. Hashing,
// disassembly and file writing happen there.
//
// GAME-DERIVED CONTENT. Shader bytecode is the game's. It goes to
// <data_dir>\dumps\, which .gitignore already excludes, and nothing derived
// from it is committed - only conclusions, in our own words, in ENGINE_NOTES.

// Bytecode is identified by CONTENT HASH, not by shader pointer: a pointer can
// be reused after the object it named was destroyed, so it is telemetry rather
// than an identity. The same bytecode runs with different constants, eyes and
// poses, which is why code is saved once and STATE is sampled repeatedly.
enum {
    PC_MAX_SHADERS   = 16,      // distinct bytecode blobs remembered this run
    PC_MAX_BYTECODE  = 64 * 1024,
    PC_MAX_CONSTF    = 256,     // float4 registers; vs_3_0 guarantees >= 256
    PC_MAX_CONSTI    = 16,
    PC_MAX_CONSTB    = 16,
    PC_MAX_ANCHOR    = MP_ANCHOR_N,
    PC_QUEUE         = 4        // packets in flight to the worker
};

// One captured state packet. Immutable once the render thread hands it over.
struct PcPacket {
    uint32_t id;                    // unique per capture
    uint32_t bytecodeHash;          // which code this state belongs to
    uint32_t resetEpoch;            // device generation; state is not comparable across one
    uint32_t frame;
    int      eye;                   // the eye tag this draw carried
    uint32_t paletteGen;            // g_mpCacheGen at the copy

    // The float bank actually in force, read from the DEVICE - not from our
    // own shadow of what the game requested. hkSetVSConstF modifies c0, so the
    // requested constants are not necessarily the ones the draw consumed.
    UINT     constFCount;
    float    constF[PC_MAX_CONSTF][4];
    UINT     constICount;
    int      constI[PC_MAX_CONSTI][4];
    UINT     constBCount;
    BOOL     constB[PC_MAX_CONSTB];

    // The draw's own contract, so the offline side can reproduce the inputs.
    INT      baseVertex;
    UINT     minIndex, numVertices, startIndex, primCount, stream0Off, stride;

    // Viewport and render target: needed to relate clip space to pixels.
    D3DVIEWPORT9 viewport;
    UINT     rtWidth, rtHeight;

    // THE ANCHOR'S OWN INPUTS. Without these the offline side has constants
    // but no reproducible way to recompute the palm position this build
    // claims - it could only take our word for it.
    int      anchorCls;
    int      anchorN;
    uint32_t anchorIdx[PC_MAX_ANCHOR];
    float    anchorPos[PC_MAX_ANCHOR][3];
    uint8_t  anchorBi[PC_MAX_ANCHOR][4];
    float    anchorBw[PC_MAX_ANCHOR][4];
    float    anchorQ[3];            // what THIS build computed, for comparison

    // The rendering configuration in force, because a capture taken with a
    // different camera setup describes a different rendering than the one the
    // hands must work in.
    float    worldScaleUU, posScaleUU;
    float    hmdYaw, viewYawRad;
    float    camPosC5[3];
    int      posTrackOn, injectHeadOn, absOn, driveOn;

    // The pose the target was built from, identified rather than "whatever was
    // most recent": head and hand transforms with their validity.
    float    headPose[3][4], handPose[3][4];
    int      headOk, handOk;
};

static bool     g_pcOn = false;             // [Hands] PaletteCapture
static int      g_pcWant = 0;               // packets still requested (F7 arms a batch)
static volatile LONG g_pcArmReq = 0;        // hotkey -> present thread
static uint32_t g_pcNextId = 1;
static uint32_t g_pcResetEpoch = 0;

// Bytecode seen this run, by content hash. Saved once each.
static uint32_t g_pcHash[PC_MAX_SHADERS];
static int      g_pcHashN = 0;

// The queue. Single producer (the draw thread), single consumer (the worker).
static PcPacket g_pcQueue[PC_QUEUE];
static volatile LONG g_pcHead = 0;          // written by the producer
static volatile LONG g_pcTail = 0;          // written by the consumer
static volatile LONG g_pcDropped = 0;       // queue-full: reported, never silent
static volatile LONG g_pcFailed = 0;        // a capture that could not complete
static volatile LONG g_pcDone = 0;

static HANDLE   g_pcThread = NULL;
static volatile LONG g_pcQuit = 0;
