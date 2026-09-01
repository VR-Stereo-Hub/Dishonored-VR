# Testing - Dishonored

## Install / launch loop

1. `tools\setup-game-ini.ps1 -Resolution` once (ResX=4032 ResY=2268 Fullscreen=False, backed
   up). `-Console` if you want the engine console (F1 in game, then ~).
2. `tools\build.ps1 -Install` (`-SkipDxvk` to keep the installed `dxvk_d3d9.dll`, or
   `tools\build-dxvk.ps1 -Install` to build the fork).
3. `tools\launch-game.ps1` (Steam) and `tools\tail-log.ps1` in a second window. The first
   lines: build id, backend verdict (`config: VR backend ...`, `probe:` lines), `backend: DXVK
   loaded`, then `device hooks installed`, `INSTALLED at 0x...` for each engine hook.
4. Continue the newest save. **Never New Game in the harness** (the prologue is broken; the
   intro skip jumps to the prison).

Files: log next to the exe; `command.txt`/`status.json` in `%LOCALAPPDATA%\DishonoredVR`.

## Flat checks (no headset)

- `tools\game-cmd.ps1 "status"` then `tools\status-dump.ps1`: `state` GAMEPLAY, `hooks.*`
  true, `counters.splices` > 0 in a 3D scene.
- `tools\game-cmd.ps1 "dump frame"`: `dumps\capture_*.bmp` is the SBS frame the headset is
  fed; `eye_*_left/right.png` are the composed eye targets.
- `tools\game-cmd.ps1 "log cat blink debug" "blink probe"`: the Blink survey lines.
- `tools\game-cmd.ps1 "console ce ChangeLvl_fromTower_toPrison"`: the console seam (script
  lane).

## Simulator checks (OpenXR lane, no headset)

`docs/VERIFICATION.md` section 3. `xrsim-launch.ps1` forces `DISHONORED_VR_BACKEND=openxr`;
expect `xr: runtime "dvr-xrsim"`, `xr: 2064x2208 per eye`, `xr: pipeline READY`. Then
`boot.ps1 -Attach`, `smoke.xrs`, `stereo.xrs`, `headlook.xrs`, `world-6dof.xrs`,
`coupling-hand.xrs`.

## Headset checklist (D1 parity, SteamVR lane)

- Boot to gameplay; F5 recenters; head look 1:1; lean/crouch; roomscale auto-recenter.
- Hands: END calibrates; HOME toggles; hands follow the controllers; the residual head
  coupling is the known issue.
- Blink hand-aims with the landing marker; crossbow/pistol hand-aim; sword swing; block.
- Wrist HUD readable; F10 overlay opens, sliders save to the ini.
- Menus mono (both eyes the full frame); loading screens do not crash.
- Save/load: head tracking survives (or F9 re-arms); no `EXCEPTION` in the log.

Compare `tools\log-parse.ps1` output against a 38.92 log from the same rig: same hooks
installed, splice counts in the same range, no new WARN/ERROR lines.

## Crash triage

`dishonored_vr_crash.txt` (fingerprint: module+offset, thread, registers, callers) and the
minidump in `%LOCALAPPDATA%\DishonoredVR\dumps`. `tools\read-dump.py <dmp>` summarises a
minidump without symbols (`pip install minidump`). `DVR_SKIP=hands,blink,overlay` bisects a
crash to a subsystem without a rebuild. `DISHONORED_VR_XR_SAFE=1` disables every game-memory
writer on the XR path (the original crash bisector).

## Unverified as of 2026-09-02

The game is not installed on the dev PC. `boot.ps1`'s key sequence, `game-shot.ps1` on the
DXVK window, `setup-game-ini.ps1` against a real ini and the sim launcher's log markers all
need their first attended run.
