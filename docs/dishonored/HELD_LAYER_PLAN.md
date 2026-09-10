# Held images and stereo identity: reviewed plan (VR-69)

2026-09-09. Reviewed against source at `fad8aff8` and the preserved observer run.
**Decision: proceed with a revised observer, not a rendering fix yet.** This is
a documentation review. No binary, configuration, installation or game launch
was changed.

## 1. Objective and verdict

Remove the shared fault behind synchronized world, weapon and hand flicker,
while preserving confirmed depth, scale and pose-alignment improvements.
The architectural target is a complete rendered stereo pair whose identity and
rendering metadata survive capture, release, storage and submission together.
This is a useful correctness requirement even if several faults contribute.
A universal single cause is not established.

The OpenXR mechanism is valid: saving a layer description does not retain its
historical pixels. Each referenced swapchain resolves to its last released image
at `xrEndFrame`. The saved subimage contains a swapchain handle, rectangle and
array layer, not a selectable historical acquired-image index. See the official
[xrEndFrame reference](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrEndFrame.html),
[projection-view definition](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrCompositionLayerProjectionView.html)
and [subimage definition](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSwapchainSubImage.html).

Three draft claims need correction:

1. Matching source-pair IDs do not exonerate a hold: both images can advance
   together while saved poses still describe an older pair.
2. Split source-pair IDs establish inconsistent provenance if their association
   with pixels is verified. They do not alone establish visible flicker or its
   frequency, and do not prove every flicker shares this cause.
3. The simple partial-left-pair explanation predicts an existing abort counter
   that the available gameplay log does not show. Investigate that contradiction.

## 2. Source evidence and the counterprediction

References are in `src/core/vr/openxr_runtime.cpp` at the reviewed commit.

| Location | Finding |
|---|---|
| 3946-3977 | Acquire, wait, copy and release operate on one eye. The release result is currently ignored. |
| 4087-4100 | The first left-eye capture can release its image, set `g_srPairOpen`, and return without submitting. |
| 3674-3675, 3810-3891 | The next present consumes open-pair state. A non-right completion increments an abort reason, including an untagged completion. |
| 2919-2926 | An expired open pair also has an abort path. |
| 4540-4568 | A no-layer present can reuse the saved layer description. |
| 4645-4655 | A newly built layer is banked after successful submission. Newly built does not prove coherent stereo pixels. |
| 789-798, 1087-1111 | `FeedSnap` stores descriptions and poses, not pixels; the detached feed path submits a copied snapshot. |

A partially updated swapchain set therefore exists between left and right
processing. Whether a held submission sees it requires the actual event order:

```
submit and bank A_L + A_R with metadata A
release B_L, leaving A_R as the latest right image
submit the saved A description before releasing B_R
```

This resolves to B_L + A_R with A metadata. The unchanged right image can still
match its saved pose. The draft's statement that poses describe neither image
is incorrect. Even B_L need not have a different render pose while stationary;
record actual metadata differences.

With pair pacing active, left release followed directly by an untagged hold
should increment `abortUntagged`. Timeout should increment `abortExpired`.
A completed-pair-then-hold sequence should increment neither.

The preserved latest observer log shows held count 47 at timestamp 41934718 and
100 at 41974500: **53 holds in 39.782 seconds**, about 1.33/s, within gameplay.
Enclosing stereo summaries repeatedly report zero left, untagged and expired
aborts. This argues against treating those recurring holds as the straightforward
left-release-then-untagged sequence. It does not prove every transition is covered
or that the abort instrument is complete.

The earlier 132-second run had 158 progress skips and 158 holds. Keep that
separate from this later run, which reports a 90 Hz display period. Similar
rates prioritize investigation; they do not identify releases consumed by each
hold or establish alignment with a visible event.

Local evidence, not for committing: `build/held-layer-review-2026-09-09/observer-run.log`.
SHA-256: `7F97D3954F2B1532D6FAF861103499B0D17AC3B854B4D4A8A77547F664B70778`.
Build tag: `vr33-hands-working-105-g34f04ffb-dirty`.

## 3. Symptom interpretation

| Observation | Supported inference and limit |
|---|---|
| World, hands and weapons disturb together | Whole-image or shared-view faults are plausible; submission is not uniquely identified. |
| Left eye is more visibly affected | Releasing left first creates a plausible asymmetry. Visibility depends on content, motion and reprojection. |
| Similar direction with an inverted weapon | Fits a shared-image fault, but also a camera-space error independent of weapon orientation. |
| Approximately 1-2 events/s | Compare defective submissions with events, not all holds. Valid repeated frames can also affect cadence. |

Neither Wi-Fi nor encoder causation follows from these observations. A downstream
runtime effect remains an alternative if submitted content, metadata and timing
prove correct. Do not assign blame before that boundary is measured.

## 4. Identity and independent verdicts

Keep three identities separate:

- Rendered content: original stereo pair ID, eye/view ID, capture serial,
  delivered record, and pose/FOV actually used to render it.
- Release event: session/swapchain lifetime, handle, acquired image index,
  array layer, monotonic release serial and result.
- Submission: serial, path/reason, saved-bank serial, display time, referenced
  subimages, submitted pose/FOV and reference-space identity.

Independent left/right release counters cannot establish a common source pair.
Ring indices repeat; handles can be reused after recreation. Carry lifetime
identity too. Stamp provenance at the producer and carry it with the texture.
Joining the left image to the latest global right record at submission would
recreate the association problem inside the observer.

If rendering provenance is inferred, print UNVERIFIED. Track reference space,
FOV, rectangle and relevant origin/recenter generation as well as pose generation.
Different metadata IDs with equal values are not necessarily a geometric error.

| Axis | Outcomes |
|---|---|
| Source stereo coherence | MATCHED, SPLIT, UNKNOWN |
| Change since saved bank | Release-event changes and content changes, separately: neither, left, right, both, unknown |
| Image-to-submitted-metadata agreement | Per eye: match, mismatch, unverified; include numerical pose/FOV differences |

Releasing the same captured content again changes the release event without
necessarily changing pixels or invalidating metadata. Conversely, exact equality
with the bank is insufficient if that bank was already incoherent.

Classify fresh, held and feed projection submissions with the same core. Fresh
submissions are essential controls and may themselves bank the fault. Count
mono/quad and empty submissions separately; neither is a matched stereo control.
Shared classification does not inherently double cost: use bounded records and
aggregate summaries.

## 5. Observer implementation

1. Audit producer -> capture -> release identity and every `xrEndFrame` site.
   Cover normal present, detached `feed_submit_cycle`, and empty-layer cleanup
   and timeout paths. Account for auxiliary HUD/laser layers separately.
2. Maintain a release ledger per swapchain lifetime. Record acquire, wait, copy
   and release outcomes; advance latest-released identity only after successful
   release. Failed or unknown copy provenance stays unknown. A wait timeout is
   nonnegative, so `XR_SUCCEEDED` alone is not a general readiness test. Do not
   propagate the existing comment that failed waits require release. Queued GPU
   copies are not CPU proof of completion; add no fence or readback for this audit.
   The [wait contract](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrWaitSwapchainImage.html)
   explicitly distinguishes a successful wait without timeout from a timeout.
3. Bank provenance with the exact `FeedSnap` description under its existing
   synchronization. Preserve coherence/verification status rather than treating
   fresh as valid. Read description and provenance together. Respect frame-call
   ownership; independent atomics must not create synthetic mixed snapshots.
4. At each submission resolve the actual referenced handles/array layers against
   the ledger. Record timestamps before/after the call, result, game/session
   state, pair-open/abort transitions, bank, verdicts and content age. Separate
   attempts from successful calls; API success does not prove visible display.
5. Retain a bounded bank -> release -> submit event ring with anomaly samples,
   aggregate counts and overflow counts. Avoid per-draw logging and holding a
   snapshot mutex across blocking XR calls.
6. Reconcile totals by path/state. Correlate partial-left holds with expected
   abort reasons. Explain any disagreement with existing counters before accepting
   either result. Unknowns cannot silently enter the MATCHED population.

Keep placement, lag, present-progress guard and `HoldUntagged` unchanged. Record
active settings and build identity at startup. The diagnostic observer may be
on; any subsequent behavioral lever defaults off with a reversible live A/B.

## 6. Validation before a headset run

Test the same classifier used by production and its integration with the actual
pair/bank state machine. A separate model alone does not validate shipped logic.

| Fixture | Expected result |
|---|---|
| Bank A; hold without release | Matched A, unchanged content, matching metadata |
| Bank A; release B left; untagged hold | Split B/A; left changed; metadata mismatch if pose differs; expected untagged abort in ordinary pair pacing |
| Bank A; advance both images to B without replacing bank | Matched B, both changed, detect old A metadata. Adversarial classifier fixture, not an assertion about the common runtime path |
| Bank an already split fresh submission; hold | Exact bank match retains split verdict |
| Release the same A content again | Release changes, content unchanged, metadata can match |
| Equal release serials from different source pairs | Split despite serial equality |
| Healthy pair with unequal release serials | Matched source pair |
| Failed release, unknown copy, wait timeout | No invented known-good release/content |
| Ring wrap, swapchain recreation, session restart | No identity alias; incompatible banks invalidated |
| Mono, empty, detached feed, failed end-frame | Correct populations and bank behavior |

Then exercise available simulator integration: partial updates, normal holds,
transitions and release failures where supported. Report unsupported injection
explicitly. Measure overhead while preserving render settings. No headset is
needed to validate the classifier or demonstrate a deterministic ledger violation.
A representative game run is still needed to establish occurrence here; visible
correlation is needed to attribute the reported flicker.

## 7. Decision gates and conditional fix

- Split content or incorrect image metadata correlates with visible events:
  repair the earliest boundary that loses association. Preserve a complete pixel
  pair with its actual rendering metadata and submit that committed product.
- Violations occur without visible correlation: these are correctness defects,
  but do not declare the universal cause solved.
- Flicker occurs with coherent content and matching metadata: reject this
  mismatch mechanism for those events. Investigate repeated-content age, missed
  slots, view/camera state and downstream presentation. Valid holds can judder.
- Unknown provenance or incomplete coverage: repair the observer; no verdict.

If preservation is needed, assess staging both source eyes before publishing,
with submission excluded during partial publication. Separate swapchain releases
are not an atomic stereo transaction: define failure handling and ownership for
all submission paths. Caching an acquired index or `FeedSnap` cannot select old
pixels. Recopying retained pixels, separate swapchain banks or a shared stereo
image have different memory/copy/latency costs; choose after tracing the failure.

Success requires both corrected trace invariants and removal of synchronized
visible flicker at the same scene/settings, without regressing scale, depth,
attachment or pose alignment. One corrective behavior per build.

## 8. Corrections to inherited evidence

The updated eye observer fixes same-draw comparisons and tests elementwise
matrix differences. Its eight fixtures do not prove that integration supplies
opposite eyes of the same object/view pair. `Sample` has draw/object IDs but no
eye or view-pair identity. The integration's `objectId` is an XOR of geometry
parameters (`hands/mesh_split.cpp:3095`), not unique component/buffer identity.
Multiple objects can share it. Repeated evaluations can reseed the next
comparison after a pair completes, yielding overlapping adjacent-draw pairs.

Therefore `VP only 1, L2W only 239, BOTH 7271, NEITHER 30070` does not close
matrix recovery: VP changed in **7,272** comparisons, including BOTH. BOTH alone
does not prove camera-relative input; NEITHER alone does not prove duplicate
passes. Retain independent shader-space evidence, but describe this counter as
distinct-draw comparisons, not verified stereo pairs. Do not revive matrix-based
placement in this investigation.

`p2write refused = 0` excludes that explicit refusal path in this run. It does
not prove accepted writes reached the intended view. Other shared causes include
wrong view/pose association, capture-slot reuse, stale textures, incorrect
projection/space metadata, shared graphics-state leakage, uneven cadence and
runtime reprojection. The identity trace should narrow these possibilities,
not presuppose the answer.
