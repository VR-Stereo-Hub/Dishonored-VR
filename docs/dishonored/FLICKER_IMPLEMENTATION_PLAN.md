# Writer-to-view identity: revised implementation plan (VR-69)

2026-09-09. Reviewed against `1757714d` and the measured history in ENGINE_NOTES.
**Status: the proposed placement consumer is blocked at the association gate.**
No new runtime observer, placement behavior, configuration or installation is
introduced by this review. The historical failure below answers the draft's
question about whether this approach had already been tried.

## 1. The outcome and the missing part

The objective remains removal of the synchronized hand, weapon and world flicker,
with correct size, depth, attachment and tracking. Preserving per-view identity
from construction to rendering and submission could remove a class of association
faults. Publishing a camera write alone does not establish that identity.

The mod knows the eye displacement it requests. It does not yet know which
original render view a particular palette draw consumes. The proposed `passId`
is the missing mechanism, not an existing shared key that can simply be added
to a struct. Do not implement a consumer that replaces this missing key with
newest-record lookup, queue order, c5 proximity, fixed lag or an eye sign.

## 2. The earlier measurement that rules out the proposed shortcut

`ENGINE_NOTES.md`, the first headset run of 2026-09-03
(`42-run30-quest3-reentry.log`, approximately lines 617-632), already records the
writer-to-render-position comparison:

- Each tag carried the position produced by the camera write.
- The consumer required c5 to agree within 2 uu.
- While walking, the engine moved the camera after the write: the documented
  example differs by approximately 2.5 uu while the eye displacement remains
  intact.
- About a third of left tags were dropped: L/s=36, R/s=54, mono/s=18. Each drop
  broke a pair and produced a visible fault. The right write, made immediately
  before the second draw, matched.
- Stationary simulator runs had concealed the failure. Position agreement was
  demoted to telemetry, with subsequent pairing corrections documented separately.

The code still contains this history in `core/gfx/reentry.cpp:101-109`.
`scene_draw.cpp:357,430` already calls `camera::last_written_pos` and carries the
result with a tag. Thus the writer's result was neither discarded nor untried.

This excludes absolute position agreement as an identity test. It does not
exclude a future record transported with an independently identified engine
view. Once identity is established, a position disagreement becomes useful
measurement of what happened after the write, rather than grounds to discard an
otherwise correctly associated view.

## 3. Correct the writer model before implementing anything

### Sign and publication cadence

The current `kFields` entry for the default camera field uses sign +1 and c5Sign
-1. `apply_offsets` writes **base + displacement**, then c5 is expected in the
opposite sign. The original `base - offset` description was superseded by the
2026-09-03 picture-based sign correction in ENGINE_NOTES.

`render_pos()` returns the latest observed raw c5. It is neither a draw-specific
record nor a positive world position. Its name and old comments must not be
used to justify a direct comparison with positive `writtenValue`.

The writer runs on camera dispatches, not exactly once per view. ENGINE_NOTES
records approximately 19 seam writes per present in one measured configuration.
A monotonically increasing write sequence therefore cannot be renamed pass ID.

### What base means

`current_base` reads the current field. If it approximately equals the writer's
last value, it subtracts the last requested offset; otherwise it accepts the
engine's current value. That is rebasing bookkeeping, not independent proof of
an unoffset head centre. The writer state is not currently keyed by camera
object plus field, so ownership changes also need explicit treatment.

The camera field can already reflect engine motion and the preceding FOV/crouch
clamp. The writer adds the eye term, adds positional tracking on the selected
lane, and applies its ceiling adjustment. Capturing those terms is useful, but
must not label the engine base as a proven head centre.

For the unclipped, correctly associated case:

```
E = B + P + eye
H = B + P
E - H = eye
```

Here B is engine base, P is applied tracking displacement, E is eye position and
H is the corresponding centre candidate. Dropping P would remove tracked head
translation from the reference. With a ceiling clamp C, generally:

```
E = C(B + P + eye)
H = C(B + P)
E - H need not equal the requested eye vector
```

Synthetic Z example: B=99, P=0, eye=+2, ceiling=100 gives E=100, H=99 and an
applied difference of 1, not 2. Define whether the desired reference follows the
clipped camera or the unclipped tracked centre before consuming an offset.
Matching c5 to E cannot settle that semantic choice.

## 4. Phase 0: establish independent view transport

This phase comes before a runtime MATCHED counter or a placement build.

1. Locate the engine boundary that constructs/enqueues a render view and the
   boundary that consumes that same view on the render thread. Use a verified
   engine view/command identity with a lifetime generation. Document source,
   ownership, queue semantics and hook byte verification in ENGINE_NOTES; any
   new engine offsets belong in patterns.h.
2. Bind a producer record to that view at construction. A write outside view
   construction remains a write observation, not a proven view association.
   Establish first- and second-eye paths separately: the first write can precede
   engine camera motion; second-eye construction has a different ordering.
3. Recover that identity at render-view entry and propagate it through render
   thread scope to the original draw. Handle nested shadow/UI/auxiliary views,
   dropped commands, replay, menus, loading and reset explicitly. An uninstrumented
   view must not inherit the previous view's identity.
4. Bind the same view identity to captured content and carry it through capture
   reuse, release and submission. Present-time tag reconciliation cannot
   retroactively identify a weapon draw that has already happened.

Current inventory: `g_sdPairId` and pose `Record.id` exist on the producer and
Present/capture paths. Palette `drawId` identifies an original D3D draw. The
second-pass latch identifies the calling script thread. None is presently a
verified engine-view token at the palette consumer. Numeric equality, a mutex
or a longer history ring cannot fill this gap.

Do not build another observer guaranteed to report UNASSOCIATED, or manufacture
MATCHED by comparing the c5 value used to select a record with that same record.
The next useful implementation is the transport above, once its engine boundary
is identified. Its absence is the present blocker, not a request for another
headset trial or approval to guess.

## 5. Phase 1: publish and observe after transport exists

A record should carry camera/field lifetime, write sequence, independently
assigned view token, source pair/eye, timestamp, rebasing decision, field value
before/after, engine base, requested eye vector, applied tracking vector, ceiling
adjustment, basis and coordinate-space definitions. Record restorations, failed
writes and camera/field changes too. Repeated writes within a view must not be
silently collapsed into a different view.

Publish/copy the whole structure with defined synchronization. Never return a
pointer into a mutable ring. Reject missing, overwritten and incompatible
lifetime keys explicitly. Coherent publication proves coherent storage, not
that engine rendering honoured the write.

Observe once per original draw before the two-hand/range loop. Read qualified
constants in effect for that draw, not the latest global c5. Verify the shader's
actual use of c5 or the relevant camera constant; a register being populated is
not proof the shader uses it as world camera position.

Keep verdict axes separate:

- Association: verified view identity, unknown view, expired record, wrong epoch.
- Content comparison: agrees, differs, unavailable/unqualified, nonfinite.
- Age: elapsed time and generations, measured separately from the two above.

A correctly associated old queued view can be legitimate. Age alone must not
cause a discontinuous switch of placement behavior. Conversely, zero position
error at a stationary camera cannot prove identity. Two independent views can
have identical constants.

## 6. Validation and the placement gate

Use the same transport/classification core in production and desk tests. Cover
repeated hand ranges, identical-valued opposite/unrelated views, delayed old
views arriving after newer writes, multiple writes per view, dropped commands,
unwritten and nested views, ring wrap, object/field changes, device reset and
concurrent publication. Test geometry with positional tracking, ceiling clipping,
head rotation, walking and first/second-eye timing differences.

Simulator integration must demonstrate both identity propagation and deliberate
missing/wrong-identity detection through actual hooks. Synthetic tokens alone
validate lookup logic, not engine association. Stationary simulation alone is
insufficient given the historical walking failure. No new headset test is
requested until this prerequisite exists and integration passes.

Placement consumption requires independently verified identity plus a documented
coordinate mapping into the draw's input space. A world-space displacement vector
still needs the correct basis, units and tracking reference; publication alone
is not the transform.

Keep the current correction unchanged during observation. For a later default-off
behavioral A/B, incomplete provenance should execute the existing complete
placement path, not omit only the eye term. Skipping all mod placement restores
engine animation but loses controller attachment; skipping only eye subtraction
creates a depth/scale error. Neither is an invisible or universally safe refusal.
If full verified coverage is required to avoid fallback transitions, enforce it
at a defined mode boundary rather than toggling behavior unpredictably per draw.

## 7. Acceptance and remaining scope

A clean association counter is not a flicker fix. Require reversing A/B/A at the
same scene/settings with hand and weapon flicker absent, correct scale/depth and
attachment, and pose-lag improvements preserved. Record the world symptom in the
same test. A placement-only improvement must not close the shared-flicker issue
if world jitter remains; the shared view/capture/submission path remains in scope.

The failed hold counterprediction weakens its simple partial-left sequence; it
does not exonerate every held-image or metadata path. Matrix recovery remains
unused because its reference and association are unresolved, not because the
distinct-draw observer proved that matrices contain no eye information.
