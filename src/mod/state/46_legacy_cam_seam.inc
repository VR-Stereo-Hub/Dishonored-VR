// State chunk 46 for legacy/cam_seam - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static int    g_csCount  = 0;
static double g_csArmAt  = 0.0;
static double g_csNextRep = 0.0;
struct CsHook {
    int       slot;
    uint8_t*  target;
    volatile LONG calls;
    void* volatile lastEcx;
    void* volatile lastA1;
    void* volatile lastA2;
    float    a1s[3];       // sampled arg contents
    int32_t  a2s[3];
    const char* tag;
};
static CsHook g_cs[24];

// ============================================================================
// 30.43 - CAMERA POV BLOCK PROBE (read-only, replaces the vtable hunt)
//
// The vtable census was a dead end: 77 entries, and the two that fire have the
// wrong shape (slot 59's "arg2" is stack memory - a frame time and a return
// address - so it takes fewer args than the ret-8 heuristic assumed).
//
// Direct route instead. UE3's ACamera caches its point of view as a compact
// FTPOV struct: { FVector Location; FRotator Rotation; float FOV; }, i.e.
// 3 floats, 3 int32 rotator components, 1 float in degrees - 28 bytes. That
// signature is highly distinctive, and we already hold the live camera object
// (g_camObj, class DishonoredPlayerCamera). Scan its memory for the pattern
// and log every hit with its offset; the user turns and walks, and the block
// whose rotation tracks the view is the real one.
//
// Once identified, this single block gives us everything the seam promised:
//   Location -> true 6DOF (positional head tracking into the engine)
//   Rotation -> head look incl. ROLL, and it works during cutscenes
//   FOV      -> render at the HEADSET's FOV = 1:1 presentation, no stretch,
//               which is the fix for the "slightly morphed" feeling.
// ============================================================================
static double g_povNext = 0.0;
