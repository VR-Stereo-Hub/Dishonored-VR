// core/gfx/aer.cpp - rung 2 of the stereo ladder: AlternateEye rendering (VR-39).
//
// IMPLEMENTED as a port of BioShock Remastered VR's AER, on top of the machinery
// SequentialReentry already proved here. The method object is reentry's own
// (core/gfx/reentry.cpp, `SequentialReentry(true)`): the same tag ring, the same
// present-side pairing and capture, the same runtime SR path. What differs is
// the game side (game/dishonored/scene_draw.cpp):
//
//   reentry  every gameplay tick draws twice: pass 1 left, pass 2 right.
//   aer      every gameplay tick draws ONCE, and the eye alternates. A left
//            tick is pass 1 as it always was; a right tick is pass 2's setup
//            (the seam writes +1 through the thread latch, the tag rides the
//            ring, the right eye culls with its own view state) with no pass 1
//            before it.
//
// BRVR -> DISHONORED, piece by piece:
//   eye owned by the game thread by strict alternation   -> g_sdAerNext in the
//       scene-draw stub (the producer index there is g_eyeWr & 1)
//   eye FIFO game -> render, one tag per frame            -> the reentry ring
//       (one push per draw, one pop per present)
//   ~2 presents per XR submit (XR_SubmitPair)             -> presents_per_tick()
//       stays 2: the runtime's pair pacing holds one XR frame across the pair
//   DeltaClamp: one world advance per eye pair            -> delta_clamp.cpp,
//       WorldInfo.TimeDilation (off by default, [Stereo] DeltaClamp)
//   pair lock / latched pose per pair                     -> not ported: the
//       runtime already submits each eye with the pose generation it was
//       rendered from (per present), so a right eye one tick newer is
//       reprojected from its own pose; the clamp removes the world's travel
//
// WHAT IS HELD OFF WHILE IT RUNS: the c5 arbitration and the late-tag and
// single-tag repairs. All three are built on reentry's within-tick invariant
// (pass 2's camera exactly one IPD right of pass 1's, same tick); under aer the
// two presents of a pair are two ticks apart and the head moves between them,
// so the ring's order is the claim, exactly as BRVR's FIFO is. The player's
// settings are restored when aer stops.
//
// THE TRADE, stated before anyone measures it: aer renders one scene per tick
// instead of two, so ticks per second can rise, but each eye refreshes at half
// the tick rate and a pair is two ticks. With the clamp OFF the second eye is a
// tick later (moving things ghost); with it ON the world advances once per
// pair (no ghosting) but the game logic runs twice per pair. Whether that nets
// out faster than reentry on this game is exactly what the A/B is for
// (docs/dishonored/PERFORMANCE.md).
//
// ACCEPTANCE (ROADMAP S2a): `stereo aer` accepted; the `aer: beat` line reads
// L/s == R/s with broken near 0; the stereo beat line reads L/s == R/s ==
// out/s / 2; the clamp's beat line reads R world advance near 0 and L near
// twice the real dt, world/real at the base, INTEREYE near 0 with the clamp on
// (and non-zero while walking with it off); then the headset: fusion, no swim
// on head turns, and a verdict on half-rate per eye.
#include "core/gfx/stereo.h"

namespace dvr::stereo {

IStereo* create_aer() { return create_alternate_eye(); }

} // namespace dvr::stereo
