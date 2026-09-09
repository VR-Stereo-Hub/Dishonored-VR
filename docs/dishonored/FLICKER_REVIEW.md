# Residual weapon flicker: revised investigation (VR-69)

2026-09-09. Review of `FLICKER_PLAN.md` against source at `cee51d35` and the
latest available headset log. This is a diagnostic plan, not an implemented fix.
Head-turn judder is reported resolved; preserve both working pose-lag settings.

## 1. Recommendation

Extend the existing identity records incrementally. First measure the association
between a weapon draw and the eye value it actually consumes. Do not replace the
fallback, reverse the eye sign again, or undertake a whole-pipeline rewrite yet.

The draft identifies a useful class of failures: metadata can describe a different
view, time, or object from the pixels it controls. But it overstates both the
architecture argument and the evidence for the remaining flicker:

* The lifetime left/right disagreement imbalance is predominantly a startup/load
  transition effect. In a stable gameplay interval it is almost symmetric.
* Most of the quoted unknown samples accumulated before that interval. Unknowns
  still occur during play, but the counters do not establish visible bad draws.
* The new eye channel is a latest-value publication from Present. It has no view
  ID, producer Present ID, timestamp, or validity epoch. A coherent read does not
  establish that its eye belongs to the draw reading it.
* The applied eye and the eye recorded on the shared weapon correction can differ.
  The correction still stores the old inference, not the effective eye.
* The old same-eye-repeat explanation is weakened for recurring steady gameplay.
  That does not clear every image-reuse or partial-pair path.

These are reasons to instrument specific boundaries, not proof that any one is
the observed cause. Preserve the successful visual improvements while doing so.

## 2. Evidence and corrected populations

The reviewed log identifies build `vr33-hands-working-91-ge5122eed-dirty`, built
2026-09-09 at 16:00:09. Its per-eye counters match the supplied draft. The dirty
build tag does not establish an exact source-to-binary match with `cee51d35`.
The raw log was preserved locally at
`build/flicker-review-2026-09-09/vr69-original.log` (ignored, not for publication).
It is 1,162,541 bytes; SHA-256:
`F0BE27DBF08E4615C6AF4006C0CFC6E995BE2E69AA79031831C9F79AC1EB6383`.

The resolved log has `PoseLag=2`, `PoseLagAb=0`, `HoldSameEye=1`, and the eye audit
acting on the measured channel with sign -1. The installed ini retains 2750x2850,
`[Pace] Lag=2`, `PaletteEyeOffset=1`, and `HoldUntagged=3`. Preserve these settings
for the next diagnostic baseline; print all effective eye levers on that build.

### The counters are not frame or flicker counts

`ms/palette/measeye` is emitted from `MpWorldTarget`, called inside the hand/range
loop in `mesh_split.cpp`. It counts placement evaluations that reached the audit,
not unique original draws, visible weapon draws, eye images, or display frames.
Several evaluations can belong to one draw and many to one image. Later placement
or draw work can still fail. Offscreen or intermediate-pass work is not separated.

At timestamp 25846515 the total is **152,751 evaluations**: 141,461 agreements,
5,520 disagreements and 5,770 no-answer evaluations. About 140,000 is the agreement
subtotal, not the total population. A disagreement also includes a nonzero method
answer against a zero inference, not just two known opposite eyes.

### Remove startup and loading before comparing eyes

The state log enters gameplay at 25750531, loading at 25753500, and gameplay again
at 25755437. The latter interval ends at 25847671. Use the counter snapshots at
25759500 and 25846515 for an explicitly trimmed, **87.015-second** gameplay
comparison. This starts about four seconds after the second gameplay entry and
ends before the menu. It is a reproducible analysis interval, not a claim that
all transitions have a universal four-second settling time.

| Counter | Whole run through 25846515 | Delta from 25759500 to 25846515 |
|---|---:|---:|
| Agree, channel-labelled left | 70,871 | 68,251 |
| Agree, channel-labelled right | 70,590 | 68,297 |
| Disagree, channel-labelled left | 3,350 | 2,160 |
| Disagree, channel-labelled right | 2,170 | 2,120 |
| Method read yielded no eye | 5,770 | 2,120 |
| All evaluations | 152,751 | 142,948 |

The trimmed disagreement rates are **3.068% left and 3.011% right**, with a count
ratio of **1.019**, rather than 1.54. The excess left count falls from 1,180 to 40.
Already at 25750484, before the first gameplay entry, the no-answer counter was
3,591. The trimmed no-answer share is **1.483%**, not approximately 4%.

These are descriptive evaluation rates. Repeated evaluations are correlated and
must not be treated as independent statistical trials. Even exact symmetry would
not eliminate the eye-decision path: event clustering, which pass becomes visible,
and the size of the displacement can differ. The log's sentence claiming symmetry
would exclude that path should be removed from the next instrument.

## 3. The most important source-level gaps

### A. A measured Present eye is not necessarily this draw's eye

`reentry.cpp:349-358` publishes `g_msMeasEye` after reconciling the tag ring with
c5. This occurs in `end_frame`, after the image being presented was drawn.
`mesh_split.cpp:2325-2365` reads that publication during later placement work.
There is no association with the consumer's render-view ID.

In a regular alternating stream, a previous eye has the opposite sign to the
next eye. Therefore near-perfect agreement after negation has at least two
possible explanations: a coordinate convention difference, or a temporal offset
between producer and consumer. Both can coexist. The public declarations and
`scene_draw.cpp` explicitly describe pass 1 as left/-1 and pass 2 as right/+1;
the claim that pass labels were inherently unrelated to left/right is not
established by the counter.

Keep the currently successful sign. Disambiguate it using independently observed
view geometry and producer/consumer identities, including mono transitions,
repeats, extra Presents, and delayed rendering. Perfect alternation alone cannot
distinguish the two explanations.

The publication has additional validity limitations:

* It happens only in the `tagged` branch. A successful zero tag publishes zero;
  an unsuccessful tag pop leaves the previous publication unchanged.
* The reader maps an odd/changing sequence and a stable published zero to the
  same `meas == 0` result. These need different reason counters.
* No reset/shutdown invalidation of this channel appears in the current source.
  A nonzero value can remain available across a gap without proving relevance.
* The audit reads the channel separately for each placement evaluation, although
  the original draw context is acquired once. Freeze the chosen eye and its
  provenance with that context after validating the intended lifetime.

This is an association problem even if the publication mechanism always returns
a coherent value. Do not spend the next run testing only for torn reads.

### B. Unknown does not imply an incorrectly drawn stereo weapon

The zero `no answer L/R` totals support the narrow conclusion that the inference
was also zero on those no-answer evaluations, subject to the counters' read
consistency. They do not name the true eye or the output image.

`MpEyeForPresent` deliberately sets the inference to zero when
`g_sdDoublingNow == 0`, clears its history, and returns. It also returns zero on
the first comparison or an oversized motion step. `g_sdDoublingNow` is itself a
game-side decision read by rendering; distinguish intended mono from an old/new
decision that does not belong to the queued draw.

The draft's suggestion to classify these samples using the method's tag needs an
explicit join. The same latest-value method channel just returned no answer.
Reading it again later can classify a different view. Instead, record the unknown
evaluation's draw/view identity and retrospectively join it to capture and
submission, preserving unresolved cases as unresolved.

Record the *final* effective eye after both measured-source and optional pass
overrides. The current audit prints before the `PaletteEyeFromPass` override.
Distinguish a disabled eye-offset lever, intentional mono, unknown stereo, invalid
pose, later placement refusal, engine fallback, successful patch, and a patch
whose pixels never reach a displayed image.

### C. Weapon correction metadata still names the inference

`MpWorldTarget` applies `eyeUse`, which can be the signed method answer, then
publishes the resulting transform through `WaPublishCommon`.
`weapon_attach.cpp:280` stores `w.eye = g_mpEyeState`, and `WaCommonFor` compares
that field to the same inference. Thus an effective left-eye correction can carry
right-eye or unknown metadata. The current checks cannot verify the effective-eye
association by comparing two copies of the inference.

This does not prove that the wrong correction was consumed: within a correct
view the label mismatch could be harmless. It does mean that the freshness guard
and its audit do not prove what their comments claim. Trace the effective eye,
render-view ID, hand correction ID and consuming weapon draw together. Also record
which weapon matcher/fallback actually ran; the palette audit is not a census of
all weapon paths.

## 4. Geometry: a missing offset can cause the reported direction

For the current placement term, let e be the correct eye (-1 left, +1 right),
h the half-IPD in placement units, and r the measured camera-right vector.
The eye contribution is `-e*h*r`. Omitting it produces an error relative to the
correct result of `+e*h*r`. Under the expected screen-right mapping that is a
leftward error for the left eye and a rightward error for the right eye. Choosing
the opposite eye doubles the magnitude.

There is no requirement that an unknown evaluation affect both eye images.
If unknowns reach visible left-eye color draws only, missing offsets can produce
a left-only symptom without any coincidence with a correct right-eye position.
Conversely, if equally visible errors occur in both eyes, the report needs another
explanation. The current zero-labelled population cannot distinguish these cases.

The half-IPD is only the omitted target term. It is not yet a measured final
weapon displacement after shared correction, animation, viewmodel projection,
depth, and clipping. A jump that remains screen-left with the weapon upside down
supports a view-related displacement but does not distinguish this from image
reuse or other camera-relative placement errors.

For each affected visible pass, project a stable mesh anchor under three
diagnostic-only transforms: the actual result, zero eye term, and the independently
identified correct eye term. Keep all other inputs identical. Record screen x/y,
depth and clipping status. Use the executed shader's matrix convention and actual
viewport, including the viewmodel lens. Direct3D projection and viewport mapping
determine the pixel displacement; a world-space offset alone does not.
[Microsoft: viewports and clipping](https://learn.microsoft.com/en-us/windows/win32/direct3d9/viewports-and-clipping).

## 5. What the earlier negative experiments actually close

The earlier palette-source/sign change is a successful mitigation of the large
flicker and double image, based on the reported visual result. Its remaining
association hazards are still open. Agreement between two derived signals does
not prove both identify the actual draw correctly.

The draft reports an earlier run with zero same-eye repeats and holds. The latest
matching log is not such a whole-run zero: it reports 49 repeats by 25756578 and
16 holds at that timestamp, clustered around startup/loading. Those observations
do not explain persistent flicker later. Steady gameplay summaries report zero
pair aborts and stale-eye events, which is evidence against the named repeat
mechanism in that population. Keep runs and populations separate.

### A bounded additional check: held layers versus held image contents

The log still has frequent no-frame resubmissions, reaching 250 by 25846656 and
253 by menu entry. These are not 250 same-eye holds and are not automatically
250 visible flickers. They include the untagged-hold path and potentially other
no-texture results. Their exact relationship to the symptom remains unmeasured.

There is a specific limitation in the claim that resubmitting the old layer
necessarily restores the previous complete pair:

1. `openxr_runtime.cpp:3977` releases an eye image before the left-eye pair-hold
   return at approximately 4090.
2. The no-frame fallback at approximately 4540 copies saved layer descriptions
   and poses. It does not restore old eye pixels.
3. OpenXR uses the latest released image from each referenced swapchain. Saving
   the layer description does not select an older acquired-image index.
   [OpenXR rendering specification](https://registry.khronos.org/OpenXR/specs/1.0-khr/html/xrspec.html#rendering).

Consequently, the synthetic sequence complete pair A, release left B, then
no-frame resubmission is worth testing: it could combine updated left pixels
with old right pixels and saved metadata. This is a code-path counterexample to
an unconditional claim, not an attribution of the observed flicker. The clean
steady-gameplay abort counters weaken its occurrence in this run.

The normal stereo age audit lives in fresh projection assembly; the later
no-frame fallback does not execute it. Move diagnostic coverage to every final
projection submission, including held layers and keepalives, recording the actual
last successfully released content ID of each swapchain beside the submitted pose
record. Check release results, not just attempted copies. The existing sampled
64x64 frame trace also cannot exclude brief peripheral weapon-only changes.

Do not disable `HoldUntagged` as the first experiment: that substitutes mono or
other previously mitigated behavior and would confound the test.

## 6. Architecture decision: extend what already exists

The original engine is monoscopic, but this method renders the viewport twice
with different camera positions. It is not reconstructing every stereo fact
from one finished mono image. Several required identities already exist:

| Boundary | Existing identity or observation | Missing guarantee |
|---|---|---|
| Camera/view request | `pose::Record.id`, `pairId`, eye, copied track/camera | Correspondence to the view actually rendered |
| Stereo tag queue | Eye, camera position and pose-record ID | Survival through extra/missing Presents and reconciliation |
| Capture | Serial, delivered serial/tag/record, slot | Join from the weapon draw to those pixels |
| Hand/weapon correction | Present, pose generation, component generation, inferred eye | Effective eye and exact render-view ownership |
| XR output | Per-eye images, poses and capture-age counters | Complete-pair content provenance on every submission path |

The proposed identity direction is worthwhile, but a single mutable global frame
record would reproduce the same fault. Use small immutable records with explicit
relationships: simulation/pair ID, render-view ID, per-draw correction ID, capture
serial, and submission ID. Keep eye image identity separate from reusable capture
slot or swapchain buffer index. Include session/reset epoch and explicit missing,
expired, mono, and unvalidated states.

Weapon identity belongs to an object/component and draw contract, which can vary
within one view. Do not put one weapon contract on a whole frame. Existing pose
generation and Present counters also have different meanings and cannot be
interchanged just because both are integers.

Start with one missing edge: source of eye selection to hand correction to weapon
draw to captured view. Carry intended identity through the engine's queued work
at a verified boundary, then validate it against render-side camera/projection
observations. A game-thread eye flag or callback scope is not automatically a
render-thread view identity. A new ID makes correspondence testable; it does not
turn an unverified association into truth.

## 7. Revised implementation and test order

### Build 1: bounded diagnostics, no rendering behavior change

Preserve the working judder fixes, resolution, refresh rate and eye levers. Log
the resolved configuration once, including every A/B toggle. Enable bounded
summary diagnostics for ordinary launches, with no command-seam requirement.

Add a small preallocated event ring with correlated records for:

* State/epoch, thread, original draw ID, pass category, view candidate and its
  validation result, render target/viewport and observed camera constants.
* Producer Present/sequence/time and raw eye value; read validity reason;
  inference and reason; final selected eye/source; actual applied offset.
* Hand correction ID, effective eye, component revision, consuming weapon
  contract/path, successful patch versus fallback/refusal, and projected anchor.
* Capture serial/slot/pose record, delivered image identity, and each XR
  swapchain's last successful release identity and submitted pose identity.

Separate unique draws, placement evaluations, affected eye images, completed
pairs, and final submissions. Summarize window deltas by gameplay/transition,
physical eye where validated, hand/object and pass. Print unresolved coverage.
The disagreement matrix must distinguish opposite known eyes from inference zero.

Trigger a bounded dump on effective-eye inconsistency within a validated view,
unknown stereo placement, correction-consumer identity mismatch, or held-layer
content/pose mismatch. Include surrounding normal samples and use cooldown and
per-session caps. Preserve an optional user event marker, but automatically retain
events and summaries so an ordinary launch is informative. Do not perform GPU
readback or synchronous disk logging on every draw.

### Validate the instruments before another headset run

Use unit/replay fixtures for the association logic, then the existing simulator
for rendered behavior. Cover regular alternation, identical pose with different
eye, mono transitions, skipped/repeated/extra Presents, delayed publication, reset
and expired records. Include multiple hand ranges and weapon passes in one view.

Positive controls must deliberately substitute the wrong view, previous eye,
zero eye term, or stale correction in a diagnostic copy and make the relevant
counter move. A correct association must remain accepted when unrelated pose
publications occur. Do not perturb the real headset image for these controls.

For hold-path validation, produce distinguishable left/right images and exercise
a complete pair, a no-frame hold between pairs, and a no-frame result after the
left image has been released. Inspect simulated compositor outputs as well as
metadata. Confirm the simulator implements latest-release semantics before relying
on it as an independent check. Keep captures local and ignored.

### First headset diagnosis

Use one familiar scene with both weapons visible. Separate stationary head and
controllers, head rotation with supported controllers, controller translation,
controller rotation, and stick movement. Keep equipment changes and loading in
separate windows. Record whether hands, weapons and nearby world geometry move
together, and distinguish an actual displacement from disappearance/duplication.

Join observed events to affected final images. A candidate is supported when its
measured error reaches the reported eye and predicts direction/magnitude at the
event; it is weakened when the event occurs with valid placement and transport.
Report missed events and diagnostic coverage. Similar rates alone are insufficient.

### Build 2: one cause-specific behavior change

Choose only after Build 1 establishes the failing boundary:

| Observed failure | Conditional change |
|---|---|
| Previous/unrelated Present eye consumed by a draw | Read the verified render-view eye once into the draw context |
| Correction labelled or reused across the wrong view | Carry effective eye and view ID through publication and consumption |
| Unknown applies only to an auxiliary/mono pass | Correct classification/scope; do not invent a stereo eye |
| Visible stereo placement lacks eye identity | Repair that identity's propagation before choosing a fallback |
| Partial update reaches held submission | Preserve a complete image pair and matching metadata with explicit ownership |
| Contract/component fallback changes the visible weapon | Fix the executed matcher or correction-lifetime boundary |

Any new behavior lever defaults off and supports a live reversing comparison.
Preserving complete pairs may require staging resources; do not add them before
the trace shows the need and their latency/cost is assessed.

## 8. Direct answers and acceptance

1. **Unknowns or disagreement lean first?** Instrument effective-eye association
   and unknown reasons together. The claimed large persistent left lean does not
   survive the trimmed comparison. Unknowns remain relevant but are not 5,770
   proven faulty visible draws.
2. **Can zero offset explain one eye?** Yes, if affected visible draws belong to
   that eye. The leftward sign is plausible. Distribution and projected magnitude
   must be measured; coincidence with the right-eye baseline is unnecessary.
3. **What fallback?** Keep behavior unchanged for diagnosis. Zero is appropriate
   for intentional mono. Blindly holding an eye can mislabel the next stereo view;
   skipping can remove the weapon or necessary depth. Neither is a general fix.
4. **Incremental fixes or identity?** Incrementally extend existing identity and
   provenance at the failing boundaries. This avoids both repeated heuristics and
   an unmeasured rewrite.
5. **What is closed?** Preserve the confirmed judder and large-flicker gains.
   Recurring same-eye pushes are not supported in the stable interval, but the
   general freshness claim remains broader than the instrument's coverage.

Accept a fix after repeatable affected-eye visual improvement and an A/B/A return
of the specific measured defect, with world/weapon judder, stereo placement,
controller tracking, weapon switching and load transitions intact. Require final
image association coverage, not only improved agreement percentages. The next
deliverable is the diagnostic build and its simulator evidence; no new headset
test, binary, or runtime setting was applied during this review.
