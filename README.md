# Dishonored VR

A VR mod for Dishonored (2012, Steam): true per-eye stereo, 6DoF head tracking with lean,
physical crouch and roomscale, motion controls with both hands on the game's rig, hand-aimed
Blink and projectiles, sword swings and blocking by motion, a wrist-mounted HUD and an
in-headset settings overlay. SteamVR headsets (Vive, Index) through OpenVR; Quest through
Virtual Desktop's OpenXR runtime (experimental, see the known issues).

Original mod by [GingasVR](https://github.com/GingasVRFO/Dishonored-VR) (alpha 38.92). This
repository continues it with the author's permission: same features, rebuilt as a proper
project with a module tree, a debugging surface, a simulated OpenXR runtime for testing
without a headset, and a documentation set for developers and agents.
Status and next steps: `docs/STATUS.md`. Known issues: `docs/KNOWN_ISSUES.md`.

## Install (players)

1. Copy `d3d9.dll` and `openvr_api.dll` from the release zip into
   `<Steam>\steamapps\common\Dishonored\Binaries\Win32\` (delete any `dxvk_d3d9.dll` an
   older release left there).
2. Run `setup_resolution.bat` once (sets ResX=4032 ResY=2268 Fullscreen=False in
   `Documents\My Games\Dishonored\DishonoredGame\Config\DishonoredEngine.ini`, with a backup).
3. Launch from Steam. SteamVR headsets: start SteamVR first. Quest: Virtual Desktop streaming
   with VDXR as the runtime, SteamVR not running, 72 Hz, SSW off.
4. F5 recenter, F10 settings, END hand calibration, HOME hand drive on/off. Motion Blur off.

Requirements: the Steam version (GOG is a different exe), a 64-bit Windows PC, a SteamVR-native
headset or a Quest with Virtual Desktop. Troubleshooting: `docs/TROUBLESHOOTING.md`.

## Controls (defaults)

- Attack with the sword by swinging it, or right trigger.
- Crouch by crouching physically, or right A.
- Block / choke: right stick click.
- Lean by leaning physically (`RoomDeadM` and `RoomBleedMS` in `dishonored_vr.ini` tune the
  room-scale dead zone).
- Blink: left trigger, aim with the left hand.
- Gun, crossbow and the other left-hand weapons: left trigger, aim with the hand.
- Interact: left X (Quest) or A (Index).
- Weapon wheel: left grip (Quest) or the left trackpad (Index), then the stick. Health:
  open the wheel and press right B.
- Hands: F5 to recenter, then F10 > calibrate hands, trim them in the Hand section, "save
  defaults" at the top.
- New game: the prologue cutscene is broken in VR; the mod skips to the prison (see the
  original author's video: https://www.youtube.com/watch?v=ikVDL2wMIYw).

## Build (developers)

Visual Studio 2022 Build Tools (C++ workload, CMake component), Git with submodules.

```powershell
git clone --recursive https://github.com/VR-Stereo-Hub/Dishonored-VR.git
cd Dishonored-VR
.\tools\build.ps1                 # d3d9.dll + the simulated OpenXR runtime + the smoke client
.\tools\install.ps1               # into the game folder (found via Steam; or set DVR_GAME_DIR)
.\tools\xrsim-selftest.ps1        # the simulator works on this PC
```

The tree: `src/proxy` (the d3d9.dll exports), `src/core` (VR core: log, hooks, present
pipeline, OpenVR and OpenXR backends, input, overlay), `src/game/dishonored` (everything that
knows an engine address: `patterns.h`, UE3 reflection, head tracking, hands, Blink, melee),
`src/legacy` (retired experiments, off by default), `src/tools` (the simulated runtime), `tools/` (the
PowerShell harness), `docs/` (the project's brain: architecture, engine notes, verification
catalog, roadmap). Agents and contributors start at `CLAUDE.md`.

## Credits

Arkane Studios / Bethesda for the game (this is a fan project; no game assets are included).
GingasVR for the original mod. Dear ImGui (MIT), OpenVR SDK
(BSD-3), OpenXR SDK (Apache-2.0). The simulator, harness and documentation shape come from the
BioShock trilogy VR mod. License: zlib/libpng (see LICENSE).
