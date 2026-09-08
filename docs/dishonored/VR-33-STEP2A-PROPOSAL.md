# VR-33: cap fix confirmed, my diagnosis withdrawn, and the step 2a proposal

For review before implementation. Section 1 is log output plus a tester
observation from a run on 2026-09-07. Sections 2-4 are proposals. No placement
code exists and nothing writes to the skeleton.

---

## 1. The cap fix is confirmed, and the mechanism was not what I said

**Tester: caps present, arms still hidden.** Both rebuilds in the run produced
78 clipped triangles and their caps.

**The cause was a stale stream binding, not a second pass.** The declaration
references **stream mask 0x3** - streams 0 and 1. It never references stream 2.
The stream-2 binding that vetoed the clip was leftover state from an earlier
draw, so that pass was clippable the whole time.

My previous document asserted that "the same mesh is drawn by more than one
pass and only some of those passes bind stream 0 alone", from two log excerpts
that showed the same buffer pair giving different answers. That was an
inference presented as a finding. The review's objection - a bound stream is
not necessarily a used stream, and two excerpts do not identify separate passes
- was correct on both counts. Withdrawn in the plan doc and in ENGINE_NOTES.

What survives:

* **`bound != used`** is the fix, and it is now in ENGINE_NOTES as an engine
  fact: the declaration decides what a draw reads, and only a stream it names
  may veto owning stream 0.
* **The draw-contract check matters regardless.** `MsDraw` was binding
  re-based indices to any draw with a matching primitive type and count,
  without checking vertex window, base vertex, stream-0 offset or declaration.
  It logged **zero** mismatches in the confirming run, so it is protection
  against a real hazard rather than a cure for an active fault - stated that
  way rather than claimed as the fix.
* **Declining used to suppress the mesh.** Once a split is ready the caller's
  auto-arm fail-soft no longer applies, so a refused draw fell through to
  suppression instead of being drawn. That was a genuine defect and is fixed.
* Decline-and-wait remains reasonable candidate selection even though it was
  not what fixed this.

**Recorded as a live risk:** the declaration names **stream 1**, and no observed
draw binds it. If one ever does, the veto fires legitimately, the clip is
genuinely unavailable on that pass, and the caps go with it. That is the
multi-pass situation I wrongly claimed to have already found. It has not been
observed.

Also fixed in the same change, both mine: a function-local `static` in the item
scan's class list persisted across calls, so a second run found no new classes
and printed that the inventory was genuinely empty - a false negative wearing
the clothes of a measurement, in the very code added to prevent that. And the
attachment scan's comment claimed to derive a stride when it only locates a
pointer and reads a name four bytes past it; every such result is now labelled
`UNVALIDATED LAYOUT`, and the closing line no longer claims the socket defaults
were contradicted, because nothing live has agreed or disagreed with them.

## 2. Where the ticket actually stands

| Question | State |
|---|---|
| Bone chains for both hands, attachments, camera | **measured**, every link round-tripped, all terminating at the root sentinel |
| `handAttachment_*_jnt` are children of `hand_*_jnt` | **confirmed** from validated edges on completed walks |
| Camera is on the spine/head branch, not either arm | **confirmed**; nearest common ancestor with either hand is `Root_jnt` |
| Skeleton indices vs palette slots | **proven different** - `hand_L_jnt` is 54, `handAttachment_L_jnt` 56, against a 48-slot palette |
| Wrist caps | **fixed and confirmed** |
| Live equipped item, its mesh, its actual attachment | **UNKNOWN** - the probe found no item component and a null inventory owner |
| Native update ordering | **not started** |
| Placement of anything | **not started** |

The item graph is the blocker for the weapon half of VR-33: the
follow-for-free property depends on ancestry, which is measured, but the gate
cannot pass without identifying a live item and its real attachment.

## 3. Proposed 2a-part-1: the three-control identity report

Independent of the item graph, so it can proceed while that is repaired.
Read-only; nothing is enabled, grafted or re-flagged.

For `m_pLookAtControl_LeftHand`, `_RightHand` and `_Camera` (+0x8A8/8AC/8B0,
already resolved), report per control:

* the object pointer, its class through the validated `Super` chain, and
  **whether any two of the three are the same object**;
* `ControlName`, `ControlStrength`, and the `NextControl` chain walked with a
  visited set and a bound, each link identified;
* the control space fields and any other-bone reference name, as raw values
  with their meaning explicitly unresolved - an integer `Space` is not
  automatically the enum a variable name suggests;
* pre/post-physics participation flags as observed.

**What it will not claim.** Which bone a control targets is *not* established
by its variable name. ENGINE_NOTES already records that the camera control aims
the arms at the view and was untouched for a dozen builds, so the naming is
demonstrably not a reliable guide here. The report states the target as
UNKNOWN unless the control's own fields name it.

**Gate:** three identities, sharing answered yes or no, chains enumerated, and
every value either interpreted with stated evidence or printed raw and marked
unresolved.

## 4. Proposed 2a-part-2: bounded equipment sampling

The probe currently fires once, early, on the first live pawn - which is very
likely before the inventory is populated, and a single early sample cannot
distinguish that from a wrong class name.

Proposed change to **when** it samples, not to what it reads:

1. Observe the equip entry points as *triggers only* -
   `Dis_NextEquippedItem`, `Dis_PrevEquippedItem`, `Dis_EquipItemByType` on the
   controller - to arm a deferred sampling window. Entry does not prove an
   equip completed, and native callers may bypass ProcessEvent entirely, so
   this is a hint about when to look and never a completion signal.
2. Sample on a short bounded window after such a trigger, on readiness changes,
   and on a queued manual request. Retry with backoff while incomplete, and
   keep reporting incomplete rather than settling on a negative.
3. Match item components through the **validated class hierarchy** rather than
   exact class-name equality, so a derived component type is not missed.
   Count exact matches, derived matches, null back-pointers and truncation
   separately.
4. Check inventory readiness properly: class and lifetime of the inventory
   object, `m_bInitialized`, and `m_pOwner` together, rather than treating a
   null owner as evidence of anything.
5. Keep every attachment-record read labelled UNVALIDATED until the element
   layout is established from reflection or a native accessor. A candidate is
   reported as a candidate.

**Gate:** for at least one item, a demonstrated path from the local inventory
to the active mesh and its actual parent and bone, holding across an observed
equip transition - or a report that names precisely which link is unresolved.
A socket default is a prediction, never the observation.

## 5. Questions

1. Is the three-control report worth doing before the item graph is repaired,
   or does its value depend on knowing which item is equipped?
2. For the equipment window: is arming on an observed command entry acceptable
   given it cannot prove completion, or is there a readiness field worth
   polling instead - `m_nPendingEquip` and the equip-change state were both
   suggested previously?
3. The `stream 1` risk in section 1: worth pre-emptively handling now, or left
   as a recorded hazard until a draw actually binds it?
4. Anything in section 1 or 2 that the evidence still does not support.

## 6. State

Branch `claude/vr-33-hands-and-weapons-at-the-controllers`, installed build
carries the stream fix, the draw-contract check, the pass-through fallback and
the corrected probes. `[Hands] BoneQuery=1` and `PoseReport=1` on the dev rig,
both read-only apart from the bone queries' ProcessEvent calls.
