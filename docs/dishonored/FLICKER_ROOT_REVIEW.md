# Weapon flicker root proposal: review and replacement plan (VR-69)

2026-09-09. Reviewed source: `7b182115`. Decision: **do not install the proposed
build as written**. The idea of replacing an eye guess with a verified coordinate
transform is worth pursuing. This implementation does not yet provide that
transform and contains a concrete regression in its refusal path.

The outcome is stable weapons with correct size, depth, attachment and motion,
while preserving the confirmed world and weapon judder fixes. Improving a counter
is not acceptance. Nor can one placement change reasonably promise to fix capture
reuse and object-matching failures that the proposal itself excludes.

This review changes documentation only. No game settings, installed binaries or
rendering code were changed. The latest available log identifies an earlier
`vr33-hands-working-94-gcee51d35-dirty` build, not the matrix proposal; it is not
evidence that this new behavior works or fails in the headset.

## 1. Blocking findings in the built code

### P1: Refusal does not execute the promised fallback

At `mesh_split.cpp:2488`, the new matrix path is the first arm of an `if` followed
by an `else if` containing the old eye offset. When the matrix path is entered
but its inner `sane` test fails, execution logs refusal and leaves that arm.
It does not enter the old arm. The resulting target receives **no eye offset**.

| Candidate state, with offset and matrix levers on | Actual behavior |
|---|---|
| `eyeMeasOk == false`, old eye known | Old offset applied |
| `eyeMeasOk == true`, gate accepts | Measured scalar applied |
| `eyeMeasOk == true`, gate rejects | No eye offset applied |

The log claiming that refusal retains the decided path is false. A candidate
that moves in and out of the gate can switch between measured placement and
uncorrected placement, creating precisely the kind of discontinuity under review.
Fix and test this control flow before any behavioral experiment. A diagnostic-only
candidate must leave the existing output unchanged on both success and refusal.

### P1: `camera + 0x80` is not an established head-centre reference

This is already addressed in `ENGINE_NOTES.md:364-378`: the measured `0x80` value
was a fixed offset vector, not the rendered camera world position. The notes
explicitly retire the older interpretation. The later correction at approximately
line 419 establishes `0x330` as the camera world position and c5 as its negation;
it does not rehabilitate `0x80`.

The historical `patterns.h` comment and existing reads in `blink.cpp` and
`aim_ray.cpp` are not independent validation of the reference. The new code copies
their assumption. Those consumers warrant a separate origin audit; do not change
their behavior incidentally in this fix.

Substituting `0x330` is not sufficient either. `camera::apply_offsets` writes the
selected eye displacement, positional tracking and possibly a ceiling adjustment
into the active field. That is not an automatically unoffset head centre. Also,
the live object can describe a later game-thread camera than the queued draw.

The reference must identify the intended head centre, coordinate origin, applied
tracking offsets, and view/time association. A readable memory range proves none
of these. The implementation also lacks the stronger camera object identity
validation used by `CamStillValid`, and reads through the live global pointer
without publishing a coherent camera snapshot.

### P1: The proposal retains a cross-thread/time association

`MpAcquireCtx` reads the matrix at the draw, then reads the live camera object.
It has removed the *eye publication* from this path, but replaced it with a
different unassociated live reference. An old rendered eye centre minus a newer
head centre includes head/body movement, not just eye separation.

This can pass even an exact half-IPD gate. In a synthetic example with h=3.2 uu,
the correct right-eye camera is 103.2 and its matching head centre is 100. Reading
a later head centre of 106.4 yields -3.2: exactly 100% of half-IPD, with the wrong
sign. The proposed gate accepts it and applies +3.2 instead of -3.2, a full-IPD
difference. This is a counterexample to the safety argument, not a claim that
these exact coordinates occurred in the game.

### P1: The gate does not mean what the plan or log says

The actual condition is:

```cpp
halfIpdUU > 0.01f && fabsf(eyeMeasR) <= halfIpdUU * 2.0f
```

It accepts zero, every value between zero and half-IPD, the wrong sign at exactly
half-IPD, and twice half-IPD. It does not test distance from the expected stereo
magnitude. An erroneous zero in a stereo draw silently recreates the disabled
offset behavior. A bad value inside the interval is used, so the claim that the
gate cannot make behavior worse than OFF is unsupported even after fixing fallback.

`eyeMeasU` and `eyeMeasF` are computed but neither checked nor reported. Large
orthogonal residuals can therefore coexist with an accepted sideways number.
There is no explicit finite/conditioning validation for the recovered camera and
reference at this boundary. A NaN right candidate fails this particular comparison
and currently takes the broken refusal path; zero is counted as implied LEFT.

Do not repair this by requiring agreement with the old eye decision: that makes
the replacement depend on the signal it is supposed to correct. Use independent
geometry and provenance validation, with agreement retained only as telemetry.

### P2: Default ON and the proposed live test are not justified

Both the static initializer and config fallback enable the unverified path.
The proposal's safety argument does not hold. Return the experimental behavior
to default OFF; bounded diagnostics can be enabled independently.

A repository search finds the new lever only in its initializer, config load and
consumer. No live setter or scheduled matrix A/B was added. Config loads through
the one-shot `EnsureConfig` path. Editing the ini alone does not implement the
promised in-session reversing comparison. Add a real render-safe toggle and
resolved-state logging, or explicitly describe separate launches as the test.

## 2. Is the matrix recovery mathematically valid?

**Conditionally, but it recovers a camera in the matrix's input coordinate system,
not necessarily an absolute world camera.** The fourth row of a combined VP is
not directly a camera position.

For the row-vector convention `clip = [p,1] M`, define the three coefficient
vectors from columns 0, 1 and 3:

```
a = (M00, M10, M20)
b = (M01, M11, M21)
c = (M03, M13, M23)
```

For a conventional finite-centre perspective view, the camera centre C satisfies:

```
a dot C = -M30
b dot C = -M31
c dot C = -M33
```

With symmetric projection, `a = sx*r`, `b = sy*u`, and `c = f` under the assumed
positive clip-w convention and unit scale. Dividing by the focal norms gives the
components in the proposal. The existing code calls these matrix rows, although
its array indexing gathers columns. Shader storage/transposition and executed
instructions must be verified; names and CTAB register locations alone do not
establish multiplication semantics.

For an asymmetric projection, `a` and `b` include forward-axis principal-point
terms. Normalizing them does not recover physical camera-right and camera-up.
The standard Direct3D off-centre matrix makes those terms explicit.
[Microsoft: D3DXMatrixPerspectiveOffCenterLH](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxmatrixperspectiveoffcenterlh).

The existing orthogonality test rejects substantial asymmetry, but allows small
terms below its 0.02 tolerance. It therefore does not prove exact symmetry. A
different viewmodel FOV alone is not asymmetry and would not invalidate the
conditional derivation. Actual viewmodel constants and shader operations are
needed to answer which case the current game uses.

A general diagnostic can solve the three equations for C with finite, rank,
conditioning and residual checks rather than treating normalized projection
columns as an orthonormal camera basis. This handles conventional off-centre
perspective geometry, but does **not** solve the origin or head-centre problem.
Do not treat the existing orientation path as newly validated by that solver.

### The camera-relative viewmodel is the critical caveat

`ENGINE_NOTES.md:3419-3430` records shader and numeric evidence that LocalToWorld
maps this mesh into a camera-relative world frame. The shader then applies VP to
those coordinates. If a draw uses a translated input origin O, the recovered
camera is `C_world - O`; the matching reference must be `H_world - O`.
Subtracting `H_world` directly mixes spaces.

In particular, if O is the eye camera, the recovered camera is zero. That is a
correct recovery in that input space, not proof of a mono pass. Eye displacement
may already live in LocalToWorld or another stage rather than the VP translation.
The current proposal has not identified where this viewmodel pass retains it.

The available engine notes make this a serious blocker. They do not establish
the exact current constants of every weapon/material pass, so the review does
not claim every present-day VP has zero translation. Capture and replay qualified
draw packets to settle that before using the result for placement.

## 3. The information that cannot be deleted

A single eye VP cannot determine its displacement from a head centre that is not
otherwise supplied. The same eye camera C can be a right eye with head centre
`C-h*r` or a left eye with head centre `C+h*r`. The eye matrix is identical in
those two cases and the required correction has opposite signs.

That is why replacing the scalar eye choice is possible, but eliminating the
reference and association requirements is not. There must be a verified centre
or paired-eye relationship somewhere in the calculation. A current global camera
is another inferred reference, not a removal of inference.

The useful target design is:

```
target_in_draw = matching_head_centre_in_draw + mapped_controller_offset
```

The mapped controller offset must preserve the working head-orientation mapping,
pose lag, handedness, scale, grip and trim behavior. If the input frame is truly
eye-relative, the matching head-centre term is the full head-to-eye translation
in those axes. The familiar signed half-IPD term is just its ideal lateral case.

Two reasonable sources are a head-centre snapshot bound to the verified render
view, or the midpoint of a *matched* eye-camera pair for diagnostics. A midpoint
across unrelated times folds motion into eye separation and is not sufficient.
Do not wait for the second eye in a hot draw hook merely to compute a midpoint;
use packet replay first and establish a producer-to-render association for live use.

Physical eye separation and placement units also require an explicit mapping.
The camera seam uses its own IPD/scale, while the proposed gate uses
`g_ipdM * g_skcWorldScale * g_mpDriveGain`. Equality is not guaranteed by the
variable names. Different gain/scale settings must not silently change validity.

Keep eye identity for correction lifetime, capture and submission even if placement
no longer branches on an eye sign. `WaPublishCommon` still stamps the inference
and `WaCommonFor` still checks it. A matrix offset does not repair that association
or ensure that all passes of the weapon use the same correction.

## 4. Does this explain the remaining left-eye jump?

A wrong lateral correction remains plausible, but it is not established as the
cause of every flicker. This change does not inherently predict a left-only event.
The affected evaluations must be joined to visible left-eye pixels and the actual
displacement must have the observed direction and magnitude.

The new broken fallback could itself produce a leftward jump in a left-eye draw:
the correct left-eye term is +h*r, while refusal now contributes zero, a -h*r
error. That is a risk introduced by this implementation, not evidence explaining
the older installed build's symptom.

The claim that all historical weapon flickers came from this line also conflicts
with the repository's recorded uncorrected additional render pass and object/
contract issues. See `VR-33-HANDS-AND-WEAPONS.md:272`. A coherent placement and
correction lifetime design can remove a class of faults. It cannot promise to
repair unrelated image transport or object recognition through this subtraction.

### The latest publication instrument did not close view association

The additional sequence check observes publication increments, not whether eye
values alternate or whether the publication belongs to the current draw. Consecutive
publication IDs can carry repeated eyes, zero, or a consistent temporal offset.
The `other == 0` result therefore does not by itself falsify broken eye alternation
or establish correct view association.

The cited counts 19,054 advances and 180,097 re-reads represent about 9.45
*additional* reads per advance, or about 10.45 total evaluations per advance over
that aggregate. Neither is an exact original-draw multiplicity. Keep the result
scoped to what the instrument measured.

## 5. Checks performed during this review

Ten standalone synthetic arithmetic/control-flow checks were executed locally.
They reproduce the proposed equations and branch nesting; they are not a compiled
engine integration test or proof of current shader layout. Reproduction:

```
node build/flicker-root-review-2026-09-09/review-fixtures.js
```

The script and `results.json` are local ignored review artifacts containing only
synthetic data. Results include:

| Case | Observed result |
|---|---|
| Symmetric absolute VP and matching absolute centre | Recovers +3.2 uu correctly |
| Eye-relative VP with an absolute reference | Produces -100 uu; current refusal applies no offset |
| Same eye-relative VP with reference expressed in the same space | Recovers +3.2 uu correctly |
| Stale reference produces the opposite half-IPD | -3.2 accepted; correction reversed |
| Zero candidate on a stereo example | Accepted and applies zero |
| Twice-half-IPD candidate | Accepted |
| Out-of-range or NaN candidate | Refusal does not apply the old offset |
| Small off-centre projection term | Passes current angular tolerance but normalized right contains forward |
| One VP with two possible head centres | Equally valid opposite eye offsets; VP alone cannot choose |

These counterexamples are sufficient to reject the proposed no-regression claim
without spending a headset run. No new binary was installed or game launched.

## 6. Revised plan toward a durable fix

### Phase A: make the experiment safe and independently measurable

Before any installation, restore default OFF for new rendering behavior and add
a separate bounded diagnostic mode. Fix fallback control flow and test that a
rejected candidate produces the exact baseline target, not a partially modified
target. Include invalid camera, nonfinite input, out-of-range result and later
placement failure. Log final source selection after all decisions.

Move any new engine-field definition to `patterns.h` with measured semantics.
Do not continue using `0x80` as a head centre or silently substitute the live
`0x330`. Preserve the successful pose-lag settings and controller sampling.

### Phase B: establish the coordinate contract without moving weapons

Capture bounded packets from qualified hand and weapon color passes, plus any
depth/shadow/material variants that share their palettes. Include shader layout
and operation metadata, unmodified matrices, render target/viewport, draw/view
identity, camera reference snapshots and their source/time, scales, effective
old offset and actual correction consumer. Keep game-derived captures ignored.

Replay stationary, translated, yaw/pitch/roll and mono cases. Determine:

1. Which input origin VP and LocalToWorld use and where eye translation lives.
2. Which camera quantity is the head centre for that view, including positional
   tracking, crouch and eye-write effects.
3. Whether recovered pair separation is correct and the midpoint follows the
   matched centre across motion. Validate all three components, not only right.
4. Whether projected anchors under the proposed target reproduce known-good
   geometry for each eye and pass, with unchanged scale/depth.

Use wrong-origin, stale-reference, wrong-eye, transposed-matrix and altered-lens
positive controls on diagnostic copies. A re-projection residual tests arithmetic
consistency; it cannot independently prove that a guessed origin/reference is
the one the renderer used. Compare with observed draw geometry and controlled
simulator images as well.

Do not assume failure to measure 100% half-IPD proves only bad matrix recovery.
It can expose a different origin, incorrect centre, time mismatch, scale mismatch,
intentional mono or another pass. Preserve reason-specific counts and unresolved
coverage instead of collapsing them into one refusal count.

### Phase C: replace the coordinate bridge, then retire its guess

Implement the verified head-centre-to-draw transform as one coherent draw-context
value. All hand ranges and corresponding weapon passes consume that value and
carry its render-view and correction identity. Retain intended versus observed
provenance for validation, rather than asking each consumer to re-read live state.

Apply it experimentally only on the validated pass population. Known mono and
unknown stereo must remain distinct. For unsupported cases initially preserve
the exact baseline behavior and count them; do not globally skip weapon draws
or silently call every small residual mono. Explicitly measure transitions
between the new path and fallback so a mixed implementation cannot hide flicker.

Once coverage and visual reversals establish the replacement, remove the old eye
guess from placement for supported views. Keep identity checks and explicit
unsupported-path handling. This retires an error-prone decision without replacing
it with a differently named unverified global.

### Phase D: test the actual outcome

First validate synthetic fixtures and rendered simulator cases, including both
eyes, viewmodel/world lens differences, multiple weapon passes, extra Presents,
mono transitions and stale/missing records. Test the production helper, not only
a separately implemented mathematical model.

Then provide a real reversing A/B/A at unchanged resolution, refresh, pose lags
and equipment. Require fewer observed left-eye jumps, no new right-eye jump,
correct size/depth, consistent hand/weapon attachment, and no regression in
head-turn judder or transitions. Join candidate failures to final images. A
better agreement percentage or a nominal half-IPD average is insufficient.

## 7. Answers to the proposal's questions

1. **Matrix recovery?** Valid under stated perspective/storage/coordinate
   assumptions. Not established for this viewmodel input space. Asymmetry changes
   the recovered basis; a different FOV alone does not.
2. **Is `0x80` the head centre?** Existing measured notes say it is not the
   world camera position. It must not be used as the required reference.
3. **Gate and sign agreement?** The current gate is not a near-half-IPD test
   and accepts wrong-sign answers. Validate space, time, geometry and pass identity;
   do not use the old decision as the truth that the new result must obey.
4. **Remove the old path?** After replacement is verified for its supported
   population. For diagnosis preserve it exactly, including on refusal; the built
   code currently fails that requirement.
5. **Left-eye-only symptom?** Plausible only with event-to-eye evidence. Neither
   the matrix formula nor its magnitude check establishes that evidence.

The next deliverable should be the corrected diagnostic implementation and its
packet/simulator validation, not a headset test of the current default-ON build.
