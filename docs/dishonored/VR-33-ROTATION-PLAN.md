# VR-33 rotation and grip: the plan as implemented

Revised 2026-09-07 after the review in `VR-33-ROTATION-PLAN-REVIEW.md`, which
found three blockers in the first draft. All three are corrected here and in the
code. Read the review for the full argument; this document is what was built,
why, and how to test it.

**Status: IMPLEMENTED, built, installed, and unverified in the headset.**

---

## 0. What the review changed, and what it left standing

| Review finding | What was done |
|---|---|
| **1. R1's wrist-roll gate is impossible.** With rotation off, nothing connects a physical wrist to the game's palette, so a good source frame will NOT turn when the tester rolls their wrist | The gate is withdrawn. Three quantities are now logged under separate names - **src** (the candidate palm frame, from the original palette), **ctl** (the controller) and **out** (the correction actually applied) - and the corrected prediction is that src and ctl are INDEPENDENT while rotation is off. The frame is validated against the game's own animation instead |
| **2. `B * (R_H^T * R_C) * B^T` reintroduces head-driven rotation.** It changes the basis of a rotation operator; a pose orientation maps controller-local axes into another frame. It also omits the back-to-forward conversion the position path already performs | Replaced by `O_C = B * F * transpose(R_H) * R_C` with `F = diag(1,1,-1)`, which is the same physical mapping as the working position path. The review's counterexample is now a shipped test case, `head_turn`, and the rejected formula fails it by 180 degrees |
| **3. Grip capture mixes spaces.** `R_src` is component-local; `O_C` is camera-relative | `G = transpose(O_C) * (R_L * R_src_local)` - the source frame is carried through this draw's own LocalToWorld first. Round trip, idempotence and basis invariance are all shipped test cases |
| **4. The dominant slot is a candidate, not anatomy** | Called a palette SLOT throughout, in the code and the log. The scale is derived per call, never the constant 0.999512. The slot is frozen with a fingerprint (slot, anchor size, first anchor identity) and invalidated when any of them is re-derived |
| **5. The normal path can be closed now** - two of the three hand shaders run normals and tangents through the same palette rows; their `WorldToLocal` is a view/light conversion and must NOT be rotated | Recorded on `MpBuild` and in ENGINE_NOTES. No second transform is applied, and `WorldToLocal` is untouched |
| **6. A validity flag beside loose floats is not publication** | One `MpPoseSnap` copied whole under a lock, with a generation. Each ORIGINAL DRAW latches one copy that both hands share. The thread contract is measured and logged rather than assumed |
| **7. Independent weapon targets do not preserve the assembly** | Accepted. The weapon section is rewritten around an assembly root with root-relative animation preserved, and is NOT part of this build |
| **8. Keep R0 logging-only; a rotation failure must keep translation** | R0 is logging only. Rotation refuses independently and placement continues translation-only, with the reason named |

Two of the review's points are recorded as carried limits rather than fixed:
the render-view ticket, and head look-ahead against hand sampling time. Neither
blocks the rotation maths.

---

## 1. What was measured before any of this was written

### 1.1 The frame maths, checked on the desk

`src/tools/frame_test` compiles the same `hand_frame.h` the proxy compiles and
runs 20 cases. All 20 pass. The load-bearing ones:

```
head_turn              stationary controller, head yaw 0..180: correct formula moves
                       the hand 0.0000 deg; the rejected similarity moves it 180.0
head_turn_can_fail     the rejected similarity DOES produce head-driven rotation,
                       so this suite would catch its return
axis_pm90              each controller axis at +/-90, column by column: error 0.000000
grip_roundtrip         capture then apply on frozen input: correction is identity
grip_basis_invariance  head and controller rotated together by 65 deg: G unchanged
pivot_rotating         the palm anchor lands on the target to 0.00000 uu
rot_off_matches_legacy with rotation off the delta is exactly target_local - q
compose_commutes       skinning with the composed palette equals D applied after
                       skinning - this is why finger animation survives
```

The proxy runs the identical suite from `DllMain` and writes the result to the
log, so every tester's log carries proof that the arithmetic in THAT build is
the arithmetic that was checked. If it ever fails, the rotation lever refuses
and placement stays translation-only.

### 1.2 The dominant slot, replayed through the shipped decomposition

`frame_test.exe <the 28 saved packets>` runs the real captures through
`decompose_scaled_rotation`, the same function the draw path calls:

```
28 packet(s): 28 decomposed, 0 refused. dominant slot 10 (the same in all)
uniform scale 0.999511659 .. 0.999512255
worst anisotropy 5.960e-07 | worst orthonormality residual 9.835e-07
the frame moved at most 1.0460 deg across these packets
```

Three orders of magnitude inside the 0.02 tolerance, and the scale range matches
the review's independent figure over all 1,344 matrices.

**The 1.05 degrees is new.** The earlier reading, on the weighted BLEND, was
identical to five decimals and looked frozen. The single slot moves. That is not
proof it follows the palm - these captures are all one near-idle pose - but it
does establish the slot responds to the engine's animation at all, which the
blend's reading did not.

---

## 2. The mechanism, as built

```
                 published by the pose tick, per hand, under one lock
   inHead   = F * transpose(R_head) * R_controller          F = diag(1,1,-1)
   ruf      = F * transpose(R_head) * (p_controller - p_head)

                 completed at the draw, from that draw's own constants
   O_C      = B * inHead                     B = [ right | up | forward ]
   d_cam    = k * B * ruf   (- the eye offset, unchanged)

                 the source frame, from the ORIGINAL palette
   A_src    = [ R_slot / scale | q ]         slot = the anchor's dominant slot
                                             q    = the existing blended anchor

                 the correction
   A_tgt_local = inverse(L) * [ O_C * G | d_cam ]
   D_local     = A_tgt_local * inverse(A_src)
   M_i_new     = D_local * M_i_original      every bone of that hand
```

* **The pivot is `q`.** `A_src` carries the palm anchor's position, so `D`
  rotates about the palm rather than the component origin. Rotating without the
  pivot is what swung the hand off its wrist in the reverted attempt.
* **Orientation from ONE slot, position from the anchor patch.** A weighted
  blend of rotations is not a rotation (det 0.970 measured); a single slot is a
  rotation times a uniform scale to six decimals.
* **Only the FRAME is normalised.** The rendered palette keeps its own scale,
  because `D` is composed onto the original matrices and `D` has scale 1.
* **Normals come with it.** `F2E11B73` and `11DD5E8A` run normals and tangents
  through these same rows. `WorldToLocal` is a view/light conversion and is
  NOT touched.
* **Rotation refuses independently.** Self-test failed, `B*F` improper, the slot
  not a rotation, the palette remapped under a frozen index, or a grip solved
  against a source frame that has since been re-derived - any of these leaves
  placement running translation-only and names the reason on the line.

### The levers

| Key | Repo default | Installed now | What it does |
|---|---|---|---|
| `[Hands] PaletteRotate` | 0 | **1** | The full rigid correction |
| `[Hands] PaletteFrameTol` | 0.02 | 0.02 | How far the slot may stray from a rotation before it is refused |
| `[Hands] GripLX/Y/Z`, `GripRX/Y/Z` | 0 | 0 | `G`, extrinsic X then Y then Z degrees (`R = Rz*Ry*Rx`) |
| SHIFT+F7 | - | - | Solve `G` for both hands from the next qualified draw |

---

## 3. THE TEST RUN

**Launch the game as it is installed. Everything is already armed.** Nothing
needs to be typed, no script needs to be run, and the previous stage is one ini
line away (`PaletteRotate=0`) if this build is worse than the last one.

**Expect the hands to be at a WRONG ANGLE when you start.** `G` is identity
until you calibrate it, so they will track your wrists at a fixed offset. That
is the designed starting state, not a fault. Test 2 fixes it.

### The tests, in order

| # | Do this | Expect | What it fails if it is wrong |
|---|---|---|---|
| 1 | Load a save and look at your hands | They are where your controllers are, as before, and now they **turn when you turn your wrists**, at a fixed wrong angle | If they do not turn at all, read `ms/palette/frame:` - `rotate=` and `refused` name the reason |
| 2 | Hold your hands roughly the way the game's idle pose holds them and press **SHIFT+F7** | The hands **snap** to the game's own orientation. That IS the calibration. The log prints `ms/palette/grip: SOLVED ... G = x y z degrees` for each hand | **Send me those six numbers.** They go in the ini and never need capturing again |
| 3 | After the capture, roll, pitch and yaw each wrist through its full range | The palm follows all three. A single axis moving **backwards** is the one failure the maths cannot rule out on its own | Report which axis |
| 4 | Hold both controllers still and turn your head through 180 degrees | The hands do **not** rotate and do not drift | This is the failure the corrected formula exists to prevent |
| 5 | Move only the left controller | Only the left hand moves. The right is the control | |
| 6 | Watch your fingers while gripping, and cast a power | Fingers still animate under the new orientation | A frozen hand means rotation replaced the animation instead of riding on it |
| 7 | Look closely at the wrist cut | The cap stays attached, no gap, no tear | |
| 8 | Close one eye, then the other | The hand is at the same angle in both | A per-eye difference means the eye state leaked into the rotation, where it does not belong |
| 9 | Roll a wrist and watch the shading on the back of the hand | Lighting moves with the surface | Lighting fixed while geometry turns would mean a normal path we have not seen |
| 10 | Play normally for a couple of minutes | No new stutter, no hands vanishing | |

### What I will read in the log afterwards, whatever you report

* `ms/frame/selftest:` - 20 cases, all PASS, at the top of the run.
* `ms/palette/frame:` every 3 s - **src**, **ctl** and **out** as three separate
  angles, plus the slot's scale, anisotropy and orthonormality residual, the
  slot numbers, and `placed`/`refused` with the reason.
* `ms/palette/lane:` every 30 s - whether the pose tick and the draws are one
  thread or two, and whether any draw ever saw a stale snapshot.
* `ms/palette/grip:` - the solved `G`.

**The one question this run answers that nothing else can:** does **src** move
when the game animates your hand, and stay put when only your fingers move? If
src never moves at all during a melee swing or a power, the dominant slot is not
the palm's frame and the orientation source has to change - the review named the
validated-landmark fit as the fallback. If it moves with the whole hand but not
with fingers, the slot is right and the rest of this stands.

---

## 4. Still to come, and deliberately not in this build

### The weapon assembly

The review's correction is accepted: independent grip targets remove the
draw-order dependency, but they do **not** preserve the assembly. Independently
pinning each animated part to a fixed controller-relative target cancels its own
animation - bolt seating, limb and string movement, magazine travel, reload
motion. So:

* choose a source frame on the weapon's stable **grip/root**;
* compute **one** root correction;
* preserve every member's current animated transform **relative to that root**;
* separate components need that relationship represented, not assumed.

Identity comes from an owned live first-person component, asset identity,
transform/palette context and pass family - the draw census is a candidate
finder, not an item identity. `DcSameGeom` deliberately groups one mesh across
shaders, so "a different vertex shader means different geometry" is false; and
two instances can share buffers. Re-equipping may recreate buffers, and holstered
or released objects may keep using the same geometry, so the tests validate the
held instance semantically rather than requiring pointer sets to vanish.

### Firing

W2 cannot promise a bolt leaving along the tracked weapon while native
projectile origin and aim are deferred. The scope is stated in advance: the
visual stage **accepts** that the released bolt returns to the native origin and
may visibly disagree with the held crossbow. Correcting it is the firing stage,
not an acceptance gate for the visual one.

### Carried

* **A render-view ticket** carrying the eye, instead of inferring it from the
  right-axis jump.
* **The `same eye` counter.** As the review notes, `same` counts the small-jump
  branch that RETAINS an inferred label and `toggled` counts the jump-band
  branch, not a checked change of label - so the 776 cases are an
  instrumentation limit, not 776 repeated delivered eyes. No threshold tuning.
* **Head look-ahead against hand sampling time.** If rotation is right when
  still and wrong when moving, this is the first suspect, not the grip.
* **One canonical metres-to-units conversion.** `WorldScaleUU` 100 against
  `[PosTrack] Scale` 108, logging only for now. It is a different quantity from
  the palette's own 0.999512 and a cleanup must account for the effective
  transforms rather than rename one of them.
* **Mesh geometry size.** Untouched.
