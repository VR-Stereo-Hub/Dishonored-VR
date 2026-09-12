# Plan: make the bolt follow the controller dot (VR-57)

2026-09-12. Revised by Codex against `4da7989a` on
`claude/vr-57-crosshair-on-hand-ray`. **Plan only; no implementation, build,
installation or game launch is part of this revision.** The user tests the game.

Scope: measure the crossbow bolt's launch path relative to the controller's
fixed-distance dot, then fix the first demonstrated discrepancy. Keep the head
dot and controller dot as user-facing references. Aim-assist changes, weapon-model
alignment, pistol, Blink and powers remain separate work.

## 1. Review decision

Keep the measurement-first approach, but make the distance sweep conditional.
The next candidate should add observation with `DriveFromHand=0`. Do not change
cache freshness, distance, visual placement and write timing together.

The original plan could report success for the wrong geometry: **a bolt parallel
to the controller ray need not pass through the dot.** A distant aim point can
reduce the angle between the rays while making the bolt miss the visible dot at
8 m. Direction agreement is one measurement, not the acceptance criterion.

| Earlier conclusion | What the evidence establishes |
|---|---|
| 485,083 writes, zero refused proves the drive works | The writer passed its own guards in that run. It does not prove which values the firing consumer used. |
| Equal angles off the head prove exact mapping | They preserve one unsigned angle. A mirror or rotation around forward can pass. Signed axes and origin mapping remain to be checked. |
| Build 111 proves all layer alignment | It supports alignment for the tested stationary head/centre-view case. It does not establish moving or off-axis accuracy. Do not repeat the settled case without contradictory evidence. |
| Build 112 settles the physical pointing pose | Accept the reported headset result: the AIM-pose dot tracks the controller. Do not reopen grip-offset tuning or use the rotated weapon mesh as the reference. |
| Pull toward the crosshair proves an assist clamp | It is an observation. Assist, stale aim, coordinate error and other downstream processing have not been separated. |
| `m_bJustFired` identifies the spawn tick | The property is declared; its native set/clear timing has not been verified. |
| The pause crash proves a dangling aim pointer | The crash is not attributed; STATUS records the same signature in older runs. Lifetime safety is still required. |

Historical findings are in [VR-57-AIM-PIPELINE.md](VR-57-AIM-PIPELINE.md).
Where that document states stronger conclusions, use the qualifications above
for this investigation. Previous logs were not reanalysed for this revision;
run counts and headset observations are carried forward as reported evidence.

## 2. Current code, checked rather than inferred

Checked in `src/game/dishonored/aim_seam.cpp`, `aim_ray.cpp`,
`ue3/process_event.cpp`, and `src/core/config/config.cpp`:

* `AimSeamDrive()` runs from the ProcessEvent observer **before the original
  engine dispatch**. This is not a verified fire-consumer hook. Frequent writes
  cannot exclude a later native refill or a different firing path.
* The inventory probe refreshes its context on a 250 ms cadence. The drive uses
  the cached pointer with gameplay/class/readability checks. These do not prove
  it is still the firing item's live context.
* The drive writes `m_bFound=1`, `m_AimPos`, `m_AimDir`, conditionally
  `m_ProjectedAimPos`, and `m_bWillTrack=0`. It leaves `m_TickTag` unchanged.
  Comments saying the projected field is untouched are stale. Holding assist
  settings fixed does not make these existing cache writes semantically neutral.
* The visual consumes `dvr::aim::Ray`, including generation, timestamp and
  validity checks. `AsHandDirGame()` separately fetches head and AIM poses;
  it does **not** consume that ray snapshot or its freshness checks.
  A common pose source is not yet one shared sample.
* The drive origin is `camera::render_pos + mapped(hand - head)`. Hand/head
  samples, render position and view yaw/pitch are obtained separately. Their
  timing and the camera position's eye/centre semantics need provenance,
  not another unexplained offset.
* Positive `DriveDistanceUU` is a game-unit distance. Zero follows
  `Crosshair.DistanceM * uuPerM`; the current conversion uses `PosTrack.Scale`
  when greater than 1, otherwise 100. The scale defaults to 108.
  The drive-distance key itself defaults to **800**, not zero.

Record effective installed values before each future run. The last reported
installed state is `DriveFromHand=0`; this revision does not change it.

## 3. Define what "follows the dot" means

Use the same shot-time coordinate frame for these quantities:

```text
H = mapped controller aim origin
d = unit controller aim direction
L = visible dot distance, converted to game units
T = H + L*d                     visible dot endpoint in game space
P = point written to the cache  (may differ from T during a distance test)
S = actual projectile launch position
b = measured initial unit velocity direction
```

Log both candidate launch directions: `d` and `normalize(P-S)`. Neither is a
measured bolt direction. Compare each with `b`, reporting signed horizontal and
vertical residuals as well as total angle.

Measure the straight launch line at the plane through `T` perpendicular to `d`:

```text
t = dot(T-S, d) / dot(b, d)
miss = length(S + t*b - T)
```

Reject this metric if the denominator is near zero, the intersection is behind
`S`, or any input is invalid. Report exclusions, not zero miss. Also report the
transverse origin separation, not just `length(S-H)`.

This distinguishes a parallel displaced bolt from a bolt aimed through the dot.
It measures initial launch geometry. Gravity, spread, collision and homing can
change later flight; a launch-line intersection is not an observed impact.
The fixed-distance dot is not yet a surface trace or ballistic impact marker.
Overlap in one eye on a nearer wall is not proof of a hit at `T`.

## 4. Phase 0: establish the observation point offline

Trace the native crossbow fire path and cache reader before adding a hook.
Identify when the cache is filled/read and where initialized launch position and
velocity become available. Script names are leads, not a native calling contract.
Put verified engine addresses/patterns in `patterns.h` as usual.

Prefer copying launch values from a verified synchronous firing/initialization
call while the engine guarantees their lifetime. If the usable seam supplies a
projectile, copy validated scalar values during that call. Do not retain an actor
pointer for a later tick, scan all actors each frame, follow a projectile list
speculatively, or restore legacy post-spawn steering.

The local declarations provide `Location`, `Rotation`, `Velocity`,
`m_InitialLocation`, `m_bInitialized`, `m_bJustFired` and `m_pSourceActor` as leads.
Verify inherited field types, offsets and boolean masks before reading. Establish
what identifies the player's crossbow projectile; do not borrow an ActorComponent
ownership layout for an Actor or assume source/instigator semantics from a name.

`Rotation` need not be velocity direction. Zero velocity during construction is
not a forward shot; a later nonzero velocity may already include gravity or
collision. Record the observation stage and elapsed time. Only label it "initial
velocity" after verifying initialization order.

Verify hook bytes, calling convention, thread, reentrancy and parameter lifetimes.
Memory readability proves accessibility, not object liveness. Do not dereference
a pointer after the original call if that call can destroy it. If a safe output
observation cannot be established, ship only a clearly labelled
cache-at-observed-event probe. That fallback does **not** prove consumption or
bolt direction and does not unlock a success claim or blind distance sweep.

## 5. Phase 1: instrument existing behavior

One candidate, read-only additions, `DriveFromHand=0`, `MotionAim.Enabled=0`.
Keep both dots, their depth and assist settings fixed. Use ordinary crossbow bolts,
a steady head/controller and a clear firing lane without a nearby assist target.
Count fire attempts separately from verified projectile spawns.

Each record contains copied values and explicit provenance:

* Session/gameplay epoch, shot sequence, observation stage/time, verified weapon
  context and player attribution. Address equality alone cannot bridge object
  destruction/reuse.
* Controller sample ID/time/validity, head and game-view provenance, camera
  position/eye information where available, and the visual's sample ID. Missing
  metadata means "unknown", not "same frame". Establish coherent snapshot
  publication before trusting a generation label across threads.
* `H`, `d`, `T`, effective scale/distance; cache contents before/after a drive
  write when enabled and at the verified consumer if available. Include flags,
  projected point and the unchanged tick tag.
* `S`, raw velocity, speed, `b` and section 3's metrics. If only a later actor
  observation exists, label it accordingly.

Associate the exact candidate write with its consumer/shot using a bounded
history of scalar copies. A one-second log line or "latest ray when the bolt was
noticed" does not identify the firing input. Nested events must not replace the
outer shot's record silently. In drive-off mode calculate the proposed ray
without writing it so the instrument still has a baseline.

Report populations: attempts, confirmed player-crossbow spawns, usable launch
samples, matched writes, unmatched/ambiguous samples, invalid data and dropped
records. Matched values alone do not prove causal consumption. Exclude ambiguous
correlation from residual analysis while retaining its count.

First gate: user-observed shot count agrees with the confirmed population,
exclusions are explained, and drive-off observation can show a nonzero
bolt/controller discrepancy when the controller points away from head aim. An
instrument that only reproduces its input ray fails this gate.

Next run, same candidate/settings except `DriveFromHand=1`: measure the existing
drive before changing it. Repeat the off baseline if needed. Invalidate pending
associations on pause, load, weapon change, gameplay exit and tracking loss.
Verify transitions with the user; no automated game launch.

## 6. Phase 2: fix the first demonstrated break

Choose the next build from evidence. Do not schedule all these mutations.

| Observation | Next bounded action |
|---|---|
| Signed XR/game mapping disagrees | Fix/calibrate mapping first. Test centre, left, right, up, down and changed head yaw, including origin offsets. Rotating the controller left is different from merely moving it left. |
| Visual and drive use different/stale samples | Establish one validated aim snapshot and a documented game-view transform. Give visual/fire consumers explicit sample identities; do not blindly read mutable visual state across threads or make firing depend on Dot being enabled. |
| Wrong equipped context, refill after write, or write missing at consumer | Fix ownership or place the write at the verified consumer seam. Establish native tag semantics before any tag-write experiment. |
| Launch agrees with `normalize(P-S)` but not `d` | Evaluate endpoint miss. Muzzle-to-endpoint convergence can be correct; nonzero direction angle alone is not a fault. |
| Launch agrees with `d` but misses `T` | Direction-only firing from a displaced muzzle does not meet the endpoint goal. Use the verified fire contract to converge on `T`, in a separate candidate. Do not move the spawn or compensate with a guessed far point. |
| Neither launch model fits | Investigate downstream processing and observation timing. Blend, spread, target choice, another cache or later velocity sampling remain possible. Do not label this "assist proven". |

Signed mapping checks precede interpreting a distance sweep. Add offline tests for
the actual mapping/measurement code before the next behavioral candidate: signed
axes at multiple head yaw/pitch angles, translated origins, differing scales,
parallel-but-offset versus converging trajectories, invalid/stale samples and
unmatched/reused shot identities. Deliberately mirrored input and displaced
parallel launch must fail their intended checks.

### Conditional distance experiment

Only run after mapping, sample correlation and output observation pass. Keep the
visible dot at baseline depth. Change only the written point's distance; log `P`
and `T` separately so the experiment cannot silently redefine the target. Compare
baseline, farther point, then baseline again. Add a third distance only if needed.

For nominal 8/30/100 m and effective scale 108 UU/m, distances are
864/3240/10800 UU. Recompute from the actual effective scale. Zero follows the
configured visual depth; it does not inherently mean 8 m.

A purely transverse 0.4 m origin difference predicts about 2.86/0.76/0.23 degrees
at these distances if the consumer aims from the muzzle to the point. This is an
illustrative model, not a measured muzzle offset. Compute the exact prediction
from each shot's `S`, `H` and `P` and compare both models' residuals. Constant error
or an inverse-distance trend alone does not prove which native field was read;
other processing can imitate either.

Keep target conditions, hold duration, ammunition and assist configuration fixed.
If target-dependent processing prevents isolation, report the limitation and
carry it to the assist investigation. Do not tune around it or declare bolt work
complete. Never leave a far experimental point as the fix merely because its
angle is smaller.

## 7. Both dots and the native crosshair

Retain the larger head dot and smaller controller dot as independent optional
features, default off in the tree with live controls. Keep `ControlDot` during
bolt investigation; a cosmetic rename adds configuration risk without helping
the shot. A later rename must preserve the existing key as an alias. Leave the
settled visual placement unchanged for these comparisons.

The head dot is a head-direction reference, not a guarantee of the engine's
firing solution or impact. Document native-crosshair lag as an observation;
its precise cause is not proven by stationary alignment. Do not claim that all
flat HUD elements cannot be reprojected or that this lag is necessarily intended.

Keep existing projected-cache-field behavior fixed throughout the causal tests.
The native reticle can move with the drive and is not an independent reference
in drive-on runs. Whether to stop driving it is a separate display change after
the bolt result; no new lever is needed for the first probe.

## 8. Completion and handoff

Before testing, record numeric angular/endpoint-miss tolerances, sample count per
condition and maximum usable observation age, justified by baseline noise, dot
size and measurement precision. Do not select thresholds after seeing candidate
results. Use repeated shots on both sides and above/below head aim. Stationary
validation comes first; movement is a separate run with timing data.

Bolt launch alignment is complete only when launch geometry meets the agreed
tolerance at the visible endpoint, population/ownership/timing checks pass, and
the user confirms intended aiming behavior without new flicker or pause/load
regressions. Later trajectory/impact claims require their own evidence. If assist
still causes misses, report the narrower launch result and remaining failure;
the visible behavior is not fully solved.

For each implementation candidate: one behavioral change, default-off/live
control for new levers, relevant offline tests, x86 Release build, lint and
installation under the user's existing workflow. Never launch the game or
simulator; never restore an older DLL automatically. Hand over installed
build/hash, effective ini values, one test question and evidence location. No
merge to `VR-Main` without explicit authorization.

### Answers to the original review questions

1. **Observe output at a verified synchronous seam.** Do not add actor polling
   and retained pointers. If only cache observation is safe, state its limit and
   continue native investigation before claiming bolt correctness.
2. **Other models remain possible.** Measure exact point/direction residuals,
   sample age, hold duration and target conditions. A distance sweep is supporting
   evidence, not a two-outcome oracle.
3. **No blind tick-tag A/B.** Read the producer/consumer contract and fix ordering
   first. One line of unknown cache semantics can invalidate the test.
4. **Keep projected-field behavior fixed for now.** Keep both dots; decide native
   crosshair presentation separately after launch alignment is measured.

For Claude: this revision corrects the success metric, separates observations
from hypotheses, identifies the independent visual/drive samples and
pre-dispatch write, replaces speculative actor polling with an observation whose
lifetime is verified, and makes the distance sweep conditional. No source, DLL
or ini was changed.
