// State chunk 43 for game/dishonored/hands/skelcontrol - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// 32.8 - BLINK PROBE: which field holds the teleport target?
//
// Powers still aim down the VIEW, which is the last place the mod contradicts
// itself now that the hands are on the controllers. Before writing anything,
// find out WHAT to write - the same order that settled the fire in one dump
// instead of a week of theories.
//
// The trap this is built to avoid: moving the Blink MARKER is easy, and may
// do nothing. If the destination is retraced from the camera at the moment you
// release, we would be dragging a decal around while the teleport still goes
// where you looked. So this does not look for "the marker". It looks for every
// FVector-shaped field on every Blink-ish object and asks two questions of
// each, over a window while the user holds an aim and sweeps it around:
//
//   1. does it MOVE?                    (a static field is not a target)
//   2. does it sit ON THE VIEW RAY?     (dot of (v - camPos) with view-forward)
//
// A field that is the live aim result scores ~1.0 on the dot with a plausible
// distance. A field that merely correlates - the camera position, a bone, some
// UI anchor - fails one or the other. Read-only: this probe writes nothing.
// ============================================================================
// 32.10: THREE things made the first run unable to answer. All measured, not
// guessed - the log named its own candidates and 11 of the 12 were dead:
//   1. The list filled with ARCHETYPES. UE3 keeps template objects inside
//      Packages, and they carry no live state. Only ONE candidate reached
//      "<- Level <- World" (the real PowerBlink on the real pawn); the other
//      eleven were Package-owned templates and Kismet SeqAct_Teleport nodes.
//      A cap of 12 spent on those is a test of nothing. Filter on the Outer
//      chain reaching a Level/World instead, and raise the cap.
//   2. The scan stopped at +0x3a0. A UE3 component is bigger than that.
//   3. Events were only captured when the dispatch object WAS a candidate -
//      but a power's script events fire on the PAWN and the CONTROLLER, so
//      "no events" was guaranteed regardless of what happened. Watch those
//      too, name-filtered.
// Plus a live progress line, so a null result is trustworthy rather than
// indistinguishable from "the window ran while nothing was held".
#define kBpMax   40
