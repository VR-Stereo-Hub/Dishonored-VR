# VR-33 revised implementation plan

Review of `aa984f37`, the reverted palette implementation at `1d1511a4`, and
the local decompiled scripts referenced by ENGINE_NOTES. This is a design
review, not a runtime validation. No game code or installed configuration was
changed during this review.

**Recommendation:** keep the confirmed wrist cut. Redesign placement around
one controller pose contract, verified anatomical/socket frames, and explicit
ownership of animation, rendering, attachments, and gameplay. First make a
bounded investigation of the native pose/attachment update boundary. If it
cannot be established, use a draw-scoped, animation-preserving palette backend
with separate draws for the two hands. Do not begin by changing the cut's
classification or by reviving the old upload-wide drive.

## Corrections to the previous brief

1. **The sphere mask and the wrist candidate are different heuristics.**
   `MsClassify` computes `g_msBoneHand` using the sphere, but `MsWrist` selects
   `g_msHandBone` by maximum degree in a vertex co-influence graph. Neither is
   verified anatomy. `g_msBoneCen` is an influence-weighted vertex centroid,
   not a skeletal joint position. Replacing the sphere test with a plane will
   not identify the wrist. Changing that wrist candidate can also change the
   cut's axis and origin, invalidating the already tuned `-4.9` setting.
   Sources: `hands/mesh_split.cpp:337`, `:499`, `:716`, `:1265` under
   `src/game/dishonored/`.
2. **A palette translation is not necessarily a bone origin.** The reverted
   `PdBoneOrigin` reads only the three register `.w` values. A skinning matrix
   commonly includes the inverse bind transform. Its translation then places
   the bind mesh origin, not the anatomical joint. The reported 66/108-unit
   distances and 176/215-unit spread do not establish a shoulder pivot until
   this distinction is resolved. The generic skinning relationship is documented
   in [Microsoft's skinned-model example](https://github.com/microsoft/DirectXTK12/wiki/Using-skinned-models);
   Dishonored's exact relationship still needs verification.
3. **210 units/metre is an estimate.** Commit `fc04aa3b` derives it from where
   a resting hand ought to be. It also records that head yaw did not change in
   the gameplay probe. Small palette translations while c5 moves establish
   translation invariance, not all three axes, their signs, rotational frame,
   or scale. A probe's computed controller `want` values cannot independently
   verify the coordinate conversion that produced those values.
4. **D3D9 does not mandate the claimed asymmetric rotation formula.**
   `SetVertexShaderConstantF` uploads float4 values; the shader determines their
   meaning. Decode the shader's actual dot products, index addressing, and
   matrix use. If output xyz are three dot products against the stored rows,
   an output-space delta rotates both the 3x3 basis and translation by the
   same left multiplication. The reverted transpose switch changes the basis
   by a different multiplication and needs proof, not a headset vote.
   Sources: [D3D9 constant upload](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-setvertexshaderconstantf),
   [HLSL matrix packing and use](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-per-component-math).
5. **The old rotation is also missing the source orientation inverse.**
   `PdRewrite` uses controller rotation directly as a delta around a reference
   point. Placing an already animated hand requires the difference between
   its current frame and the target frame. Otherwise native wrist rotation
   remains multiplied into the result. `PdControllerInRig` also uses only a
   yaw-flattened head basis; that is insufficient if the destination really
   follows the full camera orientation. Source: the reverted
   `hands/palette_drive.cpp:66` and `:126` at `1d1511a4`.
6. **A rigid delta can preserve finger animation.** Applying the same delta
   to every contributing skinning matrix moves the currently animated mesh
   coherently. No static-hand replacement is required. Conversely, updating
   only a wrist entry in an already composed GPU palette does not propagate
   to its children: those entries are already evaluated.
7. **Weapons cannot follow GPU edits through engine attachments.** GPU edits
   do not update the CPU skeleton, sockets, item transforms, muzzle effects,
   melee caches, or firing state. Even an engine-side wrist edit must prove
   those consumers run after it. Translation-only can also tear if some
   influences receive a different delta; it is not inherently a safe proof.

## What the decompiled scripts contribute

The corpus is at `tools/uscript/dishonored/`, referenced from the docs. These
are declaration/default-property findings. Most relevant behavior is native
C++ and its implementation is absent from the scripts. Names provide precise
targets for reflection and tracing, not verified hook addresses or call order.

| Evidence in the local corpus | Consequence for the design |
| --- | --- |
| `DishonoredGame/DishonoredInventoryItem.uc:1,86-110`: items derive from Object and expose owning inventory, equip usage, current socket, world/player meshes, and a post-update component. `DishonoredInventory.uc:51-56` exposes slots and equipped-slot state. | Resolve the equipped item through the local pawn's inventory. Do not identify weapons by palette size, assume an item is an Actor, or assume primary/secondary means left/right. |
| `DisTweaks_InventoryItem.uc:69` supplies `m_Sockets[EItemSocket]`; `DisGlobalEnums.uc:290` identifies equipped socket index 1. Pistol, Heart, and Spring Razor defaults name `LeftHandWpn` (`DisTweaks_WepPistol.uc:28`, `DisTweaks_Heart.uc:59`, `DisTweaks_SpringRazor.uc:15`). | Read the live item's actual tweaks/socket selection. Resolve that socket to its bone and attachment frame. Defaults do not establish every runtime asset's socket or controller handedness. |
| `Engine/SkeletalMeshSocket.uc:19-23` exposes socket name, bone name, and local transform; `Engine/SkeletalMeshComponent.uc:238-245,283-320` exposes socket and bone queries, parents, reference positions, and space conversions. | Prefer these semantic sources to a sphere or plane for identifying the wrist and grip. Resolve signatures and validate outputs on the script lane. A socket identifies an attachment frame, which still needs a controller grip offset. |
| `Engine/SkeletalMesh.uc:149-159` declares origin transform, reference skeleton, inverse reference bases, LOD data, and sockets. `SkeletalMeshComponent.uc:132-145` declares local/composed pose arrays, parent animation mapping, attachments, and control indices. | There is a route to recover anatomical frames and palette-to-skeleton mapping. Native placeholder types in the decompilation are not reliable C++ layouts; validate actual strides, counts, parent relationships, bind transforms, and each LOD's palette map. |
| `DishonoredPlayerPawn.uc:307-309` holds the three specific look-at controls. `Engine/SkelControlBase.uc:23,29,40,50` declares post-physics behavior, script tick participation, control chains, and `TickSkelControl`. | Resolve the player's own controls directly. A callback/control-evaluation boundary is a candidate to inspect; its existence does not prove it runs late enough. Do not resume high-frequency field writes or another unmeasured donor graft. |
| `DishonoredPlayerPawn.uc:1191-1198` and `DishonoredWeapon.uc:15-19` set forced attachment updates and `TG_PostUpdateWork`; `DishonoredInventoryItemPostUpdateTickComponent.uc:9` uses the same group. | Investigate native skeleton composition, attachment updates, item post-update, and render-pose copying in that order. Sharing a tick group does not specify their ordering. |
| `DishonoredPlayerSkeletalComponent.uc:6-8` exposes FOV enable, FOV, and offset; item components inherit it. `DishonoredPlayerCamera.uc:154-155` has separate current world/arms FOV fields. | Inspect viewmodel projection before inventing a separate physical unit scale. Read actual per-pass projection and live component fields. Do not compensate with an FOV-degree ratio or turn the fields off blindly. |
| `DishonoredWepPistol.uc:6-8` has a bullet mesh and muzzle effects; `DisWepCrossbow.uc:6` has a separate high-resolution arrow mesh. `DisTweaks_FirePistol.uc:19,29` names muzzle socket `Spitfire`. | Include child visuals and effects in the attachment census; moving the main weapon draw alone is incomplete. |
| `DisItemContext_ProjectileAttack.uc:27-28` exposes a camera-update firing marker and cached aim result. Pistol firing tweaks expose bullet spawn distance. Inventory items also cache melee extents (`DishonoredInventoryItem.uc:18-56`). | Placement and firing are separate contracts even with a CPU skeleton solution. Trace the actual shot/trace origin, direction, timing, and cache consumers. Moving a socket is not proof a bullet uses it. |
| `DishonoredPawn.uc:250`, `Engine/RB_Handle.uc:6-7,24-40`, `DisMovableComponent.uc:8,15-17`, and `StatePlayerGrabMovable.uc` describe a physics grab handle, held body, throw origin, and grab state. | Carried physical objects need a physics-handle target and release/throw integration. They are not another weapon palette. Corpse carrying has its own state and requires separate acceptance. |

## Implementation sequence and gates

### 0. Establish a baseline and one owner per subsystem

Use this combined branch, preserving the wrist cut, caps, head/body decoupling,
mirror behavior, and intended non-persistent mode veto. Record build hash and
effective configuration. The reviewed HEAD differs from `1309c3eb` only by
the review brief; verify the intended config fix in the actual next build.
The status document contains older claims, so do not use it as runtime proof.

Separate requested hands enable from effective enable and its veto reason.
Log the active pose owner, controller validity, matched draws, successful
applications, and refusals. A running probe does not imply a running drive.
Keep crouch/eye height fixed while testing hand placement. Preserve the camera
bone and its control; ENGINE_NOTES:840 records the regression from disabling it.

**Gate:** restore the known visual baseline and demonstrate in the simulator
that the selected drive actually executes. Resolve any existing cap regression
before adding motion. New placement is default off with one unambiguous A/B.

### 1. Build one read-only pose, socket, and render correspondence report

For the local player, resolve and identity-check the live mesh, hand controls,
equipped items, their first-person meshes, active socket names, socket parent
bones and local transforms, child meshes/effects, and relevant animation parents.
Re-resolve on asset/LOD changes, equip changes, load, and object recreation.

Correlate those CPU frames with shader-identified draws and actual palette
contents at draw time. Decode the shader's blend-index addressing and weight
interpretation, not just the vertex declaration. Capture matching projection,
pass, eye, source-pose generation, and controller/head snapshot identifiers.
Audit overlapping/partial constant uploads and state-block restores; constants
persist and several draws can reuse them. `draw_census.cpp:224-235` already
records that palette-gated identification missed passes on this mesh.

Distinguish joint transforms, inverse-bind skinning matrices, influence
centroids, clipping references, and grip/socket frames in both types and logs.
Keep the old sphere helper as a clearly named fallback, rather than changing
its behavior as an alleged wrist fix. Derive influence coverage from the FINAL
drawn vertices, including clipped vertices and cap vertices, with their actual
decoded weights. The mask need not form a small spatial cluster.

**Gate:** name each hand's source frame and its owning component, demonstrate
the palette interpretation on animated vertices, and establish mapping between
the CPU and rendered frame. If a relationship is unknown, print it as unknown.

### 2. Decide whether a native pose boundary is usable

Trace or disassemble the specific native writers/consumers identified above.
The desired point is after the final competing wrist/control evaluation but
before dependent hand composition, attachment updates, and render-pose copying,
or a boundary where those dependents can be updated consistently. Follow
native calls as well as ProcessEvent. Old unsuccessful writes into SpaceBases
do not establish this timing.

Once the boundary is identified, prove one wrist with a small deterministic
translation and then a known rotation. Use a named anatomical target. Preserve
finger locals and have the engine compose descendants, or update a validated
composed subtree consistently. Record downstream socket and weapon transforms
and the resulting palette; a write-survival counter alone is insufficient.
Respect the actual BoneAtom encoding and preserve the camera/root chain.

**Decision:** choose this backend if both position and rotation survive into
rendering and attachments in the same update. Otherwise record the exact
missing boundary and proceed to step 4 after completing the shared pose math.
Do not turn this investigation into another series of guessed controls or
headset trials. Do not build both production backends before selecting one.

### 3. Establish the shared 6DoF pose contract

Publish an immutable snapshot from the existing XR owner: head, grip and aim
poses, validity, reference-space generation, prediction time, and body/recenter
transform. Consume it on the engine/render lanes without making XR calls there.
Both eyes in one stereo pair must use the same controller snapshot.

Use grip for hand/object placement and aim for the pointing ray, with explicit
per-hand or per-item local calibration. Those are distinct OpenXR poses:
[OpenXR standard pose identifiers](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html#semantic-paths-standard-pose-identifiers).
Specify every source/destination frame and unit conversion. Use full head/eye
orientation where required, including pitch and roll. Apply body yaw, recenter,
positional head tracking, and eye offset exactly once, matching the renderer's
actual viewmodel frame rather than an assumed raw-head frame.

With column-vector notation after conversion into a common rigid frame:

```
A_source = current animated wrist or verified grip-anchor frame
A_target = controller grip frame * calibrated grip-to-anchor offset
D        = A_target * inverse(A_source)
```

For an affine skinning matrix mapping bind vertices into that same frame:

```
M_i_new = D * M_i_original
```

If verified skinning is `M_i = C * G_i * inverse(B_i)`, where `B_i` is the
bone's global bind transform, then `M_w * B_w` recovers the wrist frame including
`C`. Reading `M_w.translation` does not. Verify what `C` contains before treating
the result as rigid. Pack/unpack registers separately from this mathematical
convention. Never multiply a previously patched palette again.

For a translation-only test, use `D = Translate(p_target - p_source)`. For
rotation, require `D * A_source = A_target`. The grip offset belongs here from
the start; the 12.26-unit centroid distance can be an initial guess, not a
universal anatomical calibration. Recompute the source frame with animation.
If only geometry is available, a skinned wrist-ring center can seed translation;
rotation needs a verified frame or a tested fit of stable palm landmarks,
not the centroid of flexing fingers or raw palette translations.

Use controlled simulator displacements along all three axes and several depths
to verify unit scale and projection. Compare with an independently projected
controller marker and stereo disparity. A matching resting pose is inadequate.
If viewmodel FOV introduces non-rigid distortion, resolve that projection
explicitly before claiming the palette can carry a rigid physical pose.

**Gate:** host math checks for a nonzero bind origin, animated source rotation,
non-commuting rotations, mixed bone weights, and coordinate handedness; simulator
checks for known displacement and a stationary controller under head yaw,
pitch, roll, and translation. These checks precede grip tuning in a headset.

### 4. Render fallback: independent hand draws with complete palette deltas

Implement inside the validated `MsDraw` path, where the exact geometry is known.
The two hand classes already have independent index ranges. For each matching
pass:

1. Obtain the original constants and state that this draw actually consumes.
2. Construct a private palette by applying `D_left` to EVERY skinning entry
   consumed by that shader, then draw only the left hand and its cap.
3. Start from the same original constants, apply `D_right` to every skinning
   entry, then draw only the right hand and its cap.
4. Restore constants and all changed draw state on every path, using a scoped
   guard and the original device entry points to avoid hook recursion.

This makes shared bones harmless: only one hand is emitted under each palette.
It avoids requiring disjoint anatomical influence masks. Do not rotate every
constant register indiscriminately; only verified skinning entries are matrices.
Keep auxiliary transform consumers, normals/tangents, lighting, depth and shadow
variants consistent with each shader's contract. Do not apply camera-space
deltas blindly to light-space passes. Handle unsupported variants explicitly.

For normalized linear skinning weights, the correctness property is:

```
sum_i w_i * (D * M_i) * v = D * (sum_i w_i * M_i * v)
```

Thus this preserves the original finger deformation while relocating it. Test
the equality on final hand and cap vertices, and separately verify baseline
cap seams through animation. A common delta prevents new differential motion;
it cannot repair pre-existing clipping/skinning errors.

**Gate:** identity is visually equivalent to the existing split in both eyes;
one hand moves independently; the other hand and unrelated draws are unchanged;
constants are restored; no extra transform accumulates on repeated passes.
Then test rotation about the verified anchor, with measured anchor error and
preserved deformation. Count actual successful draws, not attempted writes.

### 5. Complete equipped items, effects, and gameplay

This is required for VR-33, not an optional follow-up justified by a hand-only
success. Use the inventory/socket census from step 1 to preserve the current
item-to-hand relationship, including item animation and separate components.

With the native backend, verify that the attachment update consumes the new
pose. With the render backend, transform each identified item/component into
the same target hand frame, converting between their actual spaces. A numeric
copy of the hand's camera-space delta is not valid for a world-space attachment.
Choose one transform owner for each object to avoid applying motion twice.

Drive the visible muzzle, crosshair/Blink endpoint, shot origin and direction,
and relevant melee geometry from the agreed controller/gameplay pose contract.
Inspect cached aim and melee state as well as sockets. Verify firing with the
head pointed away from the controller, and test near-wall muzzle obstruction.
Do not move only the crosshair. Preserve authored finger/weapon animation,
while defining which native wrist swing/recoil is replaced by tracking.

Test sword, pistol, crossbow including arrow, Heart, Spring Razor, empty/power
hand, equip/unequip, reload, and temporary animation states. A two-hand reload,
mantle, or assassination needs an explicit temporary pose policy; independent
controllers cannot automatically maintain an authored two-hand contact.

Carried bottles and other physical movables use a separate RB_Handle adapter
on the appropriate simulation lane, with measured target-write survival,
collision behavior, release and throw velocity. Do not disable physics or
move their pixels as a substitute. Keep this distinct from equipped items and
state its scope explicitly. Corpse carry is a separate state-specific case.

### 6. Acceptance before asking for another headset evaluation

Run the existing build/lint/export checks and deterministic simulator cases.
The test report must include build/config, chosen backend, effective enable
state, object/draw identity, source and target frames, successful applications,
maximum anchor error, state restoration, and both-eye evidence.

Minimum cases: identity; independent XYZ movement; controller yaw/pitch/roll;
animated fingers; fixed controller with moving head; stick turn/recenter;
equip/reload/powers; matched depth/material passes; tracking loss/recovery;
pause/resume; device reset; checkpoint reload and object recreation. Tracking
loss must select a bounded fallback rather than retain an arbitrary stale pose.

Only then ask for perceptual judgment: physical hand alignment, grip comfort,
scale, cap appearance, and residual lag. Keep each build's behavioral change
isolated and its A/B reversible. A successful render backend proves visual
placement only until the separate attachment and gameplay gates also pass.

## Direct answers to the original review questions

1. The palette is a viable visual backend, not yet the proven best complete
   solution. Draw reuse, missed passes, stale constants, stereo timing, and
   untouched CPU attachments are real concerns. Draw-scoped ownership is the
   recommended palette redesign; native late-pose integration merits the
   bounded investigation because the scripts identify specific consumers.
2. Use live sockets, verified skeleton hierarchy/bind data, and per-LOD palette
   mapping for anatomy; use actual final vertex influences for render coverage.
   They answer different questions and should not share one misleading mask.
3. Preserve animation immediately with a common delta. A static hand or axis
   marker is useful only as a small diagnostic for pose conversion. Replacing
   one GPU wrist matrix does not re-evaluate children.
4. The shoulder diagnosis, 210-unit scale, full coordinate frame, universal
   transpose convention, and automatic weapon following are not established
   by the brief's evidence. Correct these before implementing its next steps.
