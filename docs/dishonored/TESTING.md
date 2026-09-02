# Testing - Dishonored

## Install / launch loop

1. `tools\setup-game-ini.ps1 -Console` if you want the engine console (F1 in game, then ~).
   The game's video options hold the resolution (1920x1080 windowed to start); nothing in
   the mod forces a size any more.
2. `tools\build.ps1` then `tools\install.ps1` (`d3d9.dll`, `dvr_steamvr32.dll`,
   `openvr_api.dll`).
3. `tools\launch-game.ps1` (Steam) and `tools\tail-log.ps1` in a second window. The first
   lines: build id, the runtime layer's instance line (which OpenXR runtime answered),
   `device hooks installed`, `INSTALLED at 0x...` for each engine hook, then
   `xr: session created`, `xr: pipeline READY`, `stereo: method 'mono' registered - default`.
4. Continue the newest save. **Never New Game in the harness** (the prologue is broken; the
   intro skip jumps to the prison).

Files: log next to the exe; `command.txt`/`status.json` in `%LOCALAPPDATA%\DishonoredVR`.
Copy `dishonored_vr.log` out before every relaunch (rotation is one deep).

## Flat checks (no headset)

- `tools\game-cmd.ps1 "status"` then `tools\status-dump.ps1`: `state` GAMEPLAY, `hooks.*`
  true, `stereo.method` mono, `stereo.framesOut` advancing, `camera.c5ok` true.
- `tools\game-cmd.ps1 "stereo status"` / `"camera status"`: the two seams' log lines.
- `tools\game-cmd.ps1 "dump frame"`: `dumps\capture_*.bmp` is the game window as captured;
  `eye_*_mono.png` the method's output texture (colours must match the capture).
- `capture: WxH content bbox ... (FULL)` in the log: the game draws its whole window.
- `tools\game-cmd.ps1 "camera eyetest 100"` in gameplay, standing still: the six
  `camera/eyetest:` verdicts, then `DONE`; record them in ENGINE_NOTES.
- `tools\game-cmd.ps1 "stereo aer"`: refused with the note, mono keeps running.
- `tools\game-cmd.ps1 "console ce ChangeLvl_fromTower_toPrison"`: the console seam (script
  lane).

## Simulator checks (OpenXR lane, no headset)

`docs/VERIFICATION.md` section 3. `xrsim-launch.ps1` (or `-ViaSteam` if a direct launch dies
at the menu); expect `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`. Then `boot.ps1
-Attach`, foreground the window, `mono.xrs` (the rung-1 gate: a quad layer, both eyes
non-black, equal bboxes; a black eye is attributed in `xrsim.log`), `smoke.xrs`,
`headlook.xrs` (the quad is head-locked, so the captured screen must NOT move; the camera
moving shows in `dump capture`). `stereo.xrs`, `world-6dof.xrs`, `coupling-hand.xrs` and
`eye-check.ps1` wait for a stereo method (S2).

## Headset checklist (S0/S1, Quest 3 via VDXR)

- The game on a head-locked screen in BOTH eyes; F5 recenters; head look turns the game
  camera 1:1; lean/crouch; roomscale auto-recenter.
- The gamepad works (sticks, triggers, faces); both stick clicks recenter.
- F10 opens on the screen; `screen distance` / `screen width` move it; sliders save.
- Menus and loading screens show on the screen; no `EXCEPTION` in the log; the session
  survives alt-tab (`xr: SUBMISSION IDLE` lines name the reason while it idles).
- Save/load: head tracking survives (or F9 re-arms).
- SteamVR rig: `xr: runtime "DishonoredVR SteamVR shim (OpenVR)"` and
  `%LOCALAPPDATA%\DishonoredVR\ovrshim.log` present.

## Crash triage

`dishonored_vr_crash.txt` (fingerprint: module+offset, thread, registers, callers, the
`backend=openxr runtime="..."` context) and the minidump in
`%LOCALAPPDATA%\DishonoredVR\dumps`. `tools\read-dump.py <dmp>` summarises a minidump
without symbols (`pip install minidump`). `DVR_SKIP=hands,blink,overlay` bisects a crash to
a subsystem without a rebuild. `DISHONORED_VR_XR_SAFE=1` disables every game-memory writer
on the XR path (the original crash bisector).

## Unverified as of 2026-09-02 (41.0)

Everything above the crash triage is written from the code and the BioShock harness's shape
and needs its first attended run on 41.0: the game was not installed on the dev PC when the
foundation was built. STATUS records what has run.
