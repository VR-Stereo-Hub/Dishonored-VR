# Dishonored VR

A VR mod for Dishonored (2012, Steam). Head tracking with lean, physical crouch and
roomscale, the VR controllers as a gamepad, and - being rebuilt in this line - per-eye stereo,
motion controls with both hands on the game's rig, hand-aimed Blink and projectiles, sword
swings and blocking by motion, a floating HUD and an in-headset settings overlay. One OpenXR
path for every headset: Quest through Virtual Desktop's runtime, any native 32-bit OpenXR
runtime, and SteamVR headsets through the bundled `dvr_steamvr32` shim.

**41.0 is the foundation build of a new render.** The DXVK side-by-side stereo of the
original mod could not be tuned into shape (docs/STATUS.md, sessions 2-4), so the game now
renders natively and the mod shows it on a head-locked screen in both eyes while two stereo
methods (alternate-eye and scene-draw re-entry) are built and compared on one seam
(docs/ROADMAP.md, docs/ARCHITECTURE.md "The stereo ladder"). Motion controls are off by
default until then (`[Mode] GamepadOnly=1`).

Original mod by [GingasVR](https://github.com/GingasVRFO/Dishonored-VR) (alpha 38.92). This
repository continues it with the author's permission. Status and next steps:
`docs/STATUS.md`. Known issues: `docs/KNOWN_ISSUES.md`.

## Install (players)

1. Copy `d3d9.dll`, `dvr_steamvr32.dll` and `openvr_api.dll` from the release zip into
   `<Steam>\steamapps\common\Dishonored\Binaries\Win32\` (delete any `dxvk_d3d9.dll` an
   older release left there).
2. Set a normal resolution in the game's video options (1920x1080 windowed is a fine start;
   the headset shows the game window's frame). A release before 41.0 forced 4032x2268 into
   `DishonoredEngine.ini` and `DishonoredCompat.ini`; put a normal size back.
3. Launch from Steam. Quest: Virtual Desktop streaming with VDXR as the OpenXR runtime,
   72 Hz, SSW off. SteamVR headsets: start SteamVR first; the mod falls back to the shim.
4. F5 recenter, F10 settings (screen distance and width live there). Motion Blur off.

Requirements: the Steam version (GOG is a different exe), a 64-bit Windows PC, a Quest with
Virtual Desktop or a SteamVR-native headset. Troubleshooting: `docs/TROUBLESHOOTING.md`.

## Controls (defaults)

The controllers are a gamepad: left stick move, right stick turn, triggers attack and use
powers, the face buttons as the game's own pad layout, both stick clicks together recenter.
Physical crouch and lean/peek work. With `[Mode] GamepadOnly=0` the original motion controls
(swing to attack, hand-aimed Blink and weapons, END to calibrate the hands, HOME to toggle
the hand drive) are compiled in but untested on the new render.

New game: the prologue cutscene is broken in VR; the mod skips to the prison (see the
original author's video: https://www.youtube.com/watch?v=ikVDL2wMIYw).

## Build (developers)

Visual Studio 2022 Build Tools (C++ workload, CMake component), Git with submodules.

```powershell
git clone --recursive https://github.com/VR-Stereo-Hub/Dishonored-VR.git
cd Dishonored-VR
.\tools\build.ps1                 # d3d9.dll + dvr_steamvr32.dll + the simulated OpenXR runtime + the smoke client
.\tools\install.ps1               # into the game folder (found via Steam; or set DVR_GAME_DIR)
.\tools\xrsim-selftest.ps1        # the simulator works on this PC
.\tools\xrsim-launch.ps1          # the game on the simulator: expect "xr: pipeline READY"
```

The tree: `src/proxy` (the d3d9.dll exports), `src/core` (VR core: log, hooks, the frame
path, the stereo seam and methods, the OpenXR runtime layer, input, overlay),
`src/game/dishonored` (everything that knows an engine address: `patterns.h`, UE3 reflection,
the camera seam, head tracking, hands, Blink, melee), `src/legacy` (retired experiments, off
by default), `src/tools` (the simulated runtime, the SteamVR shim), `tools/` (the PowerShell
harness), `docs/` (the project's brain: architecture, engine notes, verification catalog,
roadmap). Agents and contributors start at `CLAUDE.md`.

## Credits

Arkane Studios / Bethesda for the game (this is a fan project; no game assets are included).
GingasVR for the original mod. Dear ImGui (MIT), OpenVR SDK (BSD-3), OpenXR SDK
(Apache-2.0). The OpenXR runtime layer, the SteamVR shim (adapted from BioVRDev's
OpenXRShim with permission), the simulator, the harness and the documentation shape come
from the BioShock trilogy VR mod. License: zlib/libpng (see LICENSE); third-party notices
in `THIRD_PARTY_NOTICES.md`.
