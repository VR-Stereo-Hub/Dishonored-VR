// State chunk 23 for game/dishonored/ue3/uobject - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static int  g_probeRuns = 0;
static bool g_f9WasDown = false;

// ---------------------------------------------------------------------------
// Last line of defence for the object walks. Every read above is VirtualQuery
// gated, but UE3 can free an object on another thread BETWEEN the check and
// the use - the 2026-08-06 crash was exactly that, FpCollect touching a
// component that died mid-walk. MinGW has no __try, so: a vectored handler
// that, ONLY while the game-thread walk section is active, converts the
// access violation into a longjmp back to the frame boundary, where we drop
// every cached pointer and rescan. Any other thread or any other time, the
// handler stays out of the way.
// ---------------------------------------------------------------------------
static jmp_buf        g_walkJmp;
static volatile DWORD g_walkTid = 0;         // game-thread walk (per frame)
static jmp_buf        g_walkJmp2;
static volatile DWORD g_walkTid2 = 0;        // render-thread dumps (keypress)
static volatile LONG  g_walkFaults = 0;
// 30.10: the bone probe faults deterministically and code inspection has not
// found it, so capture WHERE - the recovery log prints the faulting EIP and
// the address it tried to touch, plus &WalkVEH as an anchor so the EIP maps
// straight onto the objdump listing.
static volatile uintptr_t g_walkFaultEip  = 0;
static volatile uintptr_t g_walkFaultAddr = 0;
