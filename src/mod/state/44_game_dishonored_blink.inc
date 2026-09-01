// State chunk 44 for game/dishonored/blink - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

#define kBpOffN  504                    // offsets 0x20 .. 0x800, step 4
static uint8_t*  g_bpObj[kBpMax];
static uint32_t  g_bpIdx[kBpMax];
static void*     g_bpCls[kBpMax];
static uint32_t  g_bpLimit[kBpMax];     // readable offsets, resolved once at arm
static char      g_bpLabel[kBpMax][80];
static int       g_bpN = 0;
static double    g_bpUntil = 0.0;
static double    g_bpNextSample = 0.0;
static double    g_bpNextTell = 0.0;
static float     g_bpFirst[kBpMax][kBpOffN][3];
static bool      g_bpHave[kBpMax][kBpOffN];
static float     g_bpMaxMove[kBpMax][kBpOffN];
static float     g_bpDotSum[kBpMax][kBpOffN];
static float     g_bpDistSum[kBpMax][kBpOffN];
static uint32_t  g_bpSamples[kBpMax][kBpOffN];
// 32.11: the whole-window AVERAGE was the wrong statistic and it threw away a
// real hit. The live progress line reported "2 fields on the view ray, best
// dot 0.931" at 5 s remaining, and the final report then said NOTHING - because
// the user spent the first 15 s getting into position, and averaging a 5-second
// lock over a 20-second window buries it under the noise. Count HITS instead:
// samples where the field is essentially dead on the ray. 5 s of lock at 20 Hz
// is 100 hits; 15 s of not aiming contributes zero, and cannot dilute them.
static uint32_t  g_bpHits[kBpMax][kBpOffN];
static float     g_bpBestDot[kBpMax][kBpOffN];
static uint32_t  g_bpEvt[48];           // function FNames dispatched ON a candidate
static uint32_t  g_bpEvtC[48];
static int       g_bpEvtN = 0;

// ============================================================================
// 32.21 - NATIVE BLINK AIM DETOUR
//
// Located by hardware breakpoint (32.20) and disassembled off Dishonored.exe:
//
//   fn start 0x00bf52e0
//   bf5954  movss xmm3,[ebp-0x100]   ; blink RANGE
//   bf595f  movss xmm0,[ebp-0xc]     ; \
//   bf5964  movss xmm1,[ebp-0x8]     ;  > aim DIRECTION (unit vector)
//   bf5969  movss xmm2,[ebp-0x4]     ; /
//   bf5980  mulss xmm0,xmm3          ; direction * range
//   bf598c  subss xmm2,[eax+0x234]   ; minus a Z offset (surface pull-back)
//   bf5e29  mov [esi+0x60],edx       ; store into PowerBlink+0x60
//
// Writing +0x60 could never work (32.17 proved we own it and the teleport
// ignores it) because this function runs AGAIN at release and the caller uses
// the fresh result. So substitute the INPUT instead: overwrite the direction
// locals three instructions before they are consumed, and the range multiply,
// the trace, the pull-back and the destination all become the engine's own
// work along OUR ray. Nothing to race, nothing cached.
//
// PHASE 1 IS READ-ONLY. The detour installs and only logs - the game's own
// direction beside ours, and whether esi really is the player's PowerBlink at
// this point in the function. A mid-function detour that also changes
// behaviour on its first run gives you two suspects when it goes wrong.
// Substitution is a separate switch.
static bool     g_blkHookOn   = false;   // detour installed
static bool     g_blkAimDrive = false;   // and actually substituting
extern volatile LONG g_blkHookReq;
static volatile LONG g_blkHookHits = 0;
static volatile LONG g_blkHookMine = 0;
static double   g_blkHookTell = 0.0;
static float    g_blkGameDir[3] = { 0, 0, 0 };
static float    g_blkOurDir[3]  = { 0, 0, 0 };

extern "C" uint32_t g_blkRet = 0;        // where the stub jumps back to
extern "C" void BlinkAimStub(void);

static bool HandRelFull(int hand, float* rel, float* relPos);
static void MaimDirFromView(float viewYaw, float viewPitch, const float* rel,
                            float* dir);

// ============================================================================
// 32.26 - THE DESTINATION DETOUR, on the instruction we know executes
//
// The read-watch (32.24) caught the release: a caller chain
// 00abb1fb -> 00b507e6 -> 00b51691 -> 00c19b76 -> 0x00bfc9b7 READS +0x060 and
// range-checks it (sqrt of the delta, fcomip against [eax+0xe4], jbe away).
// So the stored destination IS what the teleport consumes - which finally
// explains 32.17. We were winning the race measured across FRAMES while
// losing it across INSTRUCTIONS: at release the engine re-traces, stores a
// fresh +0x060, and reads it back inside one native sequence with no
// ProcessEvent dispatch in between for our write to land in.
//
// So stop writing from outside and write from INSIDE that sequence. The three
// stores are at bf5e29 / bf5e44 / bf5e4c; the very next instruction is
//     bf5e4f  8d 85 30 ff ff ff   lea eax,[ebp-0xd0]     (6 bytes)
// which is a clean detour site immediately after the destination lands and
// before anything reads it. esi is the PowerBlink there - the same register
// the stores use.
//
// Unlike 0xbf595f (32.21, fired zero times) this address is PROVEN to execute:
// the write-watch caught 200 hits on it.
//
// We keep the engine's own traced DISTANCE and only swap the direction, so
// range checks and collision pull-back stay meaningful.
static bool     g_blkDstOn = false;
extern volatile LONG g_blkDstReqUI;
#define g_blkDstReq g_blkDstReqUI
static volatile LONG g_blkDstHits = 0;
static volatile LONG g_blkDstMine = 0;
static double   g_blkDstTell = 0.0;
static float    g_blkDstWas[3] = { 0, 0, 0 };
static float    g_blkDstNow[3] = { 0, 0, 0 };

extern "C" uint32_t g_blkDstRet = 0;
extern "C" void BlinkDestStub(void);

// 32.30: also rewrite the STACK LOCALS, not just the component fields.
// Disassembly of the store block:
//     bf5e21  movss [ebp-0xc],xmm3      ; original X
//     bf5e29  mov   [esi+0x60],edx      ; -> component
//     bf5e30  movss [ebp-0x8],xmm1      ; original Y
//     bf5e38  movss [ebp-0x4],xmm0      ; original Z
//     bf5e40  lea   edx,[ebp-0x30]      ; ...and the frame keeps being used
// We were overwriting the component copy and leaving [ebp-0xc/-0x8/-0x4]
// holding the ORIGINAL head-aimed point, which the rest of the function goes
// on to pass to later calls. That is the second marker the user sees sitting
// where they are looking: one visual fed from the component (ours, on the
// controller) and another fed from the frame locals (the engine's, on the
// head). Fix both and they agree.
// ---- 32.50: THE REAL FIX - aim the trace at its SOURCE ---------------------
// Hiding the engine's head-aimed marker (32.49) treats the symptom: the engine
// still traces along the view, we just refuse to draw the result. Redirect the
// direction the whole computation is built on and there is only ever ONE
// marker, on real geometry, in the right place.
//
// Disassembly of the function that owns the destination hook (starts 0xbf5520,
// the same frame - [ebp-0x4c] appears in both):
//
//   bf559e   call 0xbf52e0            ; returns EAX -> 3 floats: the aim vector
//   bf55a3   mov  ecx,[eax]           ; <-- HOOK HERE (5 bytes with the next)
//   bf55a5   mov  [ebp-0x4c],ecx      ;     dir.x
//   bf55a8   fld  [ebp-0x4c]          ;     length starts accumulating here
//   bf55ab   mov  edx,[eax+0x4]
//   bf55b0   mov  [ebp-0x48],edx      ;     dir.y
//   bf55b3   mov  eax,[eax+0x8]
//   bf55b6   mov  [ebp-0x44],eax      ;     dir.z
//   ...      fsqrt -> [esi+0x90]      ;     |dir|, used downstream as reach
//   bf5e29   mov  [esi+0x60],edx      ;     the destination we already patch
//
// This is on the function's straight-line entry path, not a branch - which is
// what killed the two earlier attempts (0xbf595f and 0xbf5d1a both sit on
// untaken branches and fired zero times). It is still logged and counted
// before it is trusted: a hook site has to be PROVEN executed.
//
// The trick that keeps the patch tiny: rather than rewrite three locals, hand
// the engine a different POINTER. The instructions that follow read [eax],
// [eax+4], [eax+8], so pointing EAX at our own three floats redirects all of
// them, and the length computation downstream then measures OUR vector.
// Which is why the substituted vector keeps the engine's MAGNITUDE exactly and
// changes only its direction - that length is the power's reach, and handing
// back a unit vector would collapse the blink to one world unit.
static volatile LONG g_blkDirHits = 0;
static volatile LONG g_blkDirMine = 0;
static double   g_blkDirTell = 0.0;
static double   g_blkDirLastMine = 0.0;
static float    g_blkDirBuf[3]  = { 0, 0, 0 };
static float    g_blkDirEng[3]  = { 0, 0, 0 };
static float    g_blkDirLen     = 0.0f;
extern "C" float* g_blkDirUse = NULL;     // the pointer the engine reads from
extern "C" uint32_t g_blkDirRet = 0;
extern "C" void BlinkDirStub(void);

// ============================================================================
// 32.31 - REDIRECT THE TRACE ITSELF, which is what should have been done
//
// The frame dumps now show ZERO world-space draws escaping the splice, so the
// marker's disappearance was never a stereo fault - M3.11 fixed a real bug,
// just not this one. The actual cause is what 32.26 does: it rewrites the
// DESTINATION after the engine has already traced along the VIEW. The distance
// is the view-trace distance, so a controller-direction point at that distance
// usually is not on a surface at all - it floats in air or sits inside
// geometry. A surface decal with no surface under it does not draw. That is
// exactly "visible when I look roughly where I point, invisible when I don't".
// And the head-aimed second marker is the engine's own, placed from its own
// untouched trace.
//
// Patching a result was always going to produce artefacts. Redirect the INPUT:
//
//   bf5cd1  movss xmm0,[ebp-0x40]   \  ray offset (direction * range)
//   bf5ce0  addss xmm0,[ebp-0x24]   /  + START  =  END
//   bf5d10  movss [ebp-0x30],xmm0   \
//   bf5d15  movss [ebp-0x2c],xmm1    > END point, passed to the trace
//   bf5d1a  movss [ebp-0x28],xmm2   /  (lea eax,[ebp-0x30] is pushed)
//   bf5d1f  call  0x64e7a0          ;  THE TRACE (SEH + stack cookie)
//
// Rewrite the END point at bf5d1a - keeping the engine's own ray LENGTH and
// swapping only the direction - and the trace runs down the controller ray.
// Hit point, pull-back, decal, destination and marker then all agree by
// construction, because they are all derived from one trace.
static bool     g_blkTrcOn = false;
static volatile LONG g_blkTrcHits = 0;
static volatile LONG g_blkTrcMine = 0;
static double   g_blkTrcTell = 0.0;
static float    g_blkTrcLen = 0.0f;

extern "C" uint32_t g_blkTrcRet = 0;
extern "C" void BlinkTraceStub(void);
