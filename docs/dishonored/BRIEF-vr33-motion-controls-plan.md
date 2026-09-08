# BRIEF 4: VR-33 - the stereo eye is unsolved; plans for scale, weapons, rotation

**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`
**Head:** `fd271125`. **Follows:** `VR-33-SHADER-CAPTURE-REVIEW.md` and
`BRIEF-vr33-build-a2-result.md`.

**This is the last review available for some time.** It asks for an outline
covering three pieces of work, and for the review's own judgement on the first,
because two of my own instruments have now produced confident readings that
were later refuted.

Tags for recoverable states: `vr33-placement-works`, `vr33-scale-correct`.

---

## 1. Where things actually stand

**Working and confirmed in the headset:**

* Hands track the controllers.
* Hands hold position through head turns (slight movement, acceptable).
* Correct occlusion against world geometry, after restoring the depth range.
* The ghost/duplicate hand is gone.

**The one remaining defect: the hands read as too large.**

The placement itself is measured, not tuned:

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local
clip    = ViewProjectionMatrix * p_cam

target_local = Rl^T * (d_cam - t),   T = target_local - q_local
```

`LocalToWorld` and `ViewProjectionMatrix` are read from the device at each draw
through the register indices that shader's own CTAB declares.

---

## 2. Two of my readings were wrong. Both are withdrawn.

This matters more than the code, because the review should not trust the
measurements below without knowing which ones already failed.

### Withdrawn 1: "LocalToWorld separates the eyes"

I reported a measured spread of **6.78-6.94 uu against a predicted IPD of
6.31 uu** as strong confirmation that `LocalToWorld`'s translation, projected on
the camera's right axis, separates the two eyes.

It does not. When the comparison was made **within a single frame**, where the
pose is identical and only the eye can differ, the separation is **0.00 uu over
24,376 pairs**. The earlier number was a midpoint classifier measuring **head
motion over time**, which is exactly why it disintegrated on head turns.

### Withdrawn 2: "the eye classification is working"

On the strength of the above I flipped a sign, the tester reported the scale
looked correct, and I recorded that as the eye offset working. Given
`LocalToWorld` carries no eye information, whatever produced that improvement
was not eye classification. The `vr33-scale-correct` tag is named for what the
tester saw, not for a mechanism I can defend.

### Also measured, and unexplained

Each of the three shaders draws this mesh **three times per frame**, not twice
(counters 2:2:1). So "two draws per shader, one per eye" is itself unsupported.

### And the instrument meant to settle it produced nothing

`PaletteEyeHunt` keeps each draw's `VP` and `LocalToWorld` and prints which
element differs between the ordinals of one frame. It is armed, its enclosing
counters increment (so the code path runs), and **it emitted no lines at all.**
I do not yet know why. I am reporting this rather than quietly re-running,
because "my diagnostic silently produced nothing" is the same class of problem
as the two withdrawals above.

**Current state: no eye offset is applied at all** (counters read `L 0 R 0`),
because applying one on an unlearned vote is a full IPD on a coin flip. The
hands are placed at the head centre, and that is the configuration the tester
calls too large.

---

## 3. Plan A - the scale/stereo problem

### The hypothesis I would pursue, and why

Stop classifying the eye. **Recover the camera's optical centre from the draw's
own `ViewProjectionMatrix` and place the controller relative to THAT.**

This is Stage B of the earlier review, which I skipped because `LocalToWorld`
appeared to hand me the frame directly. If each eye's draw carries its own `VP`,
its optical centre differs by the IPD and the correct per-eye offset falls out
with **no classification, no vote, no ordinal, and no tag** - which removes the
entire class of bug that has cost the last five iterations.

```
centre_h = inverse(Q) * [0, 0, 1, 0]        Q = the draw's ViewProjection
centre   = centre_h.xyz / centre_h.w
target   = centre + (controller offset in that camera's frame)
```

with the guards the review already specified: invertibility, conditioning, a
finite non-zero `w`, and refusal rather than a forced answer on a non-standard
projection.

### The branch that must be planned for

**If `VP` is identical between the two draws as well**, then the eye separation
does not happen in the draw's constants at all - it is applied outside them (a
viewport shift, a post pass, or the runtime layer compositing). In that case no
per-draw measurement can recover it and the eye must come from the code that
knows: the `reentry` method, which pushes `-1` and `+1` tags.

The obvious route there failed once already:
`dvr::camera::second_pass_for_current_thread()` is set on the game thread, and
UE3 queues render commands, so by the time the draw runs on the render thread
it reads false. The counter caught it (`pass2 0 draws` against `pass1 92,800`).

*Q1: is optical-centre recovery the right approach, and if `VP` also proves
identical, what is the correct thread-safe way to carry the eye from the
`reentry` pass to a draw that executes later on the render thread?*

*Q2: three draws per shader per frame. What is the likely third draw, and does
it change what "the eye" means for this mesh?*

*Q3: is there a reason the perceived size would be wrong that is NOT stereo? A
half-IPD error is 3.2 uu against a hand at ~44 uu, and it seems a large
perceptual effect for a small geometric one. Am I chasing the wrong thing?*

---

## 4. Plan B - attaching the weapon and the bolt

The crossbow currently floats where the engine put it. The hand moves; nothing
it holds follows.

### What is known

* The weapon is a **separate draw** with its own palette (`c6 x36`, 12 bones,
  against the arms' `c6 x144`).
* `handAttachment_L/R_jnt` are children of `hand_L/R_jnt`, from validated
  engine parent walks - so in the engine's own hierarchy the attachment is
  beneath the hand.
* `DishonoredInventoryItem` exposes item ownership, socket category and a
  first-person mesh; `DishonoredItemSkeletalComponent` has a component-to-item
  backlink. These are declarations only, with unvalidated spaces.

### The approach I would propose

The same rigid delta, applied to the weapon's own palette. If the weapon draw
can be qualified the way the hand draw is, and its own `LocalToWorld` read the
same way, then the transform that puts the palm at the controller can be
applied to the weapon's palette too - the weapon rides the hand because both
are being moved by the same delta in the same frame.

The bolt, muzzle effects and any reload parts are **separate draws again** and
each needs the same treatment; a weapon that moves while its bolt does not is
worse than neither moving.

**The honest limit, unchanged:** this is GPU-side. Firing aim, projectile
origin, melee contacts and any gameplay consumer read the engine's transform
and will not follow. Visual attachment and functional attachment are different
problems.

*Q4: is applying the same delta to the weapon's palette the right approach, or
should the weapon be driven from the engine's attachment hierarchy instead -
and if the latter, does that reopen the native route that was parked?*

*Q5: how should the weapon draw be identified, given three shaders already draw
the hands and a bone count of 36 is a weak identifier on its own?*

---

## 5. Plan C - rotation, and grip alignment

Currently only translation is applied. The hands keep the engine's animated
orientation, which is why they look posed rather than gripping.

### The approach

Extend `MpBuild` from translation to full rigid composition:

```
A_target = converted_controller_grip_pose * grip_to_palm
D        = A_target * inverse(A_source)
M_i_new  = D * M_i_original
```

with `A_source` the current palm frame, taken from a non-collinear vertex patch
rather than a single point, and degeneracy rejected rather than approximated.

Two specifics the earlier review already flagged:

* The linear parts of the matrices must be transformed too, not just the
  translation column, or normals and tangents will be wrong.
* The translational basis is a **coordinate convention with determinant -1**,
  not a rotation. Rotations convert by basis conjugation
  `B * R * inverse(B)` - not by negating Euler angles or quaternion components
  by analogy with positions.

`grip_to_palm` is a fixed per-side calibration whose translation rotates **with
the controller**, not a fixed camera-space correction. That distinction was
already the cause of one failure this session.

*Q6: should rotation land before or after the weapon work? Rotation may change
what "attached" means for the weapon, but the weapon may also be the better
test of whether the rotation is right.*

*Q7: the anchor is currently 12 vertices nearest the hand class's bind-pose
centroid. Is that a sound basis for deriving an orientation, or does the
orientation need a deliberately chosen non-collinear triple?*

---

## 6. Ordering, and the question I most want answered

My proposed order: **scale/stereo, then rotation, then weapons** - on the
grounds that a weapon attached to a hand whose orientation is about to change
would be calibrated twice.

*Q8: is that the right order?*

*Q9 - the one I would most like answered. Two instruments this session produced
confident readings that were later refuted, and one produced nothing at all
while appearing to run. Both survived because I checked whether they agreed
with expectation rather than whether they could fail. What should I change
about how these measurements are built, beyond the case-by-case fixes?*

---

## 7. State

| Item | State |
|---|---|
| Placement through the measured chain | Working |
| Depth range | Working; correct occlusion |
| Shader reflection (CTAB) | Working per shader |
| Draw capture | Working; 28 packets, 3 shaders |
| **Eye / stereo offset** | **Unsolved; nothing applied** |
| `PaletteEyeHunt` diagnostic | **Armed, runs, emits nothing - unexplained** |
| Scale (uu/m) | Assumed 100; `[PosTrack] Scale` is 108 |
| Pose timing | Not addressed |
| Harness case for the feature gate | Not added |
| Weapons / bolt | Not started |
| Rotation / grip | Not started |

Nothing merged to `VR-Main`.
