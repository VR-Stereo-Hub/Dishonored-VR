# VR-33 rotation, grip and weapon plan review

Reviewed 2026-09-07 against `681b54df` on
`claude/vr-33-rotation-grip-and-weapons` and the uncommitted
`VR-33-ROTATION-PLAN.md`. This is a design review only. No implementation,
installation, game run or existing configuration was changed.

## Verdict

Proceed with rotation and grip after correcting the issues below. Preserve the
headset-confirmed translation, stereo, scale and depth behavior. The dominant
palette slot is a reasonable candidate orientation source. Independent hand and
weapon grip targets are also reasonable, subject to a real source frame and
preservation of the weapon assembly's internal animation.

The current plan is not ready to execute literally. Its first motion gate tests
a dependency that does not exist yet, its orientation conversion fails a simple
head-motion invariant, and its grip capture mixes coordinate spaces. Fix these
in the plan before a headset build.

## 1. R1's wrist-roll gate is impossible in a measure-only build [P1]

Plan lines 360 and 431 require the source palm frame to turn approximately 90
degrees when the physical controller rolls, before controller rotation has been
applied to the game. The current controller path supplies translation only.
There is no causal connection from physical wrist rotation to the ORIGINAL game
palette. A good source frame will normally remain unchanged during that test.

Log three separately named quantities:

- Source palette/palm orientation, measured from the original draw data.
- Controller orientation, from the tracked grip pose.
- Applied/output palm orientation, only when rotation is enabled.

Correct R1 predictions:

| Stimulus with rotation OFF | Expected result |
|---|---|
| Controller rolls in place; game idle animation is held comparable | Controller orientation changes. The source frame has no obligation to follow it. |
| A native animation visibly rotates the palm | The candidate source frame follows that palm rotation, with a stable relationship to independent palm landmarks. |
| Finger animation with palm approximately still | Candidate frame stays stable relative to the palm; a finger-driven frame fails. |
| Head turns | Controller-relative-head orientation changes. Judge source orientation in its declared component frame against the actual animated palm; do not assume all native pose values must remain constant. |

A frame being rigid or moving during an animation is insufficient: a forearm or
finger frame can satisfy both. Use a few deliberately chosen palm landmarks as
an independent validation reference. They do not have to become the production
orientation algorithm. If no animation actually excites palm rotation, report
insufficient excitation rather than rejecting a valid frame for remaining still.

Do not spend a headset run on the original R1 test. Claude can exercise the
controller math offline and combine the corrected source-frame instrumentation
with the next useful build.

## 2. Use a pose-frame conversion consistent with the position path [P1]

The proposed `B * (R_head^T * R_ctl) * B^T` is not justified by calling it
conjugation. A pose orientation maps controller-local axes into another frame.
An active rotation operator expressed in one basis is a different mathematical
object. Conjugation is correct for changing both bases of that operator, but
the proposed right factor is the changing DRAW basis, not a fixed definition
of the controller's local axes.

There is also a concrete convention omitted from the proposal. In
`present_tick.cpp:43-80`, g_devPose contains XR device-to-tracking matrices whose
columns are right, up and BACK. `MpDriveTick` at `mesh_split.cpp:2452-2457`
explicitly negates the head's third column to publish right/up/FORWARD position
coordinates. The rotation path must represent that conversion too.

For column-vector math, define:

```
R_H = head local XR axes -> tracking axes
R_C = controller grip local XR axes -> tracking axes
F   = diag(1, 1, -1)                // XR right/up/back -> right/up/forward
B   = [ drawRight | drawUp | drawForward ]
```

The working position path is equivalent to:

```
p_C = k * B * F * transpose(R_H) * (p_controller - p_head)
      + existing eye correction
```

Using the SAME physical mapping, the controller pose orientation is:

```
O_C = B * F * transpose(R_H) * R_C
```

O_C maps fixed controller-local XR axes into the draw's camera-relative world
axes. Any fixed controller-to-palm axes convention belongs in the right-hand
grip transform G. B and F each have negative determinant on the verified path;
their product has positive determinant. Validate this rather than discarding a
reflection or forcing an improper matrix into a quaternion.

This derivation is for the measured rigid, symmetric-projection path. Preserve
its existing qualification gates. Do not transplant it into an unqualified
weapon viewmodel shader or a differently rebased pose without checking that
path's coordinate conversion.

### A counterexample already evaluated during this review

Use a synthetic common frame with a stationary controller, `R_C = I`, a turning
head, and `B = R_H * F`. This describes the same head-dependent basis used by
the position conversion. Expected controller orientation in the common frame
is identity throughout.

| Head yaw | Plan formula's output rotation | Corrected pose conversion |
|---|---:|---:|
| 0 degrees | 0 degrees | 0 degrees |
| 45 degrees | 45 degrees | 0 degrees |
| 90 degrees | 90 degrees | 0 degrees |

The original formula would reintroduce head-driven hand rotation. This is an
offline mathematical counterexample, not a claim about a headset run.

Implement typed/named frame conversions and test identity, each axis at
positive and negative 90 degrees, and combined noncommuting rotations. Check
transformed controller basis vectors independently, not solely determinant and
orthogonality: a wrong orientation can be a perfectly valid rotation.

## 3. Correct grip capture and state its frame [P1]

The plan defines R_src in component-local coordinates but captures
`inverse(R_ctl_cam) * R_src` at line 206. Those operands are in different
spaces. First move the source orientation through this draw's LocalToWorld.

For a validated rigid L and the corrected controller orientation O_C:

```
R_source_C = R(L) * R_source_local
G         = inverse(O_C) * R_source_C

A_target_C     = [ O_C * G | existing_dcam ]
A_target_local = inverse(L) * A_target_C
D_local        = A_target_local * inverse(A_source_local)
M_new          = D_local * M_original
```

Consume a grip-capture request once from a qualified original draw and one
coherent pose snapshot, independently per side. Use ORIGINAL source matrices,
not the already rotated output. A calibration press may intentionally snap the
orientation back to the captured native pose; tell the tester that expectation.
If capture is meant to preserve the currently displayed pose instead, implement
and label that different operation explicitly.

Useful exact checks before installation:

- `D_local * A_source_local` reconstructs `A_target_local`.
- After zero-translation grip capture, D's rotation is identity at that instant;
  the existing positional correction remains in force.
- Repeating capture on the same frozen input is idempotent.
- The solved G is the same under equivalent head/view basis changes.
- Config Euler angles have a declared order, units and intrinsic/extrinsic
  convention, and serialize/deserialize the same rotation. Keep a matrix or
  normalized quaternion internally.

Pure rotational G with zero grip translation is a sensible first build. A real
grip-to-palm transform may later need a small controller-local translation:
`p_target_C = p_controller_C + O_C * t_grip`. That is legitimate and bounded.
Do not reintroduce a head-local offset absorbing an unknown origin. Also correct
the history: the old fixed-length lever swept a bounded arc; it did not grow
without bound as the head turned.

## 4. The dominant slot is a candidate, not proven anatomy [P2]

I independently checked 48 palette matrices in each of the 28 saved packets,
1,344 matrices total. From the printed values:

```
uniform scale estimate: 0.999511386 to 0.999513468
largest Gram off-diagonal: 1.55e-6
largest diagonal deviation from its mean: 3.06e-6
```

These support an approximately uniformly scaled rotation in these captures.
They do not support exact zero shear at unlimited precision, or a hard-coded
0.999512 for every future pose, mesh and pass. Derive a positive scale from the
current matrix, validate anisotropy/orthogonality/determinant with numerical
tolerances, and normalize only the frame used to compute D. Preserve the
original palette's scale in `D * M_original`.

The palette is a skinning transform. It is not automatically the anatomical
joint frame; an inverse-bind rotation can be folded into it. A constant bind
orientation is acceptable as a frame convention and can be absorbed into G,
provided the selected slot follows the palm rigidly. Dominant weight alone
does not prove that condition. The same rigid-matrix test also passes for
finger and forearm transforms.

Keep the dominant slot approach if the corrected animation test validates it.
Call it a palette slot until its skeleton mapping is established. Freeze the
slot and anchor per side AND validated mesh/palette-map generation, not forever
per hand class. Rebuild/invalidate on mesh rebuild, reset, LOD/remapping or asset
change. An index remaining below the palette count does not establish that it
still names the same transform. Do not silently load an old grip calibration
against a changed source-frame convention.

If this candidate fails, use the already discussed validated landmark frame,
rigid fit, or named native bone with a verified render-space mapping. A scaled
rotation is cheaper to normalize than a blend; that does not by itself make it
a better anatomical source.

## 5. Close the normal-path question using the captures already available

I read all three saved hand vertex shaders. The findings are:

| Shader hash | Relevant behavior |
|---|---|
| DB11BB81 | Position-only vertex input/output path; no normal/tangent input. |
| F2E11B73 | Decodes normals and tangents, multiplies both through the same weighted bone-palette linear rows used for positions, and derives the bitangent from their cross product and handedness. |
| 11DD5E8A | Same palette-driven tangent/normal construction, with additional light-direction handling. |

For these captured shaders, a proper rigid D applied to the palette carries
the tangent frame as well as geometry. Keep L and its WorldToLocal inverse
consistent with the unchanged component transform. In these shaders,
WorldToLocal converts view/light vectors into component space; it is not a
missing skinning-normal matrix to rotate a second time. Do not modify it merely
because it appears in the normal/lighting path.

This closes the specific disassembly question for the three hand shaders.
Retain the visual shading check and audit any new shader hash. It does not
qualify weapon shaders in advance. Keep the MeshExtension/MeshOrigin check or
implement the actual vertex decode for source anchors.

Every original palette and every other modified render state must be restored
after the isolated draw, including failures. All relevant passes must use the
same source-frame convention and pose generation. A color pass rotating while
a depth pass stays still produces occlusion artifacts even with correct math.

## 6. A validity flag is not coherent pose publication [P1 if cross-thread]

R2 proposes writing nine floats before setting the existing flag. That does not
make an atomic packet, and the flag can already be true from the previous
sample. If producer and consumer are concurrent, this is a data race and the
consumer can combine old and new matrix rows, positions or hand poses.

First establish the actual thread contract. If both execute serially on one
thread, assert/log that ownership and copy one snapshot before the hand loop.
Otherwise publish under a proper synchronization mechanism. A small locked
copy is an adequate initial solution; a double buffer needs ownership that
prevents the writer reusing storage while a reader still copies it. A bare
sequence counter around non-atomic float reads/writes does not make a valid
C++ synchronization design.

Bundle positions, orientations, head pose, both validity states, tracking-space
identity, prediction timestamps and generation. Latch one snapshot per render
view so hands and independently drawn weapons do not sample different controller
poses during that view. The existing unknown render-ticket issue remains a
limit, but basic publication coherence cannot be postponed as optional tuning.

Head look-ahead and hand sampling time can differ in the current runtime. Start
the rotation acceptance run with a documented matched-time baseline, or expose
and reject/report the mismatch. Do not calibrate G to hide dynamic lag. A full
stereo scheduling redesign is not a prerequisite to the rotation math work.

## 7. Independent weapon targets need an assembly-root contract [P1 for W2]

I agree that the weapon need not wait for a hand draw. The previous handoff
already allowed an item-specific source grip anchor for this reason. It is a
useful design if each item can observe a reliable source grip frame at its own
draw, in the same coordinate system and pose generation as the target.

However, independent targets do NOT automatically imply one common delta. Let
the common-space source frames be S_h and S_w:

```
D_h = C_grip * G_h * inverse(S_h)
D_w = C_grip * G_w * inverse(S_w)
```

For those deltas to be equal:

```
G_w = G_h * inverse(S_h) * S_w
```

Fixed G values meet that condition only when the source hand-to-weapon relation
is fixed. Animation can change it. Different deltas may be intentional when
both grip frames are independently pinned to a controller, but do not claim
that all native attachment animation is then preserved automatically.

For the weapon ASSEMBLY, choose a source frame on its stable grip/root. Compute
one root correction and retain current animated transforms of body, limbs,
string, loaded bolt and reload pieces relative to that root. One skeletal
component can apply this through its own full palette. Separate components
need either the root's same-generation transform or an equivalent reconstruction
that includes their current root-relative animation.

Do not independently pin every member's animated dominant bone to a fixed
controller-relative target: that can cancel the bolt's seating animation, moving
limbs/string, magazine travel or reload movement. Static pieces may have no
palette at all; move their verified component transform. Controller coupling
between hand and weapon can be removed; assembly ownership and animation
relationships still need representation.

### W1 identity and tests

The census is a useful candidate finder, not a complete item-instance identity.
`DcSame` records draw geometry/layout, while `DcSameGeom` intentionally groups
the same geometry across shaders. A changed vertex shader can mean another
pass over the SAME mesh, contradicting the plan's claim that any such difference
means different geometry. Conversely, two item instances can share the same
buffers, declaration and shaders.

Use an owned live first-person component, asset identity, transform/palette
context, pass family and resource lifetime/reset generation to qualify the
candidate. The decompiled scripts still provide the useful links:

- DishonoredInventoryItem: owning inventory, current socket, player/world mesh
  fields and post-update component.
- DishonoredItemSkeletalComponent: item backlink.
- SkeletalMeshComponent: AttachedToSkelComponent, Attachments, and the distinct
  ParentAnimComponent/ParentBoneMap animation-sharing relationship.

Those declarations provide relationships to validate; they do not justify
guessing native layout offsets or palette-to-skeleton mappings.

Change the W1 assertions: the active held instance should cease being eligible
after holstering/release, but raw candidates/resources may remain cached or be
used for dropped/third-person/projectile instances. Re-equipping can create new
objects and buffer addresses. Validate semantic membership and reacquisition,
not byte-identical pointer sets or disappearance of every matching geometry.

### W2 firing expectation

The plan says the bolt leaves along the tracked weapon while native projectile
origin and aim are explicitly deferred. That cannot be promised by moving the
held palettes alone. In visual-only W2, the released projectile returns to the
native origin/direction and may visibly disagree with the held crossbow.

Choose and state one scope: accept that discontinuity temporarily, or include
the existing gameplay origin/direction handoff in the firing stage. Do not
expect a correctly launched tracked bolt as an acceptance gate for a build that
does not implement it. A purely visual projectile workaround would create
another visual-versus-collision mismatch and is not the recommended next step.

## 8. Preserve the working baseline and make tests useful

- Make R0 logging-only for this phase. Do not change the headset-approved
  100/108 effective scale behavior as a prerequisite to rotation. The palette's
  approximately 0.999512 scale is a separate quantity from metres-to-world
  conversion. A later unit cleanup must account for the effective transforms,
  not merely rename an unexplained correction as component scale.
- If only the new orientation frame fails, retain valid translation-only
  placement and log rotation's refusal. Do not throw a correctly tracked hand
  back to the native position because rotation is unavailable. Invalid source
  geometry or invalid position tracking still requires the appropriate broader
  fallback.
- Preserve the eye fix without declaring the counters ground truth. In current
  code, same counts the small-jump branch that RETAINS an inferred label;
  toggled counts the jump-band branch, not a checked change in label. The 776
  cases are an instrumentation limit, not proof of 776 repeated delivered eyes.
  Carry them without threshold tuning or reopening scale as a prerequisite.
- Equal physical hand orientation across eyes means reconstructed orientation
  in a common frame. Literal on-screen angles need not be identical under two
  perspective projections of a near, asymmetric object.
- Controller following must be tested with position-only motion, orientation-
  only motion, head motion with controller fixed, and combined rotations.
  Report relative rotation error, not three Euler numbers whose wrapping or
  order can look like an axis failure.
- Claude runs the deterministic math/feature-gate checks. The tester only
  launches the already configured build and performs the short physical test.
  Not asking the tester to run a harness is compatible with developer-run tests.

## Revised execution sequence

1. Preserve the working placement build and effective settings. Correct R1's
   predictions and implement synchronized pose/frame math with automatic logs.
   Run the stationary-controller/head-turn counterexample, calibration
   round-trip, pivot, reflection and full-compose tests offline.
2. Validate the dominant palette slot against actual native palm movement and
   finger-only movement. Use a measure-only gate only for evidence that needs
   it, not for the impossible physical-wrist test.
3. Apply full rigid hand rotation with corrected G capture, zero grip
   translation initially, and automatic translation-only fallback. Reuse the
   verified normal paths. Test all three controller axes, head turns, combined
   motion, independent hands, caps, animation and both-eye consistency.
4. Identify one live weapon assembly, define its stable source grip/root, then
   place that assembly while preserving root-relative animation. Use a shared
   view pose snapshot; support separate components explicitly.
5. Connect native ranged origin/direction before requiring a fired projectile
   to leave the tracked muzzle correctly. Broader powers/melee/carry work stays
   outside the rotation/grip acceptance gate.

Keep the requested Release installation workflow for Claude's eventual builds:
stage features enabled in the installed ini, repository defaults per project
policy, and a working live fallback. This review does not perform those actions.
