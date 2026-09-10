# Shared world/weapon jitter: revised investigation plan (VR-69)

2026-09-09. Reviewed against source at `2065cfd3` and the latest available run.
This replaces the earlier camera-only proposal. No new rendering behavior is
authorized by this plan review, and no game launch or installation was performed.

## 1. Objective and decision

Find and remove the fault that produces the reported synchronized world jitter
and left-eye weapon flicker, while preserving correct weapon size/depth,
attachment, controller motion and the confirmed pose-lag fixes.

Prioritize the shared rendering and presentation path. Keep palette placement
behavior unchanged during this investigation. That is experimental discipline,
not a finding that the palette or its state handling has been exonerated.

The new perceptual observation is a useful reason to broaden the investigation.
It does not locate the fault exclusively in the camera. A shared disturbance can
originate in camera/view construction, image capture/pairing, submission metadata,
presentation timing, or downstream reprojection. A shared-state or scheduling
side effect from a mesh hook also remains possible.

The Phase B zero-change result does not prove that the draw matrices lack eye
information. Its implementation can compare a context with itself and excludes
some changing transforms from its pair population. Repair the interpretation
before deriving another architecture change from it.

## 2. What the Phase B counter actually measured

The run reports:

```
VP only 0, L2W only 0, BOTH 0, NEITHER 105816, of 105816 pairs
```

These are **selected placement-evaluation comparisons**, not independently
identified left/right view pairs. Four implementation details invalidate the
stronger interpretation.

### Same-context comparisons can be constructed inside one draw

`mesh_split.cpp:3121` acquires one `MpDrawCtx` before the hand/range loop.
`MpWorldTarget(&ctx, ...)` is called within that loop at approximately line 3160,
and the Phase B probe runs inside that function.

The current log explicitly reports two ranges. When both evaluations reach the
probe, it can store the context for one range and compare the exact same context
for the other. No second original draw, duplicate material pass or opposite eye
is required. The probe then clears its saved sample, ready to repeat this on the
next original draw. Eye-dependent changes between original draws can go completely
unexamined.

This is stronger than the draft's duplicate-pass caveat: duplicate passes have
not been established as the explanation of the counter either.

### The matching key depends on the quantity being tested

The key is the quantized LocalToWorld translation, with 0.05-unit bins. An eye
translation that changes those bins makes the samples fail the match and replaces
the saved candidate. The probe therefore selects against detecting precisely the
LocalToWorld translation change it is supposed to find.

A translation is also not an object identity. Distinct objects can share it,
while the same object can move. There is no pair ID, original-draw ID, validated
eye, pass identity, or same-tick check in this comparison.

### Equal scalar signatures do not prove equal matrices

Each matrix is reduced to `sum(matrix[i] * (i + 1))`. Different matrices can
produce the same sum: adding 2 to element 0 and subtracting 1 from element 1
cancels exactly. Float rounding and a fixed threshold introduce further loss.

Consequently, even the statement that all sixteen elements were identical is
stronger than the instrument establishes. Compare the actual elements with
declared tolerances and report the largest difference and its location.

### The printed displacement is not an all-sample maximum

The +0.00 recovered-camera and object-position differences are the current
comparison on a rate-limited line. No min/max distribution for those differences
establishes that every comparison had zero displacement. The last 105,816 total
was printed after gameplay had entered the menu.

The defensible conclusion is: **the selected comparisons did not change either
weighted matrix signature beyond the probe's threshold**. Neither absence of
per-eye information nor absence of stereo drawing follows.

The unsafe matrix-offset implementation remains retired for the reasons in
[FLICKER_ROOT_REVIEW.md](FLICKER_ROOT_REVIEW.md). This result does not establish
a general impossibility theorem about using draw matrices. Do not restart that
behavioral experiment as part of this shared-jitter investigation.

### Local checks of the probe

Four synthetic checks were run without launching the game:

* Alternating eye matrices separated by 6.3 units, evaluated twice per original
  context, produce 100 comparisons and 100 NEITHER results.
* Sampling each such view only once produces zero matched pairs because the
  translation key changes.
* Different matrices with cancelling weighted sums produce NEITHER.
* Distinct objects at the same position can be paired.

Reproduction: `node build/view-jitter-review-2026-09-09/probe-fixtures.js`.
The script and results are ignored local artifacts with synthetic inputs only.
They demonstrate limitations of the probe, not the cause of headset flicker.

## 3. What the existing run already answers

The preserved log is
`build/view-jitter-review-2026-09-09/vr69-view-original.log`, 1,679,625 bytes,
SHA-256 `5DF20EA6A9EC7F93A4189B6704D4383EC3B64231F19AC24966364C342FA4568A`.
It identifies `vr33-hands-working-99-g474fb4fa-dirty`, built at 19:43:59.
The dirty tag means an exact correspondence to the reviewed commit is not proven.

Gameplay is logged from 38937078 through 39069312, **132.234 seconds**. Keep
startup, that interval and the subsequent menu separate.

| Existing evidence | Supported conclusion | Limitation |
|---|---|---|
| Every printed `p2write refused` lifetime total is zero; no refusal warning | The explicitly refused second-eye write is not supported as this run's recurring cause | A successful write does not prove the intended view consumed it |
| Stall-skip counter rises from 0 near gameplay entry to 158 at 39069109 | The game-side guard repeatedly chose a single draw because no Present had advanced since the preceding draw | This is a liveness condition, not a measurement of a long CPU/GPU stall |
| No-frame held-layer counter reaches 158 at 39068968; black remains zero | The runtime repeatedly reused a saved layer instead of assembling a new textured layer | These are not 158 proven visible flickers or 158 same-eye holds |
| Normal stereo summaries show healthy ages and zero aborts/stale-eye events in settled play | The normal fresh stereo path does not show the previously named stale-eye mechanism | Held-layer submissions do not pass through all those same counters |
| Positional tracking owner is `camera (auto)` | The camera lane is the active positional route | Confirm final runtime state and any overrides in a new diagnostic |
| Runtime period reports 11.11 ms with no reported changes | This run reports a 90 Hz application period | Do not describe it as the earlier 80 Hz run or use it as an SSW-active flag |
| `[Pace] Lag=2`, `[Hands] PoseLag=2 PoseLagAb=0` | Both successful lag choices are retained | Preserve them throughout the new test |

The last two stall/hold totals are consistent with the source path:

```
no Present progress at game-side gate
 -> single gameplay draw / zero tag
 -> untagged delivery
 -> HoldUntagged
 -> no texture handed to on_present_end
 -> saved layer resubmitted
```

This is a concrete lead, approximately 1.2 counter increments per second over the
gameplay interval. Equal totals and a similar symptom frequency do not prove
one-to-one correspondence or causation. Establish that correspondence by view
and image IDs, then correlate it with visible events. Do not disable the guard
or HoldUntagged before checking why it fires and what its output contains.

### Corrections to the camera candidate list

* The measured field `camera+0x330` holds camera position; c5 is its negation
  under the documented world-pass convention. The draft inverted both again.
  Preserve the later correction in ENGINE_NOTES, not the superseded sign table.
* `camera::render_pos` exposes the latest recorded c5 sample. It is not an
  identity-bound description of an arbitrary draw, nor a complete view transform.
  Position alone cannot detect rotation, FOV or viewport changes.
* The relevant hook is `core/framework/vs_const_hook.cpp`, and the active writer
  is `camera::apply_offsets`. The refusal counter is owned by scene_draw.cpp.
* The positional LeanVP arm requires the VP lane. With the observed camera owner,
  it is not the first explanation to pursue. Check its actual execution count
  before treating it as active. A separate legacy head-matrix arm has different
  conditions; do not infer its state from an unrelated heartbeat's inject label.
* Repeated camera writes do not automatically accumulate another IPD:
  `current_base` removes the previous offset when the field still equals the
  last write, then `write_offset` applies base plus the new offset. A double-offset
  hypothesis must show incorrect base detection or an intervening writer.
* The camera log uses 108 units/metre and half-IPD about 3.41 units, while the
  placement probe uses half-IPD about 3.15. Use the relevant coordinate scale
  when checking camera separation; the difference is not itself a flicker cause.

## 4. Interpret the world/weapon observation without over-localizing it

Treat the reported simultaneous jitter as evidence worth testing now. An
uninstrumented repeat run would add confidence in the perception but would not
identify the failing stage, so it need not be a prerequisite for preparing a
better diagnostic.

A correctly scoped palette transform changes only the affected mesh. That does
not imply that all effects of its hook are mesh-local: shader constants, viewport,
bindings and GPU time are shared resources. The code itself restores palette
constants because subsequent draws would otherwise inherit them. No state leak
has been demonstrated here; exclude the absolute claim, not the subsystem.

Similarly, both world and weapons are present in the final eye image. Image reuse,
incorrect image/pose association, an eye-pair error or a presentation disturbance
can affect both after the camera and palette have finished.

The submitted projection view describes the camera pose and FOV used for an eye
image; these therefore belong in the audit alongside the rendered view.
[OpenXR projection-view definition](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerProjectionView.html).

Saving a layer description also does not preserve a historical pixel pair if a
referenced swapchain has since released a newer image. OpenXR uses each
swapchain's latest released image. Include the actual content IDs in held-layer
diagnostics.
[OpenXR rendering specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html#rendering).

Neither this observation nor a clean camera-state trace establishes a network or
encoder fault. Continue from the first stage at which the image or metadata departs
from the expected result.

## 5. Revised first diagnostic: where does the shared disturbance first appear?

No placement or camera behavior change. Use a bounded record spanning both the
rendered view and the final image submission.

### Independent event detection

Do not define a flicker as an eye sign changing. Correct stereo alternates signs,
and a suspect sign cannot serve as the truth that labels a visible event.

Keep a short rolling record, with an optional user event marker and independent
automatic triggers for view residuals, source-image discontinuities, missing/
repeated image IDs, gate-to-hold transitions and image/pose mismatches. Label these
as candidate events, not confirmed flickers. Retain normal control intervals and
events where the suspect eye decision agrees.

For visual confirmation, track several stationary world features and weapon/
hand anchors in each eye separately. Compare successive samples of the **same
validated eye**, accounting for expected head and game-camera motion. Distinguish
translation, rotation, disappearance/duplication, and an entire held image. A
single anchor can be occluded or animated; retain confidence and ambiguity.

Use source-eye images before capture processing and the submitted eye images
where practical. A pinned desktop mirror does not represent both eyes or prove
what the headset displayed. If source/submitted images are clean during a reported
event, examine runtime timing and headset output rather than dismissing the report.
Measure capture overhead and avoid making recording itself the new hitch source.

### Record the chain, not a bag of latest globals

For the same candidate event, retain:

1. Game decision/pair ID; requested eye; camera base, requested offsets, clamp
   result, successful write value and time; explicit pass-2 refusal reason.
2. Original render-draw/view identity, pass category, observed position,
   orientation/projection, FOV and viewport; actual post-hook constants. Record
   the identity and age of each contributing upload, including partial uploads.
3. Effective hand/weapon correction and object identity as observers only, plus
   relevant state before and after the hook when testing a suspected leak.
4. Capture serial, slot generation, delivered eye/record, method tag arbitration,
   freshness and hold reason. Distinguish a reusable slot from the image in it.
5. Per-eye last successful copy/release content IDs, submitted pose/FOV records,
   pair-open state, layer type, predicted display time and actual submission
   timing. Include fresh, held, mono and keepalive paths in the population.

The existing world VP recorder observes uploads before the later LeanVP/head
patch branch. Do not assume that pre-hook record equals the constants ultimately
consumed by the draw. Bind observations to a qualified pass and final state;
a latest c5/VP lookup can otherwise recreate the same association error.

Reuse existing pose, pair and capture records where possible. Do not introduce
another global current-eye opinion. Emit reason counts and coverage, with bounded
ring dumps, cooldown and dropped-record counts. No per-draw synchronous disk I/O
or unbounded GPU readback. Ordinary launches should provide useful summaries
without requiring the tester to operate the command harness.

### First-stage decision table

| Earliest independently verified anomaly | Next investigation |
|---|---|
| Wrong camera/view reaches both world and weapon source draws | Camera write ownership, base/clamp, view association or executed projection patch |
| World is stable before mesh work but changes after shared-state mutation | Hook state restoration or scope, with observed state/output evidence |
| Source views are correct; capture/output uses stale or wrong content | Capture fences, slot lifetime, delivery and eye pairing |
| Images are correct; final poses/FOV or per-eye content IDs do not match | Submission association, held-layer and pair handling |
| Complete pairs are valid but held/new-frame cadence coincides with the symptom | Game/render scheduling and runtime presentation timing |
| Only the weapon source image changes | Placement, animation or object/pass correction remains open |
| Measured source/submission chain is clean while headset output jitters | Downstream timing/reprojection/display, or missing coverage in the trace |
| Camera counters are unchanged but no visual/stage evidence was captured | Inconclusive; not proof of coincidence |

A c5 discontinuity is a trigger, not a verdict that the camera writer caused it.
Expected IPD alternation, ordinary movement, a misidentified sample and a genuine
wrong write can all change it. Likewise, a zero c5 delta does not imply zero
rotation or unchanged presentation.

## 6. Verification before a headset run

Validate the observer in synthetic fixtures and the existing simulator first.
No user headset run is needed to discover another self-comparison bug.

Required controls:

* One original context evaluated for two hand ranges must produce one original
  draw observation, not a purported eye pair.
* Distinct views with known eye separation must be compared even when LocalToWorld
  translation changes; pairing uses object/view identity, not the tested value.
* Elementwise comparisons must detect cancelling-signature matrix changes.
* A correct eye alternation must not count as flicker. Unknown eye and missing
  records must remain explicit rather than counted as agreement.
* Diagnostic copies with wrong eye, old pose, altered FOV or wrong capture serial
  must trip the relevant comparison. A valid old image with its matching old
  pose must be classified separately from an image/pose mismatch.
* Exercise normal pairs, between-pair holds, and a hold after one eye has already
  been released. Inspect simulated images and verify latest-release semantics.
* Check camera-base handling for repeated writes and intervening engine writes.
  An unchanged repeated write should not be labelled a doubled offset.
* Validate every final submission path, not only fresh stereo layers; report
  unmatched events and observer overhead.

These tests establish instrument sensitivity and scope. They do not establish
the human-visible cause without event correlation.

## 7. Headset experiment and conditional fix

Keep the current selected resolution, refresh and SSW mode fixed and record
their resolved values. This reviewed log reports a 90 Hz period, unlike earlier
80 Hz runs; do not silently combine their budgets. Preserve `[Pace] Lag=2`,
`[Hands] PoseLag=2`, and disabled automatic lag A/B.

Use a familiar scene with nearby stationary geometry and both weapons visible.
Separate stationary, head rotation, head translation, controller movement and
stick movement intervals. Keep loading and equipment changes in separate windows.
Determine whether the world event is also left-only and whether the hand, weapon
and nearby geometry move together.

First correlate the observed event with the recorded gate/hold and camera-to-
submission chain. Change only the boundary shown to fail, behind a default-OFF
live lever. Do not disable the present-progress guard or untagged hold merely
because their counters correlate; those safeguards prevent other known failures.

A fix is accepted when the visible disturbance and its measured cause improve
together and return in a reversing comparison, without losing weapon size/depth,
attachment, tracking, stereo world stability or load-transition behavior.
A clean counter without visual improvement is not completion.

## 8. Answers to the review questions

1. **Weight of the shared-jitter report:** sufficient to prioritize shared rendering
   and presentation, insufficient to declare a camera-only cause.
2. **Candidate 3:** already checked. The named pass-2 write-refusal counter is zero
   in this run. Wrong-view consumption after a successful write remains untested.
3. **Detector:** independent same-eye visual evidence plus a multi-trigger bounded
   trace; never use the eye decision alone to define the symptom.
4. **Does Phase B generalize?** No. Same-context comparisons, selection by translation,
   scalar collisions and missing view identity prevent either proposed conclusion.
5. **Existing exclusions and priorities:** positional LeanVP is deprioritized by
   the resolved camera lane; explicit refused second-eye writes are unsupported.
   Present-progress skips and held-layer resubmissions are active and belong near
   the front of the investigation, alongside correctly associated view observations.

The next deliverable is a verified observer of the existing paths and one
correlated event, followed by a cause-specific correction. The plan does not
require another speculative per-eye placement change.
