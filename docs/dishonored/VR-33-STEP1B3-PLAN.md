# VR-33: review of bone-query results and plan for live attachments

Reviewed 2026-09-07 against `acff837f`, the supplied step-1b results, the new
bone-query implementation, and the local decompiled scripts. The runtime
results are supplied evidence; this review did not rerun the game. Only this
review document was added.

**Decision: proceed with 1b.3, then 2a.** The reported direct parent links
support the native hand-control approach. Keep those findings and correct
the probe's remaining failure handling before expanding it. A new search for
anatomical names or a guessed RefSkeleton element layout is unnecessary.

**What the run establishes**

- The five target names round-tripped through MatchRefBone and GetBoneName.
  The reported indices identify the targets on this component's reference
  skeleton, including the previously inferred right-hand name.
- GetParentBone reportedly returned each hand as its attachment joint's
  immediate parent. That is useful positive evidence for wrist inheritance
  after correct descendant composition and attachment updates.
- Skeleton indices 54 and 56 cannot directly address a 48-slot palette.
  Keep skeleton indices and draw palette slots separate. This does not yet
  identify which bones are uploaded, whether either target has a slot, or
  whether the selected palette reorders its included bones. Tie any future
  correspondence claim to the same component, asset, LOD, draw, and shader.
- The reported paths place camera and arms on distinct branches below
  Root_jnt. This removes a direct skeletal-parent relationship through which
  a correctly scoped wrist edit would propagate to the camera or other hand.
  It does not prove that an implementation cannot disturb them through shared
  controls, another-bone reference spaces, component transforms, or later
  native consumers. Keep camera/opposite-hand measurements in the motion gate.

**A. Tighten the instrument before extending its authority**

These corrections do not erase the positive hand-to-attachment observations.
They limit what the current report can call a complete validated result.
In particular, RealName deliberately returns NULL for both index-zero None
and invalid names (`ue3/uobject.cpp:30-36`). Test the verified full-FName
sentinel before display-name validation; NULL from RealName cannot distinguish
a root from a failed lookup.

| Source finding | Required correction |
| --- | --- |
| `hands/bone_query.cpp:182-187` reports ROOT for an unreadable parent name, the text None, or a matching name-table index. A self-parent reaches that branch before cycle detection. | Return explicit statuses for root, invalid name, failed call, cycle, and depth limit. Only the verified full-FName root sentinel terminates successfully. Check full-name self-parent/cycles as errors. |
| `BqWalk` returns only a chain length. `BqRun:286-303` can issue NOT-a-descendant after a failed or incomplete walk; exhausting the bound has no explicit truncation result. | Return status plus validated edges. A negative ancestry verdict requires complete successful termination. A positive edge can be retained as a separately labeled observation without declaring the whole chain complete. |
| Only the starting bone is round-tripped (:160-175); returned parents are followed without that cross-check. A failed target round trip does not stop the batch at :278-279. | Validate every nonterminal parent against the same mesh and preserve both FName words. Stop dependent work on a call-contract inconsistency; report a legitimately missing optional target separately. |
| Parameter frames are hand-written (:105-135). No function parameter metadata validation appears in this implementation. | Record the build-specific evidence for parameter offsets, widths, return location, and frame extent, then compare the fixed wrappers against it. Reflection or the native exec wrappers can provide that evidence. A successful run is useful precedent, but is not a completed descriptor-validation gate. |
| `g_bqDepth` is a global integer, and BqCall resets `g_peReentry` to false unconditionally (:95-101). `g_bqThread` only records the current thread (:209). | Use scoped thread-local diagnostic depth and restore the previous reentry state. Verify the execution thread/phase independently. A global depth can suppress unrelated observations on another thread, and logging a thread ID does not establish it is the intended one. |
| `g_bqDone` is set before resolution/calls succeed (:216), with no automatic pawn/component/asset-generation invalidation. Requests and enabled state are shared directly across threads. | Distinguish attempted, complete, refused, and waiting states. Reacquire after object/asset replacement. Use synchronized request publication and immutable result publication. Treat run/off/re-resolve commands as requests consumed by the owning thread. |

Full FName support is also narrower than its comments claim. The starting
names are reconstructed from strings with Number zero (:152-159), and the
declared socket-name snapshot is unused. The successful seed round trips
support Number zero for these particular targets; they do not validate a
general socket-copy path. Seed measured socket targets from actual full
FNames, preserve numbers in logs and final ancestry comparisons, and keep
inferred target spellings explicitly labeled until lookup succeeds.

Function resolution now checks the declaring owner's short name, which is
an improvement. Finish by checking the actual owner class identity against
the receiver hierarchy and rejecting ambiguous matches. The Super-offset
probe scans for three names in order, allowing intermediate classes. The
decompiled chain is DishonoredPlayerPawn, DishonoredPawn, GamePawn, Pawn,
Actor, Object. Log and cross-check the complete chain, including class object
types and unique candidate offset, rather than describing three observed
milestones as three direct links.

Correct the documentation that says g_peReentry is never read. It is read
in `console.cpp:72` and `commands.cpp:319`; it simply does not guard all of
PeHandler. Likewise, the new depth guard covers the new bone-query calls,
not every older mod-originated ProcessEvent call. Account for those other
sources before treating a ProcessEvent trace as purely game-generated.

**Acceptance:** retain the useful recorded direct-parent results. Run the
corrected bounded report once in the simulator, with explicit edge validity
and termination status. Exercise malformed names, self-parent/cycles,
truncation, and partial-chain verdicts using offline fixtures rather than
deliberately invalid engine calls. Demonstrate reload/replacement reacquisition
and query-origin exclusion before using the probe for the next measurements.

**B. Step 1b.3: prove live equipped-item identity and attachment**

Use two directions of evidence. All decompiled paths below are under
`tools/uscript/dishonored/`.

1. **Resolve inventory selection.** Follow the validated local pawn's
   `DishonoredPawn.m_pInventory` (:249) and require the inventory's m_pOwner
   to agree. `DishonoredInventory.uc:31-57` distinguishes m_Slots from
   m_EquipUsageInfo, whose m_iEquippedSlot selects a slot for each equip usage.
   Resolve both primary and secondary usage; listing every inventory slot is
   not an equipped-item report. Record item.m_pOwningInventory, m_EquipUsage,
   and m_CurSocket and cross-check their meanings during equip transitions.
2. **Validate the layouts being read.** m_Slots is an array of native
   PawnInventorySlot records, while m_EquipUsageInfo is a fixed array of native
   structs. Neither is automatically a pointer array or a TArray of pointers.
   Derive element sizes, field offsets, and dimensions through validated
   metadata/native accessors. Bound slot indices and record negative/empty
   sentinel semantics. Do not use defaults such as slot zero as proof of a
   live equipped item. These fields need adding to the current report's
   reflection table; resolving m_Slots alone is insufficient.
3. **Cross-check the item and mesh in both directions.** Follow
   item.m_pPlayerMesh and also record m_pMesh; they are distinct fields
   (`DishonoredInventoryItem.uc:86-92`). The first-person component has an
   inverse link, `DishonoredItemSkeletalComponent.m_pItem` (:6). Require its
   identity to agree with the selected item when that relationship is valid.
   If forward inventory layout is still unresolved, this inverse link can
   identify attachment candidates without pretending they are proven equipped
   items. Report actual render/visibility evidence separately from pointer
   existence.
4. **Resolve the actual parent.** `SkeletalMeshComponent.AttachedToSkelComponent`
   (:122) gives a direct parent-component lead. On that parent, locate the
   attachment record whose Component equals the exact child pointer. Record
   its full BoneName and complete local translation, rotation, and scale
   (:54-60,143). Validate the array element layout before walking it. If the
   parent differs from the pawn mesh, follow that actual chain with bounds
   and cycle detection rather than rejecting it or substituting a default.
5. **Cross-check with narrowly scoped queries where useful.**
   IsComponentAttached accepts the exact component and an optional bone name
   (:251); GetSocketBoneName is name-to-name (:245). Each new descriptor needs
   its own verification, including boolean/optional parameter handling.
   FindComponentAttachedToBone (:248) returns one component and cannot prove
   a complete list if multiple attachments share a bone. AttachedComponents
   (:254) is an UnrealScript iterator, not an ordinary one-shot array-return
   function; defer that invocation protocol.
6. **Compare against socket configuration without treating it as live state.**
   m_CurSocket is an enum category: none, equipped, holstered, or give
   (`DisGlobalEnums.uc:290-297`). The corresponding authored socket-name array
   is on DisTweaks_InventoryItem (:69). The pistol's equipped default is
   LeftHandWpn (`DisTweaks_WepPistol.uc:28`). A default predicts where to look;
   the active parent record determines the relationship reported as observed.
   Keep ParentAnimComponent and ParentBoneMap (:139-140) as separate animation
   dependencies, not substitutes for attachment parenting.

For each stable state and each observed transition, publish one row with:
simulation tag, object/asset generation, equip usage, selected slot, item class
and pointer, child mesh/asset, inverse item identity, actual parent component,
bone FName/reference index, attachment local frame, and validity/evidence source.
Include an explicit detached/transitional/unknown state. Do not freeze the
report after the first weapon.

Start with sword plus empty/power hand, then pistol, crossbow, and Heart.
Observe equip, holster, and reload; include additional reload, projectile,
and muzzle-effect components encountered in the attachment records. Bounds
and identity checks apply recursively. Dump each relevant item's own socket
asset as needed. Held physics objects and corpse carrying retain their
separate consumers identified in the previous review.

**Acceptance:** the selected item, its active mesh, and its actual attachment
parent agree across independent links, or the report identifies the unresolved
link. Observe at least an equipment transition so stale ownership would fail
the check. A socket list or several correctly resolved offsets is insufficient.

**C. Step 2a: identify native pose writers and downstream consumers**

Use the now named targets and actual component identities to narrow the trace:

- Read the pawn's m_pLookAtControl_LeftHand, RightHand, and Camera
  (`DishonoredPlayerPawn.uc:307-309`). Check identity, sharing, ControlName,
  NextControl chain, strength, control spaces, other-bone reference names,
  and pre/post-physics participation. Prove which bones each control actually
  targets; its variable name alone is not that proof.
- Trace skeleton evaluation/composition, applicable control evaluation,
  attachment transform updates, the item's post-update component, and the
  render-pose copy for these particular objects. Native work may bypass
  ProcessEvent entirely. Use byte-validated, bounded native observation points
  derived from this executable; do not invent an address from a tick-group name.
- Record thread, monotonic sequence, simulation/pose generation, object identity,
  and the selected transforms at function entry/exit or the relevant consumer.
  Distinguish a getter observation from a writer/consumer observation. Identify
  local versus composed versus world frames and validate transform-buffer
  layouts independently from the reference skeleton and GPU palette.
- Determine whether an existing single-bone control can supply the desired
  pose before engine composition and attachment updates. If so, that may
  avoid manually rewriting a composed descendant subtree. If it cannot, name
  the precise alternative boundary and the recomposition/update mechanism
  needed there. Neither route is selected solely because the bone is named.

The scripts expose control sharing and reference-space choices;
[Epic's UE3 controller documentation](https://docs.unrealengine.com/udk/Three/UsingSkeletalControllers.html)
also describes ordered control chains and controls shared across bones.
This is why separate skeletal branches do not by themselves prove behavioral
isolation. It does not establish that this game's controls are actually shared.

The player's and weapon's PostUpdateWork defaults and forced attachment-update
flags are leads. The inventory post-update component uses the same group.
Measure their order within it. Inspect multiple animation/equipment states;
one idle snapshot cannot identify all competing writers. Keep the query tracer
observational: do not force animation updates, enable script-control ticking,
change reference-pose flags, or graft controls to create an observation point.

**Acceptance:** name the actual candidate insertion point, its input frame,
the last competing writer, the descendant-composition behavior, and the later
attachment and render consumers. Mark any unresolved interval explicitly.
Native tracing can proceed without a GPU bone map; correspondence is required
only for the render/geometry claims that actually use it.

**D. Step 2b follows the measured boundary**

Then implement a default-off deterministic translation and rotation of one
hand, with identity/off restoration. A placement experiment is needed to
perform 2b; what remains deferred until it passes is controller-driven placement
and a headset build for perceptual judgment.

Measure the hand, attachment joint, selected item/muzzle, fingers, camera bone,
actual camera/view, and opposite hand at the same simulation generation.
Check both eyes, equip/reload, and repeated render passes. Apply at most once
per intended simulation pose and start from the game's unmodified pose, so
stereo re-entry cannot accumulate the change.

Verify final clipped/cap vertex influence coverage as well as named-bone
motion. The cap can still depend on forearm bones; do not solve a visual seam
by moving a shared ancestor without checking all of its consumers. Only after
this proof introduce the common controller snapshot, grip calibration, and
verified component/world conversion from the existing overall plan. Firing
aim, melee caches, physics handles, and corpse state remain distinct gameplay
checks even when the visible attachment follows correctly.
