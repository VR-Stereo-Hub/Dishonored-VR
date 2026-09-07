# VR-33: corrected query contract and next implementation plan

Reviewed 2026-09-07 against `b0499908`, the supplied call-contract proposal,
the compiled source paths, and the local decompiled UnrealScript corpus.
This is a source review and implementation handoff. No game was launched,
no native call was tested during this review, and no placement code was changed.

**Decision: retain named queries as the default next step.** The proposed
contract needs corrections before implementation, but its premise that this
would introduce the mod's first outbound engine call is false. A bounded raw
skeleton investigation is an alternative when its layout has independent
evidence; the proposed three structural checks do not supply that evidence.

This document supersedes the call-contract proposal's ordering and call
signatures. The attachment, native-update, and deformation gates in
`VR-33-STEP1B-PLAN.md` still apply.

**1. Correct the factual baseline before adding infrastructure**

The existing code already dispatches engine functions through ProcessEvent:

| Evidence | What it establishes |
| --- | --- |
| `src/mod/state/51_legacy_spacebases.inc:60` | An existing four-pointer `__thiscall` function-pointer type, including the object argument. |
| `src/game/dishonored/hands/mat_hide.cpp:130-151` | `GetNumElements` and `GetMaterial` are called on components. Both read the return from the parameter frame with the separate Result argument set to NULL. |
| `src/game/dishonored/hands/mat_hide.cpp:289,317` | The mod also dispatches `ShowMaterialSection`. |
| `src/game/dishonored/console.cpp:42` | Console commands use the same outbound convention. |
| `src/mod/dishonoredvr.cpp:86,142,158` | These files participate in the normal unity build. The state filename containing `legacy` does not make this support inactive. |
| `docs/dishonored/ENGINE_NOTES.md:2762-2779` | Previous runtime verification records successful material-section changes and engine-returned element counts. This is more than an unused declaration. |

Reuse and harden this small call path. Its previous success does not validate
every new function's parameter frame, owner, or invocation phase, but it
removes the need to invent an outbound ABI from scratch.

The decompiled declarations also contradict the proposed first call:

| Function | Input | Output | Local source |
| --- | --- | --- | --- |
| `GetNumElements` | None | Integer | `Engine/MeshComponent.uc:16` |
| `MatchRefBone` | Bone name | Integer bone index | `Engine/SkeletalMeshComponent.uc:290` |
| `GetBoneName` | Integer bone index | Bone name | Same file, :293 |
| `GetParentBone` | Bone name | Parent bone name | Same file, :302 |
| `BoneIsChildOf` | Bone name, ancestor name | Boolean | Same file, :308 |

These source paths are under `tools/uscript/dishonored/`. **GetParentBone is
name-to-name, not integer-to-integer.** An integer frame would be the wrong
contract. A returned FName's global name-table index is also not a skeletal
index, so testing it against the child's skeletal index is meaningless.

Owner checking remains necessary. `FindFunctionObj` really does return the
first matching function name without an owner check (`ue3/uobject.cpp:182-205`).
However, the local corpus search found only one GetBoneName declaration;
multiple owners for that particular function are a possibility to guard
against, not an established finding.

**2. Finish the execution-lane fix and handle synthetic events**

The automatic report now runs from `PeHandler` (`ue3/process_event.cpp:105`).
The manual path still does this:

```
present: DvrPreTick -> command::poll -> PrCommand("dump") -> PrDumpSockets
```

See `present_tick.cpp:11-16`, `commands.cpp:98`, and
`hands/pose_report.cpp:227-232`. The last file still directly invokes the dump.
Its request-consumption branch already exists at :214, but the command does
not set that request. Route dump and re-resolve commands through synchronized
requests, and consume them only on the verified game/script thread. A volatile
flag alone is not a complete cross-thread publication mechanism.

Do not equate any ProcessEvent entry with a stable local-player observation
point. Record thread identity and choose a known dispatch phase after the
local pawn/component/asset are ready. Revalidate ownership, class, object
identity, and generation; defer during destruction, loading, or an unresolved
lifetime state. An entry hook runs before the intercepted engine function,
so it does not prove animation or attachments have finished updating.

The existing `g_peReentry` flag is not a general guard around PeHandler.
Several mod actions, including PrTick, run before any event-specific filtering.
Simply setting that flag in the new wrapper would still allow the query to
re-enter those actions.

Use a scoped per-thread query-depth/origin guard, established before dispatch
and restored with nesting preserved. Have the observer recognize these
mod-originated diagnostic calls before it runs mod side effects or starts
another query batch. Returning from this observer must still let the hook
stub execute the original engine function. Audit nested events from the query
too; do not silently disable ordinary game event execution. Exclude synthetic
calls from evidence about the game's natural update order.

Cache completed semantic results per pawn/component/asset generation, rather
than once for the process lifetime. Invalidate after replacement or reload.
Publish an immutable report to the present thread. Keep new calls default off
under their own diagnostic setting, independent of the read-only report.

**3. Validate a small set of function descriptors**

Resolve the exact declaring class/function and verify that the receiver is
an instance of that class or a valid derived class. The four bone queries
are declared on Engine.SkeletalMeshComponent; GetNumElements is declared on
Engine.MeshComponent. Walking validated class children/supers is acceptable.
An existing GObjects scan with exact owner identity and receiver ancestry
checks is also acceptable; a second speculative class layout is not mandatory.
Refuse unrelated owners, ambiguous matches, or incompatible signatures.

Keep the wrapper limited to these functions. Validate UFunction metadata
layout from this build before relying on its ParmsSize or property chain.
The proven UProperty offset helper does not itself establish those other
native offsets. Bound and cycle-check traversal; distinguish parameters from
locals; validate property class, array dimension, flags, offsets, sizes, and
the single expected return. Check arithmetic and complete frame bounds.

Use suitably aligned, initialized storage whose size and return offset match
the validated call contract. Compare each descriptor with the declaration
before invoking it. Fixed C++ parameter structs are acceptable only after
their layout is verified against that evidence; compiler layout alone is not
evidence. Limit the initial supported values to integers and full FNames;
add the boolean descriptor when its storage and return encoding are verified.
Do not assume Unreal booleans use a one-byte C++ bool. The repository already
has a boolean-property mask reader (`ue3/uobject.cpp:239-267`).

Preserve both FName words, including Number, through inputs, comparisons,
returns, and logs. Seed MatchRefBone from a copied, validated socket BoneName
instead of rebuilding it from its printed string. The current socket log
reads only the index (`hands/pose_report.cpp:156-157`), so retain the full value
in the new snapshot rather than treating the old text as a complete FName.

The hook's four helper arguments are **object plus three target arguments**.
They do not include the return address. The stub pushes three stack arguments
and ECX (`ue3/process_event.cpp:677-681`); the return address is read separately
at :55-60. Its `add esp,16` cleans the cdecl observer's arguments, not the
ProcessEvent target's arguments.

The existing calls support the x86 thiscall convention. Under that convention
the object is in ECX and the callee cleans stack arguments, as described by
[Microsoft](https://learn.microsoft.com/en-us/cpp/cpp/thiscall?view=msvc-170).
If documenting exact binary-level cleanup, inspect the target's actual exit
and a compiled caller. The five-byte prologue check alone does not establish
that fact, and this review has not independently disassembled the target.

Keep the existing NULL Result argument and reflected return-in-frame path
for the verified scalar/name wrappers, checking each native wrapper's result
handling if needed. Existing scalar/object returns provide direct precedent;
they are not proof for every return type. Do not try several return locations
and select whichever looks plausible.

Deferring GetBoneNames is sensible. Its allocated array would require the
correct engine allocation/destruction contract; it does not have to be freed
by the mod's CRT. Avoid this ownership problem in the first probe. Matrix and
Vector returns can likewise wait for explicitly validated struct descriptors.

An exception handler can provide diagnostics and stop subsequent probes. It
cannot promise recovery from stack corruption, partially changed engine
state, or damaged locks/heap. Do not describe a caught engine fault as a safe
successful run. Treat such a run as failed and restart from the baseline.

**4. Implement the queries in this order**

1. Validate the descriptor and run **GetNumElements** once on the selected
   live component in the simulator. It has no inputs, a fixed integer return,
   and an existing tested caller. Compare against current component/material
   evidence and the existing census; the historical value of one is not a
   universal requirement. Confirm the return was written, guard depth restored,
   and no recursive query or unintended mod action occurred.
2. Use **MatchRefBone** on a full BoneName from the socket snapshot. Accept
   only a valid nonnegative result within independently justified bounds.
   Then call **GetBoneName** on that returned index and require an exact full
   FName round trip. Do not start an index enumeration using 48 as the count.
3. Use **GetParentBone** on that validated name. For every nonterminal parent,
   resolve and round-trip it through the same functions. Walk with a visited
   set and a strict length bound; report missing names, cycles, and uncertain
   termination distinctly. Validate the root/None convention rather than
   confusing lookup failure with a root. If testing parent-index ordering,
   first obtain real skeletal indices and establish that ordering for this
   build.
4. Repeat for both hand-attachment targets, the measured left-hand candidate,
   the camera target, and a right-hand candidate if lookup verifies it. Print
   the actual chains and the two attachment-to-hand ancestry verdicts. Add
   **BoneIsChildOf** as a semantic cross-check after validating boolean return
   handling; the parent chains can deliver the first useful result without it.
5. Expand to a complete named table only if needed and the actual reference
   count is independently validated. A readable RefSkeleton array header may
   supply a count without requiring native element decoding. Report that
   count separately from render-palette capacity and CPU-buffer indexing.

Stop the dependent sequence on a failed descriptor, invalid receiver, or
inconsistent result. Report the exact failure instead of promoting a plausible
answer. Run this diagnostic in the simulator before another headset request.

**5. Ruling on raw RefSkeleton first**

The proposed three checks support a *candidate interpretation*. They do not
prove the stride or make the route risk-free:

- For nonroot elements, requiring `0 <= parent[i] < i`, with index zero the
  root, already forces all chains to terminate at zero. Single-root reachability
  adds little independent evidence to that rule.
- Real engine memory is structured. A mistaken field containing zero can
  produce a plausible star hierarchy, and nearby FName-bearing records can
  provide valid-looking names. Random bytes are the wrong comparison model.
- Trying many strides/offsets makes accidental matches more likely. Even a
  coherent table could be the wrong table. Root sentinel and ordering are
  themselves build-specific assumptions to verify.
- RangeReadable checks memory mappings, not object lifetime or a coherent
  snapshot (`src/core/util/mem.cpp:6-23`). Memory can change after the check.
  Microsoft documents this general limitation for pointer-readability checks
  [here](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-isbadreadptr).
  The mod uses VirtualQuery, not that obsolete API; the lifetime distinction
  still applies.

A bounded copy taken at a verified engine execution point can be decoded
offline into candidate layouts. Promote one only with independent evidence:
native accessor addressing that reveals stride and fields, a validated native
asset layout, or agreement with correctly called name/parent queries. Exact
socket FNames and valid transforms strengthen cross-checks; neither a small
sample nor the number 48 establishes complete palette correspondence.

**Raw first is acceptable if that independent derivation is already available.**
It is not the recommended default on the evidence supplied here. The mod
already has outbound call precedent, and named queries answer the immediate
ancestry question without decoding a native element structure. If wrapper
metadata cannot be established, an independently derived read-only layout is
a reasonable fallback, with its remaining uncertainties explicitly reported.

**6. Keep the architecture decision tied to actual consumers**

Sibling hand and attachment bones would rule out automatic inheritance from
a wrist-only edit. They would not rule out the native approach: separate
targets could receive a coordinated transform, or a verified side-specific
ancestor might be suitable. Exclude the camera and opposite-hand branches.
Conversely, a descendant relationship is necessary evidence for inheritance,
not proof that a later animation writer or attachment update honors the change.

The decompiled scripts supply these concrete follow-on checks:

| Lead | Consequence for the next probe |
| --- | --- |
| `DishonoredGame/DisTweaks_WepPistol.uc:25-29` defaults the pistol to secondary equip usage and LeftHandWpn. | Do not equate weapons with the right hand. Resolve the live item and physical attachment per equipment state. |
| `DishonoredGame/DishonoredInventoryItem.uc:85-110` distinguishes inventory, equip usage, socket category, player/world meshes, melee caches, and a post-update component. | Follow the active item to its actual player mesh. m_CurSocket is an enum, not an FName. Offset discovery alone is not an attachment report. |
| `Engine/SkeletalMeshComponent.uc:54-60,139-145` distinguishes attachment records from parent animation and pose maps. | Measure actual parent component, bone, and local frame; shared animation is not the same as attachment. Validate native layouts for any raw array walk. |
| Player/weapon defaults request attachment updates and PostUpdateWork; the inventory post-update component also uses that tick group. | Trace ordering within the group. A getter called from ProcessEvent does not identify the final skeleton writer or the correct placement boundary. |
| `DishonoredGame/DisItemContext_ProjectileAttack.uc:6-28` carries cached aim position/direction and a camera-update firing marker; `DisTweaks_FirePistol.uc:29` names the muzzle socket. | Following the visual weapon does not establish controller-directed firing. Check the actual muzzle and final projectile/trace origin and direction separately. |
| `DishonoredGame/DishonoredPawn.uc:250` exposes a movable-object RB_Handle; `Engine/RB_Handle.uc` has grab, location, orientation, and release operations. | Trace the live physics-handle target for held movable objects. A wrist-bone change cannot be assumed to drive that separate system. These setters are later behavioral work, not part of the query probe. |
| `DishonoredGame/StatePlayerCarryCorpseIdle.uc:27-36` has a separate corpse-carry state, carried pawn, and camera-offset participation flag. | Do not assume every holdable follows either the equipped-weapon path or the movable-object handle path. Inspect that state when adding corpse carrying. |

These are declarations and useful native-code leads, not recovered
implementations. They do not reveal the final native update order.

After the semantic report, trace the verified hand controls, composition,
attachment consumers, and render-pose copy. Then prove one small deterministic
hand translation and rotation before adding controller tracking. Preserve the
existing split/caps, finger animation, and camera behavior. In particular,
changing a composed wrist transform alone does not recompose its descendants;
forearm weights at the clipped boundary can still deform the cap.

**Answers to the four review questions:** the contract is not sufficient as
written; raw-first is conditional rather than justified by zero risk; the
three structural constraints are insufficient to prove the layout; and
GetNumElements is the appropriate existing no-input smoke call, followed by
MatchRefBone and a GetBoneName round trip.

**Next deliverable:** a focused implementation and simulator report containing
the corrected command lane, scoped synthetic-call handling, validated function
descriptors, full-FName hand/attachment parent chains, and explicit per-stage
success or refusal. Then extend the same report to the live item relationships.
No additional architecture decision should depend on a guessed stride, a
misdeclared function, or an unobserved attachment relationship.
