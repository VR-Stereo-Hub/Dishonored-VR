# VR-78 revised plan: pitch-dependent view-height error

**Status: reviewed plan; implementation and headset validation pending.**
Branch: `claude/vr-78-crouch-camera-pitch`. This revision is based on the
current local camera writer, clamp, tracking composition, and engine notes.
No runtime code or installed configuration is changed by this review.

## 1. Objective and scope

Remove camera movement caused by head pitch beyond the actual tracked head
translation, in standing and toggle-crouched gameplay. Preserve crouch/vent
clearance and writer rebasing. Determine whether the reported motion is camera,
hands, or both; a vertical camera test alone cannot clear the whole camera path.

VR-86 remains outside this work. The OPTIONS.sav finding is supplied context;
this review does not modify the profile or ticket. VR-7 and VR-55 need regression
coverage because stance transitions and the shared ceiling remain relevant,
even though their feature work is complete. Include VR-28 crawl clearance.

## 2. Review findings that change the original plan

1. **The log is evidence of clipping, not proof of an engine pivot being erased.**
   `FovLeverApply` reads four mutable camera fields and logs the first field
   clipped, without its identity. These may contain a previous mod write.
   It logs only when a clip happened, throttled to once a second; 70 lines do
   not establish every-frame clipping. Equal rounded standing values can mean
   a tiny positive clip. Neither quoted value proves the untouched engine eye.
2. **The writer has memory.** `clamp_written_z` reconciles an exact previous
   write by changing `lastOff` and `last`, preserving the original base.
   `current_base` can therefore recover a base ABOVE the just-clamped field.
   A fresh engine write and a persistent mod write follow different paths.
   A simple four-term pipeline omits this distinction.
3. **The second cap matters to the predicted symptom.** Even on the simplified
   fresh-base path, with ceiling C, tracked vertical R and engine arc A,
   a fully clipped base produces `min(C, C + R - A)` (ignoring eye Z).
   For R=0 and a negative arc, the final cap holds Z at C; it does not let
   the proposed upward correction through. With R<0 it can suppress the real
   head's descent, so the view can still be too high RELATIVE to the head.
   The hypothesis is plausible, but the original arithmetic is incomplete.
4. **A present is not a camera write.** ProcessEvent runs the clamp before
   `apply_offsets`, and SceneDraw calls `apply_offsets` again for the second
   eye. Clip counters count writer events, not unique rendered frames.
   Independently published latest floats cannot close a per-render equation.
5. **Slope is an optional measurement, not the verdict.** A fixed-position
   pitch sweep has almost no raw-Z variance. A slope is then undefined or
   unstable. Real head rotation also causes forward motion, which can look
   like height motion against nearby geometry.
6. **The proposed F1 changes collision policy.** Allowing raw head motion above
   the ceiling removes an existing protection. Do not bundle that into the
   first neck correction. A camera Z result also cannot prove hands are faulty.

Code anchors: `fov_lever.cpp:FovLeverApply`; `camera.cpp:current_base`,
`clamp_written_z`, `apply_offsets`, `pitchtest_verdict`;
`head_track.cpp` raw/neck composition; `ue3/process_event.cpp` and
`scene_draw.cpp:SceneDrawMaybeSecond`. ENGINE_NOTES "The pitch pivot" records
0.321/0.062 m in a standing simulator test with zero reported ceiling clips;
that is a calibration reference, not a crouch measurement.

## 3. Build 1: bounded accounting, default off

Add `[PosTrack] ZAccount=0` and `camera zaccount on|off`. Log the effective
setting and source. Instrument existing read/write sites without adding engine
writes, object discovery, or per-property/per-draw formatting. Keep runtime
cost bounded and compare diagnostic-on/off frame timing before installation.

Capture small records at the actual writer events:

- Camera identity/generation, field offset/sign, dispatch/write sequence,
  timestamp, eye/pass and existing draw/pose pair identifiers where available.
- Pawn world position, cylinder value and age, raw and eased ceilings, clamp
  validity, stance stability, and active lane/projection/tracking modes.
- Each candidate field's pre/post-clamp Z and whether it was the exact prior
  mod write. For the selected field, record `current_base`'s recovered base,
  persisted/fresh classification, and prior offset. Call an unclassified read
  `fieldBeforeClamp`, never `engineZ`.
- Raw tracked XYZ, signed neck XYZ actually supplied, pose sequence, eye offset
  in world XYZ, actual basis-transformed position contribution, and any dropped
  position contribution. Capture the values consumed by this write, not a
  later present's request. The signed neck term is negative arc in cancel mode.
- Base used, candidate final world Z, final cap delta, written world position,
  success/skip reason and ceiling state consumed by this write.

Publish coherent records with a synchronization scheme valid for the actual
producer threads. Plain/volatile floats or a sequence counter around racy
non-atomic payloads are insufficient. Use a bounded existing synchronized
mechanism where possible; count dropped records. Match records to fresh render
c5 samples through existing eye/draw tags. If identity or freshness cannot be
established, report UNMATCHED; never substitute unrelated latest values.

At each matched sample verify the local writer equation:

`writtenZ = recoveredBaseWorldZ + eyeWorldZ + appliedPositionWorldZ + finalCapDelta`

Separately compare written position to rendered position, converting c5 using
the selected field's established convention. Do not assume c5 is positive
world position. Check both eyes separately; pair only corresponding samples.
The earlier clamp is an observed event affecting base recovery, not an extra
term blindly added to this equation.

Use cumulative per-sweep buckets, reset explicitly at sweep start. Require a
settle interval after each pitch/stance change and at least 60 distinct matched
render samples per bucket. Keep DOWN near -30 degrees, LEVEL near zero, UP near
+30 degrees, with narrow target bands and actual angles recorded. Reject and
count movement, turning/roll, stale cylinder or pose, transitions, camera changes,
menus, teleports, missing basis, unsupported lane and other active camera tests.
Report progress periodically and flush completed/partial results on stop/quit;
a ten-second reporting interval must not discard a nine-second sweep.

Primary result: level-relative rendered movement minus the matched raw head
movement, corrected for per-eye displacement, in world up AND level-heading
forward. Report residual mean, spread, sample count, and write/render closure
error. Set a provisional 1 uu residual/closure target and report the observed
noise floor; if noise exceeds the target, mark the result inconclusive rather
than silently expanding the tolerance. Optional raw/render slope requires a
separate translation sweep with adequate variance and uncertainty reporting.

Do not automatically name the largest term as the cause: the engine and neck
terms should cancel each other. Emit evidence such as CLIPPED, PIVOT_MISMATCH,
UNMATCHED, LOW_VARIANCE, or NO_MEASURED_CAMERA_RESIDUAL, allowing multiple flags.

## 4. Launch 1: reproduce and decide

Record installed build identity and all resolved relevant settings first.
Use the same save in open, level space. Hold yaw approximately fixed and roll
near zero. Standing: level, down about 30 degrees, up about 30 degrees, each
for three seconds after settling. Toggle crouch without physically crouching
and repeat. Stand and repeat to expose hysteresis. Keep the controllers still
and judge against a fixed world landmark as well as the hands. Capture the
user's observation for each stance; an incomplete bucket needs only that
portion repeated. Then make a small independent vertical head translation to
measure whether ceiling saturation suppresses upward/downward tracking.

| Evidence | Next action |
|---|---|
| Matched base/cap events explain pitch-correlated residual | Develop F1 with existing final ceiling retained |
| Clean fresh engine samples show a different crouched arc | Calibrate F2; combine with F1 only if both are demonstrated |
| Up residual small but forward residual significant | Diagnose full neck vector/timing; do not divert directly to hands |
| Both residuals small, picture still wrong relative to hands | Investigate hand-to-camera transforms and pose selection; camera submission/compositor timing remains possible |
| Accounting fails or samples are stale/persistent/insufficient | Repair measurement or collect targeted data before choosing a fix |

Fit a crouched pivot only from independently identified fresh engine samples,
using actual angles, up and forward components, and repeated sweeps. Exclude
mod-contaminated/rebased samples. Fit per-sample trig terms, not trig of a broad
bucket mean. Require fit residuals and repeatability; three mean Z values alone
cannot provide meaningful independent model validation.

If ordinary gameplay cannot supply clean engine samples, use the existing
fixed-position simulator pitch procedure in open space as a separate controlled
calibration, with clamp/neck configuration explicitly recorded and restored.
An optional EyeClamp=0 A/B supports the clamp hypothesis but neither separates
the two clamp sites nor establishes a crouched pivot. It is not the primary test
and must not be performed beneath obstacles.

## 5. Build 2 candidates

### F1: preserve the engine arc through neutral-eye clamping

First establish which base path fails. For a confirmed fresh engine base E,
engine arc A and already-eased ceiling C, the mathematical target is:

`neutral = E - A`
`pitchPreservingBase = min(neutral, C) + A`
`candidate = pitchPreservingBase - A + raw + eye`
`finalZ = min(candidate, C)`

This preserves current final clearance while removing pitch dependence from
the neutral clamp, assuming the calibrated arc and pose are correct. It does
NOT promise unrestricted upward tracking when the ceiling is active.

Implement only after mapping this target to both fresh and persistent writer
paths. A replacement of `zmax` by `zmax + arc` alone is not sufficient: preserved
`lastOff` can recover the old unclamped base. Keep explicit ownership of the
neutral base/correction and preserve non-accumulating eye/position writes,
restore behavior, and all affected camera fields. Do not double-subtract arc.

Use one coherent consumed pose/arc/config generation for a correction and its
corresponding offset. Measure actual engine-pitch versus consumed-pose timing;
one-present disagreement is not accepted by assumption. Reuse the correction
state consistently through both eye writes. Ease C first and apply arc afterward;
never feed head pitch into the ceiling's tighten/release state machine.

Gate as `[PosTrack] EyeClampNeckAware=0`, with a live A/B and effective-value log.
Require projection + active camera position lane + an actually applied cancel
term; otherwise retain existing behavior. Specify reset/rebase on toggle, camera
replacement, tracking loss, lane/mode changes and teleport. No stale correction.

### F2: recalibrate only if the data demands it

Add crouch pivot settings only after reproducible calibration demonstrates a
stance difference. Defaults equal the standing pivot. Apply the same full XYZ
arc model used today. If interpolation is needed, define its units and duration
from measured transition behavior; 300 uu/s is a ceiling movement rate, not a
ready-made pivot blend rate. Handle vents/transition states explicitly instead
of treating every small capsule as ordinary crouch. If the arc does not fit a
rigid pivot, investigate the camera animation rather than forcing a fit.

### Separate decision: real head travel at the ceiling

Keep zero extra headroom for the first fix. If the independent translation test
shows unacceptable tracking loss, record that as a remaining collision-policy
problem. Capsule height alone cannot prove space is clear above the eye. A
follow-up needs measured clearance/collision behavior or an explicitly tested
comfort tradeoff; no guessed headroom default and no claim that unchanged C
means unchanged protection when raw motion is allowed above it.

## 6. Verification and exit criteria

For build 1: host build, applicable lint/exports/config checks, golden ini update,
and synthetic accounting checks for mismatched/stale records, c5 sign, per-eye
terms, low variance and contaminated engine samples. Verify logging disabled is
cheap and enabling diagnostics does not change camera behavior.

For build 2: extend the existing `tools/camera-clamp-host.ps1` production-code
harness to cover the actual new clamp/composition path, not just copied algebra.
Cover fresh and persistent fields, repeated writes/no accumulation, alternating
eyes, engine Z-only recomputation, clamp engagement/release at extreme pitch,
raw positive/negative motion, neck off/add/cancel, tracking/lane changes,
restoration and stale state. Keep existing clamp regression cases passing.

Repeat Launch 1 with the fix OFF then ON in the same conditions. Pass requires
matched accounting, no unexplained pitch-correlated up/forward residual beyond
the declared measurement tolerance in unsaturated samples, and the tester's
reported reversal gone. A fixed-position simulator sweep should keep the camera
fixed; a real-head sweep should follow real translation. Do not require zero
motion from a moving head or zero clips when a real ceiling is reached.
Explicitly label saturated samples and verify their motion matches the retained
cap policy. If that policy still causes the reported symptom, VR-78 remains open.

Check crouch/stand transitions, slide/deep crouch, vents and VR-28 furniture
clearance, plus stereo pairing and blink/load resets. Record any remaining
tracking saturation separately from the corrected pitch residual.

Before any eventual install, preserve the complete installed ini and compare
every key afterward. Preserve the tester's established settings, including
Pace Lag=2, Stereo LagAB=0, Hands PoseLag=2, PaletteEyeOffset=1, ModelScale=0.85,
MotionAim Enabled=0, GamepadOnly=0, 2750x2850, VirtualMode=1, 90 Hz,
AttachRigRadius=200, Aim FireFromHand/ModelRay/FollowHandTrim=1, and all grip,
trim and ModelAxisL values. Make only the intended diagnostic/fix changes.

**Next implementation step:** build the matched accounting probe. The existing
logs justify investigating clamp/neck interaction; they do not yet justify
relaxing the ceiling or installing a crouched pivot.
