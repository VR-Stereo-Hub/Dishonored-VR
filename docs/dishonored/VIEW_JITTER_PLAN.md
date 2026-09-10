# The view lane, not the palette - plan for review (VR-69, 2026-09-09)

**Status: plan. Nothing built for it.** Written after a full session inside the
palette's per-eye correction produced five attempts and no fix, and after two
measurements that together say the palette is the wrong place to be looking.

---

## 0. What is wanted, and where things stand

> **Fix the CORE of the weapon flickering, once, so all the flicker bugs fall to
> the same change.**

**The mod is currently worse to play than at the start of the session.** Real
fixes landed - render size, head-turn judder, weapon judder, the large weapon
flicker and double image, correct weapon scale and depth - but a residual
left-eye flicker remains and the churn around it has made the experience net
worse. That is the thing to fix.

**Do not propose another change inside the per-eye correction.** Five have been
tried; the evidence below is why none of them could have worked.

---

## 1. The two measurements that redirect this

### 1.1 The weapon draws carry no per-eye difference at all

The Phase B probe pairs consecutive draws of the same object and asks which
matrix differs:

```
VP only 0, L2W only 0, BOTH 0, NEITHER 105816, of 105816 pairs
```

VP identical, LocalToWorld identical, recovered camera moved 0.00 uu, object
position moved 0.00 uu, against a half-IPD of 3.15 uu. **Not one exception.**

**Two conclusions, of different strengths.**

*Strong:* recovering the eye from these matrices is impossible. There is no
per-eye information in them. `PaletteEyeFromMatrix` is closed by measurement.

*Weaker, and stated as the instrument's limitation:* 100 % of 105,816 is not a
decision going wrong - that would be intermittent. The probe pairs CONSECUTIVE
draws of one object, so what it caught is one object drawn more than once within
**one view** - the duplicate passes `VR-33-HANDS-AND-WEAPONS.md` section 8
documents - not the two eyes. **It has not measured a left/right pair at all.**

What survives: at the site where the per-eye correction is applied, consecutive
draws of one object share a view, and the correction is applied per DRAW to
draws that are not per-eye.

### 1.2 The world jitters when the weapons flicker

Reported in the same run, unprompted:

> when moving my head around, there is a slight amount of world geometry jitter
> every time the weapons flicker

**Weapon placement cannot move world geometry.** The palette correction touches
the hand and weapon meshes and nothing else. A shared moment therefore means a
shared cause **upstream of both** - in the view or the camera - and the palette
is downstream of it.

That single observation is worth more than every counter in this session,
because it is the first evidence that the thing being corrected is not the thing
that is wrong.

**It is a perceptual report and it has not been instrumented.** The plan's first
job is to establish whether the synchronisation is real and how tight it is, not
to assume it.

---

## 2. Why five attempts inside the palette all failed

For the reviewer's context, and because the pattern matters more than the list:

| Attempt | What it did | Outcome |
|---|---|---|
| Delta inference (original) | eye from the sideways step between draws | holds a stale answer when the step is unreadable |
| `PaletteEyeFromPass` | eye from the drawing pass on the stack | **0 executions in 83,400 draws** - wrong thread |
| `PaletteEyeOffset=0` | apply no offset | flicker gone, stereo depth gone, weapons looked enormous |
| `PaletteEyeFromMeasured` (+ sign) | eye from the stereo method's reconciled tag | fixed the LARGE flicker and the double image; residual remained |
| `HoldSameEye` | hold the pair on a same-eye repeat | **falsified** - zero holds fired, flicker unchanged |
| `PaletteEyeFromMatrix` | recover the eye from the draw's own matrices | **impossible** - the matrices carry no eye |

Every one tried to make the eye decision more reliable. Section 1.1 says the
information is not present at that site, and 1.2 says the symptom is not
confined to what that decision controls.

### Three method failures worth not repeating

Recorded because the reviewer has caught each of them and they are the reason
this plan proposes measurement before code:

* **A mechanism that predicted the symptom was built before it was measured**,
  three times. Each was falsified by its own instrument within one run.
* **A counter that moved was believed without asking what else would move it the
  same way.** Agreement went 0.2 % -> 97.9 % on a sign flip; in an alternating
  stream, "one publication late" and "opposite convention" produce identical
  counts, and `scene_draw.cpp` declares pass 1 LEFT and pass 2 RIGHT outright,
  so the conventions never disagreed.
* **An engine field was used from a neighbouring file's stale comment**, when
  `ENGINE_NOTES` had already measured it as a fixed offset vector and retired
  it - and the instrument printed the retired constant on its own first line.

---

## 3. What the view lane already contains

The reviewer will know some of this; collected so the plan is self-contained.

| Piece | Where | Note |
|---|---|---|
| Per-eye camera write | `camera+0x330`, negated (`kPovOffs[0]`) | measured HONOURED 119/120; c5 is its negation and IS the camera world position |
| The camera seam | `camera.h` / `apply_eye_offset` | writes eye displacement, positional tracking, possibly a ceiling clamp into the active field |
| Render-side truth | `camera::render_pos` - c5 of the last draw | the position the renderer actually used |
| Stereo pass eyes | `scene_draw.cpp:369,424` | pass 2 = RIGHT (+1), pass 1 = LEFT (-1), declared |
| Tag reconciliation | `reentry.cpp` c5 pairing + tag ring | agree/disagree counters exist; ~3 % disagreement in settled gameplay |
| Pose selection | `[Pace] Lag=2`, `[Hands] PoseLag=2` | both headset-confirmed, **must be preserved** |
| The lean/positional patch | `core/framework/vs_const.cpp` LeanVP | patches c0 view-projection |

**Candidates that could move BOTH the world and the weapon**, in rough order of
how cheaply they can be separated:

1. **The per-eye camera write landing late or twice.** The seam writes
   `camera+0x330` on the script lane at dispatch cadence and the notes say it
   must be rewritten every dispatch. A missed or doubled write moves the whole
   rendered view, world included.
2. **The positional/lean patch** (`LeanVP` on c0). It patches the view-projection
   directly, so an inconsistent application shifts everything drawn under it.
3. **A pass running from the previous pass's camera.** `reentry.cpp` already logs
   `pass 2's eye write REFUSED by the camera seam ... both eyes carry one view`.
   That is a named, counted, existing condition that would move world and weapon
   together, in one eye.
4. **Positional tracking or the crouch clamp** perturbing the camera between the
   two passes.

**Candidate 3 has an existing counter and should be read before anything is
built.** It is the closest match to a one-eye symptom that moves everything.

---

## 4. Proposed first step: establish the correlation before explaining it

No behaviour change. One question: **is the world jitter genuinely synchronised
with the weapon flicker, and what else is true at that moment?**

* Detect the flicker moment from something already measured - the eye decision
  changing against its recent run, or the per-eye correction's applied sign
  flipping between consecutive draws of the same object.
* At that moment, record the view lane's state: the last `camera+0x330` write and
  its age, `render_pos` (c5) and its delta from the previous present, whether
  pass 2's eye write was refused, the tag ring's verdict, the lean patch's
  applied value, and the present/pass identity.
* Keep a small ring and print a bounded summary - not a per-draw log.

**What each outcome would mean:**

| Observation at flicker moments | Reading |
|---|---|
| c5 moves by ~IPD when it should not, or not at all when it should | the per-eye camera write is the shared cause - candidate 1 |
| `pass 2's eye write REFUSED` coincides | candidate 3, and it is already counted |
| The lean/positional patch differs between the two passes | candidate 2 |
| Nothing in the view lane moves | the synchronisation is coincidence or perceptual, and this plan is wrong |

**The last row is the point.** The instrument must be able to say the world
jitter is unrelated, or it is another instrument that can only confirm.

---

## 5. Explicit non-goals

* **No change inside the per-eye correction.** Section 2 is why.
* **No change to `[Pace] Lag=2` or `[Hands] PoseLag=2`.** Both headset-confirmed;
  an A/B left armed on the latter already contaminated four runs.
* **No new engine field without a `patterns.h` entry and measured semantics.**
* **Not the weapon-swap flicker or the contract re-match** - object identity, a
  separate problem, and `wa/key:` has now been read once (a `bolt_01` re-match on
  `vb ib numVerts primCount`).
* **Not the load-in flicker**, unless the view lane turns out to explain it.

---

## 6. Questions for the reviewer

1. **Is the world/weapon synchronisation worth this much weight** on one
   perceptual report, or should it be confirmed by a second run first?
2. **Is candidate 3 - a pass running from the previous pass's camera - already
   sufficiently instrumented** to be checked from an existing log rather than a
   new build? If so that is free and should come first.
3. **What is the right flicker DETECTOR?** Deriving it from the eye decision
   risks circularity, since that decision is a suspect. Is there an independent
   trigger - a c5 discontinuity, a projected-anchor jump - that would not beg the
   question?
4. **Does the Phase B result generalise the way section 1.1 claims**, or is
   "consecutive draws of one object share a view" over-read from a probe that
   never paired the eyes?
5. **Anything in section 3's candidate list that is already excluded** by prior
   measurement, so it is not re-derived.

---

## 7. Constraints

* One behavioural change per build; broken three times this session, one run lost
  each time.
* Every new render lever default OFF with a live A/B; an experiment left armed
  contaminates every measurement after it.
* Instruments must be able to fail their own hypothesis, and must be checked for
  circularity **before** the headset run.
* The tester runs the game, not the harness: diagnostics ship enabled and must be
  readable from `dishonored_vr.log`.
* Never commit game-derived captures.
