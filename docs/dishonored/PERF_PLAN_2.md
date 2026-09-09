# Plan 2 - the 80 Hz deficit and the weapon judder (VR-67/VR-68 draft, 2026-09-09)

**Review update:** [PERF_REVIEW_2.md](C:/dev/Dishonored-VR/docs/dishonored/PERF_REVIEW_2.md)
revises this draft against the original log and active pose/weapon paths.
Its conclusions and test order supersede the proposals below; this draft is
retained as review input.

## RETRACTIONS (2026-09-09, after review 2)

**Four claims below are withdrawn. All four were checked against the log and the
source and all four are wrong.** They are left in place, struck through here
rather than deleted, because the record of a wrong analysis is worth more than a
tidy document.

1. **1.3, "the hitch distribution MOVED", is a COUNTING ERROR.** Gameplay ran
   8314609-8403078. **All 18 `out` gaps occurred BEFORE gameplay** (8291375 to
   8313359 - startup and the menu). Inside gameplay, all 19 detected gaps sat in
   the submission tail, exactly as in the previous run. There is no shift. The
   whole "weakens a single-cause story" paragraph is void.
2. **1.1 joins the wrong windows.** The 12.4 ms GPU span belongs to a
   **57.0/s** window (8397593) that the draft never mentions; the 73.7/s window
   is 9.7 ms. And the 66.7/s row is post-MENU with **untagged 40** - unusable as
   evidence of anything.
   **The window I missed is the interesting one**: 57.0/s, 17.5 ms tick, GPU
   12.4 ms, render-thread R 11.6 ms + desktop Presents 4.8 ms = ~16.4 ms elapsed
   against a **0.1 ms pacing wait**. That is where the deficit lives, and it is
   not obviously a pixel problem.
3. **3.1's mechanism names the wrong variable.** A newer CONTROLLER sample is not
   double-corrected: a controller drawn as `H_r^-1 C_c` and reprojected by
   `H_d^-1 H_r` yields `H_d^-1 C_c` for any age of `C_c`. The residual only
   appears if the hand is made head-relative using a head `H_s` that differs from
   the head `H_r` the view was rendered with, leaving `H_r H_s^-1`.
   **The error is a head/view transform mismatch, not controller sample age**, and
   the "world at N-2, weapon later than N, plus the prediction interval" arithmetic
   is unmeasured and withdrawn.
4. **3.3.1, the lag sweep, is not a discriminator and would have misled.**
   Changing `Lag` changes the tag for the WHOLE image equally, so it moves the
   world and the weapon together and cannot move the weapon-versus-world residual.
   Running it and reading the result would have been the VR-65 mistake again.

Two citations were also wrong: `mesh_split.cpp:2109` is a flicker diagnostic
(`g_mpFlickYaw0 = g_hmdYaw`), not weapon placement; and "a grep returns nothing"
searched the submission layer's variable names. **The hands DO have pose
plumbing** - `MpPoseSnap.gen`, `WaCommon.poseGen`, `g_waPoseGenDiff`.

### What survives, in the corrected form

`MpDriveTick` reads head and hands from the SAME consume (`g_devPose[0]` and
`g_devPose[3+h]`, filled by `DvrConsumePoses`), so the snapshot is internally
coherent. It publishes head-relative data. **The draw then completes it with a
camera basis `B` derived from the shader constants of the actual render.**

So the open question is whether `B`'s head and `g_devPose[0]` are the same
sample - and `STATUS.md` already records that they may not be:

> the camera record still does not guarantee it holds the sample the camera was
> calculated from - both writers calculate from the loose globals and consume the
> coherent sample afterwards

**That is `H_r H_s^-1`, already written down as an open defect before the weapon
judder was reported.** It is a candidate, not a finding, and the motion matrix
decides whether it is worth instrumenting.

---

**Status: draft, for review before anything is built.** Section 1 is measured on
the run described. Sections 3 and 4 are hypotheses with falsification tests.
Nothing here is implemented.

This supersedes `PERF_PLAN.md` where they disagree, and accepts every correction
in `PERF_REVIEW.md`. Two of that review's corrections are load-bearing below.

---

## 0. What changed since the review

The review's central correction was that **the budget is 25 ms, not 12.5 ms**,
because the previous run was at 40 fps with spacewarp. That was correct for that
run. **It is not correct for this one, and the difference is the point.**

The new period instrument - added because the 3 s summary prints only the period
its window ENDED on - reports, once, and never again:

```
xr: runtime period is 12.50 ms (80.0 Hz) at the first located frame
    - predictedDisplayPeriod, NOT the panel refresh.
      Every later change is logged; if none is, the period held all run.
```

**Zero period changes in the whole run.** The runtime asked for **80 fps** for
its entire duration, and the previous run's 25 ms period is absent.

The tester set Virtual Desktop to force SSW enabled for this run. The runtime
still advertised the full 12.50 ms period throughout. **Open question for the
reviewer**: whether VD's forced SSW does not double `predictedDisplayPeriod`, or
whether SSW was not actually engaged. Either way, what the application was ASKED
for is not in doubt, and it is 80 fps.

So the budget question is not "which of 25 or 12.5 is right" - it is that **the
runtime chose differently between two runs and neither of us knew.** The
instrument that would have caught it did not exist until this build.

---

## 1. What is measured, this run

Render size **2750x2850** per eye (restored), `[Pace] Lag=2`, capture mode
**shared** (not deferred - the earlier draft said deferred and was wrong), the
A/B sweep **off** so nothing varied underneath.

### 1.1 The application is missing the deadline it was given

| Window | tick | achieved | budget | GPU per tick | verdict line |
|---|---:|---:|---:|---:|---|
| A | 13.6 ms | 73.7/s | 12.50 ms | 12.4 ms | `UNDER-SUBMITTING 0.92x` |
| B | 12.5 ms | 66.7/s | 12.50 ms | 9.7 ms | |
| C | 12.5 ms | - | 12.50 ms | 8.6 ms | `MATCHED 1.00x` |

The tester independently reports **60-70 fps in the main hub**, which matches the
66.7-73.7/s in the log.

GPU per tick ranges **8.6-12.4 ms against a 12.5 ms budget**. In the busy window
it is at 99 % of budget. This is the throughput deficit, and at 80 Hz it is real
- unlike the previous run, where 14-17 ms against a 25 ms budget was comfortable
and the earlier draft wrongly called it a problem.

### 1.2 The CPU split is NOT the constraint at this rate

```
P1[-1] in 3.2 (pre 0.0  begin 1.6 [wait 1.6]  tick 0.1  method 0.1
               [cap 0.0]  end 0.0 [acq 0.0 xrCopy 0.0 endFrame 0.0]  present 1.4)
       + out 4.1 (idle 0.4  R 3.8)
P2[+1] in 3.0 ... 
```

The two desktop `Present` calls now cost about **2.5 ms per pair**, not the
4.8 ms measured at the half-rate run. The pacing wait has collapsed to 1.6-2.7 ms
because the app is no longer parked. **The Present cost did not disappear; it is
simply smaller here and the budget is half as large.**

### 1.3 The hitch distribution MOVED

50 gaps this run. Location tally, against the previous run:

| Where the gap sat | this run | previous run |
|---|---:|---:|
| `present-tail` (xrEndFrame) | **26 (52 %)** | 71 (81 %) |
| `out` (render thread after Present returned) | **18 (36 %)** | 13 (15 %) |
| other | 6 | 4 |

`endFrame mean=0.29-0.53 ms, max 6.7-32.0 ms` - much smaller than the previous
run's 40-108 ms tail.

**This is a different mixture, and it weakens a single-cause story.** The
`out` share more than doubled. `out` is the render thread executing the frame's
commands after Present returned, which is our own and the engine's work, not the
runtime's. Whatever is hitching is not only in the submission path.

Both runs differed in period AND in scene AND in SSW setting, so this comparison
is suggestive, not controlled.

---

## 2. Two separate problems, and they should not be conflated

1. **A throughput deficit at 80 Hz.** GPU 8.6-12.4 ms against 12.5 ms; achieved
   66-74 fps against 80. Fixing this is a pixel/work question.
2. **The weapons (and possibly the hands) now judder** in a way the tester
   describes as exactly what the WORLD used to do before the VR-65 pose fix,
   and it shows whenever the frame rate is not hard-locked.

Problem 2 is new information, it is specific, and there is a mechanism that
predicts it exactly. It is treated first below because it is cheap to test and
because it may be a regression the pose fix introduced.

---

## 3. Hypothesis: the hands and weapons are placed at a pose the frame is not tagged with

### 3.1 The mechanism

The VR-65 fix made the submitted layer carry the pose the WORLD was rendered
from - two locate generations back (`[Pace] Lag=2`), because this engine has a
separate render thread and a delayed capture stage. The compositor reprojects the
whole submitted image from that tagged pose to the actual head pose at display
time.

**The hands and weapons do not participate in that scheme at all.**

- `openxr_input.cpp:537-540` locates every hand and aim space at
  **`predictedDisplayTime`** - the freshest, forward-predicted pose.
- A grep for `poseLag`, `viewsContent`, `viewsPrev2` or `pose_lag` across
  `src/game/dishonored/` and `openxr_input.cpp` returns **nothing**. The
  generation machinery lives entirely inside the runtime layer's submission path.
- The hand and weapon placement reads the loose `g_hmd*` globals
  (`fp_mesh.cpp:923`, `mesh_split.cpp:2109`), which is the same class of defect
  `STATUS.md` already records as open for the camera: *"both writers calculate
  from the loose globals and consume the coherent sample afterwards."*

So within one submitted frame the world is drawn at generation **N-2** and the
weapon is drawn at **predicted display time**, which is later than N. The
compositor then reprojects everything by `(display - N-2)`. That correction is
right for the world and **over-corrects the weapon by two generations plus the
prediction interval**. The weapon swims against the world by exactly the amount
the head moved in that span.

### 3.2 Why it appeared now

The lag default moved 1 -> 2 in the VR-65 fix. **That doubled the mismatch for
anything not drawn from the tagged generation, at the same moment it removed the
mismatch for the world.** A fault that was half as large and hidden underneath a
much worse world judder would become the most visible thing in the frame.

### 3.3 Predictions that would kill it

This is the section that matters; a hypothesis that cannot lose is not one.

1. **`vrpace lag 1` and `lag 0` should reduce the weapon judder monotonically
   while making the world judder worse.** If the weapon judder is unchanged
   across lag 0/1/2, the mechanism is wrong and this whole section is dead.
2. **The error should scale with HEAD angular velocity, not hand velocity** -
   because the reprojection is driven by head motion. If holding the head still
   and waving the controller reproduces it, the cause is elsewhere.
3. **It should be near-zero when the app makes every deadline** (no reprojection
   correction to apply) and grow as frames are missed - which is exactly the
   "when not at a hard locked framerate" the tester reports, and is the
   strongest existing evidence for it.
4. **The measured angle** between the pose the hands were placed from and the
   pose the frame was tagged with should be non-zero and should track head speed.
   Nothing currently measures this.

### 3.4 The shape of a fix, if it survives

Not chosen here; listed so the reviewer can rank them.

- **(a) Place the hands from the tagged generation.** Give the hand placement the
  same generation the layer will be tagged with, so the whole image is coherent
  and one reprojection is right for all of it. Most correct; needs the hand
  transform to be computed from a stored generation rather than the loose globals.
- **(b) Submit the hands as a separate layer** with their own pose. Correct by
  construction, but a quad or a second projection layer is a large change and
  the hands are drawn by the engine into the same image.
- **(c) Counter-rotate the hand placement** by the tagged-vs-fresh delta. Cheap,
  and a cancellation term is exactly what the VR-31 neck-cancel lever already
  does elsewhere in this project. Risk: it is a correction applied on top of a
  correction, and it will be wrong the moment the reprojection is not applied.

All three must ship default OFF with a live A/B, per project rule.

### 3.5 What this does NOT explain

The throughput deficit. A pose mismatch costs nothing. Problem 1 stands on its
own and needs its own work.

---

## 4. The throughput deficit

The honest position: **at 80 Hz the GPU is at 69-99 % of budget before anything
else is counted, and the app delivers 66-74 of 80.** Options, with what each
would prove:

| Option | What it would establish | What kills it |
|---|---|---|
| Resolution sweep at FIXED 80 Hz and fixed SSW setting, three points | Whether the GPU span is pixel-linear, and where the 12.5 ms line sits | A flat span - then the cost is CPU, submission or resolution-independent passes |
| One desktop `Present` per pair | Whether ~2.5 ms/pair is removable from the critical path | The wait moving into `StretchRect`, a fence, or the next draw |
| Run at 72 Hz instead of 80 | Whether a budget of 13.9 ms is enough, which the GPU numbers say it should be | Still missing slots - then the deficit is not the GPU span |
| Desktop mirror copies off | Whether the full-size snapshot/restore pair costs a measurable amount | No change in pair p99 |

**The 72 Hz option is listed first on cost.** It is a headset setting, needs no
code, and the measured 8.6-12.4 ms GPU span fits a 13.9 ms budget everywhere the
12.5 ms budget does not. If 72 Hz is smooth and 80 Hz is not, the deficit is
quantified without building anything.

---

## 5. Instrument work this needs

Carried forward from the review, still not done:

- **The mismatch angle for the hands** (3.3.4). Nothing measures it. This is the
  instrument that would make section 3 evidence rather than argument, and it is
  the same shape as the VR-65 orientation-difference instrument that settled the
  world judder - which suggests reusing that code rather than writing new.
- **D3D11 GPU timings** for the bridge blit and the XR copy. Still absent, so the
  bridge is unmeasured and cannot be ranked.
- **The GPU span still subtracts capture DMA from an interval that does not
  contain it**, and `idle(d3d9)` is a gap between markers.
- **Gap lines print `gpu pending span 0.0`** because resolution happens later;
  the zero is not evidence of zero GPU cost.

---

## 6. Questions for the reviewer

1. **Does the pose-generation mechanism in 3.1 hold?** Specifically: is
   reprojection applied to the whole submitted image such that content drawn from
   a different pose is over-corrected, and does the two-generation gap plus
   forward prediction produce an error of the magnitude a player would call
   judder at 66-74 fps?
2. **Is prediction 3.3.1 the right discriminator**, or does changing lag confound
   the world and the weapon so badly that the tester cannot separate them?
3. **Which fix shape** in 3.4 - and is (c) acceptable at all, or is a correction
   stacked on a correction a trap?
4. **The `out` share doubled** (1.3). Is that a real signal worth chasing, or an
   artefact of comparing two runs that differed in period, scene and SSW setting?
5. **Is 72 Hz a legitimate measurement** or just a smaller ask? It changes the
   budget without changing the work, which is unusually clean, but it does not
   identify a stage.
6. **The forced-SSW question in section 0**: is a runtime that reports the full
   period while reprojecting a thing that happens, and if so does any of the
   period-based reasoning survive?

---

## 7. Constraints

- Every new render lever default OFF with a live A/B toggle; fail soft.
- `src/core/` changes must default to pre-existing behaviour for the other games.
- The present thread owns every runtime call.
- The tester runs the game; diagnostics must ship enabled in the installed ini
  and be readable from `dishonored_vr.log`.
- One behavioural change per build; build on a snapshot confirmed good.
