# The flickers - a plan for review (VR-69, 2026-09-09)

## REVIEWED, AND ONE CORRECTION KILLS MY EXPLANATION

`FLICKER_REVIEW.md` supersedes this. Checked against the source, its corrections
hold:

* **The sign fix was NOT a convention difference.** `scene_draw.cpp:369` and
  `:424` declare pass 2 the RIGHT eye (+1) and pass 1 the LEFT eye (-1)
  outright. The conventions already agreed, so my stated mechanism is wrong.
  In an alternating stream the PREVIOUS eye is the negation of the current one,
  so "one publication late" and "opposite convention" produce identical
  agreement counts - the counter cannot separate them, and I read it as if it
  could.
* **And that is a better explanation of the residual flicker than anything in
  Part 2 below.** A negation only equals the previous eye WHILE the stream
  alternates. At a repeat, an extra present or a mono transition it yields the
  WRONG eye - exactly the moments that would give an intermittent one-frame
  jump.
* **The left lean is a loading artefact.** Trimmed to 87 s of settled gameplay:
  2,160 left against 2,120 right, 3.07 % against 3.01 %, ratio 1.019 - not 1.54.
  Most of the excess accumulated before gameplay.
* **The 5,770 unknowns are not 5,770 bad draws.** The counters measure placement
  EVALUATIONS, several per draw, and most accumulated before the interval. The
  trimmed share is 1.48 %, not ~4 %.
* **The correction still stores the inference** (`weapon_attach.cpp:331`
  compares `w->eye` against `g_mpEyeState`), so its freshness check cannot
  verify the effective eye it claims to.
* My "140,000 draws" was the agreement subtotal; the population is 152,751
  evaluations.

**Build 1 has started with the smallest piece that separates the two
explanations**: the eye channel now carries a publication sequence, and the draw
records whether it steps by exactly one. A stream that steps +1 every time is
reading a different publication each time and the offset is temporal.

---


**Review:** [FLICKER_REVIEW.md](FLICKER_REVIEW.md) supersedes the conclusions and
test order below. It corrects the gameplay populations, identifies missing
draw-to-eye association, and proposes an incremental diagnostic plan. This draft
is retained as the original hypothesis record.

**Status: draft for review. Nothing here is built.** Two wrong hypotheses have
already been spent on the residual left-eye flicker, both falsified by their own
instruments, and this document is written to avoid spending a third the same way.

It answers two questions. The narrow one: what is left of the weapon flicker.
The broad one, which the tester asked and which may matter more: **why has this
project produced so many separate flicker bugs across its whole development?**

---

## PART 1 - WHY SO MANY FLICKERS

### The structural answer

Every flicker this project has had is the same fact surfacing in a different
place:

> **The engine renders MONO. The mod reconstructs stereo around it. Every
> per-eye and per-frame fact the mod needs is therefore INFERRED rather than
> known - and an inference that is right 88 % of the time flickers 12 % of the
> time.**

That is not a bug list, it is an architecture. The engine never tells us which
eye a draw belongs to, which frame a captured image came from, or which
generation of head pose a rendered view used. Each of those has been
reconstructed from side evidence, and each reconstruction has its own error rate.

| What must be known per frame or per eye | How it is currently obtained | Failure mode when it is wrong |
|---|---|---|
| Which eye this present carries | the stereo method's tag ring + c5 pairing | wrong eye submitted, or one eye's swapchain left stale |
| Which eye a weapon DRAW belongs to | the palette's delta inference (holds when the step is unreadable) | weapon displaced a full IPD sideways |
| Which head pose the rendered view used | a lag chosen against a measurement (VR-65, VR-68) | world or weapon swims against the other |
| Which weapon a draw is | a fourteen-field contract match | a weapon detaches or a wrong mesh is corrected |
| Which capture slot holds this frame | shared-surface delivery, one present delayed | the mono window, the load-in flicker |

**The pattern in the fixes that WORKED is identical in all of them: stop
inferring, find the authoritative source, and use it.**

* VR-65: the submitted pose stopped being assumed one generation back and was
  chosen against a measurement (`bv/lag`, 0.119 deg at lag 2 against 1.19 at 0).
* VR-68: the hand stopped being normalised against the freshest head and was
  given the head the view was actually rendered from.
* VR-69 (this session): the weapon's per-eye offset stopped taking the eye from
  the palette's own delta inference and took the stereo method's measured eye -
  agreement went from 0.2 % to 97.9 % once the sign convention was corrected.

**And the pattern in the fixes that FAILED is also identical: a mechanism was
proposed that predicted the symptom, and it was built before it was measured.**
Three times this session alone. Each was falsified by its own instrument, which
is the only reason the cost was one run each instead of a week.

### What this suggests as a direction, not a task

Two candidate directions, and the reviewer's opinion on which is worth it is the
main thing wanted from Part 1:

**(a) Keep fixing inferences one at a time.** Cheap per fix, and each one is
verifiable. But the list above is not obviously finite, and each fix leaves the
architecture that generates them intact.

**(b) Establish a per-frame IDENTITY that everything reads.** One record per
rendered frame carrying: the locate generation, the eye, the capture slot, the
contract state - stamped at the point each becomes known and carried through to
submission. Consumers stop reconstructing and start reading. This is close to
what `PERF_REVIEW_2.md` section 8 already asked for, for a different reason.

(b) is a large change and this project has a rule about those. It is raised
because five separate flicker bugs have now been paid for individually.

---

## PART 2 - THE RESIDUAL LEFT-EYE FLICKER

### Symptom

Both weapons jump LEFT for about one frame, 1-2 times a second, **in the left
eye only**. The right eye is clean. Confirmed independent of weapon orientation:
rotated upside down, the jump still goes left. Head-turn judder is separately
fixed and confirmed gone (`PoseLag=2` fixed on).

### Two hypotheses already dead

1. **`PaletteEyeOffset` taking the eye from the palette's inference.** Real, and
   fixed - it was causing a much larger flicker and the double image. Not this.
2. **The left eye's swapchain going stale from a same-eye push.** Falsified by
   its own instrument: a run with `HoldSameEye=1` recorded **zero holds and zero
   same-eye repeats**, and the flicker was unchanged. The fault it named did not
   occur and the symptom persisted.

### What the per-eye audit now says

Split left/right for the first time, over ~140,000 draws:

| | left | right |
|---|---:|---:|
| measured and inferred AGREE | 70,871 | 70,590 |
| DISAGREE | **3,350** | **2,170** |
| method had no answer, inference ALSO unknown | 5,770 total - neither counter could classify them | |

Reading it honestly:

* **Agreement is symmetric** (70,871 vs 70,590, 0.4 % apart). The eye decision
  is being made about equally often for each eye.
* **Disagreement is asymmetric**: the left eye disagrees **1.54x** as often.
  That is a real lean, but it is 4.5 % of left-eye draws against 3.0 % of
  right-eye draws - not obviously enough to produce a flicker in one eye and
  none at all in the other.
* **5,770 draws where NEITHER source knew the eye.** Both per-eye counters read
  0, which means `g_mpEyeState` was also unknown in every one of them. In that
  case `eyeUse == 0` and **no offset is applied at all** - the weapon is drawn
  at the uncorrected position, which is half an IPD from where it belongs.

That last row is the strongest remaining candidate and it was invisible until
this run: about 4 % of draws get **no correction rather than a wrong one**.

**But it does not obviously explain a LEFT-ONLY symptom either**, since an
uncorrected draw is wrong in both eyes - just in opposite directions. Unless the
uncorrected position happens to coincide with the correct right-eye position,
which is exactly the kind of thing that should be measured rather than assumed.

### The measurement that would decide it, before any fix

* Split the 5,770 by which eye the *stereo method* said the present was, rather
  than by the palette's `g_mpEyeState` (which is unknown in all of them, so it
  cannot classify them). The method's tag is available and is the whole point of
  the VR-69 channel.
* Record, for one flicker event, the sequence of draws for both eyes with their
  eye decision, the applied offset and the resulting screen position - a short
  ring, dumped on demand, not a per-frame log.
* Confirm whether the uncorrected position is closer to the correct left or the
  correct right position. If it coincides with right, a left-only symptom from a
  both-eyes fault is explained and the mechanism is closed.

### Questions for the reviewer

1. **Is the 5,770 "neither source knows" population the right next target**, or
   does the 1.54x disagreement lean deserve to go first?
2. **Does an uncorrected draw explain a one-eye symptom?** The geometry above is
   the argument; it has not been measured.
3. **Is `no offset` the right fallback at all** when the eye is unknown? The
   alternatives are to hold the previous eye's decision, or to skip the draw.
   Each has its own failure mode and none is obviously right.
4. **Part 1, (a) or (b)?** Five flicker bugs have been paid for individually. Is
   a per-frame identity record worth the size of the change, or is the one-at-a-
   time route still the better economics?
5. **Anything in the two dead hypotheses that should NOT be considered closed.**
   Both were falsified by their own instruments, but both instruments were
   written by the same person who wrote the hypotheses.

---

## Constraints

* Every new render lever default OFF with a live A/B; fail soft.
* One behavioural change per build - broken three times this session and it cost
  a run each time.
* An experiment left armed contaminates every measurement after it
  (`PoseLagAb` did exactly this for four consecutive runs).
* The tester runs the game, not the harness: diagnostics must ship enabled and
  be readable from `dishonored_vr.log`.
* Instruments must be able to fail their own hypothesis, and must be checked for
  circularity BEFORE the headset run rather than after.
