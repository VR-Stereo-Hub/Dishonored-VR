# Status

## OPEN (2026-09-04): the GHOSTING - the cadence was NOT the cause, and that is now measured

**The report** (tester, on `alpha-264-ge2b84a80` at 2064x2208): "still ghosting flicker when turning
head and sometimes it hits harder than others".

### The resolution lever WORKS. Proven, because the tester doubted it and was right to

Every number moved, and the log names the mechanism at each step (`res: handed the game our
2064x2208@240 mode (slot 112)`, `CreateDevice - the game asked for 2064x2208`, `capture: 2064x2208`):

| | 2496x2688 | 2064x2208 |
|---|---|---|
| `perf: tick` | 9.2-9.9 ms | **8.6-8.9 ms** |
| submits/s (of 120 slots) | 88-115 | **114-117** |
| supply verdict | UNDER-SUBMITTING 0.73-0.96x | **MATCHED 0.95-0.97x** |
| slots per frame | 1.13 | **1.03-1.05** |
| interval sd | 1.3-3.8 ms | **0.75-1.36 ms** |
| cadence verdict | UNEVEN | **EVEN CADENCE** |
| capture lock | 0.5 ms | 0.0-0.1 ms |

### And the ghosting SURVIVED it. The cadence hypothesis is falsified

This is the point of having written the prediction down. The cadence is now locked - EVEN CADENCE,
1.03-1.05 slots per frame, sd under 1.4 ms, submits MATCHED to the slot rate - and **the doubled
edges are still there**. So the interference beat was real, was measured, was fixed, and **was not
what the tester is seeing.** Do not spend another session on pacing for this symptom.

What that leaves, cheapest first, all live commands with no relaunch:

1. **Virtual Desktop's Synchronous Spacewarp.** It manufactures intermediate frames by
   reprojecting, which is a literal ghost-frame generator, and it engages and disengages on its own
   - which is what "sometimes it hits harder than others" sounds like. Turn it OFF in the VD
   streamer settings and repeat the same head turn. **This is the first test and it is not ours.**
2. **`vrpace lag 0|1|2`** - which locate generation the layer's views are tagged with (ships at 1,
   "one back"). Infinite recorded the identical open suspect for the identical percept: "camera
   movement feels 'a bit jumpy' beyond the hitches - candidates: ... **the one-pair-stale
   content-pose attribution**". If the tag is a generation off the pose the frame was actually
   rendered from, the compositor reprojects by the wrong amount and the error changes every frame -
   doubled edges under rotation, worst when turning fastest.
3. **`vrpace ahead 0|1|2`** - the locate TIME (ships at 0). Already on the carried list as an
   unjudged judder item. The pair phase reads a steady **-43 ms** (we close ~5 display periods
   before the slot we asked for; on a Wi-Fi streaming runtime that is VDXR's pipeline depth, not a
   fault), so the reprojection is doing 40+ ms of extrapolation on every frame and the pose it
   extrapolates FROM has to be right.

### Not faults, checked so nobody re-checks them

- **The `CROPPED` capture bbox is a menu/loading artifact, not the resolution change.** Both runs
  show `100% x 52-53% (CROPPED)` early and then `100% x 100% (FULL)` once gameplay starts. Same
  shape at both sizes.
- **Not frame duplication or eye desync**: `mono/s=0 none/s=0-1`, `L/s == R/s == out/s / 2`,
  `ageL=1 ageR=0`, `aborts=0 staleEye 0` throughout gameplay.
- **Not Infinite's 30-second GC grid**: its spike class was
  `TimeBetweenPurgingPendingKillObjects=30`, killed A-B-A with 300. Our gaps have no 30 s
  periodicity - 0-13 s in bursts. Infinite's other signature (the streaming / level-visibility walk,
  triggered by view change and traversal) is the closer match and matches the head-turn trigger.
- **The hitches did not improve with resolution**: 62 gaps at 2496x2688, 82 at 2064x2208. They are a
  separate, later item from the ghosting.

### The 4K run: the lever is proven twice over, and SSW is OUT

**3840x4096 ran.** `perf: tick` **15.6-15.8 ms, 64 ticks/s** against 8.6-8.9 ms at 2064x2208 - the
lever moves the cost by 1.8x, exactly as pixels predict, so it is not inert and never was. Tester:
"4k does look way better". The capture lock also scales (0.0-0.1 ms -> 1.4-1.7 ms), which is the
readback and is ours.

**Virtual Desktop's SSW was already OFF** (tester confirmed), so suspect 1 of 3 is dead without a
run. The ghosting persists at every size tried, and the tester adds: **"it seems to get worse when
frame drops happen"**, and their own read is "some kind of eye submit desync, or the world geometry
isn't tracking the head tracking correctly".

**That read is now the leading hypothesis and it is testable.** The mod drives the game camera from
the head pose on the SCRIPT lane, and the compositor reprojects the submitted image using the pose
in the projection layer's views, located on the PRESENT lane. Those are two different samples of the
same head. If they disagree, the compositor's warp is wrong by the difference, and the error changes
every frame - which is doubled edges under rotation, and it grows when a frame is slow, which is
exactly "worse when frame drops happen". Nobody has ever measured that disagreement.

**The matched pair to measure it already exists in the code**: `head_track.cpp` records
`g_viewYawRad` (the yaw actually written to the engine) next to `g_injHmdYawSnap` (the HMD yaw it was
computed from) - its own comment says "Matched pair: this rotation was computed from THIS
g_hmdYaw". The instrument to build is: at submit, compare `g_injHmdYawSnap` for the frame that was
RENDERED against the yaw of the pose the layer is tagged with, and print the delta in degrees. It
can print the unwelcome answer - 0.0 deg means the two lanes agree and this hypothesis dies too.
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has been
judged.

### The tester's settings requests, with the REAL keys found (not guessed)

Asked for: killcam off, antialiasing FXAA, models high, maybe vsync on ("I just tried it and it was
maybe more smooth, but I'm not sure"). What the game's own config actually carries, in
`Documents\My Games\Dishonored\DishonoredGame\Config\`:

| ask | key | now | note |
|---|---|---|---|
| FXAA | `DishonoredEngine.ini [SystemSettings] iType_AntiAlias` | **1** (MLAA) | **2 = FXAA**, and the enum is documented in the file itself by the original devs (`EPpAa_None=0, EPpAa_Mlaa=1, EPpAa_Fxaa=2`) - measured, not guessed |
| vsync | same section, `UseVsync` | **False** | `True` is the ask; the tester is unsure it helped, so this one wants an A/B, not a default |
| models high | `SkeletalMeshLODBias`, `TextureForcedLODBias`, `DetailMode`, `Skeletal/StaticLODDistanceFactorMultiplier` | 0, 0, 2, 1, 1 | **already at the high end** (bias 0 = no reduction, DetailMode 2 = high). Nothing to change without inventing a value - do NOT write a guessed multiplier |
| killcam off | **NOT FOUND** | - | no killcam-shaped key in any of the 23 game inis (searched `Kill/Death/Assass/Slow/Cam` boolean keys). It may live in the save profile rather than an ini. Needs finding before it can be defaulted |

The AppCompat trap applies to all of these the same way it applies to the resolution: the bucket
AppCompat picks at startup overwrites `[SystemSettings]`, so anything written there must be written
to all four `AppCompatBucket*` sections too - `iType_AntiAlias` and `UseVsync` both already appear
in `DishonoredCompat.ini` with per-bucket values.

### Armed now: 3840x4096, purely to prove the lever to the eye

The tester asked to see the resolution do something visible, which is the right instinct and the
repo's own rule (confirm a lever moved something before believing its verdict). 3840x4096 keeps the
near-square eye aspect (0.9375 against 2064x2208's 0.9348) and is **3.4x the pixels** of the current
size, so if the lever were inert the tick would not move. Expect it to be slow - that IS the result.
Revert with `res 2064x2208f` (or `res 2496x2688f`) and relaunch.

## RESOLVED as a reading (2026-09-04): the submit stalls were NOT the game outrunning the headset

**The prediction written before the run held.** Submits landed at 88-115/s against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - and `endFrame mean` measured **0.08-1.99 ms**. xrEndFrame is
not blocking and is not throttling anything; the display slots are going unfilled. The reading that
opened this item ("the game produces frames faster than the headset can show them, and the runtime
absorbs the mismatch by blocking at submit") is **dead**, and it would still be alive if the period
had not been printed.

**What remains real:** 62 `perf: frame gap` lines, most still in `present-tail (xrEndFrame)` with
36-96 ms inside the submit call. Those are genuine, rare hitches, not pacing - and on a Wi-Fi
streaming runtime an 85 ms block in xrEndFrame is the encoder or the link, not our frame path. They
are worth a separate look AFTER the ghosting, because the ghosting is continuous and these are not.

**The tick, for whoever picks up "make it 120":** 9.2-9.9 ms, split almost evenly between the two
scene renders reentry needs - per pass roughly `present 1.8 ms + out/R 2.0 ms`, with our own capture
lock at 0.2-0.5 ms and endFrame at 0.0-0.6 ms. The mod is not the cost. Reaching 8.33 ms means
taking ~12 % off the game's own render, and the obvious dial is the 2496x2688 per-eye size.

## OPEN (2026-09-04): the submit stalls - 48 hitches of 44-156 ms, two thirds in xrEndFrame

**The report** (tester, 2026-09-04): "super laggy", and separately "this game is old enough that
it should run at hardlocked 120 fps, especially on a 4070 Ti Super". The headset is set to
**120 Hz**, so the budget is **8.33 ms** per frame.

**Throughput is NOT the problem, and that is the whole point of this item.** Measured on
`alpha-267-g4b9a0f3c-dirty`, Quest 3 via VDXR, 2496x2688 per eye, method reentry:

- **mean present interval 4.3-6.6 ms** (the heartbeat read GAME=138-233 fps across the run).
  That is comfortably inside the 8.33 ms budget, with 25-50 % headroom.
- **48 `perf: frame gap` lines**, gaps of **44-156 ms**. The worst is **36.3x the 4.3 ms mean** -
  at 120 Hz a 156 ms stall is 19 dropped frames in a row.

**Where the stalls sit**, from the `sat in:` field of those same 48 lines:

| phase | count |
|---|---|
| **`present-tail (xrEndFrame)`** | **31** |
| `out/idle (waiting for the game thread)` | 9 |
| `out/R (executing the frame)` | 3 |
| `game_tick` | 3 |
| the game's `Present` | 1 |
| `present-head (wait)` | 1 |

`flags:` on those lines read `reset=0 load=0 paceTimeouts=+0` almost throughout (one `+1`, one
`pairOpen=1`), so the pace lane is not timing out and the method is not re-arming. `vrpace ahead`
was at its shipped 0.

**Two thirds of the stalls are in the submit call.** So this is a pacing/submit problem, not a
rendering-cost one - which is the good news, because the fix is scheduling rather than cutting
quality.

### The gap that had to close first: the display period is now printed (session 13)

**It was not in the log.** `dvr::vr::display_period_ns()` existed in the runtime layer, was read by
the pace sync, and was PRINTED only inside the `PACE-BOUND` clause of the `perf: tick` line - so the
one run that most needed it (48 hitches, the wait at ~0, no `PACE-BOUND` line anywhere) is exactly
the run where it stayed invisible. "The game produces frames faster than the headset can show them,
and the runtime absorbs the mismatch by blocking at submit" was therefore an INFERENCE from a phase
NAME, and this project has spent whole sessions on inferences that read well and were wrong.

**What now prints** (built and lint-clean on `performance-fix`, **not yet run** - see below):

- **`stereo: rate`**, a new line on the 3 s stereo beat:
  `hmd=8.33 ms (120.0 Hz) slots/s=120.0 | presents/s=233 submits/s=116 (one xrEndFrame per pair) |
  endFrame mean=6.41 ms max=41.2 ms over 349 submits | <verdict>`.
  `submits/s` is a NEW counter: `xrEndFrame` calls from the present path, which under reentry is
  **one per PAIR** - it is the tick rate, not `out/s`. The endFrame mean and max are the submit's
  own cost, drained per window.
- **the verdict on that same line**, which is the whole point and can print the unwelcome answer:
  **OVER-SUBMITTING** (> 1.05x the slots) confirms the throttle reading and the lever becomes "stop
  producing frames nobody sees"; **MATCHED** (0.95-1.05x) means the endFrame MEAN is the pacing wait
  and is not a hitch, only its max is; **UNDER-SUBMITTING** (< 0.95x) says display slots are going
  unfilled, which falsifies the throttle reading outright and puts the cause upstream of the
  headset's cadence. A runtime that leaves `predictedDisplayPeriod` at 0 prints `hmd=UNKNOWN` and
  gets NO verdict - never read that as 0 Hz.
- **`perf: tick`** now opens with `[hmd 8.33 ms = 120.0 Hz, budget 8.33 ms/tick]` unconditionally,
  not only when pace-bound.
- **`perf: frame gap`** now ends its phase attribution with `= 19.0 display slots at 8.33 ms`. The
  "156 ms is 19 dropped frames" arithmetic in the table above was done by hand off the log; it is in
  the line now.
- **`status.json`**: `stereo.pair{displayPeriodMs, displayHz, endFrames, endFrameMeanMs,
  endFrameMaxMs}` and `perf{displayPeriodMs, displayHz}`. `game-cmd.ps1 "stereo status"` prints the
  same numbers live, without waiting for a beat.

**A prediction worth writing down before the run, because the arithmetic already argues against the
inference.** The report's own numbers are 4.3-6.6 ms mean PRESENT interval, and reentry submits one
frame per two presents - so submits/s should land at **76-116**, BELOW 120. If that holds, the line
reads UNDER-SUBMITTING and the "game outruns the headset" reading is dead: the headset would be
going hungry, not being over-fed, and the 31 present-tail stalls are genuine hitches inside
`xrEndFrame` rather than a throttle. The measurement is what settles it either way.

**Next step: a headset run on `performance-fix` with nothing else changed**, and the three lines
above out of the log. Nothing has been installed or launched for this change - the build is verified
only as compiling, linting clean and exporting the nine names.

### Not a fault, for the record

Stereo reads mono for roughly the first 6 seconds of a run (`2nd/s=0`), then latches to 103-108 and
holds - confirmed by the tester ("it is mono at the very beginning but it latches on and stays good
after a few seconds"). The `2nd/s=0` at the very END of a log is leaving gameplay, not a regression.

## FIXED (2026-09-03, headset, dev rig): all three flickers, in one chain

Branch `swapchain-one-picture-flicker`. Installed and judged as `alpha-253-g8441404f`. **The
tester's verdict after the third fix: every kind of flicker is gone.**

The three faults were nested - each fix exposed the next - and each was confirmed in the headset
and in the log before moving on.

### 1. The stale RIGHT eye (`1507bafc`)

`reentry.cpp`'s c5 invariant has two arms and they are **not equally trustworthy**. `inv=+1`
("pass 2 after pass 1") compares two draws with NO world tick between them: the step is exactly
`-ipd*scale` along the camera's right row by construction. `inv=-1` ("pass 1 after a still pass
2") is the **only arm that reasons across a world tick**, and holds solely while the player is
near still. A gently moving player - turning in place, decelerating, crouch-walk - parks the
tick's travel inside the `+-0.35*ipd` window (about `+-2.2 uu` at the measured 6.18) and the
fragile arm then names a genuine pass-2 present a pass 1. It did so **unconditionally, on a
streak of one**. One wrong `-1` writes the left swapchain twice and never writes the right.

The fragile arm now defers to the ring on a disagreement (a streak of three still earns the
override for either arm); neither arm invents a tag on an empty ring or over the `0` tag a
single gameplay draw pushes.

| | before | after |
|---|---|---|
| `STALE ? EYE` lines | 36 in 171 s | 1 in 238 s |
| one-picture pairs at sc | 14 of 39 (36 %) | 0-1 of 40 |
| `sc-target repeats` | 80 | 2 |
| `out/s` median | 117 | 212 |

`c5Held=36` counts the deferrals that did it, against `c5Agree=29210 c5Disagree=112`.

**Why no gate admitted to it**: `SceneDrawMaybeSecond` cannot skip pass 2 unlogged - `!doubleIt`
means pass 1 pushed no `-1` at all, the poison logs an `Error` at its one site and stands the
method down, and the forced skip IS the `forced=` field. Both tags were pushed every time; the
second `-1` was manufactured downstream. Every zero on the line was true.

### 2. The mono flick on the arms and weapon (`[Stereo] HoldUntagged`)

With the stale eye gone the tester saw a different flicker: "both arm models/weapons instead of
just the left handed crossbow", and it "felt slightly different". It was. `mono/s=1..4` in steady
gameplay, and a mono present is the same image in BOTH eyes, so its error scales with disparity -
near zero on distant geometry, **largest on the viewmodel at 30-50 cm**. Symmetric, hence both
arms, where the stale-eye fault was one-sided. 26 of 29 single-draw spells were the present-stall
guard; `c5Refused=72` was the rest.

`HoldUntagged=3` (ported from the parked branch) took `mono/s` 1-4 -> **0** and every other
flicker with it.

### 3. The black frame in both eyes (`8441404f`) - caused by the fix for 2

The hold immediately produced a new, subtler artifact: one black frame in both eyes, frequent.
**The whole layer assembly in `on_present_end` sits inside `if (backbuffer)`**, and
`backbuffer = frame`. A present that hands in no texture therefore reaches `xrEndFrame` with
`layerCount 0`, and a zero-layer frame gives the compositor nothing for that display slot -
black, both eyes, one frame. `HoldUntagged` made that path common: it holds a present by
returning false from the method, and `frame_hooks.cpp:181` passes the null to `on_present_end`
unconditionally.

The ported commit's own message claims "nothing is submitted; the compositor holds the previous
pair". **That is not true of this runtime**, and the commit was parked without a headset run that
would have caught it - it was taken as established behaviour instead of checked.

The guard now re-submits the layer the previous present used. Nothing is acquired or released on
such a present, so the swapchain images are untouched and the compositor genuinely re-shows the
previous pair; reprojecting it to the new display time is the runtime's job, and the
parked-session keepalive already re-submits that same snapshot. Measured over 126 s:
**`held=25 black=0`**, `STALE` 0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

### Levers and defaults

**`[Stereo] HoldUntagged=3` now SHIPS** (the user's call, 2026-09-03), joining the session-9
precedent of the headset-judged values being the defaults. It is a deliberate exception to the
default-OFF rule for render levers, made because the artifact it removes is a visible flicker on
the viewmodel and the black frame it used to expose is fixed at the runtime. `0` is the A/B and
the pre-41.1 behaviour; `stereo hold <n>` switches it live, `none/s` on the beat counts what it
held, and `zeroLayerHeld`/`zeroLayerBlack` in status.json say whether the guard covered them.

**Judged on ONE rig, for a few minutes.** A second headset that reports one-frame stalls or a
smeared weapon should try `stereo hold 1` and then `0`, and say which is better.

### The rig, and three traps that cost runs

Quest 3 via VDXR, 5120x1440@240 desktop, RTX 4070 Ti SUPER. `[Screen] RenderWidth/Height` in the
ini CANNOT affect the launch it is set on - DllMain reads only `dishonored_vr_launch.txt`, written
by the `res` seam word, so use `res 2496x2688` and never hand-edit the key. `bPauseOnLossOfFocus`
was TRUE in the game ini (now FALSE here): with it on, alt-tabbing to drive the command seam
pauses the thing being judged. VERIFICATION gotcha 17 bit 1 run in 3: state sticks at MENU in the
level, `2nd/s=0`, screen stays mono - open and close the pause menu.

**Check the build tag before reading any verdict out of a log.** The first "it's fixed!" here was
measured on a build that had been compiled but never installed - `build.ps1` had run, `install.ps1`
had not, so the game folder still held the previous DLL. The log banner names the build
(`build alpha-NNN-gHASH`) and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` settles
it in one command. A `-dirty` tag means the tree had uncommitted changes at build time and the
log cannot be traced to a commit: rebuild from a clean tree before handing a log to anyone.

## Current state (2026-09-04, session 14: the throttle reading is dead, the cadence is the suspect)

**This branch (`performance-fix`) carries two instruments and one lever, all default OFF or
log-only.** No render path changed. Session 13 added the rate measurement, it was run, and it killed
the hypothesis the branch was opened for - the submit was never throttling. Session 14 added the
cadence measurement, which points at the ghosting instead, and `[Pace] SyncHz`, which ships at 0.

What shipped: the `stereo: rate` beat line (hmd period and Hz, slots/s, presents/s, submits/s, the
pair interval's mean and sd, the submit's own mean and max cost, and TWO verdicts - supply and
evenness - either of which can print the unwelcome answer); the display period unconditionally on
`perf: tick`; the gap converted to display slots on `perf: frame gap`; `[Pace] SyncHz` as the
persisted form of `vrpace sync <hz>`; and all of it in `status.json` and `stereo status`. The two
OPEN sections at the top carry the readings and what they mean.

**Built, lint-clean, exports OK, and INSTALLED as `alpha-260-g958ab57a`** (RelWithDebInfo,
hash-checked against `build\src\RelWithDebInfo\d3d9.dll`, clean tag - no `-dirty`). It replaces
`alpha-259-g865f1bcd`, so the game folder no longer carries the crouch fix: `crouch-fix` is still
only PR #14 and this branch is cut from `VR-Main`. **Not launched, not run** - in the simulator or a
headset. Treat every number it prints as unseen until a run produces one.

The two logs that were in the game folder are archived to `D:\dvr-data\logs\s13-pre-install-*.log`
before the run overwrites them (rotation is one deep and there were already two).

**The crouch height rise is FIXED and headset-judged** (previous session): the 38.16 deep-crouch
capsule write moved the pawn 20.00 uu on every crouch and the camera's rate-limited catch-up
integrated the remainder. `[PosTrack] DeepCrouch` now defaults to 0. That work is on `crouch-fix`
and is **open as PR #14, not merged**. Its derivation and the five falsified suspects are in
`docs/dishonored/ENGINE_NOTES.md` on that branch.

**The DLL in the game folder is `alpha-262-g61130855`** (session 14's build - the cadence instrument and
`[Pace] SyncHz`), replacing the session-13 `alpha-260-g958ab57a` that produced the run above. Both
are the tip of THIS branch, built
RelWithDebInfo, installed and hash-checked against `build\src\RelWithDebInfo\d3d9.dll`. It
replaced `alpha-259-g865f1bcd` (the tip of `crouch-fix`), so **the deployed build no longer carries
the crouch fix** - `performance-fix` is cut from `VR-Main` and PR #14 is still unmerged. Nothing
diagnostic is armed either way: `[Hands] CrouchAB`, `CrouchBurst` and `[PosTrack] DeepCrouch` are
all default OFF, and the tester's `dishonored_vr.ini` and the game's `DishonoredCamera.ini` were
restored from backups after the crouch investigation; no diagnostic keys remain in either.

**Two findings from that session are deliberately unbundled and unfixed**, because neither has been
judged in a headset:

1. `LocPropFind` and `CrouchPropFind` sit below `if (!g_handMesh) return` in `ApplyHandToMesh`, and
   `[Mode] GamepadOnly=1` (the shipped default) clears `g_handMesh` - so **they have never run on a
   shipped build**, and the 38.24 eye clamp, which needs `g_actorLocFound`, has never run either.
   Reviving it is a real behaviour change and needs a headset verdict of its own.
2. The config line reporting physical crouch as "armed" when `[Mode] GamepadOnly=1` has already
   vetoed it. Cost a session's hypothesis once already.

## Previous state (2026-09-04, session 9: THE EYES ARE FIXED, root cause proven in the headset; the headset-judged values are the defaults)

**Merged to `native-stereo-rendering` (PR #7, 13 commits).** The per-eye render is correct on the
headset, at rate, with the tested configuration shipping as the defaults.

- **THE ROOT CAUSE, found and fixed and PROVEN**: the eye tags paired draws to presents by
  push/pop ORDER, and the order breaks wherever the game thread runs ahead of the render thread -
  a level load, a pause/resume, a re-arm - and spontaneously in gameplay every ~2 s on the
  tester's rig (131 skews in four minutes). Each break showed each eye the other's draw until the
  next one. That is the fault this project has chased since run 15 ("the eyes disagree", "90 % of
  the time, more at the beginning", "never correct after a load"). THE FIX: between a tick's two
  draws nothing moves the camera but the eye offset, so pass 2's camera sits exactly one IPD along
  the camera's right row from pass 1's; a present whose camera step reads that IS the second draw
  whatever the queue claims, and a disagreement streak realigns the queue. `[Stereo] C5Pair=1`.
  THE PROOF (headset, the user, 2026-09-04): unticking `c5 pairing` on the F10 EYES block and
  pausing brought the fault straight back (`swapped=24 of 25` pairs, the picture's parallax on the
  wrong side), ticking it back removed it (`swapped=0` for the rest of the run) - by eye and in the
  log, three times.
- **THE INSTRUMENT that found it ships**: the frame-identity trace (`core/gfx/frame_id`, `[Perf]
  FrameId=1 FrameIdEvery=8`, `frameid on|off|status|every N`, status.json `frameid{}`): one 64x64
  luma thumbnail per sampled present at the backbuffer as the capture found it, the shared slot,
  the eye texture and the swapchain image, read three presents later and never waited on, with the
  draw's camera position and right row on the same record. The `stereo: frameid` line prints, per
  left/right pair, the L-R difference per stage, the camera step and side check, the picture's own
  parallax shift and the first stage that reads as one picture. The F10 Display tab's **EYES
  block** shows the same numbers live and carries DUMP EYES, REARM 2, CAPTURE REINIT, PROJECTION
  OFF / AUTO, the c5-pairing tickbox and the trace tickbox.
- **PERFORMANCE: back at the headset's rate.** The trace read every present cost the tester's GPU
  1.5 ms per present (stage bb's `GetRenderTargetData` is a pipeline sync there) and the tick went
  13.9 -> 16.7 ms, 60/s under a 72 Hz headset; sampling one pair every 8 ticks removed it
  ("the fps is perfect now", 2026-09-04). The trace tickbox is off = no cost at all.
- **THE DEFAULTS ARE THE HEADSET-JUDGED VALUES** (a fresh ini now comes up where the testing left
  off): `[Stereo] Method=reentry Armed=1 C5Pair=1`, `[Camera] EyeField=0x330`, `[Neck] Mode=cancel`
  with the measured pivot (0.321 / 0.062 m), `[PosTrack] Scale=98 Lane=auto`, `[Tracking]
  HeightOffsetM=-0.090`, `[Screen] RenderWidth=2496 RenderHeight=2688 RenderFullscreen=1
  VirtualMode=1` (the Quest 3 through VDXR per-eye size, advertised so the game creates it - a
  fresh install used to render the game's own size and look soft), `[Device] Ex=1 Managed=shadow`,
  `[Capture] Mode=shared`, `[Perf] FrameId=1 FrameIdEvery=8`. Another headset: the F10 Display
  picker writes its size for the next launch; `res 0x0` asks for none.
- **Also fixed this session**: `dump eyes` writes a consecutive left/right pair and encodes it on a
  worker thread (the 640 ms present-thread stall used to re-arm the second draw, so every dump
  changed what it was taken to judge); the beat line's `presentTid` follows the presenting thread
  (it was latched at the first present, which at boot is the game thread's - a false lead in run
  17); pass-2 eye writes the camera seam refuses are counted (`p2write refused=`).

## Next steps (one paragraph per developer)

**The next developer session (performance)**: the ghosting A/B comes first and it needs no build -
`vrpace sync 60` against `vrpace sync off`, judged in the headset, with the `stereo: rate` line read
before and after (`UNEVEN CADENCE` should become `EVEN CADENCE: 2.00 display slots per frame`). Then
apply the rule the crouch item paid for five times over: confirm the lever actually MOVED the
cadence before believing either verdict - `vrpace sync` with no argument prints the delays it
imposed, and a null result with 0 delays means the gate never ran. If the cadence locks and the
ghosting stays, the cause is not ours and Virtual Desktop's own frame synthesis is next. Only after
that is the "make it 120" work worth starting, and it is a render-cost problem, not a pacing one:
9.2-9.9 ms per tick, almost all of it the game's two scene renders, with the per-eye size the dial.
The rare submit stalls are a separate, later item.

**The user (headset)**: installed as `alpha-264-ge2b84a80`, **and the render size is already set to
2064x2208** - it is armed for the next launch, so just start the game. The ghosting A/B, in this
order, and none of it is a frame limiter. **(1) Play and read the log.** Pass condition:
`perf: tick` mean under 8.33 ms and `stereo: rate` reading `EVEN CADENCE: 1.00 display slots per
frame`; `res status` says whether the engine honoured the ask. To go back, `res 2496x2688f` and
relaunch. **(2) If the tick still will not fit**, put the
headset at 90 Hz and go back to 2496x2688 - an 11.1 ms budget against a 9.4 ms tick locks with room,
at full sharpness. **(3) If the cadence locks and the doubled edges survive**, the beat was not the
cause: turn off Virtual Desktop's Synchronous Spacewarp and repeat, because that is the other thing
manufacturing frames. `crouch-fix` (PR #14) is ready for your merge decision whenever you want it.
Still open from earlier sessions: (1) the PITCH PIVOT with `[Neck] Mode=cancel` against `off` and
`add`; (2) WORLD SCALE and eye height at `[PosTrack] Scale=98` / `HeightOffsetM=-0.090`.

## Blockers

- **The ini version rewrite wipes a tuned ini** (`config.cpp` carries three keys over): the
  session-8 and session-9 keys ship without a version bump. A key-preserving rewrite is a
  separate change.
- **WM_CLOSE leaves a stuck `Dishonored.exe`** (session 5): close a healthy game with
  `Stop-Process`; a menu quit is clean on the Quest (run 15).
- **The walk-in on the simulator is by hand**: Return x4 with 28 s waits reached the level on four
  of six launches this session; otherwise the mod's menu flag sticks (VERIFICATION gotcha 17) and
  an Escape pair clears it. Look at an `xrsim-shot` before trusting a state line.

## Session log

### 2026-09-04 - session 14: the throttle reading dies, the cadence is named

The session-13 instrument was installed and run (`alpha-260-g958ab57a`, Quest 3 / VDXR, 120 Hz) and
it did its job in both directions.

**It killed the hypothesis it was built to test.** submits/s 88-115 against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - with `endFrame mean` at 0.08-1.99 ms. The submit is not
blocking and never was; the display slots were going unfilled. Written prediction, held.

**It pointed at the real one.** The tester's bigger complaint on that run was ghosting - doubled
edges on world geometry when turning the head. `perf: tick` reads 9.2-9.9 ms against an 8.33 ms
slot, and `pacetrace.log`'s `TRACE pairs` reads interval mean 8.6-11.8 ms with **sd 1.3-9.6 ms** and
`waitGate 3-64 ms/s` - the game free-runs at ~1.13 display slots per frame, unevenly, so consecutive
frames are held for different numbers of slots. That is the interference beat `pace_sync_gate()` was
written for in the BioShock lineage, and its own comment predicted this shape of fault.

Shipped: the pair interval mean/sd and an `UNEVEN CADENCE` / `EVEN CADENCE` verdict with
slots-per-frame on the `stereo: rate` line (the numbers existed only in `pacetrace.log` at trace
level, which is why no one had seen them), and `[Pace] SyncHz` - the persisted form of
`vrpace sync <hz>`, shipping OFF, refusing an out-of-range value with the number it read.

**The proposed fix was wrong and was corrected in the same session.** `vrpace sync 60` locks the
cadence but 60 is too low for VR, and the user said so. Infinite's own numbers on the same runtime
say why no limiter is needed: it ran 80 pairs/s == its 80 Hz refresh with sd 0.3-1.0 ms, LOCKED, on
the same two-draw method - because its render cost fit inside its period. Ours does not (9.4 ms into
8.33), and we are also at 47 % more pixels per eye than Infinite's native. So the lever is the gap
between tick and period, from either end: `res 2064x2208f` (the Quest 3 panel's own size) or a 90 Hz
headset. Still a prediction; nobody has judged either in a headset.

Installed as `alpha-262-g61130855` (clean tag); builds, lint clean, exports OK.

### 2026-09-04 - session 13: the display period is measured, not inferred

The branch's first code. **One commit, instrument only** - no lever, no default, no render path.
`dvr::vr::display_period_ns()` had existed since session 42 of the BioShock lineage and printed in
exactly one place, inside the `PACE-BOUND` clause of `perf: tick`, which is a clause the hitching
run never triggered. So the headset's rate was absent from the one log that needed it.

Added: an `xrEndFrame` counter with its own cost (count, cumulative sum, per-window max) at the
single present-path submit site, and the display period, both published through `PairProbe`; the
`stereo: rate` beat line built on them, with a three-way verdict (OVER-SUBMITTING / MATCHED /
UNDER-SUBMITTING, plus UNKNOWN when the runtime leaves the period at 0); the period unconditionally
on `perf: tick`; the gap in display slots on `perf: frame gap`; the same numbers in `status.json`
and in `stereo status`.

**A prediction is on the record before the run** (OPEN section): the report's 4.3-6.6 ms present
interval, halved by reentry's one-submit-per-pair, puts submits at 76-116/s against 120 slots/s -
UNDER-SUBMITTING, which would falsify the throttle reading outright. The line was written so it can
say that.

**Verified as: builds, `lint: clean`, `exports OK: 9 names`, installed and hash-checked
(`alpha-260-g958ab57a`).** Not launched, not run in the simulator or a headset - every number the
new lines print is still unseen.

What was already measured before this session and should not be re-derived:

- The rig has **headroom**: mean present interval 4.3-6.6 ms against an 8.33 ms budget at 120 Hz.
  The complaint is not framerate.
- **48 hitches of 44-156 ms**, and **31 of 48 sat in `present-tail (xrEndFrame)`** - the submit
  call. `paceTimeouts` and `reset` were 0 throughout, so the pace lane is not timing out.
- **The display period is not logged**, so the obvious reading (the game outruns the headset and
  blocks at submit) is an inference. Measuring it is step one.
- Mono for the first ~6 s of a run then latching to 103-108 `2nd/s` is NORMAL and tester-confirmed;
  do not chase it.

### 2026-09-04 - session 13: the stale RIGHT eye, read out of the code, then confirmed

Branch `swapchain-one-picture-flicker`, 6 commits, one headset run at the end.

The hand-off pointed at `SceneDrawMaybeSecond`'s unlogged early returns. They are a dead end,
and ruling them out is what found the fault: `!doubleIt` means pass 1 pushed no `-1` at all,
the poison logs an `Error` at its one site and stands the method down, and the forced skip IS
the `forced=` field. So the game side pushed both tags on all 20 stale submits, every zero on
the line was true, and the second `-1` had to be manufactured downstream of it.

It was, in `reentry.cpp`'s c5 pairing block, whose two arms are not equally trustworthy. Full
reasoning in the FIXED section above and two entries in ARCHITECTURE's decision log. The
asymmetry was the tell: only the `-1` arm can misfire, and only a wrong `-1` strands the right
eye. `L=0 R=20` is that, arithmetically. **Confirmed in the headset**: 36 stale lines in 171 s
became 1 in 238 s, sc one-picture 36 % became 0-1 of 40, `sc-target repeats` 80 became 2, and
`c5Held=36` counts the deferrals that did it.

The second finding is about the instrument, and it is the more expensive one. The STALE line
carried the game side's gates and the runtime's failures but **not one counter from the method
between them**, and its owner string mapped `abortLeft` straight to "the game side skipped pass
2" - a cause it never measured. Nine logged instances, three readers, all sent to the wrong
file. Worse, the block's unconditional `tagged = true` made `method untagged presents` read 0
*because* a tag had been invented: the counter did not merely miss the fault, it denied it.

**And a process trap that cost a run**: the first "it's fixed" verdict came from a run of a
build that did not contain the fix. `build.ps1` had run, `install.ps1` had not. The log banner
and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` catch it in one command, and
that check now leads the rig notes.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring (streak 3 still overrides); no invented tags on an empty or 0 ring; `sameEyePushed` counted at the push site; the c5 counters and a corrected owner string on the STALE line |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0, `stereo hold <n>` - now the lever for the residual mono flick |
| `539b9391` | ported: the `pair geom` separation-angle line, every 2 s |
| `9620d437` | ported: F2 stamps the fault marker, eyes-free |
| `00b833a8` | ported: the `res` seam writes its ini path with a real separator |

**All three fixed, each confirmed in the headset before moving on.** The chain: the c5 fix
exposed a mono flick on the arms (disparity is largest on the viewmodel, so a mono present shows
there and nowhere else); `HoldUntagged=3` removed that and exposed a black frame in both eyes;
the black frame was a zero-layer `xrEndFrame`, because the layer assembly sits inside
`if (backbuffer)` and a held present hands in no texture. Final run: `held=25 black=0`, `STALE`
0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring; no invented tags; `sameEyePushed` and the c5 counters on the STALE line, and a corrected owner string |
| `8441404f` | a zero-layer xrEndFrame re-submits the previous layer; `zeroLayerHeld`/`zeroLayerBlack` counted and logged |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0 - the lever for the mono flick |
| `539b9391` `9620d437` `00b833a8` | ported: `pair geom`, the F2 fault marker, the `res` seam separator fix |

**Two process lessons, both paid for this session.** A parked commit's message is not evidence:
`HoldUntagged`'s claim that "the compositor holds the previous pair" was false against this
runtime and had never been run in a headset, and taking it at face value is what put the black
flicker in front of the tester. And check the build tag before reading a verdict out of a log -
the first "it's fixed" here was measured on a build that was compiled but never installed.

### 2026-09-04 - session 9: the eyes - the trace, the swap, the fix, the proof

Branch `claude/dishonored-vr-both-eyes-same-659cb5` -> PR #7, 13 commits. Runs on the dev PC
(simulator lane, RTX 4060, 2496x2688 VirtualMode, the sewers, shipped defaults; logs in
`D:\dvr-data\logs\45-run*.log`) and the user's Quest 3 through VirtualDesktopXR:

| Run | What | Result |
|---|---|---|
| 01 | the trace and the words, first build | `stereo: frameid` pairs from the arming: L-R 4.1 at bb/slot/out, floor 1.5, c5 6.17, busy 0; `reentry rearm 2` -> SINGLE x2 then DOUBLE; `capture reinit` -> REBUILT, no STALE; `dump eyes` queued + written off-thread, no gap, no LOADING; the sc stage empty (an ordering bug) |
| 02 | the sc stage, the side check | sc reads (4.7-5.0); **the side flipped across `reentry rearm 2`** and within a second of the first arming; the ring overflowed 363 times in the menu |
| 03 | the 0-tag push + the c5 pairing (first form), the A/B | side ok from the first pair; `reentry c5pair off`: the side flipped on its own twice in 25 s, `untagged 16-19` per window; on: no flips |
| 04 | the invariant as the pairing, the picture shift | side ok + shift -1 px on every pair, P1 == P2, untagged 0-1; `reentry.xrs` 11/11 |
| 05 | the drain to the next expected tag | side ok from the first pair across a `stereo mono` -> `reentry` switch and a rearm; 0 ring drops |
| 06 | the F10 EYES block | the overlay renders the readout and the buttons in the headset's own view (`xrsim-shot`) |
| 07 | **HEADSET (the user)**, the session's build | the eyes RIGHT from the load and after every button; `side ok` / `SWAPPED=0` on every pair, c5 6.11, shift negative, L-R 3-14 (one picture = 1.5); the ring skewed 131 times in ~4 min - the old swaps, absorbed; the trace read every present cost 1.5 ms GPU idle per present, tick 16.7 ms (60/s under 72 Hz) |
| 08 | **HEADSET (the user)**, the sampled trace + the A/B | "the fps is perfect now"; `c5 pairing` OFF + pause/resume -> the fault returns (`swapped=24 of 25`, then 12 of 12, the picture agreeing), ON -> `swapped=0` for the rest of the run. **The root cause is proven.** |

### 2026-09-03 - session 8: performance - the tick budget, the census, the 9Ex device, the shared capture

Branch `claude/dishonored-vr-perf-9f4b10`, 20 commits. Runs on the dev PC (simulator lane, RTX 4060,
2496x2688 VirtualMode, logs in `D:\dvr-data\logs\44-run*.log`):

| Run | What | Result |
|---|---|---|
| 01 | the tick budget, sync / deferred / off | stereo sync: tick 46 ms (21/s), capture 17-21 ms per present of which lock 9-13, GPU dma 15.5-16.8 vs 3D 4.8; deferred: 36 ms (27/s), lock 0, dma 10.4; off: 93 presents/s pace-bound; the marker 1 BeginScene per present, 0 late GPU reads; `mark` and the gap line print; `reentry.xrs` 11/11 |
| 02 | the creation census | 8060 of 8120 creations MANAGED (398 MB), READONLY texture locks 10598, no AUTOGENMIPMAP; the shadow route decided |
| 03 | `[Device] Ex=1 Managed=shadow` | `CreateDeviceEx -> 0x0`, IS 9Ex, `shared surface AVAILABLE`; 5240 twins, 65552 updates, 0 failures; the sewers intact; shared (one slot) 0.2 ms per present |
| 04 | the fenced two-slot shared capture, stereo | SharedWait=1: tick 13.3 ms (75/s), lock = the 3.6 ms fence wait, dma 0.2; SharedWait=0: 11.1 ms (90/s) PACE-BOUND; `reentry.xrs` 11/11; hammer 0 stale over 5 cycles; the frame intact |
| 05 | the final build | deferred default 27.7/s; `capture mode off` with 0 STALE lines (the no-frame fix); `focus lose 2500` / `focus regain`: `eaten=0`, 0 stale; `reentry.xrs` 11/11 |
| 14a | **HEADSET (the user)**, Ex=0, deferred | 30-33 ticks/s at 2496x2688 (dma 10.9 ms per present), 50 gap lines, no attack freeze felt |
| 14b | **HEADSET (the user)**, Ex=1 | 9Ex device up, shared AVAILABLE but the capture stayed deferred (30 ticks/s); after repeated quickloads the twin map filled with tombstones and the game crashed in D3D9 (minidump `dvr_20260903_212436.dmp`) |
| 06 | the tombstone fix, Ex=1 + shared, 3 quickloads | 2324 live, 1984 tombstones reused of 32768, 0 failures, the game alive; 90 ticks/s pace-bound |
| 15 | **HEADSET (the user)**, Ex=1 + shared | "performance is pretty good"; the eyes disagree "90 % of the time, more at the beginning": 0 STALE, 0 tag mismatches, pairs one IPD apart, 32 pause/resumes each with a 1-1.5 s flat spell |
| 07 | the read fence + the menu resume, shipped defaults | `readWaits` 14 in the run (the race was real); hammer 10 cycles 0 stale, `view live at once` x11; `reentry.xrs` 11/11 |

### 2026-09-03 - session 7: the four headset faults and the picker, on the simulator

Branch `claude/dishonored-vr-stereo-polish-449d43`, 15 commits. Runs (simulator lane, logs in
`D:\dvr-data\logs\43-run*.log`; the run-40 headset log archived as `42-run40-quest3-verdict.log`):

| Run | What | Result |
|---|---|---|
| 01 | commits 0-1d | `Method=mono applied after the game side registered`; the gate decision logs its reason; `reentry.xrs` 11/11, `stale-eye.xrs` 18/18; hammer 10 cycles PASS, ages L=1 R=0 |
| 02 | `reentry skip2 120` | strict off: sim stale 0 -> 2, mono +62, `STALE R EYE` (owner first "unknown", then the game side); strict on: stale unchanged, 37 fallbacks to mono |
| 03 | the phase + ahead | runtime clock extension on the sim; phase +58 ms mean (synthetic); `vrpace ahead 1` logs and locates; hammer 5 PASS |
| 04 | pitchtest x3 | engine neck 0.321/0.062 m (cons 0.3 uu); the arc reached c5 on top of it; `neck cancel` -> travel < 0.5 uu; the picture agrees |
| 05 | Armed + console | park/re-arm on the seam correct; the first console word overflowed the game thread's stack (the hook re-entered) |
| 06 | the guard, boot | `Method=reentry Armed=1 -> active reentry` before the first present; `setres 2560x1440f`/`1600x900w` dispatch, empty reply, no Reset: INERT |
| 07-08 | the ini route | 2560x1440 fullscreen in every ini place -> `CreateDevice 1920x1080 windowed=1`: the ini is inert |
| 09 | the command line | `-ResX=2560 -ResY=1440 -FullScreen` via 3 import slots -> `CreateDevice 2560x1440 windowed=0`, capture and swapchains followed |
| 10-11 | 2496x2688, no VirtualMode | the game asked the mode list, fell back to 2560x1440 (a harness launch had restored the mod ini; the launch file carries the token now) |
| 12 | **2496x2688 with VirtualMode** | our mode handed at slot 123; `CreateDevice 2496x2688 windowed=1`; `res: HONOURED`; hfov 108 deg; both eyes 77 % non-black in the sewers; the frame complete; readback 18-20 ms/present |
| 13a | **HEADSET (the user)**: 2560x1440 fullscreen asked | Reset 2560x1440 twice then 2508x1411 windowed=1 (the game's own fallback); the run sat in menus, stereo never armed (`state` skips); readback 5.3-5.8 ms/present |
| 13b | **HEADSET (the user)**: 2496x2688 VirtualMode | `CreateDevice 2496x2688 windowed=1`, HONOURED, "pretty sharp"; `neck cancel` right; stereo L/s=R/s=16-28, ticks 28/s, readback 13-15 ms/present; one `STALE L EYE` (age 567) at a FOCUSED regain; the desync still seen on load; judder unjudgeable |

### 2026-09-03 - session 6: S2b - the capture cost, the lanes, the root, the second draw

Branch `claude/s2b-stereo-scene-draw-a341c5`, seven commits on `VR-Main` (24b22390): the
capture cost measured and the modes, the pipelined deferred capture, positional tracking on
the camera seam, the projection claim and the FOV handoff with the state-gate fixes, the
root derivation and the second draw, the docs. Runs on the dev PC (simulator lane, logs in
`D:\dvr-data\logs\42-run*.log`):

| Run | What | Result |
|---|---|---|
| 16 | capture modes | probe: shared REFUSED; sync 5.4 ms, deferred (first form) 5.2 ms: no gain; `mono.xrs` PASS both |
| 17 | user-memory surface | REFUSED (D3DERR_INVALIDCALL), fell back to sync |
| 18 | the lock split | sync: lock 2.4-3.1 ms, copy 0.7, upload 1.5; deferred first form: the lock still waits |
| 19 | deferred pipelined | lock 0, total 2.25-2.4 ms; `mono.xrs` PASS |
| 20 | postest | camera lane HONOURED on all axes - on the attract camera (the state mislabel found in run 21) |
| 21 | projection on the mono screen | two views, sensor 137, claim readback; the pictures were the title screen, then the loading screen (DISCARDED there), then the sewers: eyetest 120/120, postest +30.0 in real gameplay |
| 22-24 | the state gate | menu/cine tracking hoisted, main-menu flag, LOADING state, the cinematic latch cleared on a new pawn: title MENU -> quad, load LOADING -> quad, level GAMEPLAY -> projection |
| 25 | 1440x1440 | the game stayed 1920x1080 with ResX/ResY=1440 in both ini places; second aspect open |
| 26 | census + scrapes | PVR from one site once per present; render thread presents; the draw chain to the HUD PostRender |
| 27 | tick chain + probes | both chains under UGameEngine::Tick; the root 0x5fc5b0 named from the bytes at 0x6330da; pe-xref confirms every edge |
| 28 | first light | pulse: 3 second draws at 218-414 us, presents +1 each; `stereo reentry`: L/s 54 R/s 53, pair c5 travel 6.17 uu, no fault; the ring cleared every few seconds |
| 29 | the ring fix + soak | L/s 52 R/s 52 mono 0, ringCleared 0, 90 s clean; `reentry.xrs` 11/11 |
| 30 | **the headset (Quest 3, VDXR, the user)** | the doubling ran (draws 54 = 2nd 54, presents 108, pair 6.08 uu) but L/s=36 R/s=54 mono/s=18: left tags dropped by the position check while walking -> both frames in both eyes; lean reversed, a second motion on pitch (the head's displacement and roll not driven under the projection layer) |
| 31 | the fixes, sim | tags never dropped: L/s 53 R/s 53 mono 0; `reentry.xrs` 11/11; the lean under projection read 13.7 uu for 30 cm (the reference had crept) |
| 32 | the stable reference | 30 cm -> +29.4 uu held for 10 s, crouch/forward signs right, postest HONOURED; CINEMATIC stuck across the load (the pawn latched before the title toggle) |
| 33 | the cinematic latch | `cine: latch cleared - leaving the main menu`, LOADING -> GAMEPLAY, L/s 53 R/s 53 mono 0 |
| 34 | HEADSET (the user) | stereo good; tilt and lean reversed in all four directions, also with `stereo projection on` |
| 35 | the lane picture test | 2 m right / 2 m up on both lanes: the camera lane MIRRORED the vp lane on both axes - the field's sign |
| 36 | sign +1 | both axes match the vp lane by picture; eyetest HONOURED 119/120 (c5 -99.2 for +100), postest HONOURED both axes, L/s 52 R/s 51 |
| 37 | roll by picture | roll write lands (incoming = wrote); +20 right-ear-down leaned the verticals RIGHT - reversed |
| 38 | roll negated | +20 leans left, -20 right; forward axis matches the vp lane; pause/resume re-pairs cleanly (L/s = R/s, mono 0) |
| 39 | the verdict logger | `gameplay verdict: FALSE (menuOpen) ... -> the head-locked quad` 30 ms ahead of the runtime's own line |
| 40 | **HEADSET (the user), the verdict** | PASS: stereo depth, tilt, lean, look, crouch all correct. Open: the arming glitch (right eye), judder on fast movement, the pitch pivot behind the camera, and the F10 resolution picker + arming tickbox |

### 2026-09-02 - session 5: the state as session 5 left it (archived)

**The render is restarted on a native D3D9 game.** The DXVK fork, the side-by-side present
pipeline, the 4032x2268 window machinery, the OpenVR backend and the mod's own OpenXR
loader/pace thread/input are removed (one commit each, so `git revert` restores one piece;
history keeps the fork under the `dxvk-*` tags). The BioShock trilogy mod's OpenXR runtime
layer is the single backend (`core/vr/openxr_runtime`, verbatim behind two D3D9 seams: the
device provider and the frame texture), the static Khronos loader is linked into `d3d9.dll`,
and SteamVR rigs go through the bundled `dvr_steamvr32.dll` shim. Stereo is a SEAM with named
methods (`core/gfx/stereo.h`: `[Stereo] Method=mono|aer|reentry`, `stereo <name>` live):
the mono screen (rung 1) works, `aer` and `reentry` are registered design stubs with their
notes. The per-eye camera seam (`game/dishonored/camera`) carries rotation (measured), FOV
(measured) and the lateral eye offset (unmeasured, with the `camera eyetest` instrument).
Version 41.0.0, `[Meta] Version=10`. ARCHITECTURE and ROADMAP (S0-S3) describe it.

**Verified on the dev PC (the game IS installed here, `D:\SteamLibrary`), simulator lane**,
build `g4fb67333` and later, 2026-09-02 evening, eight runs:

- `xrsim-selftest.ps1` PASS; `xrsim-launch.ps1 -ViaSteam` reaches `xr: instance created on
  runtime 'dvr-xrsim'`, `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, session FOCUSED,
  `xr: first frame submitted to the headset (1600x900 quad)`, frames advancing at the sim's
  90 Hz (`stereo: beat method=mono out/s=90`).
- `status.json`: `state GAMEPLAY` (the game auto-continues into the last save), `stereo.method
  mono`, `framesOut` advancing, capture bbox `100% x 100%`, 97% non-black, `camera.c5ok true`.
- `stereo aer` / `stereo reentry` refuse with their note and mono keeps running; `stereo mono`
  is a no-op; `camera status` prints.
- `xrsim-shot`: a quad layer whose SOURCE reads 97.2% non-black in BOTH views with a full bbox;
  the composite reads L 37.95% / R 37.98% (world-locked quad, run 7) then L 16.4% / R 16.3%
  (head-locked quad, run 8), no `COMPOSITOR fault` / `APP fault` line. The session-4 black
  left eye did not reproduce; the simulator now attributes it if it does.
- `dump frame` writes `capture_*.bmp` (5.7 MB) and `eye_*_mono.png`; `soak.ps1 -Minutes 3`
  exit 0 (PASS, no wedge, no dumps); the crash file carries the run headers.
- `camera eyetest 100` in gameplay: run 7 wrote nothing (the lever off = no camera revalidation;
  fixed), run 8 wrote all six candidates and measured a CONSTANT offset between the draw's c5
  and each field (+6620 uu for 0x80/0x90/0xc4, +14140 uu for 0x330/0x350/0x374 along right):
  the fields are not c5's quantity in c5's frame, so the measure was redesigned around a
  per-candidate c5 baseline (commit `cf9ec6f2`). Runs 10-11 (after the stuck process cleared
  on its own, no reboot): **camera+0x330 HONOURED 119/120** (+99.2 uu of the asked +100; it
  holds -c5 exactly, so the write is negated), the other five DISCARDED. The eye-offset write
  point is measured; `[Camera] EyeField=0x330` is the default (ENGINE_NOTES has the table).
- `mono.xrs` PASS (both eyes 12.9%, equal bboxes, no fault line) and the head-lock pair on the
  fixed simulator: the composite bbox is IDENTICAL at yaw 0 and yaw 30 (run 9).

**Found on the way** (each fixed in its own commit, all measured, none guessed):

1. The game calls `Direct3DCreate9` twice; the second `init_instance` failed with
   `XR_ERROR_LIMIT_REACHED` and the fallback chain declared VR off (guarded).
2. The handoff's trap 6 is real: a DIRECT exe launch crashes at the main menu
   (`Dishonored.exe+0x60907e` reading NULL, thread "other", right after
   `DisGFxMoviePlayerMainMenu Start`); a Steam launch survives it. `xrsim-launch.ps1 -ViaSteam`
   exists for this and is the only way to run the simulator with the game here.
3. The agent's shell on this PC VIRTUALIZES writes under the user profile: files the harness
   wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) existed for the
   shell and a game it launched directly, and not for a game launched through Steam (its
   listing held only game-written entries; a WMI-created `dir` agreed). `[Paths] DataDir=` in the
   ini and `DVR_DATA_DIR` for the scripts point both at `D:\dvr-data`; the simulator takes its
   state dir from the manifest's directory (VERIFICATION gotcha 14).
4. The loader's property store beats the environment: `init_instance` hands `[VR]
   XrRuntimeJson` to `xrInitializeLoaderKHR` (XR_EXT_loader_init_properties) as well, and logs
   whether the manifest is readable and its library loads.
5. The config's version rewrite dropped `[VR] XrRuntimeJson`; it now carries `XrRuntimeJson`,
   `Runtime` and `DataDir` over.
6. The fresh ini armed `FovLever=130` (the side-by-side value) and wrote it 600 times per 3 s
   into a 90-deg camera; the lever ships off on the mono screen.
7. The mono quad sat in BioShock's world-locked LOCAL space; it is head-locked now
   (`[Screen] HeadLocked=1`).
8. The SIMULATOR composited quads in the wrong place (60 px outward per eye at yaw 0, 460 px
   of swing at yaw 30): the cbuffer matrix was read column-major and the view matrix's rotation
   block was transposed. Fixed (`d43eea11`), selftest PASS, and the re-measure passed (run 9:
   identical bboxes at yaw 0 and 30). BioShock's eye legs never saw it (projection layers
   rotate rays in the shader).
10. The c5 capture only caught an upload STARTING at register 5; after the two device Resets
   a level load brings, the engine batches it into a c0 x128 block and the seam saw no c5 for
   a run (run 9). Any block covering c5 feeds it now (`dd10da09`).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Headset: verified** (run 13, 2026-09-03): Quest 3 through VirtualDesktopXR, the game on the
head-locked screen in both eyes, head tracking and the gamepad working. The quit crashed on
that run (the 38.79 class: VD's thread through a freed d3d11 pointer after PreExit with the
session still open). The handler had sat inside the motion-aim block since 38.79 and never
ran under GamepadOnly; hoisted, it closes the session from PreExit and the third quit (run
15) was clean: `shutdown: game PreExit`, `xr: session teardown`, `instance destroyed`,
`proxy unloading`, no exception.

**Not verified**: the SteamVR shim with
Dishonored; `apply_eye_offset` driving a real per-eye render (no method asks for an eye yet);
`head_track`/`pad_bridge` as real modules (deferred, S1).

#### Session 5 next steps (superseded by the list above)

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini carries
`[Paths] DataDir=D:\dvr-data`), foreground the window, `tools\xrsim-run.ps1 -Path
tools\xrsim\mono.xrs -Dir D:\dvr-data\xrsim`. Close the game with Stop-Process while it is
healthy, never with WM_CLOSE (blocker below). Copy `dishonored_vr.log` out before every
relaunch. The eyetest is done: the eye offset writes into camera+0x330 in negated form
(`camera::apply_eye_offset`); `camera eyetest 100` re-measures it on any build.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The eye field is measured
(0x330), so the method only has to alternate `eye_for_next_frame()` and tag each present; the
seam writes the offset on the script lane. Acceptance: `stereo aer`
accepted, the beat line `L/s == R/s == out/s / 2`, `stereo.xrs`, `eye-check.ps1` legs 0-5, the
runtime's pair probe clean.

**Developer B (SequentialReentry, S2b)**: read `core/gfx/reentry.cpp`. Task one is the
scene-draw root (caller census at `ApplyHeadToViewRotation`, live stack scrape, identify the
pass by making it MOVE with the eyetest as the mover); every address to `patterns.h` with its
derivation in ENGINE_NOTES; the second call deny-by-default and SEH-guarded.

**The user**: the headset run on Quest 3 via VDXR: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active (SteamVR not running), expect the game on a head-locked
screen in both eyes, head rotation turning the view, the gamepad working; F10 for the screen
size; send `dishonored_vr.log`. Quit through the game's own menu and report whether the process
lingers.

#### Session 5 blockers (superseded)

- **WM_CLOSE leaves a stuck `Dishonored.exe`** (one thread, unkillable, holds `d3d9.dll` and
  the build's `dvr_xrsim32.dll`, Steam refuses a relaunch). Pid 13452 cleared on its own after
  about an hour, no reboot. Until the quit path is understood, close a healthy game with
  `Stop-Process`, which works.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The headset run needs the user.

### 2026-09-02 - session 5: the native-stereo foundation (41.0)

The decision (docs/ARCHITECTURE.md decision log, session 5): four headset sessions showed the
DXVK side-by-side design cannot be tuned; the game renders natively again and stereo is rebuilt
as a ladder of methods on one seam, two developers taking rungs 2 and 3. One PR
(`claude/native-stereo-foundation-77e2b6` -> `VR-Main`), 27 commits: seven removals, the
static loader, the runtime layer, the shim, the stereo seam + mono screen, the camera seam +
eyetest, the stubs, the simulator instruments, the harness, the docs, then the fixes the
first runs demanded (above, "Found on the way").

Runs on the dev PC (simulator lane; logs in `D:\dvr-data\logs\41-run*.log`):

| Run | Launch | Result |
|---|---|---|
| 1 | direct exe | instance on dvr-xrsim, then init_instance twice -> VR off; the game CRASHED at the main menu (`Dishonored.exe+0x60907e`, trap 6) |
| 2 | Steam | the ini rewrite dropped XrRuntimeJson -> VDXR (no headset), flat |
| 3 | Steam | env var set but the loader answered RUNTIME_UNAVAILABLE |
| 4 | Steam | loader property override set; manifest "path not found" (err 3) - the sandbox finding |
| 5 | Steam | the path probe: the game sees 5 entries where the shell sees 7 |
| 6 | Steam (no VR) | an AV in `d3d9.dll+0x87c95` after a Reset + RTD failure, three times, survived |
| 7 | Steam, `D:\dvr-data` | **dvr-xrsim, FOCUSED, first frame submitted, GAMEPLAY, both eyes 38% non-black, soak PASS 3 min**; eyetest NOT WRITTEN (null camera) |
| 8 | Steam | head-locked quad 16% per eye but swinging with yaw (the sim's quad math); eyetest wrote, measured the field/c5 offsets; WM_CLOSE -> the stuck process |
| 9 | Steam (after the process cleared) | `mono.xrs` PASS; head-lock pair IDENTICAL bboxes on the fixed sim; no c5 (the c0 x128 block) |
| 10 | Steam | c5 back; eyetest: 0x330 reads -c5 and moves c5 by -98.7 uu (75/76), the rest discarded |
| 11 | Steam | sign-aware seam: **0x330 HONOURED 119/120 (+99.2 uu)**, five DISCARDED; `camera eyefield 0x330` |
| 12 | Steam + Quest 3 (VDXR) | flat: a stale `[VR] XrRuntimeJson` (the sim manifest) made the loader fail; fixed to warn and ignore (`21e1cb64`) |
| 14 | Steam + Quest 3 (VDXR) | quit crashed again: the PreExit handler never ran (it lived inside the motion-aim block, off under GamepadOnly) - hoisted (`cf506ba4`) |
| 15 | Steam + Quest 3 (VDXR) | **clean quit**: `shutdown: game PreExit`, session teardown, instance destroyed, proxy unloading, no exception |
| 13 | Steam + Quest 3 (VDXR) | **THE HEADSET RUN: VirtualDesktopXR, Meta Quest 3, FOCUSED, READY, the screen in both eyes following the head, the gamepad working (user's report)**; quitting through the menu crashed 2.3 s after PreExit (EIP DEDEDEDE in d3d11.dll on VD's thread, the session still open) - teardown moved to the PreExit handler |

### 2026-09-02 - session 4e: gamepad-only, and the three rendering symptoms

Headset run at 4032x2268 requested / `capture: 3840x2160` actual. The tester reported three
things and they turn out to be one geometry. Full derivation in ENGINE_NOTES, "The three
rendering symptoms, and the one geometry that ties them".

1. **"Super pixelated, but the pause menu is huge like it's at full resolution."** Both
   halves are the same fact: SBS gives the WORLD half the frame width per eye
   (`per eye 1920x2160`) while a MONO menu frame samples the whole 3840 across the same quad.
   The menu is drawn at exactly twice the world's horizontal sampling density. That is the
   cleanest confirmation of the SBS packing anyone has produced, and it is not a bug - but it
   means the frame must be at least `2 x eyeWidth = 4992` columns for a 1:1 world. At 3840 the
   world sits at 77% of the panel.
2. **The fisheye is `FovLever`.** It does not only size the quad, it WRITES the game camera's
   FOV (`fov_lever.cpp`), so `FovLever=130` makes the game render 130 deg horizontal - the log
   agrees (`MEASURED render FOV ... = 130.0 deg`). A 130 deg rectilinear frame shown across a
   94 deg frustum stretches the edges. The author already knew: `frame_hooks.cpp` disarms the
   lever on overshoot, commented *"rather than leave the user in a fisheye"*.
3. **The black bottom border cannot be tuned away at 16:9.** Filling a 99 deg vertical
   frustum needs `lever = 2*atan(tan(v/2)*aspect)`: 128.6 at 16:9, 114.6 at 4:3, 100.5 near
   square. So the fisheye and the border are the SAME setting pulled in opposite directions,
   and at 16:9 nothing satisfies both. The tester found that empirically. A taller frame is
   not a preference, it is the only way out - which is exactly what they asked for.

**Next single change: `3840x2880` (4:3) with `FovLever` ~115.** Same per-eye width as now, so
no sharpness regression, +33% pixels, and it should visibly ease the fisheye.

**Two corrections to 4c, both mine.** (a) "Must be a real display mode" was too strong -
3840x2160 WAS honoured. The real rule is narrower: **`PinBackbuffer=1` causes the crop**; a
size the game rejects merely falls back, harmlessly, as long as the pin is off. Both effects
were present at 2850x2750, which made them look like one. (b) There is no 2560x1440 cap -
that was read from the run before the pin was turned off. 4032x2268 is still not honoured
(empty `setres` replies), so **trust `capture:`, never the requested number**.

**Applied this session:**

- **`[Mode] GamepadOnly=1`, new and default ON** (`config.cpp`). Turns off SkelControl hand
  writes, hand mesh, motion aim, motion melee, motion crouch and controller Blink aim, and
  scales no hand or weapon model. Head tracking, positional tracking, the FOV lever and the
  virtual gamepad keep running - this is NOT the `XR_SAFE` bisector, which also stops the
  head. It logs loudly and `status.json` gains `gamepadOnly` so the zeroes below it read as
  BY DESIGN rather than as failures. The author's rule that motion crouch and hands "must
  never stop working" is respected: nothing is retired, it is one key, set `GamepadOnly=0`.
- **The tester's tuned values are now the repo defaults**, in both the generated ini text and
  the `IniFloat` fallbacks, so a fresh install comes up where the headset testing left off.
- **`[PosTrack] Scale` default 50 -> 98.** This closes 40.2b: the tester tuned world scale by
  feel and landed on 98, within 2% of the 100 derived from the movement constants, arrived at
  independently and without seeing the number. That is the cross-check 40.2b was waiting for.
- Hand trims and `HandSize` reset to neutral, per the tester's "no scaling or changing the
  default hand/weapon models".

Build clean, exports 9/9 undecorated, lint clean, RelWithDebInfo installed. The fork and
`dxvk_stereo.txt` are untouched.

### 2026-09-02 - session 4d: the lever is half of the resolution setting

**The 3840x2160 run was full-frame but letterboxed** - tester: "almost sort of right again,
only problem was that the resolution was rectangular so it didn't fill my view". Both halves
of that are now explained, and one of them was my error.

**My error.** Session 4's restore set `FovLever=100`, correct for the near-square 2850x2750
it was paired with. Session 4c then changed the render to 16:9 and left the lever at 100.
The frustum-fill branch takes its vertical extent as `tan(fovDeg/2)/aspect`, so at 16:9 with
lever 100 the quad clamps to **67.7 deg inside a 99 deg frustum** - letterboxed by
construction. At lever 130 the same 16:9 render fills edge to edge with ~9% black at the
bottom. Changing the render aspect without changing the lever is NOT a one-variable change:
the pair is the variable. Table in ENGINE_NOTES, "FovLever IS the vertical fill lever".

**The requested resolution is not being honoured at all.** The mod asked for 3840x2160; the
log says `capture: 2560x1440`. With `PinBackbuffer=0` that is harmless - buffer and content
agree, no crop, which is why 4c's fix worked - but **this rig cannot render above 2560x1440**
(desktop 5120x1440 caps it). Every "4032x2268" run here is really 2560x1440. Trust the
logged `capture:` number, never the requested one.

**F10 SAVE AS DEFAULTS was finally pressed** (the thing session 3c said had never happened).
The tester's tuning is now persisted and snapshotted to
`tests/golden/f10-tuned-2026-09-02.ini` and `<game>\...\dishonored_vr.ini.f10-saved-0247`:

| key | default | tuned |
|---|---|---|
| `[Tracking] HeightOffsetM` | 0.000 | 0.040 |
| `[Screen] FillScale` | 1.00 | **0.74** |
| `[Screen] DistanceMeters` | 1.60 | 1.67 |
| `[PosTrack] Scale` | 50.0 | **100.0** |
| `CrouchToggle` | 1 | 0 |
| `XrLayer` | (absent) | proj |
| `StampFix` | (absent) | 0 |

`[PosTrack] Scale=100.0` matches the measured 100 uu/m from commit 60235b86 - the tester
converged on the measured value by feel, which is a good cross-check on that measurement.

**Applied on request: GingasVR's resolution default**, as the coherent pair -
`RenderWidth/Height=4032x2268`, `SpoofDesktopW/H=4096x2304`, **`FovLever=130`** - plus
`DishonoredEngine.ini` and all four AppCompat buckets at 4032x2268. `PinBackbuffer` stays 0
(her ini has no such key) and `MenuFillScale` stays 1.00 (the 4b fix). Every F10 value above
is preserved. Backups `.pre-gingasres` / `.f10-saved-0247`. NOT YET TESTED.

**Expect this**: the render will probably clamp to 2560x1440 again (fine, still 16:9), and at
lever 130 the quad should fill horizontally and to the top with ~9% black at the bottom -
**but `FillScale=0.74` will still present it at 74% of that**. F2 raises FillScale live in
the headset; that single knob is the difference between 74% and full. Judge the lever first,
then the fill.

### 2026-09-02 - session 4c: THE RENDER WAS NEVER 2850x2750

**This is the root cause.** The tester sent a desktop-mirror screenshot of the main menu with
the picture in the top-left of the window. The mirror blits the backbuffer's LEFT HALF
pillarboxed (`frame_hooks.cpp:415-460`), so its horizontal placement is expected - but the
picture filled only the top ~52% of the window, and that is not.

**Measured, six capture dumps, non-black bounding box:**

| requested buffer | actual content | real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | FULL |
| 2560x1440 | 2560x1440 | yes | FULL |
| 3840x2160 | 3840x2160 | yes | FULL |
| 4032x2268 (GingasVR's) | 3024x1440 | no | CROPPED |
| 2750x2850 | 2750x2200 | no | CROPPED |
| **2850x2750 (our "known good")** | **2560x1440** | no | **CROPPED** |

At 2850x2750 the game draws exactly **2560x1440 into the top-left** and leaves the rest
black. So the whole session's geometry was applied to a frame that is half empty. It
explains all three symptoms at once: tiny (content covers ~90% x 52% of the quad), top-left
(it is literally there), and the eyes not fusing (the SBS halves meet at x=1280, not the
x=1425 the split assumes, so each eye gets part of the other's view plus black).

**Why nothing caught it.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 while the game
renders at the size it asked for (`CreateDevice the game asked for 2560x1440`). The mod then
spoofs `GetClientRect` to 2850x2750 and the setres path reads that spoof back, concluding
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. A
check reading our own spoof cannot fail its own hypothesis, so the engine-side resize never
ran and `capture: 2850x2750` was logged for a half-empty frame.

**`PinBackbuffer` is ours, not GingasVR's.** Her tuned ini (`.pre-2750`) has no such line.
Every ini since session 2 sets it to 1.

**This is probably the central open bug.** 4032x2268 is not a standard mode either and
cropped here too. Whether an injected mode is honoured depends on the machine's GPU, driver
and desktop mode - this rig's desktop is 5120x1440, and two of the three cropped captures
came back exactly 1440 tall. It predicts affected users have a desktop shorter than the
requested render height, and it is falsifiable by asking one for their desktop resolution.

**The Documents folder was ruled out**, on the tester's suggestion: `DishonoredEngine.ini` is
vanilla plus the intended VR lines, all four AppCompat buckets were already correct, and no
file in the Config directory contains 2560 or 1440.

**Applied for testing** (one coherent change: use a resolution the display actually offers):
`RenderWidth/Height 2850x2750 -> 3840x2160`, `SpoofDesktopW/H -> 3840x2160`,
`PinBackbuffer 1 -> 0`, plus `DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` to
3840x2160. Per-eye half 1920x2160 = aspect 0.889, the same per-eye aspect as GingasVR's
4032x2268. Backups: `.pre-realmode` next to each of the three files. NOT YET TESTED.
Fallback if 3840x2160 is not honoured: 2560x1440, identical per-eye aspect, and the game
asked for it itself.

**Also confirmed this session**: the 4b MenuFillScale fix works. The 02:35 run logged 28 quad
rebuilds, all at `fill=1.00` / 100.0 x 98.0 deg, none at 0.60. The size pumping is gone.

**Next instrument to build**: nothing compares the captured frame's real content extent to
the buffer size. A non-black bounding-box check on the capture, logged once per resolution
change, turns this class of bug into one line. The setres check must also stop reading the
mod's own `GetClientRect` spoof.

### 2026-09-02 - session 4b: the world size PUMPS, and MenuFillScale is why

A headset run on the restored known-good ini (02:23, VirtualDesktopXR + Quest 3) reported
"still rendering tiny and in the top left corner". Its log names the cause outright, so this
did not need a new instrument. Full derivation in ENGINE_NOTES, "MenuFillScale pumps the
world size during GAMEPLAY".

**The number.** The quad subtends **71.1 x 69.2 deg** inside a frustum of **94.0 x 99.0 deg**
- about half its solid angle. That is "tiny", fully explained, and nothing to do with
resolution, adapter, world scale or convergence. 40 of the run's 46 quad rebuilds were at
`fill=0.60`; the run ended there.

**The mechanism.** `MenuFillScale=0.60` and the `XrFrustumFill` gate are driven by the SAME
condition (`g_menuOpen || g_inMenu || g_sbsMonoNow`), and a change in it forces a rebuild.
The menu flag flaps during gameplay (the `Req_SaveSlotInfos` save-slot polls, already
documented at `present.cpp:613-616`), so the world size pumped 100 -> 71 -> 100 -> 71 -> 100
-> 71 deg across the six seconds before the crash, all after gameplay had started. The
`sbs:` line proves it is the MENU flag and not the splice counter: its last transition is
well before the pumping began.

**Why the Index never saw it.** OpenVR has one geometry path, so `MenuFillScale` only ever
dimmed a menu. `XrFrustumFill` (38.13) added a second path for the OpenXR port without making
the transition continuous, so on Quest the same flap swaps the whole quad construction
mid-gameplay. The tester's own read - Index/SteamVR was the tuned target, OpenXR/Quest a
later port - is exactly right here.

**Applied, config only, one variable, no rebuild**: `[Screen] MenuFillScale 0.60 -> 1.00`
(backup `.pre-menufill`). The menu branch now builds the same 100.0 x 98.0 deg quad as
gameplay, so a flap cannot change the world size. Cost: menu edges crop, which is what 32.4
added the key to avoid. NOT YET TESTED.

**A falsifiable prediction, and it contradicts session 3c.** Worked from the logged frustum,
the authored quad's vertical border must be SYMMETRIC, ~21% black top and ~21% bottom, with
the world in the middle 57.6%; horizontally the left eye gets 29.8% black on the temple side
and 5.6% on the nasal side (mirrored in the right eye - the rigid-screen design). Session 3c
recorded "top ~54%, bottom half black". The next `dump eyes` settles it: symmetric borders
retire that contradiction as a misread dump; a real black bottom half falsifies this model.

**The run also CRASHED** at 02:23:49, wild instruction pointer, minidump at
`%LOCALAPPDATA%\DishonoredVR\dumps\dvr_20260902_022349.dmp`. Untriaged, separate lane.

**Good news in the same log**: the fork's projection export resolved this time -
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 100.0 deg`.
The landscape fix (session 3b) worked; world scale is no longer an assumed constant.

### 2026-09-02 - session 4: reverted to the known-good point

No launches, no code changes. Session 3c left the rig one key away from its own
confirmed-good configuration and that key was an open, unevaluated experiment; this
restores the documented point so the next run starts from a known baseline.

**What was actually different.** Exactly one line: `FovLever=130` vs `100`. Everything
else already matched - `RenderWidth/Height=2850x2750`, `SpoofDesktopW/H=2816x2880`,
`PinBackbuffer=1`, `GameFOVDeg=100`, `FillScale=1.00`, `[PosTrack] Scale=50.0`, the game
ini at 2850x2750 on both `[SystemSettings]` and `[SystemSettingsEditor]`, and all four
`[AppCompatBucket1..4]` at 2850x2750. `setup-game-ini.ps1` did not need re-running.

**The snapshot checks out.** `tests\golden\known-good-2850x2750-lever100.ini`, the game
folder's `dishonored_vr.ini.KNOWN-GOOD` and `dishonored_vr.ini.pre-lever130` (the ini as it
actually ran when the tester reported "the eyes seem to overlap correctly and provide
depth") are all identical. The snapshot is a truthful record of the run, not a
reconstruction - worth stating, because it was written at 00:54 while the live file was
already at `FovLever=130`.

**Restored** by byte copy, verified with `cmp` against both snapshots. Prior state saved as
`dishonored_vr.ini.pre-restore-known-good`.

**`FovLever=130` is untried, not disproved - and it was the well-motivated direction.**
ENGINE_NOTES "FovLever and the render size are ONE setting" has it the other way round from
how session 3c's ordering reads: at lever **100** the clamp limit is 1.91 m against a
frustum reaching 2.20 m horizontally and 2.29 m vertically, so it fires on **all four
sides**, and `dump eyes` at lever 100 confirmed the world inset with a ~9-10% border on
every side. At lever **130** the limit is 3.43 m, outside the frustum edge, so nothing
clamps and the quad fills the eye. 130 is also GingasVR's own tuned value.

**So the restore knowingly reinstates the bordered configuration.** That is the right call -
lever 100 at 2850x2750 is the only point a tester has ever confirmed fuses with depth, and
an unevaluated experiment is not a baseline - but the border is a KNOWN artifact of this
baseline, not a new symptom, and "the render window is halfway up my vision" must be judged
against that. Lever 130 stays queued as the next one-variable change once the gameplay dump
is in hand.

**Caveat that still stands**: no F10 tuning has ever been persisted (SAVE AS DEFAULTS was
never pressed), so this ini is the only reproducible configuration that exists.

**Untouched deliberately**: the proxy, the fork, `dxvk_stereo.txt`, and the stale
`command.txt` in `%LOCALAPPDATA%\DishonoredVR\` (`command.cpp:153` discards and clears a
command file older than the process, so the next run's log should show that branch firing -
a free check of the new seam diagnostics).

**Next**: unchanged from session 3c. Launch, reach GAMEPLAY with the window focused, then
`tools\game-cmd.ps1 "dump eyes"` and read the two PNGs. The unexplained contradiction is
still the lead: the world occupied only the top ~54% of the eye render target while the
measured frustum (55 deg down against 44 deg up) says the quad should overflow vertically.
That dump was a MENU frame and needs confirming in gameplay before anything is changed.

### 2026-09-02 - session 3c: the seam went deaf, and the eye texture is half black

**START HERE.** The bug is not fixed and the last measurement is incomplete.

**The one thing to do first**: launch, get into GAMEPLAY (not a menu, window focused),
then `tools\game-cmd.ps1 "dump eyes"` and look at the two PNGs in
`%LOCALAPPDATA%\DishonoredVR\dumps\`. Everything below is waiting on that image.

**The live symptom** (tester, Quest 3 + VirtualDesktopXR): eyes will not fuse, and the
image sits high - "the render window is halfway up my vision so I only see the bottom
half of it". Earlier in the session, "everything looked tiny".

**The measurement that matters.** A `dump eyes` caught mid-session shows the world
occupying only the **top ~54% of the eye render target, bottom half pure black**, in both
eyes. That is our own D3D11 pass, before the compositor. It was a MENU frame (the dump
caught a paused game), so it needs confirming in gameplay - but the vertical placement is
the lead. The measured frustum is
`eye frustums: L[-1.376 0.839 -1.428 0.966] ex=-0.0316 | R[-0.839 1.376 -1.428 0.966]`,
slots `[left, right, down, up]`: **down-biased, 55 deg down against 44 deg up**. Worked by
hand at these settings the quad should span y -2.36..+1.62 against a frustum of
-2.29..+1.55, i.e. it should OVERFLOW the eye vertically, not sit in the top half. That
contradiction is unexplained and is the next thing to chase.

Same dump showed the two eyes holding **different content** (different horizontal extents,
different fragments of the same menu text). That is the mono/stereo UV race on menu frames
- the fork stops splicing, the frame goes mono, each eye still takes its own half. Probably
menu-only: in gameplay the splice count is 5000+ and the halves are a real stereo pair.

**The command seam went deaf and blocked the session.** `status.json` stale for 18 minutes
and `command.txt` unread, while the Present hook ran at 62 fps with 251 `[present]` lines.
Two `dump eyes` commands did nothing and left no trace. `poll()` had FIVE silent returns,
so the failure was indistinguishable from the command never being written. **Fixed and
installed**: each guard now names itself with the path, the size and GetLastError. If the
seam is still deaf next session the log will say which branch refuses.

**Theories killed this session (do not re-run them):**

- The `CopyResource` size mismatch (step 0a) - eye RTs and XR eye size matched exactly.
- The pace thread as the crash victim - the faulting thread is `(other)`, not `xr-pace`.
- The black left eye - a SIMULATOR defect, not the mod; `dump eyes` shows the left eye
  texture full. See VERIFICATION "Known simulator defects".
- Eye cant - `g_eyeRot` is declared identity and the XR path correctly leaves it alone.
- The clock/rate gate - `MaimNowMs` is fine (the 5 s `depth:` line printed 50 times against
  the 3 s heartbeat's 81). Note the log uses `GetTickCount` while the gates use QPC via
  `dvr::clock`, so an advancing log proves NOTHING about the gate clock.
- A game restart clearing the seam - it did not.

**THE KNOWN-GOOD STATE, AND HOW TO GET BACK TO IT.** This rig's best result so far -
tester: *"the eyes seem to overlap correctly and provide depth, there is no freeze"* - came
from `2850x2750` + `FovLever=100` + `Scale=50`, with the main scene splicing (5202). It is
worth more than GingasVR's own values, which come from a different machine and are the
subject of the project's central open bug ("works only on her PC").

Snapshotted byte-for-byte in two places, so it survives a wiped game folder:

- `tests\golden\known-good-2850x2750-lever100.ini` (in the repo)
- `<game>\Binaries\Win32\dishonored_vr.ini.KNOWN-GOOD`

Restore = copy either over `<game>\Binaries\Win32\dishonored_vr.ini`, then
`tools\setup-game-ini.ps1 -Resolution -Width 2850 -Height 2750` for the game ini and the
four AppCompat buckets. The keys, if you ever need to rebuild it by hand:
`[Screen] RenderWidth=2850 RenderHeight=2750 SpoofDesktopW=2816 SpoofDesktopH=2880`
`PinBackbuffer=1 GameFOVDeg=100 FovLever=100 FillScale=1.00`, `[PosTrack] Scale=50.0`.

**WARNING - the snapshot does NOT contain any F10 tuning, and neither did any run.** The
overlay's sliders are live-only until someone presses **"SAVE AS DEFAULTS"**
(`overlay.cpp`, top of the panel), which calls `OverlaySaveDefaults` and writes ~90 keys -
world scale, fill, screen distance, height offset, menu fill, wrist HUD, and the whole hand
/ graft / blink block. It was never pressed, so every F10 adjustment the tester made across
this session was lost at process exit; the only thing that persisted was the hand
calibration, which `skelcontrol.cpp` writes on its own. **Procedure from now on: tune in
F10, press SAVE AS DEFAULTS, then re-snapshot the ini.** Otherwise a good configuration
cannot be reproduced, which is exactly what happened here.

**Config as left, LIVE right now**: ~~`FovLever=130`~~ **RESTORED to the known-good
snapshot (session 4)**. The live `dishonored_vr.ini` is now byte-identical to both
`tests\golden\known-good-2850x2750-lever100.ini` and the game folder's
`dishonored_vr.ini.KNOWN-GOOD` (`cmp` clean against both), i.e. `FovLever=100`. The
`FovLever=130` experiment was applied but never evaluated - the seam went deaf before a
dump could be taken - so it was discarded rather than judged; it remains untried, not
disproved. The pre-restore state is saved as `dishonored_vr.ini.pre-restore-known-good`.
Game ini + all 4 AppCompat buckets verified still at 2850x2750 (`DishonoredEngine.ini`
lines 1081/1141, `DishonoredCompat.ini` buckets 1-4), so no `setup-game-ini.ps1` re-run
was needed.
Backups in the game folder: `.pre-2750` (GingasVR's own tuned ini), `.pre-landscape`,
`.pre-scale100`, `.pre-gingas-restore`, `.pre-rollback`, `.pre-lever130`,
`.pre-restore-known-good`.

**Installed**: RelWithDebInfo proxy only, 00:50, carrying the seam diagnostics. The fork
(20:24) and `dxvk_stereo.txt` (13:52) are untouched and must stay that way - one variable.
Re-verified session 4: the installed `d3d9.dll` is md5-identical to
`build\src\RelWithDebInfo\d3d9.dll`, and the tree is clean at `112105b7`, so source,
build and install all agree. The fork and `dxvk_stereo.txt` timestamps are unchanged.

**Do not repeat these mistakes.** Restoring GingasVR's baseline I changed render size, FOV
lever and world scale in ONE step, so "misaligned" was unattributable; her values also come
from a different machine, and the project's central open bug is that her build works only
on her PC. This rig now has its own confirmed-good point (2850x2750, splices 5202, tester:
"eyes overlap correctly and provide depth") which is worth more than her numbers. Change
one thing per run, and get the gameplay dump before changing anything at all.

### 2026-09-01 - session 3b: the render was PORTRAIT, so there was no stereo

A headset run mid-session reported "the eyes are suuuuper far off and they both appear to
be zoomed in", worse than before. Its log is the **first surviving headset log** and is
archived outside the game folder. Cause found, fix applied, not yet tested.

**Session 2's resolution fix set the render to 2750x2850, which is portrait, and the DXVK
fork refuses to splice the main scene on a portrait viewport.** `d3d9_device.cpp:4381`
sets the refusal reason `"rt-portrait"`; the per-eye splice at `:4578` runs only when that
reason is `SPLICE`. So the world was drawn **mono** across the full frame while the proxy
handed each eye a different **half** of it - unrelated views that cannot fuse, each
magnified 2x by the stretch onto the quad. Exactly the report.

**The splice counter lies about it.** Light shafts, shadows and the M8.1 quarter light pass
splice under different conditions and kept working, so `splices=85` while the main scene
never spliced once - which kept `g_sbsMonoNow` false and the half-frame UVs on. The fork's
own log shows only effects splices.

**The same gate kills the FOV measurement.** `dxvk_vr_proj`'s publish (`:5996`) is also
gated on `Width > Height`, so `g_liveFovX` was 0 all session, the frustum-fill path fell
back silently to the ini constant `GameFOVDeg=100`, and an assumed number set world scale.

**This falsifies session 2's "the eyes ARE a stereo pair".** 32.7 mean-abs-diff static /
11.5 after a head turn is exactly what two different halves of one mono frame produce. That
test could not distinguish a stereo pair from two unrelated crops, so it could never have
failed its own hypothesis.

**Applied**: `2850x2750` - the same two numbers swapped. Same pixel cost, landscape by
100 px, full-frame aspect 1.036 so the quad subtends 100 x 98 deg at `FovLever=100`.
Changed in `tools/setup-game-ini.ps1` (defaults + a header section on why landscape is
mandatory), applied to `DishonoredEngine.ini` and `DishonoredCompat.ini` via the tool (both
backed up), and to the game folder's `dishonored_vr.ini` (backup `.pre-landscape`).

**Instruments added so this cannot hide again**: a portrait capture logs an Error naming
the fork's own refusal string and the fix (`present.cpp`); the frustum-fill path now says
every 10 s whether world scale comes from the MEASURED render FOV or from the assumed ini
constant (`eye_quads.cpp`).

**Installed**: RelWithDebInfo proxy only (`install.ps1 -Release -SkipDxvk`) - the fork and
`dxvk_stereo.txt` are untouched, so the resolution is the only render-path variable. The
proxy's other changes (session 3 below) are the shutdown/pace lane and logging, which
cannot confound the zoom result.

**Not yet tested.** If the fix worked the portrait Error is absent and the eyes fuse; if
the Error appears, the resolution did not take and AppCompat is overwriting it again.

### 2026-09-01 - session 3: the crash fingerprint was misread, and why

No launches: everything here comes from artifacts already on disk plus the source. The
game is installed on this PC, but nothing was run.

**Two of session 2's conclusions are instrument bugs, not engine facts.**

1. **The exit crash is an EXECUTE fault, not a freed-memory write.**
   `ExceptionInformation[0]` is three-valued (0 read, 1 write, 8 execute/DEP) and the
   fingerprinter tested it for truth, so every execute fault has printed as "writing". The
   records prove it: `ExceptionAddress == ExceptionInformation[1] == 0xDEDEDEDE` with the
   module resolving to `?`. A data write would have left `ExceptionAddress` inside
   `d3d11.dll`. **EIP landed in freed memory: a call through a poisoned code pointer.**
2. **The faulting thread is not the pace thread.** All three records say `(other)`, which
   `thread_name()` returns only for a tid in no registered slot; `present` and `xr-pace`
   both register at entry. The faulter is a third-party worker (d3d11, driver, runtime).
   The pace thread can be the cause, but instrumenting it as the victim will find nothing.

**Two evidence channels were dead and are now fixed.**

- **`dumps\` was empty by construction.** 3 `EXCEPTION` lines, 0 `minidump` lines: proof
  that `unhandled()` never ran, because UE3's own filter/SEH frame consumes the fault
  before `SetUnhandledExceptionFilter` fires. The dump is now taken from the **vectored**
  handler (which always runs), gated on the instruction pointer resolving to no loaded
  module - fatal-only by construction, and falsifiable: an ordinary in-module fault
  produces no dump and disproves the wild-EIP reading. `dbghelp.dll` is resolved at
  `install()` time so the VEH never touches the loader lock.
- **The crash file had no run identity.** `FILE_APPEND_DATA` / `OPEN_ALWAYS` with nothing
  separating runs, and `dvr-xrsim` and VDXR produce byte-identical fingerprint text, so
  the three records cannot be attributed to a backend at all. Now one header per run
  (clock, version, build id, pid, backend + runtime name).
- **Log rotation is one deep**, so two simulator runs erased both headset logs; the
  survivors contain no `EXCEPTION` and no `PreExit`. Copy the log out before each launch.

**The author read this fault correctly and session 2 inverted it.** The 38.79 comments say
"EIP dededede" and "a call through freed memory". 38.79 acted on that by standing the
**game** thread down at `PreExit`, which was right but not the whole path - it left the
pace thread running with nobody waiting for it. Closed below.

**Two pace-lane defects fixed** (steps 0b and 0c):

- **`XR_TIMEOUT_EXPIRED` is a success code.** `XrResult` is negative for failure only, so
  `XR_FAILED()` is false for it and `!XR_FAILED(xrWaitSwapchainImage(...))` ran
  `CopyResource` into an image the compositor had not finished reading - a race with the
  runtime on the one resource the headset displays, invisible because every call returns
  success. Now `== XR_SUCCESS`. In the same block `g_xrpShown` advanced **before** the
  copies, so a frame lost to a timeout was dropped permanently instead of retried; it now
  advances only once both eyes actually received the content.
- **`XrPaceStop()` joins the pace thread**, bounded at 750 ms, replacing the bare
  `g_xrRun = 0`. On expiry the thread is left running on purpose - `TerminateThread` would
  orphan `g_xrCs` and abandon an acquired swapchain image, which is worse than the race -
  and the error line is the instrument: a fault after it means the pace lane is still the
  suspect, a fault without it means the thread was already gone and it is not. The event
  pump's inner `while` now tests `g_xrRun` so an event backlog cannot hold the loop past a
  stop request.

**Changed** (8 files, uncommitted): `src/core/util/crash.cpp` and `crash.h` (three-valued
AV decode; run header; `set_context`; shared `write_dump` with the wild-EIP gate),
`src/core/vr/openxr_backend.cpp` (names the runtime in the crash context),
`src/core/vr/openxr_pace.cpp` (the wait fix, the retry fix, `XrPaceStop`),
`src/game/dishonored/ue3/process_event.cpp` (`PreExit` joins), `src/mod/fwd.h`,
`docs/dishonored/ENGINE_NOTES.md`, `docs/STATUS.md`. Verified: Debug, RelWithDebInfo and
`-Legacy` all build, and both DLLs carry the new strings; `lint.ps1` clean; exports 9/9
undecorated. **Not run in the game, in the simulator, or in a headset.**

**Next**: step 0a - the `CopyResource` size mismatch, which is still only a code-reading
hypothesis. Then the full `xrRequestExitSession` / `xrDestroySession` shutdown.

### 2026-09-02 - session 2: instrumentation, resolution, and a real headset

**First session with the game actually installed and a real Quest + Virtual Desktop
headset on the other end.** Environment: proxy and fork both built from source with
MSVC (meson + ninja + glslang 16.5.0 standalone, no Vulkan SDK); `tools\build-dxvk.ps1`
needed two fixes to run at all on a PC with VS 2026 installed next to VS 2022.

**Fixed and verified**

- **Resolution.** `ResX` in `DishonoredEngine.ini` never held: UE3 AppCompat picks an
  `[AppCompatBucketN]` at startup and writes that bucket's ResX/ResY over
  `[SystemSettings]`. Buckets 3 and 4 ship 1600x900, and any GPU newer than the 2012
  table lands in one, so every modern machine started at 1600x900 forever. Fix: set all
  four buckets; `tools\setup-game-ini.ps1 -Resolution` now does this and defaults to
  2750x2850. Measured before/after: `CreateDevice (1600x900)` -> `CreateDevice
  (2750x2850)`, `capture: 2750x2850` on the FIRST device creation, no setres needed.
- **`setres` is a dead end.** Measured `setres 2750x2850w -> "(empty reply)"` with NO
  device Reset following. New `[Screen] PinBackbuffer=1` (default OFF) sets the size in
  the present parameters at CreateDevice instead. The 32.57 "image in the corner"
  objection is answered by the GetClientRect hook that landed later.
- **World scale.** `W` is pinned to the rendered FOV and `H = W / frameAspect`, so a
  squarer render makes a taller virtual screen than the lenses can show and the player
  sees a magnified middle. 2750x2850 at FovLever=130 subtends 100x132 deg; at 100 it is
  100x102, which matches the headset. FovLever set to 100.
- **The mono/stereo UV race.** `BuildEyeQuads` BAKES the sampling UVs, but the rebuild
  only fired on an aspect change or a menu toggle. `g_sbsMonoNow` flips during gameplay
  whenever the fork's splice count dips, so a mono frame could be sampled with stereo
  UVs and each eye got a different half of one mono image. Now a change in frame kind
  forces a rebuild, exactly like the menu flag.

**Instrumentation added** (see the new "Logging" section in `CLAUDE.md`)

- Full DXGI adapter enumeration, the LUID the runtime asks for, and the adapter read
  back OUT of the finished device with an Error-level mismatch line.
- `RESOLUTION CHANGED MID-SESSION` at Warn, `quad: ... subtends AxB deg` per rebuild
  with a Warn past 110 deg vertical, `res: the game asked for WxH`, and a `skc/gate:`
  line for the hand drive.
- The hands heartbeat now names the OWNER and reports that owner's counter.

**Corrected beliefs** (all three were believed and are wrong)

1. "The hand graft never attaches." It attaches fine: `OWNER=SkelControl writes=~406/3s`
   in gameplay. The old heartbeat tracked two counters that read 0 BY DESIGN - one a
   retired subsystem, one the legacy drive that is deliberately stood down. Three
   readers including the original author concluded "the hands are dead" from a healthy
   run.
2. "The 39.3 adapter bug needs two GPUs." DXGI enumerates the SAME RTX 4070 Ti SUPER
   twice on this PC (virtual display drivers), so the default adapter is not stable on
   single-GPU machines either. **But on the real VDXR run the runtime asked for
   adapter[0], which IS the default, so the LUID mismatch is NOT the cause of the
   symptoms on this rig.** 40.1 still fixes the class of bug and makes it visible.
3. "The eyes are not a stereo pair." Measured 32.7 mean-abs-diff when static, but 11.5
   after a head turn, which is normal parallax. Stereo works; the divergence is a
   symptom of the freeze, not the disease.

**Still broken: the freeze-then-rescale.** Reported again after all of the above. What
is ruled out: the resolution (it now stays 2750x2850), the adapter (matched), the FOV
(100), the mono/stereo UV race (fixed), and the hand drive (working). What is NOT ruled
out and is where to look next:

- The **shutdown crash is a teardown race** and may share a root with the freeze: three
  runs ended with `EXCEPTION 0xc0000005 writing 0xDEDEDEDE` inside `d3d11.dll`, two
  threads at once, immediately after `PreExit` stops the pace thread. 0xDEDEDEDE is
  freed-memory poison. The detached pace thread is touching released D3D11 objects.
- The pace thread owns every runtime call while the game thread owns capture and
  UpdateSubresource on the SAME `g_ctx11`; ID3D11DeviceContext is NOT thread-safe.
  `ID3D10Multithread` is enabled but that protects the device, not a stale pointer.
- Instrument the frame path next: log around the Reset/teardown boundary and around
  every `g_ctx11` use from the pace lane, and get a minidump analysed from
  `%LOCALAPPDATA%\DishonoredVR\dumps`.

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.
