# VR-33: review of the cap fix, equipment probe, and step 2

Reviewed 2026-09-07 against `f1b10ad8`, the supplied results, the split/census
and query implementations, and the local decompiled scripts. This review
did not run the game or independently confirm the installed fix. Only this
handoff document was added.

**Decision:** retain the corrected bone-parent results and continue the native
investigation. Fix the split's draw-time compatibility and the equipment
probe's interpretation before treating either as a proven baseline. The
three-control identity report can proceed alongside those repairs. Completing
the native attachment-order gate still requires identifying an actual live item.

**1. What the cap logs establish, and what they leave open**

The logs and source agree on the immediate mechanism: when MsRead sets
g_msOwnVb false, the requested plane clipping is disabled and no cap is built
(`mesh_split.cpp:283-307,1293-1344`). That explains the zero CUT/CAP counts
in the reported build. Declining an unsuitable build candidate is a useful
change, and `draw_census.cpp:252-256` does honor the new retry flag.

The stronger claims need qualification:

- A bound stream is not necessarily used by the current vertex declaration.
  MsRead checks whether streams 1 through 7 have buffers/strides, but does not
  check whether the declaration references them. Unused retained stream state
  could therefore veto a valid stream-0-only draw. The reported stream-2 binding
  does not by itself prove a distinct rendering pass or an unavoidable clipping
  limitation. Read the full declaration and stream frequencies.
- Matching VB/IB pointers are useful within a validated resource lifetime;
  they do not establish identical draw ranges, contents, declarations, or
  resource identity across recreation/process restarts. Record process/device
  generation and draw inputs. Earlier census observations support multiple
  passes, but these two log excerpts do not identify their roles or establish
  that a compatible pass is available in every later state.
- Producing cap triangles is not proof that they reached the visible output.
  Capture and attribute the subsequent draws too. Keep creation, submission,
  and visible-result checks separate.

Microsoft documents that the declaration maps stream elements to shader inputs
and the shader interprets those inputs in its
[Direct3D 9 stream documentation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/programming-one-or-more-streams).
This supports checking declared inputs rather than treating every binding as
consumed data. It does not establish which inputs this game's stream 2 uses.

**2. The build-time fix does not make subsequent draws compatible**

The most important remaining render issue is downstream of MsBuild:

- DcIsLocked compares VB/IB pointer identity only (:177-193).
- MsDraw checks readiness, primitive type, and primitive count (:1563-1565).
- It then installs the generated stream-0 buffer and rebased indices
  (:1588-1601), without checking the current declaration, other streams,
  original start index, base vertex, vertex window, or stream offset.

Consequently, a compatible draw can build the split, then an incompatible
draw of the same buffer pair can consume it. If that later draw uses another
per-vertex stream, the rebased indices and generated vertices do not have
corresponding data in that stream. Choosing a better first draw cannot repair
this reuse path.

**Use a geometry identity plus a per-draw compatibility contract.** This is
more precise than permanently binding the split to whichever pass appeared
first:

1. Identify the copied geometry with device/resource generation, buffer
   identity, primitive type, start index/count, base vertex, min index/vertex
   count, stream-0 offset/stride, and declaration layout. Account for content
   updates where buffers are mutable. Equal triangle counts are insufficient.
2. Record build provenance: declaration content/hash, vertex/pixel shader
   identity or bytecode hash, declared streams with offsets/strides/frequencies,
   and relevant render-target/depth/blend state. Keep raw pointer identities
   scoped to their resource generation. A diagnostic pass signature need not
   be a guessed engine pass number or a permanent shader-address whitelist.
3. Before EVERY replacement draw, verify its geometry and supported input
   layout. For the first implementation, support declarations whose required
   per-vertex data is wholly supplied by the generated buffer and whose stream
   frequency semantics are understood. An unused extra binding need not veto
   it; an actively referenced unsupported stream must. Inspect declared stream
   indices rather than assuming the range 1 through 7 covers every case.
4. Observe other passes of the selected geometry outside the palette reuse
   gate. The code already admits that gate misses draws. Capturing a matching
   buffer is not evidence that the latest c6 upload belongs to this shader.
   Keep any palette interpretation tied to its actual draw and register use.
5. Classify each required pass as compatible, unsupported, or deliberately
   suppressed with measured justification. Rendering the original geometry
   on an unsupported pass may reintroduce arms; silently dropping it may lose
   lighting/depth/other contributions. Neither is a completed visual solution.
   Capture both eyes to determine which passes need equivalent split support.

Do not implement this as only another `return false` in MsDraw. The caller
can still fall through to mesh suppression when g_msReady is true
(`draw_census.cpp:271-284`). Give the replacement path explicit outcomes so
an unsupported draw reaches its intended fallback rather than an incidental
drop. Include the non-indexed path (:200-214) in pass coverage.

The existing draw-index contract includes base-relative index ranges, documented
by Microsoft for
[DrawIndexedPrimitive](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitive).
Those inputs must remain consistent with every stream actually consumed.

**The 120-skip policy also needs revision.** It counts eligible build attempts,
not frames or elapsed time, and each attempt currently performs buffer locks
and copies before declining. The counter is reset only by a successful owned
buffer build; the stale-lock release does not reset it. After degradation,
g_msReady prevents a later compatible pass from upgrading the split.

Move the cheap declaration/stream eligibility check before readback. Scope
retry state to the geometry generation, bound work per frame/window, and give
degraded mode a controlled upgrade path. Exhausting a retry budget is not
evidence that a pass is absent. The existing automatic pending path draws the
original mesh, so the justification that the alternative is inevitably no
hands is inaccurate. Preserve a valid existing split only while its geometry
and draw contract remain valid; otherwise use an explicit fallback. A rough
uncapped edge remains a degraded result, not a fixed-cap acceptance.

Treat resource/binding failure as part of this same contract. MsUpload can
disable g_msOwnVb after clipping has already generated new indices
(:1404-1409), and MsDraw can use its index buffer with the game's vertices
when replacement binding fails (:1593-1601). Never submit generated-vertex
indices without the matching vertex data. Abort replacement or rebuild a
genuinely compatible whole-triangle result; merely switching the buffer flag
does not undo clipping. Restore saved state and propagate draw/bind failures.

**Render acceptance:** exercise compatible A, unsupported B, then A again;
an unused stream-2 binding; a declared extra stream; the same buffer pair with
a different draw range; recreation/re-arm; and temporary creation/binding
failure using an appropriate harness. Verify bounded retries, no incompatible
index/stream combination, explicit fallback, and successful later upgrade.
Then capture the caps in both eyes across equip/reload and re-arm. No skeleton
placement should be involved in this regression check.

**3. The two equipment negatives remain unresolved**

Null inventory ownership does not establish an empty inventory. It can also
indicate an uninitialized/template/stale/wrongly resolved object. BqItems only
checks the pointer's alignment/readability before applying m_pOwner's offset
(:399-406); it does not establish the inventory's runtime class or initialized
state. PeLatch selects by event-stream class name, not a complete current
local-controller possession proof. Verify pawn/controller identity and object
lifetime, inventory class/full object path, reflected field owner/offset,
m_bInitialized, and m_pOwner together.

The new inverse scan also has concrete limitations:

- Exact class-name equality (:424) excludes derived item-component classes.
  Use the validated class hierarchy. Count exact and derived matches, null
  back-pointers, invalid pointers, foreign owners, and truncation separately.
- It stops after 16 global candidates, which can omit the player's item.
  Prioritize a verified local ownership/parent relationship. A child attached
  through another component must not automatically become someone else's item.
- The fallback searches every object class containing Item, not necessarily
  component instances. Its static seen list survives report invocations
  (:502), so a repeat can find zero NEW classes and falsely declare the
  inventory empty (:512-513). Use per-snapshot deduplication, actual component
  type checks, and explicit scan completeness. No substring search proves
  inventory emptiness or separates timing from layout/class errors by itself.
- The code's closing statement that a pistol socket default would name the
  wrong hand (:515-518) is unsupported. The default predicts LeftHandWpn;
  no live result has contradicted it. Report agreement, disagreement, or unknown.

Keep the currently negative result as UNKNOWN. Repeated snapshots are needed,
but re-sampling alone does not repair these interpretation errors.

**4. Remove the guessed attachment scan before it produces a positive**

BqItems scans `an * 64` bytes at four-byte intervals, then assumes a child
pointer is followed by an FName at +4 and location at +12 (:445-459). It calls
this deriving a stride, but no stride is derived or checked. If the real record
size differs, the scan can include adjacent allocation data or omit records.
Readable memory outside the logical array is not attachment membership, and
a nearby valid bone name does not prove the assumed location layout.

Establish the array element type, size, field offsets, and full record extent
from validated reflection/native accessors. Iterate exactly Count elements;
check header bounds and arithmetic; match the exact Component field. Before
calling MatchRefBone on a candidate parent, validate that receiver's class and
lifetime too. Record complete relative translation, rotation, and scale, with
UNKNOWN for failed reads instead of initialized zeros.

The local `Engine/SkeletalMeshComponent.uc` declares the Attachment fields
(:54-60), Attachments (:143), and targeted queries IsComponentAttached (:251)
and FindComponentAttachedToBone (:248). These offer alternative cross-checks
once each call descriptor is verified. The latter returns one component, not
an exhaustive list. Neither a query nor a struct read should be widened into
a guessed generic invocation/layout framework.

This is a current blocking defect for trusting a future positive attachment
record. It need not block collecting validated item/parent pointer identities.

**5. Sample after actual equipment change without inventing an event contract**

Separate skeleton discovery from an equipment snapshot. Re-running the entire
bone discovery/GObjects walk on every tick is unnecessary. Cache validated
relationships with generation checks and take bounded equipment snapshots
on readiness changes, a manual queued request, and during a short observation
window around equipment changes. Missing/partial results should retry with
backoff and remain visibly incomplete.

The decompiled scripts provide useful trigger candidates, not a universal
equip-completed callback:

| Local script | What it offers and its limit |
| --- | --- |
| `DishonoredPlayerController.uc:329-336` | Dis_NextEquippedItem, Dis_PrevEquippedItem, and Dis_EquipItemByType are input/command entry points. Observing them can arm a later sampling window, but entry does not prove an equip completed. Native callers may bypass ProcessEvent. |
| `DishonoredPawn.uc:425-426` | OnEquipItemType is a sequence-action handler, not evidence of notification for every equip route. |
| `DishonoredPawn.uc:251-252` | Desired equip state and m_nPendingEquip are readiness leads without decoding the native slot elements. Establish their live semantics instead of assuming a particular value guarantees completion. |
| `StatePlayerEquipChange.uc:5-12` | Pending item class, exit request, and m_bDidEquipChange are state-observation leads. Resolve the active state instance and usage before reading them. |
| `DishonoredPlayerPawn.uc:871,926` | The upper and left-arm state machines are bound to primary and secondary usage respectively. Observe both equipment paths; do not assume one callback covers both. |
| `DishonoredInventory.uc:56-57` | Owner and initialization fields help distinguish readiness from an ownership mismatch. |

All paths in this table are under `tools/uscript/dishonored/DishonoredGame/`.
Use an observed command merely to request deferred sampling; do not query the
result immediately in the pre-call hook or trigger an equip from the probe.
First obtain stable snapshots before and after a visible controlled equip,
checking item.m_pPlayerMesh, component.m_pItem, item.m_pOwningInventory,
actual attachment parent, and equip/socket category together. Observe a reload
or holster transition too. If the graph never becomes valid, investigate
identity/field resolution rather than declaring the inventory empty.

**6. Step 2a: begin with identities, then prove the native sequence**

The proposed control report is appropriate now: the two hand controls and the
camera control, their actual classes/identities, sharing, control names,
NextControl chains, strengths, spaces, and referenced bones. Resolve the
controlled-bone mapping through the verified component/control tables and
native consumers; names and skeleton ancestry do not establish control targets.
Report flags and values as observation, without changing control activity.

There is no demonstrated script-only signal in these files that orders all
native composition, attachment, item post-update, and render-copy operations.
Existing script callbacks, TickTag/control tags, and before/after snapshots can
locate an interval or expose a stale pose. They do not identify the last writer
inside that interval. Enabling TickSkelControl to create a callback changes the
system being measured and still does not prove the full ordering.

Use the least intrusive verified observation available for each missing
relationship. Derive a small set of native sites from this executable and the
identified objects; instrument only those with validated call/return contracts
and preserved register/state behavior. A byte check is an identity guard,
not proof that a hook's calling convention or function boundary is correct.
Record thread-local sequence, nesting, object/generation, simulation/pose tag,
and relevant transforms at writer/consumer boundaries. Do not infer cross-thread
causality from timestamp adjacency; identify the actual handoff/generation.

Account for all mod-generated query/console/material calls in the trace.
Record entry and exit where needed: the existing PeHandler observes entry,
so merely seeing an event does not prove its body has updated anything yet.
Run distinct animation/equip states so a late writer absent during idle can
appear. Keep output bounded and report dropped trace samples.

The native-boundary gate remains: identify the selected input frame, last
competing writer, descendant recomposition behavior, actual equipped-item
attachment update, and render-pose consumer. Control discovery and skeleton
tracing can advance while equipment identity is repaired, but the complete
weapon-following gate cannot pass on an unknown item graph.

**Answers to the four questions**

1. Decline-and-wait helps candidate selection, but must be paired with
   draw-time compatibility and explicit fallback. Record pass provenance;
   restrict reuse by verified compatibility, not merely the first pass's ID.
2. Re-sample after equipment change, using verified readiness and bounded
   deferred windows. The scripts offer command/state leads, not a proven
   universal equip-completion event that avoids all native work.
3. Script observations can narrow the interval. They cannot establish the
   complete native ordering requested here without evidence at its actual
   writers/consumers. Use targeted native observation only for the missing links.
4. The corrected parent results support hand ancestry. The cap logs support
   the clipping-veto mechanism, not guaranteed pass availability or visible
   recovery. Bound streams are not automatically consumed streams. Empty
   inventory, derived attachment stride, and an incorrect pistol default
   remain unsupported interpretations in the current code.

**Next deliverables:** a split compatibility fix with pass-sequence regression
evidence; a corrected equipment snapshot with explicit readiness/ownership
outcomes and validated record layout; and the three-control identity report.
Then finish the native ordering trace before deterministic one-hand movement.
Controller tracking follows that movement proof, including caps, fingers,
actual item/muzzle, camera, opposite hand, and both eyes.
