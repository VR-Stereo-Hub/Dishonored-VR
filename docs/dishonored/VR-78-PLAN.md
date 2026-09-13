# VR-78 plan: the view height moves the wrong way with head pitch

**Status: DRAFT FOR REVIEW. No code written.** Branch `claude/vr-78-crouch-camera-pitch`, off
`VR-Main` at `33e1e60c`. Reviewer: read sections 2 to 4 against the code before section 6.

## 1. The observation

* Crouched: pitching the head down raises the view, and pitching up lowers it. Reported on the
  VR-76 build, and again 2026-09-12.
* NEW 2026-09-12: the same thing happens **standing**, much weaker. The tester cannot tell
  whether it is the camera or the hand placement that moves.
* VR-7 (deep crouch climb) and VR-55 (slide height) are both Done. Neither is touched by this
  plan. VR-28 (crawl under furniture) shares the clamp in section 2 and is in the blast radius.

## 2. The vertical chain today, in order

Every present, the render camera's world Z is decided by four things on two lanes:

| # | Lane | Where | What it does to Z |
|---|---|---|---|
| 1 | engine | the game's camera | eye height for the stance, plus **the engine's own neck arc**: it pitches its camera about a pivot 0.321 m below and 0.062 m behind the eye (ENGINE_NOTES "The pitch pivot", measured STANDING, simulator) |
| 2 | script | `fov_lever.cpp` 38.24 eye clamp | `zmax = pawnZ + CollisionHeight - 8`, eased down at 300 uu/s, released instantly. Clips the engine's camera location fields to `zmax` |
| 3 | present -> script | `head_track.cpp:1396` -> `camera.cpp apply_offsets` | adds the eye offset, the RAW tracked head displacement, and with `[Neck] Mode=cancel` **minus the modelled engine arc** |
| 4 | script | `camera.cpp:578` | caps (base + offset) at the same `zmax` again and counts it in `g_ceilClips` |

The installed config runs `[Neck] Mode=cancel`, `[PosTrack] EyeClamp` at its default 1,
`Lane=auto` (camera lane under the projection layer).

## 3. What the log already shows (tester's last run, build `e61f0d28-dirty`)

```
eyeclamp: camZ 2719.8 -> 2681.9 (pawnZ 2624.9 cyl 65.0)    crouched: 37.9 uu clipped
eyeclamp: camZ 2886.8 -> 2886.8 (pawnZ 2807.3 cyl 87.5)    standing: the eye sits AT the ceiling
```

* **Crouched, the engine's eye is ~95 uu above the pawn and the ceiling is 57.** Step 2 clips
  about 38 uu every tick, so the clamp owns camera Z outright.
* **Standing, the engine's eye (79.5 uu above the pawn) equals `87.5 - 8` to the tenth.** The
  ceiling is exactly at rest height, so step 4 eats any upward component of the offset.
* 70 `eyeclamp` lines in one run. It is not a rare path.

## 4. Hypotheses, each with the reading that kills it

**H1 - the clamp erases the engine's arc, and the cancel term still subtracts it.**
Steps 1 and 3 are designed as a pair: the engine drops the eye about 8 uu at 30 deg down, the
cancel term adds 8 uu back, the tracked head supplies the real motion. Step 2 sits BETWEEN
them. Crouched, the engine's eye is clipped to the ceiling at every pitch, so its arc never
reaches the base, but step 3 still adds the cancelling +8. Pitch down, net view Z comes out
higher than the real head. Positive totals are then clipped at step 4, which also flattens
real upward head motion. Standing, the eye rests at the ceiling, so the same thing happens on
a smaller scale whenever the offset's net vertical is positive.
*Predicts*: crouched, the step-2 clip amount changes with pitch by the engine's arc (smaller
looking down). Rendered Z minus pawn Z does not follow the raw head Z with slope 1. Standing,
step-4 clips are non-zero and cluster on the pitches where the cancel term is positive.
*Dies if*: the step-2 clip amount is pitch-independent, and rendered Z follows raw head Z with
slope 1 crouched.

**H2 - the engine's neck pivot is different when crouched.**
The cancel numbers were measured standing. A crouch animation can move the camera joint, so
the standing cancel may over- or under-correct.
*Predicts*: the pre-clamp engine Z against pitch solves a different below/behind crouched than
standing.
*Dies if*: the crouched fit matches 0.321/0.062 within the standing fit's own consistency.

H1 and H2 can both be true; the instrument reads both from one run.

**H3 - neither: the camera is right and the hands are what move.**
*Predicts*: rendered Z follows raw head Z with slope 1 in both stances, and both clip counts
stay at 0 across the pitch sweep. Then the next question is hand placement against the camera,
not this plan.

## 5. The instrument (build 1)

`[PosTrack] ZAccount=0` default OFF, live `camera zaccount on|off`. Armed in the tester's
INSTALLED ini for launch 1. No engine writes, no new object pointers.

* **Script lane** (FovLeverApply, every tick, not only when it clips): publish the pre-clamp
  engine camera Z, pawn Z, the eased `zmax`, and the clip amount. Plain floats, latest value.
* **Script lane** (`apply_offsets`): publish the step-4 cap amount for this write, beside the
  existing `g_ceilClips`.
* **Present thread** (`head_track.cpp`, where the offset is composed): the raw up component and
  the neck up component, kept SEPARATE.
* **Present thread** (after the draw, beside `pitchtest_present_tick`): take c5 world Z and
  accumulate every published value into a bucket `stance x pitch`:
  stance from the cylinder (standing > 76, crouched 50..76, anything else skipped and counted),
  pitch DOWN < -20 deg, LEVEL |p| < 8, UP > +20. Keep the mean pitch per bucket.
* **Every 10 s, one line per stance** with at least 60 presents in each of its three buckets,
  and one refusal line naming the empty bucket otherwise:
  `zaccount: CROUCHED | DOWN p=-31 n=212: engine +95.2 clamp -38.0 raw -7.9 neck +8.0 cap -0.0 render +57.3 | LEVEL ... | UP ... | render-vs-raw slope 0.12 (1.00 = follows the head) | engine pivot below 0.30 behind 0.06 m (consistency 0.4 uu) | owner: CLAMP`
  The numbers are an illustration of the FORMAT (H1's shape), not data. All Z values are
  relative to pawn Z, and `engine + clamp + raw + neck + cap` must equal `render` within a
  tolerance the line prints; a mismatch is itself reported, since it means a term is missing. `owner` is the largest pitch-dependent term that is not
  the raw head. The line prints `owner: HEAD (slope ~1, no clip)` when H1 and H2 are both
  wrong, so it can fail its own hypothesis.
* Cost: a handful of adds per present, one format every 10 s. Nothing per property, nothing
  per draw (TRAPS "A read-only probe that cost the frame budget").

Before install: host build, lint, exports, golden ini regenerated for the new key, and a
**full diff of the installed ini against the copy taken before install**, every key, not only
`ZAccount`.

## 6. The launches

**Launch 1 - which owner removes the vertical motion? (build 1, no behaviour change)**
Load a save. Standing still, look level for 3 s, straight down for 3 s, up for 3 s. Crouch
(toggle) and repeat. Stand and repeat once more. Quit.

| Reading | Meaning | Next |
|---|---|---|
| crouched `owner: CLAMP`, clamp amount varies with pitch, slope far from 1 | H1 | build 2, F1 |
| crouched engine pivot differs from 0.321/0.062 | H2 | build 2, F2 (with F1 if H1 also holds) |
| both stances `owner: HEAD`, slope ~1, clips 0 | H3 | stop; take the hand question to a new plan |
| refusal lines only | the sweep never filled a bucket | repeat, holding each pitch longer |

**Cheaper alternative for the reviewer to weigh**: one no-build launch with
`[PosTrack] EyeClamp=0` and the same sweep, judged by eye. Discriminates H1 in one run, but it
is perceptual only, cannot see H2, and brings back the head-inside-the-table fault while
crouched. Recommended only if build 1 is judged too costly.

**Launch 2 - does the fix keep the view height put through the sweep in both stances?**
Build 2 with its lever armed ON in the installed ini. Same sweep. Pass: crouched slope within
0.1 of 1.00, standing step-4 clips at 0 through the pitch sweep, and the tester sees no height
change in either stance. The toggle (below) is the A/B inside the same launch if needed.

## 7. Fix candidates (chosen by launch 1, not before)

**F1 - a neck-aware clamp (for H1).** The clamp should hold the NEUTRAL eye inside the capsule,
not the engine's pitched eye. With `arc` = the modelled engine arc up (present only when
`[Neck] Mode=cancel`; zero otherwise, which reduces to today's code exactly):

* Step 2 clips the engine fields to `zmax + arc` instead of `zmax`. The base becomes
  `min(neutral, zmax) + arc`, so the arc survives the clamp.
* Step 3 is unchanged: `base - arc + raw = min(neutral, zmax) + raw`.
* Step 4 caps `(base - arc)` at `zmax`, so the cap judges the neutral eye and the raw head
  motion rides on top of it.

Lever `[PosTrack] EyeClampNeckAware`, default 0, live `postrack clampneck on|off`, logged at
config with the effective value and its source (TRAPS section 1).

**F2 - a crouched pivot (for H2).** `[Neck] CrouchPivotBelowM` / `CrouchPivotBehindM` from
launch 1's crouched fit, selected by the same cylinder test the clamp uses, eased at the
clamp's rate so a stance change does not pop the view. Defaults equal the standing numbers,
which is today's behaviour.

## 8. Open questions for the reviewer

1. **Step 4 and real upward head motion.** F1 lets the raw head rise above `zmax` (standing on
   toes, a stretch). The cap exists so a lean cannot put the eye through geometry. Should raw
   upward motion be capped at `zmax` plus a headroom lever, and if so what is a measured, not
   guessed, default? Today's value is effectively 0 headroom, which may be the standing
   symptom by itself.
2. **Lane timing.** The arc is composed on the present thread and step 2 runs on the script
   lane, so F1's clamp reads an arc up to one present old. The seam already reads the offset
   that way. Is a one-present disagreement between steps 2 and 4 acceptable, or should the
   script lane snapshot the arc once per tick and both steps use that copy?
3. **The eased ceiling.** Step 2 tightens at 300 uu/s. Does adding `arc` to `zmax` interact
   with the release-instantly branch (an arc that grows looking down reads as a release)?
   Proposal: ease `zmax` as today and add `arc` after the easing, never inside it.
4. **Is H1's arithmetic right?** Section 4 assumes the engine's crouched eye is clipped at
   every pitch. If the crouched engine arc exceeds the 38 uu clip at extreme pitch, the clip
   releases there and the percept changes shape. The instrument shows it; does the fix hold
   across that boundary?

## 9. Guards and blast radius

* Standing pitch must not regress. Launch 2 checks it explicitly.
* No new engine memory writes in build 1. F1 changes the value an EXISTING write puts into
  the camera fields; it adds no object pointers, so no new liveness guard is needed. If review
  finds one, it uses `IsLiveObject`, never a class-name comparison.
* The tester's stability config is not touched: `[Pace] Lag=2`, `[Stereo] LagAB=0`,
  `[Hands] PoseLag=2`, `PaletteEyeOffset=1`, `ModelScale=0.85`, `[MotionAim] Enabled=0`,
  `GamepadOnly=0`, 2750x2850, `VirtualMode=1`, 90 Hz, `AttachRigRadius=200`,
  `[Aim] FireFromHand=1 ModelRay=1 FollowHandTrim=1`, grip, trims and `ModelAxisL*`.
* VR-28 (crawl under furniture) and the vents use the same clamp. F1 does not change `zmax`
  itself, only what the pitch arc does on top of it.
