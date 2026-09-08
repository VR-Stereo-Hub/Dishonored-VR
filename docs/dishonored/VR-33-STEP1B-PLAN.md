# VR-33: review of step 1 and revised plan for 1b-2

Reviewed against `182022e0`, the supplied step-1 results, the current probe
implementation, and the decompiled corpus at `tools/uscript/dishonored/`.
The reported socket values are supplied runtime evidence; this review did
not independently rerun the game. Only this review document was added.

**Decision:** keep the native-pose investigation, but change its prerequisites.
First fix the probe's execution lane, query the named bone relationships
through the component API, and establish the equipped-item attachment graph.
Do not start with a guessed native RefSkeleton stride or require its count to
equal 48. Keep palette mapping as a separate render correspondence task.
The previous overall plan still applies after the more precise gates below.

## Corrections to the step-1 interpretation

| Claim | What the supplied evidence actually establishes |
| --- | --- |
| The wrist is identified on both sides. | Several sockets name `hand_L_jnt`. It is a useful anatomical candidate, to validate through bone queries and geometry. `hand_R_jnt` is still an inferred spelling until a lookup succeeds. The sockets do not identify every contributing hand/forearm skinning bone. |
| `handAttachment_*_jnt` is a child of the hand chain. | Not measured. Socket `BoneName` tells which bone owns the socket, not that bone's skeletal parent. Query parents and ancestry for BOTH attachment joints. They might be descendants, siblings, or separately controlled. |
| The right carries weapons and the left casts powers. | The asset declares left and right weapon attachment sockets, plus left power sockets. It does not say which current item uses which socket. The scripts explicitly default the pistol to `LeftHandWpn`, and assign pistol and crossbow secondary equip usage. Equipment usage and handedness must be resolved per item. |
| A zero-offset weapon socket is the attachment joint's complete frame. | Only its translation was printed. Rotation and scale must also be read; zero translation alone is not identity. |
| `camera_jnt` explains why the viewmodel is rendered in camera space. | A camera socket refers to that bone. This neither proves the viewmodel's rendering frame nor excludes external component/view/projection transforms. Its camera role was already recorded in ENGINE_NOTES:840 and :977. |
| A correct CPU hand write makes weapon, muzzle, and effects follow for free. | This requires verified ancestry, live attachment relationships, descendant recomposition, update ordering, and no later writer. Firing and cached aim can remain camera-driven even if the visible muzzle follows. |
| RefSkeleton must validate against 48 bones. | 48 is the observed GPU palette capacity for a draw, not an independently established full skeleton count. Render palettes can subset/remap bones, omit non-skinning attachment bones, or contain unused entries. Equal counts would not prove equal ordering either. |

The named sockets are useful progress. They establish concrete lookup targets;
they do not yet establish the dependency chain on which the proposed write rests.

## Issues in the current probe to address first

**Execution lane is misdocumented.** State chunk 56 says the report runs on the
script lane beside ArmFollowTick. The actual path is:

```
DvrGameTick                         present_tick.cpp:225,292
  -> DcTick                        hands/draw_census.cpp:551,556
     -> PrTick                     hands/pose_report.cpp:157
        -> PrDumpSockets
```

The command poll also runs from `DvrPreTick` (`present_tick.cpp:11-16`), and
`PrCommand("dump")` calls `PrDumpSockets` directly (`pose_report.cpp:185`).
There are no ProcessEvent bone calls here yet, but adding them in either
current location would put them on the wrong execution path. Readability
checks also do not make cross-lane object lifetime or multi-field reads coherent.

Have auto/manual requests post work; consume it on a verified game/script
execution boundary, with an owner check and recursion guard. The existing
ProcessEvent handler is a dispatch location, not automatically the correct
animation phase. Record actual thread ID and phase. Publish an immutable
result for present/render diagnostics. Do not attach this probe's availability
to `g_dcOn`: DcTick currently returns before PrTick when the draw census is off.

**The current report is not yet an equipped-item report.** It resolves offsets
for inventory/items/controls, but its object walk only visits the pawn's mesh,
asset, and sockets. No live inventory traversal, equipped-item selection,
attachment traversal, hand-control identity, or item socket census exists in
`pose_report.cpp`. Offset resolution is not a measured live relationship.

**Some failure cases currently resemble successful measurements.**

- PrTick sets `g_prDumped` before the dump can fail on an unready mesh/asset;
  it does not automatically retry for that failure or a replacement pawn.
  PrResolve also makes the first resolution pass terminal unless manually reset.
- The dump initializes location to zero and prints it even if that field is
  unreadable. Print UNKNOWN with a validity flag instead.
- RelativeRotation/RelativeScale offsets are resolved but never dumped.
  Preserve complete FNames, including their number component, rather than
  interpreting only the name-table index as a unique identifier.
- The socket header check bounds count but does not check capacity against
  count. Object checks are readability/alignment checks, not class and lifetime
  validation. A fixed 0x60 read guard is not a general validation of all resolved
  fields; RelativeScale begins at 0x60 in the supplied report.
- The walk stops at 64 entries and silently skips unreadable entries. Report
  declared/read/skipped counts and explicit truncation, including per-entry
  failures. Nine reported entries fit the current cap, but completeness should
  be an output rather than an assumption.
- PrResolve's text still attributes a lookup miss to a different game layout.
  The previous two misses demonstrated why missing owner classes, readiness,
  and lookup limitations are also possible causes.

These are focused instrument corrections, not a request to rewrite unrelated
reflection or rendering systems.

## Revised step 1b: semantic bone and attachment queries

### 1b.1 Validate the query wrappers and owning objects

Resolve the locally controlled pawn and its mesh on the script lane. Validate
class, object identity/lifetime, owning controller, asset, and relevant generation.
Do not treat a readable cached `g_pePawn` as a permanent proof of ownership.
Reacquire after load, mesh replacement, or object recreation.

Resolve functions on the appropriate class hierarchy, not just by first matching
function name in GObjects. `FindFunctionObj` in `ue3/uobject.cpp:182-205` currently
returns the first UFunction with that name without checking its declaring owner.
Check the wrapper's parameter layout, return offset, total size, FName handling,
and optional/out parameters against reflection or the native exec wrapper.
Do not copy guessed C structs for function arguments.

Start with fixed-size parameters/results. Use `GetBoneNames(out array<name>)`
only after its engine allocation and cleanup contract is implemented correctly;
do not let the engine resize a caller-owned stack buffer or free its allocation
through the mod's unrelated CRT. It is optional for the first useful result.
These queries are for observation; do not enable reference-pose mode, force
animation updates, change control flags, or alter the skeleton to make them work.

### 1b.2 Resolve the targets and prove their relationships

The local `Engine/SkeletalMeshComponent.uc` declares all of these:

| Query | Use in this probe |
| --- | --- |
| `MatchRefBone` (:290) | Resolve `hand_L_jnt`, candidate `hand_R_jnt`, both `handAttachment_*_jnt`, and `camera_jnt` to reference skeleton indices. Treat missing names explicitly. |
| `GetBoneName` (:293) | Round-trip every resolved index to the exact FName. |
| `GetParentBone` (:302) | Walk each hand, attachment, and camera chain to its root; detect cycles and invalid/missing parents. |
| `BoneIsChildOf` (:308) | Cross-check attachment-under-hand and camera-under-hand relationships. Keep the enumerated parent chain as the inspectable evidence. |
| `GetBoneLocation`, `GetBoneQuaternion`, `GetBoneMatrix`, `GetBoneMatrixLocal` (:284-299) | Observe current transforms. Validate each function's space and behavior rather than infer it from its name. |
| `GetSocketByName`, `GetSocketBoneName`, `GetSocketWorldLocationAndRotation` (:239-245) | Cross-check the asset socket dump and observe each full socket pose. |

Query exact socket names observed in the supplied dump. Read full local
translation, rotation, and scale. Verify that composing the parent bone and
socket local transform predicts the socket query result in the same space,
allowing a measured numerical tolerance. Resolve enum meanings per API:
an optional integer `Space` is not automatically the SkelControl space enum.

The first report needs the five targets, their ancestors, and attachment
relations. A full skeleton list is useful but not necessary to answer the
architectural question. If needed, use correctly managed `GetBoneNames`, or
a separately validated RefSkeleton array header count to bound `GetBoneName`
queries without reading native elements. Do not enumerate by guessing 48 or
by walking arbitrary indices until something happens to look invalid.

**Gate:** each name/index round-trips or is explicitly absent; parent chains
terminate; both claimed attachment relationships have a yes/no result; the
camera's relationship is known; full socket poses agree with their bone/local
composition. If attachment joints are siblings, report that and revise the
pose targets instead of proceeding as if a wrist edit will carry them.

### 1b.3 Complete the live equipment/attachment report

Follow the actual ownership graph:

```
DishonoredPawn.m_pInventory
  -> DishonoredInventory.m_EquipUsageInfo[usage].m_iEquippedSlot
  -> DishonoredInventory.m_Slots[slot].m_pItem
  -> item.m_pOwningInventory, m_EquipUsage, m_CurSocket
  -> item.m_pPlayerMesh and its actual attachment/animation parent
```

Validate ownership in both directions and all array/enum bounds. The slots are
structs, not an array of item pointers; derive their actual layout. Relevant
declarations are `DishonoredPawn.uc:249`, `DishonoredInventory.uc:5-16,31-56`,
and `DishonoredInventoryItem.uc:86-92`. The current probe does not yet resolve
every field this path requires.

`m_CurSocket` is an EItemSocket category such as equipped/holstered. It is not
a socket FName. Relate it to the live item's `DisTweaks_InventoryItem.m_Sockets`
selection using a validated tweak-owner relationship, or verify the actual
attachment directly. If one route is unavailable, mark it unknown and use the
other as evidence; do not substitute the class default for a live measurement.

`Engine/SkeletalMeshComponent.uc:54-60,143` declares an Attachments array with
child component, bone name, and local translation/rotation/scale. Its
`IsComponentAttached` query (:251) can cross-check known item component pointers.
`FindComponentAttachedToBone` (:248) returns one component, so it cannot prove
the list of all children is complete. The iterator API has a different calling
contract from an ordinary function; do not invoke it as a simple getter.
Also inspect `ParentAnimComponent`, `ParentBoneMap`, and transform-inheritance
flags (:139-161): animation sharing and component attachment are distinct.

Dump sockets on each selected item's own mesh asset, not only the arms asset.
Include the pistol bullet/reload component, muzzle effects, and crossbow arrow
component. Repeat on meaningful equipment transitions: sword with empty/power
hand, pistol, crossbow, Heart, and an unequip/reload transition. Record item
class/identity, equip usage, actual physical side, active component, attached
bone, and source of that conclusion. Keep mod handedness preferences separate.

**Gate:** for each tested item there is a demonstrated path from the local
inventory to the active mesh and its actual parent frame, or an explicit
unresolved relationship. No generic right-hand-weapon assumption remains.

## Step 1c: native layout and GPU mapping are separate questions

Do not make a complete palette map a prerequisite for read-only native update
tracing. Named CPU queries already let that investigation observe the correct
objects. A later raw CPU pose write still requires validation of the specific
native buffer's layout and indexing. A palette backend requires its own map.

Maintain distinct identifiers:

```
reference skeleton index       anatomical joint
CPU pose-buffer index          validated for that specific buffer
render palette slot            particular draw/shader/LOD/chunk
vertex blend-index encoding    decoded by that shader
parent animation map index     a separate component-to-component relation
```

RequiredBones and ComposeOrderedRequiredBones are not automatically a GPU
palette map. The RefSkeleton count and render palette count must be reported
separately. A camera or attachment bone may have no corresponding uploaded
slot at all. Only print anatomical names beside `g_msHandBone`/`g_msBoneHand`
once their palette slots are demonstrably mapped.

If raw RefSkeleton/bind data becomes necessary, derive stride, field offsets,
encoding, and native element type from the actual accessors/disassembly, then
cross-check against the query-produced names and parents across the table.
Validate header bounds, complete memory spans, finite transforms, valid parents,
and bind/inverse-bind relationships. Reflection of the property address does
not establish its element layout. `RefSkeleton` and `RefBasesInvMatrix` have
native placeholder declarations in `Engine/SkeletalMesh.uc:151,155`.

Do not map bones by matching reference-pose translations to current palette
translations. These differ in pose and possibly local/component/view space,
and the palette may include inverse bind and viewmodel-origin transforms.
`GetRefPosePosition` returns a position, not a complete global bind frame.
Nearest-transform matching alone is ambiguous when bones are co-located,
symmetrical, or move together.

Prefer the actual draw's native LOD/chunk palette mapping. Validate candidates
against shader decoding and multiple synchronized, non-identical animation
poses. For example, after explicitly defining matrix conventions, test a
candidate relationship of the form:

```
palette[j] = C_pass * G_current[map(j)] * inverse(B_global[map(j)])
```

This is a hypothesis to verify, not an assertion about this game. Determine
`C_pass`, origin transforms, packing, and projection independently. Report
residuals and ambiguous matches. Missing native layouts block this mapping
route, not the already available named-parent queries.

## Step 2: measure the native update boundary before placement

### 2a. Establish the update sequence without changing the pose

Use the known local hand controls and the verified hand/attachment targets.
Trace actual native writers/consumers, not only generic ProcessEvent frequency.
Record thread, pose generation/tick, object identity, and parent/child matrices
at control evaluation, skeleton composition, post-physics evaluation where
applicable, attachment updates, item post-update, and render-pose copying.
An ordinary getter snapshot does not by itself identify the final writer.

The scripts supply concrete leads:

- `SkelControlBase.uc:23,29,40,50` exposes post-physics participation, script
  tick participation, NextControl, and TickSkelControl. Inspect existing
  behavior; do not enable callbacks, graft donors, or change chain flags as
  part of a read-only phase.
- `SkeletalMeshComponent.uc:125,144-145` exposes a control tick array and
  pre/post-physics control indices. These are not interchangeable with the
  mod's GObjects discovery slots or proof of the controlled bone.
- Player and weapon defaults request attachment updates and TG_PostUpdateWork
  (`DishonoredPlayerPawn.uc:1191-1198`, `DishonoredWeapon.uc:15-19`), and the
  inventory item's post-update component also uses that group. Measure ordering
  within it. The tick-group name is not an insertion address.

Epic's [UE3 controller documentation](https://docs.unrealengine.com/udk/Three/UsingSkeletalControllers.html)
describes ordered control chains and multiple control spaces. This supports
checking chain order; it does not identify Dishonored's native hook or prove
that its later attachment/render consumers use the result.

**Gate:** name the specific validated boundary, the last competing pose writer,
and the downstream readers it must precede. If unresolved, return the trace
and exact missing relationship, not another speculative placement build.

### 2b. Prove one named hand with a deterministic delta

Keep the controller-driven placement off initially. At the proven boundary,
apply a small fixed translation, then a small known rotation about the current
hand origin, in a documented CPU component/local frame. Test one side first.
Record explicit successful applications and downstream numerical effects.

Editing LocalAtoms requires subsequent composition of affected descendants.
Editing already composed SpaceBases requires updating the affected descendants
consistently or using a verified engine recomposition path. Changing only the
wrist entry of a composed array does not magically recompute child entries.
Preserve finger locals, scale, the other hand, the root, and camera chain.
Never reuse a modified pose as the next test's original pose.

Measure the hand, both attachment joints, representative finger joints, camera,
and actual equipped item mesh. If a verified descendant is meant to inherit a
common component-space delta D, its final transform must agree with
`G_child_new = D * G_child_original` at the same animation time. Observe the
item and muzzle after their real updates. Detect later overwrites, stale-frame
reads, and double application from stereo re-entry or repeated dispatches.

**Check the cut boundary too.** The clipped/capped hand can still carry forearm
weights outside the named wrist subtree. `MsLerpVertex` retains a parent's
blend indices and weights (`mesh_split.cpp:857-859`). Hiding arm triangles does
not remove those influences. A correct wrist-only CPU write can therefore
still stretch the cap or cuff. Measure final-vertex influence coverage and
inspect the simulator result. Do not cure that by moving a shared ancestor
that also drives the camera or opposite hand. If needed, specify a separate
mesh-boundary treatment or draw-scoped visual backend; that is an additional
change with its own baseline and deformation gate.

**Gate:** the intended hand motion reaches skinning and the real item attachment
in the same pose generation; fingers and cap remain coherent; the other hand
and camera remain unchanged; identity/off restores the baseline. Passing only
the bone query or only a memory-write counter is insufficient.

### 2c. Add tracking only after the native proof

Retain the previous plan's shared XR snapshot and full frame conversion. In a
verified common rigid frame, use:

```
A_target = controller_grip * calibrated_grip_to_anchor
D        = A_target * inverse(A_current_animated_anchor)
```

Choose and document whether that anchor is the anatomical hand or an attachment
frame; item attachment locals may animate during equip/reload. Include source
orientation, not only source position. The controller offset is not determined
by the socket's position alone.

`camera_jnt` may be a useful observation or conversion frame once its current
relationship to the actual view is measured. It is not a substitute for that
measurement. Use a verified world/component conversion as the baseline, including
full head orientation and exactly one body/recenter/eye transform. Do not zero
the camera control or substitute camera-bone space merely because its name
looks appropriate. Measure viewmodel FOV and projection independently through
the exposed player/item component fields and actual render constants.

Use the simulator for fixed controller with moving head, independent XYZ
motion, controller rotations, animation, equip/reload, tracking loss, and
checkpoint/device reset. Both eyes must consume the same controller snapshot;
native skeleton work must not advance twice for one simulation pose.

## Weapons, effects, and the four reviewer questions

The native route is promising because it may preserve real attachments. It
does not finish gameplay automatically. `DisItemContext_ProjectileAttack.uc:27-28`
declares a camera-update firing marker and cached aim result; the pistol has
its own muzzle socket/effects and bullet spawn-distance tweak. Verify visible
muzzle, projectile/trace origin, direction, Blink/reticle, and relevant melee
caches separately. Carried physics objects still use the RB_Handle path in the
previous plan and are not evidence about equipped-item parenting.

**Q1: Is raw RefSkeleton walking the best next move?** No. Prefer the named
component queries first, after fixing their execution lane and call contract.
Native structure decoding remains necessary only for specific data/writes the
queries cannot supply. Failure to decode it need not stop semantic discovery.

**Q2: Does camera_jnt justify driving in camera-bone space?** No new default.
It can be a measured intermediate frame, but the socket list does not connect
it to the renderer's final viewmodel transform. Keep the existing camera
behavior and verify the conversion rather than inventing a new compensation.

**Q3: Do asymmetric Bash/Tattoo sockets indicate missing right-hand sockets?**
No. Socket lists are authored effect/attachment points, not symmetric bone
inventories. This asset has useful left-hand points; that neither proves a
missing right hand nor proves a complete multi-component dump. Resolve the
right-hand bone through the API and inspect item assets for their own sockets.
The pistol's default `LeftHandWpn` and muzzle `Spitfire` are concrete examples
of why the arms-only dump is insufficient.

**Q4: Which established claims overreach?** Both-hand wrist identification,
handAttachment ancestry, universal weapon handedness, identity socket frames,
camera-space causation, and automatic downstream effects. The supplied data
supports socket-to-bone references and local positions; the next probe should
turn the remaining relationships into explicit yes/no measurements.

**Next deliverable:** one simulator-capable read-only report covering the
correct execution lane, verified named target/parent chains, full socket
frames, and active inventory-to-attachment relationships. Include source
identity and validity for each row. Native update tracing can then use those
objects while GPU correspondence is investigated independently. No placement
build should be sent for headset judgment before the deterministic native or
draw-scoped backend gates pass.
