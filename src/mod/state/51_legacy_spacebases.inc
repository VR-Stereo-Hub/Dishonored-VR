// State chunk 51 for legacy/spacebases - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// 30.83 - THE SPACEBASES ORACLE.
//
// Re-opening the bone-bank path, because the 30.62-65 test that closed it was
// invalid in three ways that the bioshock-vr author's own engine notes name:
//
//  1. WRONG WRITE POINT. He writes "per pass-1 camera dispatch (post-tick,
//     inside the draw)" and states plainly that "a one-shot write cannot
//     survive" because the bank restamps in under 2 s. We wrote from
//     ProcessEvent - during the tick, before the pose is consumed - so the
//     evaluator had every opportunity to stamp over us before the draw.
//  2. WE NEVER CHECKED THE WRITE LANDED. He uses poke-and-read-back oracles on
//     both banks. We judged "writes do nothing" from the screen alone, which by
//     this project's own rule (§10: a test that cannot report its match count
//     is an empty test) was never a negative result.
//  3. POSSIBLY THE WRONG COMPONENT. He resolves it by calling
//     GetFirstPersonAttachment on the pawn; we guessed pPlayerMesh by asset name.
//
// And we now hold an oracle he had to work much harder for: the drawn pose
// arrives at c6, so this can answer itself objectively instead of asking the
// user whether something looked different. Writes are A/B'd half a second on,
// half a second off, logging the bank read-back and the c6 translation in both
// phases. Three outcomes, all decisive:
//   read-back fails      -> wrong bank/component, not a timing problem
//   read-back ok, c6 flat-> the renderer genuinely does not read this bank
//   read-back ok, c6 moves -> the path is ALIVE and only ever needed timing
// ============================================================================

static float g_sbC6w[3] = {0,0,0};     // the WEAPON-size upload, watched too

// Catch-all: instead of guessing which upload a bank feeds, watch EVERY
// skinned upload size and report the range each one moved over the run.
// Idle animation drifts a unit or two; a 40-unit write cannot hide in that.
static float g_sbMin[86], g_sbMax[86];
static UINT  g_sbSeen[86];

// ==== 32.83: change the resolution the way the ENGINE does ================
// Everything so far tried to make the game accept a size from outside: the
// window, the work area, the client rect, the present parameters, the adapter
// mode list. The mode list was the clearest failure of the lot - the game
// enumerated our 3200x1800 at slot 125 and then asked for 1920x1080 anyway.
// It is not reading the size from any of the places we can reach.
//
// But UE3 has a function whose entire job is this, and it does it correctly
// and atomically: viewport, render targets and backbuffer together.
//     PlayerController.ConsoleCommand("setres 3200x1800w")
// We already hook ProcessEvent, walk GObjects, resolve UFunctions by name and
// hold a live PlayerController. So ask the engine in its own language rather
// than negotiating with Windows on its behalf.
//
// Parameter block is UE3's standard shape for this signature:
//   FString Command      { TCHAR* Data; INT Num; INT Max; }   12 bytes
//   UBOOL   bWriteToLog                                        4 bytes
//   FString ReturnValue                                       12 bytes
// TCHAR is wchar_t - UE3 is Unicode - and Num/Max COUNT THE NULL.
typedef void (__thiscall *PFN_ProcessEventCall)(void*, void*, void*, void*);

static uint8_t* g_fnConsoleCmd = NULL;
static bool     g_peReentry = false;
