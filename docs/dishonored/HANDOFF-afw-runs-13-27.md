# HANDOFF: AFW (alternate-eye rebuild) - runs 13 to 27, two open faults (VR-39)

For the next agent picking up the AFW stereo method. Written 2026-09-29 at the end of a long session. Read this
whole file, then `docs/dishonored/FLICKER_REFERENCE.md` from the top down to "AFW run 11" (every run below has
an entry there with the measurements), then start on section 3.

## 0. The rules that bind this work (repo `CLAUDE.md` has the full set)

- Never merge to `staging` or `VR-Main` without the user explicitly saying "merge". Committing, pushing
  feature branches and opening PRs is fine.
- Never launch the game (not even the simulator) unless asked for one specific run. The user plays; you read
  the log and the captures. Build Release and install every change without asking (`tools\build.ps1
  -Release`, `tools\install.ps1 -Release`), backing up the installed DLL, the ini and the logs first (see 5).
- No person's name in anything pushed. Branches are `claude/...` or `codex/...`. No em dashes anywhere.
  Never quote the user's chat words in commits, docs or code; state the observation.
- Commits: conventional, imperative, subject <= 72 chars, no trailers. A fix and its docs are one commit.
  Every flicker result goes into `FLICKER_REFERENCE.md` in the same commit (newest entry at the top).
- Every render lever gets a live A/B switch (seam word and F10 checkbox).
- Force-pushing is blocked in this environment; fix mistakes with a follow-up commit.

## 1. Where everything is

### Branches (all pushed, none merged)

| Branch | PR | What |
|---|---|---|
| `claude/vr-39-afw-polish` | #159 | All AFW fixes of runs 13-26 (the rebuild shader, clean images, drawn mask, etc.) |
| `claude/vr-39-mod-spacewarp` | #161 | MSW (the mod's spacewarp) + a merge of afw-polish + run 25 (held hands follow controllers). **This branch is the superset; do new AFW work here** |
| `claude/vr-39-hands-world-fov` | #163 | Hands at the world FOV, `fgproj:` instrument's companion, `[Stereo] AfwForegroundGain` feed (0.911) |
| `claude/vr-39-dlss-object-motion` | #162 | DLSS object motion |
| `local/test-vr39-msw-objmotion` | local only | What is INSTALLED: all four merged. Build `v1.0.1-233-g6320bc59b` |

When afw-polish and MSW are merged, `afw_warp.cpp` conflicts at the constant buffer (see 4.3). The resolved
file on `local/test-...` / MSW is the reference.

### Worktrees (the main checkout `C:\dev\Dishonored-VR` is shared with other sessions - do not reset it)

- `build\worktrees\msw` - `claude/vr-39-mod-spacewarp`
- `build\worktrees\afwpolish` - `claude/vr-39-afw-polish`
- `build\worktrees\aer` - `local/test-vr39-msw-objmotion` (build and install from here)
- `build\worktrees\hwfov` - `claude/vr-39-hands-world-fov`

A new worktree needs `git submodule update --init --recursive` before `tools\build.ps1` will configure.

### Captures (local, never commit): `%LOCALAPPDATA%\DishonoredVR\dumps\afw-YYYYMMDD-HHMMSS\`

Taken from F10 "Capture AFW frames" (16 consecutive presents). Per present `pNN`:
- `pNN.txt` - header: `held=` / `fresh=` (eye indices), `matrixVerdict`, `yawDeg`, `fgFov`, `targetSize`,
  `freshMaskOk`, per-image records `fresh.*` / `heldrec.*` (pose, targets, vp, c5, rot, bodyYaw, and since run 25
  `hand0` / `hand1` = `ok px py pz qx qy qz qw` grips in tracking space), and each file's size/format line.
- `pNN_fresh.raw`, `pNN_held.raw`, `pNN_rebuilt.raw` - BGRA8 at the output size (2750x2850 on the dev rig).
- `pNN_fresh_clean.raw`, `pNN_held_clean.raw` - the game image before the mod's layers (run 15).
- `pNN_fresh_depth.raw`, `pNN_held_depth.raw` - R16F at the RENDER size (2114x2192 under DLSS Ultra Quality;
  2750x2850 with DLAA). SIGNED: negative = the drawn foreground mask says hands/weapon. Units: 1 unit = 2.315 m
  (`mPerUnit` in the header). 0 = sky.

`pNN_held` is the held eye's own previous frame, `pNN_fresh` the other eye now, `pNN_rebuilt` what the headset
showed for the held eye. The ground truth to compare a rebuilt frame against is `p(NN+1)_fresh` (the same eye,
one present later).

**Two captures from the end of this session are unanalysed:** `afw-20260929-234412` and `afw-20260929-234706`
(the wall and the head-sway faults below; order not confirmed - check the headers/images).

### Tools

- `tools\afw-replay.ps1 -Capture <dir> -Out <dir> [-Tint] [-OwnHands x] [-Fg deg]` - runs the PRODUCTION
  rebuild (`afw_warp.cpp`) on a capture and scores each rebuild against the next native frame of that eye.
  Env knobs: `DVR_AFW_CLEAN=0`, `DVR_AFW_STALE=<rel>`, `DVR_AFW_EDGE=0|1`, `DVR_AFW_STILL=0|1` (captures do not
  record controller stillness), `DVR_AFW_HELDHANDS=0|1`. Its "near band" and "world" scores are unreliable
  since the depth became signed (negative depth counts as "world"); compare images instead.
- `-Tint` colours each output texel by its source (class in `shade()`): green = hands from the fresh eye (0),
  untinted = held eye's world (1), blue = fresh eye's world (2), red = held fallback (3), yellow = held eye's own
  hands by the body hypothesis (4), magenta = disocclusion fill (5), cyan = near-miss (6), orange = held own hands
  kept for shading (7). A purple-ish overlay (x (1,0.5,1)) = held UI kept (run 15).
- `tools\afw-warp-host.ps1` - the synthetic host tests (44/44 on MSW). Test images encode SURFACE COORDINATES
  in rgb, not colours (so colour-based rules must be off there). Add a case with a control for every fix.
- Useful Python: read raws with numpy (`np.fromfile(...).reshape(H,W,4)[:,:,[2,1,0]]`), crop, upscale, PIL.
- Log lines to read: `afw/warp: beat`, `afw/warp: foreground from the DRAWN mask`, `fgmask: ON - N foreground
  candidates ... N drawn again ... pieces marked`, `afw/warp: clean sources`, `afw/warp: still-weapon shading`,
  `afw/warp: held hands`, `fgproj:` (the draws' own FOV), `stereo: rate` (presents/s).

## 2. How AFW works now (the parts that matter)

One eye is rendered per present (alternating). The runtime submits it as the FRESH eye and rebuilds the other
(HELD) eye from its own previous image and the fresh eye's image, at the fresh eye's instant
(`openxr_runtime.cpp`: `dvr::afw::note_capture`, `note_hands`, `warp_held`). Everything is in
`src\core\gfx\afw_warp.cpp` (HLSL in the `kSrc` string; `compose()` is the per-texel decision).

Per target texel, in `compose()` order:
1. Fresh eye's hands/weapon (`okF && bF`) - green. Sampled from the fresh eye's CLEAN image (run 15).
2. Run 22/23/25 rule - within 25% of either edge, or near the held eye's own foreground when the controllers are
   still or moved by grips: the held eye's own hands (body hypothesis, `solveHb`, moved by `handMove` when grips
   exist) where the fresh eye cannot see that point (outside its frame, or hidden behind something nearer).
3. Stale test and the held eye's world (`okH && !bH && !hH-free ...`), fresh world, near-miss, fill.
4. psmain: where the held eye's composed image differs from its clean image (its own UI), that is laid on top.
Before 1: the "own hands/still shading" rule (run 21): with both controllers still (`note_hands_still`, fed by
`AfwHandsStillTick` in `present_tick.cpp`: 50 ms, 6 cm/s, 12 deg/s), the held eye's own weapon pixels are kept
where the fresh eye agrees on the surface and depth, no colour test (fixes the shiny blade's reflection swap).

Foreground classification is the DRAWN MASK (`depth_probe.cpp`, fgmask): every foreground draw is drawn a
second time into a shared mask slot with a constant pixel shader, from inside `frame_hooks.cpp`
`orig_draw_indexed/prim`. A draw is foreground if it began, at the GAME's draw (`note_draw` in `hkDraw*`, before
the hands code widens the viewport), under a crushed-depth viewport (MaxZ < 0.5, >= 512 wide), or if the hands
code marks it (`fgmask_mark_piece` in `weapon_attach.cpp` and `mesh_split.cpp`). The mask is keyed by the
capture serial; `snapshot_depth` signs the depth with it (`psdepthk`). The old depth limit (0.30 units) is only
the fallback when no mask is ready.

Foreground projection: the arms render at the world's FOV (103, measured by `fgproj:`), but AFW must widen the
foreground's tangents by a fixed gain to match the native frames: `[Stereo] AfwForegroundGain=0.911`
(`tan(fg/2) = tan(world/2)/gain`, fed from `fov_lever.cpp` on the hands-world-fov branch). Replay-measured on two
captures at different camera FOVs; the physical cause is NOT known (most likely the foreground's depth in the
scene alpha is ~9% off its geometry). This gain is a prime suspect for BOTH open faults.

Seed maps (`vsmesh`): foreground seeds take the near half of the depth range so they beat the world (run 24).

## 3. The two open faults (the work)

### 3.1 Hands/weapon pushed into a wall still flicker, and "skew bigger the further in"

History: run 24 (`afw-20260929-223942`) measured wall 0.151-0.155 units, hands 0.119, blade 0.148-0.205 (behind
the wall surface but drawn on top). Fix: foreground seeds always win. Replay then showed the blade green but a
fill strip on one side. The next report (build 229) was contaminated by the run-26 stale-clean bug. This
report (build 233) is clean: still flickering, and the distortion GROWS with penetration depth.

Hypotheses to test first (in the capture, measure; do not guess):
1. **The foreground's depth inside the wall is wrong.** The scene alpha under the blade may be the wall's depth
   or a blend, not the blade's. The mapping `mapF`/`mapHx` uses depth x `prm.z` for foreground points, so the
   reprojection error grows with the true-vs-stored gap: exactly "bigger the further in". Check: histogram the
   signed depth of masked texels vs penetration across the new capture; compare with the hand's depth outside.
2. **The 0.911 gain.** It widens foreground rays; a foreground point at a wrong depth then lands farther off.
   A/B: `-Fg` in the replay (`-Fg 103` = no widening) and look at the blade.
3. **Seed ordering.** With the depth halving, foreground from the held image (mapped by the body hypothesis or
   grips) may beat the fresh eye's foreground at the wrong place. Tint the replay: which class the flickering
   texels are.
Candidate fix if (1) holds: a foreground depth that does not depend on the scene alpha - e.g. write the redraw's
own depth into the mask (a mask pass with its own depth-stencil and ZWRITE; INTZ is the D3D9 route to read
it), or clamp masked depth to the nearest grip distance +-reach (grips are in each record).

### 3.2 Moving the head left/right while looking at the sword makes the hands slide 1-2 inches the opposite way

Subtle, but it breaks the still hands and causes flicker. The world is steady. Hypotheses:
1. **Foreground depth/gain wrong -> wrong parallax.** A head TRANSLATION shifts near objects by
   `IPD-like baseline / depth`. If the foreground is reprojected with depth x 1/0.911 or widened tangents, the
   held eye's rebuilt hands get the wrong parallax and slide opposite to the head. Test: in the new capture,
   compare the rebuilt hands against the next native frame while the head translates (`fresh.pose` positions
   in the headers); A/B `-Fg` and the gain.
2. **The body hypothesis / still rules** (`solveHb`: held hands fixed in TRACKING space). Correct for a head
   translation only if the hand depth is right; same root as 1.
3. **Grips vs head space.** `handMove` (run 25) uses grips in tracking space; if the grip "then" and "now" are a
   generation off, a head move reads as hand motion. Log/compare `heldrec.hand0` vs `fresh.hand0` in the headers
   during the sway (they should be ~equal when the hands are still).
A/B levers in F10 / seam: `afw stillshade off`, `afw edgehands off`, `afw heldhands off`, `armslens gain 1.0`
(hands-world-fov branch; 1.0 = no widening). Ask the user for ONE headset A/B at a time with a named question.

## 4. Gotchas this session paid for

1. **The Bash tool turns `\\n` inside heredocs into real newlines.** Edit HLSL strings (which contain literal
   `\n"`) with the Edit tool, or write a Python script to a file with the Write tool and run it.
2. **CRLF.** Most files are CRLF; normalise in scripts (`s.replace('\r\n','\n')` then write back).
3. **The constant buffer is declared in three places** and must agree: the HLSL `cbuffer P` (field order), each C++
   `struct CB` (warp_held and synth_eye) with its `static_assert(sizeof(CB) == N * 16)`, and
   `bd.ByteWidth = N * 16` in `init()`. A too-small ByteWidth silently zeroes the new fields (run 15 cost an hour).
   On MSW it is 46 rows (prm8, prm9 after MSW's hand rows, prm7, mY).
4. **Render-size changes** (DLSS on/off) reset the device; anything cached by size must re-read the real size
   (run 26).
5. **The hands code widens the crushed viewport** (depth-range lever) before it draws: classify foreground at the
   game's draw, not later (run 19). **In some places the game draws the held weapon with the full depth range**
   (run 20) - that is why the hands code marks its own draws.
6. **The replay's depth size differs from the colour size under DLSS SR** (2114 vs 2750); the replay handles it,
   ad-hoc Python must too.
7. `IsLiveObject`, not a class-name compare, is the liveness test (repo TRAPS).
8. The user runs 144 Hz; never suggest lowering it.

## 5. Install ritual (every build)

```
cd build\worktrees\aer                       # local/test-vr39-msw-objmotion
git merge --no-edit claude/vr-39-mod-spacewarp
powershell -File tools\build.ps1 -Release
powershell -File tools\afw-warp-host.ps1     # must be all PASS
# back up: d3d9.dll, dishonored_vr.ini, dishonored_vr*.log -> build\playtest-candidates\vr-39-aer\pre-install-<stamp>\
powershell -File tools\install.ps1 -Release
# verify: installed d3d9.dll == build\src\RelWithDebInfo\d3d9.dll; ini unchanged vs backup (or report each change)
```
Game dir: `C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32`. The log banner shows
`build v1.0.1-NNN-g<sha>` - check it before trusting a run.

## 6. What was done this session (runs 13-26), one line each (details in FLICKER_REFERENCE)

- 13: GObjects search on the ProcessEvent path cost 1/4 of the frame rate; bounded, then removed.
- 14: `fgproj:` measured the arms at the world FOV; AFW foreground feed = world FOV widened by gain 0.911.
- 15: objective text/F10 on the other eye's sword - clean images before the mod's layers; held UI laid back.
- 16: game markers were in the game image (marker redirect refused without DLSS SR) - redirect at render size.
- 17-20: foreground by DRAWN mask (redraw of foreground draws), classified at the game's draw, hands code marks
  its own draws. Fixed the close-NPC face split, the far blade, the spot flicker.
- 18: stale test 0.03 -> 0.015 (NPC walking in front of a wall trailed).
- 21: still weapon keeps each eye's own shine.
- 22-23: hands at the frame edges / hidden from the other eye keep the held eye's own.
- 24: foreground seeds beat the world (weapon in a wall) - NOT sufficient, see 3.1.
- 25 (MSW branch): held eye's hands moved by their controller grips.
- 26: clean-image rotation survived a render-size change (DLSS toggle doubled the sword until restart).
- OPEN besides 3.1 and 3.2: turning with the stick uncovers world beside the hands that neither image has
  (fill smear); needs a third source (a reprojected background history). MSW (half-rate + extrapolation) showed a
  one-frame world ghost on stick turns; suspect the turn extrapolation (no clamp, up to 382 deg per slot).
