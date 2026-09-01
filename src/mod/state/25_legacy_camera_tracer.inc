// State chunk 25 for legacy/camera_tracer - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

static uintptr_t g_watchAddr   = 0;
static volatile LONG g_traceCount = 0;
struct TraceHit { uint32_t eip; uint32_t ret[4]; };
static TraceHit g_hits[24];
static volatile LONG g_hitN    = 0;
static int      g_traceState   = 0;   // 0 idle, 1 armed, 2 reported
// 32.24: log each DISTINCT instruction the moment it appears, instead of
// batching every finding into a report at the end of a window. That batching
// has now cost three separate runs tonight - the Blink probe reported after
// the user stopped holding, the frame dump reported after they quit, and this
// tracer armed and then lost a race with the exit by a couple of seconds.
// Emit the payload as it is discovered. A diagnostic that only pays out if the
// operator waits for a timer is a diagnostic that throws away good runs.
static LONG     g_hitLogged    = 0;
static double   g_traceStopAt  = 0.0;
static uint32_t g_traceArmFrame = 0;

// set/clear DR0 on every thread in this process
// 32.22: RW bits in the DR7 nibble - 01 = write only (0xD with LEN=11),
// 11 = read OR write (0xF). The Blink question needs the second: we proved
// nothing we WRITE to +0x060 reaches the teleport, so the thing to find out is
// whether anything READS it at the moment you release. A reader would mean the
// value is used and our write was mistimed; no reader at release means it is
// display-only and the destination is recomputed, which is what the aim-detour
// evidence already suggests.
static uint32_t g_hwNibble = 0xD;

// ============================================================================
// Stage 7.0 - FIRE-AIM TRACER (motion-controls expedition, step 1)
//
// Goal: find the native code that computes a shot's start/direction, so we can
// substitute the hand ray (BioShock-style). Method: when the player pulls a
// trigger, arm hardware READ-watchpoints (DR0+DR1) on the live camera's two
// cached POV locations for ~250 ms and record every instruction that reads
// them. Readers present during pistol/crossbow/power use but absent during
// sword swings are the aim path. EIPs get disassembled offline (fixed base
// 0x400000). READ-ONLY diagnostic: observes, never modifies.
// ============================================================================
// DIFFERENTIAL design (Stage 7.1): a per-shot fire read is a needle in the
// haystack of per-frame camera/aim ticks. So we continuously learn the
// BACKGROUND set of readers during normal play (short samples between shots),
// then on a trigger pull report only readers NOT already in the background - 
// the discrete fire event. Four watchpoints: camera ORIENTATION (basis X row
// +0x50, right row +0x60 = the fire DIRECTION source) and POSITION (POV caches
// +0x90, +0xC4 = the fire ORIGIN). READ-ONLY: observes, never modifies.
static PVOID g_fireVeh = NULL;
