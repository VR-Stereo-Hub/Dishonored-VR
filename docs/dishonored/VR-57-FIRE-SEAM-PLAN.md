# Plan: identify and decouple the crossbow's firing aim (VR-57)

2026-09-12. Revised against `859d7d82` on
`claude/vr-57-crosshair-on-hand-ray`. This is a documentation review only.
No source, DLL or ini changes; the user performs game testing.

Follows [VR-57-BOLT-PLAN.md](VR-57-BOLT-PLAN.md). Scope: identify the crossbow's
actual aim consumer, then direct the bolt through the controller dot without
changing the view or body-facing behavior. Assist changes, weapon-model alignment,
pistol, Blink and powers remain separate work. Keep both dots.

## 1. What the recent runs establish

The supplied findings report matching fired/scored populations on three runs
(5/5, 5/5, 6/6). The camera-origin sign correction reduced the launch/controller
gap from hundreds of metres to 0.5-0.8 m. With the cache drive enabled, the native
reticle followed the controller while off-gaze bolts remained about 21-23 degrees
from the requested direction, with 3.0-3.3 m endpoint miss. Near gaze, reported
miss was 0.06-0.10 m. These are useful distinctions; the new plan should build on
them rather than repeat the cache experiment.

**Decision: stop using the existing cache writer as the proposed firing seam.**
Keep `DriveFromHand=0` during source identification. Do not increase its cadence
or experiment blindly with `m_TickTag`.

The evidence supports a narrower conclusion than the original plan: the tested
write affects the reticle and does not control those shots. It does not prove
`m_AimDir`/`m_AimPos` are never read anywhere in the firing path. A refill, another
context, conditional use or a downstream override could produce the same result.
Only a reader trace establishes that stronger claim. There is no need to resolve
it before pursuing a better firing seam.

The crosshair's periodic downward excursion remains an observation, not a measured
refill cadence. Keep it separate from source identification.

Counts and headset results above are carried forward from the supplied plan and
repository notes; raw run logs were not reanalysed in this review. The code checks
below identify limits on what the current probe can establish.

## 2. Comparison narrows the candidates; it does not identify a reader

Keep all three candidate families. Log the two camera entries separately, rather
than treating them as one already-verified value.

| Candidate family | Observation and limits |
|---|---|
| Camera rotation | Read the camera-object entries named by `kCamRotBase`, with distinct labels for POV/cache. Also retain the published `g_viewYawRad/g_viewPitchRad` as a separate last-write record. A publication is not a readback of the camera at firing. |
| PlayerController rotation | Resolve `Actor.Rotation` on the live possessed controller. A different address from the camera does not establish independence: one may feed the other. |
| Pawn rotation | Resolve the same property on the possessed pawn. Visible body/arms orientation can include mesh, animation and mod transforms; it does not directly measure this field. |

Do not drop the pawn because its visible model faces left. The prediction is:
**if the measured pawn forward differs from the bolt beyond observation noise,
raw pawn forward is inconsistent with those shots.** That does not rule out a
pawn aim function that consults its controller or view.

Two relevant code corrections:

* The local `Engine/Pawn.uc` implementation of `GetBaseAimRotation` contains a
  controller `GetPlayerViewPoint` path and a pawn-rotation fallback. Therefore the
  generic assumption that base aim necessarily means PlayerController's
  `Actor.Rotation`, independent of the camera, is not a foundation for this fix.
  The native Dishonored crossbow path and applicable overrides still need tracing.
* `ApplyHeadToViewRotation` edits view-rotation event parameters and publishes
  `g_viewYawRad/g_viewPitchRad`. The separate `RotInjectTick` fallback writes
  camera fields and still references retired `kPcRotBase` entries on the
  controller. Record which writer is active; do not claim head tracking only
  writes a camera field. Do not enable or copy the fallback for this experiment.
  If it is active and contaminates the comparison, address that as a separate
  prerequisite, not an unrecorded change to this run.

An angle match means **consistent with**, not **read from**. Identical rotations
can share an upstream producer. None may match if the fire path uses another
cache, an adjusted aim, an offset target point, or a different sampling time.
A valid "none match" result must not be treated as a failed probe automatically.

## 3. Phase A0: qualify the observation before selecting a writer

Extend the existing read-only probe, without reopening the origin fix. The current
implementation has useful velocity observations but is not yet a verified launch
consumer trace:

| Current implementation | Required qualification for the next comparison |
|---|---|
| `AimShotSee` records position on first projectile dispatch and velocity on a later dispatch | First sight is not proven spawn time, and later velocity is not automatically initial velocity. Record both event names/call sites, timestamps and positions. Use stationary shots and bound observation delay; do not extrapolate a post-collision or late-flight velocity from the first position as a measured launch line. |
| `ShRec` matches by object address across calls; declared name/class fields are not populated | Reads use the current dispatch's live argument, which avoids off-call dereferences, but address reuse can merge shots or suppress them as already reported. Establish a verified spawn/reinitialization identity and gameplay epoch, including pooled reuse. If identity is ambiguous, exclude/count it. |
| The top-level filter includes projectile, bullet and grenade classes | Confirm player ownership and ordinary crossbow bolt class before scoring. Matching a short quiet run's count does not prove this filter excludes enemy shots. |
| The history selects the newest valid solve before first sight, without a maximum age | Label this an association, not the input consumed at fire. Record sample/solve/observation ages separately and reject stale or cross-epoch matches. Twenty-four dispatches are not a fixed number of game ticks or milliseconds. |
| `AimSeamSolve` copies the visual ray's generation but obtains direction via a new pose fetch | That label does not identify the pose used for the solve. Record the actual snapshot used; do not claim visual/drive identity until publication is coherent. |
| `ShRayPush(true)` recomputes after the write | It is not an exact copy of the written values. Any later write/consumer experiment must record the original locals, not a second solve tagged as written. This is dormant with the drive off. |

Reset pending associations on load, possession/weapon change, gameplay exit and
probe re-arm. Keep scalar history bounded; never dereference a remembered actor
address outside a verified live call. Preserve population counts for unscored,
late, foreign, reused/ambiguous and invalid samples.

Snapshot the candidate rotations together at first sight and again at velocity
observation; include the earlier associated view record separately. Do not compare
a new rotation with an old bolt and label them simultaneous. Stable values across
the interval allow a stationary comparison; dynamic causality still requires the
actual fire/initialization boundary.

Validate `Actor.Rotation` through reflected property owner/type/struct size where
available, class/possession and range checks, plus known motion response. A plausible
three-int range cannot prove a Rotator layout: integers wrap, and unrelated bytes
can look plausible. Decode signed/wrapped yaw/pitch consistently; retain raw values
and roll even though forward direction ignores roll. Readability checks do not
prove lifetime or semantics. Refuse unresolved candidates rather than borrowing a
camera offset for a controller.

Also verify signed-axis conventions offline. `ShFrame` currently forms horizontal
with `cross(d, worldUp)`, opposite the positive-right row used by the game-space
mapping for horizontal forward. Label or correct that sign before presenting
left/right residuals as a calibrated direction. Unsigned angle comparisons alone
do not detect this discrepancy.

## 4. Phase A1: distinguish rotation values, read-only

Candidate settings: `ShotProbe=1`, `DriveFromHand=0`, `MotionAim.Enabled=0`.
Keep dots, dot distance, body-facing controls, assist settings and stereo settings
fixed. Log their effective values and active head-writer path.

For each qualified player-crossbow observation, log:

* Raw and decoded rotations, forward vectors and validity for each candidate.
* Total and signed bolt-to-candidate angular errors, candidate-to-candidate
  differences, and first-sight-to-later-observation change for each candidate.
* Event/time/identity provenance, speed, observation delay and counted exclusions.

First try stationary, ordinary shots with head and body naturally separated.
Use a small repeated batch, for example five qualified shots per distinct pose.
Include a changed head pitch as well as yaw; visible body yaw alone cannot separate
all aim behavior. Keep the controller moderately off gaze so the endpoint metric
remains well conditioned, rather than aiming about 90 degrees away.

If needed, the user can change head/stick orientation and let it settle before
another batch. **Stick turning is not guaranteed to separate camera and controller.**
Inspect the logged pairwise difference first. If they still track together, record
an indistinguishable group and proceed to native tracing; do not spend repeated
launches trying the same uninformative maneuver or inject arbitrary rotations.

Set the comparison noise bound from stationary sample variation and observation
age before using the result to select a candidate. A useful distinction requires
candidate separation comfortably larger than that uncertainty. Do not reuse the
endpoint tolerance as a source-identification threshold.

Outcomes are: one candidate consistent, multiple indistinguishable candidates,
none consistent, or inadequate observation. All are legitimate. This phase ranks
leads; **none of its outcomes alone authorizes a write to Actor.Rotation.**

## 5. Phase B: locate the actual consumer before changing behavior

Follow the strongest lead into the native crossbow fire/initialization path.
When values are indistinguishable, trace the call/data flow that selects the aim
instead of naming whichever field happened to match first.

Observe naturally occurring calls and verified return/output values where possible.
`GetBaseAimRotation` and `GetPlayerViewPoint` are local leads; `GetAdjustedAimFor`
is a search lead, not a verified available Dishonored seam. Do not issue synthetic
aim calls and count them as the firing path, or assume all native calls pass through
ProcessEvent. A verified direct native call may require a separate hook.

Before choosing an intervention, document:

1. The concrete instruction/call consuming aim for this player's crossbow, its
   calling convention, parameter/return layout and timing relative to projectile
   position/velocity initialization.
2. Whether it reads a raw rotator, a function result, a direction, or an aim point;
   any adjustment between that value and the measured velocity.
3. Other consumers of the proposed field/result: camera, FaceRotation, body-yaw
   bookkeeping, arm-follow, movement and other weapons. Verify relevant downstream
   behavior rather than inferring independence from different memory addresses.

The legacy `aim_watch.cpp` is historical tooling, not a ready reader probe. It
watches writes to a hard-coded projectile yaw, retains an address and changes
thread debug registers. Its recorded stack-looking values are leads, not verified
callers. Do not re-enable it wholesale or call writer hits proof of the aim reader.
Any adapted watch needs a separately verified target, lifetime, thread coverage,
cleanup and a bounded observation window.

The existing cache writer remains off. A matching rotation is not a reason to
write it continuously and see whether the view moves.

## 6. Phase C: intervene at the narrowest verified aim boundary

Preferred order:

1. Override a verified shot-specific aim argument/result before the engine
   initializes this player's ordinary crossbow bolt. Keep camera/body state intact.
2. If the consumer truly requires persistent state, establish why and audit its
   other readers before proposing a dedicated write. Continuous Actor.Rotation
   replacement is not the default.
3. Shared-state swapping is a last-resort design review, not an automatic branch
   when bolt and camera values agree. A matching camera value does not prove that
   a shot-specific aim result cannot be intercepted downstream.

Scope to the synchronous consumer call, **not an entire firing tick**. Preserve
normal behavior on invalid tracking, stale data, wrong weapon, unknown ownership,
menus or unresolved contracts. Retain an independent default-off/live control for
the new firing override. One behavioral change per candidate.

If a shared rotation swap is unavoidable, document exact save/restore boundaries,
nested/reentrant calls, all normal/exception exits, object lifetime and concurrent
or reentrant camera/body readers. Restoring bytes cannot undo camera caches or
other side effects created during the swap. A same-thread restore or RAII wrapper
alone does not prove safety. If readers cannot be excluded or restore cannot be
guaranteed, do not ship that design. Never leave the camera changed until next tick.

### The value to supply must converge on the dot

Keep the bolt plan's definitions: `H` is the mapped hand origin, `d` the controller
direction, `T = H + L*d` the visible endpoint, `S` the actual firing origin.
If the verified consumer takes a launch direction/rotator from `S`, the target
value is derived from `normalize(T-S)`, not simply `d`. If it consumes a point,
supply `T` through that contract and measure the resulting direction.

Obtain `S` from the verified fire path before the override. If changing aim also
changes muzzle position, trace that order and measure the resulting `S`; do not
use the previous bolt's position or a guessed offset. This is a prerequisite for
an endpoint fix, not a post-spawn correction. Do not move the spawn to the hand.

Engine-side aiming does not guarantee muzzle meshes, attachments or traces all
follow automatically. A narrow shot override may deliberately leave the body and
weapon model unchanged. Measure firing/trace behavior and keep model alignment a
separate task. Never restore post-spawn steering to compensate for an unknown seam.

## 7. Acceptance: keep the 0.25 m bar, remove the false guarantee

Retain **0.25 m launch-line miss at the 8 m visible endpoint** as the proposed
first alignment gate, not as pixel-perfect or ballistic-impact accuracy. Its
approximate angle is 1.79 degrees only under the corresponding simple geometry;
with displaced origins, report the actual muzzle-to-target angle and endpoint miss.
Keep physical miss as the primary metric. Other dot depths need explicit criteria,
not a silent substitution of a constant angular tolerance.

A historical transverse gap of 0.27-0.62 m does not guarantee that every future
direction-only shot fails a 0.25 m test. Pose, muzzle position and uncertainty can
change, and the smallest reported gap clears the threshold by only 0.02 m.
For every validation shot compute the parallel counterfactual:

```text
parallelMiss = length((S-H) - dot(S-H,d)*d)
```

Use deliberately separated poses where `parallelMiss` exceeds 0.25 m by more than
the measured uncertainty. Require actual endpoint miss to pass while that
counterfactual fails. Include both horizontal sides and a vertical offset. If the
two models are within uncertainty, that shot cannot demonstrate convergence.

Set sample counts, usable observation age and uncertainty rules before evaluating
the behavioral candidate. Require all qualified shots in each prescribed condition
to meet the gate; report failures and exclusions rather than discarding them.
At least five qualified shots per selected distinguishing condition is an initial
protocol, not a statistical guarantee. First qualify stationary behavior; test
movement separately with coherent timing records.

A launch-line test uses the bolt plan's plane-intersection metric and refuses
ill-conditioned cases. It is not proof of later impact through gravity, collision
or homing. Assist settings remain fixed; target-dependent misses are reported as
remaining failures, not excused as a completed user-visible aiming feature.

The user must also confirm both-eye view stability, continued stick/body/arm
behavior and clean pause/load/re-arm behavior. Instrument actual camera output and
writer identity around shots; an unchanged `g_viewYawRad` alone cannot prove no
view flicker. Do not merge before those results.

## 8. Crosshair dip, implementation handoff and review answers

Leave the dip outside the firing-source experiment. Keep its existing observation
record; investigate its cadence separately if it persists with `DriveFromHand=0`
or prevents using the independent dots. Do not treat a HUD-cache cadence as a
scheduler for the native firing consumer.

Each implementation candidate needs relevant offline tests (rotator wrap/signs,
snapshot timing, reuse/ownership rejection and converging versus parallel launch),
lint, an x86 Release build and the installed build/hash/settings recorded. Keep
existing working visuals and stereo unchanged. The user launches/tests; never
launch the game or simulator, or automatically restore an older DLL. No merge to
`VR-Main` without explicit authorization.

Answers to the original questions:

1. **Keep the pawn candidate.** The visible model is not a readback of its actor
   rotation, and logging one more qualified field is cheap. Separate candidates
   by measured values; do not promise stick turning will separate camera/controller.
2. **Prefer a shot-specific argument/result at a verified consumer.** A firing
   tick is too broad, and continuous rotation writes conflict with more systems.
   Correlation alone is insufficient for either write.
3. **Keep 0.25 m at 8 m as a provisional alignment gate.** Report both metres and
   angle, and add a per-shot parallel counterfactual with uncertainty margin.
   The threshold alone cannot prove convergence.
4. **Defer the dip measurement.** It is not evidence of the new consumer's timing.
   Revisit separately if it persists with the cache writer disabled.

For Claude: this revision preserves the successful origin correction and measured
head/controller discrepancy. It replaces the HUD-only claim with the supported
result, qualifies the later-event velocity probe, distinguishes value correlation
from native consumption, corrects the local base-aim/head-writer assumptions, and
requires a shot-specific convergence fix rather than a speculative actor rotation
write. This revision changes only this plan.
