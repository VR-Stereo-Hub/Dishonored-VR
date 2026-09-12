# VR-57: hand tuning and hand-following ray, revised execution plan

2026-09-12. Review of `VR-57-LASER-FOLLOWS-HAND-PLAN.md` against source at
`88327ce1`, whose implementation parent is `1af1527e`.
This document changes the plan only. It does not change or install game code.

## Decisions

1. Raise existing per-hand rotation trims to +/-180 degrees per axis. Keep
   translation at +/-0.25 m. Preserve all current values, bindings and calibration.
2. Add an opt-in hand-trim-following ray using a full rigid transform, including
   its origin. Do not add weapon rotation controls or change weapon placement.
3. Keep two independently buildable commits: range first, ray second. This is not
   a requirement for two user diagnostic runs. Do offline checks autonomously;
   the user launches the game and decides when to play.
4. A ray that follows trim is not yet a measured barrel axis or muzzle position.
   Do not claim those outcomes, or invent a per-weapon calibration to obtain them.

## Corrections to the original reasoning

- Saturation proves the control prevented further adjustment. It does not prove
  a converged calibration or diagnose a bad grip. Remove the categorical advice
  to recalibrate both hands. A neutral large-trim notice is sufficient.
- There are TWO rotation clamps: numpad adjustment in `hands/mesh_split.cpp` and
  INI loading in `core/config/config.cpp`. Updating only the former loses tuning
  on restart. Search all controls and validation paths before changing either.
- `palm_target` really does compose `O_C * G * Trim.r`, with translation
  `d_cam + (O_C * G) * Trim.t`. Its operands are camera-relative, whereas the
  published aim ray is XR LOCAL. A matrix cannot be applied across these spaces
  without deriving the change of basis.
- The hand mapping and stored grip can contain reflections. Do not convert the
  grip to an ordinary quaternion or drop its parity to make the algebra easier.
- Rotating direction alone does not attach the ray to a translated hand, and it
  rotates about no specified palm pivot. Rotation angle also is not generally
  the angular change of a ray: rotation about the ray leaves its direction fixed.
- Sharing an endpoint makes the fire hook target that endpoint when its guards
  accept. It does not make the beam and bolt the same line. The bolt starts at a
  different native spawn position; gravity and native assist remain in effect.
- ShotProbe's later velocity observations are not guaranteed launch measurements.
  Keep their age, identity and population attached to any result. A first-sight
  actor rotation is a proxy, not measured velocity. Do not use either as an
  unconditional proof of barrel or impact alignment.

## Commit 1: extend the existing tuning range

Use one named rotation limit, +/-180 degrees, for INI load and live adjustments.
Keep Euler order `Rz * Ry * Rx` and existing per-hand key names. Do not wrap angles,
re-solve grip, reset trims, add weapon-specific keys, or alter the other hand.
Reject nonfinite input before clamping and report the requested and effective
values. Verify legacy seed keys follow the same validation as per-hand overrides.

At the hard limit, say which hand/axis reached which limit. When a hand first
crosses 45 degrees, a rate-limited notice may say "large hand trim; retained".
It must not assert that calibration is wrong or request SHIFT+F7. Do not log on
every draw. Existing resolved-configuration logging should include both hands.

Check +/-45, +/-90 and +/-180; attempts beyond the new limit; NaN/infinity;
per-hand isolation; persistence through the actual INI load/save paths. Check
that translation still has its existing bounds. Build x86, lint, save the commit.
Install this build first if it is ready while ray work continues, preserving the
current INI. Do not launch the game or require a calibration run to proceed.

## Commit 2: apply the hand trim to the authoritative ray

### Contract and coordinates

Name the feature `FollowHandTrim`, not "muzzle alignment". Default it off in new
configurations with a live toggle. Off returns the existing AIM-pose ray directly.
On transforms that ray before BOTH visual and `FireFrame` publication. No consumer
may independently re-read AIM and recreate the old ray. Keep diagnostic grip-ray
visuals explicitly labeled; they are not a second authoritative firing ray.

First extract/reuse a pure helper for the effective palm transform, rather than
copying calibration/default/trim logic into `aim_ray.cpp`. Preserve grip version,
parity and the hand's effective calibration selection. Do not assume a stored
matrix is the one the draw actually uses; its uncalibrated parity fallback exists.

Derive the mapping from `hand_frame.h`:

    M = B * F * transpose(R_H)
    O_C = M * R_C
    P0_C = O_C * G
    D_C = P0_C * Trim.r * transpose(P0_C)
    D_XR = transpose(M) * D_C * M
         = (R_C * G) * Trim.r * transpose(R_C * G)

Here R_C is the controller GRIP orientation used by the hand, not the AIM
orientation used to seed the ray. M is orthogonal, possibly improper. The
cancellation is why the rotational trim transport can avoid a draw-basis read.
Verify that the actual pose snapshot and effective G match these definitions
before using the simplified expression. G and R_C*G may be improper; the final
D_XR must be a finite proper rotation. Use full matrices and shared parity code.

Transform the whole ray around the untrimmed palm pivot, all in XR LOCAL metres:

    Q  = R_C * G
    p0 = untrimmed palm origin expressed in XR LOCAL
    p1 = p0 + Q * Trim.t
    origin1 = p1 + D_XR * (origin0 - p0)
    dir1    = normalize(D_XR * dir0)
    endpoint1 = origin1 + DistanceM * dir1

Derive p0 from the existing hand position contract, including configured offsets
and units. Do not silently substitute AIM position for palm position. Prove the
translation conversion against the actual position path, including effective
scale. If an input cannot be expressed without a fresh draw measurement, mark
the mode unavailable with a reason rather than inventing a constant or reusing
a stale draw. The rotation-only cancellation does not prove the position mapping.

When both translation and rotation trims are exactly zero, return the original
ray directly to preserve its values bit for bit. With rotation zero but nonzero
translation, the ray MUST translate. Merely zeroing Trim.r is not an identity test.

This applies the same trim delta to the existing ray. It preserves any baseline
offset between the AIM ray and the weapon barrel; it cannot remove that offset.
If runtime AIM-to-GRIP relation varies, it also is not a fixed palm-local barrel
axis. Document that behavior rather than silently adding a new calibration mode.

### Publication and refusal behavior

Trace every writer before deciding there is "no lane bridge". Grip capture is
written on qualified draws; the aim publisher is separate. Publish an immutable,
synchronized per-hand calibration/trim snapshot with a revision. Combine it with
coherent current grip/aim samples. No unsynchronized reads of mutable matrices.
Do not hold the publication lock while calling XR, logging or doing engine work.

Publish the selected ray, distance, pose generation and trim revision together
for the dot, beam and `fire_frame()`. Keep the existing native fire guards and
MotionAim/Blink behavior. Do not change the native spawn position or fire hook.

An enabled mode with invalid tracking/calibration produces an invalid ray and a
reason; native firing then retains native aim through its existing guards. Do not
display a stale hand-aligned guide while firing uses something else. Stowing alone
must not invalidate a fresh analytic ray. Turning hand rotation off explicitly
must not leave a guide claiming to follow rendered rotation.

The computed target is an intended palm frame, not proof a draw applied it.
`g_mpPalmTarget` is optional diagnostic evidence only. If cross-checking it, add
hand, eye, pose generation, calibration revision, space and freshness metadata;
compare only matching records after explicit conversion. Current untagged last
values cannot establish agreement, and cumulative successful-draw counts cannot
establish current success. Do not modify the render path to force this agreement.

### Verification and installation

Host-test the production helper against an independent reference built from
`palm_target` and explicit XR/camera conversions. Cover both parity signs,
noncommuting calibration/trim rotations, arbitrary head/controller orientations,
translation with a nonzero pivot, mixed trim, both hands and the full new range.
Use matrix/ray tolerance 1e-5 and endpoint tolerance 1e-4 m for float tests; record
any justified tolerance change before judging results. Rotation about a ray and
rotation about other axes must both behave correctly.

Check exact zero-trim/off behavior; invalid/stale inputs; settings revisions;
save/reload; selected-hand isolation; visual/fire endpoint equality; and native
muzzle-to-endpoint convergence. Run existing hand-frame, aim-ray and native-fire
host checks, then x86 build, lint and diff checks. No simulator/game launch.

Install with FollowHandTrim enabled for the requested candidate after those checks
pass, preserving existing trims, selected aiming hand, distance and FireFromHand
setting. Do not enable DriveFromHand or legacy MotionAim. Save the previous files
and verify installed/build hashes. Two commits need not mean two test requests.

Handoff must name exactly what was changed and installed, effective settings,
test results, hashes and remaining limits. Do not describe the beam as emerging
from the actual muzzle: existing beam markers start 0.25 m along the ray, and no
continuous native muzzle source is established here. If physical barrel/muzzle
alignment remains required, that is a separate explicit design using verified
geometry, not permission to recreate the reverted weapon rotation controls.
