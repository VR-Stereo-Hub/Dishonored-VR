# VR-33 implementation plan

This is a build plan, not another investigation. Enough is measured; what
follows is what to write, in what order, with what acceptance and what stop
conditions.

---

## 1. What is settled

| Fact | How |
|---|---|
| `handAttachment_L/R_jnt` are children of `hand_L/R_jnt` | engine parent walks, validated edges, chains to root |
| Camera is on the spine/head branch; nearest common ancestor with either hand is `Root_jnt` | same |
| The two arms meet only at `Root_jnt` | same |
| All three hand controls are **distinct** `SkelControlSingleBone` objects | pointers 16D816A0 / 16D815B0 / 16D81790 |
| All three are live: `ControlStrength` 1.000, `ControlTickTag` 10, `BoneScale` 1.000 | control report |
| Each has a **non-empty** `NextControl` chain to another `SkelControlSingleBone` | control report |
| Control field offsets | name +0x5C, strength +0x64, boneScale +0xA0, tickTag +0xA4, next +0xAC |
| Skeleton indices are not palette slots | 54 and 56 against a 48-slot palette |
| The split's clip and caps are stable | stream fix confirmed, two runs |

**Still unknown, and the plan does not depend on it:** which bone each control
drives, the live item graph, and the native update ordering. Phase 1 below
determines the first of those *by moving something and looking*, which is
cheaper than any further static tracing.

## 2. Why the old SkelControl attempt failed, and what is different now

The 38.x attempt and the retry this session both concluded: world-space
translation works, world-space rotation does not - "9,000 writes/s into
`BoneRotation` outrun the recompute".

**That number is the clue, not the verdict.** 9,000 writes a second losing a
race does not mean the field is ignored; it means the writes land at the wrong
moment relative to when the control is evaluated. A control's value is consumed
during its own evaluation, so writing at ProcessEvent rate is a lottery on
whether the last write before evaluation was ours.

What we now have that the old attempt did not:

* The three controls are **separate objects**, so the left hand can be driven
  without touching the right or the camera.
* `ControlTickTag` is readable at +0xA4. UE3 stamps this when a control is
  evaluated, so **watching it change locates the evaluation moment** without
  any native hook.
* `SkelControlSingleBone` gives `bApplyRotation` / `bAddRotation` /
  `BoneRotationSpace` as explicit flags rather than a guess about semantics.

## 3. Phase 1 - one hand, translation only, prove the write holds

**Goal:** move `LookAtControl_LeftHand` by a fixed offset and see the left hand
move, with nothing else moving.

**Write:** `BoneTranslation` = a constant (start 10 uu along one axis),
`bApplyTranslation` on, `bAddTranslation` on, `BoneTranslationSpace` left as
the game set it. Once per frame from the script lane. No rotation.

**Instrument:** log `ControlTickTag` each frame beside our write count. If the
tag advances between our write and the next frame, evaluation happened after
our write, which is what we need.

**Acceptance:**
- the left hand visibly moves and the right hand and camera do not;
- `ControlTickTag` advances after our write;
- turning the lever off restores the original pose exactly.

**This also answers which bone the control drives** - the thing three documents
have been unable to establish statically. If the elbow moves instead of the
hand, the control targets something else and the log says so.

**Stop condition:** if the hand does not move at all, translation does not
reach the pose from this control, and phase 2 is skipped in favour of section 6.

## 4. Phase 2 - rotation, written at evaluation time

Only if phase 1 moves the hand.

**Write:** `BoneRotation` with `bApplyRotation` on. Try in this order, each a
single build, each with a live toggle:

1. Same lane and cadence as phase 1. If rotation holds, stop here - the old
   conclusion was a cadence artefact.
2. If it does not hold: write when `ControlTickTag` changes, i.e. immediately
   after an evaluation, so our value is the freshest before the next one.
3. If that does not hold: set `bShouldTickInScript` and write from
   `TickSkelControl`, which is the engine calling us at exactly the right
   moment. This changes the system's behaviour and is therefore an
   implementation mechanism, not a measurement - it is used to make the write
   land, not to observe anything.

**Acceptance:** the hand rotates with a fixed test rotation, holds through an
animation, and returns to normal when the lever is off.

**Stop condition:** if all three fail, rotation genuinely cannot be driven from
this control, and section 6 becomes the plan for hands.

## 5. Phase 3 - the controller, the grip, the weapon

Only after phases 1 and 2 hold.

1. **Pose source.** One immutable snapshot per frame - head, grip pose, aim
   pose, validity - published by the XR owner and consumed on the script lane.
   Both eyes use the same snapshot.
2. **The delta.** `D = A_target * inverse(A_source)`, where `A_source` is the
   hand's current animated frame and `A_target` is the controller grip times a
   calibrated grip offset. **The inverse of the source is what the last attempt
   omitted**, which left the native wrist rotation multiplied into the result.
3. **The grip pivot.** Rotation about the palm, seeded from the split's
   measurement (12.26 uu from the wrist ring to the hand's centroid) and
   adjustable live. This is where the earlier grip work returns, now applied
   somewhere that is not recomputed away.
4. **The weapon.** Nothing extra is written for it. The attachment joint is a
   child of the hand, so it should follow. **This is the test, not the
   assumption** - if the weapon does not follow, that is the finding, and the
   item graph work becomes necessary rather than optional.

**Acceptance:** hand at the controller, rotating about the grip, weapon
following, fingers still animating, caps intact, other hand and camera
unmoved, both eyes agreeing.

## 6. Fallback, if the control lane cannot carry rotation

Draw-scoped palette, hands only: build a private palette per hand by applying
one common delta `D` to every skinning matrix that draw consumes, and draw each
hand under its own palette. Finger animation is preserved by
`sum_i w_i (D M_i) v = D (sum_i w_i M_i v)`, so this is not a static hand.

It gives correct-looking hands and **does not move weapons, muzzle effects or
aim** - those stay on the engine's transform, which a GPU edit does not touch.
VR-33 would then need a separate mechanism for the weapon half, and that is the
cost of this fallback rather than a hidden risk.

## 7. Ordering, and what is deliberately deferred

Build phases 1, 2, 3 in that order, one behavioural change per build. Deferred
until they land, and none of them blocks:

* the live item graph and its attachment record layout;
* the native update ordering trace;
* the palette-to-skeleton mapping;
* firing aim, melee caches, physics-held objects, corpse carry.

The cut boundary keeps its own acceptance throughout: `MsLerpVertex` retains a
parent's blend indices and weights, so clipped and cap vertices can carry
forearm influences and a wrist-only move can still stretch the cap. If it does,
that is fixed at the cut, never by moving `Root_jnt` - which is the common
ancestor of both arms and the camera.

## 8. Questions

1. Phase 1 writes a constant offset to a live control that currently reads
   strength 1.0. Is there a reason to expect that to destabilise anything
   beyond the hand, given the three controls are distinct objects?
2. Is `bShouldTickInScript` + `TickSkelControl` acceptable as an implementation
   mechanism in phase 2 step 3, given it does change control behaviour?
3. Phase 3 assumes the weapon follows because the attachment joint is a child.
   Is testing that directly the right call, or is the item graph needed first?
4. Anything in section 1 that is not actually settled.
