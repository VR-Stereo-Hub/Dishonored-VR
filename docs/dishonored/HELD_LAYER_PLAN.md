# The held layer, and what it actually re-submits - plan for review (VR-69)

**Status: plan. Nothing built for it.** Written after the validated observer
closed Phase B's first question, and after the review named a lead in a log that
was already on disk and had not been looked at.

---

## 0. What is wanted

> **Fix the CORE of the flickering, once, so all of it falls to the same change.**

The mod remains worse to play than at the start of the session. Real fixes
landed - render size, head-turn judder, weapon judder, the large weapon flicker
and double image, weapon scale and depth - and a residual remains.

**The symptom has grown, and the growth is the most useful thing about it:**

| What flickers / jitters | Placed by |
|---|---|
| Weapons | the palette |
| Hands (newly reported this run) | the palette |
| World geometry, at the same moments | **not the palette** |

Three things move together and only two of them are ours to place. That is
either a shared upstream cause or a whole-image effect, and both live downstream
of where six attempts have been spent.

---

## 1. What is now settled, with evidence

### 1.1 The eye cannot be recovered from a draw (measured, not argued)

The validated observer (`eye_observer.h`; suite `eye_observer_test.h`, 8 cases
passing, run at init and on the desk) pairs draw N against draw N+1 of the same
geometry, on `drawId` stamped once per ORIGINAL draw:

```
POPULATION samples 75163 over 37582 DISTINCT draws, 37581 pairs formed,
refused 1 same-draw / 0 other-object
VP only 1, L2W only 239, BOTH 7271, NEITHER 30070
```

Population is sound: exactly 2.0 samples per draw, pairs only ever across draws.

* **VP only: 1 of 37,581.** If the eye lived in the view matrix, that population
  would be large. It is empty.
* **BOTH: 19.3 %** - the population where the view changes. Both matrices move
  together, which is what a **camera-relative input space** looks like, and it
  confirms the shader evidence already in ENGINE_NOTES rather than resting on it.
* **NEITHER: 80 %** - consecutive draws sharing a view: the duplicate passes
  documented in `VR-33-HANDS-AND-WEAPONS.md` section 8.

**Consequence:** a reference must be expressed in the DRAW's space, never the
world's, and no single draw can yield an eye. The matrix-recovery direction is
closed by measurement.

### 1.2 The camera-refusal candidate is dead

`p2write refused` reads **0** for the whole run. Explicitly refused second-eye
writes are not this. (A successful write reaching the wrong view is untested.)

---

## 2. The lead: the hold path

From a 132-second gameplay interval already on disk:

```
present-progress skips ....... 158
held-layer resubmissions ..... 158
```

**About 1.2 per second, against a reported 1-2 per second.** The chain:

```
no Present progress at the game-side gate
 -> single gameplay draw / zero tag
 -> untagged delivery
 -> HoldUntagged
 -> no texture handed to on_present_end
 -> the saved layer is resubmitted
```

**Matching totals are not causation** and are not treated as such here. But this
is the only candidate with the right rate, in the right place, already counted,
and never joined to a visible event.

### 2.1 The mechanism that would explain every part of the symptom

From the review's own citation, and this is the sharp end:

> Saving a layer description does not preserve a historical pixel pair. **OpenXR
> uses each swapchain's LATEST RELEASED image.**

So a "hold" re-submits the **saved poses and layer descriptions** but the
**runtime resolves each swapchain to whatever it most recently released**. If
one eye released a new image between the saved pair and the resubmission, the
held layer combines:

* a **new** image for that eye,
* an **old** image for the other,
* **stale poses** describing neither.

That predicts, without further assumption:

| Observed | Predicted by this mechanism |
|---|---|
| One eye only | yes - only the eye that released a newer image is disturbed |
| World AND weapon together | yes - it is the whole eye image, not a placement |
| Orientation-independent | yes - a stale/mismatched image, not a transform |
| ~1 per second | yes - the hold rate |
| Hands as well as weapons | yes - same image |

**It is a hypothesis. It has not been measured, and the last six were not
either.** The plan's job is to measure it, including the outcome where it is
wrong.

### 2.2 What would falsify it

Record, at **every** final submission - fresh, held, mono and keepalive alike:

* each eye swapchain's **last successfully released image id**, and
* the image ids the **saved** layer was assembled from, and
* the submitted pose/FOV records and their source.

Then: **if a held submission's two eyes resolve to image ids from the same pair,
the mechanism is dead** and the hold is innocent. If they resolve to ids from
different pairs, the mechanism is live and the fix is to preserve a complete pair
rather than a description of one.

This is falsifiable without a headset - the ids alone settle it.

---

## 3. What must NOT be done first

* **Do not disable `HoldUntagged` or the present-progress guard** to see if the
  flicker stops. Both prevent known failures; removing them substitutes a
  different artefact and confounds the test. The review says this explicitly.
* **Do not touch placement.** Six attempts, and 1.1 says the information is not
  there.
* **Do not change `[Pace] Lag=2` or `[Hands] PoseLag=2`.** Both headset-confirmed;
  an A/B left armed on the latter already contaminated four runs.

---

## 4. Proposed instrument

Bounded, records only, default enabled so an ordinary launch is informative.

1. **Per-eye release identity.** A monotonically increasing id stamped when an
   image is successfully released into each eye swapchain, kept per eye.
2. **At every submission**, record: path (fresh / held / mono / keepalive), the
   two eyes' current last-released ids, the ids the saved layer was built from
   if held, the submitted pose record ids, and the frame index.
3. **The verdict line**: how many held submissions resolved to a MATCHED pair
   versus a SPLIT pair, and the id gap when split.

**The line must be able to say MATCHED.** If every held submission resolves to a
matched pair, this plan is wrong and says so - the property the four failed
instruments lacked and the validated observer now has.

### Population discipline, learned the hard way

Print the population beside the verdict - submissions seen, held, fresh, and how
many were classified - and separate startup, gameplay and menu. Four instruments
in one session produced confident numbers about the wrong sample, and the fifth
only worked because its population was printed first and checked.

---

## 5. Questions for the reviewer

1. **Is the latest-release mechanism in 2.1 correct for this runtime's usage** -
   specifically, does the mod release an eye image between a saved pair and a
   resubmission, or is release always paired with submission such that the
   situation cannot arise?
2. **Is a per-eye release id sufficient identity**, or does the acquire/release
   index cycle make ids ambiguous across a swapchain's image ring?
3. **Should the fresh path be instrumented too**, or is the held path enough for
   a first correlation? Instrumenting both doubles the cost but makes MATCHED
   meaningful as a control rather than an absence.
4. **Is 158 skips / 158 holds over 132 s worth this much weight**, given the
   review's own caution that equal totals are not correspondence?
5. **What else could move world, hands and weapons together** that is neither
   the palette nor the submission path, so it is not missed by scoping to this?

---

## 6. Constraints

* One behavioural change per build; broken three times this session.
* Every new lever default OFF with a live A/B; an armed experiment contaminates
  every later measurement.
* Instruments validated in fixtures before a headset run - `eye_observer_test.h`
  is the pattern and it worked.
* Verify the POPULATION before reading any number.
* The tester runs the game, not the harness.
* Never commit game-derived captures.
