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

### The door keyhole (VR-133)

Needs a save facing a closed door (the dev PC's Continue save is the Hound Pits pub door).
Walk in: Return on the title, Return on Continue, Return on the confirmation, ~45 s, one
Return on the board, then `[game] state: GAMEPLAY`. Use is gamepad X; a peek ENDS on a
Use PRESS (releasing the hold does nothing), so release before pressing. `head rot a b c`
is yaw, pitch, roll. On this PC pass `-Dir D:\dvr-data\xrsim`.

```powershell
.\tools\game-cmd.ps1 "keyhole on"                 # the lever, live (status: keyhole status)
.\tools\xrsim-cmd.ps1 "reset" "head rot 0 0 0"
.\tools\xrsim-shot.ps1 -Out D:\dvr-data\shots\kh\walk0
.\tools\xrsim-cmd.ps1 "btn x down"                # hold Use: anim: master=StatePlayerMasterHolePeeking, keyhole: ENTER
.\tools\xrsim-shot.ps1 -Out ...\peek0 ; (4 s) ; .\tools\xrsim-shot.ps1 -Out ...\peek4s   # identical = no shrink
.\tools\xrsim-cmd.ps1 "head rot 60 0 0"  ; .\tools\xrsim-shot.ps1 -Out ...\yaw60        # past the +-35 cone: the view follows
.\tools\xrsim-cmd.ps1 "head rot 0 -30 0" ; .\tools\xrsim-shot.ps1 -Out ...\pitch30      # past the +-17.6 cone
.\tools\xrsim-cmd.ps1 "head rot 20 0 0" "btn x up" ; .\tools\xrsim-cmd.ps1 "btn x press 200"   # leave with the head turned 20
```

Read: `cine/head: entered authored camera ... owner=keyhole` and `cine/head: scope=N ...
keyhole=1` with `refused=0`; `keyhole: active ... headOwnsInput=1`; `keyhole: EXIT ...
travel -20.0` then `keyhole/exit: carry yaw -20.0 deg once`; `armfollow/yaw` ctrl/view at
pre-entry + travel (69.8 for 89.8 - 20) and stable; `cine/fov: ... keyhole=1/0` while
peeking, `0/1` for one line after, then `0/0`; `hud/layout: routed this window: ...
keyhole=N default=0` while peeking. `keyhole off` and repeat is the A/B: the view stops at
the cone, `headOwnsInput=0`, no carry. The game auto-pauses when it loses focus: a census
that returns 0 clusters means the pause menu is riding (`Escape` once).

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
