# Roadmap

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
      ~5 ms per present at 1080p, all of it `LockRect` waiting on the queued readback; the
      D3D9Ex shared surface is REFUSED by this game's device (not 9Ex, and UE3 needs
      D3DPOOL_MANAGED), so `[Capture] Mode=deferred` (queue the readback, lock it one present
      later) is the cut: 2.3 ms, `mono.xrs` passing. Ships default sync; the headset run
      picks the default (ENGINE_NOTES "The capture cost, measured")
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
- [ ] PERFORMANCE (the next session, before anything about comfort): the per-present CPU
      readback (5-15 ms, a GPU sync, twice per tick) replaced by a GPU path - the game's
      device created as D3D9Ex so `[Capture] Mode=shared` (a shared surface opened in D3D11)
      works, `deferred` as the interim; the render thread's second-draw cost measured; the
      attack freeze caught with a user-placed marker; then `ahead`, the desync on load
      (add the pace guard's eaten tag to the owner line, the eyes line during mono spells,
      `strict` on by default if it holds) and the pivot re-judged at a real frame rate

## S3 - Compare and choose; the features come back on the winner

- [ ] The comparison written in ARCHITECTURE (cost per present, per-eye correctness,
      failure modes, the headset verdicts) and the method chosen
- [ ] Hands (SkelControl drive, hand meshes), the wrist HUD (through the runtime layer's HUD
      quad and texture-provider seam), Blink and motion aim brought back on the winner;
      `[Mode] GamepadOnly=0` default again when they hold
- [ ] The losing method kept registered as the A/B (every render lever ships with a live
      toggle)

## After S3 - carried from the D-milestones

- The author's 39.x fixes (docs/dishonored/HANDOFF-GINGASVR.md): 39.4 menu-ghost quadrant,
  39.2 pitch kept/discarded loop, 39.0 calibration bank by asset name (39.3, the adapter
  LUID, is in: the runtime layer asks for the device on the adapter it names)
- The prologue block fixed at the source; head-look in cutscene cameras
- Hand-aimed Possession, Devouring Swarm, Windblast
- Presentation polish; `tools\package.ps1` release; the config table (`core/config`) and the
  dissolution of `src/mod/state`
