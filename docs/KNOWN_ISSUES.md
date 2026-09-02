# Known issues

41.0 is the FOUNDATION build for the new native-stereo render: it is not a release. The
milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **No stereo yet: the game is on a head-locked screen, the same image in both eyes** [S1,
  S2]. The DXVK side-by-side render of releases up to 40.x is gone; 41.0 shows the game's own
  frame on a flat screen in front of you (`[Screen] DistanceMeters`, `WidthMeters`; F10 has the
  sliders). Turning your head turns the GAME camera as before (head tracking is unchanged), the
  screen itself stays in front of your eyes. Two stereo methods are being built on this
  foundation and compared; `stereo aer` / `stereo reentry` exist as names and refuse.
- **Motion controls are OFF by default** [S3]. `[Mode] GamepadOnly=1` makes the VR controllers
  a plain gamepad: no hand models, no motion aim, no motion melee, no motion crouch; Blink aims
  down your view. Head tracking, positional (lean/peek/crouch) tracking and the FOV lever work.
  The hands come back on the winning stereo method; setting `GamepadOnly=0` re-enables the old
  hand code, which is compiled but untested on this render.
- **The wrist HUD and the aim reticle are gone** [S3]. The game's own HUD is on the screen with
  the rest of the frame; the floating panel returns through the runtime layer's HUD quad.
- **The headset image is the game window's resolution, captured once per frame on the CPU**
  [S1]. Set the game's video options to what your PC renders comfortably (1920x1080 is a fine
  start); a bigger window costs a bigger per-frame readback. A D3D9Ex shared surface replaces
  the readback in S1.
- **SteamVR headsets need the shim** (Vive, Index, WMR through SteamVR). 41.0 is OpenXR-only
  and SteamVR ships no 32-bit OpenXR runtime, so `dvr_steamvr32.dll` and `openvr_api.dll` sit
  next to `d3d9.dll` and the mod falls back to them (`[VR] Runtime=steamvr` forces it). Vive
  wands have no face buttons; bind through SteamVR's controller settings. The shim is
  adapted from the BioShock trilogy VR mod and has NOT been run with Dishonored yet.
- **Quest via Virtual Desktop: VDXR must be the active OpenXR runtime** (Virtual Desktop
  Streamer sets it). With SteamVR's runtime active instead, the mod lands on the shim through
  SteamVR, which works but adds a hop.
- **Quitting can leave the process lingering** [S1]. Measured on the dev PC with the
  simulator: closing the window (WM_CLOSE) logged the viewport closing and then the process
  stayed alive with one thread and could not be killed until a reboot; the mod's own teardown
  never ran (no `PreExit`). Quit through the game's menu, and if `Dishonored.exe` stays in
  Task Manager afterwards, say so in the report - it blocks the next Steam launch.
- **A direct `Dishonored.exe` launch crashes at the main menu** (the original author's trap
  6, confirmed 2026-09-02). Launch through Steam; the harness does (`xrsim-launch.ps1
  -ViaSteam`).
- **Head-look parks after a missed menu close event** ("F9 fixes it") [after S3]. The
  author's 39.4 fix is still to be ported. Plain F9 clears it.
- **The prologue cutscene is broken** [after S3]: the boat arrival blocks with a Block prompt.
  The mod jumps straight to the prison (IntroSkip). Start a new game, then continue from the
  prison save.
- **Cutscene cameras are fixed** (no head-look) [after S3].
- **Possession, Devouring Swarm and Windblast are head-aimed** [after S3].
- **GOG version unsupported** (different exe; every hook address is for the Steam build).
- **Motion Blur must be off** in the game's options.
