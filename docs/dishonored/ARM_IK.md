# Full-arm IK: reference review and Dishonored implementation design

Reviewed and revised 2026-10-04. Local branch `codex/ik-full-arms`, based on staging
`957322031d6c67aebd4f0910261f682501b43074`. No Linear ticket: local work was
requested because the workspace issue quota is exhausted. This document is a
reviewed implementation design, not an enabled or headset-tested IK feature.

## Reference versions

Both repositories were inspected from local source checkouts, including the
shoulder publisher, arm solver, callers and stereo replay path. Pin these
versions when comparing later changes:

| Reference | Revision | Relevant source |
|---|---|---|
| [BioShock IK improvements](https://github.com/VR-Stereo-Hub/bioshock-trilogy-vr/tree/feat/bs1-ik-improvements) | `e929cfe9513c6f9f7f22fa4136b19128cf999843` | `src/game/bioshock1r/bones.cpp`: `solve_arm`, driven/free-hand callers, `reapply` |
| [BioShock left-hand fork](https://github.com/Owloeb/bioshock-trilogy-vr-lefthand) | `3b5b818ed5cdb4e372320d0521e5b26f7a277c5e` | `src/game/bioshock1r/hands.cpp`: `publish_arm_targets`; `bones.cpp`: `arm_ik`, `frame_rotation`, `twist_about`, `write_arm_bone` |

Both declare the MIT license, copyright 2026 bioshock-vr contributors. Preserve
that notice with any substantive code adaptation. No source code is copied by
this documentation change. BioShock's bone indices, memory layouts and engine
write functions are not Dishonored interfaces.

## Decisions from the comparison

- Use analytical two-bone IK: shoulder, elbow, wrist. Derive lengths from named
  reference joint positions, with both ends in the same coordinate system.
- Use a single shoulder center and a total shoulder width. The fork already
  publishes paired anchors; the upstream revision mirrors the driven hand's
  settings onto its other shoulder. A center plus width avoids dependence on
  which hand currently holds a weapon.
- Use one yaw-only body frame for both arms, with the same artificial turn and
  recenter accounting as the hands. Upstream records a shoulder drift caused by
  subtracting drive yaw without also accounting for transferred recenter yaw.
  The fork uses head-yaw deadzone/drift; that is an optional later body policy,
  not a reason to bypass Dishonored's existing tracking transform.
- Keep elbow history in the shared body frame, separately for each arm. Upstream
  records coupling when history lives in the held weapon's frame, and locomotion
  lag when it lives in world space. Transform history into the current solve
  frame before using it. Do not advance temporal state per eye or per draw.
- Build an outward/downward elbow pole with mirrored lateral signs. Near a
  straight arm or a pole parallel to the shoulder-wrist line, use a projected
  previous direction, then a deterministic body-relative fallback. Never
  normalize a near-zero cross product.
- Prefer the fork's reference-frame rotation approach over assuming that a
  particular local bone axis means up. Skinning should preserve authored bone
  orientation and use the change from reference to solved pose.
- Derive forearm twist from that arm's own wrist orientation relative to its
  authored wrist/forearm relationship. Upstream documents a constant error when
  authored twist is not removed, and fractional-roll snaps at the +/-180-degree
  wrap. Keep continuity and reset it across invalid tracking or identity changes.
- The fork's two twist helpers and clavicle weighting are specific to its rig.
  Dishonored's sleeve and hand-helper bones require their own validated mapping
  and weights. Do not transplant helper indices or the fork's twist fractions.
- Both references replay cached writes for stereo. Reuse a single body-space
  solution for the two eyes of the same pose generation, then apply each eye's
  existing rendering transform. A cache must also carry identity and validity;
  a time threshold alone does not validate a mesh after a menu or load.

## Paired shoulder controls

Dedicated **IK** tab in the existing L3+R3/F10 menu, beside Hands. It contains
full-arm enable, shared shoulder position and width, and arm tuning. These
are proposed keys, not settings supported by the current binary:

| Proposed `[Hands]` key | Meaning |
|---|---|
| `ArmIK=0` | Experimental full arms; default off, with a live A/B toggle |
| `ArmShoulderForwardCm` | Positive moves both shoulders forward; negative backward |
| `ArmShoulderRightCm` | Positive moves the entire pair right; negative left |
| `ArmShoulderUpCm` | Positive moves both shoulders up; negative down |
| `ArmShoulderWidthCm` | Total left-to-right separation, not a per-side offset |

Let `O` be the validated body origin and `F`, `R`, `U` its forward, right and up
unit vectors. Convert centimeters to the solve's units once, with scale `k`:

```text
C       = O + k * (forwardCm * F + rightCm * R + upCm * U)
S_left  = C - k * widthCm/2 * R
S_right = C + k * widthCm/2 * R
```

The shared lateral offset is not negated for the left arm. Only the half-width
changes sign. These are the nominal anchors: both start at the same forward and vertical
coordinates, with midpoint C and the configured separation. Rotation of the
body carries the pair. The reach solve may independently displace either
shoulder from its nominal anchor, as described below. That runtime correction
does not alter the saved shared position or width.

No per-hand shoulder trim. Hand/grip calibration remains separate and continues
to own the controller target. Body origin, physical crouch and room-scale motion
must use Dishonored's established pose ownership; raw eye position must not
quietly become the body origin. Numeric defaults require calibration in the
verified runtime coordinate bridge, not direct reuse of Blender coordinates.

### Independent shoulder reach correction (revised requirement)

Follow the fork's reach behavior. The shared controls define each arm's nominal
anchor; reach correction is independent per arm and may temporarily put the
shoulders at different forward or vertical coordinates. This supersedes the
initial design's fixed-shoulder refusal policy.

For segment lengths a and b, nominal shoulder S0, and the final hand wrist W:

```text
n        = normalize(W - S0)
maxReach = (a + b) * 0.995
minReach = max(abs(a - b) * 1.05 + margin, (a + b) * 0.40)
d        = clamp(length(W - S0), minReach, maxReach)
S        = W - n * d
```

When the wrist is within range, S equals S0. Outside the range, move only that
arm's shoulder along the shoulder-wrist line, then solve the elbow with the
original segment lengths. The wrist stays at its existing final hand target.
The fork's margin is 0.5 in its solve units: its conversion to Dishonored's
units must be explicit, not a copied engine constant. Guard invalid lengths
and inconsistent bounds; at zero shoulder-wrist distance use a stable previous
or body-relative direction rather than normalizing zero. Tracking/identity
loss still refuses safely and resets temporal state.

Neither wrist motion nor reach history may move the other arm's shoulder.
Recompute correction from nominal anchors each pose generation so it does not
accumulate or remain after the hand returns to ordinary reach. Log nominal
and solved shoulder positions separately when diagnosing reach behavior.

### Dedicated IK tab

Use the existing overlay tab system in `src/core/ui/overlay_tabs.inc` so the
same IK page is accessible through L3+R3 and F10, with controller navigation,
scrolling and save-on-release controls. Place it beside Hands, including at
the Basic detail level. Do not bury IK inside the Sleeve group.

Planned contents: full-arm enable; shared forward/right/up offsets and total
width; shared arm-length tuning and outward elbow bias following the fork;
reset IK settings; and a concise active/unavailable state with the reason.
Any further tuning needs a real solver consumer before becoming a saved key.
Independent shoulder reach adjustment is normal solver behavior, not two
additional sets of shoulder calibration sliders. Existing hand/grip settings
stay on Hands. Sleeve cut/cap settings apply to floating-hand fallback, not
the full-arm mesh. A tab is not considered implemented until its controls
reach the runtime consumers and survive save/reload.

### Animation ownership: native hands, IK arms

While IK is active, suppress native upper-arm, forearm and sleeve motion in
the rendered pose. Preserve the existing hand/finger animation and weapon
behavior, including configured trigger/recoil animation and hand-back blends.
Use the final wrist after those effects as the IK endpoint, not an earlier
controller target that the visible hand has already left. IK owns the arm
bones; the current hand path owns the wrist and fingers. Native arm animation
must not be multiplied back onto an already solved arm.

The current `RigidWrist=1` path in `MsUpload` remaps forearm influences on the
clipped hand vertices onto the hand bone. It prevents forearm/twist animation
from bending the wrist cut and cap while retaining finger animation. This is
the existing protection relevant to the requirement; it does not provide a
full-arm IK pose or establish a named bone boundary. Keep it for floating-hand
fallback. Do not remap the entire full-arm mesh to the wrist.

For full arms, preserve authored skin weights and wrist-relative hand/finger
animation, then construct the arm palette from reference transforms plus IK.
Validate sleeve/helper bone membership and blend weights around the wrist.
`MpWorldTarget` applies `dvr::anim::blend` before publishing the correction
consumed by weapons; the IK endpoint must reflect that same final correction.
`g_mpPalmTarget` alone is earlier than the blend and is not sufficient during
an animation handoff.

`MsDraw` currently returns immediately for `native_full_arms()`, while
`native_draw()` can bypass controller palette correction for both hands.
The IK integration must replace the full-arm passthrough while it owns the
arms: retain the appropriate animated hands and solve arms to their wrists.
Selected actions must not silently restore animated upper arms. This changes
rendered arm ownership, not action eligibility, camera animation or gameplay.
With IK off, the current animation and full-arm action choices retain their
existing behavior. Invalid IK mapping uses the established hands-only fallback
and reports the reason; it must not pretend a native full-arm pose is IK.

## Dishonored integration boundary

The active hand placement is draw-scoped palette correction in
`src/game/dishonored/hands/mesh_split.cpp`. `MsDraw` obtains one `MpDrawCtx` for
both hands; `MpWorldTarget` determines their transforms; `MpBuild` applies those
to the source palette. It restores the original constants after drawing.
Native full-arm actions and native animated hands already have separate
ownership rules. Integrate them using the animation boundary above: preserve
hand animation, but prevent native full-arm passthrough while IK owns the arms.

The existing split deliberately works without a skeleton map. `MsBones`
calculates weighted vertex centroids and co-influence adjacency; `MsWrist`
identifies the hand cluster structurally. Those centroids are not shoulder,
elbow or wrist joint origins. They cannot establish arm lengths or inverse
bind transforms for IK. The documented 48-entry shader palette is also not
the 79-entry reference skeleton: reference bone indices cannot index it.

Local inspection of the player asset supports an upper-arm -> lower-arm ->
hand chain and authored skin weights. It does not establish the runtime
reference-to-palette map or the live coordinate bridge. Extracted files stay
local and untracked. See ENGINE_NOTES for the existing palette-mapping warning
and earlier bone-bank write attempts that had no visible effect.

Preferred first integration: mod-owned full-arm draw geometry and palette
transforms, conditional on proving the map and reference transforms. Keep the
source vertex weights. The current clipped sleeve buffers, cut caps and rigid
wrist remapping are unsuitable for an articulated full arm. Retain the existing
floating-hand geometry for feature-off and refusal paths.

An initial read-only mapping stage must record component/asset/LOD/section
identity, palette slot -> reference bone correspondence, named reference joint
positions, and the transform from reference mesh space to the current draw.
Validate hand endpoints against the current hand-placement path. Identity
changes invalidate the map, elbow/twist history and stereo cache together.

If the rendering route cannot establish that contract, investigate the native
post-animation pose seam separately. Old SpaceBases/LocalAtoms writes were not
visually effective; do not revive them merely because the arrays are readable.
Any engine-memory writer must use IsLiveObject against a table current for the
loaded level, and revalidate retained identity after menu/load transitions.
Class-name comparisons and unchanged pointers do not establish liveness.

## Implementation and verification order

1. Add the shared shoulder-frame and two-bone math as engine-independent code.
   Host tests must check paired translation, symmetric width, arbitrary body
   yaw, preserved segment lengths, singularities, invalid inputs, independent
   near/far shoulder correction, return to nominal and left/right independence.
   Test the exact functions the integration uses.
2. Establish the read-only runtime map and pose-space contract. Reuse existing
   reflected property and native query instruments only after checking their
   liveness and ABI guards. Log every refusal with identity and expected values.
   Put newly derived offsets in patterns.h and their evidence in ENGINE_NOTES.
3. Add complete weighted arm geometry and scoped palette corrections behind the
   default-off toggle. Solve to each final animated/driven wrist. Preserve fingers,
   weapon attachment, empty-hand mirroring and hand animation; suppress native
   arm animation while IK owns the arm pose. Verify exact
   restoration of the original draw state and no work when the feature is off.
4. Add the dedicated L3+R3/F10 IK tab and config persistence after its consumers
   exist. Validate full INI compatibility before installing a candidate; back
   up DLL/INI/logs, compare the entire INI, and verify CRLF byte-wise.
5. Host/replay checks: no opposite-hand coupling, no extra smoothing advancement
   for the second eye, no stale map after mesh/LOD changes, no stale solution
   after menu/load/recenter, no dependent writes when validation fails.
6. Headset tests, one question per launch, only after a candidate passes its
   applicable host checks. The tester launches; the developer archives logs,
   confirms the installed build banner and reads the results.

First mapping-only launch question: does the read-only collector resolve the
same valid named arm chains and palette map in gameplay? A valid mapping with
matching hand endpoints permits the render integration. Missing, ambiguous or
inconsistent mapping blocks arm palette writes and identifies the next probe.

First visible-IK launch question, after that prerequisite is resolved: with the
body and one controller stationary, does the other arm remain connected to its
hand when reaching from close to far? Its own shoulder may move at the reach
limits. A connected wrist and unchanged stationary arm support the fork's
reach policy; movement of the other shoulder/elbow indicates cross-arm
coupling; wrist separation indicates endpoint/scale disagreement.

A separate animation launch asks whether an enabled hand animation still plays
while its upper arm/forearm remain IK-driven and connected at the final wrist.
A native arm swing indicates the full-arm bypass or animated arm palette is
still active. A frozen hand indicates the animation boundary is too broad.
Later launches independently cover IK tab persistence, wrist roll, menu/load
lifetime and the shared baseline controls.

## Current result

The branch and both reference reviews are complete. Runtime IK, the configuration
keys and F10 controls are not implemented. No candidate DLL is installed and no
game launch is requested by this review. The next implementation step is the
pure solver plus read-only runtime mapping, not a write into guessed bone slots.
