# Plan: widen the hand tuning range, and make the laser follow the hand (VR-57)

2026-09-12, for review before implementation. Branch `claude/vr-57-laser-to-bolt`,
off `claude/vr-57-crosshair-on-hand-ray` at `1af1527e`. Nothing is implemented.

Two asks, from the tester:

1. **The hand tuning control runs out of range.** The rotation trim is clamped at
   +-45 deg and theirs sits pinned at `TrimRRX -45.00` and `TrimRRY +45.00`, with
   46 clamp events in one session.
2. **The laser should follow the HAND, not the controller**, and sit where the bolt
   actually comes from, so that tuning the hand moves the laser with it and sighting
   down the weapon works.

## 0. A premise I got wrong, retracted

I built a per-weapon rotation on the theory that the sword and crossbow needed
different corrections because one shared per-hand term could not fit both. That was
wrong and is fully reverted (source, binary and ini).

**The weapons are correctly positioned in the hands already.** The trim is per hand
and per axis, with separate position and rotation modes, so the existing controls
are sufficient in KIND - they are only insufficient in RANGE. Nothing needs to move
a weapon independently of the hand that holds it, and a per-weapon term would have
introduced a second correction competing with the first.

What survives from that work is one fact worth keeping, because the laser design
below depends on it: the correction applied at the weapon draw is expressed in the
PALM's own frame, so the palm origin is that frame's origin.

## 1. The range (ask 1)

The clamp is `mesh_split.cpp`, `const float lim = rot ? 45.0f : 0.25f`, and it is
deliberate: it logs *"A correction this large is a wrong grip calibration rather
than a trim - press SHIFT+F7 again."*

The evidence says 45 is genuinely too small for this tester's hold: two axes
saturated simultaneously, which is a correction converging on a value it is not
allowed to reach rather than a wandering search. But the clamp's advice may still be
right that the GRIP is the better thing to fix, and raising the bound would hide
that signal.

Candidates, for review:

| Option | For | Against |
|---|---|---|
| Raise the rotation bound (90 or 180) | Direct, one constant, immediately unblocks the tester | Removes the only signal that a grip calibration is wrong; a trim doing a grip's job is a number nobody can interpret later |
| Keep 45, re-run SHIFT+F7 first | Respects the existing design; a correct grip should leave a small residual | SHIFT+F7 solves BOTH hands from one snapshot, so it disturbs the hand the tester says is fine |
| Raise the bound AND keep the clamp message, warning above 45 | Unblocks without losing the signal; the log still says a large value means the grip is suspect | Two mechanisms for one thing |
| Per-hand grip re-solve | Fixes the cause rather than the symptom | Does not exist yet; SHIFT+F7's one-snapshot behaviour would have to be split |

My inclination is the third, with the warning threshold at the old 45 so the
existing advice keeps printing, but this is exactly the kind of call the review
should make. **Whatever is chosen, the effective resolved value must be logged**,
because an ini key that exists beats every compiled default and this project has
lost two sessions to that.

## 2. The laser (ask 2)

Today the ray comes from the runtime's AIM pose (`aim_ray.cpp` ->
`input_hand_aim_sample`), so it tracks the physical controller and is completely
independent of the hand trim. Tuning the hand moves the weapon and leaves the laser
behind. That is the whole complaint.

The hand's drawn orientation is `O_C * G * Trim.r` (`hand_frame.h`, `palm_target`):
the controller orientation in camera space, the calibrated grip, then the trim. So
"the laser follows the hand" means deriving the ray from that product instead.

### Option A: conjugate the trim onto the ray, on the present lane (preferred)

Rotate the aim direction by the same rotation the trim applies to the hand:

```text
dir' = (O_C * G * Trim.r) * (O_C * G)^-1 * dir
```

Every term is already available on the present lane - the pose, the stored grip, the
trim - so there is no lane bridge, no draw dependency, and no new space conversion.
With `Trim.r` at identity it is exactly today's behaviour, which makes the A/B
trivially honest.

Properties worth stating before a run: it moves the laser by exactly the angle the
hand moved, it keeps working when the weapon is stowed, and because the native fire
hook consumes this same ray, **the bolt follows automatically** and the laser and
bolt cannot drift apart.

### Option B: read the drawn palm frame from the draw

`g_mpPalmTarget[hand]` and `g_mpPalmTargetOk[hand]` already publish the drawn palm
frame with the trim baked in. More directly "what was actually drawn", but it is in
DRAW space on the render thread, is per eye, and is only written on the rotation
path - the existing log already warns that the trim "rides the rotation path and
rotation has placed 0 draws so far". A laser that silently stops when rotation is
refused is worse than one derived arithmetically.

**Recommend A, with B's published frame used only as a cross-check** in the log: if
the two disagree beyond a small bound, one of them is wrong and the line should say
so rather than a single number being believed.

### The origin, and what "aligned with the bolt" requires

Direction and origin are separate questions and only direction is settled above.

The laser is drawn from the controller's aim-pose position; the bolt launches from
the engine's own spawn point, measured at 0.5 to 0.8 m from the controller with 0.27
to 0.62 m of that transverse. For sighting down the weapon, the direction is what
matters - but a laser whose origin is the controller while the bolt leaves the
muzzle will still not look like it emerges from the weapon.

Options: leave the origin at the controller (simplest, direction-only fix); move it
to the palm (available, no capture needed); or capture a muzzle offset in the palm
frame once per weapon and use that (most faithful, needs a calibration key and
per-weapon storage - the thing I was wrong to build for rotation, but which has an
honest justification here because a muzzle position genuinely is per weapon).

**Question for review: is direction-only sufficient for the stated goal, or does the
origin have to move as well?** The tester's words were about sighting down the
crossbow, which is a direction test, so direction-only may be the whole fix.

## 3. What must not regress

The hands and the crouch motion controls must never stop working - the original
author's rule, paid for. So:

* `Trim.r` at identity must reproduce today's ray bit for bit, and the A/B lever
  makes that checkable rather than asserted.
* The native fire hook consumes `fire_frame()`; if the ray changes, the bolt changes.
  That is the point, but it means this is NOT a visual-only change and cannot be
  reviewed as one.
* Blink and the legacy MotionAim path read the GRIP pose separately and must be left
  alone, or tuning the hand would silently move Blink.
* One behavioural change per build. The range and the laser are two changes and
  should be two builds, in that order, because the range unblocks the tester
  immediately while the laser needs a headset verdict.

## 4. How it gets verified

The existing `ShotProbe` already measures the bolt's launch against the visible dot
and reports the miss at the dot's plane, the signed residuals and the populations.
With the laser hand-derived, that instrument answers the real question directly: the
miss should stay where it is now while the laser moves with the hand.

The perceptual half is the tester sighting down the crossbow and reporting whether
the dot sits on the barrel line. Tolerance is not set in advance here because this
is an alignment-by-eye task, but the probe's number must not get worse: a laser that
looks right while the bolt stops agreeing with it is a regression, not a fix.

## 5. Questions for review

1. Which range option, and if the bound is raised, to what, and does the clamp keep
   its grip warning above the old threshold?
2. Option A or B for the laser derivation, and is the cross-check worth the code?
3. Direction-only, or does the laser's origin have to move to the muzzle too? If the
   latter, is a per-weapon muzzle capture justified here given a per-weapon rotation
   was not?
4. The conjugation in A assumes the trim's rotation composes on the right of
   `O_C * G`, matching `palm_target`. Worth confirming against `hand_frame.h` rather
   than trusting this plan, since a wrong side would rotate the laser about the
   wrong axis and look like a calibration error.
5. Should the two changes ship as two builds as proposed, or is the range change
   small enough to fold in with the laser?
