# Release notes

## 41.0.0 (unreleased) - native stereo foundation

- Removed: the DXVK fork (`dxvk_d3d9.dll`) and the whole `dxvk/` tree. The game renders
  natively through D3D9 again. Git history keeps the fork and its tags (`dxvk-base`,
  `dxvk-m8.2-shipped`, `dxvk-m8.4`, `dxvk-shipped`).

Upgrading: delete `dxvk_d3d9.dll` and `dxvk_stereo.txt` from the game folder (the
installer does).

## 40.0.0 (unreleased)

Numbered 40 because the original author's private line reached 39.4 (see
docs/dishonored/HANDOFF-GINGASVR.md; those fixes are being ported). No intended behavior
change from 38.92 apart from the fixes below. The mod is rebuilt with
Visual Studio from a module tree instead of one file, the DXVK fork lives in this repository,
and there is a debugging surface for development.

- Fixed: hand skin `.mtl` files never loaded (a bad escape in the path).
- Fixed: the VR backend is chosen by asking the runtimes instead of looking for Virtual
  Desktop's streamer process; Quest over Link, Air Link and Steam Link, and other OpenXR
  headsets, now take the OpenXR path when a 32-bit runtime with an HMD answers.
- New: `dishonored_vr.log` has levels and subsystem tags, keeps the previous run as
  `dishonored_vr.prev.log`, and a crash writes `dishonored_vr_crash.txt` plus a minidump.
- New: the F10 overlay has a Log tab.
- New (developers): `command.txt` / `status.json` in `%LOCALAPPDATA%\DishonoredVR`, frame
  dumps, the simulated OpenXR runtime and the PowerShell harness.
- Known: `[VR] StampFix` is inert (the fork export it needs is not in the published patches).

Upgrading: drop the three DLLs over the old ones; your `dishonored_vr.ini` is kept.

## 38.92 (shipped alpha, GingasVR)

The last build of the original author. SteamVR headsets tuned; Quest via Virtual Desktop
experimental. Features: true per-eye stereo through the DXVK fork, 6DoF head tracking with
lean and physical crouch, roomscale with auto-recenter, motion controls with both hands on
Arkane's rig, hand-aimed Blink with distance by hand pitch, hand-aimed projectiles, sword
swings and blocking by motion, wrist-mounted HUD, F10 settings overlay, per-eye shadows,
light shafts and reflections. Known issues: docs/KNOWN_ISSUES.md.
