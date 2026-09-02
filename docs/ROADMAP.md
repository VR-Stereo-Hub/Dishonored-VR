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
- [ ] The capture cost measured and cut: a D3D9Ex shared surface opened on the D3D11 side
      replaces `GetRenderTargetData` (the per-frame CPU round trip in `core/gfx/capture`)
- [x] `camera eyetest` verdicts recorded: camera+0x330 HONOURED (holds -position), the five
      others DISCARDED (ENGINE_NOTES)
- [ ] Positional (lean/crouch/roomscale) tracking moved from the c0 `LeanVP` patch to the
      camera seam's position write once the write point is known, and measured equal
- [x] `pad_bridge` converted to a real module (`core/input/pad_bridge.{h,cpp}`, its own
      translation unit and state, Dishonored behind a Callbacks seam); the motion controls
      are out of the input path. `head_track` still to do
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

- [ ] The scene-draw root found and byte-verified (caller census, live stack scrape, the
      eyetest as the mover); its address in `patterns.h` with the derivation in ENGINE_NOTES
- [ ] The second draw gated deny-by-default and SEH-guarded; a fault poisons the method for
      the session and the game runs on mono
- [ ] `stereo reentry` accepted; presents = 2x ticks; the beat line reads `L/s == R/s`;
      second-draw cost logged
- [ ] `stereo.xrs`, eye-check legs 0-5 on the simulator; headset: fusion, per-eye
      reflections and effects, no flicker on fast motion

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
