# The universal flicker fix - implementation plan for review (VR-69)

**Status: plan. Nothing built.** Written after six attempts inside the per-eye
correction, four instruments that measured the wrong population, and one
mechanism whose own counterprediction failed. It proposes a different shape, not
a seventh attempt at the same one.

---

## 1. The principle

Everything that flickers - hands, weapons - is placed by a correction that must
know **which eye a draw belongs to**. Six ways of answering that have failed:

| Source of the eye | Outcome |
|---|---|
| delta between consecutive draws | holds a stale answer when the step is unreadable |
| the drawing pass on the stack | **0 executions in 83,400 draws** - wrong thread |
| no offset at all | flicker gone, stereo depth gone, weapons enormous |
| the stereo method's reconciled tag | fixed the large flicker; residual remained; association unproven |
| a same-eye hold | falsified - zero holds fired, symptom unchanged |
| recovery from the draw's own matrices | no fixed origin; the input space moves with the camera |

Every one tried to **recover** the eye from something downstream. The proposal
is to stop recovering it:

> **The mod WRITES the per-eye camera displacement itself. Read it from the
> writer instead of reconstructing it from the renderer's output.**

`camera::apply_eye_offset` writes `base - offset` into `camera+0x330` on the
script lane, once per pass. **`base` is the head centre and `offset` is the eye
displacement, and both are ours.** Six attempts have gone into recovering a
number the mod computed and then discarded.

This also answers the blocker `FLICKER_ROOT_REVIEW.md` raised and the last plan
could not: *there is no established head-centre reference.* There is. It is the
seam's own `base`, and nothing else in this codebase is entitled to that name.

---

## 2. What must be true for this to work, stated as risks

The proposal is not "read a global". The association problem is real and has
sunk two earlier attempts, so it is the design, not a footnote.

| Risk | Why it matters | How the plan addresses it |
|---|---|---|
| The seam writes on the SCRIPT lane; draws are on the RENDER lane | A latest-value read is the retracted 39 % figure again | Publish base+offset **with a pass/write identity**, and have the draw match on that identity, not read the newest |
| The engine may re-derive or overwrite `0x330` between the write and the draw | The value consumed may not be the value written | Compare the seam's written value against `render_pos` (c5, the render-side truth) and report disagreement rather than assuming |
| A draw may belong to no pass we wrote | Mono, menu, shadow, an auxiliary pass | The correction must REFUSE, not guess, and refusal must leave the engine's own placement untouched |
| `base` may itself contain positional tracking or a crouch clamp | Then it is not a pure head centre | Publish the components separately - base, eye term, positional term - so the consumer chooses and the log says which were included |
| Two hand ranges share one draw context | This produced 105,816 meaningless comparisons | Identity is `drawId`, already stamped once per original draw; the validated observer already enforces it |

**If the association cannot be verified, the plan fails and must say so** rather
than shipping a better-looking guess. Section 5 is the gate.

---

## 3. Phase 1 - publish, and prove nothing consumes it yet

No behaviour change. `camera::apply_eye_offset` publishes a record:

```
{ writeSeq, passId, base[3], eyeOffset[3], posTrack[3], field, writtenValue[3], tMs }
```

- `writeSeq` monotonic per write; `passId` from the seam's own pass notion.
- Published under the existing seam synchronisation, whole, one structure - the
  `MpPoseSnap` pattern, which is already the project's answer to torn reads.

The draw side records, per draw, **which record it would have matched** and how
stale it is, and compares the seam's `writtenValue` against `render_pos` (c5).
It applies nothing.

**Verdicts, all reachable:**

| Verdict | Meaning |
|---|---|
| MATCHED | the draw found a record whose pass identity it can justify |
| STALE n | matched, but n writes behind |
| NO RECORD | the draw belongs to no pass we wrote - a refusal, not a guess |
| SEAM/RENDER DISAGREE | what we wrote is not what the renderer used |

**The last row is the one that would kill the whole plan**, and it is checked
first, before any placement change. If the seam's write is not what the renderer
draws from, reading the writer is worth nothing.

---

## 4. Phase 2 - validate the association before a headset run

The pattern that worked: a pure core plus a suite that runs at init and on the
desk (`eye_observer.h` / `eye_observer_test.h`, 8 cases, and the first is the
defect that cost a run).

Fixtures, each of which must FAIL the naive implementation:

1. Two hand ranges of one draw produce ONE observation, never two matches.
2. A draw arriving between two writes matches the EARLIER one, not the newest.
3. A write with no matching draw is not consumed by the next unrelated draw.
4. A draw from an unwritten pass reports NO RECORD, and the correction is
   byte-identical to the engine's own placement.
5. Script-lane writes interleaved with render-lane reads never produce a torn
   record - the whole structure or nothing.
6. `base` containing a positional term is reported with its components, and a
   consumer asking for the head centre alone gets it.
7. A stale record beyond a stated age reports STALE and is not silently used.

Then the simulator, for the paths fixtures cannot reach: pass transitions, mono,
menu, load. **No headset until every fixture passes**, because four instruments
in a row were discovered broken on the tester's time.

---

## 5. The gate

Phase 3 happens only if Phase 1 reports, over a real run:

* **MATCHED dominant**, with NO RECORD confined to passes we can name;
* **SEAM/RENDER DISAGREE ~ 0** - the written value is what the renderer used;
* **STALE bounded** and its distribution printed.

If MATCHED is not dominant, or the seam and renderer disagree, **the plan is
wrong and stops here.** That is the whole point of separating publish from
consume: the sixth attempt looked plausible right up to the headset too.

---

## 6. Phase 3 - consume it, and retire the decision

Only after the gate:

* The correction takes its eye displacement from the matched record.
* On NO RECORD or STALE it **refuses**, leaving the engine's placement exactly
  as it found it. Refusal must be byte-identical to no correction - the
  `PaletteEyeFromMatrix` build failed exactly here, falling past its own
  fallback and applying no offset, which is itself a half-IPD error.
* The old inference stays as **telemetry only** for one build, then goes.
* One behavioural change in the build. Default OFF with a live reversing A/B.

**Acceptance is perceptual and measured together:** the flicker gone from hands
and weapons in a reversing A/B/A, with size, depth, attachment, tracking and the
confirmed pose-lag fixes intact - and the trace showing MATCHED throughout.
A clean counter without the visual result is not acceptance; today produced
several of those.

---

## 7. What this plan does NOT claim

* **It does not explain the world jitter.** Weapon placement cannot move world
  geometry. If hands and weapons stop flickering and the world still jitters,
  that is a second fault and this plan says so in advance rather than absorbing
  it. Section 8 keeps it separate.
* **It does not rest on the hold mechanism.** That hypothesis has a failing
  counterprediction - `aborts=0` across 22 summaries while holds advanced 53
  times in 39.8 s - and is not part of this.
* **It does not revive matrix recovery.** Closed by `FLICKER_ROOT_REVIEW.md`.
* **It assumes `base` is the head centre.** That is the load-bearing assumption
  and Phase 1 tests it against c5 rather than asserting it.

---

## 8. The world jitter, kept separate on purpose

Three things move together: hands, weapons, world. Only the first two are placed
by the correction this plan fixes. Either the world jitter is a second fault, or
all three share a cause upstream of the correction - in which case fixing the
correction will not fix any of them, and Phase 1's trace will show MATCHED while
the flicker continues.

**That outcome is a result, not a failure**, and it is the cheapest way this
plan can be wrong. It should be stated in the acceptance criteria so it cannot
be quietly reinterpreted.

---

## 9. Questions for the reviewer

1. **Is `base` in `apply_eye_offset` genuinely the head centre**, or does it
   already carry positional tracking, the crouch clamp, or a previous eye term
   that `current_base` did not remove?
2. **Is a pass identity available on both lanes** that a draw can honestly match
   on, or does the script/render split make MATCHED unachievable in principle -
   in which case this plan should be rejected now rather than after Phase 1?
3. **Is comparing the seam's write against c5 a sufficient test** that the
   renderer used it, given c5 is the render-side position and the write is
   negated into `0x330`?
4. **Should Phase 3 refuse or fall back to the current inference** on NO RECORD?
   Refusing is honest and visibly wrong; falling back keeps a known-flickering
   path alive.
5. **Is there a reason six attempts all reached for downstream reconstruction**
   rather than the writer - some earlier measurement that ruled the writer out
   and is not in the notes?

---

## 10. Constraints

* One behavioural change per build.
* Every lever default OFF with a live reversing A/B; an armed experiment
  contaminates every later measurement.
* Instruments validated in fixtures before a headset run.
* **Verify the population before reading any number**, and print it on the line.
* **Ask what else would produce the same counter** before believing it.
* Engine fields go in `patterns.h` with measured semantics; `ENGINE_NOTES` is
  the source for what a field is, never a neighbouring file's comment.
* The tester runs the game, not the harness.
