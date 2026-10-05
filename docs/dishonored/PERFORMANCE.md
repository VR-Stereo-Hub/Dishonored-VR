## 2026-10-05: the unattended audit run - measured, and in which configuration

One headset run of `tools/perf-plans/audit-all.txt` (41 segments of 20 s, 4 s warm-up each) on
build v1.0.3-54-gd0c57b1b9 (a local merge of #178, #179 and #180; RelWithDebInfo), VDXR at
144 Hz, RTX 4070 Ti SUPER. Parsed from the log with a throwaway script; every number below
names its population. Tools: the A/B plan (`perf/ab`), the stage profile (`perf/stages`),
`pe fn`, the `perf: tick / parts / gpu/present` lines, the helper's own `dlss:` line.

### 1. Identity of the run - read this before any number

- Plan LOADED (41 segments, 21 baselines), armed by `[Perf] AbPlanOnce` (the key emptied
  itself), `gameplay reached`, PLAN STARTED 60 s later, PLAN COMPLETE. 40 segments DONE; segment
  41 (baseline B13) DISCARDED: the restore word of row 40 resized the device inside it.
- **The plan did not run in the played configuration.** The session started as `afw` with DLSS
  Ultra Quality (2114x2192 -> 2750x2850) and the fast CNN model (the ini had `DlssModel=1`).
  Six seconds into the 60 s lead-in the F10 panel was opened by the controller chord, and F10
  Display wrote `[Stereo] Method=reentry` and then `[Clarity] DLAA=0` (DLSS off, the engine
  resized to 2750x2850, one device reset) 41 s before the first segment.
  **Every plan row is therefore: reentry, native 2750x2850, DLSS OFF.** 273 of 273
  `stereo: beat` lines inside the plan read `method=reentry ... 2750x2850`.
- Consequences inside the plan: row 14 (`dlss model fast`) and row 40 (`dlss off`) were no-ops.
  Their RESTORE words were not: after row 14 the live model was Transformer K, after row 40
  DLSS was ON (Ultra Quality, K). The plan's restore words assume the state its header
  describes; this session was in neither. `hud sharp off` was a no-op as well (with no
  upscaler reducing the render the sharp path and the old path are the same size).
- A pair is one tick here (two presents); 1296 to 1584 pairs per segment, 86 to 102 pairs/s.
- **Baseline drift is larger than most rows.** Baseline p50: 9.76, 9.77, 10.08 | 10.82, 11.08,
  11.04, 11.01, 11.26 | (profiler on) 11.53, 11.41, 11.61, 11.35, 11.51, 11.46, 11.25 | 10.44,
  10.51, 10.51, 10.59, 10.33. Two steps, neither a drift:
  1. +0.74 ms between baselines A3 and A4: the restore `reshade effects on` (section 3).
  2. -0.8 ms between baselines B7 and B8, across the `LightEnvironmentShadows` toggle pair:
     BasePass GPU fell from 1.65 to 1.3 ms a pair at an unchanged draw count (1665 -> 1670)
     and stayed there. Head yaw moved less than 2 degrees. Either the toggle pair does not
     return the engine to the state it found, or the scene changed. Not identified; that row
     has no verdict.
  Head yaw per segment stayed within -19..-25 degrees for the whole plan.

### 2. Hidden area (XR_KHR_visibility_mask, probe only)

`xr/vismask: eye 0|1 HIDDEN MESH 52 triangles, 52 vertices | hidden area 0.5789 of the eye
image's 5.3035 (tangent units squared) = 10.9 %`, both eyes, 0 vertices outside the FOV
rectangle, 0 bad triangles. The extension is offered and the share is **10.9 % of the pixels**
on this headset through VDXR (the audit's working figure was 15 %).

### 3. Every row against the baselines either side of it

p50 of the pair interval, ms. "Spread" is the difference between the two flanking baselines; a
row inside it is "no change". The tail (p99) floor over the 20 baselines was 12.5 to 21.5 ms,
so no row has a tail verdict.

| # | Row | pairs | p50 | flanking baselines | delta | spread | Verdict |
|---|---|---:|---:|---|---:|---:|---|
| 2 | hud off | 1520 | 10.28 | 9.76 / 9.77 | +0.51 (+5.3 %) | 0.01 | SLOWER: under reentry the redirect is cheaper than the HUD drawn into both eye passes |
| 4 | hud sharp off | 1547 | 9.82 | 9.77 / 10.08 | -0.11 | 0.31 | no-op in this configuration (no upscaler) |
| 6 | reshade effects off | 1546 | 9.94 | 10.08 / 10.82 | -0.51 | 0.74 | not measured by the row, see below |
| 8 | aniso 4, no trilinear | 1472 | 10.83 | 10.82 / 11.08 | -0.12 | 0.26 | no change (11653 sampler binds were being raised) |
| 10 | sharpen 0 | 1432 | 11.08 | 11.08 / 11.04 | +0.02 | 0.04 | no change |
| 12 | script lane named | 1444 | 11.00 | 11.04 / 11.01 | -0.02 | 0.03 | no change: `pe fn` costs nothing measurable |
| 14 | dlss fast model | 1425 | 11.18 | 11.01 / 11.26 | +0.04 | 0.25 | NOT TESTED (DLSS was off) |
| 16 | stage profile ON | 1296 | 11.74 | 11.26 / 11.53 | +0.35 (+3.0 %) | 0.27 | the profiler's own overhead; p95 17.3 in the switch-on segment only |
| 18 | light shafts off | 1392 | 11.42 | 11.53 / 11.41 | -0.05 | 0.12 | no change; no LightShafts stage in any table of this view |
| 20 | cheap bloom | 1359 | 11.63 | 11.41 / 11.61 | +0.12 | 0.20 | no change; BloomParts untouched |
| 22 | bloom off | 1375 | 11.57 | 11.61 / 11.35 | +0.09 | 0.26 | no change; BloomParts untouched |
| 24 | half-res translucency | 1371 | 11.43 | 11.35 / 11.51 | 0.00 | 0.16 | no change; nothing translucent of size in this view |
| 26 | distortion off | 1365 | 11.54 | 11.51 / 11.46 | +0.05 | 0.05 | honoured (the stage is gone), no gain |
| 28 | dynamic shadows off | 1433 | 11.01 | 11.46 / 11.25 | -0.35 (-3.0 %) | 0.21 | faster |
| 30 | light env shadows off | 1477 | 10.56 | 11.25 / 10.44 | -0.28 | 0.81 | no verdict (the step of section 1) |
| 32 | shadow texels 0.64 | 1512 | 10.26 | 10.44 / 10.51 | -0.21 (-2.1 %) | 0.07 | faster |
| 34 | shadow max 400 | 1507 | 10.35 | 10.51 / 10.51 | -0.16 (-1.5 %) | 0.00 | faster |
| 36 | lens flares off | 1495 | 10.36 | 10.51 / 10.59 | -0.19 (-1.8 %) | 0.08 | faster by the rule, but no flare stage is identified: weak |
| 38 | screen percentage 80 | 1529 | 10.26 | 10.59 / 10.33 | -0.20 | 0.26 | honoured in the post chain only, no change in the pair |
| 40 | dlss off | 1490 | 10.44 | 10.33 / discarded | +0.11 | - | NOT TESTED (DLSS was already off) |

Cross-checks from the lines inside each segment (medians of 5 or 6 lines):

- `perf: tick`: render thread `R` 4.7-5.3 ms (first eye) + 4.0-5.1 ms (second), idle 0.1-0.2 /
  0.0, our present path `in` 0.4-0.7 ms a present. `perf: parts` sum 450-640 us a present
  (`hk.method` 78-97, `hk.hudRedirectEnd` 30, 2 with `hud off`). `gpu/present`: span 7.4-9.8 ms
  a tick, D3D9 idle 0.0-0.3.
- **ReShade.** `hk.reshadeEffects` was 80-97 us a present in baselines A1 to A3 AND in the
  `reshade effects off` row, then 148-175 us from the restore `reshade effects on` to the end.
  So the effects were not being drawn when the plan started, the row changed nothing, and the
  restore switched them on. The pair p50 stepped 10.08 -> 10.82 across that restore and stayed
  (11.0-11.3 for the next four baselines). **One unbracketed step: the bundled preset costs
  about 0.7 to 0.9 ms a pair (7 to 8 %) at native 2750x2850.** Why the effects were off at the
  start is not established (the runtime was recreated by the device reset 40 s earlier).
- The engine switches: every reply was `console: 'scale ...' -> 0 (empty reply)`, so the reply
  is no evidence. The stages are: `scale toggle Distortion` removed the Distortion stage (0.39
  ms GPU, 16 draws a pair) and `scale set ScreenPercentage 80` cut the depth-of-field stage's
  GPU time from 1.9 to 0.42 ms a pair, both back at the restore. **The console route reaches
  the SCALE handler for TOGGLE and for SET.** The three shadow rows moved the dominant light's
  stage. `Bloom` and `UseHighQualityBloom` moved nothing: the bloom here is the game's own
  BloomParts stage (0.45 ms GPU, 108 draws a pair, identical in both rows).
- `ScreenPercentage 80` did not shrink the scene passes (BasePass GPU 1.36 against 1.36 / 1.29),
  only the post chain. It is not a render scale for this renderer.

### 4. The stage profile (plain baselines B1 to B7, profiler on, 86 pairs/s)

Per PAIR: both eye images plus the once-a-tick scene capture (DPG World runs 3 times a pair).
Halve for one eye image. Inclusive times; mean of 7 baselines, 3 tables each (min..max over the
baselines in brackets where it matters).

| Stage | calls | render-thread CPU ms | GPU ms | draws |
|---|---:|---:|---:|---:|
| DPG World (the scene, both halves) | 3 | 8.00 (7.93..8.08) | 4.97 (4.89..5.06) | 4356 |
| - BasePass | 5 | 3.36 | 1.66 | 1670 |
| - ShadowedLights | 5 | 2.49 | 1.62 | 1125 |
| -- of it DominantDirectionalLight_0 | 5 | 2.09 | 1.47 (1.32..1.56) | 1063 |
| - PrePass | 5 | 0.92 | 0.27 | 824 |
| - Distortion | 5 | 0.11 | 0.39 | 16 |
| PostProcessEffects | 8 | 0.08 | 2.64 (2.51..2.81) | 26 |
| - depth of field (ArkPpNodeDof) | 2 | 0.06 | 1.86 (1.67..2.06) | 20 |
| - MLAA (edge detect, edge length, blend) | 2 | 0.02 | 0.82 (0.64..1.10) | 6 |
| InitViews | 3 | 1.27 | 0.17 | 0 |
| Scene Captures | 2 | 0.73 | 0.15 | 159 |
| BloomParts | 2 | 0.23 | 0.45 | 108 |
| DisFog | 2 | 0.08 | 0.39 | 10 |
| DPG Foreground (arms, weapon) | 2 | 0.34 | 0.11 | 18 |
| ResolveSceneColor | 5 | 0.03 | 0.33 | 5 |

The indentation is a reading of the names and of which GPU figures add up, not the profiler's
tree (limit 3 below).

- **What the render thread's time is made of:** the stages above sum to about 10.6 ms of an
  11.5 ms pair. The scene is 8.0 ms of it (BasePass 3.4, shadowed lights 2.5, PrePass 0.9),
  InitViews 1.3, the scene capture 0.7. It is draw submission: 4356 draws a pair.
- **GPU, ranked:** scene 5.0 (BasePass 1.7, shadowed lights 1.6), post-process 2.6 (depth of
  field 1.9, MLAA 0.8), bloom 0.45, fog 0.39, resolve 0.33. Sum about 8.8 ms against a measured
  GPU span of 8.4-8.7 ms a tick.
- **Shadows:** 2.49 ms CPU (22 % of the pair) and 1.62 ms GPU, 1125 draws (26 %), almost all
  one dominant directional light. The stage does not split the shadow DEPTH render (the part
  that does not depend on the eye) from its projection onto each eye's scene, so the share
  that sharing could save is unknown; half of the stage (1.2 ms CPU, 0.8 ms GPU a pair) is the
  upper bound.
- **Two stages run that the settings say are off.** DishonoredEngine.ini `[SystemSettings]
  DepthOfField=False`, yet the depth-of-field node costs 1.86 ms GPU a pair, the largest single
  post stage. The game's Antialiasing option read 0 (off) at the automatic read
  (`gameopts: id 122 ... VALUE 0`), yet MLAA runs its three passes per eye (0.82 ms GPU a
  pair). Neither was a plan row. Both are full-screen GPU work, so they matter most where the
  GPU is the limit (DLSS, AFW). (The depth-of-field half of this bullet is corrected in section 10.)
- **Which limit this configuration is on.** Cutting 0.6 ms of GPU a pair (`ScreenPercentage 80`,
  `gpu/present` 4.0 -> 3.7 ms a present) returned 0.2 ms; cutting 2 % of the draws and 0.35 ms
  of GPU (`DynamicShadows`) returned 0.35 ms. Native without DLSS is nearer the render thread's
  limit than the GPU's. That is the opposite of the played configuration (2026-10-04 entry).
- What each Part B row changed, stage by stage (ms a pair, row against the two baselines):
  dynamic shadows off: dominant light GPU 1.15 against 1.53 / 1.47, CPU 1.98 against 2.11 /
  2.06, draws 1044 against 1071 / 1053. Shadow texels 0.64: dominant light GPU 1.10 against
  1.46 / 1.37, CPU -0.12. Shadow max 400: dominant light GPU 0.96 against 1.37 / 1.17, CPU
  -0.10. Distortion off: its stage gone (0.39 GPU), but DisFog +0.31 and post-process +0.33 in
  the same row (the GPU interval of the next stage absorbs it), net nothing. Screen percentage
  80: depth of field 0.42 against 1.96 / 1.72, post-process 1.67 against 2.56 / 2.69, scene
  unchanged. Light shafts, both bloom rows, half-res translucency: no stage moved.

Instrument limits found in this run (all in `stage_profile.cpp`, none fixed here):

1. The timestamp ring is too small: 46,000 to 62,000 stamps skipped per 5 s window, so only
   half of the intervals are sampled (uniformly: 0.49-0.50 for every stage at a fixed depth).
   The printed `gpu ms/s` is the sampled half. The GPU figures above are scaled by the scene
   stage's sampled share per table. 0 refused, 0 dropped, 0 unbalanced.
2. The table prints the 28 costliest stages; a small stage (light shafts, translucency) cannot
   be told from an absent one.
3. A stage's depth is the depth it was first seen at, so the `d` column is not a tree.
4. Its own cost: +0.35 ms a pair (3 %).

### 5. The script lane, named (row 12, 3 windows of 5 s, 90 ticks/s)

`pe/cost` over the whole plan: 2,780-3,080 events/s at 52-58 us, 143-171 ms/s, 1.6-1.8 ms a
tick. By section (ms/s): camera/aim 57-64, mid ticks 54-59, view rotation + hands 23-25, name
tests 14-16, front ticks 7-8. The 14 costliest statements, median ms/s (us a tick):

FovLeverApply 32.0 (355), CarryHoldTick 20.2 (224), SkcRotApply 18.8 (208), CamShakeTick 16.2
(180), camera::apply_offsets 6.1 (68), FxFollowTick 5.6 (62), anim::tick 5.3 (59), UiPeLatch
3.8 (42), CamModTick 3.0 (33), PossessionStateTick 1.5, InteractAimTick 1.1, PawnCollisionTick
0.9, ArmFollowTick 0.9, UiSurfaceTick 0.8. Together 116 of about 160 ms/s; the first four are
87 ms/s, 0.97 ms a tick. The game thread was not the limit in this run (the render thread's
idle was 0.0-0.2 ms).

### 6. DLSS: what this run can and cannot say

- Fast against K as plan rows: NOT measured (section 1).
- The helper's own GPU timer, same session: fast CNN (preset 5) at Ultra Quality 0.85-1.10 ms
  per eye image (3 lines in the lead-in, another spot, afw then reentry); Transformer K at
  Ultra Quality 3.34-3.86 ms per eye image (2 lines, the plan's spot, segment 41). **K costs
  2.4 to 2.8 ms more per eye image, about 5 ms of GPU a pair.**
- K against native at the same spot: the steady tail of the discarded segment 41 (3 `perf:
  tick` lines of 216-234 ticks, DLSS Ultra Quality K, 12.9-13.5 ms, 72-78 pairs/s) against
  baseline B12 forty seconds earlier (native, 10.33 ms, 96.8 pairs/s). **Ultra Quality with K
  was 2.6 to 3.2 ms a pair slower than native without DLSS**, while shading 59 % of the pixels.
  Not a plan segment (the instrument discarded it for the resize inside it): indicative.

### 7. HUD and ReShade checks, from the log only

- HUD (#178): `config: [Hud] SemanticOwnership=1`, `hud/semantic: hooks=1`, no REFUSED line;
  `roots=31..32 active=1 required=7 ambiguous=0` on all 300 beat lines; 0 `hud/why ... CHANGED`
  lines; `hud/task-parent ... moved=2138 refused=0`. Whether the widgets stayed in one piece is
  perceptual and not in the log.
- ReShade (#179): `reshade: ReShade.ini already as asked (before ReShade loads): every
  installed effect is loaded` - this ini carries `[ReShade] LoadAllEffects=1`, so the run did
  not exercise the preset-only list. 62 effects compiled per runtime, three runtimes (start and
  the two device resets), 0 errors, PerformanceMode 0. `reshade effects off|on` honoured. The
  log has no line for what the F10 tab listed.

### 8. Faults

- No crash (the crash file on disk is from 2026-10-03). No stage-profile refusal, no timestamp
  query refused. Two device resets, both from a DLSS resize (the lead-in, and the last restore).
- `hud/markers-sharp: REFUSED owner=serial-overlay reason=no reduced reentry upscaler and AFW
  clean sources off` is a Warn once a SECOND for as long as the state holds (842 lines). A
  state, not a change: it should log once.
- The plan's restore words are fixed text, not "the state found". On a session that differs
  from the plan's header they CHANGE the state (here: the model to K, then DLSS on). The plan
  should refuse to start, or record and restore what it found.
- The F10 panel can be opened by the controller chord during the lead-in and its Display tab
  then rewrites the configuration under the plan. A plan should hold the overlay closed, or
  log the configuration it starts in on the PLAN STARTED line and refuse a mismatch.
- The installed ini after the run differs from the one the run started with in two keys, both
  written by F10 Display in the lead-in: `[Stereo] Method=afw -> reentry`, `[Clarity] DLAA=1 ->
  0`. `AbPlanOnce` is empty as designed. `[Screen] RenderWidth/Height` and the launch file
  still say 2114x2192, so the next launch as it stands is DLSS off at 2114x2192.

### 9. Ranked, in THIS configuration (reentry, native 2750x2850, no DLSS)

| # | Lever | Measured | Visual cost | Standing |
|---|---|---|---|---|
| 1 | ReShade preset off | about 0.7-0.9 ms a pair (7-8 %), one unbracketed step | the preset's look | option (it already is); needs a bracketed row |
| 2 | DynamicShadows off | 0.35 ms (3.0 %) | all dynamic shadows gone | not worth it |
| 3 | ShadowTexelsPerPixel 0.64 | 0.21 ms (2.1 %) | softer shadow edges | at most an option |
| 4 | MaxShadowResolution 400 | 0.16 ms (1.5 %) | softer large shadows | at most an option |
| 5 | LensFlares off | 0.19 ms (1.8 %), no stage identified | flares gone | not worth it |
| - | anisotropy 4 / no trilinear, sharpen 0, Distortion, Bloom, UseHighQualityBloom, ScreenPercentage 80 | no change | - | not worth it |
| - | HUD redirect off | 0.51 ms SLOWER | - | keep the redirect |

Still unmeasured, and why: **everything in the played configuration (afw, DLSS on)** - no row
ran there; DLSS fast against K as a matched pair; the AFW HUD hand-off (the `hud off` question
of the 2026-10-04 entry was an AFW question and this run was reentry); `hud sharp off` (no
upscaler); light shafts and half-res translucency (the view had neither at a measurable size);
`LightEnvironmentShadows`; depth of field and MLAA off (found by the profile, not rows); the
hidden-area mask's real saving (only its 10.9 % share is known); the shadow depth share.

Standing of the six questions the audit asked, on this evidence (decisions are the
maintainer's; nothing was changed):

- Hidden-area mask: 10.9 % of the pixels, depth-tested passes only (about 3.3 of the 8.8 ms of
  GPU a pair here; the post chain is not covered), so at most about 0.35 ms of GPU a pair at
  native size. Against a build that needs the eye at pass start and touches every depth
  consumer, the evidence does not support building it now.
- DLSS fast model as the default: the helper's timer supports it (K costs about 5 ms of GPU a
  pair more), the matched pair row is still owed.
- Skipping empty HUD sinks: no evidence either way (an AFW question, a reentry run).
- Engine switches: none earns a shipped default. The two shadow sizes are a possible quality
  option at 1.5-2 % each.
- Script-lane gate: named, 0.97 ms a tick in four statements, no frame-time gain while the
  render thread is the limit.
- Sharing shadow work between the eyes: the stage is the second largest on the render thread
  (2.5 ms a pair); how much of it is shareable needs a static read before anything else.

Next run, if one is wanted: the same spot in the played configuration with the panel left
closed, restore words fixed first, and rows for DLSS model, DLSS off, HUD off, HUD sharp off,
ReShade effects, the two shadow sizes, and whatever handle switches the depth-of-field and
MLAA stages off (to be found: the `[SystemSettings]` flags are already off).

### 10. Run 2, prepared the same day (built, not run)

Only what run 1 left open, in the played configuration. `tools/perf-plans/audit-run2.txt`:
17 segments of 30 s with a 10 s warm-up (a DLSS model change and a DLSS resize need several
seconds), about 9.5 minutes after a 60 s lead-in.

- Rows: `hud off`, `hud sharp off`, `reshade effects off`, `dlss model k` (restore `fast`),
  then the stage profile on (so the played configuration gets its own stage table, and it says
  whether MLAA runs with DLSS on), the two shadow rows below, and `dlss off` last with a
  settling baseline after its restore.
- **Correction to section 4.** The stage called ArkPpNodeDof / `D.O.F.us` is not a depth-of-field
  pass that ignores `DepthOfField=False`. The class's parameter block carries depth of field,
  colour balance and HDR overrides together: it is the game's combined final post-process node
  (the tone map and colour grade live in it). It cannot be switched off for a row, and its 1.9
  ms of GPU a pair is the price of the final image, not a stray effect. No row for it.
- **Shadow sharing, sized before anything is built.** The engine labels its shadow depth
  render as a stage of its own (`Shadow Depths`; `Shadow Projection` is the per-eye half; both
  labels are in the image). New, default off, session only: `stages skip odd|all <stage name>`
  drops the game's draws inside one named stage (a prefix, any case) on every second present
  or on all, from the draw hooks; it needs `stages on`. `Shadow Depths` dropped on every
  second present is one eye without its shadow depth render: **the upper bound of what sharing
  it between the eyes can return**, measured, at the cost of moving shadows missing in one eye
  for 30 s. The second row drops it in both eyes as the cross-check (it should be about twice
  the first). The clear still runs, so the eye reads an empty shadow buffer; reusing the other
  eye's buffer (the actual sharing) would also need the clear held back, which needs a hook
  on Clear and is not built. If the bound is small the idea ends here. The table's header
  counts the dropped stages and draws; 0 dropped means the name matched nothing and the row is
  a baseline.
- The plan runner (`perf_ab.cpp`) gained what run 1 showed was missing: `atstart <words>` (run
  when gameplay is first reached, so a resize settles in the lead-in; here it sets afw, DLSS
  on, the fast model and the ReShade effects on), `expect <key> <value>` (keys `stereo`,
  `dlss`, `dlssmodel`; checked at the start, the found configuration is logged on its own line
  and a mismatch REFUSES the plan and runs `atend`, so a wrong run ends after a minute),
  `holdpanel` (the F10 panel is closed again if it is opened between first gameplay and the
  plan's end, with a Warn), and gameplay lost inside a segment's WARM-UP no longer discards the
  segment (a DLSS resize drops the gameplay flag for a moment; that is the lever's own
  switching cost).
- The stage profile's three limits of section 4: the timestamp ring is 8192 (was 2048) and is
  resolved at every top-level stage end (was every sixteenth), the table prints 56 rows (was
  28), and each row names the stage it was first seen inside (`in <parent>`).
- Read in run 2, in this order: the `perf/ab: configuration at the start:` line; `skip: ...
  dropped this window` on the stage headers of the two shadow rows; the `Shadow Depths` and
  `Shadow Projection` rows of baseline B1 (their CPU, GPU and draws are the split the first
  run could not see); whether an `MLAA` row exists in B1.

## 2026-10-04: pre-release audit - where the frame goes in the played configuration, and what is left

Branch `claude/performance-audit` (off staging). No game launched: every number is from logs
already on disk (the 2026-10-04 headset sessions, builds v1.0.3-36..38, VDXR, RTX 4070 Ti SUPER,
Ryzen 5 5600X) or from this record. Parsed with a throwaway script over every `perf: tick`,
`perf: present`, `perf: parts`, `perf: gpu/present` and `pe/cost` line; medians per regime.

### 1. The played configuration is not the one this record measured

The uncap deep dive (2026-09-27) measured native 2750x2850 without DLSS: 8.6 ms per pair, 115
pairs/s, GPU full. Every 2026-10-04 session ran with DLSS on (DLAA at start, then Ultra Quality
and Quality, transformer model K), and the last one mostly with `stereo afw`.

| Regime (headset, 2750x2850 output) | n | per tick / present | rate | render thread `R` | our present path `in` | of it GPU fence wait | D3D9 GPU span |
|---|---:|---:|---:|---:|---:|---:|---:|
| reentry, DLAA K, 144 Hz (builds 36/37) | 49 / 167 | 13.5 / 15.8 ms | 74 / 63 ticks/s | 6.9 | 5.7 / 8.2 | 3.3 / 5.6 | - |
| reentry, Ultra Quality K, 144 Hz | 23 | 12.6 ms | 79 ticks/s | 7.6 | 6.0 | 3.7 | 8.2 |
| reentry, Quality K, 72 Hz | 149 | 14.8 ms | 68 ticks/s | 7.3 | 7.3 | 3.8 | 6.2 |
| afw, Quality K, 144 Hz (one eye a present) | - | 9.6-9.8 ms a present | 102-104 presents/s | 4.2-4.7 | 4.7-4.9 | 2.0-3.5 (at the HUD hand-off, see 3) | 2.8 |
| for reference: reentry, native, no DLSS (2026-09-27) | - | 8.6 ms a pair | 115 pairs/s | - | - | 1.1-1.9 | 6.9 |

- The render thread is saturated in every DLSS regime (idle 0.3-0.5 ms). The game thread is not
  the limit there, with one exception: `RENDER THREAD STARVED` appears in 1-4 % of the windows.
- **The largest single cost in the played configuration is DLSS model K itself.** This record
  already measured it in the simulator (DLAA K: the game's render 2.7 -> 4.4 ms per eye, DLSS
  2.2 -> ~5 ms contended; the fast model 86 against 70 pairs/s). The headset now agrees in size:
  reentry with DLSS K runs 12.6-15.8 ms a pair against 8.6 ms native. Under AFW the budget closes
  the same way: 2.8 ms of D3D9 span plus about 5 ms of DLSS per eye image is the 9.6 ms present.
  Not a matched scene (different spots and builds), so the size is indicative, not exact.
- At 144 Hz the budget is 6.94 ms a present. AFW with DLSS K reaches 102-104 presents/s.

### 2. The script lane (the mod's ProcessEvent hook) in the headset

| Regime | hook cost | per tick | front | mid ticks | camera/aim | name tests | view+hands |
|---|---:|---:|---:|---:|---:|---:|---:|
| reentry, Quality (68 ticks/s) | 184 ms/s | 2.7 ms | 0.13 | 1.09 | 0.83 | 0.21 | 0.22 |
| reentry, Ultra Quality (54 ticks/s) | 184 ms/s | 3.4 ms | 0.15 | 1.91 | 0.81 | 0.26 | 0.26 |
| reentry, DLAA (63-74 ticks/s) | 191-198 ms/s | 2.7-3.0 ms | 0.14 | 1.2-1.4 | 0.8-1.0 | 0.23 | 0.23 |
| afw (about 100 ticks/s) | 158 ms/s | 1.5 ms | - | - | - | - | - |

2,000-3,500 script events a second at 60-73 us each. The simulator figure after route 2 was
about 0.9 ms a tick at 172 ticks/s, so a headset tick pays three times the simulator's: the cost
is per second, not per tick, and the headset ticks slower. It is not the limit today (the render
thread is). It becomes the limit the moment the render thread's wait is removed, so it is second
in line. `pe fn on` names the statements; no log on disk has that split for a headset run, which
is why plan 1 below has a row for it. The 29 "mid tick" statements run on EVERY dispatch (each
behind its own timer or lock), not once a tick: the cheap structural fix is one shared
once-per-tick gate in front of the read-only ones, after the split names which.

### 3. Our own work inside the present, per present (`perf: parts`)

| Part | reentry | afw |
|---|---:|---:|
| `hk.method` (capture + its GPU fence) | 2.2-3.2 ms | 0.6-0.7 ms |
| `hk.hudRedirectEnd` (the HUD sinks' copy, fences and five D3D11 flushes) | 0.04-0.11 ms | **2.0-3.5 ms** |
| `hk.xrEnd` | 0.08-0.24 ms | 0.29-0.32 ms |
| `hk.reshadeEffects` (CPU side only) | 0.10-0.20 ms | 0.11-0.12 ms |
| everything else together | about 0.3 ms | about 0.3 ms |

- Under AFW the wait that reentry pays in the capture moved to the HUD hand-off: with the eye
  capture at depth 2 its fence no longer blocks, and the first fence after it is the HUD's
  (`read_wait` / `blit_wait`, `hud/beat ... read waits 14641 timeouts 36`). Two readings, as in the
  uncap deep dive: (a) the GPU is full (game + DLSS helper + Virtual Desktop) and this wait is the
  only backpressure, so removing it moves it; (b) the GPU has room (`idle(d3d9)=1.5 ms` a present)
  and the HUD's synchronous hand-off is what serialises the frame. The `hud off` row of plan 1
  decides: (b) predicts the present falls toward 6-7 ms, (a) predicts no change.
- Found in the code (`hud_capture.cpp`, `end_frame`): every sink in use does its full work every
  present whether or not anything was drawn into it: a fence wait, a StretchRect of the whole
  target, a clear, a D3D11 alpha blit, a second fence and a `Flush`. With `[Hud] UpscaleSharp=1`
  the target is the output size (2750x2850). In the measured window five sinks were in use and
  three of them received 0.0-0.2 draws a present. Unmeasured GPU cost; the `hud sharp off` and
  `hud off` rows size it. Candidate: skip a sink whose target has had no draw since its last two
  copies (both slots already hold the empty image). Not built: the unconditional copy exists
  because an earlier design showed stale panels, so it needs its own run.

### 4. What is new in this audit (not in the record before)

1. **The hidden-area mask (XR_KHR_visibility_mask). Never considered here; the standard VR
   saving for a pixel-bound renderer.** The lenses never show the corners of the eye image. The
   runtime publishes that region per eye as a triangle mesh; drawing it into the scene depth at
   the near plane right after the depth clear makes every depth-tested scene pass skip those
   pixels (it is how SteamVR and Oculus titles save 10-20 % of shading; D3D9 needs nothing more
   than a depth write). It attacks P, the 6.1 ms a pair that follows pixels at native size.
   - Verified: the installed Virtual Desktop runtime (`virtualdesktop-openxr-32.dll`) carries the
     extension name; the vendored OpenXR header has `xrGetVisibilityMaskKHR`.
   - NOT known: how much of a Quest 3 eye image is hidden. Built on this branch, default off:
     `[VR] VisibilityMaskProbe=1` enables the extension and logs once per session
     `xr/vismask: eye N HIDDEN MESH ... = X % of the pixels the lenses never show`. Probe only,
     nothing is masked. That percentage is the go / no-go.
   - Design if it is worth it: hook `IDirect3DDevice9::Clear` (not hooked today); on a depth
     clear of the scene depth target (the depth probe already identifies it) at the full render
     viewport, draw the eye's mesh with pre-transformed vertices, colour writes off, Z write on,
     Z func ALWAYS, z = 0. Hard parts: the eye must be known on the render thread at the START
     of a pass (today it is settled at the present); a wrong eye hides visible pixels, so an
     unknown eye must use the intersection of both meshes. Post-process full-screen passes are
     not depth-tested and stay at full cost. Depth consumers (DLSS guides, AFW's foreground and
     depth layer, the depth share) will read z = 0 in the corners and need the mask excluded.
     Occlusion queries behind the mask report hidden, which is correct. Default OFF, live A/B.
2. **The DLSS model is the release decision with the largest measured effect.** Not new as a
   measurement, new as a consequence: the shipped default model (K) is the expensive one, and the
   played configuration pays for it on every eye image. Plan 1 measures K against the fast model
   and against DLSS off in the headset, in one run.
3. **The HUD sinks' per-present cost** (section 3). New as a finding.
4. **ReShade's GPU cost** has never been measured in the headset (only its CPU side, 0.1-0.2 ms a
   present). The bundled preset runs LumaSharpen, FakeHDR and SMAA on every eye image. New seam
   word `reshade effects on|off` (session only, the preset is not saved) so a plan row can size it.
5. **Texture filter override.** The mod forces 16x anisotropy and trilinear mips (the game's own
   maximum is 4x). On the GTX 1650 the lower setting felt smoother with the same median. Never
   sized on this GPU; a plan row does.

### 5. Deep-rooted angles, and what a static look at the engine can and cannot add

- Checked against the record before proposing anything: one engine view for both eyes (route 3),
  shared view-independent passes, InitViews sharing (1.3 ms a pair, VR-79 forbids copying the
  result), the driver's threaded optimisation (route 4, still unmeasured, no code), DXVK
  (forbidden by CLAUDE.md), capture depth (no gain when the GPU is full; auto depth ships).
- IDA cannot find a "big switch": the cost is not one function. It is pixels shaded twice (GPU)
  and two full scene submissions (render thread). What static analysis CAN do next, in order of
  expected value: (1) name the render thread's stages behind the two return addresses that hold
  37 % and 33 % of its samples (RVAs 0046C1F4 / 0046C208, "Representative CPU evidence") so a
  duplicated view-independent stage (shadow depth, scene captures) can be recognised and counted;
  (2) find where the scene depth is cleared per view, to place the hidden-area draw in the engine
  rather than at the D3D9 call if the D3D9 route proves fragile. Neither was run tonight: both
  need a hypothesis the plan's numbers will supply (how much of the frame is GPU at each setting).
- A settings-level angle the record judged with an instrument that could not see it: "dynamic
  shadows via the game INI: no gain" was measured in the SIMULATOR, which is game-thread-bound and
  cannot show a GPU saving. In the headset the GPU is the limit at native size, so that test, and
  light shafts / bloom, are unmeasured there. Worth one plan after plan 1, with the game's own
  settings (GAME_CONFIG_MAP), not new code.

### 6. Ranked, with what each needs

| # | Lever | Expected | Evidence | Cost / risk | Next step |
|---|---|---|---|---|---|
| 1 | DLSS fast model as the default (K as the quality option) | large in the played config: toward the native rate (the simulator: Quality SR fast = native) | measured (simulator), sized (headset, unmatched) | image quality is the trade; no code | plan 1 rows 1-2, then a headset look |
| 2 | Hidden-area mask | P x hidden share: at 15 % about 0.9 ms a pair at native size | standard technique; extension offered; share unknown | medium build, needs the eye at pass start; default off | read `xr/vismask` after one run |
| 3 | HUD sinks: defer or skip the empty ones | up to 2-3.5 ms a present under AFW if reading (b) holds | measured wait, cause open | touches code with flicker history | plan 1 `hud off`, `hud sharp off` |
| 4 | Script lane once-per-tick gate | 1-2 ms a tick of the game thread, visible only once the render thread is freed | measured cost, statements unnamed in the headset | low, per statement | plan 1 `pe fn on` |
| 5 | ReShade preset cost, texture filter, sharpen | unknown, each a plan row | unmeasured | none | plan 1 |
| 6 | Game shadow / post settings in the headset | unknown | prior test could not see it | none | a second plan |

`tools/perf-plans/audit-1.txt` is plan 1: nine baselines around eight rows, 20 s each, about six
minutes standing still. It needs this branch's build for the `reshade effects` word (other
builds treat that row as a baseline).

### 8. What the IDA database adds (series pf1..pf5, run the same night)

The staged database (63,921 functions, analysed once on 2026-10-04) had only been asked about
cinematics. Five one-question scripts, `tools/ida/pf1..pf5`; derivations in ENGINE_NOTES,
"Scene render stages, the draw-event switch and the SCALE command".

1. **The engine names its own render stages, and sends them through our DLL.** The scene render
   function (VA 0x0086C060, the one holding the two sampled return addresses of "Representative
   CPU evidence") brackets every stage with D3DPERF events, and the executable keeps the labels.
   - The 37 % return address (0x0086C1F4) is the call to the pass function that runs, in order:
     PrePass, Dominant light shadows, BeginRenderingSceneColor, ClearView, BasePass,
     FinishRenderingSceneColor, ResolveSceneDepthTexture.
   - The 33 % return address (0x0086C208) is the call to the function that runs: ShadowedLights
     (with ModShadow), UnshadowedLights, Translucent / Opaque / Decals, RenderSoftMasked,
     BeginOcclusionTests, BloomParts, DisFog, Distortion, ResolveSceneColor, Translucency,
     RadialBlur, LightShafts (Downsample, RadialBlur, Apply), PostProcessEffects.
   - All of it runs once per eye pass. Shadow depth rendering sits inside the light stages, so it
     is done per eye although a shadow map does not depend on the eye: a sharing candidate, but
     only if its measured share is worth it.
   - The events are gated by ONE dword (UE3's GEmitDrawEvents, VA 0x0141B268): 139 reads in the
     image, 123 directly in front of an event constructor, one writer, the `TOGGLEDRAWEVENTS`
     console handler. The game imports D3DPERF_BeginEvent / EndEvent from d3d9.dll, which is the
     proxy.
   - **Built: the stage profile** (`core/framework/stage_profile.{h,cpp}`, seam `stages on|off`,
     `stages gpu on|off`; default off, session only). `stages on` byte-verifies two sites and sets
     the switch; the proxy then times every named stage: calls, inclusive CPU on the render
     thread, draws inside it, and GPU time from D3D9 timestamp queries. A `perf/stages:` table
     every 5 s, costliest first. No hook, no stage address. This is the instrument the record
     lacked ("later render stages less precisely attributed"): one headset run says how many
     milliseconds of CPU and GPU each stage costs per eye. Not run yet: built and compiled only.
2. **The stock `SCALE` console command is in the image** (handler VA 0x00586740: SCALE, SET,
   TOGGLE, ADJUST, LOWEND, HIGHEND, RESET, DUMP, DUMPINI) with its whole switch table. So the
   engine's rendering switches can be flipped LIVE through the mod's `console` seam word, one
   plan row each, with no restart. Whether the seam's console route reaches that handler is not
   established (an earlier `setres` through it was inert); the plan's first rows show it.
   The game's current values for the ones that cost GPU time, read from DishonoredEngine.ini:

   | Switch | Now | What a change does |
   |---|---|---|
   | `bAllowDownsampledTranslucency` | False | True renders smoke, fog and other translucency at reduced resolution: the usual large saving in particle-heavy views |
   | `bAllowLightShafts` | True | a downsample, a radial blur and an apply pass per shaft light, per eye |
   | `UseHighQualityBloom` / `Bloom` | True / True | the cheaper bloom, or none |
   | `Distortion`, `AllowRadialBlur`, `LensFlares` | True | full-screen passes; radial blur is also a comfort question in VR |
   | `DynamicShadows`, `LightEnvironmentShadows`, `bAllowWholeSceneDominantShadows` | True | the shadow stages; judged before only in the simulator, which cannot see a GPU saving |
   | `ShadowTexelsPerPixel` / `MaxShadowResolution` | 1.27324 / 800 | shadow maps are sized by the subject's size in SCREEN pixels, and an eye image has 5.4 times the pixels of 1080p, so shadows are rendered far larger here than the game was tuned for |
   | `ScreenPercentage` (+ `UpscaleScreenPercentage` True) | 100 | the engine's own render scale with its own upscale: fewer scene pixels with the HUD untouched and none of DLSS's cross-process cost |

   None of these is in the record as a headset measurement. `tools/perf-plans/audit-2-engine.txt`
   is plan 2: one row each, about nine minutes.
3. Not found, and said so: no single function to patch for a large gain. The two sampled return
   addresses are whole halves of the scene render, not a hot spot.

**Order for the next headset session:** `stages on` + `stages gpu on` for a minute in a typical
view (the stage table), then plan 1, then plan 2. The stage table says which of plan 2's rows
can matter before they are run.

**One unattended run instead (2026-10-05):** `tools/perf-plans/audit-all.txt` holds plans 1 and 2
and the stage profile in 41 segments of 20 s (about 14 minutes standing still). For it the plan
file gained two directives and the ini one key: `delay <ms>` (time in gameplay before the first
segment, to reach the spot), `atend <seam words>` (run once after the summary; here it switches
the stage profile off and opens the F10 panel as the visible end), and `[Perf] AbPlanOnce=<file>`
(arms a plan for one launch; the key is emptied as it is read). The plan table holds 64 rows now.
Part A measures our own levers with nothing else on; the row "stage profile ON" then leaves the
engine's stage events on, so every later baseline and row has a `perf/stages:` table and carries
the same profiling overhead. A row is compared with the baselines either side of it; the plan's
own overall noise floor mixes the two halves and is not the reference. Not run yet.

### 7. What this audit did NOT do

No game or simulator launch; no GPU timeline; no IDA run (section 5 says why and what for).
The regime table mixes scenes and builds. The DLSS cost per eye image under AFW (about 5 ms) is
inferred from the budget and the simulator figure, not measured in the headset.

## 2026-10-04: distinguish steady FPS regression from intermittent spikes

Follow-up report attributes the largest intermittent spikes to a suspected
network issue, not a confirmed mod fault. Keep the measured XR-submission
stalls above as observations; their origin was not isolated by that trace.
The sustained regression is a separate report: approximately 110 FPS in the
same fixed test location previously reaching 125-135 FPS, possibly since
1.0.3. Turning Full-arm IK off has no perceptible effect on that average.
This supersedes the proposed IK ON/OFF/ON experiment: the reported negative
comparison deprioritizes IK, without establishing a different root cause.

No speculative performance change is included in the IK PR. Next candidate
combines PRs #168, #172, #173 and full-arm IK on a local staging-based test
branch, before any staging merge. The capture-timeout correction in #173 is
included as requested, not asserted to recover this machine's missing FPS.
Compare the combined build in the same location; retain exact configuration,
matching banner and separate sustained rate from occasional network spikes.
## 2026-10-04: frequent frame drops on IK build 13, XR submission stalls measured

Banner/proxy hash verified before interpretation: v1.0.3-13-g1c9252b4e,
A77743DE9903A5D0806919516D12C4333315B1E4832A260C2BDB6691252780EA.
Archive: build/arm-ik-install/20261004-150657-menu-visibility includes the
complete current/previous logs, tested DLL/INI/rig, appended pacetrace log and
VDXR OpenXR.log. Local quantitative extracts are under build/arm-ik-test/
run13-performance-analysis.json and run13-performance-comparison.json.
No image burst was requested or saved in this run. The restored capture
control therefore cannot explain its recurring readback-free hitches.

Configuration changed DURING this run: reentry at 2750x2850, then AFW at
log ms 14808656, then render resolution 2114x2192 at 14833546. DLSS output
remained 2750x2850; helper reinitialized and reported ready at 14837265.
Do not read the final saved INI as the configuration of the whole run.
The runtime predicted period stayed 6.94 ms; this is not a measured panel rate.

After transitions, interval 14845000..15078000 (233 seconds) contains 110
individually logged frame gaps, ALL attributed to the submission tail, plus
27 more tail gaps in suppressed-window summaries. Their measured endFrame
section takes 28.4..95.6 ms, median 45.35 ms, median 82% of the whole gap.
The broad tail label also includes other work; inspect its explicit endFrame
field, not the label alone. A further non-tail summary at the first boundary
covers preceding time and is excluded from within-window claims. At least
55 itemized/summarized gaps reach 60 ms in this interval. These are thresholded
hitch counts, not frame-time percentiles. One-second pacetrace samples remain
FOCUSED, foreground and attached; this is not evidence of headset-idle throttling.

Typical three-second windows: median present interval 9.2 ms, 108 submits/s,
game D3D9 GPU mean 4.3 ms. The latter EXCLUDES DLSS/AFW/compositor work:
DLSS separately reports roughly 2.3..3.6 ms per-eye GPU evaluations in late
windows; overlapping metrics cannot simply be added. VRAM samples stay
3023..3354 MB against a 15293 MB budget, system-backed 94..110 MB, largest
free address range 1660.6 MB. Seven of the 110 gap streaming windows have
zero uploads in the preceding two seconds. This weakens VRAM exhaustion and
texture uploads as universal causes, without ruling out driver/GPU waits.

The previous matching build 11 already had the same signature. Its AFW
2114x2192 interval 11722000..11928000 (206 seconds) has 85 itemized tail gaps
plus 15 summarized, measured endFrame 30.6..94.8 ms (median 40.9). The earlier
2026-09-18 periodic-xrEndFrame record also documents it before full-arm IK.
These sessions differ in scene/movement and settings history, so the rates
are NOT a controlled regression measurement. The new roll correction did
not originate the stall pattern; whether IK increases its frequency is open.
Current IK history ends advance/reuse/old=16766/56832/0, supporting the
counter-domain repair but not proving any perceptual flicker fix.

Separate 1.7-second and 3.5-second stalls cluster around method/resolution
changes and helper initialization. The former has endFrame=0.1 ms despite
its broad tail label; the latter sits in the method/capture stage. Do not
combine these transition stalls with the recurring 30..96 ms submit stalls.
VDXR's own log confirms runtime 1.0.10/Streamer 1.34.22 but contains no
per-frame encode/network/driver timing to identify the blocking component.

Conclusion: recurring delay localized to XR submission; origin inside the
runtime/driver/GPU completion path remains unproven. No speculative pacing
or rendering change is applied. Menu fix build 15 is installed with the
entire INI preserved. Next launch asks ONE question: in the same stationary
scene, do the large hitches stop with Full-arm IK OFF and return with it ON?
Use ON/OFF/ON about 45 seconds each after closing the menu, with no captures,
resolution or stereo changes. Log toggle boundaries and compare stable
segments. A repeatable difference implicates IK or its downstream rendering;
unchanged stalls deprioritize it and require runtime-side timing next. The
wheel, roll and flicker acceptance tests remain pending separately.

## 2026-10-03: GTX 1650 laptop - 1.0.1 vs 1.0.2/1.0.3 (MEASURED from field logs)

Remote player: GTX 1650 (4 GB, 3345 MB budget), i7-9750H, Quest 3 on VDXR at 120 Hz,
2750x2850 or 2382x2468. GPU-bound in every run (`gpuIdleMs` ~0).

- **The perceived slowdown is mostly one eye refreshing at 4-9 Hz.** Per-tick cost on the
  clean 1.0.2 normal-render run (25.5 ms) is the same as 1.0.1 (26.7 ms); what changed is
  that capture-wait timeouts refuse since 1.0.2 and starve one eye (FLICKER_REFERENCE,
  same date). Candidate `[Capture] TimeoutRefuse=0` restores the 1.0.1 behaviour.
- **GPU work added since 1.0.1, all active on this machine, none measured separately yet:**
  forced 16x anisotropic and trilinear on every sampler (`[Clarity] Anisotropy=16
  TrilinearMips=1`, the game asks for 4x); the depth-share ring (up to 359 MiB) copied
  every present, wanted by AFW; per-eye occlusion (`[Stereo] Occlusion=pereye`); the
  sharp-marker composite. VRAM median rose from 1.4-1.7 GB (1.0.1) to 2.05 GB, peak
  3.07 GB = 92% of budget (1.0.3, with FSR on).
- **The 1.0.3 run also had FSR native AA turned on in F10:** 18-28 ms of helper GPU time per
  eye on this card, more than the whole frame budget. FSR native AA and DLAA are not for
  this class of GPU; the release notes and the launcher should say so.
- Next: an A/B on this machine, one lever at a time after TimeoutRefuse - Anisotropy 0 /
  TrilinearMips 0, then Occlusion native - reading `gpuSpanMs` and `stereo: beat`.
- **Result (reported, then measured):** Sharpen 0 / TrilinearMips 0 / Anisotropy 4 felt
  smoother; ticks/s median 27.3 vs 27.4, capture timeouts 43% vs 50% of grabs, so the gain
  is in frame-time spikes, not the median. 1.0.1 at the same size: 29.7 ticks/s. Capture
  timeouts rose from ~6% of grabs (1.0.1) to 43-50%: the clearest single number for the
  extra GPU time per present. Auto depth (FLICKER_REFERENCE) removes the wait from the
  render thread; on a saturated GPU it is not expected to raise the rate (dev-PC result).
- **2026-10-04, auto depth measured:** after the step to depth 2, 30.2 ticks/s against 1.0.1's
  29.7 on the same machine and settings, capture wait 0.0 ms against 4.6. The cost is one
  present of image age (~16.5 ms). The headset's FOV is 102.2 deg (half-angles 51.1/52.1)
  and every build renders 103.0: reprojection has no margin, so fast turns show black at the
  edges on 1.0.1 as well, and the extra present shows a little more. Candidates, not
  defaults: `[Pace] Ahead=2` locates the head pose two display periods later (= the extra
  present at 120 Hz; never headset-run), or a wider ProjectionFov (margin at a sharpness
  cost of ~15% for +5 deg per side).
- Product follow-up, not built: a low-end profile (Anisotropy 4, TrilinearMips 0, Sharpen 0,
  a smaller render size) offered by the launcher for 4 GB cards, and a 72 Hz recommendation
  where the GPU cannot reach half the display rate.

## 2026-10-03: build 266 F10 ReShade controls accepted

The tester confirms normal controller operation of ReShade effects and shader sliders
inside F10. Installed v1.0.1-266-gf3bd14b91 and DLL SHA256
44151a462741a90c252e24ef410042256dd990627e5f46a1fe218a4feca874e9 were verified against
the log before interpretation. Archived current/previous logs, complete VR INI and ReShade
configuration are in main build/texture-reshade-candidate/f10-panel/accepted-run. The log
reaches normal PreExit and reports the manual runtime ready. The hook-preservation census
restores 30 actual device hooks. This confirms usability and the tested startup path, not
a controlled performance comparison or every ReShade shader. Public Enabled=0 remains;
the tester uses Enabled=1. Launcher follow-up changes do not alter runtime rendering.

## 2026-10-03: build 264 accepted; F10 ReShade controls

The tester reports good appearance and smooth operation with the tuned Carinth preset.
Matching installed DLL SHA256 99941f52ff4df38118f1704300d7c807ea62fa7fff4ee979abc1114f593f7024
and build banner v1.0.1-264-gfbc2bd23e verified before interpretation. Full logs and
configurations are archived in main build/texture-reshade-candidate/accepted-264.
The saved ReShade INI selects Carinth again; the game reaches ordinary PreExit.
Late sampled windows have desktop actual=0/off=1 and hk.reshadeEffects=162..176 us.
Those are CPU-part means, not GPU timing, frame-time percentiles or a controlled A/B.
The manual runtime startup correction is headset-accepted by this report.

Follow-up adds an F10 ReShade tab using the public API and the existing panel input.
Effect handles are re-enumerated each frame. Performance mode must be switched off
before editing parameters; switching modes queues an effect reload. Native UI event
checks change shader values and read back the changed pixels; production F10 relative
nudging passes. Shipped Enabled=0 prevents even loading an installed DLL; the tester
keeps Enabled=1 and their selected preset. Headset panel acceptance remains pending.

## 2026-10-03: manual ReShade startup crash reproduced and corrected in native host

Build v1.0.1-262-gabf374ce3 failed on startup creating a 256x256, single-mip DXT5
texture with D3DERR_INVALIDCALL. Installed DLL SHA and log banner match. Archived
VR current/previous logs, INI and ReShade files are in main build/texture-reshade-candidate/
crash-262. VRAM was 506/15,293 MiB, with a 2,044.8 MiB largest free address range;
this evidence does not support an allocation-pressure diagnosis. Mirror-off did skip
the first native Present, but the run provides no gameplay performance result.

**Measured cause:** the native PURE D3D9 device rewrites its dispatch table when
BeginStateBlock is called. ReShade uses state-block recording. A native reproduction
using production device_census, d3d9ex paged backing, official ReShade 6.8 and the game's
PURE/HWVP/FPU_PRESERVE flags succeeds creating MANAGED DXT5 before ReShade, then fails
with 0x8876086c afterward. The CreateTexture table entry changes from our hook to the
native entry, while the census remains at zero failures because the call bypasses it.
A direct BeginStateBlock/EndStateBlock control reproduces the table rewrite as well.
The earlier 510-check effect host did not install the game's hooks and missed this.

**Correction:** save this module's 119-slot base-device detours before runtime creation,
restore overwritten detours on return from manual rendering and runtime destruction,
and discard the retained device on reset. Native entries and other modules' entries
are not overwritten. No settings/default or texture-backing policy changes.

The extended native host passes 759 checks with effects enabled and 759 disabled:
DXT5 creation, lock and upload after each ReShade update; preserved texture hook;
effect pixels, render state and viewport; runtime destroy/reset/recreate; zero native
Present calls. Eight creation hooks are observed restored. This confirms the reproduced
failure is corrected in the host; game startup and headset performance remain unverified.
Installed v1.0.1-264-gfbc2bd23e after release build/export/lint checks. The entire
VR INI is byte-identical (73,485 bytes, 1,684 CRLFs); ReShade INI and presets are unchanged.
DLL/INI/log backups, expected INI and installation hashes are in main
build/texture-reshade-candidate/state-block-fix/install.json.
Next launch has one question: does the same save load normally with the no-effects
ReShade preset? Success supports the startup fix; another crash requires its matching
banner and failure evidence before proceeding to performance testing.

## 2026-10-03: ReShade lag with effects disabled and forced desktop mirror

**Reported:** build v1.0.1-257-g705b282c7 becomes very slow with ReShade, including
with effects disabled; desktop mirror stays visible despite its off setting.
The installed DLL hash and log banner were verified before reading the run. Archived
VR log, previous log, full VR INI, ReShade log/config/preset are under main build/
texture-reshade-candidate/reshade-lag-257. The tested resolution was 2114x2192.

**Measured/source-confirmed:** ReShade successfully compiled all seven original
preset effects. The bridge's active branch directly calls native Present and bypasses
`desktop_eye::present`, including DesktopMirrorOff=1 and DesktopMirrorStrictOff=1.
Representative late gameplay windows show 13.4-13.8 ms Present intervals, about 7.1 ms
inside native Present, 3.7-4.3 ms game GPU time and 6.8-7.7 ms D3D9 idle. This supports
forced presentation as a contributor; it does not isolate total ReShade GPU overhead.
The native presentation interval was already immediate, so another vsync INI edit
would not repair the bypass.

**Candidate:** optional [ReShade] ManualRuntime=1 disables ReShade graphics interception
before loading the DLL, then uses its official create/update/destroy effect-runtime API
on the raw D3D9 swapchain. Effects run before the existing VR capture and desktop policy.
The API's update call renders but does not call native Present. Thread-local guards
keep its draws, state changes and resource creation out of the game classifiers and
managed-shadow translation. Reset and both PreExit paths destroy the runtime before
releasing/resetting the device; the runtime pointer is cleared before destruction to
handle nested Release calls. ManualRuntime ships 0 pending headset validation; changing
integration mode requires restart. Scroll Lock retains live effects on/off. Manual mode
does not observe game draw/depth events, so depth-dependent presets need legacy hooks.
The selected colour/sharpening preset has no active depth-dependent technique.

**Host validation:** tools/reshade-manual-host.ps1 runs the production runtime module
on a hidden native D3D9Ex device. Enabled effects turn synthetic red pixels cyan;
disabled effects preserve red. Both modes restore render state and viewport and pass
reset/recreation, without native Present calls: 510 checks each. The first test shader
used bitwise operations unavailable in vs_3_0; replacing that test-only arithmetic
resolved compilation. These checks prove API/capture order, not headset smoothness.
The RelWithDebInfo build, 11-export check, lint, 86 launcher unit checks, full installer
smoke (including exact binding edits and CRLF), and 57 updater checks pass.

**Next launch, one question:** with ReShade loaded, its test preset empty, the desktop
mirror off and the same save/resolution, does headset performance return to its usual
smoothness? Improvement supports the forced-present/interception cost explanation;
persistent lag means it is not sufficient. A crash or blank headset is an integration
failure. Read the matching new build log and ReShade log before another experiment.
Install changes are limited to [ReShade] ManualRuntime=1 and [Perf] Parts=1 in the VR
INI, plus selecting a separate empty ReShade test preset. Keep the user's tuned Carinth
preset unchanged. Archive and byte-diff the complete installed INI on installation.

Official source: [manual runtime API](https://github.com/crosire/reshade/blob/v6.8.0/include/reshade.hpp),
[implementation](https://github.com/crosire/reshade/blob/v6.8.0/source/addon.cpp),
[graphics-hook opt-out](https://github.com/crosire/reshade/blob/v6.8.0/source/dll_main.cpp).

## 2026-10-02: community texture mapping and ReShade integration (HOST MEASURED, headset pending)

Work: `codex/vr-133-texture-reshade`, branched from staging `1c47937a6`. VR-133 already
tracks the SYSTEMMEM allocation / LockRect crash. A separate issue could not be created
because the Linear workspace reached its free issue limit; the camera portion of VR-133
remains separate and must not be closed by this integration.

Source audit: the supplied source archive's Git baseline is released v1.0.2 `cecdae230`.
Its 14 changed source files plus a new ReShade add-on contain paged CPU texture shadows,
concurrent mip/face locks, post-effect XR capture, subtitle readability and a 450% ceiling.
Only reviewed source was ported. Donated binaries and scripts were not executed.
Archive SHA256: source `442C3889C956A817E26B4F975C9FA85B9FC6E9897094C42E70441429D5424DB8`;
binary archive `8417C5E93B0725B936478770AB2D869AE8BCBE541583A8B7568238446672FF6E`.

### What pagefile backing does and costs

`Managed=paged` creates a pagefile-backed section for each translated 2D/cube texture,
maps only currently locked mips, then unmaps on unlock. Distinct mips/faces can overlap.
Writes copy into a cached full-chain SYSTEMMEM staging texture and use UpdateSurface at
the corresponding mip. Read-only unlocks skip uploads. Volume textures and formats whose
layout is not supported retain native SYSTEMMEM twins. The cache has eight slots and a
128 MiB target; a single larger texture may exceed that target after older entries are dropped.

This trades persistent **32-bit virtual addresses**, not total backing storage. Windows
reserves system commit for the full mapping. Unmapping does not discard the CPU texture.
Mapping is not an unconditional disk read, but memory pressure can cause paging and stalls.
No Windows pagefile setting is changed. See Microsoft's
[CreateFileMapping documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-createfilemappinga)
and [file mapping overview](https://learn.microsoft.com/en-us/windows/win32/memory/file-mapping).

Native x86 host, same machine, real D3D9Ex HAL, 80 serial 4 MiB fill/upload iterations per
mode, identical 1024x1024 A8R8G8B8 resource. These are CPU wall times through UnlockRect,
not GPU completion timestamps or a game-FPS estimate. The constant shape favors cache reuse.

| Run / mode | Median | p95 | Maximum |
|---|---:|---:|---:|
| Initial conventional shadow | 0.326 ms | 0.793 ms | 2.780 ms |
| Initial paged shadow | 1.632 ms | 2.250 ms | 2.375 ms |
| Final conventional shadow | 0.339 ms | 1.008 ms | 3.057 ms |
| Final paged shadow | 1.603 ms | 1.927 ms | 2.544 ms |

Separate section-only experiment: 64 mappings of 4 MiB were filled and unmapped while their
handles stayed open. Persistent available-VA loss was 0.00 MiB; system commit increased by
262.84 MiB initially and 257.12 MiB in the final run. System commit is machine-wide and can
move with other processes. This experiment excludes staging/GPU allocations and does not
prove zero hard page faults in gameplay. Evidence: local `build/paged-tests*.log`;
reproduction tool `tools/paged-texture-host.ps1`.

Verdict: useful optional mitigation for address exhaustion with large texture packs,
with a measured per-upload cost. Keep conventional shadows as the public default.
No claim that all reported crashes are fixed, or that gameplay performance is unchanged.

### Hardening and validation

- Lock bounds, mip counts (including requested zero/full chain), faces, block alignment,
  duplicate locks and mapping sizes are checked; publication protects section identity.
- Unlock keeps its subresource claimed during upload; ordinary release no longer scans
  8192 lock slots when there are no outstanding locks. Staging releases at Reset and PreExit.
- Failed backing allocation refuses resource creation rather than returning an unusable texture.
- Host GPU readback confirms pixels from levels 0/1; DXT1/DXT5 full mip chains upload,
  including tiny tail levels. Concurrent mips and six cube faces, read-only persistence,
  invalid requests, release and staging cleanup pass.
- `device/paged` and `device/paged-cost` report backing, mapped/peak bytes, upload totals and
  maximum, cache hits/misses and available commit/RAM every 60 seconds in paged mode.
  These counters do not infer disk I/O. Existing streaming bursts still carry upload timings.
- Launcher unit suite: 86 passed. Scratch installation/update/rollback/uninstall passes,
  including exact whole-INI comparison for texture selection, omission and invalid flags.
- Production HUD shader passes existing circle/crop/alpha checks and new subtitle off,
  color, black-outline and premultiplied-background pixel checks. The initial outline test
  omitted inverse texture size (supplied by the game); correcting that fixture made it pass.

### ReShade and other integration limits

The launcher optionally downloads the official 6.8.0 full add-on setup from
[ReShade](https://reshade.me/#download), checks SHA256
`AFE4C8F13048306307983B8B3D41D5BF00A86820440B0E57DEA10950E1176445`, extracts only the
PE32 runtime as `ReShade32.dll`, and never executes the setup. Extracted DLL SHA256:
`DA430E0A9C6EECEFA0D1B27D05E16C426FB5D04E808B194D914EAAC4B31BC0F8`.
The mod remains `d3d9.dll`. Existing runtime gets a unique backup; existing presets/settings
are preserved. Download mismatch leaves game files unchanged. Tests cover these cases.
Runtime/shader binaries are not redistributed; shader packages remain a separate opt-in.
ReShade is [BSD licensed](https://github.com/crosire/reshade/blob/v6.8.0/LICENSE.md).

The rebuilt add-on uses official API 20, event 75 (`reshade_present`). The pending XR tail
belongs to the Present thread and can execute only once. It preserves staging's AFW/MSW
cycle ownership and foreground mask work. A missing callback finishes XR and disables the
bridge for that run. While active, ReShade needs native Present and bypasses mirror-off
Present suppression, so its cost must be tested independently. Actual headset effect capture
and stereo compatibility remain unverified; an extraction test is not a rendering test.

Subtitle color/outline/background controls and the community rectangle/enlargement preset
are optional. The proposed rectangle is not a locally measured universal subtitle region.
The resolution limit is 450% (5834x6046), not a new default. The earlier Basic Stereo/AFW
Display placement is retained from PR #168 without merging that PR.

### Installed candidate identity

Source build `v1.0.1-257-g705b282c7`, RelWithDebInfo, legacy off. Installed DLL SHA256
`B469222B1E798715FC4B08A940787CDC096DC98476A05ACE6AFFE61A376FD1C1` matches the built DLL.
The previous installed/log identity was `v1.0.1-258-g949e1ea8b` (different branch ancestry,
not a semantic version downgrade). It is archived with the full INI and both logs under
main `build/texture-reshade-candidate/before-256`. The new log banner is not yet available.

Entire installed INI equals the expected target, SHA256
`D2112CD2BD961E979D0C3312AC9AAA514291CD3A9543CC200B50395B038804CF`, 73348 bytes,
1676 CRLF lines and no bare LF. Only Managed=shadow -> paged and ShadowSurfaces=0 -> 1
changed. Existing Ex=1, ShadowFullCopy=1, AFW and MSW off remain. No ReShade runtime was
installed for this texture test. Cost diagnostics are automatic at Info in paged mode.
The earlier Display UI request is retained in this binary despite its separate PR ancestry.

The first local install had a dirty build stamp caused by unrefreshed Git metadata after
CRLF normalization. It was not playtested. Refreshed index metadata and rebuilt cleanly;
the pre-reinstall DLL/INI/log pair is archived in `before-257`; final installation
changed no INI bytes.

### Next launch, one question

Install the paged candidate with Ex=1, Managed=paged, ShadowSurfaces=1, ShadowFullCopy=1;
keep ReShade absent and subtitle effects off. Load an existing save and quickload that same
save three times. **Does every load return to gameplay without crashing?** Passing supports
baseline loading stability only (and does not reproduce a texture-pack crash if no pack is
installed). A crash keeps the mitigation unaccepted and the matching log/dump is the next
artifact. Verify the installed DLL hash/log banner before reading this run; archive both logs
before any relaunch. A texture-pack stress run, controlled frame-time A/B, and ReShade/subtitle
headset checks follow as separate launch questions.

## 2026-10-02: spacewarp follow-up shelved for patch preparation

Further SSW/mod-spacewarp experiments are parked by user direction. The installed build-253
source is integrated into staging through PR #167, including the earlier pacing work and
optional guards. Existing branches, plans, measurements and unresolved perceptual questions
remain preserved; integration does not establish a new headset verdict.

The patch UI keeps Stereo and experimental AFW selection in Basic. All remaining stereo
section controls, including SSW/depth and mod-spacewarp, require Debug. No rendering or
shipped-default change accompanies this UI gate. The installed patch candidate retains
ModSpacewarp=0 and SubmitDepth=0. Resume research from the existing evidence below only when
requested; there is no active spacewarp test queued for this patch.

## 2026-10-02: Cyberpunk VR port frame generation reviewed for MSW (RESEARCH, nothing built)

The Cyberpunk 2077 VR port's 0.1.7 frame generation interpolates (midpoint between two real frames, the
newer held one slot) with FidelityFX frame interpolation on D3D12, from engine motion vectors, depth and
optical flow; the game is limited to half the refresh. Their measured cost at 2560x2560 per eye on an RTX
5070 Ti: FidelityFX about 2.5 ms per frame and 456 MiB, the NVIDIA optical-flow hybrid about 7 ms and 279 MiB;
45 real plus 45 generated at 90 Hz in the simulator. Their numbers, their GPU: not a prediction for ours.
What carries over to MSW (discontinuity reset, bounded extrapolation, repeat counters, one pacing schedule,
no game-rate hand smoothing) and what does not (interpolation latency, the GPU cost on a GPU-bound rig) is the
integration plan in [PLAN-mod-spacewarp.md](PLAN-mod-spacewarp.md) section 7.

Offloading to the x64 helper, recorded so it is not re-asked: the helper shares the GPU and the CPU with the
game. Moving the GAME's rendering out of process is not feasible (UE3 issues D3D9 calls from its own render
thread; streaming them to another process costs more per draw than it saves, and the DXVK route is retired).
Moving the MOD's own GPU work (synthesis, upscaling, resolve) there only helps where it can overlap the game:
D3D12 async compute against a D3D11 immediate context that today serializes it. That is a possible gain of
our post-work's share, not of the game's frame, and it needs shared fences both ways. Unmeasured.

## 2026-09-30: build 248 live slot ordering succeeds; residual deadline misses (MEASURED)

Identity: installed DLL and log `v1.0.1-248-gdc57a1c05`, SHA256
`5B02B32615DDF859CC4030C6FA18CA01C14E3D0F06E2E792056436DAF4CCA4F8`. The full run and
previous log, DLL and INI are retained in main `build/msw-run31-analysis/baseline-248`.
Headset report: large improvement, residual small hitches and foreground jitter during head
sweeps, mainly with MSW on. This is not final acceptance of smoothness.

The approximately 49-minute session contains two active MSW intervals, log ms
59619796-60477593 and 60852703-61316421. Of 439 slot-order windows, 386 have at least 60
real and 60 synthetic submissions/s and zero not-ready counts. This operational steady-window
filter excludes startup/loading stalls but cannot prove every remaining sample is gameplay.
Those windows contain 484 target gaps over 1.5 display periods, zero non-increasing targets,
zero consecutive real submits, and zero Present assists. 129 windows have no target gaps.
Mean rates are 71 real and 71.583 synthetic submissions/s. Alternation works live; average
rate alone still hides deadline misses. CSVs remain local in `build/msw-run31-analysis`.

During active MSW intervals, existing gap classifications include 127 pre_tick, 113 present-tail
(XR end), 41 out/idle, ten capture, and one each gameTick, out/R and desktop. Some include
loading/resolution changes and are not a steady-gameplay ranking. One 44 ms gap near a bad
steady window contains 32.5 ms pre_tick; MSW owns the same frame mutex there. The old budget
cannot distinguish the worker's XR wait, eye work and submit from ordinary real-frame work.
Low sampled VRAM usage and small streaming reads there do not establish a GPU paging cause.

Instrumentation now measures CPU wall time for the synthetic cycle's wait, view locate,
eye construction, XR end and total. Eye time includes swapchain acquire/release, context lock
and GPU command submission; it is explicitly NOT GPU execution time. Rate-limited mean/max
snapshots use try_lock between complete cycles, so logging does not wait on Present or mix
part of an assisted cycle. Counter baselines initialize at thread start, avoiding inflated
first-window rates when MSW is toggled back on. No new GPU query or engine memory writer.

Configuration candidate: enable existing ModSpacewarpHands, disable Perf FrameId and re-enable
MSW, preserving all other settings. FrameId was performing synchronous D3D9 image readback
every eight pairs. Removing it reduces diagnostic work, but earlier FrameId-off/diagnostic A/B
tests found NO repeatable FPS/tail benefit (see the prior experiment records below). That failed
prediction remains valid evidence; removal is a low-overhead baseline for the newer MSW path,
not a proven hitch fix. Leave GPU-memory polling and other diagnostics unchanged.

Next decision: first obtain the single hand-follow ON/OFF/ON perceptual comparison described
in FLICKER_REFERENCE. Read stage timing and target gaps from that run to choose the next
hitch investigation. A large XR end tail, eye-build tail or wait tail requires different follow-up;
do not infer GPU cost from CPU wall time or change prediction clamps from lifetime maxima
that include loading. Remaining smoothness is open, and no new performance gain is claimed.

## 2026-09-30: half-rate slot ownership and prediction timeline (HOST-VERIFIED candidate)

The request concerns ModSpacewarp in F10. Build 242 remains the headset-accepted AFW baseline
with this feature off; the current work has no new headset verdict yet.

Code finding: the half-rate worker assigned `seenEnds = ends` before trying `g_cycleMx`. A lost
try_lock consumed the only notification of that real frame. Also, a fast next Present could win
the recursive mutex repeatedly; Windows mutex fairness does not reserve an alternating slot.
The overdue filler sometimes hid the loss, but could not guarantee real/synthetic alternation.

Correction: under the existing mutex, a successful real stereo end records one pending synthetic
slot. The worker consumes it only with the lock. If Present wins first, its outermost entry
services it before any real XR cycle; nested Present cannot do so. The overdue filler consumes
the same pending slot, preventing duplicate service. Off, stopped, adaptive, and blocked runtime
states cancel it without adding a wait on the worker. No engine memory writer is introduced.
The synthetic work in Present assistance is included in the existing blocked-time accounting.

Host evidence: `tools/msw-slot-host.ps1` extracts the production service and cycle entry/exit.
12/12 checks pass, including 200 frames alternating worker wins and Present wins with deliberate
contention before every unlock. `-OldControl` models build 242's consumed-before-lock sequence:
8 pass / 4 fail, including loss of the token and broken alternation. This proves the ownership
correction on the host, not XR driver deadlines or end-to-end headset cadence.

New three-second `msw: slot order` line reports worker/Present-assist counts, target intervals
over 1.5 display periods, non-increasing display targets and consecutive real submits. A steady
half-rate run should have roughly 72 real + 72 synthesized slots/s at 144 Hz, and zero in the
last three counters. Missing GPU/runtime deadlines may still produce target gaps even with
correct ordering. Counts use successful stereo submissions, with the chain reset on non-stereo
frames; warm-up, focus changes and loading are not steady-state pacing evidence.

Prediction also had a timing mismatch: capture arrival intervals measured delivery stalls,
while the synthesized head pose targeted an XR display slot. Native images now carry the XrTime
of their real submission. Body motion is measured per submission interval and advanced to the
synthetic display time, retaining the native pipeline's fixed latency instead of treating a late
readback as later game motion. Both eyes share that target even though one native image is older.
Missing/reversed timing refuses prediction; the old wall-clock path is retained only for callers
that explicitly supply no display timestamp (host/replay compatibility).

55/55 GPU warp checks pass, including unchanged accepted wall/hand checks and new independent
capture-clock and stale-camera-writer controls. Build 242 fails six of the 53-case suite before
the off-origin pivot cases were added. Final host informational cost: grid-4 synthesis 0.703 ms
per eye, full AFW rebuild 1.444 ms at 2750x2850; this is not an in-game performance comparison.
Geometry details and perceptual limits are in FLICKER_REFERENCE. Next user launch asks only
about stick-turn geometry; read pacing counters from that run without conflating the verdicts.

## 2026-09-30: accepted build 242 baseline; mod-spacewarp pacing follow-up

AFW wall and hand/head-motion corrections are headset-accepted on `11dcf7db9`, with
ModSpacewarp off. The next request explicitly concerns the mod's F10 spacewarp, whose remaining
reported faults are one-frame world ghosts during right-stick turning and uneven pacing even
when the reported frame rate is high. A high average rate does not establish even slot delivery.
No new timing cause or fix is claimed from this acceptance run.

Plan after the staging consolidation, one behavioral change and one headset question at a time:

1. Preserve build 242's DLL/INI pair and accepted AFW source. Confirm MSW owns synthesized slots
   in the next evidence run, including half-rate lock, extrapolation and lead settings. Keep 144 Hz.
2. Inspect existing `msw:`/XR timing and identity instruments before adding diagnostics. Correlate
   real and synthesized submissions, display targets, source-image age, lock/wait ownership and
   skipped or repeated slots. Quantify interval distributions rather than only mean FPS.
3. For stick turns, distinguish one-frame stale world/turn prediction from disocclusion by comparing
   the synthetic slot with its identified real source and target. Preserve independent controls for
   extrapolation and scheduling; do not infer that AFW's hand-side fill explains all MSW ghosts.
4. Validate scheduling and geometry candidates on the host with failing old-code controls. The
   tester launches; no agent-launched game. Judge turn geometry and perceived pacing separately.

New Bug creation was refused by Linear's free issue limit; this remains under parent VR-39.
Visual evidence and acceptance identity are in `FLICKER_REFERENCE.md`.

### Parked extra-pair experiment (#140), retained when closing the old PR

Source branch `claude/extra-pairs-per-tick`, verdict commit `7cda0c54b`, is retained. Its headset
test on `v1.0.1-106-g10f9cd0ab` at 144 Hz with DLSS Quality SR showed 120-126 pairs/s with or
without the extra pair, world ticks around 60/s and increased hand/weapon judder. The earlier
simulator gain of about 18% used DLSS off; each extra pair duplicated DLSS's per-image cost in
the headset configuration. The experiment is not part of build 242 and is not being merged.
Do not reopen it as a pacing remedy without evidence addressing those failed predictions.

## 2026-09-29: DLSS object motion cost (VR-39, host)

Measured on the host at 2114x2192 per eye image (RTX 4070 Ti SUPER, 20 frames between timestamps after a
warm-up, the camera vectors' own time subtracted):
- 0.25 ms with a still camera and one moving character;
- 2.05 ms when every tile disagrees with the camera.

The static world costs one early-exit pass (about 0.15 ms with no character). Method and traps:
`PLAN-motion-vectors-dlss.md`, "Object motion".
## 2026-09-28: AFW slot fill rate and the mod spacewarp's cost (MSW, estimate)

- **Measured (run 8, AFW at 3012x3122, 144 Hz):** 87-91 presents and submits per second; the
  `stereo: rate` line says 0.60-0.63x of display slots filled.
- **Estimated, not measured in the game:** MSW's cost is one seed map and a compose per eye per
  synthesized slot. That is about 1 ms per eye (from the host's 1.54 ms full rebuild at 2750x2850), about
  2 ms per slot, and about 110 ms of GPU per second at 54 slots/s.
- **Expected pacing:** a game that manages about 90 on a 144 Hz display is paced to the slots it can make
  (about 72 real plus 72 synthesized).
- The `msw:` log line prints both rates. See `PLAN-mod-spacewarp.md`.

## 2026-09-27: sharp-marker overlay resource budget, unmeasured candidate

MarkersSharp defaults off. Six output-sized RGBA8 shared images and one D24S8
surface cost about 209 MiB at2750x2850 before driver overhead. It adds a per-image
D3D11 over pass and a flush after reads; fences refuse rather than spin. No
performance gain or headset cadence result is claimed. Measure against lever off
after native-marker coverage is confirmed. Architecture and HUD_ANCHORS describe
ownership and guard limitations; keep future performance results in this file.

## 2026-09-26: walking discovery correction accepted locally

Installed DLL hash and log banner match v1.0.1-50-g6bc58a449. Final local report
accepts the run after the periodic walking catch-up correction. Archived current and
previous log/full INI: primary build/hud-regression-20260925/walk-accepted-002458.
Across64 printed collect-cost windows,269 collections have weighted mean6.349ms
and maximum9.653ms, including fresh live table plus discovery. These windows include
changing scene/state and are not a controlled whole-frame comparison. The new
counter had no old-build equivalent; no exact speedup or zero-overhead claim.
The750ms schedule remains; scan membership/range probing changed. Preserve the fix.
CPU flight recorder/pixel/legacy flags are OFF in the accepted local build. Integration
retains the opt-in recorder with one history frame formatted per Present. Normal
build validation does not establish remote end-to-end diagnostic overhead.

## 2026-09-26: periodic walking hitch, candidate discovery optimization

Verified installed95ae3f7af DLL/banner. Archives: primary
build/hud-regression-20260925/walk-judder-000731 (current/previous logs, full INI).
Player confirms potion HUD now placed correctly; quickMode4/quickReady1 and
quickCaptured192->1566 independently prove activation. New reported surface is
whole-world translation during steady walking, brief hold/forward catch-up at
roughly one-second intervals despite similar average FPS. Not HUD decoupling.

In gameplay59682000..59737000, repeated printed frame gaps are mostly53..62ms
waiting for the game thread, with adjacent stalls often750ms apart (also735/765ms
clock quantization). Example59684328->59685078:56/57ms,52.8ms out/idle in each.
The gap logger rate-limits after3 events/window, so printed intervals are censored.
Ordinary ticks are around8..10ms; averages hide individual missing frames.
Live-table summaries max1.24ms then1.11ms do not explain the50ms stalls. Some
separate xrEndFrame stalls and streaming bursts also exist; not all gaps have
one established cause. Flight recorder/pixel diagnostics remain compiled out.

Source lead: hand SkelTick recollects components every750ms. FpCollect probes
376 raw pointer slots per expanded object, calls RangeReadable for every slot,
and LooksLikeObj on arbitrary scalar values. This exact periodicity and the
expensive discovery path make it a strong candidate, not proven stack attribution.

Candidate keeps the750ms schedule, search depth, candidate cap, equipment roots
and menu/load recovery. Rebuild the live-object hash table at collection entry
before retained-object restores or discovering new equipment. Check root and
child membership via IsLiveObject before dereference. Validate the entire scan
range once per object; when partially readable, retain the original per-slot
boundary checks. Typical range queries drop376->1 per expanded object. No camera,
movement, stereo or accepted HUD policy changes.

Add one3s aggregate handmesh/collect-cost line (mean/max/last, including live-table
refresh) to distinguish successful optimization from unchanged hitching. Six host
checks extract the actual production scan and verify both range endpoints, retired
and arbitrary pointer rejection, one-query complete ranges, and guarded partial
page fallback. No in-game timing gain claimed before a matched acceptance run.

Next single question: same-save straight walking, does the periodic hold/forward
catch-up disappear? Compare collection cost and frame gaps; continued stalls with
cheap discovery would reject this hypothesis and direct investigation to the
remaining game-thread work. Never disable liveness checks to gain performance.

## 2026-09-25: remove failed cross-movie activation census

Matched36a8d7f95 DLL/banner and archived logs/full INI under primary
build/hud-regression-20260925/potion-root-return-235618. Actual mode4 has zero
quickCaptured throughout; active potion-capture cost is still unmeasured.

Correction removes the per-poll base-HUD movie lookup and31 sprite comparisons.
The existing direct potion-owner identity/mode/view checks remain, as does the
single relaxed counter on successful outer ownership. No extra GPU resource,
readback, engine call or log stream. Summary remains3s.97 host ownership checks
and100000 transfers pass, including production activation through queue replay.
Host costs16.65ns unowned wrapper/21.51ns queue roundtrip are not in-game timings.

## 2026-09-25: quick-potion return and ownership-link correction

Verified b52c0c579 installed DLL/banner; current/previous logs and full INI archived
under primary build/hud-regression-20260925/quick-potion-return-233600. User reports
acceptable performance.15 existing perf samples have median8.6ms, range7.5..817.8ms
(the whole run includes transitions, so these are not controlled A/B timings).
All28 semantic summaries report movieLink=0, quickReady=0; final transport overflow0.
This supports only the inactive-extension baseline, not the cost of active capture.

Correction changes one native field read and validates the actual view table.
All current clip/movie relationship checks stay on the existing UI poll, max32
comparisons, with no per-draw heap work. Mode is read regardless of linkage to
avoid misleading counters. A successful outer potion owner adds one relaxed
counter increment; inherited children do not repeat this ownership validation.
The existing summary remains once per3s; no new log stream, capture sink, GPU
readback or engine call is added. CPU ownership checks use existing live-object
membership plus guarded native reads. Active potion rendering still adds normal
commands to the already allocated default panel; real active cost remains for
the next matched playtest, not established by the inactive log.

Host checks:85 ownership/reader tests and100000 concurrent transfers pass;
123 native-HUD and503 routing tests pass. Host replay-wrapper15.06ns and queue
roundtrip23.12ns are microbenchmarks, not measured in-game frame savings.

## 2026-09-25: failed tutorial trial and returned performance report

Returned e322911fb is reported slightly slower. Whole-run perf tick medians:
9.1ms/58 summaries versus fe3c3f8768.3ms/101 summaries. Different scenes, menus,
durations and lens settings make this an uncontrolled comparison, not attribution
of0.8ms to the change. The tutorial sink was allocated55995281 and released
56005593, a10.312s interval; it did not remain copying for the rest of the run.
Slots were1375x1425 (Scale0.50) against2750x2850 targets. The extra panel therefore
had real GPU allocation/copy cost while visible, but did not capture the icon.
Remove it. No flicker recorder, pixel probe or new high-frequency log was enabled.

Replacement quick-potion capture reuses the existing default panel, introducing
no additional sink, readback or per-draw logging. Native sprite/movie reads only
run for unowned Display roots while a mode4 wheel view is available; children
inherit scope. Live owner/membership checks run only after a matching movie link.
Optional field resolution retries at most every5s if unavailable. The existing
poll cross-checks the view against known HUD clips; existing3s summary gains three
scalar fields. Host81 checks plus100000 concurrent transfers pass. Queue round
trip21.07ns and unowned replay16.78ns are host microbenchmarks, not in-game timing.
Headset frame-time recovery remains unconfirmed; no claim of restoring a measured
FPS amount from the uncontrolled runs.

## 2026-09-25: semantic tutorial placement cost

No new diagnostic counters, stacks, readbacks or log cadence. Candidate adds one
existing full-resolution capture panel while identified tutorial content draws.
At2750x2850 RGBA8 one image is31,350,000 bytes; this is not a zero-GPU-cost change.
The panel is retired after the existing two-present grace without draws; the
capture subsystem releases inactive targets/slots and skips their copies. Thus
occasional heal reminders do not add an idle copy for the rest of a level.
129 native-HUD policy/math checks pass. No headset GPU timing claim is made.

## Accepted ownership run and small follow-up cost (2026-09-25)

Verified4c38bf526 run52040281..52879xxx reports improved HUD stability; not an FPS
A/B measurement. Last ownership snapshot has66994508 queued,66993732 replayed,
4113901 known HUD draws and869382 native fallbacks, cumulative, overflow0.
The difference is outstanding or retired generation work, not proof of a drop.
Unknown marker roots explain a portion of fallback, not every unknown draw.

Follow-up changes only marker target validation and three clip-to-element routes.
The same3s diagnostic now includes family-root counts and fresh pivots. Count
computation is behind both the log-level and time gates, not done per draw or
on every UI poll. No new GPU diagnostic, draw census, stack tracing or recorder.
New70-check production-reader/transport suite retains100000 concurrent transfers;
latest host queue round trip23.20ns, unowned wrapper18.39ns. Host timings exclude
native marker shader transforms now becoming reachable; do not claim zero whole
frame cost for restoring those existing transforms.

## Semantic HUD transport candidate cost (2026-09-25)

Optimized x86 host tests of production CommandOwners and extracted replay wrapper:
1,000,000 iterations measured about 22.49 ns per put/take and 17.93 ns per unowned
wrapper around a trivial native command. Earlier repeats were 21.26/15.14 ns.
100,000 acknowledged cross-thread address reuses preserve payload identity. These
are host microbenchmarks, not engine FPS, full hook cost or GPU measurements.

Unowned command consumption has an empty-table atomic fast path. Probe misses use
loads before compare/exchange; they do not execute eight locked CAS operations.
The transport allocates a fixed table once, with no per-command allocation or log.
Publication reads the borrowed native allocation record under SEH, avoiding a
VirtualQuery per command. Root discovery/membership still range-checks pointers.
Display performs bounded binary search under a shared lock, then live-membership
validation only for a recognized root. The existing UI poll rebuilds at most 288
root records; full GObjects refresh happens only on load/menu generation changes.
Range checks, registry locking, source-publication hook cost and changed capture
work are outside the tiny queue benchmark, so do not extrapolate a full FPS claim.

New diagnostic output is one aggregate ownership line per three seconds while
active, plus startup/refusal messages. Counters are fixed atomic increments. No
stack captures are armed, no GPU readbacks are added, and flicker CPU recorder,
pixel diagnostics and legacy code compile OFF. This bounds instrumentation work
but does not settle the earlier overall performance report; same-save/view GPU
and native-versus-pereye attribution remains open after functional acceptance.

## Returned bounded owner capture cost (2026-09-25)

Verified ea83dc5be run 48242328 onward. Exactly three renderer snapshots reported
16.6, 22.8 and 17.4 us (56.8 us total) for stack capture, formatting and the first
log write. The following cost line and the once-only native identity block are
outside those timings; this is not an exhaustive logger benchmark. There were
no recurring ownership snapshots after these three and no GPU readback. This
capture cannot explain sustained multi-millisecond frame loss in this run.

Before pause, 48339859 reports 132.7 stereo ticks/s, 7.5 ms/tick and 6.4 ms GPU
span/tick at 144 Hz. This is a different scene from the prior grenade run, not a
controlled performance improvement or release comparison. Opening pause includes
147 ms frame gap, predominantly game-thread wait (142.7 ms); the later 52 ms gap
is predominantly xrEndFrame (45.4 ms). Do not conflate them with the finite probe.

The pause readiness repair adds one bounded atomic heartbeat per Present; no
per-draw search, extra GPU work or recurring log. Disable OwnerTrace in its expected
INI because the native/queue boundary is captured. Broader HUD/performance work
remains open and still requires controlled attribution before changing culling.

## Local HUD regression run: cost attribution and release diff (2026-09-25)

Verified 1ed638c01 DLL SHA256 b6fda98f04b9d8433ff0b6fde35ec821f7acdb94d870d048b9c918dd99dbb569,
log 32232546..32949437 (716.891 s). 2750x2850, 144 Hz, 6.94 ms budget. CPU flicker recorder,
GPU pixel probes and legacy code OFF; resolved Perf.FrameId=0. This excludes the
new recurring flicker recorder as this run's cause. It does not exclude all logging.
51,324 lines versus prior c4f5fe5df 60,335; different gameplay, not an A/B benchmark.

At 32935281 near the grenade throw, 73.7 stereo ticks/s, 13.6 ms/tick, zero untagged;
P1 OUT 7.1 ms (idle 1.3, rendering 5.8), P2 OUT 5.2 ms (idle 0, rendering 5.2), GPU span 10.3 ms
per tick, capture 0.4 ms, GPU idle 1.3ms. GPU span alone exceeds the 144 Hz budget here.
At 32701187, 57.7 ticks/s with P1/P2 render work 7.7/8.0ms and little render-thread idle.
The separate startup 26.3 ticks/s window has 17.3 ms game-thread waiting and 128 untagged;
do not label every slowdown as the same bottleneck or include loads in an FPS claim.

HUD vertex probing at 32935281: 38,349 probes/3 s, 33,631 us total, 0.9 us/probe, 64 refused.
Approximate 11.2 ms of probe CPU per second is below the multi-ms/tick deficit; it is
not the whole HUD cost (routing locks, render-target switches, copies and GPU work
are outside that interval). Native task/awareness broad matching can cause routing
churn independently of its cost. The small new interaction-cache lookup has no
allocation/logging. Wrist capture retains the existing held-item vote and steady
matrix multiply; no new engine-object scan in that draw path. Scoped-axis prior
host 0.082 us/sample is not a headset end-to-end measurement.

Important baseline difference: installed Stereo.Occlusion=pereye now activates the
post-release two-view-state implementation; 1.0.0 and the prior c4f5fe5df remote-base
DLL lack that consumer. The unchanged key was NOT equivalent runtime behavior.
Separate visibility can legitimately add draws. This remains a source suspect,
not proof of the performance regression. Both runs use 2750x2850/144 Hz; no matching
v1.0.0 playtest log was found in the available local playtest archive. Compare native
and pereye at the SAME save/view with paired timing windows before changing that
accepted visibility fix. Preserve resolution, quality, HUD and the hand/swing fixes.

The proposed ownership replacement should remove per-draw broad searches and
cross-thread position mutexes by carrying immutable owner records with render work.
Do not add per-draw GFx queries, object-table scans or GPU readbacks. First prove
the queue/display boundary with the finite OwnerTrace capture described in
HUD_ANCHORS. Actual production-source host tests exercise disabled logging/capture,
per-Present limit, 16-stack lifetime budget, once-per-family native reads and four
bounded failure attempts. Exhausted render gate measured about 3-4 ns/call on host;
not an in-game performance guarantee. Each actual capture reports elapsedUs;
no claim that its isolated stack walk/log burst is free. Normal default remains 0.

## VR-229 local diagnostic cost and normal follow-up build (2026-09-25)

Verified local build v1.0.1-8-gc4f5fe5df, recorder ON, GPU pixel collection OFF,
17263437..18005296 (741.859seconds). 60335 log lines total; 11142 flicker lines,
4476863bytes,99 windows. Approximate diagnostic output6KB/s is modest, but the
largest measured recorder finish/log burst is1.606ms. This includes the history
copy/format/log work and can include buffered file flush or scheduling; it excludes
other per-draw collection and is not an end-to-end off/on benchmark. It therefore
does not establish negligible frame-time impact.

Cause of burstiness: opening a window prints12 historical frames plus the current
frame in one Present, four lines each. Windows are bounded to one per5seconds;
healthy heartbeat10seconds. The logger is buffered with a200ms flush cadence,
not an unconditional per-line flush. Prior host recorder mean1.660us/present and
max0.867ms were a different machine/workload and do not override the measured
local1.606ms peak. No attribution of every perceived hitch to logging is possible.

The local follow-up is an optimized normal build: DVR_FLICKER_DIAGNOSTICS=OFF,
DVR_FLICKER_PIXEL_DIAGNOSTICS=OFF, DVR_WITH_LEGACY=OFF. Thus recorder history,
camera-upload census, formatting and the extra XR snapshot work compile out.
Its expected INI explicitly sets Perf.FrameId=0 because a normal build would
otherwise re-enable GPU probes from the retained FrameId=1 (the remote diagnostic
DLL had forcibly suppressed them). RingLedger's existing bounded reports remain;
new late-expire/progress reports run at most once per3seconds. Hand deferral logs
are capped at two per hand; capture/rebuild logs replace existing lines. HUD
continuity adds no logging and one bounded cache lookup for task-text candidates.
The scoped-axis publisher/read host benchmark on this checkout is0.082us/sample;
no claim of measured headset FPS improvement. Remote9da0a0b48 ZIP remains unchanged.

Next: judge the normal candidate visually, verify the matching log banner and
FrameId disabled. Re-enable the full recorder only for an identified need; if
further recording is necessary, spread historical output over presents before
claiming a negligible tail cost. Keep all performance follow-ups in this file.

## VR-79: turning occlusion queries off costs draws (2026-09-24)

Headset, same day: `off` fixed the one-eye culling and read laggier than native
(perceptual; no pairs/s were taken). The shipped candidate is `pereye`, which keeps
culling per eye and should cost about what native does plus the second eye's own
queries. The measurement below still applies, now as native vs pereye vs off.

`[Stereo] Occlusion=off` (live `occlusion off`) sets UE3's
GIgnoreAllOcclusionQueries so one eye's query results cannot cull the other
eye's draw (ENGINE_NOTES "VR-79"). Every primitive inside the frustum and its
cull distance is then drawn in both passes. The cost has NOT been measured.
Expected to be largest in dense interiors and the city (many hidden rooms behind
walls) and smallest outdoors in open areas. Earlier query-wait measurements
(0.102 ms/pair) bound the wait for results, not the draws culling saves, so they
cannot predict this.

Measurement to run, one lever, same spot, same view: pairs/s and the frame line
with `occlusion native`, then `occlusion pereye`, then `occlusion off`, then
`occlusion native` again, in the Hound Pits hub interior and on a street. `querywait on` in the
same run confirms the switch took (occlusion-path reads drop to about zero).
`pereye` is the per-eye culling that follows from that.

## VR-229 early versus stable cinematic cadence (2026-09-25)

On returned9da0a0b48, early InDialog587411343..587448000 has12 printed performance
windows: median reported stereo tick rate58.0/s (range27.7..66.3), median tick15.95ms,
median GPU per-tick span8.9ms (range6.4..13.9),137 untagged presents summed over those
windows. Later587448000..587486000 has13 windows:70.7/s (69.3..71.7),13.9ms,
GPU7.7ms (6.0..11.0),61 untagged. Headset72Hz,13.89ms budget.
These are medians of printed windows, not percentiles of every frame, and intervals
have different duration/content. No claim that GPU work is free or that the output
change recovers any measured amount. Lower GPU medians and continued diagnostic
recording during the stable period do not support GPU saturation or logging as a
complete explanation. The simultaneous plateau in stale/expiry/duplicate counters
and improved cadence supports addressing stereo interruptions/queue phase first.
The test must still distinguish residual ordinary frame-time judder from eye faults.

## VR-229 returned scoped-eye recorder and bounded output (2026-09-25)

Returned9da0a0b48: GPU frame-id probes verified disabled. Largest printed CPU recorder
finish/log peak0.512ms. The stable later cinematic still records windows, so logging
alone does not explain the early-only judder. No controlled end-to-end A/B exists.

Local actual-recorder host run before output change:100000 presents,50 c5 uploads
per present, real formatting/buffered file output; mean1.987us/present,max1.639ms.
After change:mean2.125us,max1.556ms. These are separate host runs subject to scheduling
and file flush noise, not proof of a meaningful peak-time improvement or regression.
Do not claim negligible tail cost from either mean. The structural improvement is
verified: opening a history window no longer prints12 prior frames plus the current
frame in one call (52 data lines). It prints one frame per Present, four data lines,
plus at most a window header. Same12-before/16-after evidence retained, maximum12
frames of output lag in a64-frame history. Window reopening waits for pending output;
very slow frame rates cannot overwrite the requested history. Abrupt exit may leave
the last pending records unwritten.255 production-recorder checks pass.

New render-progress fix adds only a game-thread boolean/counter and one value in the
existing3s beat. Its purpose is to prevent an unnecessary center-eye draw during one
queued render interval; it can add one extra double draw before a genuine stall is
refused. That is rendering behavior, not diagnostic overhead. Camera/state/session
guards remain. Remote candidate keeps CPU history so an unsuccessful run is still
useful, with GPU probes suppressed regardless of saved FrameId. The maintainer's
normal1ed638c01 build remains installed with the entire recorder compiled out.

## VR-229 scoped-axis follow-up (2026-09-25)

Returned candidate c4f5fe5df verifies GPU frame-id pixels OFF. Recurring recorder
max observed0.586ms (previous diagnostic0.523ms); this is a rare window maximum,
not an every-frame charge or a complete remote performance A/B. No new logging or
GPU probes added for the scoped-axis correction. One script writer publishes three
atomic floats and sequence; render reader makes at most two snapshot attempts,
never waits/spins without bound. Local x86 host100000 publish+read iterations average
0.091us/sample, checksum250000, concurrent no-torn-read stress passes. Host harness
is not optimized game timing and does not prove total render cost on the tester's PC.
The new candidate retains the earlier lightweight recorder; no claim of a measured
headset performance improvement. Source/geometry evidence: FLICKER_REFERENCE top.

## VR-229: diagnostic overhead and false draw stalls (2026-09-25)

Current returned build v1.0.1-6-g31450526c,3025x3135,shared wait0. The recorder's
largest measured finish/logging burst is0.523ms.3024 printed backbuffer sample
issue calls average0.003452ms,p95 0.005,max0.152; this excludes later maps,
D3D11 sampling and GPU synchronization. No matched off/on run exists. Therefore
the old full pixel diagnostic is NOT established to have negligible total cost.

New acceptance candidate keeps CPU history and mono/eye outcomes but forces
frame-id collection OFF, even with Perf.FrameId=1 in the unchanged saved INI.
All four GPU stages exit at the collection gate. Optional pixel investigation
now requires -FlickerDiagnostics -FlickerPixels; both flags default OFF and build.ps1
explicitly clears stale cached flags. Normal builds retain their saved FrameId
policy. Pixel opt-in without the recorder is rejected. No installed INI edits.

Actual recorder host benchmark:100000 synthetic presents,50 c5 uploads/present,
production history/event/formatter code, buffered file logging, recurring windows.
Mean1.660us/present; max finish0.867ms (rare historical-window printing). At144
presents/s the measured mean is about0.024% of one core. Includes recording and
format/file sink work, excludes game-side pose assembly and actual remote disk
behavior. Returned recorder max and host cost support low CPU overhead; they do
not prove an end-to-end FPS difference. Zero added pixel GPU work is enforced by
the candidate collection policy, which is tested even with INI request=true.

The source gate saved Present at draw return and ignored progress inside that
draw. Candidate measures entry-to-entry instead; an old-policy negative control
produces199 false stalls in200 ticks while the new policy produces0. Genuine
stalls still refuse. Additional doubled draws may change total rendering cost;
that is the intended removal of false mono interrupts, not logging overhead.
Evidence, counterprediction and limitations: FLICKER_REFERENCE.md, VR-229 top entry.

## VR-260 affected-player acceptance (2026-09-25)

The affected player reports the fix-only60bbd0afc candidate resolves the severe
performance issue. This is reported acceptance; no returned post-fix timings
were supplied. The pre-fix attribution and native validation below remain the
measured record. No merge or release is authorized by this result.

## VR-260: shared capture rejection and slow CPU fallback (2026-09-25)

Measured in support-20260925-235231-155-28068: log banner 1.0.1,
v1.0.0-8-gf5176aeae, RelWithDebInfo, legacy off; the install record agrees.
The current run uses VirtualDesktopXR 1.0.10, D3D9Ex and 2750x2850.
Do not combine its measurements with the older SteamVR shim log in the bundle.
The configured capture mode is shared, but the startup probe creates a standalone
render target with success and a null sharing handle. The following 0x80070006
is synthesized E_HANDLE; OpenSharedResource was never called. The probe rejects
sharing, leaving mode=sync throughout the measured windows.

Fourteen capture windows cover 254 grabs: weighted mean capture 149.927 ms,
including 145.555 ms in LockRect; window mean capture ranges 131.806-167.005 ms.
That is about 6.7 captures/s before other work, consistent with the reported
single-digit frame rate. This happens already on the mono startup/menu path.
D3D9 and the XR-selected D3D11 device have matching adapter LUIDs on the RTX5080.
The desktop mirror is off; native-present and xrEndFrame timings are small.
Focus is lost later, but the stall exists while FOCUSED. Neither a wrong adapter,
legacy input work nor ordinary stereo draw cost explains this capture stall.
The log's 3072 MB VRAM field is not evidence of actual RTX5080 capacity.

Source-confirmed weaknesses: probe and slots use CreateRenderTarget, whereas
Microsoft's D3D9/D3D11 interop contract specifies CreateTexture with pSharedHandle.
The probe also uses X8 when the real slots would first try A8, so a rejected probe
can prevent a supported format from being tried. Driver rejection of the old
resource/format is the leading explanation, not a remotely confirmed root cause.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource

Candidate uses one-level DEFAULT render-target textures and their level-zero
surfaces, with the same A8-first format rule in probe and slots. It retains the
D3D9 texture owner through capture and releases all views/surfaces/owners on reset
or partial failure. Logs name the exact failing step and do not misattribute a
missing handle to a D3D11 call. Existing fencing/delivery and fallback policy stay
as before; VR-114 fence timeout handling is separate.

Native x86 hardware test (no game): 162 independent D3D11 pixel checks after
alternating D3D9 colors and X8-to-A8 StretchRect, at 64x64 and 2750x2850; three
release/ResetEx/recreate cycles pass. Simulated success-with-null-handle rejects
before OpenSharedResource, and failed-open/unsupported-format cleanup passes.
Optimized x86 build (legacy OFF), repository lint and nine proxy exports pass.
This proves the candidate bridge works locally, not that the remote driver now
accepts it. Command: tools/shared-capture-native-host.ps1.

Next test, one question: with this candidate and the same saved settings, is the
startup/menu still limited to single-digit FPS? Collect support after about 30
seconds. Acceptance requires the candidate banner, shared texture AVAILABLE,
live shared slots and mode=shared with the huge readback cost gone. If sharing
still fails, the named API/format/HRESULT directs the next fix. If sharing works
but FPS stays low, attribute the remaining time from that new run. No game launch
or install was performed on the maintainer's machine.

## Post-merge intro and hub slowdown (2026-09-23, attribution open)

Reported: intro and hub rates fall into the 50s after integrating PRs105-110;
previous intro performance was reported at least around90. This report is from
the RTX4070 Ti SUPER tester, not the RTX4060 rig in VR-160.

Identity: installed DLL SHA256
717432F6AFC8EBAA200B7C936297069F7881FF533457BC6D81BFB95E77A9F993
matches the log banner756-g1726cee95, RelWithDebInfo, legacy off. Current and
previous736-gab7023884 logs both resolve2750x2850, VirtualDesktopXR, Quest3,
144Hz and desktop mirror off. Launcher/Layout candidate759 was not installed.
Archive: build/integrate-105-110/build/perf-last-run-20260923-024218, containing
both logs and the full installed CRLF INI before any subsequent run.

Measured on756, seconds from its first banner:

| Interval | Observation | Limit on interpretation |
| --- | --- | --- |
|37-100s, intro|Mostly53-76 ticks/s, 12.9-18.2ms/tick; drop context unknown throughout|Not a controlled comparison against an earlier intro run|
|88.3s|52.7 ticks/s,18.2ms/tick; P1 OUT11.9ms includes7.9ms render-thread idle; P2 OUT4.1ms includes1.2ms idle|Game-thread feeding is a material bottleneck; this does not identify the costly function|
|120.1s|Drop context first discovered|A level transition also occurred, so subsequent improvement is confounded|
|127-130s|115-120 ticks/s with context known|The merged build can still run quickly|
|134.6-137.6s stereo beats|About45-50 complete pairs/s,52-53 none/s; repeated no-present-since-previous-draw refusals|This is delivery cadence, not raw Present FPS; the interval precedes a loading transition|
|170-191s|About120-125 ticks/s with context known|Later slower windows also have a known context, excluding missing-context discovery as a complete explanation|

No VRAM-budget exhaustion is evident. Capture and xrEndFrame means are small
in the sustained slow intro block. Live-object hash rebuilds average about1ms
at roughly1Hz; the merged hash implementation does not show the old12ms sort
cost. MoveTrace, CamModProbe and Cine Trace resolve off.

Concrete source suspect: PR106 moves drop-context discovery from a20ms-gated
sample into the approximately10ms anim sample without a separate discovery
throttle. An unsuccessful call scans up to1024 GObjects slots, including
IsLiveObject and ObjClassName, whose readability checks call VirtualQuery.
The slow intro keeps known=0, so this repeats throughout. The old build also
scanned1024 slots while DropWatch=1, but at the lower cadence. This is increased
work established from the diff, NOT proof of the full FPS loss. In particular,
the later known-context slowdown needs its own attribution.

Next: measure or exclude discovery cost independently of cached decision
sampling, which must stay responsive for drop attacks. Compare the same save
and view; scene-to-scene rate changes are not a valid A/B. Preserve render size,
refresh and accepted gameplay behavior. Do not widen stereo hold windows or
relax scene gates to conceal the later delivery deficit. The latter signature
has prior context in VR-77 and FLICKER_REFERENCE's cadence routing, but no new
stereo correctness claim or change is made here.

The initial investigation was read-only. Tracking issue: VR-212. The subsequent
candidate and installation are recorded below.

## Menu cadence and camera-upload gate coverage (VR-178, 2026-09-22)

Verified combined650-g76ae6804a, optimized and legacy off. Preserved run:
build/playtest-candidates/menu-choppiness/run650. Source/fix scope and the
one-question headset test are in FLICKER_REFERENCE's VR-178 entry.

The journal's engine rate is high while stereo production is low. Between the
beats at50031250 and50034250, camera-silent skips rise3160->3472 (+312),
present-stall1464->1470 (+6), scene-state stays10573. The latter beat reports
129 draws/s,23 second draws/s,152 presents/s. Delivery at50033437 reports
L15/R15,mono61,none54 per second. These adjacent windows have different boundaries;
do not treat their differences as dropped-frame counts. Pause at50040250 reports
123 draws/s,119 second draws/s,242 presents/s; silent stays3543 from the prior
beat while stall rises1482->1492. Average FPS conceals intermittent stereo output.

The observed-upload exception only covers pause3; journal5/wheel6 throw away its
history. New separately opt-in MenuSceneFreshness retains actual during-draw c5
movement for less than100ms within the same menu epoch/load. Existing scheduling
guards and compositor hold remain. No desktop-output policy or pose-lock change.
72 policy cases and404 pairing checks pass. Headset cadence/left-hand verdict open.
This does not prove the older ReduceDesktopPresent hypothesis in VR-144, nor
attribute left-hand-only motion to the renderer before stereo continuity is tested.

## VR-180: a quarter-second freeze on every trigger pull was a legacy build (2026-09-22)

**Report:** pulling either trigger froze and stuttered the game, every time, fading after
some play. **Not a performance regression in any feature and not the machine:** the build had
`src/legacy` compiled in, and its projectile-spawn tracer walks every engine object for about
four frames after each trigger edge. `docs/TRAPS.md` has the whole account and the fix.

| Build, same source, eight trigger pulls on the simulator | `perf: frame gap` sat in `game_tick` | `spawn: NEW obj` lines |
|---|---|---|
| legacy ON (`614-gc7317261`, RelWithDebInfo) | 11, at 75 to 133 ms | 256 |
| legacy OFF (`614-gcce004d3`, RelWithDebInfo) | 0 | 0 |
| legacy OFF with the VR-180 guard (`600-gd556eb58` + the guard) | 0 | 0 |

In the affected headset log (VirtualDesktopXR) the same stalls read 76 to 88 ms per present in
runs of three. The guard build on the simulator at 2064x2208: 89.7 ticks/s against a 90 Hz
display, tick 11.1 ms. The guard adds nothing to the frame path: one word in the log banner
and one field in `status.json`.

**How to read this next time:** `sat in: game_tick` is the mod's own per-present work. A stall
there that lines up with an input is the mod doing something on that input.

## VR-160: the dev PC's 43-57 pairs/s - what the record already answers (2026-09-20)

**Report:** about 40 fps in the headset on the dev PC, while the sibling BioShock
mod is comfortable on the same PC. Decisions taken for this investigation: the render
size stays 3012x3122 (no resolution sweep), the headset test rate is 120 Hz, and
headset runs are few, so everything a log or the simulator can answer is answered
there first. No new run is claimed in this entry; every number below was already on disk.

### The populations (Q1). Do not combine them.

| Name | What it counts | Where it is read |
|---|---|---|
| ticks/s | game ticks per second. Under `reentry` one tick draws two scenes | `perf: tick N ms (X/s, ...)` |
| presents/s | calls to Present. Under `reentry` this is 2 x ticks/s | the same line, `Y presents/s`; `stereo: beat out/s` |
| fresh pairs/s | complete stereo pairs with both eyes renewed. Equals ticks/s ONLY while every tick draws both eyes | `stereo: beat L/s` and `R/s` |
| the streamer overlay's fps | application submissions as Virtual Desktop counts them | the overlay. WHICH of the above it matches is unverified; headset run 1 reads both in one marked window |

`L/s` is what the player perceives as smoothness. VR-152 showed it can fall 59 % while
the tick mean moves 13 %, because a tick that draws one eye is still a tick. In both
dev-PC logs below the armed invariant holds (`L/s == R/s == out/s / 2`), so here
ticks/s, pairs/s and `L/s` are the same number: 43-55. "40 fps" is that number.

### Two rigs are mixed in this file (the correction that matters most)

| | Rig A (the tester's) | Rig B (the dev PC) |
|---|---|---|
| GPU | RTX 4070 Ti SUPER (STATUS "The rig") | RTX 4060 (`adapter[0]: NVIDIA GeForce RTX 4060` in both logs), Ryzen 5 5600X |
| Runs in this file | builds 239-527: every 9.4-13 ms tick, the 0.64 ms/MP + 5.6 ms fit, "quarter pixels bought about 6 %", the 100-110/s judder report | `vr37-pre-existing.log`, `vr37-HEADSET-run1.log` |
| 3012x3122 judged in the headset | yes, 2026-09-15 | never |

Every headline number above this entry is rig A's. **No like-for-like regression on rig
B is established**: there is no earlier rig-B headset number at this size to regress
from. `arm-res.ps1` predicts 11.6 ms at 9.40 MP and rig B measures 18.3, so that fit is
not this card's. A measurement carries the identity of what it measured; this file
did not carry the machine, and from this entry on it does.

Rig B, active play only (tick < 60 ms; exact over every `perf: tick` line), VDXR,
90 Hz, `stereo reentry`, `res: HONOURED - the game renders 3012x3122`:

| Log | Build | Config | tick median / p90 | cap=lock P1 / P2 | out P1 / P2 | annotations |
|---|---|---|---|---|---|---|
| `vr37-pre-existing.log` 09-18 | `vr33-hands-working-434-g94202ce8` | UNKNOWN (the banner could not say) | 18.3 / 20.6 ms, 337 windows, 0 idle | 2.10 / 2.60 ms | 3.60 / 5.40 ms | 20 RENDER THREAD STARVED, 9 PACE-BOUND |
| `vr37-HEADSET-run1.log` 09-20 | `vr33-hands-working-540-g3051531e-dirty` | Debug (STATUS) | 23.0 / 24.2 ms, 214 active of 541 | 1.55 / 1.10 ms | 5.00 / 5.10 ms | 379 PACE-BOUND (mostly the idle block), 14 STARVED |

The 09-20 log's other 327 windows are the headset sitting idle: the runtime drops to
72 Hz (`hmd 13.89 ms`), ticks read 74-612 ms, and 10,487 of its 10,694 frame gaps sit in
`present-tail (xrEndFrame)`. The 72 Hz block is a cleaner idle filter than a tick
threshold. The 09-18 log never leaves 90 Hz and has no `present-tail` gap at all.

### This card's GPU cost was already in the log

`vr37-pre-existing.log`, active play: `perf: gpu/present render-to-entry=6.1-7.3 ms
capture=0.2-0.3 idle(d3d9)=0.0-0.1 | per tick span=12.1-14.6 dma=0.5 idle=0.1-0.2`,
341-355 of 341-355 resolved, 0 late, at about 57 ticks/s (17.5 ms tick).

Two scene renders cost about 12.3 ms of D3D9 GPU span per pair on rig B at 9.40 MP per
eye. That span alone is over 11.11 ms (90 Hz) and 8.33 ms (120 Hz); with everything
else free it caps rig B near 80 pairs/s at this size. Per this file's own evidence
rules a D3D9 span can contain feeding gaps, so 12.3 ms is an UPPER bound on GPU busy
time. Whether the card or the render thread owns it is the question headset run 1
decides, and it also decides where the other ~5 ms per tick can be won.

### Q0: the build config, static half

- The project sets no optimisation flags, so each config is the generator's default.
  Debug: `/Od /Ob0 /RTC1 /MTd`, `_DEBUG`, `_ITERATOR_DEBUG_LEVEL=2`. "Release" in every
  script means RelWithDebInfo: `/O2 /Ob1 /Zi /MT`. No `/GL` or LTCG anywhere. A true
  `/Ob2` Release is never built (`CMakePresets.json` has `debug` and `release` ->
  RelWithDebInfo only).
- `build.ps1`, `install.ps1` and the `xrsim-*` scripts default to Debug;
  `install-candidate.ps1` and `package.ps1` are always RelWithDebInfo. So tester builds
  were optimised and dev-PC installs were not, unless `-Release` was typed.
- The banner printed version, build id, `__DATE__ __TIME__` and no config. Two logs of
  the two configs differed only in the DLL's size (7.2 MB against 4.8 MB). There are no
  `_DEBUG` or `assert` blocks in `src/`, so Debug's cost is codegen, runtime checks and
  the debug CRT, all on the CPU side.
- **Fixed with this entry (the banner commit and the install-script commit):** the banner, the crash header and `status.json` name the
  config; an unoptimised build logs one `Warn` and tags its own `perf: tick` line
  (optimised lines are byte-identical to before, for the scripts that parse them);
  `install.ps1` prints the DLL's hash and a three-line warning for a Debug install. The
  default stays Debug by decision: the simulator workflow relies on it.

**Prediction, recorded before the simulator A/B runs** (Debug / RelWithDebInfo / Debug,
one save, one spot): Debug inflates `pre`, `tick`, `method` minus `lock`, `end`, and the
share of `out` that is our per-draw hooks. It does not move `cap lock` or the GPU span.
If Debug costs under 1 ms per tick on the simulator it cannot explain 23.0 against
18.3 ms, and those two runs differ for another reason (scene, or build 434 was Debug
as well). The unwelcome answer this can print: Debug is most of the gap, and half of
"40 fps" on 09-20 was the install script.

### Q5: nothing game-side caps the rate

Read from `DishonoredEngine.ini` `[SystemSettings]` and `[Engine.Engine]` on rig B:
`bSmoothFrameRate=FALSE` (so `MaxSmoothedFrameRate=130` is inert), `UseVsync=False`,
`MaxMultisamples=1`, `MaxAnisotropy=4`, `ResX/ResY=3012/3122`, `ScreenPercentage=100`,
`OneFrameThreadLag=True`, `DynamicShadows=True`, `MaxShadowResolution=800`. Neither log
mentions smoothing or vsync. The only pacer in the system is `xrEndFrame`.

### Q6: why BioShock Infinite is comfortable on the same card (architecture only)

From the sibling repo's own docs; no constant is carried over. Infinite uses the SAME
stereo bet: the scene-draw root called twice per tick, two presents per pair. On this
RTX 4060 it logs 77-80 pairs/s, at the runtime's native 2064x2208 per eye (4.56 MP),
and its record says it was already GPU over budget there before its settings pass.

> Infinite is comfortable because it draws less than half the pixels per pair on the
> same card (9.1 MP against Dishonored's 18.8 MP) and hands each eye to the runtime
> with one same-device D3D11 `CopyResource` and no fence. Dishonored is a D3D9 game, so
> each eye crosses to D3D11 through a fenced shared surface (`cap lock` 1.1-2.6 ms per
> present on rig B), and the 3012x3122 default was judged on a card with about twice
> the throughput.

Differences worth a written proposal, not code: Infinite holds ONE XR frame open across
both presents of a pair (one wait, one locate, one prediction, one `xrEndFrame`); its
per-draw detours cost one relaxed atomic load when idle (about 380 draws per present).
Dishonored already runs `xrWaitFrame` on a pace thread, as Infinite does.

### Suspects of ours, found by reading (none measured on rig B yet)

This table is the state BEFORE the simulator legs and is kept as written. Suspects 1 and
2 are decided in "The ranked table after the simulator legs" below. Suspect 3 is VR-162;
suspects 8 and 11 are VR-163.

| # | Suspect | Evidence | Status |
|---|---|---|---|
| 1 | The card at 18.8 MP per pair | GPU span 12.3 ms per pair, `idle(d3d9)` 0.1 ms; rig A is about 2x the card and ran about 2x the rate | open: headset run 1 (GPU engine utilisation beside the log) |
| 2 | Debug build | 09-20 was Debug; all CPU-side | open: simulator A/B, prediction above |
| 3 | `hkSetVSConstF` has no early-out and carries its view-model-hide block, its `DcNotePalette` call and its palette cache TWICE (`vs_const_hook.cpp` 219-256 and 307-361) | verified in source. It is the highest-frequency hook in the mod. `g_hmStaticWindow` is decremented twice per static upload, and two palette caches with different length rules write one buffer | open, and a correctness question before it is a cost question: ticket, measure with NativeProfile's `ConstHook` scope, do not delete blind (hands are headset-judged) |
| 4 | Instruments the shipped ini turns ON against compiled defaults of OFF: `[Hud] Regions=1`, `[Perf] NativeProfile=1`, `[Perf] BridgeGpu=1`; also `[Hands] DrawCensus=1`, `[Cine] Trace=1` | `probe_draw` locks a vertex buffer and hashes up to 8 KB per HUD-class draw; NativeProfile wraps about 20 D3D9 entry points; 15,771 `native-profile:`, 49,304 `cine/trace` and 17,402 `dc:` lines in the 09-20 run. DiagnosticAb's mask (about 1 % on rig A) never covered these | open: one grouped off/on/off rung |
| 5 | Ticks that draw one eye | 8,964 `reentry: gates -> DOUBLE draw after N single tick(s)` lines on 09-20, 6,944 on 09-18. VR-77 is this signature | open: count per minute in the standing-still windows of run 1 |
| 6 | `anim::weight()` takes an exclusive SRW lock, and `GetTickCount64`, on every draw, twice on a qualifying one (`draw_census.cpp:241`, `mesh_split.cpp:2991`) | read, not measured | open |
| 7 | The script lane: `PeHandler` has one early return; about 40 `strstr`/`strcmp` and a `RealName` per dispatch; `UiSurfaceTick` walks 1024 GObjects slots with `IsLiveObject` every 16 ms; an unresolved `CineTraceTick` re-runs about 11 full GObjects walks every 5 s | read, not measured. Lands in `out`, which is NOT "not the mod" (the VR-143 trap). Two log lines on this lane are now bounded (VR-160): `armfollow` change lines stop at 8 per field with the counters kept for `armfollow status`, and the `headtrack:` line is on a 3 s time gate instead of every 150th dispatch. Hygiene, not a rate claim | open |
| 8 | The HUD redirect's per-sink `StretchRect` and clears are charged to `end`, which the gap line names `present-tail (xrEndFrame)` | `frame_hooks.cpp` 242-256: `hudcap::end_frame` sits between the two stamps | an attribution fault in our own instrument; ticket |
| 9 | The frame-gap report | one ungated Info line per gap, plus a 16-record ring format and two more ungated Info lines, all formatted whether or not they print; about 10,500 of each on 09-20 | fires when the rate has ALREADY collapsed, so it is hygiene, not the 40 fps. FIXED (VR-160): the first 3 gaps of any 5 s window are itemised, the rest are counted and summarised once with their worst, their count >= 60 ms and their `present-tail` share; the ring, streaming and memory formats run only for an itemised gap and only when the line will print. Predicted effect on a run like 09-20: about 32,000 lines become about 1,500. Not a frame-rate claim |
| 10 | The log write path | NOT flushed per line: buffered stdio, `fflush` every 200 ms, format before the lock (`log.cpp` 29, 84) | eliminated as a per-line flush |
| 11 | `DVR_SKIP` | `dvr::diag::skip()` has one caller, the echo command. The env knob disables nothing, so the ladder rung that relies on it cannot run as written | fault; ticket. `DISHONORED_VR_XR_SAFE=1` and `[Mode] GamepadOnly=1` are the levers that exist |
| 12 | A game-side cap | smoothing and vsync off (above) | eliminated |

Not repeated, and why: nonblocking Present, the max-frame-latency sweep, the query-wait
helper, dynamic shadows via ini, FrameId-off and pair pacing as a default all failed on
rig A for reasons that do not depend on the card. The resolution route is the one whose
negative was rig A's alone; it stays closed here by decision, not by evidence.

### Headset run 1, the plan and its prediction (recorded before the run)

One launch, RelWithDebInfo, 120 Hz, 3012x3122, one save, one spot, standing still, with
GPU utilisation and the per-process GPU engine counters sampled beside the log. Marked
windows: `stereo reentry` 60 s, `stereo mono` 45 s, `stereo reentry` 60 s, the suspect-4
instruments off as one group 45 s, `hud off` 45 s, `stereo reentry` 60 s to close.

Prediction: the game's 3D engine reads above 90 % under `reentry`; `mono` runs at 1.8-2x
`reentry`'s rate; the instrument and HUD rungs sit inside the spread of the three
baselines. That outcome means the card is the limit at this size and the rest is a
written proposal. The unwelcome outcome the run can print instead: the 3D engine well
under 90 % with the span still near the tick, which means the render thread is starving
the card and suspects 3, 4, 6 and 7 are where the rate is.

Both halves of this were then answered on the SIMULATOR, which renders the same
3012x3122 (results below), so the headset run shrinks to one question the simulator
cannot answer: what the streamer's encode takes from the same card at 120 Hz.

### RESULTS, simulator lane, rig B, 2026-09-20 (Q0 measured half, Q2 rungs 3-4, the card's ceiling)

All legs: rig B (RTX 4060), the simulator (`dvr-xrsim`, 90 Hz, free pace), the newest save
(an interior with the hands drawn; a picture was looked at before any number), simulated
head fixed at `head rot 0 0 0`, `res: HONOURED - the game renders 3012x3122` (the SAME
render size as the headset; only the compositor's 1032x1104 capture is smaller),
`stereo reentry`, `[Capture] Mode=shared` (it is the default now; the 9Ex shared capture
is NOT off), 30 s settle then 38 windows of 3 s. Logs: `D:\dvr-data\logs\vr160-sim-*.log`.
Not headset performance: no encoder, no streamer, no compositor pacing.

**Debug / RelWithDebInfo / Debug** (medians over 38 windows each):

| Leg | Build, config, DLL sha256 | tick median / p90 | pairs/s | `R` P1+P2 | `cap lock` P1+P2 | GPU span per tick |
|---|---|---|---|---|---|---|
| A1 | `550-ged3621c8` Debug `BE1DE2B5` | 20.1 / 23.5 ms | 49.7 | 5.7 + 5.0 | 0.2 + 0.9 | 15.4 ms |
| B | `548-g3487c5d0-dirty` RelWithDebInfo (same source as 550) | 18.9 / 21.1 ms | 52.5 | 3.6 + 3.2 | 1.5 + 1.8 | 13.9 ms |
| A2 | `550-ged3621c8` Debug `BE1DE2B5` | 20.4 / 23.4 ms | 49.0 | 5.8 + 5.0 | 0.1 + 0.7 | 16.2 ms |

- **Debug costs 1.2-1.5 ms per tick here, 6-7 %.** It is real and it is not "40 fps".
- **The prediction was half wrong, usefully.** Debug does inflate the render thread's own
  time: `R` falls 3.9 ms per tick in the optimised build, which is our per-draw hooks and
  the engine's submission running under them. But `cap lock` did not hold still: it ROSE
  2.2 ms per tick and ate most of the saving. `lock` in shared mode is the wait on the
  previous present's blit fence, a GPU wait. Make the CPU side faster and the present
  thread simply arrives at the fence earlier. That is what a card-limited frame looks like.

**`stereo reentry` against `stereo mono`, with the game's 3D engine utilisation** (Windows
`GPU Engine` counter for the game's process, 1 s samples stamped with the log's clock and
joined to the 3 s perf windows; RelWithDebInfo `550-ged3621c8`, DLL `6EE3CD19`):

| Method | scene renders/s | pairs/s | GPU 3D engine | GPU-busy per scene render |
|---|---|---|---|---|
| `reentry`, fast phase (idle 0.4 ms) | 108-112 | 54-56 | 86-87.5 % | 7.9-8.0 ms |
| `reentry`, slow phase (idle 7.5 ms) | 91-93 | 45-47 | 72.7-73.4 % | 7.9 ms |
| `mono` (PACE-BOUND at the simulator's 90 Hz) | 90 | n/a | 72.4-74.8 % | 8.1 ms |

- **The card's ceiling at this size is about 62 pairs/s.** Three independent rows give the
  same 7.9-8.1 ms of GPU-busy time per scene render at 3012x3122 on the RTX 4060, capture
  copies included. 1000 / 8.0 = 125 renders/s = 62 pairs/s at 100 % utilisation. This is an
  instrument that could have failed its own hypothesis: had `mono` and `reentry` disagreed,
  or had the slow phase shown a different cost per render, the counter would not have been
  measuring render work. They agree to 3 %.
- **What it means for the asked rates.** Two full scene renders per displayed frame need
  240 renders/s at 120 Hz (1.92 s of GPU per second), 180 at 90 Hz (1.44) and 144 at 72 Hz
  (1.15). None fits in one second of this card at 9.40 MP per eye. The size at which 90 Hz
  fits needs about 5.5 ms per render; how render cost scales with pixels ON THIS CARD is
  unmeasured (the 0.64 ms/MP fit is rig A's) and stays unmeasured by decision.
- **The recorded prediction for headset run 1 is REFUTED in its wording and confirmed in
  its substance**: the 3D engine never reads above 90 % (max 87.5 %), and the limit is
  still the card. 86 % busy at 55 pairs/s against a 62 ceiling means everything of ours
  that starves or stalls the card is worth at most 12 % in the fast phase. The earlier
  "about 80 pairs/s" ceiling from the 12.3 ms D3D9 span was too generous: the span stops
  at Present entry and leaves out the capture blit and the D3D11 side.
- **The headset will read LOWER than the simulator**, not higher: Virtual Desktop's encode
  and colour conversion share this card. 09-18 in the headset was 54 pairs/s median with
  no slow phase; the simulator's fast phase is 55.

**A 12-13 s cycle on the game thread, found by looking at the series instead of the
median.** Every leg alternates about 6 s of tick 17.5-18.5 ms (`idle` 0.4, render thread
never waits) with about 6 s of tick 21-24 ms (`idle` 6-8 ms, `RENDER THREAD STARVED`),
15-20 of 38 windows. Its size is the same in Debug (7.0-7.9 ms) and RelWithDebInfo
(6.8-8.1 ms). Our script-lane code is several times slower under `/Od`, the engine's is
the same binary in both, so **this cycle is not the mod's code**: it is the engine's or
the level's (a looping scripted sequence is the likely owner). No log line's rate differs
between the slow and fast windows (`tools\perf-tick-cycle.py`, 14 slow against 17 fast). It costs this
save about 9 % of its mean rate. The 09-18 headset log, on a different spot, shows 20
STARVED windows of 337, so it is content, not a constant. Not pursued further: the mod
cannot shorten the engine's game thread, and VR-143's trap is respected (the claim rests
on the Debug A/B, not on `out` being "not ours").

One hitch IS ours and is Debug-only: `perf: frame gap 41-44 ms ... sat in: game_tick`,
a present that spent 19-22 ms in `DvrGameTick` and 36-38 ms inside our hook, about once
a second in the slow phase. 22 of them in leg A2's gameplay, 13 itemised in A1's window, and
**none in leg B**: the RelWithDebInfo measurement window has zero frame gaps of any
owner. So it is a cost of the unoptimised build, it is part of why a Debug build feels
worse than its 6 % median says, and it needs no ticket beyond the banner's warning.

**The frame-gap change works as designed**: a window with 5 gaps printed 3 itemised and
`perf: frame gaps - 2 MORE in the last 5 s not itemised ... worst 41 ms sat in: game_tick,
0 sat in present-tail (xrEndFrame)`. The banner reads `config Debug` / `config
RelWithDebInfo`, the Debug `Warn` prints, and the Debug tick line carries its tag.

### The ranked table after the simulator legs

| # | Suspect | Measured cost | Status | Decided by |
|---|---|---|---|---|
| 1 | The card at 9.40 MP per eye, two scene renders per frame | 8.0 ms GPU-busy per render = a 62 pairs/s ceiling; we run at 86 % of it | CONFIRMED (simulator). The headset number with the encoder on the same card is owed | `mono` vs `reentry` vs the slow phase, three agreeing rows |
| 2 | The engine's 12 s game-thread cycle on this save | 6-8 ms per tick for half the time, about 9 % of the mean | CONFIRMED not ours; content-dependent | identical in Debug and RelWithDebInfo |
| 3 | Debug build | 1.2-1.5 ms per tick, 6-7 % | CONFIRMED, and fixed as a trap (banner, Warn, install line) | A1 / B / A2 |
| 4 | Everything of ours on the render thread and the present path (suspects 3, 4, 6, 8 above: the doubled constant hook, the shipped-on instruments, the per-draw SRW lock, the HUD pass) | bounded above by the 12 % between 55 and 62 pairs/s, all of them together | OPEN, bounded. Worth doing for the 12 %; cannot reach 72 Hz, let alone 90 or 120 | the GPU utilisation rows |
| 5 | `game_tick` 20 ms spikes at about 1 Hz in the slow phase | about 4 % of that phase, Debug only; zero gaps of any owner in the RelWithDebInfo window | CONFIRMED a Debug-build artifact | the itemised gap lines, A1/A2 against B |
| 6 | One-eye ticks (`gates -> DOUBLE draw after N single`) | not measured in these legs (`L/s == R/s`, `none/s=0` throughout) | OPEN for the headset logs only | - |
| 7 | The log | not flushed per line; 3 lines per gap now bounded | ELIMINATED as a rate cost; hygiene fixed | source + the summary line |
| 8 | A game-side cap | none | ELIMINATED | the ini |

**What this says about the architecture (proposal material, no code):** at this size on
this card the only routes to the headset's rate are fewer pixels per scene render, or
fewer scene renders per displayed frame. The first is a settings decision that belongs
to the player and was set aside for this investigation. The second is what this file
already lists as unbuilt: alternate-eye rendering (one fresh eye per displayed frame,
with its temporal mismatch) or a reprojected second eye. Neither is a fix to make inside
a performance pass. The honest sentence for rig B today: **3012x3122 per eye is a
4070-Ti-SUPER-class setting; on the RTX 4060 it renders 54-56 pairs/s at best and no
change to the mod's own code can lift that past about 62.**

### RESULT, headset run 1, rig B, 2026-09-20 (the streamer's share, and a retraction)

Rig B (RTX 4060), VDXR at 120 Hz (`hmd 8.33 ms = 120.0 Hz`), `res: HONOURED - the game
renders 3012x3122`, `stereo reentry`, build `vr33-hands-working-550-ged3621c8`, `config
RelWithDebInfo`, DLL sha256 `6EE3CD19`. The newest save, standing still, 46 windows of 3 s
after the last `state: GAMEPLAY`. Log `D:\dvr-data\logs\vr160-HEADSET-rel-120hz.log`, GPU
samples `vr160-headset-gpu.csv` and `vr160-headset-gpu-all.csv`. The streamer's overlay
read 45-50 fps in the same minutes.

| | tick median / p90 | pairs/s (`L/s`) | `R` P1+P2 | `cap lock` P1+P2 | `tick` field P1+P2 | GPU span per tick |
|---|---|---|---|---|---|---|
| Headset, 120 Hz | 22.1 / 22.3 ms | 44.7 (45) | 5.1 + 6.1 | 2.1 + 2.1 | 1.2 + 1.2 | 17.4 ms, `idle(d3d9)` 0.0 |
| Simulator leg B, for scale (a different view) | 18.9 / 21.1 ms | 52.5 | 3.6 + 3.2 | 1.5 + 1.8 | 0.8 + 1.0 | 13.9 ms |

GPU engines over the same 113 s: the game's 3D engine **76.1 %**, the streamer's video
encode engine 21.3 % (its own silicon), the streamer's 3D engine 6.8 %.

- **Q1 settled: the streamer's overlay counts stereo pairs.** It read 45-50 while the log
  read 44.7 pairs/s and 90 presents/s. "40 fps" is pairs per second.
- **The rate half of the prediction held** (at or under 54: it is 44.7). **The
  utilisation half is REFUTED**: the game's 3D engine was predicted at or above the
  simulator's 86 % and reads 76 %. 76.1 % over 89.4 scene renders/s is 8.5 ms of GPU per
  render (8.0 on the simulator; the streamer's colour conversion shares the card). With
  the streamer's 6.8 % set aside, the card's ceiling in the headset is about 93 % / 8.5 ms
  = 109 renders/s = **about 55 pairs/s**, and the game runs at 82 % of it.
- **So in the headset the render thread is the co-limit, more than on the simulator.**
  The render thread never waits for the game thread (`idle` 0.2, no STARVED window, no
  12 s cycle in this spot) and never waits for the runtime (`wait` 0.0, 0 PACE-BOUND).
  It spends 11.2 ms per tick in `R`, 6.6 ms in our present path outside the fence, and
  4.2 ms at the fence. The CPU-side share of that (our per-draw hooks inside `R`, the
  2.4 ms `tick` field, the 2.4 ms `end` field) is what the 18 % of idle card can be
  bought with: **at most about 44.7 -> 55 pairs/s**, and not past it. The ceiling
  sentence stands with a smaller number: no change to the mod's code lifts this card past
  about 55 pairs/s in the headset at this size.
- The tick is flat to 0.2 ms across all 46 windows, with nothing pacing it. That is a
  steady scene on a steady load, and it makes this spot a good A/B bench.

**RETRACTION.** "The `game_tick` hitch is Debug-only and needs no ticket" (the simulator
results above) is withdrawn. The optimised build in the headset shows 12 of them in 135 s:
`perf: frame gap 40-51 ms ... sat in: game_tick`, irregular, three times as a pair one
second apart. The simulator's optimised window had none, so the owner is something the
headset lane runs and the simulator lane does not, or runs cheaper. No log line clusters
before them (`ledger:` lines follow a gap, they do not cause it). Open, tracked on VR-160 (no separate ticket, by decision). Also seen
and not pursued: 112 `reentry: gates -> DOUBLE draw after N single tick(s)` recoveries in
the same 135 s, about 0.8 a second, with `L/s == R/s` holding at the 3 s scale (VR-77).

Measured in passing, for the Q3 table: `draws/regions` 4035 probes per 3 s at 0.8 us per
probe = 1.1 ms/s; NativeProfile's `DrawIndexedPrimitiveUP-hook-inclusive` 26,000 calls per
3 s at 0.8 us sampled mean = 6.9 ms/s. Both are under 1 % of wall time. The shipped-on
instruments are therefore NOT the 18 %; what is left is the hooks on the indexed draw
path, the constant hook (VR-162) and the present path's own 2.4 + 2.4 ms per tick.

### RESULTS, round 2, rig B, 2026-09-20: four costs of ours on the present thread, and the size test

`perf parts on` (new, default off) splits the present path into named parts. It named three
of these in three simulator legs; the fourth came from `hud off` against `hud on`.

| # | Cost | Where | Measured | Fix |
|---|---|---|---|---|
| 1 | The video-memory sampler (added 2026-09-18) | `gpu_memory::tick`, present thread | gated to 4 Hz as designed, but one sample is two kernel video-memory queries plus a `VirtualQuery` walk of the whole 32-bit address space: 25.8 ms on the simulator, 27.6 ms (max 46.9) in the headset. 0.8 ms per present averaged = four stalls a second. This is the `sat in: game_tick` frame gap, and it explains the retraction above: it is a wall-clock cost, so the optimised build has it too | its own low-priority thread at 1 Hz; it prints its own cost; `[Perf] GpuMem=`, `gpumem on|off` |
| 2 | The HUD panel, twice per displayed frame | `hudcap::end_frame` | per in-use element, per PRESENT: a full 3012x3122 `StretchRect`, a full-size clear, a D3D11 draw and a `Flush`. `hud off` / `hud on` / `hud off`, fast phase: 16.0 / 17.8 / 16.0 ms per tick (62 against 56 pairs/s) | `[Hud] OncePerPair`, `hud pair on|off`: hold the first present of a pair (clear only, last output stays delivered, never two holds in a row) |
| 3 | `status.json`, once a second | `status::tick`, present thread | file create, write, close and rename on the present thread | built on the present thread, written by a worker; the present thread never waits for the disk |
| 4 | The live-object table | `BuildLiveSet` | a copy and `qsort` of about 115,000 pointers with the table's lock held throughout, from five independent periodic timers on two threads | copy and `std::sort` into a scratch buffer with no reader lock, swap under the lock; `RefreshLiveSet(maxAgeMs)` lets periodic callers share one rebuild |

Simulator, fast phase, same save and view, RelWithDebInfo, 3012x3122: **17.8 ms (56 pairs/s)
before, 15.7 ms (63.7 pairs/s) after**, the game's 3D engine at 93 %, zero frame gaps in the
window. `OncePerPair` off / on / off on its own: 16.9 / 15.8 / 16.9 ms; `hud/beat` reads
`held` = presents / 2, deliveries halved, `empty-while-armed=0`.

**Headset run 2** (VDXR 120 Hz, 3012x3122, build `vr33-hands-working-557-g02d34c1e`, `config
RelWithDebInfo`, DLL sha256 `BF472731`, the same save, standing still, 19 windows per leg;
log `vr160-HEADSET2-fixes-120hz.log`):

| Leg | tick median / p90 | pairs/s | our present path (`in`, P1+P2) | frame gaps |
|---|---|---|---|---|
| Headset run 1, before the fixes | 22.1 / 22.3 ms | 44.7 | 10.5 ms | 12 in 135 s, all `game_tick` |
| Fixes, `OncePerPair` off | 20.6 / 20.9 ms | 48.3 | 8.2 ms | 1 in the whole run |
| Fixes, `OncePerPair` on | 19.9 / 20.1 ms | 49.7 | 5.0 ms | (same run) |

The game's 3D engine 81.8 %, the streamer's 3D 7.3 % and encode 23 %: 8.2 ms of GPU per scene
render, a headset ceiling near 56 pairs/s, and the game now at 88 % of it (82 % before).
Hands healthy (`hands OWNER=SkelControl writes=302/3s`). The player reported no HUD flicker
with `OncePerPair` on and an overlay reading of 45-52, and could not judge smoothness
standing still. On that verdict the missing-key default of `[Hud] OncePerPair` is now 1
(no config-version bump, so no ini is rewritten: VR-159). Scope of the judgement: about a
minute, standing, three elements; the lever stays for anyone who sees otherwise.

**The size test, because the question was whether something deeper is wrong.** One
simulator leg at the sibling mod's 2064x2208 per eye (4.56 MP), same build, same save,
`res: HONOURED - the game renders 2064x2208`. Prediction recorded before it: 70-85 pairs/s,
and a rate still near 50 would mean a deeper fault.

| Size per eye | fast phase | GPU 3D engine | slow phase |
|---|---|---|---|
| 3012x3122 (9.40 MP) | 15.7 ms, 63.7 pairs/s | 93 % | 20-21 ms, 48-49 pairs/s |
| 2064x2208 (4.56 MP) | 11.1-11.9 ms, 84-90 pairs/s, touching the simulator's 90 Hz pace | 63-67 % | 20.3 ms, 49 pairs/s |

CONFIRMED: on the render side the pixel count is the limit on this card, and at the
sibling's size the same code reaches the sibling's class of rate with a third of the card
idle. The four files `arm-res.ps1` writes were restored byte-for-byte afterwards.

**And one thing the size test exposed.** The SLOW phase of this save's 12 s cycle does not
move with the size at all: 20.3 ms at both. There the game thread needs about 20 ms per
tick and nothing on the render side matters. The earlier reading "identical in Debug and
RelWithDebInfo, so not the mod's code" is WEAKER than it was written: script-lane code that
spends its time in `VirtualQuery` (every `RangeReadable`) costs the same in both configs,
so that A/B could not have told such code from the engine's. OPEN, and the next test is
direct: the same leg with the ProcessEvent hook not installed. The headset spot shows no
such phase (`idle` 0.2 ms), so this is about other places in the game, not that one.

## Reboot/save comparison and resolution check (2026-09-19)

After a PC restart and a known-good save, the tester reports performance close
to the prior baseline with a possible small residual slowdown. These two changes
were combined; neither is independently established as the cause or fix.
A remembered VD resolution percentage changed from about102 to112, prompting an
actual-dimension audit. Latest verified build486 starts at tick107093; DLL hash
34bc3cb31eb540190a4c0dd2291ac47a0be7a325e9b871622913cabd50969c3d.
Artifacts: build/playtest-candidates/hud-improvements/run486-after-reboot.

Latest and original accepted486 both create3012x3122 game targets and eye
swapchains, with VDXR recommending2688x2880. Archived464,470,473,476,483 and490
also have those same dimensions;480 additionally has its already documented
accidental1355x1405 reset. VDXR currently reports1.000 supersampling and1.000
upscaling.3012/2688 is1.1205, numerically consistent with the reported112 percent;
this is an inference about the overlay number, not verified overlay semantics.
No logged evidence supports a new render-resolution increase across these runs.
The remembered102 percent remains unverified. No resolution setting changed.

Before the reboot report, the streamer was restarted with the game closed and
without settings changes; prior process27772 was replaced by32024 at12:15:02.
Its effect was not tested separately. The poor run preceding that restart is
archived as run486-rollback. Do not call the reboot, save, streamer restart or
resolution a confirmed explanation of the earlier regression.

## Rollback489 performance regression and accepted486 baseline (2026-09-19)

Tester reports poor performance from launch after the rollback. Verified489
banner and installed DLL71540df1; archived full run in
build/playtest-candidates/hud-improvements/run489-rollback. Runtime is VDXR1.0.10,
3012x3122,120Hz, matching accepted486. Whole INI comparison with pre490 differs
only in explicit VDXR selection and reticle appearance. Build optimization flags
and generator instance match the main checkout. No cause established.

Whole-session descriptive comparison (different lengths/scenes, not a controlled
benchmark): accepted486 5.6min,97 tick samples, median9.4ms,p9010.6ms,5.5 gaps>=60ms/min;
rollback489 36.1min,626 samples, median10.7ms,p9016.0ms,11.4 gaps>=60ms/min.
489 logs575 frame-gap events attributed to xrEndFrame across all gap lengths;
486 has none in that category. This locates waits, not their underlying cause.
VDXR's own log has no reported error. SteamVR processes were absent after exit.

Correction to rollback target:489 was the immediate pre-split source, while486
was the last accepted pre-split playtest. Rebuilt3ec56e3bc in isolated checkout,
froze rollback-486 with copied503 installer and dry-run, and installed it with
its entire archived install486 INI except explicit nativeVDXR selection.
Hash34bc3cb31eb540190a4c0dd2291ac47a0be7a325e9b871622913cabd50969c3d;
banner vr33-hands-working-486-g3ec56e3bc. This is a rebuild, not the original DLL.
Full installed INI diff removes reticle colour keys and restores SizeDeg0.500;
CRLF1249/1249. Prior logs preserved. Next question: does performance return to the
previous baseline immediately after loading? Improvement implicates the intervening
reticle change or session state; no improvement leaves runtime/streamer/system
state and the rebuild comparison open. No claimed fix, launch, or merge.

## Vitals-first candidate and pending desktop-present A/B (2026-09-19)

Candidate505 implements the selector/model-draw diagnostic stage of
CODEX_PLAN_VITALS_CHOKE.md. It leaves ReduceDesktopPresent unchanged so the
first question remains model drawing. VR-144 still needs the independent
ReduceDesktopPresent=0 comparison with run499's 28% wheel singles/writes.
No new wheel measurement or performance verdict is claimed.

## Wheel stutter and the crouched-load stand-up stall (2026-09-19, VR-144, VR-143)

- VR-144, the weapon wheel stutter: the share of one-eye ticks while the wheel
  is open (`menu/head ... singles/writes`) was 10% (run470), 25% (run486), 47%
  (run497) and 28% (run499). Build 497 took a head-pose lock on every hand draw.
  Build 499 made it once per present, only while attaching, and the ratio fell
  back to 28%. Remaining suspect: `[VR] ReduceDesktopPresent`, 0 -> 1 between
  run473 and run476, when the ratio first rose. Next: an A/B.
- VR-143, the stand-up stall after loading a crouched save: run499 loaded
  crouched and stood at 19399687. The next perf tick read 20.5 ms at 48.6/s,
  against 9.5 ms at 95-106/s normally. The excess is `out` idle 10.7 ms, outside
  the mod's present path, and the mod's hook scopes were no higher than
  elsewhere. The load window shows 11996 TexLockRect calls in 3 s, so texture
  streaming and the device shadow copy are the first suspects. Not fixed.

# Performance research

## Periodic xrEndFrame hitch: measured, not yet explained (2026-09-18)

**Report:** a large frame drop every 5-10 s, believed to happen while walking.

**Measured in the build464 run** (`vr33-hands-working-464-g0b7171bd1`, 120 Hz,
VDXR, 3012x3122, mirror-off/strict; logs in
`build/playtest-candidates/weapon-mirror/run464`):

- 363 of 366 gameplay frame gaps sit in `present-tail (xrEndFrame)` on the +1
  (pair-closing) present, 60-107 ms, typically 88-94 ms (about 11 display slots).
  Bursts of 1-7 such stalls about 100 ms apart.
- Burst starts sit on a ~4.0 s beat that jitters by about +-1 s (consecutive
  intervals pair up to ~8 s: 3016+5297, 2906+5250, 2625+5453). Peak of the
  interval histogram 4.0-4.5 s.
- **Not movement.** Burst rate while moving (>= 30 uu/s) 12.6/min vs still
  14.3/min; head turning >= 20 deg/s 14.5/min vs calm 14.2/min (458 run the
  same: 11.8 vs 12.0). Bursts also occur with the pause menu open (13 in 464).
  Walking makes a 90 ms freeze visible; it does not cause it.
- **Not the game's draw cost.** In 146 of 154 gap rings the game's own GPU time
  per present is normal (median max 4.7 ms); the last presents' timing queries
  are pending. Only 8 rings show a present over 20 ms.
- **Not the texture-streaming change.** The 452 run from before the
  NumStreamedMips=-1 change (07:34) already had 16.9 stalls/min >= 60 ms.
- **Not phase-locked to the mod's 3 s diagnostic beat** (burst delay after the
  capture beat is spread evenly over 0-3 s). No runtime period change all run.
- **Not the capture queue.** Blit-fence timeouts are 5-7 per run (lifetime);
  20-50% of grabs wait on the fence, so D3D9 queue depth is bounded by the
  capture ring.
- **It got worse over the last builds.** Stalls >= 60 ms per gameplay minute,
  per archived build (noisy, sessions differ): 353-395 1.6-6.6; 397 18.1,
  399 10.1; 407-427 1.7-6.9; 431-448 5.3-11.2; 452 10.3-16.9; 458 15.6;
  464 29.7 (>= 80 ms: 19.2). Same ~90 ms signature throughout, so one
  mechanism fired more often, not a new one. 90 Hz runs (239-264) sat at the
  same low rates as early 120 Hz runs, so the display rate is not the driver.

**Hypotheses left, each with the instrument that can kill it (build465):**

1. Video-memory paging (32-bit process, 4096 texture pack, ~3.3 GB of texture
   creations per census, 3012x3122 targets): `gpumem` samples DXGI
   QueryVideoMemoryInfo LOCAL/NON_LOCAL usage vs budget for the D3D9 adapter at
   4 Hz, with process private bytes and the largest free address range; logged
   every 10 s, on any NON_LOCAL move of 64 MB, and at every frame gap.
   Prediction if paging: usage at/over budget or NON_LOCAL moving at the stalls.
2. Texture-streaming uploads through the Managed=shadow twins (every streamed
   mip is an UpdateSurface): `device/stream` logs the last 2 s in 100 ms buckets
   (uploaded MB / created MB, CPU ms inside UpdateSurface) at every frame gap.
   Prediction if streaming: a burst of MB in the buckets just before a stall.
3. The runtime/streamer side (VDXR/VD encoder or network): what remains if
   both of the above read flat at the stalls. The VD performance overlay
   (network latency, encode) during a burst would then be the next evidence.

No mitigation shipped: pacing to a fixed rate was already tried (2026-09-15)
and felt laggier; nothing else is justified until one hypothesis survives.

## Status: accepted FOV/mirror improvements merged; broader research shelved

The earlier research established a substantial
resolution-independent rendering cost, ruled out several cheap fixes, and measured
ordinary per-eye scene preparation. It did **not** establish a safe way to eliminate
that work or a single CPU/GPU bottleneck. HUD is outside this investigation.

This is the single maintained performance record. Update findings, failed hypotheses,
sources and resumption decisions here. STATUS/NEXT_SESSION only summarize and link.
Engine derivations remain in [ENGINE_NOTES.md](ENGINE_NOTES.md); stereo correctness
and pose history remain in [FLICKER_REFERENCE.md](FLICKER_REFERENCE.md).

Unfinished performance tickets VR-17, VR-67, VR-77, VR-113, VR-115, VR-121, VR-123,
VR-124 and VR-125 are parked in Backlog, unassigned. Completed stability fixes stay
completed. Merging this research does not promote experimental settings.

## Active exception: F11 clarity, FOV and desktop presentation (VR-50, 2026-09-15)

The broader optimization program remains shelved. A new headset observation reopened
VR-50 only: toggling F11 twice yielded a sharp, smooth, smaller square view with stereo
depth and normal head/walking response. This is a reported perceptual improvement,
not a controlled FPS result or proof of a new anti-aliasing mode.

Verified build307 DLL and log banner. Both logs and installed configuration are in
`build/performance-results/f11-discovery-20260915-201742`. Native F11 toggles engine
fullscreen/windowed; the mod's VirtualMode converts fullscreen requests to windowed
while retaining the requested backbuffer dimensions.

| State | Backbuffer / XR swapchain | Horizontal FOV |
|---|---|---|
| Before F11 | 2750x2850 | 108.07 degrees |
| First F11 | 1355x1405 | Falls gradually toward 75 degrees |
| Second F11 | 2750x2850 | 74.89 degrees, remains narrow |

Both source bounding boxes remain full-size. Projection layers stay enabled, runtime
quad stays off, and eye separation remains about 6.8 uu. This was not a mono cinema
screen or an embedded low-resolution rectangle. Immediate Present was applied before
and after the resets, so this did not newly unlock vsync.

**Specific boundary:** the persistent FOV lever's natural-base cache had recaptured its
own widened 108-degree output. It then multiplies the sensor by target/natural on every
script dispatch. The small aspect change (2750/2850 versus 1355/1405) makes the target
slightly smaller; feedback can repeatedly narrow the FOV. Restoring the original aspect
makes the ratio approximately one and retains the narrow result. Logs show 3,710
natural-base captures, with the later 3,709 recapturing about 108 before F11. The old
assumption that this recapture was harmless fails when the target changes.

At the restored width, central density is about 31.34 pixels/degree at 74.89 degrees,
24.00 at 90, and 17.41 at 108.07. Thus the narrow view has roughly 1.80x the normal
central linear density; 90 offers about 1.38x with more angular coverage than 75.
These are projection arithmetic, not optical headset resolution or added AA samples.
A narrower frustum may also reduce visible work, but moving-view tick windows averaged
74.67 before, 85.8 at lower resolution, and 77.63 after restoration. Different views
prevent a causal performance claim. Do not repeat the failed query-helper hypothesis.

**Candidate:** `vr33-hands-working-353-gf0fa9fef4-dirty`, archived under
`build/playtest-candidates/vr50-projection-fov90`, installed with `[Screen] ProjectionFov=90`.
The explicit request makes 90 the runtime, missing-key, generated and packaged default;
0 or live `projectionfov off` restores the old headset-derived route. Valid range 60-120.
It uses the existing temporary reflected CameraCache.POV.FOV scope around both eye draws,
with proportional tangent-space zoom, identity/liveness validation and restoration.
The persistent ratio writer is suspended during the gameplay scope, avoiding feeding the
temporary narrow FOV back into itself. Cinematic handling retains precedence. This does
not repair all legacy natural-base behavior, especially after another F11/aspect change.
No image-owned orientation or stereo-pair synchronization policy was changed.

Build and standalone tests passed: 30,045 FOV/restore checks including 10,000 repeated
scopes, frame math, 248 reentry checks, 23 single-tag checks, 9 exports, golden INIs and
lint. No game/simulator launched. Final rendered acceptance and comfort remain untested.
Both logs, prior DLL and INI archived at
`build/playtest-candidates/installs/20260915-203356-472356` before replacement.
The full installed INI comparison adds only ProjectionFov=90; existing saved HUD and hand
trim changes since the old manifest are preserved. CRLF and installed hashes verified.

**First build353 headset result:** improved appearance versus108 reported, but the
rectangular boundary remains visible. Verified353 log shows gameplay submission90.00
at2750x2850 and camera scopes108.07 ->90 with zero refusals through5,531 writes.
This validates reported clarity improvement and scoped submission, not a complete
world-matrix audit or a measured performance gain. The remaining request is to enlarge
the angular presentation of the90-degree image. A projection layer has no screen-distance
parameter; widening only its submitted frustum magnifies the image and mismatches rendered
rays, potentially changing head-motion gain and stereo geometry. A stereo quad/screen is
a different presentation mode and would need explicit design/testing. Do not silently
replace the accepted projection with that mode. Evidence: `build/performance-results/vr50-fov90-headset-20260915-203910`.

**Live slider follow-up:** build355 (`vr33-hands-working-355-gf8380e1e3-dirty`) is now
installed from `build/playtest-candidates/vr50-fov-slider`. F10 -> View exposes Custom
gameplay FOV, a live60-120 slider and Reset FOV to90. Existing Save As Defaults persists
it. This is UI over the353 setter, with no new render behavior. Default remains90.
Build, exports, lint and diff checks pass; UI/headset operation is not yet validated.
Both logs and prior files archived at
`build/playtest-candidates/installs/20260915-204101-720438`; complete INI diff is empty,
installed hashes and CRLF verified by installer. No game/simulator launched.

**Build355 acceptance and new default:** the user reports100 degrees removes the visible
black rectangle and retains a substantial apparent clarity improvement. The verified355
log records live slider changes and100.00-degree submission at2750x2850. This confirms
slider operation and the preferred FOV; it does not measure a resolution increase from
FOV alone. Accepted evidence is archived under
`build/performance-results/vr50-fov100-accepted-20260915-205050`.
100 is now the runtime, missing-key, generated/package and F10 reset default.

**Current candidate:** build356 (`vr33-hands-working-356-g0b1b9ca55-dirty`), installed
from `build/playtest-candidates/vr50-fov100-pixels110`. User clarified the resolution
control should represent TOTAL PIXELS, superseding the initial per-axis interpretation.
100% always means2750x2850;110% produces2884x2989 (109.988% after pixel rounding).
Both axes scale by sqrt(percent/100), preserving aspect within half-pixel rounding per
axis. F10 -> Display -> Total pixels (%) offers50-200%, previews width/height, and
Set for next launch saves via the existing ResRequest path. Dragging alone changes
nothing. Settings survive restart without Save As Defaults. No new live reset is
introduced: prior engine setres tests were inert. The mod INI is authoritative at the
next launch and reconciles its launch-argument mirror automatically.

100-degree FOV is accepted;110% resolution is a new unaccepted trial. It adds about10%
pixels, not21%. The uninstalled per-axis110% draft was superseded before installation.
Build, exports, golden INIs, lint and arithmetic checks passed. Both prior logs/DLL/INI
archived before install at `build/playtest-candidates/installs/20260915-205309-560273`.
The full installed INI comparison changes only RenderWidth2750->2884 and
RenderHeight2850->2989; saved100-degree FOV and all other settings are preserved.
CRLF/hashes verified. No game/simulator launched; UI operation and rendered size await
headset/log verification.

**Hub mirror-off discovery (verified356,2026-09-15):** the user reports an approximately
30-40% FPS improvement in the slow hub area after disabling the desktop mirror in F10.
Earlier sewer experience showed little perceived benefit. This is an area-dependent
headset observation, not a new matched A/B measurement or proof all performance issues
are solved. Earlier controlled sewer captures did show throughput gains with worse
frame-time tails; preserve those results rather than treating either scene as universal.
Evidence: `build/performance-results/vr50-hub-mirror-off-20260915-210745`. The356 banner
and DLL match; mirror-off logs confirm real skips, e.g.657/665 hooks skipped with zero
non-OK results in one late3-second window. FOV was also adjusted during this run, so
uncontrolled rate changes cannot isolate the reported percentage. No extra capture is
required merely to honor the requested default.

**Build357 defaults and live resize (superseded by359 below):**102-degree FOV, desktop mirror off,
120% total pixels (3012x3122 versus2750x2850; rounding only). Mirror-off is promoted in
runtime/missing-key/generated/package defaults by explicit request; guarded non-XR/menu
presentation fallback remains. ReduceDesktopPresent stays off. All unrelated settings
and accepted image-owned orientation/stereo policies remain intact.

The scale button now queues a byte-verified six-argument engine ResizeViewport call on
the next game-thread draw, before both eyes. The engine owns its window/RHI reset; no
proxy-forced D3D reset or synthetic F11 toggle. Fresh live-table/IsLiveObject owner,
current HWND/thread and vtable/ABI checks refuse unsupported calls. Set persists the
size and applies it in this run; only matched downstream capture shows Applied. A
10-second timeout reports unconfirmed without automatic retries. Static derivation and
23 production-code host fixture checks are in ENGINE_NOTES and viewport-resize-host.
Native D3D9Ex mirror-off regression passed120 GPU markers plus full-return/reset;
248 reentry and23 single-tag checks, release build, exports, golden INIs and lint pass.
No game/simulator launched. Native resize acceptance remains pending in the headset.
The earlier per-axis110% draft never installed; next-launch-only scale is superseded.

**Installed357:** `vr33-hands-working-357-g847030698-dirty`, bundle
`build/playtest-candidates/vr50-live-resize-hub`. Prior logs/DLL/INI archived at
`build/playtest-candidates/installs/20260915-211639-478150`. Full INI diff changes
ProjectionFov100.00->102, RenderWidth2884->3012, RenderHeight2989->3122 only.
DesktopMirrorOff was already1 from the F10 test. All other settings are preserved;
CRLF and hashes verified. Native resize is installed but not game-tested. Exact356
rollback remains archived.

**Latest359: live resize measured; Display-tab FOV flicker fix pending acceptance.**
357 performed one engine Reset to3135x3250 and capture confirmed it; dimensions stayed
there. Repeated zoom-like flicker instead coincided with260 post-resize FOV changes
between104 and108.05 degrees and repeated scope releases. Missing braces in the legacy
F10 Display FOV control wrote0 into the automatic target every idle UI frame, racing
Present's108-degree handoff. Source-confirmed defect, not resolution oscillation.
The initial150ms-expiry hypothesis is superseded by explicit scope releases and this
writer. Corrected control writes only on edits; production regression7/7 passes while
old control fails7/7. No stereo/orientation/timeout changes. Full flicker record and
falsifiable continuation: [FLICKER_REFERENCE.md](FLICKER_REFERENCE.md), latest VR-50 entry.

Installed359 (`vr33-hands-working-359-gb5e0af9dc-dirty`), candidate
`build/playtest-candidates/vr50-display-fov-flicker`: requested defaults103 FOV,
130% total pixels3135x3250, mirror-off. Archive before install:
`build/playtest-candidates/installs/20260915-213922-577631`. Full installed INI diff only
ProjectionFov102->103; live Set had already saved130% dimensions. Other settings and
CRLF preserved. Build/exports/golden/lint passed; no game/simulator launch. Host success
is not visual acceptance.357 reproduction archive:
`build/performance-results/vr50-resize-flicker-20260915-213237`.

**One launch question:** is the view stable with F10 Display open and after Set120%
then130% in the same run? Expect brief resize pauses, one size transition per Set,
steady103-degree gameplay projection and no repeating zoom pulses. Continued flicker
requires reading the newly explicit scope gate reasons and the actual submitted FOV;
do not assume an eye-sync or resolution-flapping cause. No F11 during this test.

## Mirror-off pacing review (2026-09-15)

**Repository defaults verified:** 103-degree FOV, 130% total pixels (3135x3250),
DesktopMirrorOff=1 in runtime/missing-key defaults, generated/release/golden INIs
and F10 reset/fallback values. Both golden comparisons pass. No new binary is
needed for this request; installed359 already contains these defaults. A later
live Set saved120% in the machine INI; this does not change the repo defaults.

**Correction to the broad "worse tails" warning:** rereading the two original sewer
Full/Off/Full captures shows a consistent small p95 regression and faster typical
frames, but not consistently worse extreme stalls. These are fresh stereo-pair
submission intervals, not headset display FPS or measured motion-to-photon latency.

| Run / metric | Full before | Mirror off | Full after |
|---|---:|---:|---:|
| First, fresh pairs/s | 87.01 | 98.28 | 84.72 |
| First, p95 interval ms | 18.878 | 21.250 | 20.486 |
| First, p99 interval ms | 37.350 | 35.166 | 40.814 |
| Second, fresh pairs/s | 81.86 | 95.62 | 84.02 |
| Second, p95 interval ms | 21.364 | 22.440 | 21.309 |
| Second, p99 interval ms | 42.446 | 36.959 | 36.806 |

**More specific boundary:** fully interior three-second capture windows, ending
more than six seconds after each phase starts and before its end, show D3D9 blit
fence waits rising with mirror off:

- First run:149/4178 (3.57%) ->631/4724 (13.36%) ->124/4091 (3.03%).
- Second run:42/3915 (1.07%) ->471/4604 (10.23%) ->76/4054 (1.87%).
- Every logged large frame-gap event in those windows is attributed to
  `present-tail (xrEndFrame)`, in all three modes. Counts are11/11/16 and18/10/13.
  These gap events use a dynamic threshold; counts are not a fixed-threshold
  stutter comparison, nor does API attribution establish the underlying cause.

Mirror-off skips the desktop snapshot/re-blit and native Present after capture/XR.
It issues a current-work D3D9 event and one GetData(FLUSH), accepting S_FALSE as
submitted-but-pending. This is submission, not a completion wait. Capture keeps its
separate ownership fences. Microsoft's [D3D9 queries reference](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries)
confirms that distinction. Removing Present's waiting plausibly lets capture reach
unfinished work sooner; that is an inference supported by the increased wait
frequency, not proof that an unbounded GPU queue causes every long frame.

**Routes:** preserve mirror-off. Best prospective mitigation is pacing complete
stereo pairs or bounding queued work without restoring desktop presentation.
Existing `Pace.SyncHz` gates only pair opening; the generic per-Present FpsCap is
bypassed by reentry. No arbitrary cap is promoted: a cap may trade some peak FPS
for regularity and cannot shorten a frame already slow inside xrEndFrame. A bound
on outstanding GPU work would need independent completion events; the current
submit-only query deliberately abandons prior results and cannot serve as that
bound. Never remove the capture ownership fences or change image/pose identities.
Reduced desktop cadence already failed to provide consistent tail improvement;
nonblocking Present and maximum-frame-latency sweeps are also exhausted routes.

**Next discriminating check, using existing359:** in the slow hub at fixed103 FOV
and130% pixels, F10 Display's existing Full/Off/Full benchmark compares one stationary
view for100 seconds and restores the original mirror mode. One question: does the
current hub benefit also worsen fresh-pair p95/p99? If both improve, retain off
without adding a limiter. If throughput improves but p95 worsens beyond both full
baselines, test pair-opening pacing against off/unpaced/off with the target derived
from this hub's measured sustainable rate. If the bracketing full runs drift or
settings/view change, the comparison is inconclusive. No benchmark was armed or run
in this review; no new headset run is claimed.

Evidence: existing `build/performance-results/desktop-first`, `desktop-second`,
`desktop-reduced-first`. Current359 DLL/banner verified and both logs/INI/manifest
archived to `build/performance-results/vr50-mirror-review-20260915-224832`. Current
run has no controlled desktop A/B; do not infer a new mirror causal result from it.
No install or runtime policy change. Visible acceptance of the prior FOV fix still
requires the tester's report.

## Pair-pacing test prepared (2026-09-15, build361)

The user requested the pacing comparison directly, superseding the proposed extra
mirror on/off hub test. Installed `vr33-hands-working-361-g140afb6e7-dirty` from
`build/playtest-candidates/vr50-pair-pacing-ab`. Existing pair-opening pacing is
unchanged; this adds automatic A/B/A control and an actual delay-event counter.
No engine-memory, eye-tag, image-owned orientation or capture-fence policy changes.

- `[Perf] DesktopAb=3` arms one comparison per launch. Repo/missing-key default
  remains off; F10 Display has a live pair-pacing benchmark selector and Start/Stop.
-30 seconds of gameplay settle, then30 seconds each: mirror-off/unpaced,
  mirror-off/paced, mirror-off/unpaced. First3 seconds of each segment excluded.
- Target is floor(90% of baseline fresh-pair rate), capped at measured headset Hz.
  The10% margin is an experimental choice, not a measured optimum. Invalid/empty/
  overflowing baseline refuses a target. No new fixed FPS default is promoted.
- Counts successful submissions with both captured serials renewed. Logs p50/p95/
  p99/p99.9/max, rate, fixed-threshold exceedances, held submissions and full-phase
  pacing-delay events. Zero actual delay events means pacing was not exercised.
- End, manual stop or menu/load abort restores original mirror/reduction/pacing/
  target. Changing mirror or pacing during measurement aborts the comparison.
  Installed ini keeps DesktopAb=3 until the agent disarms it after reading results.

**Validation:**26 production-benchmark host checks pass: transition order, automatic
rate selection, display bound, held/warmup rejection, retaining long gaps, insufficient
samples/overflow, external mode changes, completion/abort restoration and original
Full/Off/Reduced behavior. Release build,9 exports, package golden and lint pass.
No game/simulator launched. These prove benchmark control, not headset smoothness.

Both build359 logs/INI/manifest preserved before changes in
`build/performance-results/pair-pacing-before-20260915-230201`; DLL/banner verified.
Install archive `C:/dev/Dishonored-VR/build/playtest-candidates/installs/20260915-230217-845830`.
Full INI diff: DesktopAb0->3, RenderWidth3012->3135, RenderHeight3122->3250; this
restores requested130% pixels for all three phases.103 FOV and mirror-off preserved,
all unrelated settings preserved, installed hashes/CRLF verified. Source snapshot
ships in candidate/source.patch. Exact359 rollback remains archived.

**One launch question:** does pair pacing reduce hitching versus both surrounding
unpaced phases while retaining useful mirror-off throughput? Load the slow hub;
use the30-second grace to settle into one view, then remain there for the90-second
comparison (two minutes total after gameplay starts). No F10/F11/setting changes or
menus during the comparison. Expected: pacing actually engages, rate approaches the
calculated target, and slow-frame intervals improve. Better p95/p99 beyond baseline
spread with modest throughput cost supports this target; lower rate without better
tails rejects it. Zero delays, mode abort or drifting baselines is inconclusive.
Visible discomfort or stereo instability rejects the candidate regardless of averages.
Agent reads and archives the result and disarms DesktopAb; no visual result claimed yet.

## Latest correction:120% default (2026-09-15, build362)

User clarified120% total pixels is the new default, superseding130%. Runtime
missing-key, generated/package/golden INIs and F10 scale fallback now use3012x3122.
FOV103 and mirror-off remain. Installed `vr33-hands-working-362-g40474ba59-dirty`
from `build/playtest-candidates/vr50-pair-pacing-120`; the automatic pair-pacing
comparison above remains armed (DesktopAb=3), now at120% throughout all phases.
No pacing behavior change. Same two-minute launch question and outcome criteria.
Both logs/DLL/INI archived before replacement at
`build/playtest-candidates/installs/20260915-230558-957582`. Complete installed INI
diff changes only RenderWidth3135->3012 and RenderHeight3250->3122; other settings
preserved. Build,9 exports, both golden comparisons and lint pass; installed hashes
and CRLF verified. No game or simulator launched; no new measured pacing result.

## Pacing result and strict mirror-off trial (2026-09-15, build363)

**Verified362 result:** user reports possibly more consistent delivery but a laggier
feel. The automatic trial completed, target66 Hz, with1645 actual pacing-delay events.

| Phase | Fresh pairs/s | p50 ms | p95 ms | p99 ms |
|---|---:|---:|---:|---:|
| Unpaced before | 73.77 | 12.035 | 22.319 | 41.524 |
| Paced66 Hz | 63.37 | 15.141 | 20.490 | 34.008 |
| Unpaced after | 96.58 | 9.524 | 16.104 | 23.171 |

The paced phase is slower than both baselines and improves tails only versus the
first. Baseline drift is substantial, so this is not evidence of a repeatable
smoothness improvement. Pacing is not promoted; automatic benchmark now disabled,
SyncHz remains0. Preserve the adaptive test for future use, not as a default.
Logs/INI/manifest: `build/performance-results/pair-pacing-result-20260915-231337`;
installed362 DLL and log banner verified before interpretation.

**Residual desktop updates:** real context fallbacks, not a cosmetic F10 label.
Late windows show2-4 native Presents/3 seconds, about5-7 ms per actual call.
The prior off guard requires a fresh delivered capture plus a runtime mirror callback.
A missing/held capture restores desktop Present even though the XR session is still
running. Removing these rare calls is not predicted to repeat the large full-mirror
FPS gain; the test targets residual updates and possible local stalls.

**Installed363** (`vr33-hands-working-363-g2714e9b73-dirty`), candidate
`build/playtest-candidates/vr50-strict-mirror-off`: new opt-in
`[VR] DesktopMirrorStrictOff=1` extends mirror-off across missing fresh capture and
missing mirror callback whenever the XR session has begun. Current-frame GPU submit
flush remains, as do image/eye identities and capture ownership fences. Normal
window/device/swap parameters are still required; stopped XR, unsupported parameters
or an explicit submission failure uses real Present. Zero native calls is expected
throughout healthy running-XR windows, including temporary capture gaps. Startup or
stopped-session desktop activity is outside that interval. This does not hide the
window, remove engine rendering, or switch the headset to a different image path.

The stricter option defaults0 in source/generated/package/missing-key settings,
with F10 Display toggle `Keep desktop frozen across VR frame gaps (test)` for A/B.
Main mirror-off default1,103-degree FOV and120% pixels3012x3122 remain unchanged.
Build/9 exports/both golden INIs/lint pass. Native D3D9Ex host verifies240 independent
GPU markers and pixels with zero desktop calls:120 guarded,120 strict without fresh
capture (half also omit the callback). Negative control with strict disabled presents
a capture gap. Stopped-XR, failed submit, unsupported-parameter, full-return and reset
checks pass. No game or simulator launched; rendered/headset result pending.

Before installation both logs/DLL/INI archived at
`build/playtest-candidates/installs/20260915-231719-701999`. Complete INI diff only
DesktopAb3->0 and new DesktopMirrorStrictOff=1. Hashes/CRLF verified. Source patch and
exact prior candidate retained. Existing profilers remain as previously configured.

**One launch question:** does the desktop stay frozen through normal hub play while
the headset remains responsive and free of new stalls? Expected log evidence:
strict=1, strictSkips increasing, actual=0 and nonOK=0 in running-session windows.
Zero actual calls plus normal headset behavior accepts suppression, not a measured
FPS gain. New stalls/eye instability rejects it. Remaining actual calls require
matching session state and logged parameter/query/context refusal before broadening
the guard. Pair pacing and its benchmark stay off throughout this test.

## Accepted profile and publication (2026-09-15)

User accepted363 strict mirror suppression and reports it may feel better. Verified
363 DLL/banner and archived both logs, INI and exact DLL in
`build/performance-results/strict-mirror-accepted-20260915-235444`. All621 logged
strict-mode windows have actual=0 desktop Presents. This confirms suppression;
subjective improvement is not a controlled FPS measurement.

At explicit request the complete saved machine INI is promoted byte-for-byte to
release/golden and the generated default writer, including HUD anchors/placements,
alpha gain/gamma, hand trim, crouch hold mode and existing diagnostic flags. Strict
mirror-off is now a compiled/missing-key default as well. FOV103,120% total pixels
3012x3122, mirror-off and strict-off enabled, pair pacing and benchmarks off.
Existing explicit INI settings still override defaults. New profiles reproduce the
accepted saved settings; the promotion does not add new HUD rendering behavior.

Branch renamed `codex/performance-improvements`; publication explicitly authorized.
Performance PR targets VR-Main and remains unmerged. PR67's crouched pitch fix will
be combined LOCALLY on a separate playtest branch, preserving this accepted profile;
its author branch and both GitHub PRs remain unmerged. Local integration is a test
of the combination, not a claim that the exact standalone PR67 head was tested.

## Results and routes

Numbers below come from different matched workloads. They must not be combined into
one frame budget. Fresh stereo submissions, eye Presents, simulator ticks and headset
refresh rate are different populations. Baseline rendering was 2750x2850 at 120 Hz.

| Route | Evidence and decision |
|---|---|
| Lower resolution | 1375x1425 (quarter pixels) improved hub throughput only about 6%. 3850x3990 hurt substantially, to roughly 45-50 fps. Both a fixed rendering cost and a pixel-dependent cost exist; neither sole CPU nor sole GPU ownership is proven. Original resolution restored. |
| Per-eye scene preparation | Ordinary InitViews costs 1.322 ms/pair in the fixed simulator pub view, including 1.082 ms of frustum culling. Best specific remaining CPU boundary; no sharing or skipping implemented. See detailed result below. |
| Extra left-view preparation | Reflection, not a second world tick: 0.199 ms/pair, including 0.140 ms culling. Lower priority than ordinary views. |
| Engine query-result waits | Real headset build316: 0.102 ms/pair across 14,230 pairs, 0.596% of elapsed time. Not a useful hub target; do not repeat unchanged. This bounds the measured helper, not all occlusion/visibility work. |
| Nonblocking desktop Present | Build313 off/on/off 58.21 / 57.70 / 58.29 ticks/s. 4,818 accepted attempts, zero busy skips. Failed hypothesis; implementation and one-off harness removed. |
| Omit desktop Present completely | Two sewer Full/Off/Full runs: 87.01 / 98.28 / 84.72 and 81.86 / 95.62 / 84.02 fresh pairs/s. Repeatable throughput gain, modestly worse p95, mixed p99/extreme tails (see review above). Earlier opt-in result; now mirror-off is the requested default after the separate hub report above. Sewer numbers are not a hub forecast. |
| Reduce desktop Present cadence | Full/Reduced/Full 86.34 / 90.28 / 89.27 pairs/s; p95 18.385 / 18.488 / 17.656 ms. Much waiting moved into remaining calls (about 1.46 ms/hook, 2.91 ms/actual call). No consistent tail benefit. |
| Dynamic shadows via game INI | Applied settings, simulator 79.94 / 80.63 / 79.26 ticks/s. No useful gain; do not repeat unchanged. Does not eliminate all lighting/shadow work. |
| Suppress selected diagnostics | Build295 baseline/reduced/baseline 63.66 / 64.48 / 64.27 fresh pairs/s, no consistent tail improvement. Keep accepted diagnostics. Mask covered ZAccount, PairTrace, FrameId and AttachCensus only. Cine.Trace/DrawCensus/PoseReport have functional dependencies. |
| Shader reflection cache | Build275 estimated reflection 2.801 ms/s plus bytecode 2.203 ms/s, not ms/frame. Too small for a complex cache as a leading fix. Layout/router/state/locking measurements are inclusive, not additive. |
| Native draw/state submission | Substantial aggregate sampled CPU wall cost. Resource-lock/upload samples small; their maxima do not bound unsampled calls. Driver waiting and deferred work remain unresolved. |
| D3D11 bridge copies | Conversion about 0.10 ms/eye; XR copy about 0.057 ms/eye. Individually small. Timestamp intervals are not additive GPU busy time or end-to-end latency. |
| FrameId readback / maximum frame latency | Earlier FrameId-off test remained inside baseline noise; D3D9Ex latency 1/2/3 sweep applied and read back successfully without benefit. Neither is an untapped proven fix. |
| Head/hand pose lag | Head/view candidate failed its measured prediction. Historical PoseLag=2 weapon improvement is a separate correctness result. Preserve accepted image-owned orientation; do not alter image tags to chase FPS. |

## Representative CPU evidence

The normal headset CPU capture is `build/performance-results/vr125-light-headset`.
Before/during/after rates were 55.47 / 55.80 / 56.80 ticks/s, with representative lag.
In its interior 5-35 seconds, render thread 19568 was running 72.43%, blocked 26.91%,
ready 0.57%. About 7.382 seconds of blocking ended with wakeups from NVIDIA worker
14260. The worker also spent many CPU samples polling. That is not useful scene work,
but neither polling nor the wakeup source identifies an automatically removable wait.
Low whole-machine utilization does not rule out a critical-thread constraint.

Of 21,085 render-thread CPU stacks, InitViews appeared in 2,575 (12.21% inclusive),
its dominant child in 2,062 (9.78%). Later render-stage return RVAs 0046C1F4 and
0046C208 appeared in 36.97% and 33.29%; inclusive shares can overlap. Those later
render/submission paths remain substantial and less precisely attributed.
Separate stage-cycle measurement put 87.95% of measured render-thread cycles outside
Present, where engine rendering and draw hooks execute. The game-thread viewport
calls totalled about 1.23 ms wall time but overlap queued render-thread execution.
**World tick runs once; viewport Draw runs twice. Duplicated NPC AI is not established.**

The heavy combined CPU/GPU capture changed the workload: 45.85 ticks/s versus 57.14
untraced (control about 24.6% faster). Its rolling GPU events began at 312.752 seconds,
after game exit at 239.448 seconds. Zero lost events did not make that GPU timeline
usable. It cannot identify the game's GPU queue bottleneck. A future GPU trace needs
short capture, retained in-game events, matching symbols and an overhead control.

## Latest boundary: InitViews and frustum culling

Build349 (`vr33-hands-working-349-g227da088c-dirty`) measured a fixed Hound Pits pub
view at simulator yaw 90, original engine resolution, 120 Hz source setting. Simulator
output was 1032x1104; this is CPU attribution, not headset performance acceptance.
Selected 13 complete windows span 39.056 seconds and 8,179 InitViews calls. Total
InitViews was 4,152.868 ms (10.633% elapsed). Unknown-eye boundary intervals were
excluded from pair normalization, leaving 2,707.5 equivalent tagged stereo pairs.

| Inclusive boundary | ms per tagged pair | Nested culling, ms per pair |
|---|---:|---:|
| Ordinary left + right preparation | 1.3215 | 1.0818 |
| Reflection preparation | 0.1995 | 0.1398 |
| Total | 1.5210 | 1.2216 |

Culling is about 82% of ordinary preparation and is already contained in InitViews.
Reflection appeared only in left intervals: 2,684 calls, usually ordinal 1 followed
by ordinary ordinal 2. Ordinary right was ordinal 1 (2,707 calls). Every selected
InitViews had one matched child call; no unknown classification, selector changes,
foreign calls, unmatched child, nesting, ordinal clamp or row overflow occurred.
Build347 independently measured approximately 1.523 ms/pair for total InitViews.

Off/on/off rates were 67.63 / 69.05 / 68.16 ticks/s. On-phase render-target workload
was lower, so this is neither a speedup nor a tight small-overhead bound. A subsequent
yaw change retained two nonblack views; simulator finished 23,479 frames with zero
errors, discarded frames or out-of-order submissions. No optimization was applied.

The probe byte-verifies both hook boundaries and uses derived calling conventions.
It classifies a borrowed renderer's family reflection selector only during the call,
retains no engine object and changes no engine result. Addresses, ABI and label
provenance are in ENGINE_NOTES, not duplicated here.

If resumed, first distinguish octree candidate gathering from per-view primitive
tests, using the existing stacks and offline code. A shared conservative candidate
set would need to include both eyes and current dynamic objects. Do not copy the
left visibility result or skip the right pass: existing VR-79 is a stereo-visibility
correctness constraint. Reflection/scene captures can also be view-dependent.

## Other routes worth preserving

These are options, not queued work or promised gains.

- **Submission and visibility history:** correlate later renderer stages, native
  draw/state counts, driver workers and a valid GPU timeline. D3D9 can charge deferred
  work to a later API call. Cheap query reads do not rule out excessive draw counts,
  query issue cost or incorrect shared per-eye visibility history.
- **Per-draw state:** share one lazy per-draw snapshot before attempting a global
  binding cache. Any shader-layout cache needs resource-lifetime identity, negative
  entries, bounded size and reset handling; a reused pointer is not an identity.
  State-block Apply and mod-originated state changes must invalidate caches. Repeated
  animation-weight locking and invariant weapon transforms are measurable candidates,
  but never cache away fresh instance/liveness checks or animation handback.
- **Object discovery and liveness:** early UiDiscover scans measured 367/547 ms in
  build264. This is a load/tail route, not proof of stationary hub cost. Coalesce
  lifecycle-aware live-set refreshes, cache validated metadata and budget discovery
  by elapsed time. Keep current-level IsLiveObject for all engine writers; class names,
  unchanged pointers and permanent readable-page caches are not substitutes.
  ProcessEvent routing must retain synchronous writers on the script lane.
- **Bridge ownership:** drawing directly from the shared SRV into an acquired XR RTV
  could remove an intermediate copy. Formats, alpha, overlays, acquire/wait/release,
  held-frame fallback and reset must remain correct. A third capture slot trades
  memory/latency for reuse slack; no unmeasured queue gain is assumed. VR-114 separately
  tracks capture timeout/error paths that proceed without explicit ready ownership.
  No timeout explained the selected slow window. Never remove image-ownership fences
  or Flush just because an elapsed interval looks expensive.
- **Managed-resource emulation:** READONLY unlock uploads already skip. ShadowFullCopy
  uploads the written mip, not the entire mip chain. Dirty rectangles, repeated uploads
  and streaming pressure merit work only with measured bytes/use/lifetime evidence.
  Delaying upload until bind misses already-bound resources. Preserve mip/reset/readback
  behavior; 32-bit virtual-address pressure remains distinct from GPU memory capacity.
- **Quality and reconstruction:** motion blur, depth of field, ambient occlusion,
  frame smoothing and VSync were already off in inspected settings. Generic UE4/5
  console recipes do not establish a Dishonored control. PoolSize=160 is a streaming
  budget, not total VRAM. Dynamic resolution needs stable output resources and an
  internal viewport/upscale path; recreating swapchains per change would hitch.
  Temporal upscaling additionally needs history/depth/motion and stereo identity.
- **Architecture:** alternate-eye rendering remains unimplemented and is a separate
  tradeoff: 120 alternating images/s is only 60 fresh images/eye with temporal mismatch.
  Single-pass stereo is a major engine/shader project. A different 64-bit executable
  requires new ABI/addresses/hooks and compatibility validation; bitness alone does
  not remove per-view work. Do not restore the retired DXVK path as a routine tweak.
- **Other measured boundaries:** input/runtime calls, pose-publication contention,
  periodic status I/O and per-frame hand correction need actual critical-path evidence.
  Mesh identification/rebuild is not automatically per-frame work. Release optimization
  already exists; LTO/PGO/inlining follow a hot-path profile, not a blanket fast-math edit.
  Runtime/compositor/encoding/clocks/driver policy need independent evidence; a rate near
  60 on a 120 Hz headset does not prove half-rate locking. SteamVR shim needs its own rig.

## Retained tools and removed experiments

All added experiment switches default off. Existing accepted rendering/diagnostic
settings remain intact. Run only one behavioral benchmark at a time.

| Retained code/tool | Concrete future use and limits |
|---|---|
| ScenePrepareProfile, `sceneprepare on/off` | Exact ordinary/reflection parent/child and per-eye classification on another scene or candidate. INI must arm hooks at launch; mismatch refuses safely. |
| QueryWaitProfile, `querywait on/off` | Complete helper wall time by caller/type/eye on a materially different workload. Launch-armed, pass-through; do not repeat the resolved hub question. |
| NativeProfile / RenderProfile | Bounded sampled native API/draw-hook and reflection/router/state timing. Useful for regression attribution; nested wall times and sampled maxima are not additive/exhaustive. |
| BridgeGpu | Bounded delayed timestamp/disjoint rings for conversion and XR copy; no profiling flush or wait. Keep unresolved/late/invalid/overflow counts. |
| CpuScopes, `perf cpu on/off` | Thread-cycle and wall-time boundaries, with thread/epoch checks. Coarse GetThreadTimes CPU-ms output removed because it was phase-biased and sometimes exceeded wall time. Cycles are not milliseconds. |
| Desktop Full/Reduced/Off and DesktopAb | Preserves a real throughput/tail tradeoff for future controlled comparison. Mirror-off now default by request; fresh-capture/session/parameter guards and current-work submission query required. Earlier old-query design failed frame 2 and was corrected. |
| DiagnosticAb and fresh-pair counters | Reversible collector-overhead check after future changes. Counts successful submissions with both eye serials renewed; restores on completion/abort. Not all enabled diagnostics can be suppressed safely. |
| Host tests, symbol resolver, thread profiles, WPR profile/timed recorder | Reusable validation and attribution without rebuilding tools. IP suspension samples are not on-CPU percentages; ETW needs a perturbation control. Helpers do not authorize game launches. |

Removed: nonblocking PresentEx(DONOTWAIT) policy, configuration/command seam,
borrowed Ex-device accessor, its two standalone test files and timed launch-phase
helper. The measured negative and original implementation remain in git history and
on the preserved resolution-floor branch. Also removed the invalid coarse CPU-ms
field and its per-boundary GetThreadTimes calls. No other diagnostic earned deletion
solely because one workload made its measured path cheap.

Shelving validation: Win32 RelWithDebInfo build, all nine DLL exports, frame/weapon/
animation tests, scene/query ABI tests, native/render sampler tests, bridge policy
and real-device lifecycle tests, desktop policy/copy/benchmark/native-device tests,
diagnostic A/B, reentry (248 checks), single-tag (23 checks), lint and both golden
INI checks passed. Game and simulator were not launched for this cleanup. Installed
DLL/INI hashes still match exact307; release/golden and installed INIs remain CRLF.

## Evidence rules and corrected claims

- Preserve exact DLL/PDB/INI identity and both logs before any future install/launch;
  compare the entire installed INI and verify CRLF. Match log banner before analysis.
  Keep matched saves, FOV, resolution, scene and warmup. Retain all baseline phases,
  populations and tails; a mean or a changing view is not enough.
- D3D9 GPU frame spans can contain feeding gaps. Capture DMA is outside the older
  render span and must not be subtracted from it. D3D9 marker gaps are not whole-GPU
  idle, and pending/unresolved queries are not zero time. CPU-side capture subtotals
  do not bound the whole D3D11 bridge. Present wall time is not automatically savings.
- Old reports mixed startup/menu and gameplay, mismatched timestamp windows, and
  called elapsed intervals CPU work. The claimed migration of hitch ownership was
  withdrawn: all 18 supposed outside gaps preceded gameplay; its 19 gameplay gaps
  remained in the submission tail. The 12.4 ms GPU span belonged to 57.0/s, not 73.7/s;
  the 66.7/s untagged post-menu row was unusable.
- Controller sample age alone does not cause double correction. The relevant residual
  is mismatched head/view bases; changing the whole image's Lag cannot isolate a
  weapon-versus-world error. Earlier age arithmetic and pose-plumbing absence claims
  were withdrawn. Accepted stereo fixes supersede those drafts; see FLICKER_REFERENCE.
- Raw flat 240 fps at 1440p is capped and not a matched workload: two 2750x2850 eyes
  contain about 4.25 times the pixels, plus wider view/submission work. Neither doubling
  flat cost nor aggregate CPU/GPU utilization predicts VR throughput.

## Provenance and recovery

Full pre-trim chronology is recoverable at commit `e02c97d74`, in this same file.
Earlier report paths redirect here. Local raw captures and analysis stay ignored;
never commit game-derived dumps. Under `build/performance-results/`:

| Evidence | Directory |
|---|---|
| Representative CPU stacks/waits | `vr125-light-headset` |
| Latest classified culling result, reproducible analysis | `vr125-culling-classification/sim-20260915-183218` |
| Original InitViews result | `vr125-initviews/sim-20260915-181540` |
| Query helper headset result | `vr125-query-waits` |
| Rejected nonblocking Present | `vr125-desktop-nonblocking` |
| Heavy trace and control | `vr125-etw-headset-20260915`, `vr125-etw-off-control` |
| CPU stages, shadows and symbols | `vr125-cpu-scopes`, `vr125-shadows`, `vr125-symbols` |
| Resolution trials | `quarter-pixel-20260914-221912`, `high-resolution-20260914-222549` |
| Native draw/state and reflection | `native-hub-20260914-215400`, `native-state-hub-20260914-220319`, `render-profile-first` |
| Bridge and diagnostic comparison | `bridge-hub-20260914-210808`, `diagnostic-hub-repeat-20260914-214029` |
| Desktop comparisons | `desktop-first`, `desktop-second`, `desktop-reduced-first` |

Candidate manifests and DLL/PDB/INI bundles are under `build/playtest-candidates`.
Exact307 restoration archive: `installs/20260915-183805-018147`. Preserve the branches
`codex/vr-113-performance-audit`, `codex/vr-115-desktop-present`,
`codex/vr-115-performance-rollout`, `codex/vr-121-render-thread-profile`,
`codex/vr-121-native-draw-profile`, `codex/vr-123-bridge-gpu-profile`,
`codex/vr-124-diagnostic-overhead`, `codex/vr-125-resolution-floor` and `performance-fix`.
They are ancestors of the consolidation. Unrelated older perf/camera branches were
not imported; their superseded stereo behavior is not part of this work.

## Primary references

Sources explain mechanisms, not measured Dishonored savings:

- [Epic UE3 level optimization](https://docs.unrealengine.com/udk/Three/LevelOptimization.html): game/render threads, driver overhead, visibility and object/light interactions.
- [Microsoft D3D9 profiling](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls) and [optimization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations): deferred charges, batching, state and dynamic buffers.
- [Microsoft D3D9 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries) and [NVIDIA occlusion culling](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-29-efficient-occlusion-culling): polling, flush, latency and overlap.
- [D3D11 Flush](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-flush) and [shared resources](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource): submission does not prove completion.
- [OpenXR wait](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrWaitSwapchainImage.html) and [release](https://registry.khronos.org/OpenXR/specs/1.0/man/html/xrReleaseSwapchainImage.html): image ownership contract.
- [Microsoft GPU accounting](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/): engine-specific utilization and summary semantics.

## Accepted combined main merge (2026-09-16)

The tester accepted build369 combining performance PR68 and crouch-camera PR67,
then explicitly authorized both merges. Installed DLL hash and log banner match
vr33-hands-working-369-g6c3ef07b4. Both logs and INI preserved under
build/playtest-candidates/pr67-combined/accepted-20260916. Crouch logs confirm
camera LEFT ALONE and standing pivot retained. PR67 merged as6ec63636e and PR68
as52107a094; resulting source/release/test trees match the accepted combination.
Source branches remain.103 FOV,120% pixels and strict desktop suppression remain
the accepted profile. No new controlled performance percentage is established.
Subsequent HUD work is VR-126 and is documented in HUD_ANCHORS, outside this research.


## VR-143: the stand-up stall - streaming and paging are CLEARED (2026-09-19)

The ticket was opened on run 499's reading that the load window carried 11996
`TexLockRect` calls, making texture streaming and the device shadow copy the
first suspects. Run 514 measured a comparable stall directly and clears both.

Run 514 (`vr33-hands-working-514-g8bba892f7-dirty`), the save load at t=6634593:

- `perf: frame gap 2441ms ... sat in: out/idle (waiting for the game thread) of
  #7664 tag -1 (2437.6 ms of in 1.3 / out 2440.1; wait 0.0 lock 0.6 endFrame 0.1)`
- `device/stream (gap)`: all twenty 100 ms buckets 0.0/0.0 MB, `totals uploads 0
  (0.0 MB, 0.0 ms CPU in UpdateSurface) creates 0 (0.0 MB) releases 0`
- `gpumem (gap)`: VRAM 2003 / 15293 MB and flat over 4 s, system-backed 88 MB
  and unmoving, largest free address range 1322.8 MB and unmoving

So: no texture uploads, no creations, no VRAM growth, no paging, no 32-bit
address pressure, and the render thread idle waiting on the game thread. The
streaming hypothesis does not survive this, and neither does paging.

**The measurement trap that remains.** `out` is "not our present hooks", which
is NOT the same as "not the mod". Every mod tick on the GAME thread - `PeLatch`,
the hand drive, the latches, the property resolvers, the marker hooks - runs
inside `ProcessEvent` and lands in the same `out` bucket as the engine's own
work. Run 499's conclusion that "the mod's hook scopes were no higher than
elsewhere" was read off the present-thread split, which never covered that lane.

**The instrument built for it** (`StandUpProbeTick`, `crouch.cpp`): the first
stand-up after a new pawn starts a bounded 4 s capture, 40 buckets of 100 ms,
recording presents and the script lane's own time and outermost dispatch count
per bucket, then printing them beside `device/stream`, `gpumem` and a `perf`
mark. A bucket the script lane never reached rolls forward EMPTY rather than
being skipped, because a run of empty buckets is the signature that matters: the
game thread was inside the engine and not in our code at all.

The reading is stated on the line and can print the unwelcome answer either way.
Script-lane ms rising with the buckets where presents collapse means the stall is
ours. A flat or empty script lane while presents collapse means the game thread
was in the engine and the mod is a bystander, which closes the ticket rather
than continuing it. Not yet run.


## VR-143 SOLVED: the stall is the crawl tuck deferring every discovery (2026-09-19)

Run 516, the first stand-up after a load, from the probe built for this ticket:

```
standup: presents per 100 ms, oldest first: 2 0 0 0 ... 0 1 1 0 ... 1 2
standup: script-lane ms per 100 ms: 3132 0 0 0 ... 0 64 652 0 ... 88 39
standup: totals over 4000 ms - 7 present(s) (571.4 ms mean), script lane 3975 ms
         in 218 outermost dispatch(es) = 99% of wall, 35 buckets the script lane
         never reached
```

99% of the window inside our own ProcessEvent handler. The empty buckets are the
signature working as designed: the lane could not roll a bucket because it was
still inside one dispatch.

What ran in it, from the same window (t=8176000..8180000):

| t | what |
|---|---|
| 8176000 | `script: EndCrouch`, `script: NotifyTakeHit` - knocked out of crouch |
| 8176000 | `skc: drive is ON but NO SkelControl slots are latched (probeFails=0)` |
| 8176000 | `skc: ==== SkelControl probe over 115054 objects ====` |
| 8176296 | the walk returns - 296 ms - then the ownership dump and `skc/prop` |
| 8178453 | `graft:` x25 |
| 8179140 | `wa/scale`, `wa/comp`, `wa/id` - weapon attach derivation |
| 8179968 | `dc:` x102 - draw capture re-arm |
| 8180000 | `blink: latched the live PowerBlink` - the latch finally gets its turn |

**Cause.** The crawl tuck's `if (t) return;` in `ApplyHandToMesh` sits above
`ApplyHandToMeshInner`, which holds the whole 30.95 discovery block. A crouch
therefore parked every discovery until the player stood, and the load left
nothing warm, so all of it ran at once. `probeFails=0` proves the probe had never
been attempted, not that it had failed.

Note that the walk itself is only 296 ms of the 4000. The rest is the cascade it
gates: graft, weapon attach, draw capture, each re-deriving from scratch.

**Fix.** The tuck runs the discovery and skips only the calibration request and
the drive writes. The fault guard moved above the tuck so the discovery stays
inside the walk's recovery, and every path out clears `g_walkTid`.

**Not yet re-measured.** The prediction this makes, and which the next run can
refute: the same capture should show the script lane spread thin across the
window instead of 99% in it, because the work now happens during the crouch
rather than at the release. If it does not, the cascade has another gate.


## VR-152: a per-draw log that leaked its own cap (2026-09-19)

**Report:** framerate reads high and consistent (100-110) but movement feels
laggy and stuttery; standing still and looking around is smooth; the weapon
wheel shows high FPS and feels just as bad. It was better a day or two earlier.

**120 Hz is NOT the change.** The tester runs 120 and has for days, and the
archived build 512 log is also 120 Hz and was the smooth one. The refresh is a
constant here, so it cannot be the regression. Recorded because the shipped ini
comment argues for 90 Hz and a reader will reach for it.

**What did change, measured over two runs at the same 2750x2850:**

| | build 512 (smooth) | build 517 (laggy) |
|---|---|---|
| mean `perf: tick` | 9.41 ms | 10.70 ms |
| mean rate | 103.2/s | 89.2/s |
| samples | 486 | 884 |
| `pcap/layout` lines | 5.3/s | 26.6/s |

**Cause 1, and the large one: `pcap/layout` leaked its cap.** The per-shader
naming guard reads

    if (saidN < 16) said[saidN++] = key;
    Log("pcap/layout: shader %p declares ...");

It stops REMEMBERING at sixteen and does not stop LOGGING, so the seventeenth
distinct shader onward prints on EVERY DRAW, forever. 77992 of those lines in
one 49 minute run - a five-argument format plus file I/O per draw, on the render
thread. The comment directly above it says it exists to prevent exactly this
("it produced a 25 MB log in a single short run"); the guard just did not hold
past sixteen. Full now means silent, with one Warn saying so.

**Cause 2, mine, from the same day: the awareness census paid for a line it does
not print.** `awareness_report` takes the position mutex and was called on every
parent update (16667 in one run) only to build arguments for a line gated to
once a second - and that mutex is the one `match_awareness_draw` takes per HUD
draw on the RENDER thread. Cross-thread contention at game-thread rate, for
nothing. The gate now runs first. `match_awareness_draw` also gets a lock-free
early out on an atomic publish stamp, because the common case is that no meter
is live at all and a mutex per draw to discover that is pure contention.

**Prediction the next run can refute:** mean `perf: tick` returns toward 9.4 ms
and `pcap/layout` falls to a handful of lines for the whole run. If the tick
does not move, these two were not the cost and the next suspect is the
discovery that VR-143 moved into the crouch.

**Not established:** why the distinct-shader count passed sixteen when it did.
Different levels draw different shaders, and the laggy run is twice as long, so
the 5x rate rise may be content rather than a change in our code.

## VR-158: fullscreen and vsync as live A/B levers (2026-09-20, UNMEASURED)

**Status: built and installed, nothing measured yet.** The prediction below has
not been tested and must not be quoted as a result.

**Where it came from.** Setting the game to windowed through its own settings
menu was reported to cost a large amount of performance, resembling the state
before the desktop mirror was turned off. That is a mechanism worth testing, not
a coincidence: a windowed D3D9 swapchain presents through DWM composition, so
every desktop present is paid for, while a fullscreen exclusive device bypasses
it. If that is what happens, the windowed cost and the desktop-mirror cost
(`DesktopMirrorOff`, already a measured win) are the same cost seen twice.

**What was missing.** Neither lever could be switched during a run. Fullscreen
was the literal `1` in the engine resize call; vsync (`[Perf] ForceNoVSync`, which
defaults to 1) is only read by `UncapPresent` at device create and reset, so it
had never been A/B'd in a headset at the current frame path. Both now switch live
through the resize path VR-50 proved - `fullscreen on|off` and `vsync on|off` on
the seam, and two checkboxes in the F10 Display tab. Each costs one device reset.

**Prediction the next run can refute.** At ONE fixed resolution, with the desktop
mirror already off:

* windowed -> fullscreen moves the tick and `stereo: beat` pairs/s measurably
* vsync on -> off moves the present rate but NOT the pair rate

If fullscreen moves nothing once the mirror is off, the mechanism above is wrong
and the two costs are one; record that outcome here rather than leaving the
prediction standing.

**Read pairs/s, not the tick mean.** TRAPS carries the reading error from VR-152:
median pairs went 109 to 45 across two builds, a 59% loss that the tick mean
showed as 13%.

**Four combinations, one session, one resolution.** Changing the resolution
between legs makes the comparison worthless, and `[Screen] RenderFullscreen` is
written by the toggle, so the ini after the session reports the last leg, not the
shipped default.

### VR-158 run 2026-09-20: three faults, and the game has been WINDOWED all along

First run of the levers. No A/B was obtained; what it produced instead is more
useful than the A/B would have been.

**The finding that reframes the ticket.** The proxy creates the device windowed
ON PURPOSE, and always has:

```
res: CreateDevice - the game asked for 2750x2850 windowed=0 (ask 2750x2850 fullscreen, virtual ON)
res: CreateDevice - VirtualMode: the game asked FULLSCREEN 2750x2850 (our advertised
mode); creating it WINDOWED with the backbuffer kept
```

2750x2850 is not a display mode on this rig (the monitor lists 20 modes, the
largest 5120x1440), which is why VirtualMode exists at all. So **every headset
session to date has run windowed**, and if windowed-through-DWM carries a cost,
this project has been paying it the whole time without knowing. The one
fullscreen reset in the run was `device Reset (2560x1440 windowed=0)` - a real
display mode.

That makes the original hypothesis untestable as stated: fullscreen at the
headset render size cannot be had while VirtualMode is on, and VirtualMode is
required to reach that size. The comparison that IS available is fullscreen at
2560x1440 against windowed at 2750x2850, which confounds mode with pixel count.

**Fault 1 (ours): the vsync toggle collapsed the render resolution.**
`ResLiveSetVsync` passed the DEVICE's fullscreen state into the resize. Under
VirtualMode that reads windowed, so it asked for a windowed 2750x2850 where
VR-50 had always passed 1. The engine clamped it to the desktop:

```
res: Reset - the game asked for 1355x1405 windowed=1 (ask 2750x2850 windowed, virtual ON)
res/live: NOT CONFIRMED state=4 requested=2750x2850 capture=1355x1405 after10s
```

Fixed: vsync no longer touches the fullscreen ask, and a windowed ask larger
than the desktop is refused up front with both sizes on the line.

**Fault 2 (ours): the fullscreen checkbox could never stay ticked.** It read the
device, which is windowed by design here, so it snapped back every frame. It now
shows what was ASKED, reports the device separately, and says when VirtualMode
is the reason the two differ.

**Fault 3 (ours): the vsync ON leg never existed.** `UncapPresent` returns at
its first line when `ForceNoVSync` is clear, so clearing the flag only stops
forcing vsync off - it does not turn vsync on. This game asks for
`D3DPRESENT_INTERVAL_IMMEDIATE` itself (`interval=0x80000000` at CreateDevice),
so the "vsync on" leg ran uncapped and would have reported no difference for the
wrong reason. `g_vsyncWant` now forces both directions and logs the interval at
every reset, including when there was nothing to change.

**What DID work.** The engine resize provokes a real device reset every time
(`device Reset (WxH windowed=1)`, five in the run), so the mechanism for making
a vsync change take is sound. The guarded resize path refused nothing and the
capture confirmed each size it was given.

**Next run.** Vsync is now A/B-able at a fixed size and is the cheaper question;
take it first. For fullscreen, the honest test is VirtualMode OFF at a real
display mode (2560x1440) against VirtualMode ON windowed at the same 2560x1440,
so mode is the only variable.

## VR-44: hot-path probes removed after a lag report (2026-09-21, UNMEASURED fix)

**Report.** Build 619 felt noticeably laggier in the headset.

**Measured.** All three runs were at 144 Hz, so the rate is not a variable here. The
figures below are gameplay `perf: tick` samples (runs over 30 ticks/s):

| Build | Ticks/s (mean) | Game time outside our frame path |
|---|---|---|
| 615 | 115.5 | 2.2 ms |
| 618 (power census added) | 94.0 | 3.9 ms |
| 619 (power seams added) | 96.3 | 3.7 ms |

The step is at 618, not 619. Within the 619 run, the game ran at about 127 ticks/s
(1.5 ms) for 30 s with every hook installed. It then fell to 75-90 ticks/s before any
power was used. So the extra cost scales with the scene and is not a flat per-frame
cost. The scenes were not the same across the runs, so this does not prove the cause.

**Suspects, and what was done about each:**
* **The power census.** Its hook on `0x00B515C0`, the camera accessor, has 95 callers,
  and AI code is among them. The hook ran a full `pushfd/pushad/fxsave/fxrstor` on every
  call before filtering for power-code callers. That cost grows with the number of NPCs.
  REMOVED; its result is recorded in ENGINE_NOTES.
* **The trace census (VR-166).** It hooked `execTrace` and three camera-trace helpers,
  which AI and script traces call constantly. It was already on in 615, so it is not
  the step, but it is a standing cost. REMOVED; its job ended with the razor seam.
* **The aim-assist swap (619).** It ran only on casts from UsePower's slot. It is not a
  hot path, but it was also wrong (see ENGINE_NOTES). RETIRED.

**Still on, and why:**
* The spawn census stays, because spawns are rare and it measures the swarm's landing
  point. It is armed by `[Aim] SourceProbe`.
* The power seams run only on a cast. The one exception is Possession's pick, which
  runs every tick, but only while Possession is held.

**Next run.** Compare ticks/s against 615 in the same kind of scene. If the drop
persists with these probes gone, set `SourceProbe=0` as the next A/B.

**Build 620 (headset).** No lag was reported. The log is short (18 gameplay samples):
104.7 ticks/s, with 3.9 ms outside the frame path. The scene differs from 615's, so this
is a report, not a measured A/B. The suspects stay removed.

## VR-204: two diagnostics that ran all session, default off (2026-09-22, UNMEASURED fix)

**Report:** in the fifth VR-204 run, average fps looked right but a few hitches remained.
That build had a median stereo present rate of 237/s, against 236/s for the trace-only build.

**Log census (180 s run):** `camera/source` wrote 8,625 lines (56/s) and `cine/trace` wrote
7,442 (50/s). Together that is about 106 lines/s for the whole session.

* `camera/source` is the VR-165 census (`[Diagnostics] CamModProbe=1`, shipped ON). Every
  500 ms, `CameraSourceTick` calls `BuildLiveSet()`, a full copy and sort of GObjects
  (115,893 slots on this save), on the script lane. It then prints a table of about 28
  lines. VR-165 is closed. A periodic whole-table rebuild on the game thread is a hitch
  candidate by construction.
* `cine/trace` (`[Cine] Trace=1`, shipped ON) samples every 100 ms and prints 5 lines per
  sample. It rebuilt the live set only once in the run (`liveRefresh=1` on 1 of 1,488
  samples), so its cost is the logging.

**Change:** both now ship 0, in code, the ini writer, the golden and packaged ini, and the
tester's installed ini. The installed ini's original is kept beside the fifth run's logs.
Both keys still turn the diagnostics on for an investigation.

**Not measured:** the hitches themselves. The log has no frame-time histogram, so whether
these two were the spikes is a prediction. Prediction: with both off, the next run's
`stereo: beat` minimum rises toward its median, and a hitch at a 500 ms period no longer
appears. If the hitches remain, the next suspects are the other `BuildLiveSet()` callers
(`cinematic_fov`, `cinematic_pitch`, `game_opts`) and the native-profile hooks
(`NativeProfile=1`).

## VR-204: the live-object table as a hash set (2026-09-22, UNMEASURED fix)

**Report:** after the two diagnostics above were turned off, a few hitches remained at
high fps.

**The run's `perf: frame gap` lines** (the ones logged are 40 ms or more, or 2.5 times the
mean present interval):
* Most steady-play gaps sat in `out/idle (waiting for the game thread)`: 40-60 ms, every
  few seconds, several at exactly 33 ms. Six sat in `present-tail (xrEndFrame)`, at 31-42
  ms: the runtime or compositor.
* The two load-time gaps line up with the `uistate` scan: 101 ms at 43719500 against a
  114 ms gap, and 145 ms at 43795593 against a 164 ms gap. That scan runs once per load and
  is left alone.
* Mod work logged near the steady gaps: none that repeats.

**The mod-side periodic cost:** `RefreshLiveSet` is called by the UI surface poll (1 s),
crouch (1 s), rain, the sword trail and the camera shake (2 s). The shared table was
therefore rebuilt about once a second. Each rebuild copies and sorts about 116,000
pointers, about 12 ms by VR-160's measurement, on whichever thread asked. At 144 Hz that
is at least one dropped frame each time. It sits below the gap logger's 40 ms line, so
the log could not show it.

**Change (`ue3/uobject.cpp`):** the table is now an open-addressing hash set with linear
probing, a load factor of at most 0.5 and an integer avalanche hash. A rebuild is one
linear pass with no sort, and `IsLiveObject` takes one or two probes instead of about
seventeen. The answers are the same: membership in the current GObjects array. Every
30 s the rebuild cost is summarised as `live: N rebuild(s) ... mean X ms, max Y ms`.

**Prediction:** `live:` reports a mean well under 3 ms. The steady out/idle gaps at 40 ms
and above are the game's own, so they should mostly remain. What should go away is the
once-a-second single-frame drops that are too small to itemise.


## 2026-09-23: current public refresh recommendation

The project owner directs the launcher and quick start to recommend 120 Hz, or
144 Hz with Virtual Desktop Beta. This supersedes the old 90 Hz onboarding advice
from earlier render builds. It is current product guidance, not a new measured
benchmark in this session; historical 90/120 observations above retain their
original build context. No timing or pacing implementation changes accompany it.

## VR-212 candidate: bounded missing drop-context discovery (2026-09-23)

Discovery again runs at most once per20ms, with a1s pause after an unsuccessful
full sweep. Pawn changes, a growing/replaced object table and invalidated cached
contexts wake discovery. Cached attack decisions are still sampled every anim
tick. Within each1024-slot slice, readability regions and class verdicts are
reused; both caches expire before returning to the engine, so they retain no
identity across a menu or GC. IsLiveObject and owner validation remain.

This fixes concrete avoidable game-thread work. The entire reported hub slowdown
is not yet attributed or headset-confirmed fixed. No quality/settings reduction.

Candidate761-g14728179e built optimized with legacy off, installed with all64705
INI bytes unchanged and CRLF verified. Launcher embeds this DLL. Native schedule
checks,75 swing-core checks,9 exports, default writer/reset parity and lint pass.
No game launch or post-fix headset result yet. The fixed unnecessary work is
source-verified; the claimed FPS recovery remains pending.

## Mirror policy across runtimes (VR-216, 2026-09-24)

Requested policy: default mirror off on SteamVR too, with visible performance
and compatibility guidance. Remove VR-208's runtime override and disabled launcher
checkbox. The performance hint says disabling the mirror can produce a large
boost with any runtime; it is a conditional user-facing recommendation, not new
cross-runtime benchmark evidence. Existing controlled throughput measurements
above remain Quest/VDXR-specific. No new SteamVR throughput or headset startup
measurement was performed. Native D3D9 GPU submission/pixel checks and all existing
fallbacks pass. Retain the earlier Index startup report in DESKTOP_MIRROR.md.

## Anti-aliasing and clarity (2026-09-26)

Branch `claude/antialiasing-clarity`. Built and host-tested, not installed, not headset-run.

**Why a lower FOV looks so much clearer.** A flat (rectilinear) render spends its pixels
by the TANGENT of the angle, not the angle. Centre density = (width / 2) / tan(hfov / 2).
At 2750 px: 108.07 deg gives 17.4 px/deg, 103 deg (the default claim) 19.1, 90 deg 24.0,
75 deg 31.3. The Quest 3 panel is about 25 px/deg at the centre, so at the full FOV and
100% the centre is rendered BELOW the panel and magnified 1.3-1.4x: every stair-step is
enlarged. The 75 deg box is rendered at 1.25x the panel: supersampled. The periphery goes
the other way (sec^2): at 54 deg off-axis a 108 deg render has 2.9x its centre density.

**Why raising the resolution with the FOV at the same rate does not match it.** Matching
the 75 deg box's centre at 108 deg needs tan(54)/tan(37.5) = 1.79x per axis, 3.2x the
pixels, not 1.44x. The 200% cap reached 24.6 px/deg (about the panel, no supersampling);
300% reaches 30 px/deg, with most of the added pixels spent where the lens is blurry.
Three further losses: (1) an image larger than the runtime asked for is minified by the
compositor with about one bilinear tap per output pixel, which reads roughly a third of the
rendered pixels and aliases again (host-measured below); (2) the head never stops moving,
so aliasing is seen as crawl, which spatial supersampling only reduces slowly; (3) the
GPU cost of 300% (about 0.64 ms per megapixel per eye) drops the frame rate and the
reprojection that follows shows as more shimmer.

**Levers built (all default OFF, F10 Advanced > Display > Clarity and anti-aliasing,
`clarity ...` and `aniso ...` on the seam, `[Clarity]` in the ini):**

| Lever | What | Host evidence (`tools/clarity-gpu-host.ps1`, RTX 4070 Ti SUPER) |
|---|---|---|
| Resolve | Above ~100%, filter the render to the runtime's recommended size (Mitchell kernel scaled to the step, linear light); the swapchain becomes that size | Stripes at the render pitch, 1.73x step: pattern sd 0.004 vs 0.289 for one bilinear tap (the control); crawl at a quarter-pixel move 0.006 vs 0.247; mean preserved |
| Temporal (experimental) | Each eye blended with its previous frame, reprojected by the rotation between the two rendered cameras (the pose record's rotator), history clipped to the current 3x3 YCoCg spread; head micro-motion is the jitter | Point-sampled slanted edge under half-pixel head jitter: coverage error 0.219 raw, 0.095 after 48 frames; a still view stays exact; a scene cut shows the new picture at once |
| Sharpen | Contrast-adaptive sharpening (CAS weighting) | Flat areas untouched; edge corners pushed apart (0.2..0.8 -> 0.145..0.855) |
| Anisotropy | Raises MAXANISOTROPY on samplers the game already makes anisotropic (game: 4x) | Counters in the `samplers (10 s)` line; not measurable off the game |
| TrilinearMips | POINT mip filter -> LINEAR on those samplers (the texture groups' default is point mips) | As above |

**Instruments added:** `xr: eye L/R fov ...` (each eye's own angles and how much of the
symmetric render it uses: sizes the off-axis frustum idea below); `device/census:
multisampled surfaces ...` (whether the game's own MSAA, `MaxMultisamples`, engaged);
`clarity:` every 5 s (what ran, why a history restarted); `samplers (10 s)`.

**Not built, ranked:** (1) off-axis per-eye frustum: free density if the eyes' angles are
asymmetric; the new `xr: eye` line gives the gain, then it needs an engine projection change.
(2) The game's own MSAA: try `[SystemSettings] MaxMultisamples=4` once; the census line says
whether it engaged. (3) The game's AA mode: Options > Antialiasing, MLAA vs FXAA vs off, in
one session (the mod rewrites MLAA at the next start). (4) Quad views / foveated inset: two
extra scene passes at the ~5.6 ms per-pass CPU floor; not viable. (5) A non-linear
projection: D3D9 rasterises straight lines; not viable. (6) VD: Godlike, highest bitrate,
its own sharpening.

**The 32-bit ceiling.** The game is large-address-aware (4 GB). This rig at 2750x2850 with
stock textures: about 2 GB process private, largest free address block about 1.2 GB (the
`gpumem` line). An HD texture pack plus a larger render is consistent with a reported freeze
when the resolution is raised: that is address space, not VRAM. The `gpumem` line's
`largest free address range` is the number to ask for.

### Headset verdict (2026-09-26, 200% at 144 Hz with SSW, VDXR)

- Supersampling itself (200-300%) is the large visible gain. The resolve on top of it was
  judged no better, or slightly softer in the distance, with both kernels (Mitchell, then
  Catmull-Rom). VDXR's own downscale is evidently good enough that the host-measured
  sparse-sampling aliasing does not show through the stream. Resolve now ships OFF.
- Each resolve toggle changes the swapchain size and rebuilds it; VRAM rose ~450 MB over six
  toggles in one session (3029 -> 3478 MB). A report of lasting lag after many toggles fits
  that; not reproduced in the measured session (tick 14-16 ms throughout).
- 16x anisotropy and trilinear mips run (about 40000 sampler binds a second raised) but the
  visible gain over the game's 4x is small; kept on as nearly free.
- Temporal: no visible effect at a new-frame weight of 0.50 (the slider's maximum, ~2 frames
  of history); at 0.15 it smoothed shimmer but smeared while walking until the motion
  weighting; with it, walking no longer smears. Still experimental, off by default.

### Motion-vector calibration and TAA candidate (2026-09-26)

Branch `claude/motion-vectors`, not merged. Plan and depth transport evidence remain in
PLAN-motion-vectors-dlss.md; engine coordinate provenance is in ENGINE_NOTES.

- MIRROR TEST, installed DLL hash `143c21a3900b637dd1b84efe1148009a2bc1f24d2da5bbf4e7c4ce6ff8cd8016`,
  banner built 16:16:52: 56 pure turns, normal luminance error 0.0082, mirrored 0.0468.
  This rejects the horizontal-mirror hypothesis; clarity's rotation was already correct.
- Corrected translation, installed candidate hash prefix `1f1536d781add944`, built 16:39:14:
  70 moving frame pairs; error by scale 25:0.0593, 50:0.0406, 100:0.0278, 200:0.0226,
  400:0.0254, 700:0.0327, 1000:0.0341, 1500:0.0355, 2500:0.0373, 4000:0.0383,
  7000:0.0389, rotation-only:0.0400. 200 wins 65/70 pairs, 43.5% below rotation-only.
  Coarse minimum only: 200 is the experimental default, not a proof of the exact depth unit.
  Initial load transients are included in this aggregate; repeated controlled lateral steps
  dominate it. No missing matching-depth lookup in that run.
- Cause: `camera::last_written_pos` multiplies the written field by c5Sign, publishing
  c5 = negative world position. Its header and pose-record comment incorrectly called it world
  position. Convert only in clarity's `view_for`; do not change shared pose transport or yaw.
- Built per-eye RGBA16F vectors: xy previous-minus-current UV, z validity, w reserved.
  Current depth and colour must have the same capture serial and dimensions. Vectors run at
  TAA output size, including when resolve is enabled. Invalid depth/behind/outside rejects
  history; sky keeps rotation. No object vectors or disocclusion-depth history yet; colour
  clipping remains, so moving NPCs/hands can still trail. Motion weighting is bypassed only
  with matching depth; the rotation-only fallback retains it.
- First normal-path smoke failed usefully: zero vector completions, all rotation fallback.
  Present's caller still gated depth copying on Diagnostics.DepthShare. The service gate now
  includes active Temporal + MotionVectors, and services release after the last consumer
  switches off. Diagnostic readbacks remain gated by DepthShare. A success counter is updated
  only after the GPU chain completes, not when its inputs are merely available.
- Final installed hash `5469cd53f7b674c9247a9047f11be736d4db2d36355358929349448194ceb661`,
  banner built 16:54:27: both diagnostics off at startup, gameplay combined head translations
  and rotations, about 450 successful vector-TAA passes per eye per 5 s, zero fallback.
  Live off/on released/recreated the depth ring and resumed at 354/354 passes in the partial
  window, zero fallback. This proves execution/transport, NOT headset quality or a GPU speedup.
- Host GPU: 54 clarity checks pass. Independent plane geometry predicts 4 px, measured worst
  vector error 0.0010 px. TAA error 0.00085 vs rotation-only 0.09700 and reversed-translation
  control 0.13496. Eye isolation, output resizing, invalid/NaN depth, behind-camera rejection,
  sky rotation and freeing resources pass. Existing 6 calibration GPU checks pass, including
  independent ray-cast correspondence and a flat curve without translation.

`[Clarity] MotionVectors=0` default; F10 under Temporal AA, `clarity motion on|off` live.
`MotionDepthScale=200` is an experimental calibration parameter (`clarity depthscale 25..7000`).
TAA itself remains off by default. No diagnostic needed for the feature. Full original
installed INI restored byte-for-byte after the simulator test, including both diagnostics off.

Cost is still OPEN for a headset: this adds the existing 3-slot full-size RGBA16F depth ring
and two output-size RGBA16F vector textures plus a pass, roughly 5 * width * height * 8 bytes
(~299 MiB at 2750x2850, no resolve), before D3D interop/driver duplication. Do not equate a
paced simulator's frame rate with the feature's GPU cost. Next: one walking/leaning headset
A/B with Temporal held on, vector option toggled, panel closed; judge trails and edge stability.
Local archives (gitignored): `build/mv-session/mirror-normal-wins.log`,
`translation-corrected.log`, `vector-missing-depth-gate.log`, and `final-vector-run/`.


## 2026-09-26: full TAA audit (source 87a892cef, no runtime changes)

Scope: the optional resolve -> temporal -> sharpen chain, per-eye history, pose/FOV
provenance, capture/depth matching, shared-resource synchronization, controls, reset/failure
recovery, memory and GPU cost. Both rotation-only TAA and experimental depth-vector TAA
were examined. No game was launched and no installed DLL or INI was changed for this audit.
This supplements, rather than invalidates, the earlier simulator geometry measurements.
Those runs establish a useful correspondence improvement in that scene, not production readiness.

### Evidence and reproducibility

Run `tools/taa-audit-host.ps1`: x86 native host, production `clarity_gpu.cpp`, no game.
It includes the existing 54 checks and 8 audit characterizations: 62 checks, zero failures.
Several new checks deliberately PASS when a known limitation is reproduced. They are
characterization tests, not assertions that the current output is desirable. Convert their
expectations when fixing the respective defect. Local outputs: `build/taa-audit/results.txt` and `results-final.txt` in that directory.

- Stationary isolated white pixel on black: first frame 1.000 gamma; after 40 frames,
  0.729412 gamma / 0.491021 linear. Its position and the input never changed. The temporal
  variance box excludes the current bright sample, so even perfectly corresponding history
  gets clipped. This is a reproducible loss of fine bright detail with Temporal enabled,
  independent of the new vector option. Default sharpening cannot restore lost energy.
- Static slanted edge: coverage error 0.212219 both raw and after 40 identical frames.
  No new sample positions means no static supersampling. Head micro-motion remains the
  source of sample diversity; there is no deliberate projection jitter.
- A one-pixel shift of a 0.2/0.8 checker pattern, with zero camera motion and valid depth:
  the new dark pixel is 0.705882 and the new bright pixel is 0.384314. Mean gamma error
  0.460784. Invalidating history returns the correct picture within 2/255. This isolates
  unmodelled object/image motion: neighbourhood clipping does not mean 'show current frame'.
- One missing-depth frame after both eyes had vectors: intermediate bytes drop from
  1,228,800 to 819,200 at 160x160. Both vector textures are discarded, not just the
  unavailable eye's input. Subsequent vector frames allocate them again.
- Existing positive controls still pass: independent plane geometry predicts four pixels,
  vector error about 0.001 pixel, translated TAA error 0.00085 versus rotation-only 0.09700;
  turn direction, eye isolation, invalid/NaN depth, sky, output resizing, snap-turn/Blink
  rejection, gamma round-trip, resolve and sharpen all pass their existing cases.

### Prioritized findings

**P1 - Thin stationary detail loses brightness.** `clarity_gpu.cpp:195-199`: the
mu +/- sigma box does not include the current sample by construction. One bright sample
among eight black neighbours gives a maximum near 0.425 in linear luminance, even when
history is exactly the current image. The blend then converges below the input. Repair
history validation so an exactly matching sample is preserved; evaluate centre-inclusive
bounds or confidence-dependent clipping against BOTH thin-detail and moving-pattern tests.
Simply widening all bounds trades this loss for more trails. Add saturated RGB and line
patterns before accepting a change. This affects both TAA modes.

**P1 - Depth reuse has no independent consumer-completion contract.**
`depth_probe.cpp:307-326,368-384`: the producer cycles three slots and only waits for the
D3D9 copy before exposing an SRV. It never fences the D3D11 read before reusing a slot.
Normal shared-colour delivery can indirectly protect this through its two-slot read fence;
that does not establish safety for every path. Deferred capture has no such shared-colour
read fence, and `capture.cpp:427-440` proceeds after a 10 ms read timeout. A queued D3D11
read can therefore overlap a later D3D9 overwrite on those paths. This is a source-level
synchronization defect, NOT a corruption reproduced by this audit's standalone host.
Give depth slots explicit read-done/read-wait ownership and refuse reuse on timeout. Also
check the HRESULT of `Issue(D3DISSUE_END)` before publishing the slot. The broader colour
capture timeout behavior deserves its own fix; a timeout is not evidence of completion.

**P1 - The camera record is not guaranteed to carry the rendered eye's position.**
`scene_draw.cpp:393-410`, `pose_record.cpp:172-185`, `clarity.cpp:61-78`:
pass 2 writes the right eye's camera offset, but ordinary gameplay does not republish
that position into the camera record. The three republish helpers are conditional cinematic/
menu scopes. `pose::open` copies the last globally published camera; the eye tag separately
carries the actual `wrotePos`. TAA uses the copied camera position, ignoring the tag position.
A constant offset cancels during pure translation, which helps explain why that test can
pass; head rotation changes the eye offset and exposes the distinction. Capture the actual
per-eye view position alongside the rendered colour/depth, preserving the rotation sample's
existing provenance. Do not silently replace the shared tracking record with live globals.
Visual magnitude and the effect on the coarse scale calibration still need measurement.

**P2 - Stale history can survive a reset or a long capture gap.**
`reentry.cpp:517,659`, `clarity_math.h:116-145`: a non-fresh grab skips clarity after the
first output; D3D9 reset clears capture but not clarity. The history guard checks dimensions,
FOV and pose jumps, but has no scene epoch, delivered-frame age or camera identity. A same-size
reset/cut near the same pose can accept pre-transition history. Untagged *fresh* images and
explicit option changes do invalidate correctly. Add lifecycle invalidation and a bound on
same-eye frame age; test same-pose textured cuts, not only the existing flat-white cut.

**P2 - Speculative depth serials can select an older present.** `depth_probe.cpp:324,370`:
every present assigns `capture::serial()+1`, but `capture.cpp:583-590` returns with capture
off without incrementing it. If serial S is paused for several presents, slots 0/1/2 can all
hold S+1. On resume, a fresh slot 0 still loses the lookup to the older slot 2 because lookup
takes the last matching array entry. This is a deterministic consequence of the bookkeeping,
not an observed gameplay incident. Assign a unique render/copy identity and commit its
association after a successful grab, or explicitly invalidate superseded duplicate serials.

**P2 - Missing depth/reset causes avoidable allocation churn.** `clarity_gpu.cpp:386`:
`!useMotion` releases both vector targets. Clarity supplies depth only when history is valid,
so ordinary history resets trigger this as well as real depth misses. Keep appropriately
sized targets across transient fallbacks; free them on feature disable, resize or shutdown.
Separate 'feature enabled' from 'this frame has usable depth'. The old test calling this
release a success verifies behavior, not whether that behavior is efficient.

**P2 - Failure latches do not recover through expected lifecycle paths.**
`depth_probe.cpp:251-274,302`: allocation failure sets `g_shareFailed`; reset does not clear
it, nor do Temporal/MotionVectors toggles. Only the diagnostic `set_share` clears it.
`clarity_gpu.cpp:229,326-336` similarly keeps `failed_` through shutdown, although the outer
clarity wrapper resets its init-attempt flags. Add bounded, explicit recovery on device/
resource reset or a deliberate feature retry; do not retry allocations every frame under
memory pressure. `LoadLibraryA` references in clarity/calibration initialization also lack
matching `FreeLibrary` calls, a smaller repeated-lifecycle leak.

**P2 - Projection is read live rather than paired to the image.** `clarity.cpp:66-72`
reads the current global rendered HFOV while consuming a possibly deferred colour capture.
The shaders use the same tanH/tanV for current unprojection and previous projection. The
0.2% history reset threshold catches larger changes only when those sampled values change;
it cannot prove either projection belongs to its image. Stamp projection/FOV and dimensions
with each eye capture, and use separate previous/current projection parameters. No wrong-FOV
frame was reproduced here; this is a provenance gap during FOV/zoom changes.

**P2 quality limitation - Camera vectors are not object vectors or visibility tests.**
`clarity_gpu.cpp:159-200`, `clarity.cpp:394`: current depth reconstructs static-world camera
motion only. There is no previous depth, disocclusion test, moving-object velocity or reactive
mask. Valid depth also disables the old camera-motion reduction of history globally, including
unreliable pixels. The checker test demonstrates the remaining trail class. First add depth
history and a disocclusion/confidence test, plus conservative colour-change responsiveness;
then investigate object vectors. Depth rejection alone cannot solve a moving surface at
unchanged depth. The mod's separately rendered hand overlay and F10 panel are already drawn
AFTER TAA; native game weapons, particles and surviving scene HUD are the relevant risks.

### Performance and quality improvements, ordered by payoff

1. Fix resource ownership and per-eye view provenance before tuning blend or depth scale.
   Keep the experimental option default off until these and transition tests pass.
2. Fix stationary-detail attenuation and add per-pixel history confidence. Keep antialiasing
   strength separate from rejection of unreliable history; a stronger global blend is not
   a substitute for visibility information. A frame-time-aware blend is worth an A/B across
   refresh rates, since the current fixed coefficient changes response time with Hz.
3. Fuse camera reprojection into the temporal shader when no external vector consumer needs
   a texture. Today only temporal consumes it in production. This can remove the vector pass
   and both vector render targets. If materialized vectors are needed for future DLSS, retain
   that path as a separate consumer requirement. At minimum use one transient vector target;
   RG16F with a defined invalid encoding is another option, subject to precision tests.
4. Add explicit GPU/CPU timing around depth copy and its fence wait. `frame_hooks.cpp:170`
   starts depth work before `perf::kEntry` and before the conversion scope. Existing clarity
   conversion timings therefore do not attribute the full interop cost. `depth_srv_for` can
   poll with FLUSH for up to 20 ms, longer than a 90 Hz frame. Prefer a completed matching
   slot or a controlled fallback; measure fence wait percentiles and fallback frequency.
5. Depth edges currently use a nearest depth sample even when colour has been resolved over
   a wider footprint. Compare nearest-foreground depth/dilation against silhouette reference
   scenes; indiscriminate dilation can drag backgrounds. Previous-depth validation should be
   in place before selecting a policy.
6. Deliberate, stereo-consistent projection jitter could improve a motionless view, but this
   is an engine/projection change with culling, reprojection and HUD consequences. The current
   no-jitter choice is an intentional limitation, not a sign error. Do it after correctness
   and visibility, with paired eye samples and explicit jitter subtraction.
7. Profile gamma decoding and history sampling before simplifying them. The temporal shader
   fetches the centre separately from its 3x3 neighbourhood; reusing the centre may save work
   if the compiler has not done so. Nine bilinear Catmull-Rom history taps are justified by
   moving-edge quality; reducing them needs a negative control, not just a lower instruction
   count. The reciprocal-luminance blend can bias changing brightness and merits a coloured/
   HDR-content test; this audit does not classify that deliberate weighting as a defect.

### Measured pass cost and memory

Hardware host: RTX 4070 Ti SUPER; production shaders and RGBA16F source depth. At
2750x2850, one eye, median of 24 GPU timestamp samples after 12 warmups in each mode.
The final rerun also verifies the stationary-detail test with default sharpening enabled:

| Chain | First recorded run, ms | Final rerun, ms |
|---|---:|---:|
| Sharpen 0.4 only | 0.5161 | 0.5161 |
| Rotation TAA + sharpen | 2.2897 | 1.6404 |
| Vector TAA + sharpen | 2.8498 | 2.7822 |

These are synthetic flat-image shader timings, not a headset frame-budget prediction.
They exclude uploads, CPU readback, D3D9 depth copy/fences, the game and compositor. GPU
clocks, cache behavior and workload affect the result. The extra vector path measured about
0.56-1.14 ms per eye across these two runs; the variation rules out a precise cost claim.
Shader fusion must be measured rather than assumed to save all of that difference.
An initial exploratory run used RGBA32F test depth and is deliberately excluded from this table.

Without resolve, four RGBA16F histories cost 239.2 MiB and two vector targets 119.6 MiB at
2750x2850. The three shared RGBA16F depth slots cost another 179.4 MiB: **538.2 MiB total**
for these nine textures, before colour capture, eye outputs, driver overhead or other game
allocations. At twice that pixel count this is about 1.05 GiB. These are texture-storage
estimates, not a claim that all bytes are committed to CPU address space. Still material in
this 32-bit title. Resolve lowers output history/vector sizes, but retains full-size depth
and adds its own intermediates. Track both clarity and depth allocations in one budget.

### Remaining acceptance gates and sources

No runtime fixes were installed in this audit. Next implementation tests should cover
same-pose textured cuts, capture off/resume with duplicate depth serials, reset and allocation
failure recovery, deferred/shared capture under delayed GPU completion, per-eye rendered
view provenance during head turns, coloured detail, disocclusion and native moving objects.
Then do a headset A/B for static detail, walking/leaning trails and frame-time percentiles.
Avoid interpreting simulator pacing or vector-completion counters as image-quality evidence.
Depth scale 200 remains a scene-specific coarse minimum: calibration candidates can reject
different pixel populations near image borders, and the discovered pose-provenance issue
must be resolved before deriving exact engine units or hardening the sky cutoff of 1000.

Primary background references (recommendations above are engineering inferences applied to
this code, not benchmark claims from these sources):

- [Yang, Liu and Salvi, Eurographics 2020 TAA survey slides](https://www.leiy.cc/publications/TAA/TAA_EG2020_Talk.pdf):
  sampling jitter, reprojection, validation and accumulation are separate components;
  static-camera vectors do not describe animated objects. Supports the staged quality plan.
- [Microsoft OpenSharedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-opensharedresource):
  D3D9/11 sharing restrictions and submitting updates across devices. Resource sharing does
  not itself provide the per-slot ownership policy this depth ring needs.
- [Microsoft D3D9 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries):
  issued/signaled query states; a polling timeout must not be mistaken for completion.


## 2026-09-26: TAA audit fixes implemented and simulator-tested

This supersedes the open implementation defects in the preceding audit. Branch remains
`claude/motion-vectors`, unmerged. The installed candidate is optimized x86, legacy off:
SHA256 `f25fc06e5a6d2f07d241cd071d84c4ea87b9f21b4e25372a8d289d8fed75d32b`,
banner `v1.0.1-93-gf0ef210dd-dirty`, built Sep 26 2026 17:47:48.

### Changes and disposition

| Audit item | Implemented behavior |
|---|---|
| Stationary bright-detail loss | Variance bounds include the current centre sample. White and saturated RGB point tests preserve intensity and hue. |
| Stale colour on changed surfaces | Near-zero camera displacement enables a conservative colour-change response. Actual subpixel motion retains accumulation. This is not animated-object velocity. |
| Visibility/disocclusion | History alpha now holds the corresponding current depth. Fused TAA compares previous depth against the reconstructed previous-view depth before using colour history. Invalid/missing depth and sky/surface transitions reject reuse. |
| Per-eye position | Draw records carry the same explicit c5 position passed to that eye's tag. Tracking publication remains separate; rotation provenance is unchanged. |
| FOV association | Each draw record captures its scoped FOV (or camera sensor fallback). Current and previous projection tangents are separate shader inputs; large FOV changes still reset history. |
| Reset/capture gap/scene changes | Missing grabs invalidate history; D3D9 reset shuts down clarity resources. Record age, camera identity, level-load/UI epoch and finite camera values guard reuse. No engine-memory writer was added. |
| Depth overwrite hazard | Every shared-depth slot has its own D3D11 event query, ended/flushed after its last use. Busy/error slots are not reused. D3D9 producer query Issue failures do not publish a serial. |
| Duplicate serials | Every new copy opportunity invalidates previous entries for the pending capture serial, even when no slot is available. Capture off/resume cannot choose an older duplicate. |
| Transient vector allocations | Production TAA reconstructs directly in the temporal shader; it needs no vector texture or separate motion pass. Optional materialized vectors remain for independent geometry tests/future consumers and survive transient missing-depth frames. |
| Failure recovery | Explicit setting epochs retry failed clarity initialization; depth allocation refusal recovers after reset/disable or an explicit lever change. Shader compiler DLL references use scoped ownership. |
| Colour capture timeout | Existing D3D9 producer and D3D11 consumer fences now refuse the grab on timeout/error instead of proceeding with unsafe data. Pending read queries stay pending until completion. |
| Refresh-rate response | The base temporal blend is normalized to elapsed same-eye time with the existing value interpreted at 90 Hz. |
| Cost attribution | The Present CPU budget begins before depth work; `hk.depthCopy` labels submission. Asynchronous D3D9 timestamps report the copy bracket under `perf/depth`. Runtime depth availability and memory are reported without diagnostic readbacks. |

History alpha reuses existing storage: no additional visibility texture, copy pass or
synchronization boundary. Colour Catmull-Rom samples RGB only. Depth validation uses point
samples to avoid blending unrelated surfaces' depths. Resolve still needs careful silhouette
A/B: nearest-foreground dilation was evaluated conceptually but not enabled indiscriminately,
since spreading foreground depth without object velocities can drag background history.

### Reproducible evidence

`tools/taa-audit-host.ps1`: **72 checks, zero failures**, including the original geometry,
resolve, sharpen and moving-edge controls. The audit's reproduced-defect assertions were
changed to desired-output regressions; the no-jitter static-edge case remains a limitation
characterization. `tools/motion-gpu-host.ps1`: 6 checks, zero failures. `frame_test.exe`:
146 PASS lines, zero FAIL, successful exit. Default writer/profile/golden byte parity,
configuration persistence/failure tests, nine exports and lint all pass.

- White stationary detail: old accumulated linear intensity 0.491021 -> **1.000000**.
  Default sharpening enabled. Independent red/green/blue point channels also survive.
- Changed stationary-camera checker: old mean gamma error 0.460784 -> **0.000000**.
  Both old and current images have valid depth, so this tests colour response rather than
  merely rejecting an unseeded depth history.
- Disocclusion under camera translation: colour error **0.000981**, against **0.015416**
  when the control supplies a matching previous depth. The camera moves, so quiet-camera
  colour response cannot explain the successful rejection.
- Micro-motion edge control retains the original improvement: coverage error **0.095**
  versus raw **0.219**. An initial response threshold extending to 0.5 pixel motion raised
  this error to 0.197 and FAILED the existing test. Restricting that response to effectively
  stationary reprojection restores the antialiasing benefit. Do not broaden it blindly.
- FOV regression checks a known non-identity current/previous projection mapping. Tests
  also reject old frames, replaced cameras, same-camera scene epochs and nonfinite poses.
- Depth fallback holds the same four history allocations: 819,200 bytes before and after
  at 160x160, no production vectors. Shader shutdown/reinitialization is tested.

At 2750x2850, four histories remain **239.2 MiB**, the depth ring **179.4 MiB**. The two
119.6 MiB vector textures are gone: total **418.6 MiB**, down from **538.2 MiB** before other
rendering resources/driver overhead. This removes about 22% of the audited texture storage.
Do not confuse texture bytes with CPU virtual-address commitment.

A recorded standalone GPU run on the RTX 4070 Ti SUPER: sharpen 0.4178 ms, rotation TAA + sharpen
1.8749 ms, fused depth TAA + sharpen 2.2958 ms per eye at 2750x2850. Earlier fixed runs were
0.50-0.53 / 2.18-2.24 / 2.75-2.79 ms. As before: flat synthetic image, 24 timestamp samples
after 12 warmups, excludes D3D9 transport/game/compositor. Clocks and scheduling vary enough
that this is NOT a proven headset speedup. The final rerun measured 0.5724 / 2.8129 /
3.6577 ms, reinforcing that limit on comparisons. Fusion removes work/storage, while visibility and
colour response add work; measure frame-time distributions on the target headset.

### Simulator integration and installed state

User-authorized simulator run used xrsim-launch ViaSteam/NoInstall/Release and boot Attach.
The boot helper again reported gameplay before the loading screen's final key prompt.
A simulator compositor capture identified the prompt; after Space, actual both-eye gameplay
counters appeared. The native computer-use helper could not initialize; the repo's simulator
capture/input harness completed the test. No screenshots are committed.

Matching banner verified before interpreting the log. Tested repeated +/-3 degree turns,
combined +/-3 cm translation and turns, capture off/shared, shared-slot reinit, deferred
capture, return to shared, and MotionVectors off/on. After recovery: **450 completed fused
TAA passes per eye per 5 s**, zero ongoing fallback, zero clarity refusals. Two depth
unavailability events appeared across mode transitions and did not persist. No clarity/depth
or capture error lines. This verifies execution/recovery, not a forced GPU-hang scenario or
headset appearance. The new fences also cover deferred capture independently of colour.

The new D3D9 timing bracket recorded 35,485 resolved samples, cumulative mean 0.463 ms and
peak 10.167 ms, including menu/loading/gameplay and mode transitions. Do not interpret that
mixed-run peak as a steady-state depth-copy percentile. Query results were never waited on.

Simulator stopped. Entire installed INI restored byte-for-byte to the pre-install backup,
CRLF verified: Temporal=0, DepthShare=0, MotionCalib=0; MotionVectors defaults off. Temporary
MotionVectors/MotionDepthScale entries were removed by the exact restoration. Installed DLL
remains the tested candidate. Archives: `build/taa-fixes/before-candidate/`, `simulator-run/`,
`tests-final.txt`, `frame-tests-final.txt`, and build logs (all gitignored).

### Investigated improvements not enabled as unverified fixes

Deliberate projection jitter remains OFF/unimplemented. The old `IsMainScenePass` hook allows
unknown render targets and expects aspect 1.4..2.4, while this eye render is 2750x2850. The
render instrumentation also records many c0 uploads per view, not one uniquely identified
projection. Reusing that hook could jitter only some geometry or non-scene passes. A correct
implementation first needs complete world-pass/projection ownership, coherent eye sample
phases, jitter subtraction in reprojection, and culling/HUD validation. The static-edge test
correctly still shows no supersampling from identical frames. This was an investigation
recommendation in the audit, not a safe local shader fix.

Animated-object vectors, transparent/reactive material classification and exact engine depth
units remain research. Colour response and depth rejection reduce their consequences but
cannot reconstruct unobserved object motion, especially during simultaneous camera motion.
The 200 uu/unit value and sky cutoff remain experimental. Next human acceptance question:
with Temporal enabled and the panel closed, does the revised depth mode preserve fine detail
and reduce walking/leaning trails versus rotation-only? Remaining trails need scene-specific
evidence; they are not proof that the synchronization or sign convention regressed.


Final review corrected the time-normalization ordering: apply camera-motion response first,
then normalize to elapsed same-eye time, so low frame rates never lower the requested response.
An additional regression verifies equivalent decay at 45 and 90 Hz (72 total GPU checks).
The full transition run above used candidate SHA256
`4d3f4096cbe24ccbfabc05b5da394c9d18ac35ea97f1efda1c0cf408183e705b`; the final installed
SHA256 is `f25fc06e5a6d2f07d241cd071d84c4ea87b9f21b4e25372a8d289d8fed75d32b`.
The latter differs only in blend time normalization. Its incremental build retains the
17:47:48 banner timestamp, so the archived DLL hash and fresh process/log identify the run.
An initial final-build smoke stopped at the loading prompt (zero TAA counters); it is NOT
counted as an integration pass. `final-smoke/` records that failed boot, not a TAA defect.

Final repeat reached gameplay after the compositor capture showed the completed-loading
prompt. Both-eye fused TAA completed L=449 / R=450 in the last report, fallback=0.
Final DLL hash verified against the installed file before interpreting this fresh log.
Evidence is in `build/taa-fixes/final-passed/`; simulator stopped and the full original
INI restored again. No merge.

## 2026-09-26: FSR implementation plan and depth-foundation handoff

### Decision and evidence boundary

The tester reports no perceptible benefit from the revised TAA. Preserve that negative
quality result; the synthetic improvements and simulator execution do not establish a
headset benefit. This planning session did not replay or independently verify the exact
settings of that comparison. Stop tuning the custom TAA as the next task.

Keep `claude/motion-vectors` unmerged as the depth/camera-motion foundation. Retain its TAA
code/tests as a baseline and regression harness; do not rewrite its history or rename it.
Create `codex/fsr-implementation` directly from the foundation's planning commit, in
`C:\dev\Dishonored-VR\build\worktrees\fsr`. The user's requested stacked branch overrides
the usual branch-from-staging rule. No merge to staging or VR-Main is authorized. A future
review should make the dependency explicit; do not merge the parent just to simplify it.

Goal: an optional temporal FSR upscaler with a visible quality benefit or a measured net
frame-time benefit at acceptable headset quality. First target: FSR 2.2.1 with AMD's DX11
backend patch, in-process on the existing x86 D3D11 device. This is a compatibility target,
not a claim that it is the newest FSR. FSR 1 spatial scaling is not an equivalent substitute.
Frame generation, DLSS, a Vulkan translation layer and a helper-process architecture are
outside the first implementation. If the proposed backend cannot pass gate 1, record the
specific failure before changing architecture; do not silently relabel a custom filter FSR.

### Primary sources and compatibility gate

AMD documents a DX11 backend patch against FSR 2.2.1 in its Unity integration. Reuse the
backend, not Unity's renderer/plugin. That article establishes a DX11 route, not working
Win32 support in this game. Preserve upstream licenses and pin both upstream revisions.

- [AMD DX11 integration and patch links](https://gpuopen.com/learn/fsr2-for-unity-urp-dx11/)
- [AMD DX11 backend patch](https://github.com/GPUOpen-Effects/FidelityFX-FSR2-Unity-URP/blob/main/src/patch/0001-fsr-2.2-dx11-backend.patch)
- [Pinned-version integration reference](https://github.com/GPUOpen-Effects/FidelityFX-FSR2/blob/v2.2.1/README.md)
- [Current SDK integration reference, compare deliberately rather than mixing versions](https://gpuopen.com/manuals/fidelityfx_sdk2/techniques/super-resolution-temporal/)

The reference integration requires jittered scene rendering and matching depth/motion data;
FSR replaces the existing temporal AA. Quality/Balanced/Performance use per-axis divisors
1.5/1.7/2.0. These are not the mod's total-pixel percentages. Consult the pinned version for
resource formats, depth conventions, jitter units, exposure, reset and mask contracts.

### Existing foundation and constraints

Runtime source baseline: `1d2ee24a5` (TAA audit fixes); preceding calibration `87a892cef`.
- D3D9 scene colour alpha contains measured view-depth-like data. Shared RGBA16F depth
  slots are keyed to colour grab serials, with separate producer/consumer fences.
- c5 records negative world position; `clarity::view_for` converts once. Normal image axes
  won the mirror test. Never flip the yaw convention again without new contrary evidence.
- Scale 200 uu/depth-unit is an empirical minimum, not an exact derivation. Sky cutoff is
  also experimental. FSR needs a proved depth conversion, not this number used as truth.
- Pose records carry eye, position, rendered FOV, camera identity and level/UI epoch.
- Production TAA now fuses reprojection. Explicit vector output remains a diagnostic path;
  FSR will need a dedicated vector texture and correct resource lifetime again.
- Animated-object vectors, reliable reactive masks and deliberate projection jitter are
  missing. Camera vectors alone are a static-world prototype, not a complete integration.
- Captured colour is currently gamma-encoded with game post effects. Prove the selected
  input's colour space and scene/HUD ownership; do not assume it is pre-tonemap HDR.
- `IsMainScenePass` is not a safe jitter classifier: unknown targets pass, landscape aspect
  assumptions conflict with portrait eye buffers, and many c0 uploads occur per view.
- Keep D3D9 rendering, the D3D11 shared-device path, OpenXR pacing, per-eye publication and
  later hand/HUD composition intact. No additional frame queue or CPU texture readback.

### Gate 0: reproducible baseline and branch setup

Read CLAUDE.md, newest three STATUS sections, NEXT_SESSION, this section, and the latest
FLICKER_REFERENCE entries. Read HANDOFF-GINGASVR Traps/Dead ends before runtime edits.
Inspect current installed DLL/INI/log rather than relying on old handoff settings. Archive
DLL, entire INI and rotated logs before any installation or authorized relaunch. Verify log
banner and installed hash before interpreting a playtest. No game launch in this plan task.

Current DLL hash: `f25fc06e5a6d2f07d241cd071d84c4ea87b9f21b4e25372a8d289d8fed75d32b`.
Banner recorded by the previous run: `v1.0.1-93-gf0ef210dd-dirty`, Sep 26 17:47:48.
Read-only installed INI check during this handoff: Temporal=0, TemporalBlend=0.12,
Sharpen=0.40, MotionVectors=1, DepthShare=0, MotionCalib=0. These differ from the prior
restore because preferences were changed afterwards. Preserve them; this task changes no
installed files. Diagnostics remain off. MotionVectors=1 alone does not enable TAA.

Linear search for FSR returned no match. Prior work records the workspace issue limit;
no new ticket exists and no ticket number is invented. Recheck ticket availability when
implementation begins. Credit commits to BioVRDev, no trailers, no subagents.

### Gate 1: prove the x86 DX11 backend outside the game

1. Pin FSR 2.2.1 and an exact revision/hash of the AMD DX11 patch. Audit its license and
   dependencies; vendor only required source/headers/shaders with license notices and a
   reproducible patch/build record. Do not bring Unity into the runtime.
2. Build the host API and DX11 backend for Win32. Audit pointer/size casts, alignment,
   hardcoded x64 assumptions, allocation sizes and compiler flags. Build-time tools may
   be 64-bit, but every library loaded by the game must be x86. Identify the actual shader
   model and UAV/format/feature-level requirements from the patched sources; do not assume
   stock SDK shader binaries are valid for DX11. Use supported FP32 shaders first.
3. Add an isolated x86 D3D11 host test with two independent FSR contexts, deterministic
   synthetic colour/depth/motion/jitter, reference images and reset/resize tests. Check
   debug-layer errors, finite output, correct two-eye isolation, shader creation and leaks.
4. Measure context/intermediate allocation sizes at actual eye output dimensions. Prove
   shader/resource creation on this GPU, not only a successful static-library compile.

Exit: reproducible Win32 build and a real two-context GPU dispatch with correct synthetic
output. If unsupported, document the failing API/assumption and an alternative design's
cost; do not spend a headset run debugging backend build support.

### Gate 2: separate render size from headset output size

Design a small `core/gfx/fsr` owner and testable GPU adapter. The game renders at the input
size; reconstructed output and OpenXR swapchain remain at a fixed selected display size.
Wire explicit input/output dimensions through capture, clarity/output_size and eye submission
without changing FOV, world scale or HUD placement. Avoid reducing both sizes together.

First implement a bypass at the same dimensions and verify it reaches the submitted eye
image. Then allow Quality input at output/1.5 on each axis. For output 2750x2850 the input
is approximately 1833x1900, about 44.4% of output pixels. Existing F10 resolution has a 50%
total-pixel floor and a fixed 2750x2850 reference, so it cannot express this directly as-is.
Use an explicit FSR mode contract; log actual dimensions and rounding/alignment decisions.
Do not promise savings from reducing a buffer that the engine is not actually rendering.

Exit: a held output size, verified smaller scene render/depth sizes, unchanged stereo/FOV,
and one safely reversible resolution transaction. Preserve and restore the user's previous
resolution on disable; invalid configurations fail to the plain blit with a logged reason.

### Gate 3: prove stereo-consistent projection jitter

Identify all relevant world projection uploads by observed pass/target identity at both
input sizes. Record coverage for opaque, transparent and native weapon passes, plus negative
controls for shadow maps, reflection captures, menus and HUD. Derive a clip-space translation
from the real projection layout. Do not implement head-rotation jitter as a shortcut.

Store applied jitter with each image's pose/serial. Advance phase by successfully rendered
stereo pairs; both eyes of a pair use a coherent phase. Do not advance on a repeated capture
or compositor-only submission. Account for phase gaps and resets. Jitter affects colour and
depth together; it must not move the physical pose submitted to OpenXR. Validate culling
edges, viewmodel alignment, overlays, portrait aspect and post-load replacement cameras.
Any engine-object writer needs IsLiveObject against a current table and revalidation after
menus. All addresses/offsets belong in patterns.h with derivation in ENGINE_NOTES.

Exit: synthetic projection tests and simulator images demonstrate intended subpixel shifts
only on the intended scene, with accurate recorded offsets. Block temporal quality claims
if world-pass coverage cannot be proved. Never enable an unverified broad c0 patch.

### Gate 4: supply inputs with explicit units and provenance

Introduce a per-eye input bundle: colour/depth SRVs, render/display sizes, eye/serial/epoch,
current and previous projection/view, applied jitter, elapsed same-eye time and reset reason.
Use matching capture/depth serials only. Dispatch once per new eye image, not per xrEndFrame.

- Derive the engine alpha-to-view-depth relation and exact projection near/far conventions
  from measured geometry and matrix evidence. Convert to the selected FSR device-depth
  convention, with tested near/far/infinite/sky handling. Never feed raw alpha linear depth
  into a normalized device-depth input. Validate multiple distances, FOVs and resolutions.
- Materialize camera motion at render resolution (prefer RG16F if the backend accepts it).
  Existing xy is previous UV minus current UV. Prove the pinned SDK's scale, sign, Y axis
  and jitter cancellation with known one-pixel translations and rotations; set flags from
  that proof. Include distinct previous/current projections. No double removal of jitter.
- Use separate eye contexts and histories. Reuse the audited fence/serial/reset contracts
  for all added consumers; depth read_done must occur after the FSR input reads are queued.
  Do not let the earlier clarity RAII scope release a slot before FSR reads it.
- Audit colour placement and format. Prefer a coherent scene input before HUD and unsuitable
  post effects; if using current post-tonemap colour, implement/test the correct LDR path
  and document the remaining limitation. Supply exposure according to that chosen path.
- Moving-object vectors need reliable draw identity plus previous transforms/bones. Treat
  this as a separate measured feature. Until available, label the candidate camera-only.
  Masks can reduce history trust; they do not manufacture missing object motion. Automatic
  reactive generation needs its required opaque/composited inputs, which are not yet proved.
  Test particles, glass, water, animated NPCs, hands and rapid camera motion explicitly.

Exit: deterministic depth/motion/jitter/exposure tests, two-eye isolation and disocclusion
controls pass. A static-world prototype may proceed for diagnosis with limitations logged;
full moving-scene quality acceptance cannot be inferred from those results.

### Gate 5: lifecycle, F10 and safe fallback

FSR replaces custom TAA while active; do not accumulate twice. Avoid stacking FSR sharpening
with the existing sharpening pass by accident. Proposed controls (not implemented): FSR
Off/Quality/Balanced/Performance and one sharpening slider, default Off. Start with Quality;
expose lower modes only after validation. Keep the original TAA available for comparison and
restore its stored preference on disable. Clarify output resolution versus FSR input scale.

Free inactive TAA histories rather than retaining two temporal pipelines. Account separately
for colour/depth transport, vectors, two FSR contexts, output and transient storage. Measure
process virtual address space as well as GPU bytes; this is a 32-bit game. Avoid holding old
and new full allocations concurrently on resize. Recreate contexts only when required.

Handle missing/stale depth, refused shader creation, device loss/reset, resolution/FOV change,
mono menus, load/teleport/cut, capture mode changes and eye gaps. A failure returns a complete
current image by the existing blit, resets the affected history, logs the reason and allows
an explicit retry. Never submit an uninitialized/stale FSR output. Ensure D3D11 bindings are
restored/unbound before the later overlay passes. Do not block the render thread waiting for
GPU query results in the steady state.

### Gate 6: validation and acceptance

Retain the existing 72 TAA GPU checks, 6 calibration checks and frame regressions. Add FSR
host controls for stationary slanted edges under deliberate jitter, fine coloured points,
camera translation, moving foreground/disocclusion, invalid depth, sky, transparent changes,
independent eyes, frame intervals and resets. Compare against a high-resolution reference
and equal-resolution raw/bilinear controls. A passing dispatch counter is not a quality test.

Simulator: prove actual gameplay first (boot Attach can report success while still at a
loading prompt), then rotations/translations, menus/load, FOV/resize, live FSR modes,
capture off/shared/deferred, reset and disable/re-enable. Verify dimensions, jitter phases,
per-eye dispatch counts, stale-depth rejection and deterministic recovery from each change.
No game-derived captures committed. Game launches follow the user's current permission;
previous simulator permission was for motion-vector work, not blanket future permission.

Performance: keep output size, scene, refresh rate and quality controls fixed. Compare
native with TAA off, old TAA, reduced-input plain scaling, and FSR Quality. Warm up and gather
CPU/GPU frame-time distributions, including tail percentiles and dropped/reprojected frames,
plus separate depth/input preparation and FSR GPU brackets for both eyes. Include allocation
and resize peaks. Prior low-resolution experiments gained little in a CPU/fixed-cost-heavy
scene, so do not promise FPS from pixel count alone or compare synthetic shader times to
whole-game frame times. Try at least a pixel-heavy scene and the known fixed-cost scene.

Headset: one question per launch, expected outcomes stated beforehand. First ask whether
Quality at the same output size visibly improves edge stability over equal-input plain
scaling without objectionable trails. Improvement supports reconstruction; unchanged output
requires checking live activation and input/output provenance before retuning. Separate
later questions cover native-quality tradeoff, moving-object trails and comfort/performance.
No perceptible improvement is a valid negative result, as it was for custom TAA.

Done only when the Win32 integration passes host and simulator regressions, toggles/loads
recover, memory fits, and the tester accepts a visible quality or net performance tradeoff.
Keep it default off and unmerged until then. Record failures and evidence here as work proceeds.

### First implementation session deliverable

Complete gate 1 first: pinned backend, reproducible x86 build, two-context synthetic dispatch,
resource budget and an explicit compatibility verdict. Then proceed to dimensions and jitter
ownership. Do not spend the session tweaking custom TAA weight, installing an x64 SDK DLL,
or assuming the engine projection classifier is already suitable. NEXT_SESSION contains the
copyable Claude starting brief; this section is the sole maintained implementation plan.


## 2026-09-26: DLAA through an x64 NGX helper (phase 1) - built, host and simulator verified

### Decision: DLSS first, then FSR 3.1; FSR 2.2.1 dropped

The maintainer redirected the FSR plan above in three steps in one session: FSR 3.1 instead of
FSR 2.2.1; then DLSS if FSR 3.1 needed a 32-to-64-bit bridge; then, after research into a
community BioShock VR fork that ships DLSS in a 32-bit VR mod, DLAA first, then DLSS Super
Resolution, then FSR 3.1. The FSR 2.2.1 gate 1 above is superseded and was not run. Branch
renamed from `codex/fsr-implementation` to `claude/dlss-dlaa` (the old remote name had no PR
and was deleted). Still stacked on the unmerged `claude/motion-vectors`.

### FSR 3.1: the 32-bit build question, answered at compile level

FidelityFX SDK v1.1.4 (MIT, `c6efa6bf`) ships FSR 3.1's upscaler with DX12 and Vulkan back ends
only, prebuilt as x64 DLLs. Its CMake names a Win32 platform (`FFX_PLATFORM_NAME x86`) but
forces x64 in `toolchain.cmake`. Three local patches built `ffx_fsr3upscaler_x86.lib`,
`ffx_fsr3_x86.lib` and `ffx_backend_dx12_x86.lib` with VS 2022 Win32, zero pointer-truncation
warnings (C4244/C4267/C4311/C4312):

1. `toolchain.cmake`: generator platform x64 -> Win32.
2. PIX removed (no Win32 WinPixEventRuntime): `ENABLE_PIX_CAPTURES` undefined, `pixlib` unlinked,
   `libs/pix` not added, two now-unused parameters voided (warnings are errors there).
3. The frame-interpolation swapchain excluded from the DX12 back end: it includes AntiLag 2,
   whose struct-size static assert is 64-bit only, and frame generation is out of scope anyway.
Shaders are compiled by the SDK's own x64 tool; the permutation headers are bytecode, the same for x86.

`C:\Windows\SysWOW64\D3D12.dll` exists, so a 32-bit D3D12 device is available in-process.
NOT proven: a GPU dispatch. The route is FSR 3.1 in-process on a 32-bit D3D12 device sharing
textures with the proxy's D3D11 device (same process, same adapter) - no helper process.
Scratch build in `C:\dev\fsr-src` (not in the repo). This is phase 3.

### Why the DLSS route is a helper process

`nvngx_dlss.dll` is x64-only and closed, so it cannot load into Dishonored. The community
BioShock VR DLSS/DLAA fork (MIT, built on the trilogy mod v0.8.2) solves this with 64-bit
helper exes that open textures and fences the 32-bit game shares by NT handle. That fork ships
zero projection jitter and camera-only vectors (its own panel says so). Its helper is a 3,400-line
generalised DLSS5-Feeder derivative with ReShade/RenoDX lanes; this mod uses a rewrite of the
same route instead, credited in `src/tools/dlss_host/NOTICE.md`.

### What was built

- `src/tools/dlss_host/dlss_host.cpp` (x64, `tools/build-dlss-host.ps1`): ONE helper process,
  a D3D12 device on the proxy's adapter LUID (passed on the command line; it refuses any other
  adapter, because shared textures cannot cross adapters - see the dual-LUID history), NGX via
  `Init_with_ProjectID` (custom engine, own UUID), one DLSS feature per eye, preset K for DLAA
  set explicitly, flags MVLowRes | DepthInverted | AutoExposure. Explicit COMMON <-> read/UAV
  barriers on the shared textures, a three-allocator ring, sampled GPU timestamps, SEH around
  NGX create/evaluate.
- `src/core/gfx/dlss_ipc.h`: the wire contract. Every shared texture and both fences per eye
  are created by the PROXY on D3D11 and duplicated into the helper. The helper never opens the
  game process.
- `src/core/gfx/dlss_client.{h,cpp}` (x86, no mod dependencies): launch in a kill-on-close job,
  pipe handshake, per-eye build, and per eye image: copy colour/depth/motion, Signal(in, n),
  Frame, FrameAck, GPU Wait(out, n). The ack means the helper has QUEUED Signal(out, n), so the
  present thread never CPU-waits for the GPU work. A dead helper fails the next evaluate at once.
- `src/core/gfx/dlss_gpu.{h,cpp}`: guides from existing data. Depth R32F reversed, 1/(1+z) of the
  shared scene-alpha depth, 0 for sky/no depth - an ordering, not device depth. Motion RG16F,
  previous UV minus current UV from the same reprojection as the fused TAA, unclipped.
- `src/core/gfx/dlss.{h,cpp}`: settings, a worker thread for start/build (the present thread
  never blocks on NGX init), state Idle/Working/Ready/Failed, fail-soft to the normal path,
  `idle()` releases everything when switched off, a 5 s status line.
- `clarity::draw`: with DLAA on, a tagged eye image goes through DLAA and then clarity's
  resolve/sharpen; the custom temporal blend does not also run. `depth_probe` requests the
  depth copy while DLAA is on.
- Levers: `[Clarity] DLAA=0` (default), `DlssPreset=0`; seam `dlss on|off|retry|preset <n>`;
  F10 Advanced > Display > Clarity and anti-aliasing > "NVIDIA DLAA (experimental)".
- Packaging: `tools/fetch-ngx.ps1` pins NVIDIA DLSS SDK v310.7.0 (`a291cc7d`), the same runtime
  the fork tested: `nvngx_dlss.dll` 310.7.0.0, SHA256 `BE6E434A...F6EE6E`. `install.ps1` refuses
  any other hash and installs `<game>\dvr_dlss\` (helper, runtime, NVIDIA license, NOTICE).

### Host test (`tools/dlss-host-test.ps1`, 10/10, RTX 4070 Ti SUPER, no game)

32-bit client, real x64 helper. Each check can fail:

| Check | Result |
|---|---|
| start + NGX init | ready in 1.6-1.7 s |
| two DLAA features | 512x512 each |
| eyes isolated (different scene per eye) | own image error 0.0055, other eye's 0.736 |
| motion vector sign, scene moving +1 px/frame | true sign 0.0055, flipped 0.0123, zero 0.0155 |
| reset on a hard cut | 0.0106 after one frame |
| eye size 2752x2848 | GPU evaluate 2.01-2.09 ms per eye, isolated |
| helper killed | evaluate fails in 1 ms, no hang |

The sign test is what fixed the vector convention: DLSS reads previous-minus-current, the same
field the fused TAA already computes.

### Simulator (two launches, this session)

Build `v1.0.1-95-ga4fb67869-dirty` (19:14 and 19:33). Gameplay reached (log state, both eyes
90/s) before every measurement.

- `dlss on` live: helper ready in 2.1 s, both features at 2750x2850 within 0.2 s, then
  **69 DLAA images/s per eye, 0 refused, 0 fallback**. Present-thread cost 0.4 ms per eye.
- **Cost: stereo 90/s -> 69/s.** The helper's evaluate timestamps read 5.1-5.6 ms per eye in the
  game against 2.0 ms isolated: the D3D12 queue shares the GPU with the game's own rendering,
  so wall time between its timestamps includes time-slicing. Treat 2 ms/eye as the kernel cost
  and 5.4 ms as what it occupies under contention. Either way, DLAA at 7.8 MP per eye costs frame
  rate. It is a quality lever; the performance lever is DLSS SR (phase 2).
- Eye images: compositor captures with DLAA on/off gave mean luma 8.2/8.8 vs 8.3/8.9 and
  non-black 31.7/35.1 % vs 31.7/35.0 % (not committed, game-derived).
- Helper killed mid-game: detected in 46 ms, stereo continued (85-90/s), `dlss retry` back in 1.9 s.
- FOUND AND FIXED: `dlss off` left the helper and 299 MiB running because nothing called into
  DLSS while off. `idle()` now releases it; verified: stopped in 125 ms, 90/s restored.
- `[Clarity] DLAA=1` at boot: DLAA came up by itself through menu -> load -> gameplay.
- Game exit kills the helper (kill-on-close job): no orphan either time.

### Limits (phase 1) and what is not established

- No headset result. Whether DLAA is visibly better than the plain path here is NOT known.
- No projection jitter. The head's own motion moves the image a little every frame; that is
  all DLSS has for new sample positions.
- Camera-only vectors: NPCs, hands, weapons, particles, water carry the camera's vector.
- Depth is an ordering from scene alpha with the coarse 200 uu/unit scale.
- Colour is the post-tonemap gamma capture (LDR, AutoExposure); HUD/Scaleform in the scene
  image goes through DLAA too.
- Sharpen (0.40 installed) still applies after DLAA: one sharpening pass, no DLSS sharpening.
- NVIDIA RTX only; anything else logs the NGX refusal and runs the normal path.

### Next

1. Headset: one question - does DLAA (F10 toggle, panel closed) visibly reduce edge shimmer
   compared with off, without objectionable smearing on moving NPCs or hands?
2. Phase 2 DLSS SR: render/output size split (FSR plan gate 2 applies unchanged), preset by
   ratio, the helper's feature already accepts ow > w.
3. Projection jitter (FSR plan gate 3 applies unchanged) - benefits DLSS and FSR alike.
4. Phase 3 FSR 3.1: in-process 32-bit D3D12, the three patches above, same guides and lifecycle.


### 2026-09-26 (later): the walking smear, measured and fixed in the simulator

Headset report on the first DLAA build: very good image, better still with SSW, slight smear
while moving. The pipeline was healthy in that run (72 DLAA images/s per eye, 0 refused).

New instrument `dlss/audit` (dlss_gpu.h): every eye image, a 64x64 grid compares the current
image against the previous one moved by the guide vectors ("vec") and not moved ("zero"),
binned by depth, read back two frames late without stalling. Standing still both read ~0.0046.
All numbers below: stick walking in the simulator, 12 s per condition, vec/zero ratio.

| Condition | 0.1-0.3 (arms) | 0.3-1 | 1-2 | 2-10 |
|---|---|---|---|---|
| as shipped (all pixels walking parallax) | 3.2 | 0.78 | 0.65 | 0.76 |
| forward translation negated | - | 1.77 (whole <2 band) | | 1.20 |
| arms excluded (DlssBodyDepth 0.30) | 1.00 | 0.78 | 0.64 | 0.75 |
| depth scale 100 / 400 / 800 (arms excluded) | 1.0 | 1.21 / 0.78 / 0.89 | 1.10 / 0.69 / 0.82 | 1.03 / 0.77 / 0.84 |

- Written vs rendered camera: identical in the simulator (0.00 uu against a 2.2-2.7 uu step).
- Fix: the arms keep rotation, drop translation. Scale 200 kept (broad minimum 200-400).
- Anti-smear bias mask built and host-tested; DLSS already rejects large unexplained motion and
  the mask cannot see sub-pixel errors, so it ships OFF as an A/B (`dlss mask on|off`).
- Left: NPCs and controller-moved hands have no own vectors; the remaining far-band residual
  sits near the frame noise floor. Headset verdict on the fix pending.


### 2026-09-26 (later still): vector accuracy in pixels - matrices and the real depth scale

The second headset run reported the smear unchanged; its audit showed error spread across all
depth bands, not the arms. New `dlss/flow` block-search check (FLICKER_REFERENCE top entry has
the method). Simulator, 12 s per condition:

| Condition | walk | strafe | smooth stick turn |
|---|---|---|---|
| rotator/FOV vectors, scale 200 | 1.68 px, gain 0.90 | 1.28 px, 0.84 | 1.38 px, shift 1.04 |
| game matrices, scale 200 | 1.68 px, 0.90 | 1.28 px, 0.84 | 0.56 px, shift 0.13 |
| game matrices, scale 300 | 1.44 px, 1.07 | 0.99 px, 1.10 | - |
| game matrices, scale 250 (shipped) | 0.66 px, 0.98 | 0.64 px, 0.98 | 0.43 px, 1.00 |
| standing still (instrument floor) | 0.28-0.41 px | | |

- The captured world view-projection is camera-relative and row-vector: `clip = [P - C, 1] * M`,
  with w the linear view depth in uu (checked: forward yaw -81.5 vs record -81.46, 103 degrees
  FOV, square pixels). It is stored per record at the present with the rendered c5.
- Depth is linear in scene alpha (gain flat across bands); 250 uu per depth unit replaces the
  coarse 200 from the rotator-model calibration. `MotionDepthScale` default 250 (the custom TAA
  shares it).
- `dlss vp on|off` A/Bs matrix vs rotator vectors live; `dlss taxis` is a diagnostic only.


## 2026-09-26: DLSS Super Resolution (phase 2) - built, host and simulator verified

Headset verdict on DLAA after the vector fixes: the smear gone as far as the tester can tell
(mask off), aliasing removed, very sharp; a large frame-rate cost that SSW makes playable.

Design: `[Clarity] DlssQuality` 0 DLAA, 1 Quality (1.5x per axis), 2 Balanced (1.72x),
3 Performance (2x), 4 Ultra Performance (3x). The OUTPUT is the headset resolution
(`DlssOutputWidth/Height`, taken from the current resolution when SR first turns on, set by the
F10 resolution while SR is on). The game side (`DlssResTick`, viewport_resize.cpp) keeps the
render size at output / ratio through the guarded live resize, which persists it as `[Screen]
RenderWidth/Height`, so a later launch boots reduced. SR off, or DLSS failing, resizes back to
the output and clears it. One ask per target per 15 s. clarity's output size is the SR output
whenever the eye image is the reduced render, so the eye texture and swapchain stay full size.
The helper picks the NGX quality mode from the ratio and the 310.x presets (K for DLAA/Quality/
Balanced, M Performance, L Ultra Performance).

Host test 13/13, new: Super Resolution 512 -> 768 builds, output 768x768, error against the
enlarged input 0.0117.

Simulator (same scene, 2750x2850 output, sim capped at 90/s per eye):

| Mode | Render | Per eye | DLSS GPU per eye |
|---|---|---|---|
| DLAA | 2750x2850 | 64-65/s | 4.2-5.3 ms |
| Quality | 1832x1900 (44% of the pixels) | 90/s (the cap) | 3.5-3.9 ms |
| Performance | 1374x1424 (25%) | 85/s | 4.4-4.5 ms |

Live transitions: DLAA -> Quality resize confirmed in 0.5 s, both eyes ready 2.6 s later; Quality
-> Performance and Performance -> DLAA likewise; DLAA restored 2750x2850 and cleared the output.
Mean luma and coverage match across the three modes. No headset result yet. Limit: no projection
jitter, so SR reconstructs from head micro-motion only - expect it softer than DLAA until jitter.
Next: headset check of Quality, then projection jitter (FSR plan gate 3), then FSR 3.1.


### 2026-09-26 (later): Super Resolution does not raise the frame rate here - measured

Headset: Quality and Performance both ran ~85-93 images/s per eye against 130-140 native at
144 Hz. The log confirmed the reduced render sizes (1832x1900, 1374x1424) and ~3-3.8 ms of DLSS
GPU per eye with NVIDIA's per-mode presets (M for Performance).

Cost per evaluate at a 2750x2850 output, isolated (`tools/dlss-host-test.ps1 -Cost`), ms per eye:

| Preset | Performance in | Quality in | DLAA |
|---|---|---|---|
| M (NVIDIA's Performance default) | 2.80 | 4.17 | 7.98 |
| L (Ultra Performance default) | 3.26 | 5.08 | 9.71 |
| K (transformer) | 1.99 | 1.96 | 2.23 |
| J | 1.93 | 1.90 | 2.03 |
| E / F (CNN, marked deprecated, still run) | 0.94 / 0.82 | 0.94 / 0.86 | 0.92 / 0.91 |

Simulator, cap lifted (`refresh 240`), same room, per eye:

| Mode | frames/s | frame (both eyes) | game GPU render per eye |
|---|---|---|---|
| native 2750x2850 | 148-153 | 6.5-6.8 ms | 2.6 ms |
| native 3368x3490 (150%) | ~129 | 7.7 ms | 3.1 ms |
| Performance, fast (CNN) | 124-132 | 8.0 ms | 1.9 ms |
| Quality, fast | 119-124 | 8.4 ms | 2.1 ms |
| Performance, K | ~108 | - | - |
| Quality, K | ~96 | 10.4 ms | 2.8 ms |
| DLAA, fast | ~80 | 12.6 ms | 4.1 ms |
| DLAA, K | ~67 | 14.9 ms | 4.3 ms |

- The game's GPU cost barely follows the pixel count: a quarter of the pixels saved 0.7 ms per
  eye (2.6 -> 1.9), the same fixed-cost floor recorded earlier. Super Resolution therefore
  cannot buy frame rate in this game on this GPU; every DLSS mode is a net cost.
- The DLSS helper's work contends with the game on the GPU: under DLAA the game's own render
  rose 2.6 -> 4.1-4.3 ms per eye, and the helper's timestamps read 2-5 ms against 0.9-2.2 ms
  isolated. The cross-process (D3D9/D3D11/D3D12 in two processes) scheduling is a large part of
  the cost, beyond the model itself.
- Changes: preset K for every mode by default (M/L were the most expensive); `DlssModel=1`
  (F10 "DLSS fast model", `dlss model fast`) selects the CNN presets E (SR) / F (DLAA); the mask,
  audit, flow check and previous-image copy run only when the mask or `dlss audit on` asks.
- FOUND AND FIXED: `dlss output` with SR off set an output the game side then "restored" by a
  native resize; refused now unless SR is on.
- Simulator caveat: several mode changes were lost when seam commands were written back to back
  (command.txt holds one command); the table uses only windows whose mode the log confirmed.
- Open: pipelining DLSS so its GPU work overlaps the game's (the fork overlaps left-eye DLSS with
  the right-eye scene) is the remaining lever for DLAA's cost; FSR 3.1 in-process avoids the
  second process but not the cross-API scheduling.


### 2026-09-26 (later): the overlap - capture and depth released before the DLSS wait

The perf line showed the present thread waiting ~2.6 ms per eye image on the capture ("lock"):
`read_wait` holds a capture slot until the D3D11 reads of it are done, and the read fence was
ended after all the present's D3D11 work, i.e. behind the DLSS wait. The slot is read only by the
DLSS input copy (and the optional mask/audit before it); the shared depth only by the guide
pass. Both are now released there (`EyeInputs::afterCopy`, `depthprobe::read_done` after the
guides; `capture::read_done` is once per delivery so the present's own later call is a no-op).

Simulator, cap lifted, one command per window (per eye):

| Mode | before | after | capture lock after |
|---|---|---|---|
| DLAA fast | ~80/s | 86/s | 0.6 ms |
| DLAA K | ~67/s | ~70/s | 2.6 ms |
| Quality SR fast | ~120/s | 131/s | 0.3 ms |
| Performance SR K | ~108/s | 115/s | 0.6 ms |
| native (same run) | 148-153 earlier | 133/s | 0 |

- DLAA with K is GPU-bound: with the helper on the GPU the game's render rose 2.7 -> 4.4 ms per
  eye and DLSS 2.2 -> ~5 ms. The cross-process scheduling cost, not the CPU wait, limits it. One
  batched DLSS submission per frame (both eyes) is the remaining lever; it needs a per-eye
  texture and deferred swapchain copies in the runtime layer - not attempted (flicker history).
- Quality SR with the fast model ran at native's rate in the same run (131 vs 133).
- Stale-eye check: every "pushed eye -1 TWICE" (and the one STALE L EYE) in these runs falls
  within 0.2 s of a live resize confirmation, before and after this change (3 and 5 in the two
  previous runs), none in steady play. A one-present transient of the live resize path, not the
  overlap; recorded as an open item.


## 2026-09-27: Projection jitter for DLSS - built, host-proved, headset-confirmed (default off)

`core/gfx/dlss_jitter.{h,cpp}`, `[Clarity] DlssJitter=0` / `DlssJitterWide=1`, `dlss jitter on|off`,
`dlss jitter wide on|off`, F10 "DLSS jitter (experimental)" and "Jitter: include all eye-size passes".

- **What:** every perspective c0..c3 upload of the world passes gets a clip shift `clip.x += ax *
  clip.w`, `clip.y += ay * clip.w` (a pure sub-pixel screen shift at every depth), Halton(2,3),
  8 * ratio^2 phases (DLAA 8, Quality 18, Performance 32). Both eyes of a stereo pair (pose pairId)
  share the phase; it advances after the pair. The offset is fixed at each present and stored in that
  image's pose record (`jitter`, `jitterDraws`); DLSS gets it from the record, so image and offset
  cannot come apart. The recorded view-projection is the game's own (the hook records before it
  patches), so the vectors exclude the jitter.
- **Which draws:** world passes are identified by OBSERVATION, not `IsMainScenePass`: the depth
  surface bound at the c5-tied view-projection (new `SetDepthStencilSurface` hook, identity only),
  confirmed when its viewport equals the captured eye image. Shifted: every perspective upload on that
  depth surface, plus (wide rule) every perspective upload into an eye-size colour target whatever
  depth is bound. Not shifted: shadow maps and captures (800x800 here), affine 2D/post/HUD uploads.
- **Sign, host test** (`tools/dlss-host-tests.cpp`, 15/15): Super Resolution 512 -> 768, still
  scene finer than the render grid, 18 phases, error vs the 4x4-supersampled scene: reporting the
  NEGATED sample offset -x-y 0.0043; -x+y 0.0119, +x+y 0.0198, +x-y 0.0231; no jitter 0.0220;
  jittered but reported 0 0.0173. And on the CPU, the hook's row patch on the measured matrix layout
  moves 72 points at 20-20000 uu by exactly minus the offset (worst 2e-4 px).
- **Simulator (colour-target key, first build):** LIVE, 3,800 uploads/s shifted, 161 images/s
  jittered, 80 pairs/s, frame rate unchanged (~80/s per eye DLAA fast). FAILED: flow-check jitter
  gain 0.48-0.59 (1 expected), scatter 1.1 px against 0.26 off, and the left eye lost lit surfaces.
  The flow check now subtracts the recorded jitter change and reports the jitter gain
  (`dlss/flow jitter`).
- **The fix, in two steps (FLICKER_REFERENCE 2026-09-27):** keying on the depth surface caught
  1,750 uploads/s drawn into another colour target on the scene depth; the headset still showed black
  speckles in the left eye; the wide rule caught the rest. Per-eye census on the headset: L 39 scene-
  depth + 6.0 wide uploads per image, R 37 + 1.0, UNSHIFTED 0; speckles gone.
- **Not yet measured:** whether Quality/Performance SR are visibly sharper with jitter (the reason
  it exists); the flow-check jitter gain on the final build; shimmer on thin geometry.


## 2026-09-27: Why DLSS cannot raise the frame rate here, and the routes past the CPU ceiling

### The measurement (already on disk, no new launch)

Simulator, cap lifted (`refresh 240`), 2750x2850 per eye, `stereo reentry`, build
`v1.0.1-100-g1a497d4cd-dirty`, log `build/dlss-install/overlap-logs/` (local). This PC: Ryzen 5 5600X
(6 cores / 12 threads, 32 MB L3, DDR4-3600 CL16 at XMP), RTX 4070 Ti SUPER, High performance power
plan, hardware GPU scheduling on. Per stereo pair, from the `perf: tick` split:

| | Performance SR (1374x1424, 25 % of the pixels) | Native 2750x2850 |
|---|---|---|
| Pair time | 8.6-8.9 ms (113-117/s) | **6.5-7.6 ms (132-154/s)** |
| Render thread executing the engine's frame (`R`, both eyes) | 4.3-4.5 ms | 4.1-4.5 ms |
| Render thread idle before the left eye (`idle`: nothing queued, waiting for the GAME thread) | 0.3-0.6 ms | **1.4-2.1 ms** |
| Our present path (`in`, both eyes) | 3.8-3.9 ms (1.3 + 0.8 ms of it the capture fence behind DLSS) | 1.1 ms |
| D3D9 GPU span per eye (an upper bound on GPU busy) | 2.3 ms | 2.6 ms |

- **The game thread is the ceiling.** At native the render thread finishes its two views and then
  waits 1.4-2.1 ms for the next tick: the game thread needs about 6.5-7.5 ms per tick (world tick,
  script, our script lane, and the two viewport draws re-entry makes it issue). That is 133-154
  ticks/s, and one tick is one stereo pair.
- **The render thread is the second limit**, about 5.4 ms per pair busy (engine 4.3 + ours 1.1).
- **The GPU is the third**, at most 5.2 ms per pair. Cutting pixels moves only this one: a quarter of
  the pixels saved 0.7 ms per eye of GPU span (2.6 -> 1.9, the earlier SR entry) while the pair time
  stayed on the game thread's floor, and DLSS then added its own present-thread wait and GPU
  contention with the helper process. So every DLSS mode is a net cost here, and more modes or presets
  cannot change that. This agrees with the 2026-09-15 finding that a quarter of the pixels gained ~6 %.
- **Consequence for how DLSS should be used on this PC:** not to render less, but to output MORE at the
  same cost. Quality SR with the fast model already ran at native's rate (131 vs 133/s in one run), so
  a 150 % output rendered at 100 % should look sharper than native 100 % for about the same frame rate.
  PREDICTION, not measured: `dlss on`, `dlss quality 1`, `dlss output 4126 4276`, compared against
  native 100 % in the same spot.

### Routes past the ceiling, ranked by payoff for THIS machine

1. **Decoupled rendering: more head-tracked pairs per world tick (the out-of-the-box one).** The
   re-entry already patches UGameEngine::Tick's single viewport-draw call site and calls the draw root
   twice per tick with the camera field rewritten between the passes (`scene_draw.cpp`). Calling it
   four times (L, R, L, R), with the second pair's camera taken from a NEWER head pose, renders a fresh
   stereo pair without running another world tick. Head motion and parallax are real renders at the
   higher rate (not reprojection); animation, physics and AI move at the tick rate - the split every
   fixed-timestep engine makes between simulation and rendering (Gaffer on Games, "Fix Your
   Timestep"), here without the interpolation. Predicted ceiling with one extra pair per tick: game
   thread ~7 ms + ~1 ms for the two extra draws per two pairs (~270 pairs/s), render thread ~5.4 ms per
   pair (~185/s), GPU <= 5.2 ms per pair (~190/s): **about +25-30 % over today's ~145/s, and past that
   point DLSS becomes useful** because the GPU turns into the limit. Costs and risks: the second pair
   needs a pose located for its own display time on the game thread; one XR frame per pair (the
   runtime layer's pair pacing already works per pair); the HUD PostRender runs per draw; moving
   objects step at the tick rate (visible only when the tick rate falls well under the refresh);
   VR-79's stereo-visibility rule must hold for the extra views; re-entry's fail-soft gates apply
   unchanged. First step: measure the game thread's per-draw cost (the existing cpu scopes, lanes 8/9)
   and prototype an extra-pair lever default OFF behind a live A/B.
2. **Our own game-thread cost (bounded by the 1.4-2.1 ms idle).** The ProcessEvent hook runs about
   40 `strstr`/`strcmp` and a `RealName` per dispatch before its first early return (VR-160 suspect 7),
   plus periodic GObjects walks on the script lane. A dispatch table keyed on the UFunction pointer
   makes the hook near free. Measure first: a cycles scope around the hook, then the A/B.
3. **One engine view for both eyes (render-thread halving).** NVIDIA 3D Vision's automatic mode issued
   every draw twice with a clip-space shift in the vertex shader; vorpX's Geometry 3D does the same for
   DX9. Here it would mean one InitViews, one culling pass (1.08 of 1.32 ms of preparation per pair is
   culling, the 2026-09-15 boundary), one draw-list build and no second viewport draw on the game
   thread, with only the D3D9 calls doubled - the clip-space shift is the same row patch the jitter
   already applies. Largest engineering item: per-eye render targets, view-dependent passes
   (reflections, screen-space post, occlusion) and the HUD all need eye copies.
4. **Driver-side submission.** The vr125 CPU capture showed the render thread blocked 27 % of the time,
   mostly woken by an NVIDIA driver worker. An A/B of the driver's Threaded Optimization for
   Dishonored.exe (NVIDIA Control Panel, per program) costs no code and one run. A stock DXVK used only
   as the D3D9 translation layer (its submission thread offloads CPU-bound DX9 games, GTA IV being the
   well-known case) is a different thing from the removed side-by-side fork, but CLAUDE.md forbids
   bringing a Vulkan layer back and the shared capture, depth probe and HUD capture would all need
   re-proving; it needs the maintainer's explicit decision, and a flat A/B (`disable_vr.txt`) would size
   it first.
5. **Hardware.** A Ryzen 7 5800X3D drops into the same AM4 board; 3D V-Cache is the standard answer for
   cache-sensitive CPU-bound games. The most dependable single uplift, unmeasured for this game.
6. **Runtime.** At a steady 120 Hz the CPU has headroom (8.33 ms per pair against ~7 ms); at 144 Hz it
   does not. SSW/ASW at half rate (already in use) buys supersampling headroom.

Not useful here: alternate-eye rendering (one world tick per displayed image, so the game thread stays
the limit and the eyes desync); more DLSS modes for speed. Already checked on this PC: RAM at XMP,
High performance plan, HAGS on.

### Next measurements (one launch each, asked for individually)

(a) cpu scopes on, native, standing: the game thread's per-draw cost against its whole tick - decides
route 1's prediction. (b) Threaded Optimization off / on / off - route 4 at zero code cost.
(c) DLSS Quality at a 150 % output against native 100 % - the supersampling use above.

Sources: [Gaffer on Games, Fix Your Timestep](https://gafferongames.com/post/fix_your_timestep/);
[NVIDIA 3D Vision Automatic background](https://archive.docs.nvidia.com/gameworks/content/technologies/desktop/nv3dva_background.htm);
[vorpX features](https://www.vorpx.com/features/); [UEVR documentation](https://docs.uevr.io/)
(synchronized sequential is this mod's re-entry, with its stated cost); [DXVK on PCGamingWiki](https://www.pcgamingwiki.com/wiki/DXVK);
[GTA IV optimization guide](https://gillian-guide.github.io/optimization/); DLSS 4.5 presets:
[NVIDIA](https://www.nvidia.com/en-us/geforce/news/dlss-4-5-dynamic-multi-frame-gen-6x-2nd-gen-transformer-super-res/).

## 2026-09-27: Route 2 built - the script lane's own cost (simulator-measured, default on)

Branch `claude/pe-hook-dispatch`. `game/dishonored/ue3/pe_fast.h`; levers `[Perf] PeFast=1`,
`PeHeavyMs=2`, `PeHeavyInDraw=1`; seam `pe fast on|off`, `pe heavy <ms>`, `pe heavydraw on|off`,
`pe fn on|off` (diagnostic).

- **The measurement that sized it** (`pe/cost`, new, every 5 s; simulator, 2750x2850, cap lifted, DLSS
  off, this PC's Ryzen 5 5600X): the ProcessEvent hook ran **~7,000 script events/s at ~75 us each =
  ~500 ms of the game thread per second** - about half of each ~6.4 ms tick, on the thread that is
  the frame-rate ceiling. By statement (`pe/cost-fn`): FovLeverApply 185 ms/s, CarryHoldTick 46,
  SkcRotApply 42, CamShakeTick 36, camera::apply_offsets 26, FxFollowTick 22, UiPeLatch 10, the rest
  under 7 each. VirtualQuery (RangeReadable) ran ~15,000 times a second.
- **Fast path (PeFast):** a region cache for the hook's own readability checks (cleared every second)
  and an FName-index -> traits cache for the event-name tests (the same strstr/strcmp tests, resolved
  once per index). VirtualQuery 15,000/s -> ~150/s; saves ~20 ms/s. Menu events still register
  (Dis_OpenPauseMenu / OnResumeGameClicked measured with it on).
- **Heavy-writer cadence (PeHeavyMs):** FovLeverApply (FOV lever + eye clamp) and apply_offsets wrote
  their fields on every event so ours is the last value before the draw. With the re-entry draw hook
  installed they now run at most every 2 ms during the tick, ALWAYS once at the viewport-draw entry
  (after the tick's last event, before pass 1 reads the camera) and ALWAYS for events inside the draw;
  without the hook every event runs them as before. SkcRotApply, CamShakeTick and CarryHoldTick are
  deliberately NOT throttled (they race the animation/physics tick itself, not the draw).
- **Result:** old cadence 151-162 ticks/s (hook ~495 ms/s) against **throttled 162-177 ticks/s, mostly
  170-175 (hook ~395 ms/s)**, about +10 %, repeated off/on twice. FOV readback held at the lever's
  108.07 deg target throughout, stereo eye check 33,063 agree / 0 disagree, walking fine. Throttling the
  in-draw events too (`pe heavydraw off`) cut the hook to ~320 ms/s with the same FOV and eyes but no
  further rate gain - the render thread is now the limit (idle 0.3 ms) - so it stays off by default.
- **Left on the table:** ~390 ms/s of per-event work remains (the mid ticks and SkcRotApply). Each is a
  candidate for its own cadence after checking what it races; the per-statement split (`pe fn on`)
  names them. Not headset-tested.

### 2026-09-27: in the HEADSET the GPU is the limit, not the game thread (corrects the routes above)

The maintainer's route-1 headset run (VDXR 144 Hz, 2750x2850, DLSS off, no SSW, route 1 toggled four
times each way): 122-124 pairs/s with the extra pair, 120-127 without - no difference. With it off,
per pair: 7.3-8.1 ms total, **D3D9 GPU span 6.7-6.9 ms**, the present thread waiting 1.1-1.9 ms on the
capture fence (the GPU), and the runtime reporting UNDER-SUBMITTING 0.89x. The simulator's game-thread
ceiling (~7 ms/tick, GPU ~5.2 ms per pair) does not carry to the headset, where Virtual Desktop's
encode and the compositor share the card and the GPU becomes the limit. So routes 1 and 2 (CPU) cannot
raise the headset's rate on this PC; route 2 stays as CPU headroom. Next lever: GPU time per pair
(our sharpen pass ~0.4 ms per eye, the streamer's encode, render size). Late in the same run the
runtime's period went to 13.89 ms (72 Hz) - Virtual Desktop halving the rate on its own.


## 2026-09-27: The uncap deep dive - where the headset frame goes, and the plan that tests it

Branch `claude/uncap-deep-dive` (stacked on route 2). Goal: find the serialisation that holds the
headset at ~120-137 pairs/s with the PC at ~25 % CPU and ~80 % GPU.

### What the latest headset log already says (offline, no new run)

Source: the maintainer's route-2 headset log of 2026-09-27 (build `v1.0.1-106-gb0e953df2`, VDXR 144 Hz,
2750x2850, SSW off, DLSS off, Sharpen 0.40, capture shared depth 1). Only the 35 windows of steady play at
144 Hz (the first ~290 s of that run sat at 13.89 ms / 72 Hz, Virtual Desktop halving the rate, and a
heavier area at ~70 pairs/s; both excluded). Medians, p10-p90 in brackets. The perf line's P1/P2 are the
presents that DELIVER the left/right image (delivery is one present late), so P1 is physically the
right-eye present and its `out` renders the next tick's left eye.

| Per stereo pair | median | p10-p90 |
|---|---|---|
| tick | 8.0 ms (124 pairs/s) | 7.5-8.6 |
| render thread, engine rendering (R, both eyes) | 4.6 ms | |
| render thread, waiting on the capture blit fence (P1 lock + P2 lock) | 1.3 ms | 0.6-2.4 |
| render thread, waiting on the game thread (P1 out idle) | 0.6 ms | 0.5-1.1 |
| render thread, our present path excluding the fence | ~1.0 ms | |
| D3D9 GPU span (render start to present entry, both eyes) | 6.9 ms | 6.6-7.7 |
| D3D9 GPU idle between presents | 0.1 ms | 0.1-0.3 |
| our D3D11 GPU work (conversion + sharpen 0.09, eye copy 0.05, per eye) | ~0.3 ms | |
| xrWaitFrame | 0.1 ms | (UNDER-SUBMITTING 0.81-0.94x: the runtime never throttles) |

- The capture's own window line: `blit fence waits` in 256-680 of ~750 grabs per 3 s, the D3D11 read fence
  never. So the render thread waits on the GPU finishing the PREVIOUS eye's blit, i.e. the GPU is more than
  one eye behind at that moment, on most ticks.
- All three stages sit near the same cost: GPU ~6.9 ms of D3D9 span plus ~0.3 ms of ours plus Virtual
  Desktop's share; render thread ~7.4 ms busy; game thread ~7.4 ms (the render thread waits 0.6 ms for it).
  A three-stage pipeline whose stages are this balanced, with at most one eye of buffering between the render
  thread and the GPU (the capture fence) and one frame between the game and render threads
  (OneFrameThreadLag), loses throughput to each stage's variance; nothing is saturated because each stage
  waits for a neighbour part of the time. That is the "25 % CPU, 80 % GPU" picture.
- Our D3D11 side is not the cost: ~0.3 ms of GPU per pair (`perf/bridge`), so the sharpen pass is not the
  0.4 ms per eye the earlier handoff guessed. Not a lever worth a segment.
- OPEN, and what the GPU timeline must answer: the D3D9 span (6.9 ms) is wall time on the GPU and can hide
  gaps. Two readings predict different things. (a) The GPU is truly busy for the whole span (game + ours +
  Virtual Desktop): the fence wait is only the throttle, and removing it moves the wait elsewhere with no
  rate change. (b) The GPU starves inside the span because the next eye's commands reach the kernel late
  (they sit in the D3D9 driver's buffer until something flushes it; with the desktop Present skipped, the
  flushes are ours - the capture fence poll and `submit_without_present`): then letting the render thread
  run further ahead raises the rate toward the GPU's own cost.

### Instruments built for it (this branch)

- `core/util/etw.{h,cpp}`: a TraceLogging provider `DishonoredVR` {6b3c1f4e-2d6a-4f7c-9a51-0d2e8c7b4a19}
  with begin/end events for the present, the xrWaitFrame, the game tick, the method, both capture waits,
  the HUD and its fence, xrEndFrame, the desktop Present or its flush, the frame-start marker, each
  game-thread scene draw (eye in `a`), and every seam command as a mark. Free unless a trace session enables
  the provider (one flag test per call); `[Perf] Etw=0` is a kill switch only.
- `tools/wpr/dvr-gpu.wprp` (`DvrGpu`: CSwitch + ReadyThread, DxgKrnl Base+Profiler+LongHaul with the
  context rundown, the proxy's markers; `DvrGpuStacks` adds 1 kHz samples with stacks) and
  `tools/perf-gpu-trace.ps1` (elevated; never launches anything): `-Smoke` proves the recorder; armed, it
  waits for the game, arms an A/B plan through the seam (`-Plan`), traces a few seconds inside chosen plan
  segments, and samples GPU clocks/power/throttle reasons (nvidia-smi, 4 Hz) and per-process GPU engine load
  (1 Hz) for the whole run, all stamped on the log's clock.
- The A/B plan reads a FILE now: `perf ab plan <file>` / `[Perf] AbPlan=<file>`, one segment per line,
  `label | apply seam words | restore seam words`, a row without apply words is a baseline; the built-in
  plan is unchanged. `tools/perf-plans/uncap-1.txt` is the first plan.
- Two levers, default off:
  - `[Capture] SharedDepth=1` / `capture depth 1|2|3`: the shared capture ring's delivery depth. Depth 1
    is the 41.1 two-slot ring (unchanged). Depth N keeps N+1 slots and delivers the slot blitted N
    presents ago, so the render thread can run up to N eyes ahead of the GPU instead of waiting on the
    previous eye's blit. Cost: one present (~4 ms) more latency per step, the pose record riding the image
    (VR-65) so the runtime still submits the pose each image was rendered with. Not in the default ini.
  - `res live pct <25..200>` / `res live <W>x<H>`: a session-only render size through the engine resize
    the F10 control and DLSS SR already use; nothing is written to either ini.

### Plan 1 and its predictions (recorded before the run)

`tools/perf-plans/uncap-1.txt`: 15 segments of 20 s (3 s warm-up discarded), seven baselines A-G between
depth 2, `pe heavydraw off`, a serial control (`capture sharedwait on`: the render thread waits for THIS
eye's blit, which must lower the rate or the plan cannot see a wait at all), depth 2 + heavydraw off, and
the render size at 70 % and 130 %.

| Segment | If (a) the GPU is the ceiling | If (b) our wait starves the GPU |
|---|---|---|
| depth 2 | fence waits ~0, rate within the baseline floor, the GPU timeline shows the 3D queue never empty | fence waits ~0 and pairs/s up toward 1 / (GPU span + ours), up to the 144 cap |
| heavydraw off | no change | no change unless the game thread is the next limit (render idle 0.6 ms falls) |
| serial control | rate DOWN (both) | rate DOWN (both) |
| res 70 % | rate up by roughly the pixel share of the GPU span | small change (a starved GPU does not care about pixels) |
| res 130 % | rate down | rate down less than the pixel ratio |

- Trace overhead is controlled by construction this time: VR-125's combined GeneralProfile+GPU capture cost
  ~25 % of the rate and its memory-ring GPU collector kept only events after the game had exited. DvrGpu is
  file-mode, has no stacks or sampling, and records 5 s windows inside 20 s plan segments, so every traced
  segment has untraced perf windows of the same configuration beside it.
- WPR needs admin; `tools/perf-trace-task-setup.ps1` (run once, elevated) registers four on-demand tasks
  under `\DishonoredVR\` that run a fixed script from an admin-only folder, so the recorder then runs
  without elevation and without a prompt. `-Remove` undoes it.
- Driver lever queued for after plan 1 (needs a restart per setting, so not in the plan): NVIDIA
  Threaded Optimization. VR-125's lightweight headset trace had the render thread blocked 27 % of the
  time, 91 % of that ended by the NVIDIA D3D9 worker thread, which itself polls; whether the driver's
  worker is the render thread's hidden wait is the question the CSwitch/ReadyThread data in DvrGpu answers.

### Plan 1 in the SIMULATOR (2026-09-27, shakedown; the headset question is still open)

Build `81bef09dd` + the fixes below, RelWithDebInfo, simulator at 240 Hz (cap lifted), 2750x2850, the same
save, standing still, untraced. Noise floor from 7 baselines: p50 5.91-6.34 ms (6.9 %), p99 9.28-11.47 ms.

| Segment | p50 pair | pairs/s | verdict |
|---|---|---|---|
| baselines A-D | 5.91-6.10 ms | 164-169 | - |
| depth 2 | 5.95 ms | 168 | no change |
| heavydraw off | 6.09 ms | 164 | no change |
| serial control (SharedWait=1) | 8.88 ms | 113 | **+45 %: the plan sees a wait** |
| depth 2 + heavydraw off | 6.14 ms | 163 | no change |
| res 70 % (1926x1996) | 6.13 ms | 163 | no change |

- The simulator does not exercise the headset's question: its capture fence already waits in 0-4 of ~950
  grabs per window at depth 1 (headset: 256-680 of ~750), and a 70 % render size did not move the rate,
  so here the game thread, not the GPU, is the limit (as recorded for route 2). Depth 2 worked
  mechanically (ring 2 -> 3 -> 2 rebuilt live, 0 fence waits, no stale eye), and the serial control
  proves the instrument can see a render-thread wait. The headset run is where depth 2 decides (a) vs (b).
- FAULT FOUND AND FIXED: the live resize persisted what it applied. `res live` reached `ResRequest`,
  which writes `[Screen] RenderWidth/Height`, `DishonoredEngine.ini [SystemSettings] ResX/ResY` and the
  launch file, and moved the base the percentage is taken from, so `res live pct 100` "restored" 70 %
  and the last three rows measured 70 % and 91 %, not 100 % and 130 % (discarded). `pe heavydraw`
  persisted `[Perf] PeHeavyInDraw` through `ConfigWriteKey`. Now: `ConfigWriteKey` writes nothing while an
  A/B plan row runs, `res live` advertises the mode in memory only and takes its percentage from the size
  configured at the first call. All three files were restored byte-for-byte from the pre-run backup.
- FAULT FOUND AND FIXED: the trace tasks ran with an interactive logon, and each start flashed a console
  that took the focus from the game, which pauses on focus loss - it stalled the first two plan attempts.
  The tasks now run as S4U (background session, no window).
- FAULT FOUND AND FIXED: the DvrGpu traces held the kernel events and the proxy's markers (~4,800
  phase events/s) but NO DxgKrnl events, by name or by GUID. Cause: the provider lacked
  `NonPagedMemory="true"`, which WPR's own GPU profile sets (`wpr -exportprofile GPU`): DxgKrnl logs from
  interrupt-level code and a paged session silently receives none of it.

### Plan 1 in the HEADSET (2026-09-27): no hidden serialisation - the GPU is full

Build `28078EF3` (the branch at `2407a4c9e`), VDXR 144 Hz, 2750x2850, SSW off, DLSS off, standing still,
the maintainer's own INI (unchanged, verified). Plan 1, 5 s DvrGpu traces inside five segments. Baseline A
was discarded (the plan started during the load); six baselines agree within 2 % (p50 8.56-8.73 ms).

| Segment | p50 pair | pairs/s | verdict |
|---|---|---|---|
| baselines B-G | 8.56-8.73 ms | 115-117 | - |
| depth 2 | 8.66 ms | 115.5 | NO CHANGE |
| heavydraw off | 8.84 ms | 113.2 | no gain |
| depth 2 + heavydraw off | 8.80 ms | 113.6 | NO CHANGE |
| serial control | 11.31 ms | 88.4 | -31 %: the plan sees a render-thread wait |
| res 70 % (1926x1996) | 7.64 ms | 130.9 | **+13 %** |
| res 130 % (3576x3706) | 12.29 ms | 81.4 | **-30 %** |

GPU timeline (`tools/perf-gpu-timeline.py`, baseline D, 5.8 s, 656 pairs, 112.5 pairs/s under trace):

- **The game's D3D9 queue had work pending 96.8 % of the window; the 3D engine was occupied by some
  process 97.6 %; idle 0.22 ms per pair.** The game's queue occupancy is 8.61 ms per pair of 8.89. Its
  gaps total 0.28 ms per pair, 72 % of them while the render thread was inside the engine's own
  rendering, none in our capture fence. The only GPU-side sync waits of the game's process (one per pair)
  are on a small secondary queue (our D3D11/XR copy, released by Virtual Desktop), not on the D3D9 queue.
- nvidia-smi over the run: utilisation 94 % median, graphics clock 2745 MHz (full boost), no throttle
  reason active 93 % of samples, 204 W of 285 W. Virtual Desktop: 3D ~10 %, video encode ~56 % (a separate
  engine). **The Task Manager "80 %" understated a saturated GPU.**
- depth 2 (trace seg02): queue occupancy 97.2 %, 8.54 ms per pair, the same. Removing the capture wait
  only let the render thread queue further ahead of a GPU that was already full - reading (a).
- res 70 % (trace seg10): the game's queue occupancy fell to 5.52 ms per pair and the 3D engine went 25 %
  idle, so at 70 % the limit moves to the CPU side (~7.6-8.0 ms per pair). Solving
  `occupancy = F + P * pixels` from 100 % (8.6) and 49 % of the pixels (5.5): **F ~2.5 ms per pair is
  resolution-independent, P ~6.1 ms per pair is proportional to pixels at 2750x2850**; it predicts
  12.8 ms at 130 % against 12.3 measured.
- This corrects the simulator-era reading that "the GPU cost barely follows resolution": in the headset,
  with Virtual Desktop on the card, ~70 % of the GPU time per pair follows the pixel count. The
  simulator is game-thread-bound and cannot show it (plan 1 in the simulator: 70 % did nothing).
- Trace overhead: the traced baseline D window ran 112.5 pairs/s against the plan's 115.2 for the whole
  segment (~2-3 %).

**Verdict.** The headset at 2750x2850 is GPU-bound on the game's own rendering, with the three CPU
stages close behind (~7.6 ms per pair at 70 %). Nothing of ours serialises it; capture depth and the
script-lane cadence stay default off / as they were. The routes that can raise the rate are GPU cost per
pair, above all per-pixel cost: the game's AA pass (MLAA is the VR preset) and bloom/light shafts, our
16x anisotropic override (the game's own default is 4x), the render size itself, and DLSS SR with the
fast model, now that the headset is pixel-bound (re-test; its earlier headset loss used the heavy M/L
presets). Sharing view-independent passes between the eyes targets F (~2.5 ms per pair) and the CPU floor.

### Follow-up: the game's MLAA is switched off while DLSS/DLAA is on (2026-09-27)

The VR preset (`[GameOptions] DefaultsAtStartup`, game_opts.cpp) wrote Antialiasing = MLAA at every launch.
It now writes OFF (profile id 122 = 0, `AntialiasingMode_Off` in ArkProfileSettings) when `[Clarity] DLAA`
is on - DLAA or Super Resolution already anti-alias, and MLAA is a full-screen pass in a headset shown above
to be pixel-bound - and MLAA as before when it is off. Decided at launch, through the same pre-apply profile
write the preset already used (a live switch would need the engine's settings apply called mid-session,
not reverse-engineered). The log's `gameopts/defaults: startup policy` line names the value and why.
Not yet measured: its rate gain with DLSS on (the MLAA cost was never isolated).

## 2026-09-27: AMD FSR 3.1 (FSR 4 where offered) through the DLSS helper - host and simulator verified

Branch `claude/fsr-upscaler` off staging. No Linear ticket: the workspace refused new issues (free limit).

### Route and why

The x64 helper already receives each eye's colour, depth (1 / (1 + z)), camera vectors (previous minus
current UV), the anti-smear mask and the projection jitter by shared handle and fence. FSR runs in the
same helper through AMD's FidelityFX API: `amd_fidelityfx_dx12.dll` (SDK v1.1.4, FileVersion 1.0.1.41314,
AMD-signed, MIT; `tools/fetch-ffx.ps1` pins commit and SHA256), or SDK 2.x's
`amd_fidelityfx_loader_dx12.dll` when it is placed beside the helper (FSR 4 on RDNA4). This supersedes the
earlier plan of an in-process 32-bit D3D12 port (the patched x86 SDK build above, never dispatched).

- IPC v3: the Hello carries the backend (DLSS or FSR) and an FSR version choice; the reply names the provider
  in use and every version the runtime offers; each frame adds frame time, vertical FOV and metres per depth
  unit for FSR's depth reconstruction (DLSS ignores them). Flags: non-linear colour (the game's gamma LDR),
  inverted + infinite depth, auto exposure. The anti-smear mask is passed as FSR's reactive mask.
- Levers: `[Clarity] Upscaler=0|1` (missing = DLSS; not in the default ini), `FsrVersion=0` (0 = the
  runtime's default), seam `dlss backend dlss|fsr`, `dlss fsrversion <n>`; F10 Basic "Upscaling and
  anti-aliasing (DLSS, FSR)" gains an Upscaler choice; the mode list is shared (DLAA reads "Native AA" under
  FSR) and the DLSS model list shows only for DLSS. The game's MLAA-off rule covers FSR too.

### Host test (`tools/dlss-host-test.ps1 -Fsr`, RTX 4070 Ti SUPER, no game): 13/13; full suite 30/30

- The runtime: FSR 3.1.4 (it also offers 2.3.3); start 300-430 ms.
- Per eye isolated, reset, 512 -> 768 upscale: all pass (errors 0.005-0.014).
- Motion vectors, production sign `kFsrMvSign = +1`: error 0.0007 against 0.0404 flipped and 0.0449 zero.
- Projection jitter, production `kFsrReportX/Y = -1, -1` (the same as DLSS): 0.0080, the best of the four
  pairs (+x+y 0.0196, +x-y 0.0206, -x+y 0.0120), against 0.0163 without jitter.
- GPU per eye at a 2750x2850 output: native AA 1.70-1.80 ms, Quality (1832x1900) 1.23 ms, Performance
  (1374x1424) 0.93-0.98 ms. DLSS on the same machine: K ~2.0-2.2 ms, the fast CNN ~0.9 ms (table above).

### Simulator (240 Hz, cap lifted, the same save; game-thread-bound, so rates are not headset predictions)

`dlss backend fsr` live from DLAA: helper restarted as FSR 3.1.4 in 472 ms, both eyes rebuilt, native AA
65 images/s per eye with no fallback and no refusals; Quality 110/s; Performance 126/s; upscaler off 155
pairs/s. Images clean at every mode (captures, mean luma 27.5 / 27.0 / 26.8). Helper GPU under load 4.3-5.6
ms (native), 2.2-2.8 (Quality), 1.8-2.0 (Performance) per eye - inflated by sharing the GPU with the game, as
DLAA's were. One untagged present held by the existing HoldUntagged guard 9 s after the switch (one
duplicate right eye), the one-present class already open under DLSS; the other stale-eye lines fell within
two seconds of a helper restart or resize.

### What the headset has to answer (plan `tools/perf-plans/fsr-1.txt`)

From plan 1 (~6.1 ms per pair pixel-proportional, ~2.5 ms fixed, CPU floor ~7.6-8.0 ms), FSR Quality should
land near the CPU floor (6.1 x 0.45 + 2.5 + 2 x 1.2 = ~7.7 ms of GPU per pair) and Performance below it:
predicted ~125-130 pairs/s against ~115 native at 2750x2850, if the cross-process contention stays near the
isolated cost. A loss like DLSS SR's earlier one (heavy presets, 3-4 ms per eye) would say the contention
dominates. FSR 4 needs an RDNA4 GPU and SDK 2.x's DLLs beside the helper (not fetched here).

### FSR 4: FidelityFX SDK 2.3.0 (same day)

The helper now ships SDK v2.3.0 (commit `60f4ea81`): `amd_fidelityfx_loader_dx12.dll` 2.3.0.2740 and
`amd_fidelityfx_upscaler_dx12.dll` 4.1.1.2740, both AMD-signed, hash-pinned in `tools/fetch-ffx.ps1`. The
upscaler provider offers FSR 4.1.1 on the GPUs that run it (RDNA 4) and FSR 3.1 elsewhere; the helper
passes the API-version descriptor 2.x expects (`FFX_UPSCALER_VERSION` 4.1.1) and picks the HIGHEST version
number offered by default (`[Clarity] FsrVersion=0`); F10 Advanced "FSR version" lists what the runtime
offers. Upscale descriptor layouts are unchanged from 1.1.4 (compared); the 2.x loader header fills its
function table only under `_WINDOWS`, which the helper build now defines (without it FSR would have had no
entry points).

- Host suite on SDK 2.3.0: 30/30. On this RTX 4070 Ti SUPER the runtime offers FSR 3.1.5 and 2.3.4, NOT
  FSR 4 (AMD restricts it to RDNA 4), so FSR 4 itself is untested here: an RX 9000 rig should log
  `[ffx] offers 1: 4.1.1` and `using 4.1.1` in `<data>\dlss\dlss_host.log`. Costs unchanged (native AA
  1.86 ms, Quality 1.20, Performance 0.96 per eye at 2750x2850).

## 2026-09-28: AlternateEye (VR-39) - an A/B, not yet measured

Built, host-verified, not yet run. `stereo aer` (F10 Advanced > Display > Stereo rendering)
draws ONE scene per game tick, alternating the eye, and pairs two ticks into one XR frame. The
prediction to test against reentry on the same save, spot and settings:

- Reentry: per displayed pair, one game tick and two scene renders. The headset traces measured
  the GPU about 97 % occupied under reentry (the uncap deep dive above), so if the GPU is the
  limit, AER renders the same two scenes per pair and should NOT raise pairs/s.
- AER without the clamp: per pair, two game ticks and two scene renders, with the engine's
  game-thread, render-thread and GPU stages overlapping as designed. If serialisation between
  those stages is the limit (the BioShock result: BRVR's AER ran 10-20 fps above the trilogy's
  re-entry on the same PC), pairs/s rises.
- AER with the clamp: the same work as without, plus the dilation writes (negligible).
- Falsifier: `stereo: beat` out/s and the `aer: beat` pairs/s at the same spot, reentry then aer
  then reentry again. A gain that does not return when reentry returns is not real.
- Read with it: `aer/clamp: beat` INTEREYE (the world's slide between the eyes of a pair),
  ghosting on walking NPCs with the clamp off and on, and judder (each eye refreshes at half the
  tick rate).

## 2026-09-28: AER measured (run 1) and AFW built

- Reentry 106-116 pairs/s against AER 99-115 at the same spot, 144 Hz, 2688x2880. AER's pair costs
  two world ticks (4.4-4.9 ms each); the render thread's idle per present rose from 0.4-0.6 to
  0.9-1.3 ms. The game thread is the limit under AER; the GPU side has about 1 ms per present of
  slack, so a higher resolution under AER costs little until that slack is spent.
- AFW sends every tick as its own XR frame (fresh eye plus the other eye's last image, reprojected by
  the compositor). Prediction: displayed frames per second rise toward the tick rate (about
  200/s available on this PC, capped at 144 by the headset) because one tick, not two, makes a
  frame; GPU work per displayed frame halves. Falsifier: `stereo: beat` out/s and the pair/frame
  rate at the same spot, reentry then afw then reentry.

## 2026-09-28: AFW headset run 2 - the headset rate reached

`stereo: beat method=afw out/s=144 L/s=72 R/s=72` at 144 Hz (reentry about 110 pairs/s at the same
settings). Reported: DLAA preset K without SSW at about 120 fps against 50-70 before. The
prediction held: one game tick and one scene render per headset frame. Each eye refreshes at half
the display rate; moving objects are a tick apart between the eyes; the held eye's parallax is not
corrected yet (FLICKER_REFERENCE, 2026-09-28 AFW entry).

## 2026-09-28: AFW run 4 - pacing and fallbacks measured, prior art, the two-source rebuild

Run 4 (`v1.0.1-162-g71e98fcae`, 144 Hz, VirtualDesktopXR, 2750x2850): `stereo: beat method=afw` 113-143
presents/s; `stereo: rate ... UNDER-SUBMITTING 0.78-0.87x` in the heavier stretches (display slots going
unfilled, not a throttled surplus); 155 `perf: frame gap` lines over the session, most waiting for the game
thread, some inside xrEndFrame (30-90 ms, `present-tail`). The held-eye warp itself fell back for 60-70% of
presents in long stretches (the depth ring, FLICKER_REFERENCE same date): each fallback is a one-frame swap
to a differently posed image, which reads as unevenness at any frame rate. `perf/depth: D3D9 copy mean
0.311 ms, peak 8.380 ms` for the shared depth.

Prior art read (online):
- PureDark's AFW (UEVR fork and RE Engine builds): renders one eye per frame and rebuilds the other from the
  alternate eye plus the previous frame; about 500 MB extra VRAM; the release notes say to aim for the
  headset refresh rate (frames below it make everything look worse) and not to run SteamVR Motion
  Smoothing, ASW or SSW with it; known artifacts are volumetrics between the eyes and hair edges
  (translucency without depth of its own). https://github.com/PureDark/UEVR/releases ,
  https://newreleases.io/project/github/PureDark/REFramework/release/RE9_AFW_v1.0-beta.4
- UEVR AFR/AFW ghosting-fix guide: under alternate eyes an eye's history is two game frames old while object
  motion vectors span one, so moving objects get half their motion; the fix doubles the object residual.
  https://gist.github.com/elliotttate/d0985ed09167529d7c04ea0c6679ecf6
- Oculus Stereo Shading Reprojection: one eye's colour reprojected into the other through depth by a
  backward full-screen pass, disocclusion holes re-rendered; about 20% saved in suitable scenes.
  https://developer.oculus.com/blog/introducing-stereo-shading-reprojection-for-unity/

Built from it: the two-source rebuild (FLICKER_REFERENCE and ARCHITECTURE, same date). Its cost is one
full-screen pass over the eye with up to six short searches per pixel in two R16F depth copies; the beat
line now carries its GPU time (`afw/warp: beat ... GPU x ms mean, y max`), to be read against the 6.94 ms
slot. Not built: submitting depth to the runtime (`XR_KHR_composition_layer_depth` is OFFERED by
VirtualDesktopXR) so its own reprojection could be positional; a slot filler for presents the game misses.

## 2026-09-28: AFW rebuild cost - review measurement and the seeded rework

The adversarial review measured the fixed-seed rebuild (145c03b5d) at 3.05 ms mean per present (4.13 ms
in an earlier batch) on this machine's RTX 4070 Ti SUPER, at 2750x2850. That is 34 dependent depth
fetches per world pixel. Moving the matrix inverse out of the shader alone saved about 20%.

The seeded rework, measured by the host test's cost case (GPU timestamps, 35 rebuilds at 2750x2850,
excluding the per-eye depth copies):

| Grid step | Seed maps | Mean per rebuild | Suite |
|---|---|---|---|
| 2 | half | 1.48-1.52 ms | 23/23 |
| 4 | half | 0.97-1.02 ms | misses a one-pixel hand ring |
| 4 | full | 1.32-1.65 ms | misses a one-pixel hand ring |
| 3 | full | 1.84 ms | misses a one-pixel hand ring |
| 2 | full | 2.47 ms | 23/23 |

Step 2 at half resolution is used. One reading of 2.43 ms at the same setting was taken while the GPU
clock was low, so read the in-game `afw/warp: beat ... GPU` figure for the real cost.

### 2026-10-02 headset follow-up: paged build 257

Tester reported stable behavior for this run. Installed DLL hash matches the recorded
candidate and the archived log banner is v1.0.1-257-g705b282c7. Six periodic snapshots
report zero backing-allocation failures, map failures, lock-table-full events and upload
failures. Last periodic totals: 31,800 translated textures, 484,530 locks, 293,683 uploads,
1,183 maximum simultaneous locks, 466.8 MiB live section backing and 0.00 MiB mapped at
that sample. Peak mapped footprint 327.25 MiB. The run reaches ordinary PreExit/device
teardown. No independent claim that an HD pack was installed is made from this report.

Last sampled cumulative mapping time is 681.58 ms; uploads total 11,630.74 MiB in
5,092.33 ms, with one maximum upload of 126.76 ms. These are lifetime totals spanning
loading and gameplay, not frame-time percentiles. Available system commit in the six
samples ranges from 1,269.4 to 2,108.2 MiB. Neither disk IO nor paging faults were measured.
The stability result therefore does not eliminate loading stalls or establish HD-pack
performance. Logs are archived in main build/texture-reshade-candidate/stable-run-257.
ReShade was absent in this run and installed afterward, keeping its next test separate.
### 2026-10-03 ReShade preset test armed, no performance result yet

The user-downloaded Carinth v3 preset is installed and selected with its seven active effects
(SMAA, LiftGammaGain, LumaSharpen, Vibrance, Curves, FakeHDR and prod80 contrast/brightness/
saturation). Official dependency commits and hashes are recorded in ../INSTALLER.md.
VR DLL remains accepted build 257; the entire VR INI is byte-identical and retains paged
texture backing. ReShade runtime/bridge are present; no HD texture pack was installed by
this work. Scroll Lock toggles effects. No game was launched, so neither shader compilation,
post-effect headset capture nor performance is accepted yet. The first question is whether
the toggle visibly changes the headset view. A later timed comparison can measure effect
cost, keeping the same save, resolution and VR mode; do not mix that with a texture-pack
install or assume mirror suppression still applies while ReShade is active.

## 2026-10-04: restored frame burst for native-stereo IK diagnosis

The AFW capture control was nested under Debug and the AFW method; it was not
visible in the tested reentry configuration. A Basic IK/Display control now
routes AFW to that existing capture, and other modes to 16 full-resolution
source-eye BMPs after a five-second delay. Native disk writes run on workers,
with at most 96 MiB and three outstanding pixel jobs. Busy omissions and
source identities are recorded, not presented as consecutive real-time frames.
Readback still waits on the GPU and can perturb the cadence being inspected;
no nonintrusive timing or throughput claim is made. It allocates/reads nothing
while idle. Capture times out after 15 seconds if output never becomes usable.
At 2750x2850x4, 16 raw images use about 478 MiB on disk before tiny headers.
The richer existing AFW capture remains much larger and retains its known
readback cost; it is not substituted for native stereo or silently enabled.

A standalone x86 D3D11 WARP host tests the production I/O path: 32 exact BMP
pixel checks across RGBA/BGRA, PNG compatibility and failed-write cleanup pass.
Game capture and perceptual effect are pending. Screenshot output is locally
ignored game-derived data. See ARM_IK and FLICKER_REFERENCE for experiment
identity and the one-question roll/capture launch. No new performance report.
