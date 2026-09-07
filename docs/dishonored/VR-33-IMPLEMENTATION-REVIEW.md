# VR-33: proceed with the movement experiment, correct its contract

Reviewed 2026-09-07 against `6b040eef`, the supplied implementation plan,
the control-report code, existing SkelControl writers, and local decompiled
scripts. This is a revised build handoff. No game was run or implementation
changed during this review.

**Decision: proceed with Phase 1 after the small changes below.** A complete
item graph, full native ordering trace, and skeleton-to-palette map are not
prerequisites for a reversible constant-input experiment. The earlier demand
to finish the whole ordering trace before any movement was too restrictive
for this bounded test. Put the needed observations into the movement build.

The build should prove that a specific input affects the intended output.
Do not make its acceptance depend on an unverified interpretation of a tick
tag, or make a failed experiment a verdict on every native approach.

**Corrections that affect implementation**

1. Distinct control pointers prove distinct objects, not independent effects.
   They can participate in shared downstream chains or control more than the
   assumed target. BqControls compares the three starting pointers, but its
   chain log prints class names rather than each link's identity
   (`hands/bone_query.cpp:581-637`). Measure camera and opposite-hand behavior
   in the experiment. This uncertainty is a reason for the small test, not
   a requirement for a new exhaustive control-map probe.
2. A single snapshot at strength 1 and tag 10 does not prove ongoing evaluation
   or final influence. The local `Engine/SkelControlBase.uc:38` declares a
   transient integer; it does not reveal where native code stamps it.
   Observing a change only brackets a tag update between observations. It
   does not locate field consumption, prove the input survived overwrites,
   or prove a later control did not replace the result.
3. The old high write count proves neither that cadence caused the failure
   nor that rotation is impossible. Wrong spaces/modes, another writer,
   chain order, or a different affected target remain possible. Rotation
   flags were already handled by SkcOffsetAudit and SkcRotApply
   (`hands/skelcontrol.cpp:22-36,1180-1252`); reuse and verify that machinery.
4. The control report repeats an obsolete interpretation of the camera
   control. `ENGINE_NOTES.md:840-865` explicitly withdraws the claim that it
   aims the arms and records a camera regression when disabled. Preserve its
   behavior. Distinct pointers do not overturn that runtime finding.

Epic documents ordered and shared control chains in its
[UE3 skeletal-controller guide](https://docs.unrealengine.com/udk/Three/UsingSkeletalControllers.html).
That supports the isolation checks above; it does not establish this game's
exact chain sharing or update order.

**Phase 1: one controlled translation experiment**

Build one default-off lever scoped to the current local pawn's left-hand
control. Resolve it through the live pawn field, validate its class and
component ownership, and reuse the established field-offset/boolean-mask
audit. `Engine/AnimObject.uc` also exposes SkelComponent as an ownership
cross-check. Record the original translation, space, and relevant flag bits.
Record the full control identity and immediate NextControl identity when
arming; addresses in the supplied log are not constants to reuse.

Give the experiment a single owner for its translation fields. Keep competing
legacy hand writers from modifying those fields during the test, with an
explicit effective-mode log. Do not enable unrelated legacy hands/camera
features merely to reach this writer. Keep the experiment's enabled state
independent of the earlier material/probe toggles and persistent mode-veto bug.

Use a deliberately specified payload:

```
bApplyTranslation   = true
bAddTranslation     = true
BoneTranslationSpace = BCS_ComponentSpace
BoneTranslation    = (testOffset, 0, 0)
```

Use the verified enum/property representation for this build. Component space
makes the test axis explicit; save and restore the game's original space.
Keep all rotation settings, strength, scale, and chain links under their
existing owners. Modify only the specific boolean bits, not the whole packed
word containing unrelated control flags.

Run `off -> additive zero -> +10 uu -> -10 uu -> off` in the same idle state,
then with a normal animation. This is one translation lever in one build.
The zero state matters: replacing a live control's authored translation with
a constant is not the same as adding 10 to its original input. bAddTranslation
controls how the input affects the bone; it does not preserve the previous
contents of BoneTranslation. Compare +10 and -10 against the additive-zero
state, and compare restored behavior against off. If the zero state already
disturbs camera/opposite-hand behavior, stop this experiment.

Apply the absolute test payload, not `field += 10`, on a verified game/script
execution path. Define the cadence in terms of a validated game/simulation
tick where available. A Present count is not automatically an animation tick,
and ProcessEvent dispatch count is not a frame count. If the available entry
point runs repeatedly, use an explicit application token and log its source;
never accumulate the offset. No precise native evaluation boundary is required
to try this idempotent write.

Include bounded counters for requests, eligible executions, writes, failures,
and object invalidations. Record input before/after the store and sparse tag
changes as diagnostics. A successful store or advancing tag is not the output
acceptance. Do not flood a per-frame log.

Revalidate pawn/control/component identity before stores. Stop on load or
AnimTree/component replacement and reacquire explicitly. On disable, release
field ownership and restore owned settings on the same still-valid instance;
do not write a saved snapshot into a replacement object or overwrite unrelated
bits updated by the game. A saved animated vector can be stale, so ensure the
native owner resumes fresh inputs. Acceptance is restored normal animated
behavior, not equality with an old pose from several seconds earlier.

**Acceptance:** repeatable opposite hand displacements for opposite payloads,
no drift, and normal behavior after off. Check caps/fingers, actual view, other
hand, and both eyes. If the weapon is visible, include it in the capture now.
Use the simulator before headset judgment, with no automatic return to tracking.

Visible hand movement proves this control affects the rendered hand. It does
not identify the exact controlled bone: moving a forearm ancestor can move
the hand too, and the arms are currently hidden. If exact localization is
needed for the next implementation, include existing named-bone measurements
or temporary joint markers in this same test. Do not require a standalone
anatomy investigation just to establish visible influence.

**Failure means:** the attempted payload at this execution path did not produce
the required result. First distinguish no eligible write, overwritten input,
and an unexpected/no output. Stop the lever on unintended motion. Do not label
all translation from this control impossible or automatically commit VR-33
to the GPU fallback.

**Phase 2: rotation, with a concrete callback alternative**

After translation works, add a small signed one-axis rotation test, using
zero/positive/negative/off states. Specify bApplyRotation, bAddRotation,
BoneRotationSpace, and the treatment of bRemoveMeshRotation explicitly.
Log degrees alongside the engine Rotator integers; do not put degrees directly
into the integer fields. Restore the owned rotation settings when disabled.

Start at the working Phase-1 execution point. Accept visible, repeatable
orientation changes that survive animation and restore cleanly. Confirm the
rotation pivot and the camera/opposite-hand behavior. Success establishes a
working implementation path; it does not retrospectively prove why the old
attempt failed.

**Remove the planned tag-change-only build.** Seeing a tag change does not mean
the observer runs immediately after evaluation, and writing then does not make
the value the last input before the next evaluation. That build would introduce
another timing hypothesis without supplying the claimed guarantee.

If the ordinary path fails, a default-off `bShouldTickInScript` experiment
with TickSkelControl is acceptable as an implementation mechanism. The local
declaration is `TickSkelControl(float DeltaTime, SkeletalMeshComponent SkelComp)`
(`Engine/SkelControlBase.uc:50`); preserve that two-parameter contract.

Filter the actual engine-originated event by exact UFunction/owner, receiver
equal to the selected control, and validated SkelComp equal to the selected
mesh. Execute the bounded writer there while allowing the original event and
engine function to continue. Save/restore the callback-enable bit and preserve
the existing query recursion/origin handling. Do not call TickSkelControl
manually and count that as evidence the engine scheduled it.

Enabling the flag does not itself prove that the cached control tick path
will issue the callback, or that this is the final effective write. Count
matching callbacks and judge their resulting motion. No callback means this
callback mechanism was not reached; a callback with no motion means the
mechanism did not yet produce the desired pose. Those are different findings.

If the callback route fails, choose between a narrowly targeted native
consumer/last-writer check and a visual fallback experiment. That is a practical
choice about further effort. It is not proof that this control or UE3 cannot
drive rotation. Do not spend three builds on unverified timing labels.

**Phase 3: use the control's actual transform semantics**

The rigid-delta formula is correct when the backend applies a common left-side
delta to transforms, in a consistently defined frame:

```
D = A_target * inverse(A_source)
```

It is not automatically the value to put into SkelControl fields. Build a
small adapter whose contract follows the modes established in phases 1-2:

- For a validated absolute/replacement control, supply the target position
  and orientation in its selected control space. Do not supply a delta as
  though it were an absolute target.
- For an additive mode, derive the required translation/rotation from the
  verified composition order and frame. Do not assume its separate additive
  translation and Rotator operations implement the whole rigid matrix D.
- If the implementation needs A_source, read the unmodified animated source
  for that same pose. Sampling the already overridden result can feed the
  previous correction back into the next computation. If an absolute adapter
  avoids needing that source, do not add an unnecessary inverse anyway.

Use a single immutable XR snapshot with pose validity, timestamp/generation,
head/grip/aim poses, and a consistent conversion into the selected game frame.
Pair it to the simulation update and use it consistently across the eyes.
Define tracking-loss/recenter behavior and avoid double head/body transforms.

Calibrate a full grip-to-hand transform. The old 12.26 uu ring-to-centroid
distance may seed a tuning slider, but it is not an anatomical wrist location
or a complete grip frame. Do not restore the discarded centroid heuristic as
a measured native pivot. Define the direction of the grip offset so the target
frame and the rotation center follow from the same transform.

Test visible weapon following directly. The full item graph need not block
that experiment. Use known equipped items and include reload/equip/holster
transitions. A successful result validates those tested cases; a mismatch
triggers focused attachment/ordering work for the failing item. This can begin
in Phase 1 without building a new inventory report first.

Gameplay aim, muzzle/trace origins, melee caches, physics handles, and corpse
carrying can remain deferred for the visual milestone. They remain separate
requirements for the corresponding motion-control functionality. Record the
scope achieved rather than claiming those consumers follow automatically.

**Fallback and completion**

The common-delta palette fallback is a valid hands-only experiment if it uses
the original palette for each draw, a verified matrix space/packing, every
contributing matrix, and separate draws for each hand with full state
restoration. Shared bone influences and the cap must receive the same delta
within each hand draw. A per-bone anatomical map need not block that uniform
operation; draw/palette identification and frame conversion still do.

The algebra preserves the source skinned deformation under those conditions;
it does not guarantee correct placement without them. Weapons and gameplay
consumers remain on their engine transforms, so this fallback alone does not
complete hands-and-weapons placement.

**Answers:** a small left-control test is justified, with output isolation
tested rather than inferred from pointers. The script-tick callback is an
acceptable reversible implementation trial, with actual callbacks and motion
as its evidence. Direct weapon-following tests may precede the full item graph.
Tag-based evaluation timing, independent control effects, exact target bones,
and the cause of the old rotation failure are not settled by the current
snapshot. The reported cap recovery is a valid baseline for these runs, not
proof that a moving wrist cannot expose the existing cut-weight issue.

**Next action for implementation:** build the Phase-1 translation lever and
run its zero/positive/negative/off sequence. No additional review-only probe
or full native-ordering report is required before that experiment.
