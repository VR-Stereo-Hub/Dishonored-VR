# Fixing the ROOT of the weapon flicker - proposal for review (VR-69, 2026-09-09)

**Status: written, built, NOT installed for testing and NOT verified.** This is a
proposal. It exists because the per-symptom approach has now failed enough times
that continuing it is the wrong call.

---

## 0. What is actually wanted

Stated plainly, because the last several rounds have drifted from it:

> **Fix the CORE of the weapon flickering, once, so that all of the flicker
> bugs are addressed by the same change - rather than chasing them one at a
> time.**

And the honest status: **the mod is currently worse to play than at the start of
this session.** Several real fixes landed (render size, head-turn judder, weapon
judder, the large weapon flicker and the double image, correct weapon scale and
depth), but the residual left-eye flicker and the churn around it have made the
experience net worse. That is the thing to fix, not the counter.

---

## 1. Why the one-at-a-time approach kept failing

Every weapon flicker in this project traces to a single line of code needing to
know **which eye a draw belongs to**:

```cpp
dcam[i] -= (float)eyeUse * halfIpdUU * c->r[i];
```

The placement target is **head-relative** (the hand resolved into the head's
right/up/forward). The draw's own position is **eye-relative**, because
`LocalToWorld` here is rebased on the view. Bridging those two frames needs the
distance between this eye's camera and the head's - and that has always been
obtained by **deciding** which eye the draw is and applying a fixed half-IPD in
that direction.

Every flicker has been that decision being wrong:

| Attempt | How the eye was decided | How it failed |
|---|---|---|
| Original | palette's delta inference between consecutive draws | HOLDS a stale answer when the sideways step is unreadable -> full-IPD displacement |
| `PaletteEyeFromPass` | the drawing pass on the stack | executed **zero times in 83,400 draws** - the passes run on the game thread, these draws do not |
| Offset OFF | no offset at all | flicker gone, stereo depth gone - weapons read as infinitely far away and looked enormous |
| `PaletteEyeFromMeasured` | the stereo method's reconciled eye | fixed the large flicker and the double image, but the value belongs to a *publication*, not to this draw's view |
| `HoldSameEye` | (guarded a stale swapchain instead) | falsified - zero holds fired and the flicker was unchanged |

**Three separate hypotheses about the residual flicker have each been falsified
by their own instrument.** All three were attempts to make the decision more
reliable. None questioned whether the decision needs to exist.

---

## 2. The proposal: delete the decision

**The draw is already holding this eye's own ViewProjection.** `MpAcquireCtx`
reads `vp` straight off the device every draw and derives the camera basis from
it. That matrix *is* the eye's view - the two eyes differ precisely by it.

For a row-vector `VP` the fourth row is the translation term, so the camera's
own position resolves directly from the matrix the draw is already using:

```
camPos . r = -vp[3][0] / |r_raw|
camPos . u = -vp[3][1] / |u_raw|
camPos . f = -vp[3][3]
```

Subtract the head camera's position (`g_camObj + 0x80`, the read `blink.cpp` and
`aim_ray.cpp` already use) and **the remainder along `r` IS this draw's eye
offset** - in the draw's own units, signed correctly by construction.

```cpp
c->eyeMeasR = camR - headR;                 // measured, per draw
dcam[i] -= c->eyeMeasR * c->r[i];           // no eyeUse, no half-IPD constant
```

### What this removes

* No inference, so nothing to hold a stale answer.
* No ±1 decision, so no convention to get backwards - and the sign question
  raised in review (temporal offset versus convention) becomes **moot**, because
  neither is consulted.
* No cross-thread channel, so no association problem between a publication and a
  draw. This is the gap `FLICKER_REVIEW.md` section 3A named, closed by not
  needing the channel rather than by fixing it.
* No unknown case. A mono pass measures ~0 on its own and needs no recognising.
* No dependence on the alternation holding, which is what killed the last
  hypothesis.

### It checks itself, and refuses

The measured magnitude **must** land near half the IPD. If it does not, the
recovery from the matrix is wrong, and the code keeps the old decided path
rather than applying a number that cannot be an eye offset - because a wrong
offset here is the very full-IPD displacement being replaced. The refusal is
counted and logged with both numbers.

The old decision is retained as **telemetry**: agreement between the measured
sign and the decided eye is counted and printed. That is the falsification -
if the measured offset is real, it should agree with the decision on the ~97 %
of draws where the decision was already right, and differ on the rest.

---

## 3. What this does NOT claim

* **It is unverified.** Built and compiled; not installed, not run, not seen in
  a headset. Three hypotheses have already died at this stage today.
* **It does not fix the load-in flicker by itself.** That is a capture/slot
  identity question, not a per-eye placement one. If it helps, that is evidence
  the two share the decision; if it does not, they are separate and this plan
  should say so rather than absorb it.
* **The matrix recovery may be wrong.** The `vp[3][*]` layout is inferred from
  how the existing code reads `r`, `u` and `f` out of the same matrix, and the
  standing caveat in `MpAcquireCtx` - that normalising rows this way assumes a
  **symmetric projection** - applies here too. An asymmetric projection would
  need the principal-point terms removed first. The sanity gate is what stops a
  wrong derivation from displacing anything, but the gate is not a proof.
* **It does not address the weapon-swap flicker or the contract re-match.** Those
  are object identity, not eye identity.

---

## 4. Questions for the reviewer

1. **Is the camera recovery from `vp[3][*]` correct for this engine's matrix
   convention**, and does the symmetric-projection caveat already noted in
   `MpAcquireCtx` invalidate it for the viewmodel pass specifically, which is
   the pass that matters here?
2. **Is `g_camObj + 0x80` the HEAD camera or one of the eyes?** The whole
   subtraction depends on it being the centre. If the mod is writing per-eye
   camera positions into that object, the difference measured would be zero or
   nonsense, and the gate would refuse everything.
3. **Is the sanity gate the right shape** - magnitude within 2x the half-IPD -
   or should it also require the sign to agree with the decided eye before
   trusting it?
4. **Should the decided path be removed once this is confirmed**, or kept as the
   fallback it currently is? Keeping it means keeping the inference alive; removing
   it means a refusal has nowhere to fall back to.
5. **Does this plausibly address the residual left-eye flicker specifically?**
   The mechanism explains a wrong offset; it does not obviously explain a
   one-eye-only symptom, and that gap has caught two previous hypotheses.

---

## 5. Test plan, if the review approves

One run, no other changes, `PaletteEyeFromMatrix=1` against `=0` as a reversing
A/B so the comparison returns to its baseline.

**Read first, before any perceptual judgement:** the measured magnitude as a
percentage of the half-IPD. If it is not near 100 %, the derivation is wrong and
nothing else in the run means anything - that check comes before asking the
tester what they saw.

Then: flicker present or absent in each eye, weapon size and depth unchanged,
and the agreement counters between measured and decided.

---

## 6. Constraints being honoured

* Default ON is a deliberate exception to "every render lever ships OFF": the
  lever's OFF state is the current broken behaviour, and the gate plus fallback
  means a failed derivation cannot make it worse than OFF.
* One behavioural change in the build.
* The instrument can fail its own hypothesis, and the check that would fail it is
  named above, before the run.
