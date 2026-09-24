## Support logs and history (VR-215)

- [x] Reproduce released launcher error 3 with a fresh temp parent.
- [x] Recursive staging with app-data fallback and UTF-8 output paths.
- [x] Ten sessions retained; failed rotation preserves current evidence.
- [x] Compressed ZIP cap, newest-first history and marked header/tail excerpts.
- [x] Real helper, Windows PowerShell 5.1, locked/missing files and size tests.

## Launcher updates and GOG support (VR-214)

- [x] GitHub startup update prompt, verified download and EXE replacement.
- [x] About update check and cached recent release history.
- [x] Default INI reset with warning, backups and partial-update rollback.
- [x] Steam/GOG discovery, home folder selection, Win64 rejection, Galaxy command.
- [x] Host tests, real GitHub download, helper handoff and native screen previews.
- [ ] Actual GOG Galaxy launch acceptance on a GOG installation.
- [x] Public release download, checksum/version verification, helper replacement
  and exact embedded-payload installation into a scratch game folder.
- [ ] Interactive update from installed 1.0.1 to a future published 1.0.2.

## 1.0.1 FOV hotfix (VR-213)

- [x] Reproduce persistent downward feedback and verify a resolution-independent correction.
- [x] Host regression: interpolation, delayed readback, zoom, scopes, owner lifetime.
- [x] Optimized build and installation with full INI comparison (byte-identical, CRLF).
- [x] Simulator: gameplay, settings chord, pause/resume, stable 108.07-degree FOV.
- [ ] Headset: stable full view for 30 seconds after loading at unchanged resolution.
- [ ] Headset: spyglass zoom/recovery and cinematic transitions.

## F10 painted UI (VR-206)

- [x] Original reusable PNG materials embedded in the DLL.
- [x] Native controls with square default, scrolling pages and extensible helpers.
- [x] Native preview and pointer interaction verification.
- [ ] Headset layout/controller acceptance after explicit install approval.

## Deferred rain TODO (VR-205)

- [ ] Optional outdoor rain visible from awnings while keeping shelter dry. Requires
      roof-aware filtering; a global shelter bypass is not sufficient.

## Positional falling-rain investigation (VR-202, 2026-09-22)

- [x] Trace decompiled ownership and native camera shelter versus independent impact paths.
- [x] Add read-only shelter/particle-consumer diagnostics with bounded logs.
- [x] Match the stationary pitch reproduction: sustained absence with positive MaxParticles weakens shelter.
- [ ] Measure actual particle count, render bounds and render time across pitch changes.
- [ ] Fix a confirmed defect, if the absence is not expected shelter behavior.

## F10 improvements (VR-199, 2026-09-22)

- [x] Remove reticle hand selection and beam, with old values normalized.
- [x] Themed click/hold controller hint and Reset to Defaults beside Save as Defaults.
- [x] Reset host verification, including persistence and safe failure paths.
- [x] Basic rain toggle targets camera box plus identified rain lens particles.
- [x] Headset: lens-only close overlay disappears and returns; template Over_camera_rain_01 confirmed.
- [ ] Headset: F10 tiers, save-on-change and Reset to Defaults across relaunch.

## The sword's swing trail (VR-171, 2026-09-21)

- [x] Probe: is it an anim-trail notify? No: 0 `TrailsNotify` from anyone in 4 sword attacks, with the names in the table.
- [x] Component census: the attack adds one particle component to the pawn, template `Sword_Trail`, 282-290 ms in; it stays attached between swings.
- [x] Hide with the engine's native `SetHidden`, default on, live `swordtrail on|off` and an F10 checkbox; pooled-component reuse handled.
- [x] Simulator: `trail-hide.xrs` passes (found, hidden 0 -> 1, held through three attacks, shown and re-hidden by the lever).
- [x] Fixed: with the lever on from launch the component was hidden and then forgotten one scan later (too new for the 2 s live-object table), so the lever could not show it again. Liveness for an attached component is now the pawn's own list on that scan; leg 0 of the sequence is that path.
- [ ] Headset: the ribbon is gone on swings and enemy trails are unaffected. The simulator could not show the ribbon even with the hide off, so this box is the only visual evidence there will be.
## The game's own camera shake (VR-172, 2026-09-21)

- [x] Instrument: `camshake capture`, one row per game tick, the game's camera motion with the mod's own offset removed. Floor 0.00 / 0.00 / 0.06 uu.
- [x] Attribution by holding one handle at zero: landing dip 45.8 uu = `PhysicalReact`, pistol kick 2.84 deg = `Recoil`, push-off lag 10.4 uu = `BumpSmoother`, bob and roll = two camera floats (already 0 through the head-bob option), `m_fReactionWeight` a master over the group. Each proven live by exaggeration.
- [x] The feature: master + six categories, default removed (smoother kept), F10 section, `camshake` word, status.json, stand-down in cutscenes.
- [x] Simulator: `camshake.xrs` passes (5 A/B legs); `headlook.xrs` and `swing-edge.xrs` pass with it on.
- [ ] Headset: walking, firing, landing; and the three the simulator could not reach - damage taken, a sword landing on an enemy, explosions. A knockdown and camera collision near walls must still behave with `Landing` removed.
- [ ] The 1.5 uu walking swing and 0.4 uu idle sway that no handle owns (the animated body the camera rides on): VR-175.

## Motion sword (VR-37, 2026-09-20)

- [x] Measure the old detector on the simulator: 0 attacks from three swings, every gate open.
- [x] Pure decision core (edge + sustain) with 36 host checks; adapter, gates, pulse, honoured-check.
- [x] `swing` word, `features.swing` status, F10 rows, default ini (parity gate passes).
- [x] Simulator: `swing-edge.xrs` and `swing-gates.xrs` pass; rb output reads NOT HONOURED as it should.
- [x] Headset (dev PC, VDXR, 3012x3122): 47 swings, 46 honoured, 0 refused, 2 stealth kills; nothing to change. Default flipped to `edge`.
- [x] VR-155: the thrust detector, armed off the crouch capsule; 16 host checks, `swing-stab.xrs` passes.
- [x] VR-155 headset: `HONOURED kill` twice (through the slash detector: a kill plunge in earnest is over 3.6 m/s); armed 18 times, no stray attack.
- [ ] VR-155: a SLOW plunge (under 3.6 m/s) producing the kill in the headset - the stab detector's own positive case, simulator-proven only.
- [ ] VR-156: a readable kill-available signal, for arming on it and a haptic ready cue.

### The threshold and the hump census (VR-170, 2026-09-21)

- [x] Hump census and travel guard in the pure core; 75 host checks (15 new). Measured there: a real swing has travelled 0.15 m when it crosses the threshold, so the travel guard ships OFF.
- [x] `EdgeSpeed` 3.6 -> 3.0 with a one-time per-ini migration (`EdgeSpeedRev`), no config version bump. Dev PC ini (held 3.60): line on the first launch, none on the next.
- [x] Simulator: `swing-soft.xrs` passes (attacks at 3.0, NEAR MISS at 3.6, guard delays and never refuses, the census ignores `swing sim`); `swing-edge.xrs` and `swing-gates.xrs` still pass.
- [x] A hump cut by a tracking gap is reported `CUT SHORT`, found when a simulator hitch made one vanish.
- [ ] Headset: soft swings register, walking / turning / reaching does not attack; the census from that run decides whether 3.0 stays and whether `EdgeTravelM` gets a value.

## Accepted startup preset; chain remains open (2026-09-20)

- [x] Default-on startup preset for the ten approved game options; later edits allowed.
- [x] Head bob off on startup and sound working on retest;86 host checks pass.
- [x] PR86 ready for review; not merged.
- [ ] VR-165: chain-camera displacement root cause and fix remain OPEN.

## Native settings apply candidate (2026-09-20)

- [x] VR-161: verified direct native dispatch, open pause/listener gate, no raw fallback.
- [x] VR-161:42 host checks and Release/lint/export validation; candidate installed.
- [x] VR-161: startup head bob off accepted; saved F10 startup-default toggle.

## Option and chain source review (2026-09-20)

- [x] VR-161: fix nested menu enumeration and raw profile write validation; 17 host checks.
- [x] VR-165: remove invalid radius inference; measure the separate influence graph.
- [x] VR-161: native setter mapping verified; ten startup profile targets match.
- [ ] Further per-setting visual acceptance is distinct from profile verification.
- [ ] VR-165: correlate reproduced displacement with camera sources, then fix its owner.

## Misc fixes (2026-09-17)

- [x] VR-134: animation-state arm checkboxes implemented with host coverage.
- [x] VR-133: lean trace, decompiled special states and crash allocation boundary inspected.
- [ ] VR-133: headset validate special-camera candidate; resolve allocation failure.
- [ ] VR-134: headset validate arm checkboxes.

## Controller emulation (2026-09-17)

- [x] Merge accepted marker work via PR72; branch from updated main.
- [x] Inspect BioShock1 modifier/menu policy and stock Dishonored mappings.
- [x] Add F10 Controls, pure composer, saved settings and221 host checks.
- [x] Headset: modifier D-pad selection, menu scrolling and reading attachment accepted; PR73 merged.

# Roadmap

## VR-129 HUD orientation and closing lifecycle

- [x] Create new branch/ticket from merged PR70 and preserve391 evidence.
- [x] Export native wheel/HUD Scaleform, textures, structure and authoring previews locally.
- [x] Implement guarded native-upright and animation-length visual lease candidates.
- [ ] Headset: objective upright tracking and residual wheel closing flash.
- [x] Separate potion/shortcut panels with tuned source crops and shared alpha; headset accepted.
- [x] Rounded wrist ends and latest full F10 profile accepted (VR-130).
- [x] Native objective/rune parent placement and bounded icon continuity accepted (VR-129).

## VR-50 F11 clarity follow-up (2026-09-15)

- [x] Trace fullscreen reset and narrow-FOV feedback; preserve findings in PERFORMANCE.md.
- [x] Build/install default 90-degree gameplay candidate with standalone validation.
- [x]100-degree gameplay FOV and live F10 slider headset-accepted; default promoted.
- [x] Add F10 total-pixel scale/preview/Set; install110% pixel trial.
- [x] Record hub mirror-off improvement; promote102 FOV/120% pixels/mirror-off defaults.
- [x] Add guarded engine-owned live resize and production host tests.
- [x]357 live engine resize confirmed by one Reset and matching capture.
- [x] Fix idle F10 Display FOV writer; old-code negative control fails, corrected control passes.
- [ ]359 headset check for zoom-pulse removal at103 FOV/130% pixels; no main merge authorized.

- [x] Build/install automatic mirror-off pair-pacing A/B/A;26 host checks pass.
- [x]362 pacing result recorded; baseline drift and laggier report; benchmark disarmed.
- [x] Build/install strict mirror-off candidate363 with native GPU host validation.
- [x]363 headset accepted;621 strict windows have zero desktop Presents.
- [x] Promote complete saved profile and rename performance improvements branch.
- [ ] Open performance PR; prepare local combined PR67 crouched-pitch playtest.

## HUD Weapon Dial (VR-126)

- [x]387 crouch transition grouping accepted; gamma color correction provisionally accepted.
- [x]387 wheel hand stability improved enough to park; retain menu half-step setting.
- [x]389 pause world stability, reader rotation and wheel origin accepted; closing flash provisionally absent.
- [ ] Menu exit yaw retention and native objective title/distance association trial.
- [ ] Pause hand apparent-size change: collect scale/depth diagnostics.

- [x] Continuous wheel analog input and independent world-space hand dial implemented.
- [x] Standalone dial geometry/input and existing HUD regression checks pass.
- [x]372 hand dial accepted and physical size/crop tuned.
- [x] Tiny-motion direction, camera-plane orientation, circular crop and distance controls.
- [x] Scoped menu head look and per-menu UI blur candidate with host/GPU validation.
- [x]374 wheel appearance/selection accepted; motion-dependent camera issue reported.
- [x] Scoped single-draw and translation-frame corrections pass host negative controls.
- [x] Independent wheel alpha and hand-following note/journal controls implemented.
- [x]376 positional camera slide appears fixed; readers accepted except horizontal alignment.
- [x] Add reader right-offset sliders, bounded menu stereo gap hold, private measured HUD textures/content continuity.
- [x]378 reading flicker removed and wheel greatly improved; accepted for merge.
- [ ] VR-128: remaining left-eye hands/weapons flicker during wheel head motion.
- [x] PR69 merged; saved378 profile promoted; codex/hud-fixes created from main.
- [x] VR-127: shared reading/interaction alpha, isolated original general reset, continuous reading vertical input.
- [x] VR-127: default-off moving-marker/interaction grouping candidates and host regressions.
- [ ] VR-127 headset: complete interaction grouping, objective identity, reading scroll and alpha acceptance.

## Performance research shelved (2026-09-15)

- [x] Consolidate findings and reusable default-off tools; preserve source branches.
- [x] Complete query-helper and classified per-eye preparation measurements.
- [x] Remove failed nonblocking Present and invalid coarse CPU-time output.
- [x] Park unfinished performance tickets in Backlog, unassigned.
- Broader work remains shelved; VR-50 exception above. Routes and evidence: [PERFORMANCE.md](dishonored/PERFORMANCE.md).

## World head-motion stability (2026-09-14)

- [x] VR-116 image-linked world orientation confirmed and reconfirmed at 120 Hz.
- [x] Exact accepted INI promoted, including Pace.ImageOrientation=1.
- [ ] VR-95 residual left-eye hand jump after the opening cutscene, deferred.

## Weapon lens follow-up (2026-09-14)

- [x] VR-112 crossbow unsheath/tracking recovery confirmed on build260.
- [x] VR-112 residual crossbow surfaces: build264 headset-confirmed; inverse-lens consistency accepted.

## Load/reload stability (2026-09-13)

- [x] Startup stereo without vertical movement, headset-confirmed.
- [x] Fast notes survive save reload, observer lifetime repaired and confirmed.
- [x] Adjacent R/0 capture repair, host checks and headset reload stability.
- [x] VR-96 crawl-release stale write rejected, regression and headset confirmed.
- [x] Complete installed settings/F10 profile promoted to generated/package defaults.
- [x] VR-70 cinematic head look, natural pitch and native border control: headset-confirmed; PR54 merge authorized.
- [x] VR-103 dialogue/cinematic stereo accepted; later post-load handoff refinements accepted in PR58.
- [ ] Remaining weapon startup freeze: separate timing work, name cache still off.

## VR-57 native launch (2026-09-12)

- [x] Derive and byte-verify the native crossbow pre-spawn direction consumer.
- [x] Implement guarded hand endpoint convergence; validate x86 bridge and geometry offline.
- [ ] Confirm launch behavior during user play; no runtime verdict yet.

## VR-57 hand aiming (2026-09-12)

- [x] One ray from the runtime's AIM pose, with its own mapping check.
- [x] Dot and beam drawn from it; both controller poses drawable for comparison.
- [x] The game's aim-assist cache found, read, and written from the ray.
- [x] Measured: the ray maps exactly; the grip pose points 74-84 deg up; the
      layer budget draws every point.
- [ ] The beam does not lie along the controller in the headset - test the
      compositor-versus-world alignment (head-anchored control dot, FOV audit,
      submitted view pose) before doubting the ray again.
- [ ] The aim assist clamps shots back toward its own crosshair; decide whether
      to drive its input, widen it, or disable it per the tester's choice.
- [ ] The weapon model's barrel axis, so the model points along the ray.

## VR-57 visual aiming guide (2026-09-11)

- [x] Review the prior failure and preserve one ray for dot and beam.
- [x] Implement fixed-distance XR visuals, F10/seam controls and renderer outcomes.
- [x] Host-test ray/compositor paths, build x86 and install with MotionAim off.
- [ ] User headset test: guide appears and follows the selected controller.
- [ ] Separate static crossbow-barrel alignment test before trace/projectile work.

## VR-76 mirror correction (2026-09-11)

- [x] Reproduce delayed-tag eye switching offline and implement current-draw pin.
- [x] Validate policy, actual copy module, 32-bit build and standalone simulator.
- [x] Install candidate with only DesktopEyeSource=draw added to the tested ini.
- [x] User test with single-draw bursts and counterfactual raw leaks (1,400 ticks, 1,397 shadow leaks, 0 failures).
- [x] Headset play through the prologue to the hub: no remaining jump; `draw` is the default.

## VR-69 stability correction (2026-09-10)

- [x] Restore render-side weapon eye correction without the live script mono reset.
- [x] Preserve camera offset ownership through the mod's downward Z clamp.
- [x] Verify both changes in controlled headset builds, including weapon swaps
      and downward movement; preserve the confirmed DLL and configuration.
- [ ] Verify a newly packaged mainline integration and the subsequent reticle-aim
      setting. The source merge is not a release or a new-binary headset verdict.

41.0 restarts the render on a native D3D9 game: the DXVK fork and the side-by-side pipeline
are gone, one OpenXR runtime layer serves every headset, and stereo is rebuilt as a LADDER of
methods on one seam (docs/ARCHITECTURE.md, "The stereo ladder"). Every milestone below is
"done when" a MEASURED effect holds, not when code lands; the simulator runs first
(docs/VERIFICATION.md), the headset last. Two developers take the two stereo methods (S2a,
S2b) on the same foundation; S3 compares them and picks.

## S0 - Foundation for native stereo (session 5, 2026-09-02) - delivered

- [x] Removed, one commit each so `git revert` restores one piece: the DXVK fork tree and
      tooling; the fork bridge in the proxy; the side-by-side present pipeline and the quad
      geometry; the wide-window machinery (4032x2268 spoof, user32 hooks, mode injection,
      setres); the OpenVR backend; the mod's own OpenXR loader/pace thread/input; the state
      chunks and ini keys that only served them (RELEASE_NOTES "Upgrading")
- [x] The static OpenXR loader linked into `d3d9.dll`; the BioShock runtime layer
      (`core/vr/openxr_runtime`, `openxr_input`) as the single backend behind two D3D9-host
      seams (device provider, frame texture); `dvr_steamvr32.dll` (the SteamVR shim) built and
      shipped with `openvr_api.dll`
- [x] The stereo seam (`core/gfx/stereo`: `[Stereo] Method`, `stereo <name>|status`) with the
      mono screen working (capture -> D3D11 -> head-locked quad, both eyes) and `aer` /
      `reentry` registered as design stubs that refuse with their note
- [x] The per-eye camera seam (`game/dishonored/camera`) with the eye-offset write-point
      instrument `camera eyetest` and ENGINE_NOTES "The per-eye camera seam: write points"
- [x] `core/framework/frame_hooks` a real module owning the D3D9 hooks and the frame path's
      order; `core/gfx` born as real modules (stereo, capture, blit, mono, stubs)
- [x] Instruments: the capture's non-black bbox line; the simulator's per-eye SOURCE stats,
      the black-eye discriminator, pose/fov validation at `xrEndFrame`, `stats.bboxL/R`,
      `mono.xrs`; eye-check leg 0 on the `stereo: beat` line; `xrsim-launch.ps1 -ViaSteam`
- [x] Verified on the dev PC (2026-09-02, runs 7-11): `xrsim-selftest` PASS, `xrsim-launch
      -ViaSteam` reaches `xr: pipeline READY`, `mono.xrs` passes (both eyes non-black, equal
      bboxes, head-locked under yaw), `camera eyetest` run in gameplay with its verdicts in
      ENGINE_NOTES (0x330 HONOURED), `stereo aer|reentry` refuse and mono keeps running,
      crash file and status.json intact, `soak.ps1 -Minutes 3` exit 0
- [x] Verified in a headset (user, Quest 3 via VDXR, 2026-09-03, build `g4cae928b`): the game
      on a head-locked screen in both eyes, head rotation turns the view, the gamepad works;
      `xr: instance created on runtime 'VirtualDesktopXR'`, 2496x2688 per eye recommended,
      `xr: pipeline READY`, 68 presents/s at 1920x1080. The quit crashed (below, fixed).

Done when both verification lines are ticked; the PR carries the removal list and the
results.

## S1 - The mono screen accepted in the headset

- [ ] The headset run above signed off: readable screen at `[Screen] DistanceMeters` /
      `WidthMeters`, no judder at the game's frame rate, `[VR] FpsCap` cadence chosen
- [x] The capture cost measured and cut (2026-09-03, runs 16-19): the shipped path costs
      ~5 ms per present at 1080p, all of it `LockRect` waiting on the queued readback;
      `[Capture] Mode=deferred` (queue the readback, lock it one present later) is the cut:
      2.3 ms, `mono.xrs` passing (ENGINE_NOTES "The capture cost, measured"). Session 8:
      deferred SHIPS as the default; the readback's GPU side measured (16 ms of DMA per
      present at the Quest 3 size) and removed by the 9Ex shared path behind `[Device] Ex=1`
      (ENGINE_NOTES "The tick budget, measured")
- [x] `camera eyetest` verdicts recorded: camera+0x330 HONOURED (it holds the POSITION and
      c5 is its negation - the sign corrected 2026-09-03 by picture, bbd04fec), the five
      others DISCARDED (ENGINE_NOTES)
- [x] Positional (lean/crouch/roomscale) tracking on the camera seam's position write behind
      `[PosTrack] Lane=vp|camera` (default vp): `camera postest` HONOURED on all three axes
      within 1-2 % on the camera lane (run 20; ENGINE_NOTES "Positional tracking on the
      camera seam"); the vp lane's matrix effect is not c5-measurable, its APPLIED count is
- [ ] `head_track` and `pad_bridge` converted to real modules (the D1-era refactor step
      that S0 deferred)
- [ ] SteamVR rig confirmed through the shim (`xr: runtime "DishonoredVR SteamVR shim (OpenVR)"`)

Done when a tester plays a level on the mono screen and calls it comfortable.

## S2a - AlternateEye (rung 2; developer A)

`core/gfx/aer.cpp` carries the design. Acceptance, in order:

- [ ] `stereo aer` accepted (needs `[Camera] EyeField` from the eyetest); the beat line reads
      `L/s == R/s == out/s / 2`
- [ ] `stereo.xrs` on the simulator: two projection views, `EyeSeparationM` == IPD, left vs
      right `img-diff` well above the noise floor with parallax on near geometry
- [ ] eye-check.ps1 legs 0-5 PASS; the runtime's pair probe reports no untagged presents
      (the stale-left class)
- [ ] Headset: fusion at the measured IPD, no swim on head turns; half-rate per eye judged
      acceptable or not (write the verdict)

## S2b - SequentialReentry (rung 3; developer B)

`core/gfx/reentry.cpp` carries the design. Acceptance, in order:

- [x] The scene-draw root found and byte-verified (2026-09-03, runs 26-27: the caller
      census, the live stack scrapes, pe-xref confirmation, `reentry pulse` as the mover);
      `kViewportDraw` and its call site in `patterns.h`, ENGINE_NOTES "The scene-draw root,
      derived live"
- [x] The second draw through a patched call site (deny-by-default by construction, the
      return address checked), SEH-guarded; a fault poisons the method for the session and
      the game runs on mono
- [x] `stereo reentry` accepted; presents = 2x ticks (106 vs 53); the beat line reads
      `L/s == R/s == out/s / 2`; the second draw costs 220-470 us (run 28-29)
- [x] `stereo.xrs` and `reentry.xrs` on the simulator, eye-check legs 0-1 (legs 2-5 carry
      BioShock's bands: KNOWN_ISSUES); the pair line proves the two cameras half an IPD apart
- [x] Headset: fusion confirmed (user, Quest 3 via VDXR, 2026-09-03, run 40) - the world
      reads in 3D, head tilt, lean, look and crouch all correct. Merged to
      `native-stereo-rendering` (PR #3, 3be4a0c4), which is the working branch from here.
- [x] The four faults run 40 left open, each fixed or levered on the simulator (session 7,
      2026-09-03): the desync (the gates decided once per tick; `vrpace strict` off; the
      `STALE R EYE` line; `reentry skip2` reproduces it), the judder (the pair phase measured;
      `vrpace ahead` 0..2 ships at 0), the pitch pivot (the engine's own neck measured at
      0.321/0.062 m; `[Neck] Mode=cancel` cancels it, ships off), the F10 tickbox (ticked,
      `reentry` the default) and the picker (the command-line route, VirtualMode: 2496x2688
      honoured on the simulator). Headset verdicts pending (STATUS "Next steps").
- [x] The headset run on session 7's build (the user, 2026-09-03, runs 13a/b): the picker
      WORKS and is sharp at 2496x2688; `neck cancel` is RIGHT (now the default); the desync
      still recurs on load and after some pause/resumes (one stale-left submit at a FOCUSED
      regain, owner unnamed); the judder could not be judged: 28 ticks/s at the eye's size
- [x] PERFORMANCE, on the simulator (session 8, 2026-09-03): the tick budget measured (the
      readback owns the tick on the CPU and the GPU: 16 ms of DMA per present at the Quest 3
      size against 5 ms of 3D per draw), the creation census (99 % MANAGED, READONLY streaming
      locks), the game's device as D3D9Ex with the managed-pool shadow (`[Device] Ex=1
      Managed=shadow`, off by default), the fenced two-slot shared capture (75-90 ticks/s at the
      Quest 3 size, pace-bound), `deferred` shipping as the default, `mark` and the F10 MARK
      button, the richer gap line; the pace guard's eaten tag named on the STALE line and the
      no-frame tag fixed; a simulated focus loss did not reproduce the regain desync
- [x] PERFORMANCE, on the headset (the user, 2026-09-03, run 15): `[Device] Ex=1` + `capture mode
      shared` judged good at the Quest 3 size and made the defaults
- [x] THE ONE-VIEW STATE, the instrument (session 9, 2026-09-04): the frame-identity trace
      (`core/gfx/frame_id`, `[Perf] FrameId=1`, the `stereo: frameid` line: a 64x64 thumbnail of
      every present at the backbuffer, the shared slot, the eye texture and the swapchain image,
      the c5 step between the two draws, the picture's own parallax sign), `reentry rearm [n]`,
      `capture reinit`, `dump eyes` as a pair encoded off the present thread (a dump used to
      re-arm the doubling), the presenting thread followed live, pass-2 write refusals counted
- [x] THE EYES SWAPPED, found and fixed on the simulator (session 9): the tag ring's order broke
      across single -> double transitions, within a second of an arming and spontaneously in
      gameplay (the A/B: twice in 25 s with `reentry c5pair off`); the pairing follows the
      within-tick camera step now (`[Stereo] C5Pair=1`), the ring realigned when it disagrees;
      `reentry.xrs` 11/11 on the fixed build
- [x] THE EYES, on the headset (the user, 2026-09-04, runs 07-08): RIGHT from the load and
      through every word on the F10 EYES block; and the A/B proves the cause - `c5 pairing`
      unticked plus a pause/resume brings the fault straight back (24 of 25 pairs swapped, the
      picture agreeing), ticking it on clears it (0 swapped for the rest of the run). The
      per-eye ladder's correctness question is CLOSED
- [x] The headset-judged values are the defaults (2026-09-04): `[Stereo] Method=reentry Armed=1
      C5Pair=1`, `[Camera] EyeField=0x330`, `[Neck] Mode=cancel` with the measured pivot,
      `[PosTrack] Scale=98`, `[Tracking] HeightOffsetM=-0.090`, `[Screen] RenderWidth=2496
      RenderHeight=2688 VirtualMode=1`, `[Device] Ex=1 Managed=shadow`, `[Capture] Mode=shared`
- [x] THE GHOSTING, instrumented (session 15, 2026-09-04): the SCRIPT lane's rendered head
      sample against the PRESENT lane's layer tag, per eye, in degrees and in locate
      GENERATIONS, with a self-checking sign calibration (`vrpace poseaudit on`). Built and
      installed, not yet run. Written prediction: gap +1 at `lag 1`, nulled by `vrpace lag 2`,
      because the game's `OneFrameThreadLag=True` puts the render thread a frame behind
- [x] The content-bbox readback gated (session 15): a full-frame CPU round trip every 3 s on
      the present thread in the shipping `shared` mode, with no lever. `[Capture] BboxMs=30000`,
      `capture bbox off|<ms>` live
- [ ] Then `ahead`, the desync on load with the new owner line, and the pivot re-judged at a
      real frame rate (STATUS "Next steps")
- [x] THE GHOSTING, SOLVED (session 15b, 2026-09-04, the tester on a Quest 3 over VDXR): it was
      the cadence beat. 2750x2850 at **90 Hz** puts the tick one display period long, 1.00-1.02
      slots per frame, and the doubled edges are gone; the same size at 120 Hz beats at
      1.05-1.11 and ghosts. The `EVEN CADENCE` threshold that hid this in session 14 is fixed
      (0.06 -> 0.02) and now prints the beat as a number
- [x] The headset-judged size is the default (session 15b): `[Screen] RenderWidth=2750
      RenderHeight=2850`, with the 90 Hz requirement in the ini text beside it, and
      `tests/golden/known-good-2750x2850-90hz.ini` as the byte copy of the judged machine
- [ ] THE STARTUP FLICKER, measured (session 15c, 2026-09-04) and NOT fixed: while the level
      streams the tick runs 51-72/s against 90 slots/s, the tag stream goes lopsided (L/s 18 vs
      R/s 73, 1016 same-eye pushes) and one eye starves - which is the flicker. Self-heals when
      draws/s reaches the display rate. **First test is `vrpace strict on`, which already
      exists and has never been judged**; only if that fails is code warranted. Open question
      first: why LEFT specifically (hypothesis: shared-capture deferred delivery repeating a
      tag; `capture sharedwait on` is the A/B)
- [ ] The remaining drops to ~60: 54 of 71 gaps sit in `present-tail (xrEndFrame)` up to 101 ms.
      Virtual Desktop side first (bitrate, codec, link, channel, wired AP), then ours
- [ ] Optional headroom under the 90 Hz cliff: ~2600x2700 predicts ~10.3 ms against 11.11 ms.
      Untested
- [ ] The pose-lane verdict (`vrpace poseaudit on`) read in the headset - the seam check passed
      in both session 15b runs but nobody armed the audit. No longer a ghosting suspect; keep it
      for the judder / `ahead` work


## S3 - Compare and choose; the features come back on the winner

- [ ] The comparison written in ARCHITECTURE (cost per present, per-eye correctness,
      failure modes, the headset verdicts) and the method chosen
- [ ] Hands (SkelControl drive, hand meshes), the wrist HUD (through the runtime layer's HUD
      quad and texture-provider seam), Blink and motion aim brought back on the winner;
      `[Mode] GamepadOnly=0` default again when they hold
      - [x] The HUD on its anchors (VR-117, 2026-09-14): the window and the hand quads through
            the runtime's provider seam, the whole HUD as one element, menus in the window;
            headset-confirmed 2026-09-15
      - [x] Per element (VR-118, VR-119, VR-120, 2026-09-15): the transform read from the
            HUD's vertex shader, the element table (vitals, reticle, prompt measured; the rest
            ride `default` until named), six anchors, two hands, the captured alpha; simulator
            green, the headset picks the shipped preset and the alpha mode
- [ ] The losing method kept registered as the A/B (every render lever ships with a live
      toggle)

## After S3 - carried from the D-milestones

- The author's 39.x fixes (docs/dishonored/HANDOFF-GINGASVR.md): 39.4 menu-ghost quadrant,
  39.2 pitch kept/discarded loop, 39.0 calibration bank by asset name (39.3, the adapter
  LUID, is in: the runtime layer asks for the device on the adapter it names)
- The prologue block fixed at the source; head-look in cutscene cameras
- Hand-aimed Possession, Devouring Swarm, Windblast
- Presentation polish; `tools\package.ps1` release (the zip, and the installer exe since
  VR-198); the config table (`core/config`) and the dissolution of `src/mod/state`

- [x] VR-103: script-state cinematic/dialogue stereo transitions; candidate225 headset-confirmed, PR55 approved for merge.

- [x] VR-50 cinematic/speaker FOV suppression accepted; broader manual-FOV and kill-cam cases remain open.
- [x] VR-104 native cinematic hands/arms and gameplay restoration accepted.

- [x] VR-105 authored pitch suppression accepted; physical tilt and actual height remain active.
- [x] VR-104 mantle handback headset-confirmed in later combined tests.
- [x] VR-50 exit blend and full-size cinematic transition headset-confirmed.

- [x] VR-50 cinematic exit recovery: build232 headset-confirmed (broader ticket remains open).
- [x] VR-105 upright yaw, roll comfort and dialogue free look headset-confirmed.
- [x] VR-104 later tests confirmed mantle and block-counter handback.

- [x] VR-106 standing pitched-head roll accepted on build245 and subsequent stack tests.
## Cinematic handoff regression (2026-09-14)

- [x] Build234 roll/pitch comfort, FOV and free look reported accepted.
- [x] VR-109 yielded-dispatch activity and body-heading handoff implemented and host-tested.
- [x] Continuous stereo and head-based movement verified; character-mode drift remains separately open.

## Movement mode follow-up (2026-09-14)

- [x] VR-109 mono handoff correction headset-confirmed.
- [ ] VR-109 character-oriented movement drift after cinematics remains open.
- [x] VR-110 saved head-based movement option implemented and host-tested.
- [x] Head-based direction accepted on build239 and retained through final stack.


## Deferred UI candidate (2026-09-14)

- [x] Implement VR-107 configurable mono anchoring and VR-108 menu/loading ownership guard.
- [x] Mono anchoring, loading release and main-menu/navigation fixes accepted; ordered merge explicitly authorized.

## Accepted defaults and stack integration (2026-09-14)

- [x] Build264 final crossbow tracking/opacity acceptance verified and archived.
- [x] Full installed INI/F10 profile promoted to writer, package and missing-key defaults.
- [x] All findings and rejected approaches consolidated in dishonored/STACK_ACCEPTANCE.md.
- [ ] VR-50 broader manual-FOV/kill-cam validation, VR-109 character mode, VR-99 brief note flicker, VR-87 height ceiling and VR-102 startup timing remain separate.

- [x] PR56/57/58 merged to VR-Main in order; all branches retained and10 linked tickets verified Done.

## Render-thread performance investigation

- [x] VR-121 independent profiling branch from accepted main.
- [x] Bounded sampled scopes and production host checks.
- [ ] Headset log attribution, then choose a measured optimization.
- [ ] Integration branch validation after independent wins are established.
