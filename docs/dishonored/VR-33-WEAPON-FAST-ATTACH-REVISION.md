# VR-33: shortest route to a correctly held weapon

Reviewed 2026-09-08 against `72db01f0`. Scope: revise the current implementation
plan so the next behavioral build can move the crossbow and loaded bolt. Keep
the working hands and current tested settings. This review changes no runtime
code or installed configuration.

## Decision

Retire the long whole-scene sweep as the required attachment gate. Use the known
owned components and a qualified full-transform match to identify their draws.
Rotation equality alone is a candidate filter, not an identity.

For initial attachment, reuse the ACTUAL hand correction in a common frame.
Apply it to the native weapon and bolt through their respective component
transforms. Their original alignment with the native hand is already authored
by the game. This avoids introducing a weapon pivot, a new grip calibration,
and an independently pinned bolt before any attachment has ever been seen.

The next build should contain matching, corrected attachment and automatic
readiness/refusal logs together. Do not require another reviewer round or a
separate headset build merely to print the next diagnostic. Activate only the
qualified path; the tester launches with the feature already armed.

## 1. Correct the diagnosis without spending another run on it

Confirmed from source:

- Census age is not maintained when its reporting path is disabled or bypassed.
- WaDraw is gated on a nonempty adoption table, so zero adoption prevents entry.
- Recorded palette sizes cannot safely size current shader reads/writes.

The following conclusions are not established by the supplied totals:

- Many world draws do not drown a disappearance test performed separately per
  correct signature. A 450-draw component can be detected among 90,000 draws.
  Total phase counts are also affected by the number of frames rendered.
- Since bones is stale, the top rows' bones=1 cannot establish their shader
  type, or prove that viewmodel draws were absent from the table.
- A changing background does not necessarily invalidate every component's
  signature. It indicates a confound; the actual target and control data matter.
- A vertex/index buffer pair is not universally one geometry or one instance.
  Shared/dynamic buffers and differing offsets/ranges can keep the pair alive
  while the selected weapon disappears. WiHash currently merges them all.
- The four-phase predicate requires absolute zero draws in both hide periods.
  WiTick publishes the phase immediately after changing the CPU visibility
  flag. There is no demonstrated render-queue transition boundary. Previously
  queued visible draws can contaminate a hide phase and reject the correct
  signature. Flag readback does not prove queued pixels use the new flag.

Keep the sweep only as an optional, TARGETED confirmation if needed. Do not
repair the entire sweep before attaching anything. If used, keep full draw
discriminators, group known passes explicitly, exclude unresolved transition
frames and use a known hand draw as the positive control. Do not infer absence
from unchanged scene-wide totals or tune a world-population threshold.

## 2. Remove the unreachable-entry and stale-state traps first

### Direct adoption

Put inexpensive routing counters at the general indexed-draw entry, before
the current g_waMeshN gate. Separate enabled, candidate checked, matched,
ambiguous, source unavailable, draw attempted, draw succeeded and restored.
Report these automatically even when matches remain zero.

The new matcher must return a concrete qualified draw/component/assembly record.
Do not pass it to the current WaAdopt(asset, comp) unchanged: that function
rescans g_wiSig and calls WiOwns again. It would silently keep requiring the
failed sweep. Replace its input with the match result, or let the matched draw
consume that result directly.

### Shader state

Refresh/lookup the layout for the shader ACTUALLY bound before reading the
weapon context. WaDraw currently calls MpAcquireCtx without PcRefreshLayout;
the globals may still describe the last HAND shader. A reflected register
number is only useful when its shader identity is current.

Read and save the actual consumed constant ranges. Obtain the skinned/static
classification from the declaration and shader position path. For a skinned
draw, establish the bone block layout and entries consumed by its geometry;
uploads may be partial and constants persist across many draws. The size of
the last c6 write is not necessarily the full palette size, even after fixing
the age counter. Check register start plus count against device limits.

Use the reflected bone register, not hardcoded c6. Delete the fallback that
calls three arbitrary registers a static object's one-bone palette. An unskinned
shader needs its actual local-to-world or combined position transform handled.

Maintain upload-age telemetry at the top-level original draw boundary if still
needed, independently of reporting and early returns. Count original draws,
not injected hand subdraws. Keep this heuristic out of attachment correctness.
Do not gate required state maintenance on WeaponIdentify or DrawCensus being on.

## 3. Route A, with a real identity test

Reuse the individually suppressed owned component records for the crossbow,
bolt and sword. Reacquire instances on equip/load; do not call broad legacy
collect/restore writers as if they were read-only discovery helpers.

Publish finite, validated component transforms with object/lifetime identity,
asset, ownership, snapshot generation/time and visibility/attachment state.
The historical reads at +0x60 and +0x90 are evidence to check, not independent
proof that a current native field is the render transform. Convert the native
matrix storage and shader storage into ONE column-vector convention explicitly.
Retain scale for comparison instead of silently normalizing it away.

### Establish the coordinate bridge using an independently known draw

The hand mesh already has a validated draw contract. Use its ORIGINAL draw
transform and the matching original CPU hand component transform as an anchor.
If native and shader transforms differ only by camera rebasing:

```
R_draw_hand ~= R_cpu_hand
eyeOrigin = t_cpu_hand - t_draw_hand
predicted_L_draw_weapon = translation(-eyeOrigin) * L_cpu_weapon
```

This obtains the offset from a separately identified reference rather than
declaring each proposed weapon match's own residual to be its camera position.
Where a verified renderer camera/rebasing origin is already available for the
same view, use that as another independent check. Do not substitute an XR eye
position in metres or a camera position from another generation.

If a common rotation is also present, test the complete frame conversion
`K = L_draw_hand * inverse(L_cpu_hand)` and predict `K * L_cpu_weapon` only
after establishing that both components use that same conversion. A viewmodel
adjustment need not equal the world-camera conversion. Do not silently absorb
different skinning/component conventions into a free fitted matrix per item.

CPU simulation snapshots and rendered poses can lag each other. Matching per
wall-clock frame or nearest Present is not proof of matching render generation.
Compare held-out, independently isolated component draws and retain timing
metadata. If the correspondence fails during movement, fix the generation or
frame conversion before loosening the tolerance.

### Match the whole predicted transform plus the draw contract

Use rotation AND translation AND scale against the predicted transform, within
the known owned-component/asset candidates. Several weapons and a loaded bolt
can have equal rotations, and attached components can even have equal full
LocalToWorld matrices. Transform equality alone does not separate those cases.
Use the known geometry/range/declaration/pass information as a second key.

Require a unique candidate and a meaningful margin to the runner-up. If both
crossbow and bolt are spatially indistinguishable but both are independently
known members of the SAME assembly and take the same correction, an assembly
match is sufficient for placement; do not invent a member identity. Ambiguity
between different hands, owners or native/world copies must refuse.

Residual consistency across several self-selected matches is supporting evidence,
not a proof: a wrong common offset or permutation can also agree. One brief
targeted hide/restore of the shortlisted component, with render transitions
excluded, is preferable to another full 26-second scene sweep when a tie remains.

For tolerances, use maximum angular or basis-vector error on orthogonalized
same-parity bases, plus separate positional and scale residuals. Derive operating
limits from the positive-control residuals and candidate separation. For initial
diagnostic display, 0.25 degrees and 1 uu are useful labelled provisional bands,
not measured authorization thresholds. Equal rotations cannot be separated by
choosing a smaller angle. Do not widen a band to compensate for stale snapshots.

Keep work bounded: cache shader layouts by shader identity/hash and reset epoch;
filter by known declarations/shader paths/geometry before device readbacks;
compare against the small owned-component list in memory. A few 3x3 comparisons
are not the main cost. Repeated shader reflection, constant readbacks and full
object scans over every world draw are. Proximity is only an optional coarse
filter; nearby world objects exist and a valid component origin may be far from
its visible handle. No arbitrary 100-150 uu correctness gate.

## 4. Replace the attachment's source calculation with the hand correction

The current attachment is NOT ready just because it has not executed:

- WaAssemblyFrame assumes palette slot 0 is the grip and uses its translation
  as the grip position. Skinning matrices can contain inverse-bind transforms.
- Weapon components can animate internally; every bone need not move rigidly
  with every other bone.
- Crossbow and the separate bolt each target the SAME palm independently.
  Applying a common delta within each separate palette does not make their two
  deltas equal. This can collapse/misplace the loaded bolt.
- g_mpPalmTarget is assigned only INSIDE the hand's MpWorldTarget draw path.
  The Boolean has no view/pose generation and is not generally cleared at every
  view boundary. Its comment claiming availability before the hand draw is not
  implemented. WaDraw can consume the previous eye/frame's target.

### The smaller correct first implementation

The hand path already computes D_hand_local that moves its original animated
hand into the tracked pose. At that point publish:

```
D_common = L_hand * D_hand_local * inverse(L_hand)
```

Include side, original component identity, pose/view generation, current eye,
and a validity state. Publish only after all hand inputs and the correction
are validated. If the existing model-size adjustment is active, include it
exactly once in D_common; do not apply another size factor in WaDraw.

For an original weapon/member draw with transform L_member:

```
D_member_local = inverse(L_member) * D_common * L_member
newPalette = D_member_local * originalPalette
```

This moves the original hand, crossbow and bolt through the SAME common-space
transform. Their existing native relative geometry stays aligned, including
animated bolt/socket motion. There is no weapon bone-0 pivot and no new weapon
grip to solve. All original animated member matrices remain the input.

For a qualified static path, the equivalent is
`L_member_new = D_common * L_member`. Only use it after reading that shader's
position and normal/lighting paths and updating dependent inverse transforms
consistently. Reuse the palette compose for known skinned paths; do not rewrite
both backends unnecessarily. Refuse an unsupported shader path by name.

This deliberate simplification preserves the game's hand-to-weapon relationship.
Independent per-weapon grip profiles can be added AFTER that works, if native
animation makes a different relationship desirable. The immediate goal does
not require a new root solver or a new calibration UI.

### Honor actual draw ordering

Log when the first qualified hand correction and weapon member occur in each
view. If the hand correction already precedes every member in the tested path,
use it with an exact generation check. Multiple material passes may reuse the
correction only while the original pose/view is unchanged.

If a required member precedes it, arrange a same-generation original hand source
and target at a verified earlier render/pose boundary. The pure target helper
alone is not sufficient: a correction also needs the original source hand pose.
Do not borrow the last-eye correction, defer/reorder the engine's draw, or
promise that a global valid flag solves ordering. Report this one specific
source-readiness failure and implement its producer. A partial body-only result
does not complete the crossbow-plus-bolt milestone.

The current eye classifier samples a particular hand component. Keep weapons
as consumers of a current view decision; do not mix their LocalToWorld values
into its history. The shared correction and coordinate bridge must refer to
the SAME view origin. A prior Present's camera-relative correction is not a
world-space correction just because it was named common.

Verify controlling side against the actual working hand mapping. The new
defaults say sword left and crossbow right, opposite the earlier component/
socket evidence for the ordinary game setup. Preserve an explicitly configured
swap, but do not treat those defaults or asset-name substrings as measured
attachment data. Unknown held assets must not all default to the crossbow hand.

## 5. Keep one scoped draw, and return its actual result

The current patch -> original draw -> restore structure is appropriate. There
is no redundant original draw when the handler reports that it handled it.
Do not patch constants and return false: that sends modified device state into
other routing paths with no reliable restore scope.

Return a handled status AND the HRESULT of the actual draw to the caller.
Currently the draw detour returns D3D_OK whenever WaDraw returns true, even if
the underlying draw failed. Count successful draw submission separately from
attempts. Restore saved constants on every exit, check restoration failures,
and avoid redrawing on an error after the draw was already attempted. Honor
the device-lost/reset path. Qualification failures leave the native draw alone.

Qualify and cover the required pass family so a color pass does not move while
its depth or lighting pass remains at the native position. Every modified range
is restored to the EXACT values in force before that draw. The weapon handler
must never alter the subsequent hand or world draw's constants or layout cache.

## 6. The next build and its acceptance test

Make one focused build containing direct matching/adoption, fresh shader state,
the shared correction path, scoped draw restoration and automatic counters.
The long sweep is off. Weapon placement is on in the installed configuration.
No unrelated scale, hand grip or gameplay changes. Keep the previous working
hand build/config available and restore any diagnostic material hiding.

Developer tests before installation:

1. A known hand draw is observed while DrawCensus and WeaponIdentify are OFF.
2. Two candidates with equal rotation cannot falsely identify each other;
   a known independent camera/frame reference disambiguates valid translation.
3. Direct match reaches attachment without any WiOwns/WaAdopt sweep result.
4. Different component origins and nontrivial rotations yield one D_common;
   the original hand-to-crossbow and crossbow-to-bolt relationships survive.
5. A stale view/pose packet refuses, and early weapon ordering is reported.
6. Non-c6 layouts, split uploads, persistent palettes and a static shader are
   handled according to their real layouts, not an upload age/count guess.
7. Forced upload/draw/restore failures do not leak constants or draw twice.

Tester steps, with automatic readiness and failure reporting:

| Action | Expected result |
|---|---|
| Launch and draw crossbow | Crossbow and loaded bolt appear in the tracked hand when qualified; no long blinking sweep. |
| Move/rotate that controller, then keep it still and turn the head | Weapon and bolt maintain their relationship to the palm. Other hand/world remain unchanged. |
| Inspect each eye | Consistent placement and depth, no stationary second copy. |
| Holster/re-equip, switch weapon | Current owned instances reacquire; old buffers do not retain authority. |
| Fire/reload | Loaded membership and native internal animation behave correctly. Released projectile aim is still native and is not claimed fixed here. |

The log must distinguish no candidate, ambiguous match, unsupported shader,
missing current source, attempted draw, successful draw and failed restore.
Every one of these can print even when nothing attaches. No further generic
probe-only headset run is required before this implementation is concrete.

## Answers to the four review questions

1. Camera-relative c231 is established for the inspected HAND path, not every
   future weapon pass or the legacy CPU fields. Assert the conversion against
   a separately identified reference in the same view. A candidate's own
   fitted residual cannot independently validate that candidate.
2. Compare normalized orientation with a proper angular/basis metric and compare
   translation and scale separately. Require unique identity evidence. Calibrate
   tolerances from controls, not from the desire to obtain a match.
3. Cache shader/geometry qualification and use the small owned-component set.
   Avoid per-draw reflection/object scans. Rotation arithmetic is cheap; readback
   and ambiguous instance matching are the important costs.
4. Keep a scoped patch/draw/restore handler, returning handled plus HRESULT.
   Patching and returning false is not a safe simplification of this router.
