# BRIEF 3: VR-33 Build A2 - placement works; depth and scale remain

**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`
**Head:** `645c2381`. **Follows:** `VR-33-SHADER-CAPTURE-REVIEW.md`.

**Headline: the hands track the controllers and hold position through head
turns.** The tester rates it the best result of the session. Two defects remain
and both are now diagnosed from measured data.

**A note on this review's scope.** This is the last review available for
several hours, so the questions at the end ask for **conditional guidance -
decision trees rather than single answers** - so work can continue down
whichever branch the evidence takes. Where a question has a plausible "if X
then Y, if not-X then Z" shape, please answer both limbs.

---

## 1. Measured facts now in hand

### The vertex path, from the shader disassembly

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local            (camera-relative world, uu)
clip    = ViewProjectionMatrix * p_cam
```

The palette's output is the component's **local** space; `LocalToWorld` maps it
to a camera-relative world frame. The ~138 uu vertical term in the earlier
calibration was that local origin. The pawn-root conjecture was wrong, as the
review anticipated.

### Three shaders draw this mesh, and they differ

| Shader | Packets | Viewport depth range |
|---|---:|---|
| `DB11BB81` | 14 | **MinZ 0.0, MaxZ 0.001** |
| `11DD5E8A` | 7 | MinZ 0.0, MaxZ 1.0 |
| `F2E11B73` | 7 | MinZ 0.0, MaxZ 1.0 |

All three agree on `ViewProjectionMatrix` c0, `BoneMatrices` c6[225]
(`float4x3[75]`, 48 uploaded), `LocalToWorld` c231. They disagree elsewhere:
`MeshOrigin`/`MeshExtension` at c235/c236 in `DB11BB81` and c238/c239 in the
others; `WorldToLocal` at c235 exists only in the latter two.

`DB11BB81` defines **c4 as an immediate `(3, 1, 0, 0)`** while the device
reports `(0, 0, 0, 1)` for that register - the `def` hazard, live.

### Confirmed predictions from the last review

* Weights are **not** normalised by the shader; it sums `w_i * M_i` into one
  blended matrix, so a translation `T` moves a vertex by `wsum * T`.
  **Measured on the anchor: the weights sum to exactly 1.0000**, so this is
  harmless in practice here. The tolerance guard stays.
* Register numbers are per shader and are parsed from each shader's CTAB.
* The `def` override is real, as above.

### The camera basis, verified before placement was written

Rows 0 and 1 of `ViewProjectionMatrix` normalised give right and up (lengths
are the focal scales); the w row is forward. On a captured packet the left palm
came out **15.3 uu left, 26.1 uu below, 44.1 uu in front** of the camera, basis
orthonormal to five decimals, w row unit to six. Derived FOV 108.1 x 110.0 deg.

---

## 2. What placement does

```
target_local = Rl^T * (d_cam - t)      [Rl | t] = LocalToWorld, read at the draw
T            = target_local - q_local  q re-skinned from the game's palette each frame
```

No calibration, no neutral, nothing left underneath. The present thread
publishes only frame-free scalars (controller offset from head in the head's
own right/up/forward); the draw converts them with the basis from *that draw's*
constants, so no assumption about game axes exists in the path.

Validations that a wrong layout will not satisfy by accident: rigid
`LocalToWorld`, orthonormal camera basis, unit forward row. Refusals name their
reason and draw the engine's own hand. **104,670 draws placed, 12 refused**
(tracking not yet acquired).

---

## 3. Defect A: drawn over everything - measured, not a placement fault

`DB11BB81` runs with the depth range crushed to the nearest **0.1%** of the
buffer. That is the standard first-person guarantee that the view model is
never occluded. Correct placement cannot change it.

A lever is implemented and **armed but not yet run**
(`[Hands] PaletteDepthRange`): restore `MinZ 0, MaxZ 1` for our draws only,
only when `MaxZ < 0.5` (so it touches the crushed pass alone), and put the
game's range back on every exit path.

**It may not be sufficient.** If that pass runs after a depth clear there is
nothing to occlude against and the hands will look identical.

### The tester's hypothesis, which reorders the work

The tester suggests the "huge" appearance may be **entirely** the overlay: with
no occlusion and no depth cues, a correctly placed hand at ~0.4 m drawn on top
of everything reads as enormous. If so, fixing depth dissolves the scale
question rather than merely preceding it.

*This seems right and is why depth is being tested first.*

---

## 4. Defect B: apparent scale

A typical logged frame:

```
L ctl r/u/f (-0.170 -0.105 +0.373) m -> pcam (-26.4 +31.4 -10.7) uu
```

At the assumed 100 uu/m the controller sits at right/up/forward
`(-17.0, -10.5, +37.3)` uu. The engine's own palm, from the capture, sat at
`(-15.3, -26.1, +44.1)` uu. Right and forward are close; **up differs by about
15 uu**, so the hand is placed markedly higher than the engine places it.

**No paired measurement exists.** The capture packets predate the controller
scalars being published, so no packet holds both the engine's palm and the
controller position at one instant. The comparison is across different moments
and arm positions and is suggestive only.

Candidates: `[Hands] WorldScaleUU = 100` and `[PosTrack] Scale = 108`, neither
validated for this conversion; and the review's own point that palette and
world units may legitimately differ through `F` rather than conflicting.

A pure scale error should be proportional on all three axes. This one is not,
which is what makes the vertical term suspicious as something else.

---

## 5. Process failure: the same bug three times

The first A2 run gave "the arms didn't move at all". `MpDriveTick` opened with
`if (!g_mpDrive && !g_mpAbs) return;` and `PaletteWorld` was a third consumer
not in that list, so the controller pose was never published. The reflection
worked perfectly throughout; there was nothing to place.

Third instance of one shape: `MsTick` behind `g_dcOn`, poses behind
`g_mpDrive`, then behind `g_mpDrive || g_mpAbs`. **The previous review asked
for a harness case covering exactly this and it was not added before the run.**

Fixed as a class: the gate is now the backend being on, not a list of
consumers. Refusals distinguish "the pose tick never ran" from "tracking
invalid", which had read identically.

---

## 6. State

| Item | State |
|---|---|
| Shader reflection (CTAB) | Working per shader; refuses if unreadable |
| Draw capture | Working; 28 packets, 3 shaders |
| Placement through the measured chain | Working; tracks, holds through head turns |
| Depth-range lever | Implemented, **armed, not yet run** |
| Scale | **Assumed, never measured** |
| Eye tag | `-2`, "not identified" - mono has no per-draw tag |
| Pose timing (head look-ahead vs hand) | **Not addressed** |
| Harness case for the feature gate | **Not added** |
| Render states in the capture | **Not captured** (no ZENABLE/ZFUNC/clear info) |
| Build B (orientation, grip) | Not started |
| Build C (weapons) | Not started |

Nothing merged to `VR-Main`.

---

## 7. Questions - please answer both limbs where conditional

**Q1 - Depth, both outcomes.** If restoring `MaxZ` to 1.0 yields correct
occlusion, is that the right long-term mechanism, or is it masking something
that will break elsewhere (other passes, the two full-range shaders, stereo)?
**And if it does NOT** - the hands still composite on top, implying a depth
clear before that pass - what is the next mechanism: draw the hands in the
world pass, reconstruct depth, or accept overlay and compensate?

**Q2 - Scale, contingent on Q1.** If depth fixes the appearance and the tester
reports scale now looks right, is any further scale work needed before Build B,
or should a rigorous uu/m measurement land regardless? **If scale still looks
wrong after depth**, what is the correct measurement - would paired packets
with the controller at two known separations (touching the headset, then fully
extended) suffice, or does a viewmodel projection differing from the world
projection make even that ambiguous?

**Q3 - The vertical discrepancy.** ~15 uu on `up` with `right` and `forward`
close is not proportional. Is this more likely a grip-to-palm offset that
Build B supplies anyway, an anchor that is not where I think it is on the hand,
or a genuine scale/units artefact? What single measurement distinguishes them?

**Q4 - Three passes, one transform.** The mesh is drawn by three shaders per
frame and placement currently applies to all of them, since qualification is on
geometry. Is that correct, or should the transform be restricted to specific
passes - and how should the two full-range passes be identified as to purpose
(depth prepass, shadow, something else) without more capture?

**Q5 - Ordering.** Given position works: depth, then scale, then Build B
orientation? Or does orientation belong earlier because a grip transform may
absorb part of what currently looks like a scale or vertical error?

**Q6 - Outstanding items.** Which of the untouched items in section 6 must land
before Build B rather than after: pose timing, the harness case, render states
in the capture, the eye tag? Please rank rather than listing.

**Q7 - Anything not asked.** Given the whole picture, what is the most likely
thing to be wrong that this brief has not considered? Previous reviews have
twice identified an error that was not on my list at all - the truncated
census, and the baseline term the relative drive never cancelled - and that has
been the highest-value part of each review.
