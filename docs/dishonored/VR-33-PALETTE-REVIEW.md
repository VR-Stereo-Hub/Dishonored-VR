# VR-33: keep the palette backend, implement absolute placement next

Reviewed 2026-09-07 against `fb7e6322`, BRIEF-vr33-palette.md, the current
render and OpenXR code, and the local decompiled scripts. Review only: no
implementation changed, game launched, build installed, or headset result
verified here.

**Recommendation:** retain the separate hand draws and common palette
transform. Replace the neutral-relative drive with absolute placement of a
selected palm anchor. Then add controller orientation and a calibrated grip
transform. Do not spend another series of builds adjusting neutral spaces or
yaw signs. The latest fix changes the added displacement, but does not cancel
the hand's original animated placement.

## 1. What the evidence supports

- The split and caps have a confirmed working baseline. Preserve it.
- A draw-scoped translation moves the selected hand independently. This is a
  useful rendering mechanism and does not need another anatomical pivot guess.
- The reported axis tests support left/down/forward for the tested palette and
  camera-relative displacement under the tested turns. Use this as the working
  conversion for the next build, with numerical and rendered checks inside it.
- The weapon stayed behind when the hand moved. The GPU hand transform does
  not automatically update the item's render transform or engine attachment.
- The named attachment joints descend from the corresponding hands. That is
  still useful for a future native implementation, but is not attachment-update
  ordering or proof of the live equipped item's parent.

Test 7 is useful evidence about a displacement direction. It does not establish
the palette origin, all three rotational axes, projection scale, pose timing,
or absolute placement. A yaw sweep also cannot establish pitch/roll behavior.
At several fixed head poses, an immediate offset-on versus offset-off comparison
is more discriminating than following the hand's overall position during motion.
The untouched opposite hand helps detect leakage but is a different animated
object, so it is not an exact baseline for the tested hand.

A nearly constant camera-minus-head yaw during physical head rotation is
compatible with a fixed tracking-to-game alignment, including a body orientation
that is not changing during that experiment. It does not by itself distinguish
all of those meanings. None of this justifies putting an extra scalar yaw
correction back into a camera-relative target.

## 2. The neutral fix does not guarantee a stable hand

The actual implementation is at `mesh_split.cpp:1984-2023`. Its stored value is
controller position minus head position, expressed in XR tracking axes. This is
a head-relative vector, not the controller's position in the game world, and
not a fixed tracking-space point. It changes when the head translates.

Use one common coordinate convention below, absorbing the measured axis
conversion and unit scale. Let:

- `c, h` be controller and head positions in tracking space;
- `R` be the head rotation;
- `b(t)` be the current animated hand anchor in camera-relative coordinates;
- `w = c - h`, with `w0` captured at recenter.

The new displacement and resulting position, in the ideal camera-relative case,
are:

```
d_new = inverse(R) * (w - w0)
p_out = h + R * (b(t) + d_new)
      = h + R * b(t) + w - w0
```

The brief demonstrates cancellation in `R * d_new`, but omits `h + R * b(t)`.
That is the original hand, and `MpBuild` really does leave it present: it adds
translation to the existing matrices rather than replacing their placement
(`mesh_split.cpp:1657-1663`).

A counterexample needs no headset: hold the controller and head positions fixed,
capture neutral, then rotate the head. `w == w0`, so the new displacement is
zero throughout. An ordinary camera-following hand continues to follow the
camera. The proposed guarantee therefore fails even under its own ideal frame
assumptions.

I evaluated a numerical example with the baseline hand and controller both
40 cm forward initially, no head translation, and a constant baseline animation:

| Head yaw | New-neutral hand-to-controller error | Absolute-anchor error |
|---|---:|---:|
| 0 degrees | 0 cm | 0 cm |
| 45 degrees | 30.6 cm | approximately 0 cm |
| 90 degrees | 56.6 cm | approximately 0 cm |

These are analytical model results, not measurements of Dishonored. They refute
a general guarantee, not predict the exact size of the next headset symptom.
In this deliberately aligned example, the old head-space-neutral formula also
stays aligned: its changing delta cancels the changing baseline. Removing that
term is not automatically an improvement. With an arbitrary neutral or animated
baseline, the old formula is not generally correct either.

There is a second diagnostic distinction: at a fixed head orientation, both
formulas have the same derivative with respect to controller translation,
`inverse(R)` before axis/scale conversion. Changing a neutral changes an offset;
it does not rotate the directions of subsequent controller travel. If the
reported tilt means rotated movement axes, rather than a displaced resting
position, this fix cannot be its sole explanation.

## 3. Rendering and timing corrections to include in the placement build

**Bind the palette to the draw that consumes it.** The current cache accepts any
upload beginning exactly at c6 with 3 through 256 registers
(`vs_const_hook.cpp:224-227`). The draw accepts any cached count of at least 3
(`mesh_split.cpp:1731`). This is not a validated 48-matrix contract:

- An upload starting earlier and covering c6 is missed. An update starting
  inside the palette is missed too. This exact batching issue already exists
  in the c5 handling at `vs_const_hook.cpp:22-35`.
- A static mesh's c6 x4 upload can become the cache, despite not being a bone
  palette. The code itself distinguishes that case elsewhere in this file.
- The same geometry can be consumed by different shaders/passes. Geometry
  identity does not establish the shader's register layout or output space.
- There is no palette invalidation on device reset in the current cache's
  references. Reusing constants within valid device state is fine; treating
  old state as valid across reset is not.

For the first absolute build, snapshot the actual constants at a qualified draw
if the device supports the getter, or maintain a validated shadow of every
overlapping update. Qualify the shader, declaration and register layout; bound
all nonzero vertex influences by the available palette. Restore the exact
pre-draw device state on every exit. A copy of the last game-requested upload
is not necessarily that state when another mod writer has changed it. Keep
competing hand/palette writers disabled while this backend owns placement.
Handle other observed hand passes explicitly; an unsupported pass may leave a
stationary contribution and must not silently count as a fully placed hand.

**Finish the existing geometry contract.** `MsDraw` is not passed `startIndex`,
although the build stores it, so it cannot compare that part of the contract.
It reads the stream stride but does not compare it either. Its caller's
`DcIsLocked` checks VB/IB identity only (`draw_census.cpp:177-193`). Pass and
check the missing range/stride fields; a failed declaration/state query should
not certify compatibility. This is a bounded correction, not a new mesh hunt.

**Make the pose generation explicit.** The present-side consumer already uses
grip poses (`present_tick.cpp:74`) and reads head/hands in one routine, which is
a useful starting point. However:

- Head location uses `predictedDisplayTime + paceAhead * period`
  (`openxr_runtime.cpp:3062-3067`); hands use `predictedDisplayTime`
  (`openxr_runtime.cpp:3191`, `openxr_input.cpp:537-540`). They match when
  look-ahead is zero, the source default. Check the installed value, and make
  the target consume matching prediction times when it is nonzero.
- `HandSlot` has ordinary float fields and a relaxed atomic validity flag
  (`openxr_input.cpp:107-118, 622-630`). That flag does not make a coherent
  cross-thread snapshot or protect concurrent float reads/writes. Use an actual
  synchronized snapshot for the consumers that cross threads.
- The new drive globals are described as present-written/render-read. Either
  establish that those callbacks are serialized on the same thread, or publish
  a complete target packet safely. Include pose time, generation and validity.

Do not automatically subtract the newest head pose from an older camera draw.
Associate the target with the camera/eye transform used for that draw. Start
without late-latching; add it only as a deliberate, consistent later feature.
These timing issues can cause dynamic error, but are not proven causes of the
reported fixed tilt and do not replace the baseline-placement correction.

## 4. Revised implementation sequence

### Build A: absolute position of one palm anchor

Keep orientation animated initially. Reuse the working split, caps, per-hand
draws and translation writer. Implement the required state checks above as
part of this build, not as a succession of headset probes.

1. Select a small, fixed palm patch on the current mesh. Use persistent vertex
   identities/barycentric coefficients, not a changing centroid of the entire
   hand, a finger tip, or a guessed palette translation column. This is an
   explicit visual anchor, not a claim to have found an anatomical joint.
2. Evaluate its current skinned position from the unmodified draw palette:

   `q_palette = sum_j a_j * sum_i weight_ji * M[index_ji] * vertex_j`.

   Position, blend indices and weights are already copied into `g_msVert`
   (`mesh_split.cpp:270-275`). Verify the selected shader's index addressing,
   packing and weight treatment when using that data. Match the GPU rather
   than silently normalizing only the CPU copy. The affine common-transform
   identity assumes effective weights sum to one; verify that for the anchor
   and rendered hand rather than relying on the split's loose census tolerance.
   Project a marker at this
   computed point through the draw's actual downstream transform to confirm
   that it lands on the chosen palm patch before enabling displacement.
3. Convert the controller grip position into the same palette-output space.
   Under the current camera-relative hypothesis, the initial model is
   `target = originOffset + scale * B * inverse(R_head) * (c - h)`, where
   `B = -I` is the measured translational basis. Establish the origin offset
   and effective projection relationship in the same build; the axis test did
   not measure them. Account for the actual draw camera and eye, and any
   viewmodel-specific projection. Do not hide perspective mismatch in a
   different gain for each hand or axis. The code defaults WorldScaleUU to
   100, while earlier work suggested a different effective viewmodel scale;
   record the actual configured value and validate known distances at more
   than one depth instead of treating either number as settled here.
4. Set `translation = target - q_palette` at draw time and apply it to every
   matrix consumed by that hand class. Compute the anchor from the original
   palette each time, never the previously transformed palette.
5. Keep the other hand as the unchanged reference for the first run. Add both
   hands after the one-hand acceptance check passes. Invalid tracking or an
   invalid draw contract gives an explicit fallback to original placement.

This does not require a reference-skeleton-to-palette map. Skinning the actual
vertices answers where a point on the visible hand is, without pretending that
a matrix translation is a joint position. A validated named-bone transform is
an alternative source anchor, but converting it into this render space is an
extra contract, so it need not block the first implementation.

Acceptance in the simulator first: fixed controller while the head yaws,
pitches, rolls and translates; controller-only movement along each axis;
baseline animation moving while the controller is fixed; independent hands;
stick turn; tracking loss; device reset. Log the source anchor, target,
corrected anchor and frame generation. Check numerical residual and the
projected marker, since a zero residual alone only validates the internal
arithmetic. Then do one headset acceptance run. F6 should not be needed to
recapture an arbitrary resting pose for positional stability.

### Build B: orientation and grip alignment

Represent the desired and current palm frames in the same space. Prefer the
validated named hand frame if available; otherwise use a fixed non-collinear
palm patch with a validated stable orientation. Detect degeneracy and reject
an orientation inferred from finger animation or a nearly collinear patch.

Calibrate a fixed grip-to-palm transform per side. Its translation rotates with
the controller; it is not a fixed camera-space positional correction. With
column-vector notation, use:

```
A_target = converted_controller_grip_pose * grip_to_palm
D = A_target * inverse(A_source)
M_i_new = D * M_i_original
```

Extend `MpBuild` from translation to full rigid composition, including the
matrix linear parts used for rotated normals/tangents. Preserve the current
engine finger animation. Apply the same D to every contributing matrix for
that isolated hand draw, including influences on cut/cap vertices. This
preserves the deformation already present; it does not repair an existing
bad boundary skinning assignment.

The translational basis negates three axes and has determinant -1. It is a
change of coordinate convention, not a quaternion rotation. Convert rotations
with basis conjugation (`B * R * inverse(B)`), with all frame definitions and
grip calibration consistent. Do not negate Euler angles or quaternion
components by analogy with positions. Full inverse removes inherited wrist
orientation; a translation-only build cannot validate this part.

Acceptance: palm anchor remains on the grip while each controller rotates
about all axes; natural grip alignment survives head turns; fingers animate;
caps remain intact. Validate both hands separately and together.

### Build C: one equipped weapon, then other interactions

Give the same canonical controller target a second consumer for the actual
equipped item. Start with one known weapon and validate its complete rendered
assembly, including reload parts, before expanding.

Prefer an engine transform/attachment mechanism that updates the item at the
correct point in its native lifecycle. If that is not yet available, a
qualified weapon-render transform can provide an interim visual implementation.
GPU weapon movement is possible in principle; the fact that the hand palette
does not move weapons automatically does not mean all weapon visuals require
native skeleton writes. Static pieces and skeletal pieces need their own
verified transforms, not a c6-count or neighboring-draw guess.

Treat visible alignment, muzzle effects, aim/projectile origin, melee contacts
and carried objects as separate acceptance checks. A GPU-only implementation
does not update CPU gameplay consumers. Avoid applying both a native following
transform and the same GPU correction to a weapon.

## 5. What the decompiled scripts add, and do not establish

Rechecked locally under `tools/uscript/dishonored/`; these are references to
local artifacts, not game code copied into this document.

| Source | Useful consequence |
|---|---|
| `Engine/SkelControlBase.uc:38,50` | Declares ControlTickTag and a script tick event; does not expose the native tag writer or establish pose-evaluation semantics. |
| `Engine/SkelControlSingleBone.uc:6-16` | Apply flags, additive flags and translation/rotation spaces are separate. Strength alone does not establish an active transform. |
| `Engine/AnimObject.uc:13` | SkelComponent provides control ownership information, beyond merely finding objects in GObjects. |
| `Engine/SkeletalMeshComponent.uc:284-311` | Named bone location/quaternion, matrix and reference-position queries are available declarations. Their output space and timing still need validation. |
| `Engine/SkeletalMeshComponent.uc:54-60,122,139-143,233-254` | Attachment transforms, actual attachment parent and attachment queries exist; ParentAnimComponent is a separate animation relationship. |
| `DishonoredGame/DishonoredInventoryItem.uc:86-92,110` | Item ownership, equip usage, socket category, first-person mesh and a separate post-update component are distinct pieces of the live item path. |
| `DishonoredGame/DishonoredItemSkeletalComponent.uc:6` | Component-to-item backlink can identify a live assembly once the correct component/subclass is found. |
| `DishonoredGame/DisTweaks_WepPistol.uc:28` | Pistol default uses LeftHandWpn. Right-hand weapon versus left-hand power is not a universal equipment assignment. |
| `DishonoredGame/DisItemContext_ProjectileAttack.uc:5-28` | Cached aim data and a camera-update firing field show why drawing a moved gun is not enough to redirect shots. |
| `DishonoredGame/StatePlayerCarryCorpseIdle.uc:29-39` | Corpse carrying has its own native state, including acquisition/drop and camera-offset concerns. It is not established as an ordinary weapon attachment. |

The global sweep now establishes that the sampled tag did not advance for the
66 discovered controls. It does not prove that no native bone/attachment path
can work. The local decompilation contains declarations, not the native
evaluation implementation. Replace the broad impossibility verdict in the
brief/ENGINE_NOTES/STATUS with the measured result and the decision to park
further SkelControl experiments. Keep the expensive global sweep off. Do not
reopen cadence guessing to correct the wording.

[Epic's UE3 controller documentation](https://docs.unrealengine.com/udk/Three/UsingSkeletalControllers.html)
also describes controls as chains applied to bones, with metadata, LOD and
visibility conditions. It does not certify this game's tick-tag semantics.
It supports retaining the distinction between an object existing and its
transform influencing the current pose.

## 6. Direct answers to the brief

1. **The neutral fix is not established as the solution.** Its cancellation
   proof covers an added vector and omits the original rendered hand. The
   counterexample above already falsifies its stability guarantee.
2. **Test 7 supports a working displacement-frame hypothesis, not a full
   pose conversion.** Proceed with that hypothesis and validate absolute
   placement inside the next build; do not demand another investigation phase.
3. **Implement absolute placement now.** The current renderer and copied skin
   data provide a practical source anchor without a full skeleton/palette map.
4. **Keep the palette backend.** It is a demonstrated visual mover and a useful
   independent check for future native work. Full weapon and interaction
   integration remains separate work with the same controller target.

The next meaningful result is a palm anchor staying at the controller while
the head and baseline animation move. That is a better acceptance criterion
than a relative hand looking less tilted after another recenter.
