# BRIEF: VR-33, the draw-scoped bone palette

**Status:** rung 2 shipped, one fix awaiting its first headset run.
**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`.
**Purpose:** an external review of the reasoning, not of the code style. The
question worth arguing with is whether the conclusions below are actually
supported by the runs that produced them.

Written for a reader who does not know this project. Terms: "the proxy" is our
`d3d9.dll`; "the palette" is the block of skinning matrices the game uploads to
vertex-shader constant `c6`; "the split" is the existing mesh surgery that cuts
the hands from the arms; "rung" is a step on a deliberately staged ladder.

---

## 1. What VR-33 is trying to do

Put the player's hands where the VR controllers are, in a 2012 Unreal Engine 3
game (Dishonored, build 9099, 32-bit, D3D9) that has no VR support. The mod is
a `d3d9.dll` proxy; the game renders natively and we intercept.

Prior work established the presentation: the hands are already cut free of the
arms at the wrist, the arms are hidden, and the cut is capped. What was missing
was any mechanism to MOVE a hand.

---

## 2. The starting position, and the finding that had to be retracted first

The session opened with the previous session's conclusion recorded in
`ENGINE_NOTES.md`: **"SkelControls are NOT evaluated on this build"** - meaning
UE3's animation-control objects are never ticked, so writing to one cannot move
anything, and the "native" route to moving a hand was closed.

That conclusion rested on a scan of `GObjects` (UE3's global object array)
sampling each `SkelControl`'s `ControlTickTag` a second apart. It reported
"zero of 64 SkelControl objects advance".

**The scan was truncated.** Its comparison table was `uint8_t* curObj[64]` and
its loop condition was `for (i = 0; i < onum && curN < 64; i++)`. `curN`
increments exactly when the SkelControl counter does, so the sweep **stopped as
soon as the table filled**. The "64" read as a population was the array's
capacity. The walk never reached past the 64th SkelControl, and the first
entries of `GObjects` are the earliest-constructed objects - the least
representative slice available for a question about the player's live view
model.

So the finding was "none of the first 64 advanced", not "none advanced". It was
retracted, the scan fixed (full sweep, 1024-entry table, and the population it
ran over - walked / tracked / untracked - printed on the same line as the live
count), and re-run.

### Test 1: the fixed sweep

Run with `[Hands] Enabled=1` and `[Mode] GamepadOnly=0` - the hands subsystem
LIVE, which every earlier measurement had lacked. 34 consecutive one-second
samples, all identical:

```
66 SkelControl object(s) out of 103117 GObjects entries walked (FULL sweep),
66 tracked for comparison, 0 NOT tracked, 0 advanced their tick tag in the
last second. hands=1 gamepadOnly=0
```

**Outcome:** the conclusion survives, now as evidence. No SkelControl is
evaluated on this build. The old 64-entry table had missed exactly two objects
out of 66, so the retracted reading was right by luck.

**Cost measured:** the sweep stalls the game thread **505-520 ms once per
second** (`out/idle`, ~46 display slots at 90 Hz). Unplayable while armed.

*Reviewer's question: is "0 of 66 over 34 samples" sufficient to close the
native lane, or is there a state (in a menu, mid-animation, a specific pawn)
where a control would tick that this run would not have entered?*

---

## 3. The chosen route: a draw-scoped bone palette

With the engine-side lane shut, the remaining place to move a hand is where the
draw consumes it. The game uploads 48 skinning matrices to `c6` (`c6 x144`, 3
float4 rows each, row-major 3x4, translation in `.w`).

**The mechanism:** apply one common rigid delta `D` to every skinning matrix of
one hand, and draw that hand under its own palette.

**Why finger animation survives** - this is the load-bearing claim:

```
sum_i w_i (D M_i) v  =  D (sum_i w_i M_i v)
```

A common rigid transform commutes with the weighted skinning blend. The engine
keeps animating `M_i`; `D` only moves the result. This is not a frozen hand.

**Its honest limit, stated before the first line was written:** this is a
GPU-side edit. The engine's own transform is untouched, so **weapons, muzzle
effects and firing aim do not follow**. Confirmed in testing: the crossbow
stays where the engine put it. The weapon half of VR-33 needs a separate
engine-side mechanism.

### Implementation, three pieces

1. `vs_const_hook.cpp` caches the `c6` block the game uploads, ahead of every
   rewriting path (so it records what was requested, not what we left behind).
2. `MsDraw` (`hands/mesh_split.cpp`) issues **one draw per hand class** instead
   of one merged draw, setting that class's palette before each and restoring
   the game's own block after. D3D9 constants are current state, not one-shot.
3. `mesh_split` carries the delta and the counters.

The per-class index ranges (`MS_CLS_HAND_A` / `_B`) already existed from the
split. That is why this was a join of two existing things rather than new
machinery.

**A correction made during the session:** an earlier reading claimed the
30.70/71 "hand drive" in `vs_const_hook` already did this and worked. It
exists, but its pose source `RtdSnapshot` lives in `src/legacy/` and stubs to
`return false` in shipped builds - that path has been dead. The new backend has
its own source.

### Test 2: rung 1, does per-class scoping work

15 uu (later 20) applied on hand class A only; class B untouched as the control.

**Outcome: pass.** Log: `PER-CLASS | 2 range(s) | delta (0.0 12.0 0.0) uu on
class A | c6 x144 (48 bones) | 477,616 per-class draws, 0 with no c6 cached`.
Tester: the left hand left the crossbow, the right stayed correctly on the
sword hilt. This also establishes **class A = left, class B = right**.

---

## 4. Measuring the palette's basis

The palette's axes were unknown. Guessing them is how a wrong sign ships as a
fix, so they were measured.

### Test 3: the timed axis sweep - TWO RUNS, NEITHER ADMITTED AS EVIDENCE

The delta walked axis 0 -> 1 -> 2 on a 3-second timer. Tester reported the
three directions.

- Run A: `left, down, forward`
- Run B: `down, forward, left` - with the tester noting they may have caught
  the cycle mid-phase.

These are the same cycle entered one step in. **But nothing in either run could
prove that rather than a changed basis**, and the tester said so before it was
acted on. Both were discarded as evidence.

*This is the single best decision in the session and it was the tester's, not
the agent's.*

### Test 4: the tester-stepped axis probe

Rebuilt so the tester starts the cycle: one keypress advances
`rest -> axis 0 -> axis 1 -> axis 2 -> rest`, starting at rest so nothing moves
until asked. No phase to infer.

**Outcome:** log shows `REST, AXIS 0, AXIS 1, AXIS 2` at a steady `hmdYaw` of
about -35 deg. Tester: **`left, down, forward`** - agreeing with run A.

| palette axis | direction |
|---|---|
| 0 | LEFT |
| 1 | DOWN |
| 2 | FORWARD |

`left x down = -forward`, so the frame is **left-handed** - which is what UE3
should give, and is the main reason to believe the reading rather than suspect
an artifact. Against OpenXR (x right, y up, z BACKWARD, forward = -z) the map
is a componentwise negation: `T = -k * v`.

**Two hotkey bugs found on the way**, both of which had produced a false "the
probe did nothing":

- `MsTick` was called from inside `DcTick`, BELOW its `if (!g_dcOn) return;`.
  Switching the draw census off silently killed the mesh split's whole tick and
  put the arms back on screen with nothing in the log naming the cause.
- The bare-numpad hotkey blocks did not check their modifier, so one
  `CTRL+Numpad5` press both stepped the probe and cycled the draw census.
  Additionally CTRL is the game's block, so a CTRL chord makes the character
  act while the diagnostic is pressed. The probe moved to unmodified F6.

### Test 5: is the frame view-aligned or world-aligned

Only "down" is yaw-invariant, so axes 0 and 2 could have been world-aligned
axes that merely looked view-aligned from one facing. Tester ran the cycle,
turned ~90 deg with the right stick, ran it again.

**Outcome:** directions unchanged relative to the tester. The log confirms
`hmdYaw` stayed at about -15 deg throughout - the turn was stick-only, so the
game camera rotated while the head did not. **The frame rotates with the game
camera.**

---

## 5. Rung 2: the controllers drive the delta, and the two wrong turns after it

Each hand's offset taken relative to the HEAD, mapped through the measured
basis, scaled by uu/m. Relative drive: `D = 0` means "wherever the engine put
the hand", so it moves the hand BY the controller's travel from a captured
neutral. It does not place the hand AT the controller - absolute placement
needs the hand's position in palette space, and a skinning matrix's translation
column is not the bone's position.

### Test 6: rung 2 in the headset

**Outcome, mixed and very informative:**

- **All three axes now track correctly.** The basis is right.
- A **constant rotation** of the mapping, ~30-45 deg, worse on the right hand;
  resetting the neutral with the hand at rest made the left hand accurate.
- **Hands still counter-rotate with head movement, horizontally only.**
  Vertical is correct.
- Weapons do not follow (expected, see section 3).

### Wrong turn 1: the yaw-residual hypothesis

Hypothesis: the game camera carries a body/stick yaw the head does not, leaving
a residual `phi = camera yaw - head yaw` between the frame the offset was
computed in and the frame the palette applies it in. That single error predicts
BOTH a constant rotation (phi at capture) and drift under head turns (phi
changing), and predicts vertical is unaffected - which matched.

`g_bodyYawU` looked like the source and turned out to be a leftover: declared
and never written anywhere in the tree. `g_viewYawRad` - the yaw actually
written to the camera - was used instead. Sign convention unknown, so it
shipped as an A/B: **left hand +phi, right hand -phi**, the stable one to name
the sign.

**Outcome: both hands swivelled. Hypothesis killed - and the run measured why.**

```
phi=144.6 deg (camera 142.2 - head -2.3)
phi=144.7 deg (camera 109.3 - head -35.4)
phi=144.3 deg (camera 189.1 - head +44.8)
phi=144.8 deg (camera  73.6 - head -71.2)
```

**phi held 144.0-145.6 deg while the camera swung 73.6 -> 189.1 and the head
-71.2 -> +44.8.** The camera tracks the head **1:1**; phi is the fixed offset
between UE's yaw origin and XR's, not a body yaw. Reverted.

### Wrong turn 2: an instrument that could not fail

Next attempt: log three candidate frames (world / head-relative / yaw-only) as
raw XR metres with the controller held still, and take whichever stayed flat.

**This probe was tautological and its output was withdrawn.** With the
controller physically still, `w = hand - head` is constant *by construction* - a
head that rotates barely translates - so "WORLD is flat" was decided by the
arithmetic before the run started. The other two candidates move only because
they are `w` rotated by the head yaw. The measured spreads say exactly that and
nothing more: WORLD 0.28 m across a 158 deg swing (about a neck-pivot
translation), against HEAD 1.19 m and YAWONLY 1.07 m.

This violates a rule this project already had in writing - *an instrument that
cannot fail its own hypothesis is not evidence* - and the code carries the
withdrawal as a comment rather than being deleted.

### Test 7: the frame question, with the palette actually in the loop

Correct form of the question: apply a KNOWN delta and watch where the hand goes
as the head turns. 20 uu on axis 0 (left) on the left hand only; hold both
controllers still; turn the head.

- If the offset **stays on the same side of the face** -> the frame rotates
  with the head.
- If the offset **swings round to a fixed spot in the room** -> the frame is
  body-fixed and head turns do not move it.

**Outcome: the offset stayed offset to the tester's left.** The palette frame
**rotates with the head**.

---

## 6. The conclusion, and the fix now awaiting a run

Combining tests 5, 6 and 7: the palette frame is the head/camera frame, and the
camera follows the head 1:1. That makes rung 2's *transform* correct - and
locates the bug in its *neutral*.

**The neutral was stored in head-space coordinates.** So the zero point itself
rotated with the head. With the controller still and the head turning, the
difference `v - v0` changed, and the hand swung to chase it. And the head yaw
at the moment of capture became a fixed rotation of the whole mapping.

One error, both symptoms - the constant tilt and the head drift - and it
predicts vertical being unaffected, which is what was reported.

**The fix:** store the neutral in WORLD space, subtract in world, and rotate
the difference into the head frame:

```
D = -k * R_head^T * (w - w0)
```

World effect is then `R_pal * D = R_head * R_head^T * (w - w0) = w - w0`:
constant while the controller is still, however the head moves. A stick turn
rotates the frame without rotating the head, so the hand rides round with the
body - which is what it should do.

**This is built and installed but HAS NOT BEEN RUN.** It is a prediction, not a
result.

*Reviewer's questions, in priority order:*

1. *Does the world-space-neutral fix actually follow from tests 5-7, or is
   there a fourth reading of the frame that also fits and has not been
   excluded?*
2. *Test 7 was a single perceptual binary. Is it strong enough to carry the
   conclusion, given tests 3A/3B showed perceptual reports being
   misattributed?*
3. *The relative drive never places the hand AT the controller. Is continuing
   to refine a relative drive the right use of runs, or should absolute
   placement (which needs bone positions in palette space, currently
   unavailable) be attacked first?*
4. *The weapon half has no mechanism at all. Does the palette route earn its
   place if weapons must be solved engine-side anyway?*

---

## 7. Method notes worth reviewing separately

What worked, repeatedly: **A/B with a control**. Every clean result this
session came from moving one hand and leaving the other alone. Every muddy one
came from an instrument with no control, or one whose answer was predetermined.

What cost the most runs: **assuming a frame relationship instead of measuring
it**. Three separate wrong turns (head-space neutral, the phi rotation, the
tautological probe) are all the same mistake in different clothes.

The tester runs the game and reports; the agent builds, installs, arms the
diagnostics default-on in the installed ini, and reads the log. Two false
"nothing happened" reports came from hotkey collisions rather than from the
thing under test, which is an argument for logging every keypress a diagnostic
consumes.

---

## 8. Where the code is

| Piece | File |
|---|---|
| The palette backend, the probes, the drive | `src/game/dishonored/hands/mesh_split.cpp` |
| Globals and the recorded basis | `src/mod/state/55_game_dishonored_hands_mesh_split.inc` |
| The `c6` cache | `src/core/framework/vs_const_hook.cpp` |
| Hotkeys (F6) | `src/core/input/hotkeys.cpp` |
| Ini keys | `src/core/config/config.cpp` |
| The findings | `docs/dishonored/ENGINE_NOTES.md` |

Nothing has been merged to `VR-Main`.
