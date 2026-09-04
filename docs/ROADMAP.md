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
- [ ] Then `ahead`, the desync on load with the new owner line, and the pivot re-judged at a
      real frame rate (STATUS "Next steps")


## S3 - Compare and choose; the features come back on the winner

- [ ] The comparison written in ARCHITECTURE (cost per present, per-eye correctness,
      failure modes, the headset verdicts) and the method chosen
- [x] The HUD panel, head-locked (session 10): the draw class measured and proven by picture
      (ENGINE_NOTES, "The Scaleform HUD draw class, measured"), redirected into a private
      target and handed to the runtime layer through `set_hud_texture_provider`.
      `[Hud] Panel=0` ships off; the headset judges legibility and placement. The WRIST
      anchor is a second change and waits for the hands.
- [x] Cutscenes have a policy (session 10): `[Cine] Mode=quad|stereo`, `[Cine] HudPanel`,
      and the runtime layer's own `vrcine` seam is reachable at last.
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
