# BRIEF 3: VR-33 Build A2 - placement through the measured chain works

**Date:** 2026-09-07. **Branch:** `claude/vr-33-hands-and-weapons-at-the-controllers`
**Head:** `93b2510c` plus an uncommitted depth lever. **Follows:**
`VR-33-SHADER-CAPTURE-REVIEW.md`.

**Headline: the hands track the controllers and stay in place through head
turns.** Two defects remain, both now diagnosed from measured data rather than
inferred, and one of them is not a defect in the placement at all.

---

## 1. The capture worked, and it ended the guessing

Register layouts are parsed from each shader's own CTAB. The run produced 28
state packets and three distinct shaders.

### The vertex path, read from the disassembly

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local            (camera-relative world, uu)
clip    = ViewProjectionMatrix * p_cam
```

**The palette's output is the component's LOCAL space**, and `LocalToWorld` -
present in the constants - maps it to a camera-relative world frame. The
~138 uu vertical term in the old calibration was that local origin. The
"pawn root" conjecture was wrong, exactly as the review predicted it might be.

### Three predictions of the review, all confirmed in the data

* **Weights are not normalised by the shader.** It sums `w_i * M_i` into one
  blended matrix. So a palette translation `T` moves a vertex by `wsum * T`.
* **Register numbers are per shader.** `MeshOrigin` sits at c235 in one shader
  and c238 in two others; `WorldToLocal` exists in only two.
* **The `def` hazard is live.** One shader defines c4 as an immediate
  `(3, 1, 0, 0)` while the device reports `(0, 0, 0, 1)` for that register. A
  device read of that register would have been wrong by construction.

### The basis, verified before the placement was written

From `ViewProjectionMatrix`: rows 0 and 1 normalised give right and up (their
lengths are the focal scales), and the w row is the forward axis.

On a captured packet the left palm came out at **15.3 uu left, 26.1 uu below,
44.1 uu in front** of the camera, with the basis orthonormal to five decimals
and the w row unit to six. Three correct signs on real data, checked with no
headset. Derived FOV 108.1 x 110.0 degrees.

---

## 2. What placement does now

No calibration, no neutral, nothing left underneath:

```
target_local = Rl^T * (d_cam - t)      [Rl | t] = LocalToWorld, read at the draw
T            = target_local - q_local  q re-skinned from the game's palette each frame
```

`d_cam` is built from frame-free scalars published by the present thread (the
controller's offset from the head in the head's own right/up/forward), turned
into a world vector by the basis read from *that draw's* constants. No
assumption about the game's axes exists anywhere in the path.

Placement validates what a wrong layout will not satisfy by accident - a rigid
`LocalToWorld`, an orthonormal camera basis, a unit forward row - which also
refuses an orthographic or non-standard path rather than forcing an answer out
of it. Every refusal names its reason and draws the engine's own hand.

**Tester result:** tracking is good, hands mostly stay in place through head
turns, and this is the best of every attempt so far. 104,670 draws placed,
12 refused (tracking not yet acquired).

---

## 3. Defect A: they draw over everything - MEASURED, and not a placement fault

From the captured packets:

```
viewport 0 0 2750 2850 minZ 0.000000 maxZ 0.001000   <- the hand draw
viewport 0 0 2750 2850 minZ 0.000000 maxZ 1.000000   <- another draw same frame
```

**The view model is drawn with its depth range crushed into the nearest 0.1% of
the depth buffer.** That is the standard first-person trick that guarantees the
weapon and hands can never be occluded by world geometry. Correct placement
cannot change it; the hands composite on top by construction.

A lever is implemented (`[Hands] PaletteDepthRange`, default OFF): restore
`MinZ 0, MaxZ 1` for our draws only and put the game's own range back on every
exit path.

**It may not be sufficient, and the brief says so rather than claiming a fix.**
Whether it produces real occlusion depends on the depth buffer still holding
world depths at that point in the frame. If the pass runs after a depth clear
there is nothing to occlude against and the hands will look identical. That is
the experiment, and it has not been run.

*Question 1: is restoring the depth range the right mechanism, or does correct
occlusion require the hands to be drawn in the world pass entirely? The capture
does not currently record render states (ZENABLE, ZFUNC, ZWRITEENABLE) or
whether a depth clear precedes this pass - should it, before the lever is
trusted either way?*

---

## 4. Defect B: the scale looks wrong, and it is the one number never measured

Reported as "the scale seems huge". A typical logged frame:

```
L ctl r/u/f (-0.170 -0.105 +0.373) m -> pcam (-26.4 +31.4 -10.7) uu
```

At the assumed 100 uu/m that puts the controller at right/up/forward
`(-17.0, -10.5, +37.3)` uu. The engine's own palm, from the capture, sat at
`(-15.3, -26.1, +44.1)` uu. Right and forward are close; **up differs by about
15 uu**, so the hand is being placed markedly higher than the engine places it.

**No paired measurement exists.** The capture packets predate the controller
scalars being published, so no single packet contains both the engine's palm
and the controller position at the same instant. The comparison above is across
different moments and different arm positions, and is therefore suggestive
only.

The candidates remain what the review named: `[Hands] WorldScaleUU = 100` and
`[PosTrack] Scale = 108`, neither validated for *this* conversion, and the
review's own note that palette and world units may legitimately differ through
`F` rather than conflicting.

*Question 2: what is the right rigorous scale measurement here? The review
asked for known distances at two depths. Concretely - would capturing paired
packets with the controller held at two known separations (touching the
headset, then fully extended) and solving for uu/m be sufficient, or does the
viewmodel projection differing from the world projection make even that
ambiguous?*

*Question 3: could the vertical discrepancy be something other than scale - a
grip-to-palm offset that Build B would supply anyway, or an anchor that is not
where I think it is on the hand? A pure scale error should be proportional on
all three axes, and this one is not.*

---

## 5. A process failure worth reviewing: the same bug three times

The first A2 run produced "the arms didn't move at all". The cause:
`MpDriveTick` opened with `if (!g_mpDrive && !g_mpAbs) return;` and
`PaletteWorld` was a third consumer not in that list, so the controller pose
was never published. The reflection worked perfectly throughout - every
shader's layout read correctly - and there was nothing to place.

Third instance of one shape: `MsTick` behind `g_dcOn`, then poses behind
`g_mpDrive`, then behind `g_mpDrive || g_mpAbs`. The previous review asked for
a harness case covering exactly this and it was not added before the run.

Now fixed as a class: the gate is the BACKEND being on, not a list of
consumers, so a mode added later cannot miss it. Refusals also distinguish "the
pose tick never ran" from "tracking is invalid", which had read identically and
cost a run each time.

*Question 4: is the harness case still worth adding now that the gate no longer
enumerates consumers, or is the structural fix sufficient?*

---

## 6. State, and what is still not done

| Item | State |
|---|---|
| Shader reflection (CTAB) | Working, per shader, refuses if unreadable |
| Draw capture | Working; 28 packets, 3 shaders |
| Placement through the measured chain | Working; tracks and holds through head turns |
| Depth-range lever | Implemented, default OFF, **never run** |
| Scale | **Assumed, never measured** |
| Eye tag | Records -2, "not identified" - mono has no per-draw tag |
| Pose timing (head look-ahead vs hand) | **Not addressed** |
| Harness case for the feature gate | **Not added** |
| Build B (orientation, grip) | Not started |
| Build C (weapons) | Not started |

Nothing merged to `VR-Main`.

*Question 5: given position now works, is the right order (a) depth, (b) scale,
(c) Build B orientation - or should orientation come first on the grounds that
a grip transform may absorb part of what currently looks like a scale error?*
