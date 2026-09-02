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

## After S0: two lanes on one foundation

S1 was folded into the two method lanes (2026-09-03, the user's call): every S1 item is either
something the native-stereo lane needs anyway or housekeeping either lane can absorb. The two
lanes run in parallel on the same foundation; S3 compares them.

## S2a - AlternateEye (rung 2; developer A)

`core/gfx/aer.cpp` carries the design. The eye field is measured (camera+0x330, negated
position; `[Camera] EyeField=0x330`), so the method alternates `eye_for_next_frame()` and tags
each present; the seam writes the offset on the script lane. Acceptance, in order:

- [ ] `stereo aer` accepted; the beat line reads `L/s == R/s == out/s / 2`
- [ ] `stereo.xrs` on the simulator: two projection views, `EyeSeparationM` == IPD, left vs
      right `img-diff` well above the noise floor with parallax on near geometry
- [ ] eye-check.ps1 legs 0-5 PASS; the runtime's pair probe reports no untagged presents
      (the stale-left class)
- [ ] Headset: fusion at the measured IPD, no swim on head turns; half-rate per eye judged
      acceptable or not (write the verdict)

## S2b - Native stereo by scene-draw re-entry (rung 3; the user)

`core/gfx/reentry.cpp` carries the design. The former S1 items this lane needs come first,
because re-entry doubles the presents and the eye offset rides the same write:

- [ ] The capture cost cut: a D3D9Ex shared surface opened on the D3D11 side replaces
      `GetRenderTargetData` (the per-frame CPU round trip in `core/gfx/capture`); measured as
      presents/s at the headset's refresh on the mono screen (68/s at 1920x1080 on the Quest 3
      run of 2026-09-03 is the baseline)
- [ ] Positional (lean/crouch/roomscale) tracking moved from the c0 `LeanVP` patch to the
      camera seam's position write into camera+0x330, and measured equal (the eye offset uses
      the same write, so one mechanism drives both)
- [ ] The scene-draw root found and byte-verified (caller census at `ApplyHeadToViewRotation`,
      live stack scrape, identify the pass by making it MOVE with the eyetest as the mover); its
      address in `patterns.h` with the derivation in ENGINE_NOTES
- [ ] The second draw gated deny-by-default and SEH-guarded; a fault poisons the method for
      the session and the game runs on mono
- [ ] `stereo reentry` accepted; presents = 2x ticks; the beat line reads `L/s == R/s`;
      second-draw cost logged
- [ ] `stereo.xrs`, eye-check legs 0-5 on the simulator; headset: fusion, per-eye
      reflections and effects, no flicker on fast motion

## Housekeeping, either lane, whoever touches it first

- [ ] `head_track` and `pad_bridge` converted to real modules (the D1-era refactor step S0
      deferred)
- [ ] A SteamVR rig confirmed through the shim (`xr: runtime "DishonoredVR SteamVR shim
      (OpenVR)"`)
- [ ] `[VR] FpsCap` cadence chosen for VDXR (72 or 45 at 90 Hz) once the capture cost is cut

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
