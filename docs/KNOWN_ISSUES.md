# Known issues

41.x is the FOUNDATION line for the new native-stereo render: it is not a release. The
milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **Stereo ships OFF: the game is on a head-locked screen, the same image in both eyes**
  [S2, S3]. The DXVK side-by-side render of releases up to 40.x is gone; the shipped method
  shows the game's own frame on a flat screen in front of you (`[Screen] DistanceMeters`,
  `WidthMeters`; F10 has the sliders). Turning your head turns the GAME camera as before, the
  screen itself stays in front of your eyes. `stereo reentry` (41.1) draws the scene twice
  per tick, once per eye, into a projection layer, and is HEADSET-VERIFIED (Quest 3,
  2026-09-03): depth, head tilt, lean, look and crouch are all correct. It stays off by
  default until the four items below are fixed. The tick rate halves while it runs (the
  second draw is a full scene draw). `stereo aer` is still a design stub.
- **`stereo reentry`: the eyes occasionally desync after a pause/resume** [S2b]. Rare, and
  it clears itself, but while it lasts the two eyes show different images instead of a
  stereo pair. Reported symptom: the judder below stays visible in the left eye and stops
  in the right, so the right eye looks like it stops getting fresh frames. Suspected in the
  arming path (the pair ring resyncs from mono when the cinematic quad hands back).
- **`stereo reentry`: fast head or player movement judders** [S2b]. Slightly floaty rather
  than locked. The pair is submitted as it completes rather than being paced against the
  predicted display time.
- **`stereo reentry`: looking up and down pitches about a point behind your eyes** [S2b].
  It reads as pitching your whole body instead of your head; a neck model (the pivot below
  and behind the eyes) is missing.
- **The F10 overlay has no render-resolution picker and no stereo arming tickbox** [S2b].
  Planned: a custom per-eye resolution defaulting to the Quest 3's, and a tickbox that arms
  the current stereo method, ticked by default.
- **The eye-check bands are BioShock's** [S2b]. `tools\eye-check.ps1` legs 2/4/5 were
  calibrated on another game; on Dishonored's sewers a true stereo pair reads an
  interocular mean of 6-7 against 13-22 for the same image shown twice, so those legs FAIL
  on a correct render. Leg 0 (the pairing) and the pair line's c5 travel in the log are the
  instruments that carry the verdict until the bands are recalibrated after a headset run.
- **The lever's FOV law is measured at 16:9 only** [S2b]. Under a projection layer the game
  renders the circumscribed 137 deg for a Quest 3 (a wide, rectilinear frame the compositor
  crops to the eye); a squarer frame would render less and waste less, but 41.0 removed the
  window machinery and `ResX/ResY` in the game's ini did not move the render here, so the
  second aspect is unmeasured and the render size stays the game's own.
- **Motion controls are OFF by default** [S3]. `[Mode] GamepadOnly=1` makes the VR controllers
  a plain gamepad: no hand models, no motion aim, no motion melee, no motion crouch; Blink aims
  down your view. Head tracking, positional (lean/peek/crouch) tracking and the FOV lever work.
  The hands come back on the winning stereo method; setting `GamepadOnly=0` re-enables the old
  hand code, which is compiled but untested on this render.
- **The wrist HUD and the aim reticle are gone** [S3]. The game's own HUD is on the screen with
  the rest of the frame; the floating panel returns through the runtime layer's HUD quad.
- **The headset image is the game window's resolution, captured once per frame on the CPU**
  [S1]. Set the game's video options to what your PC renders comfortably (1920x1080 is a fine
  start); a bigger window costs a bigger per-frame readback (~5 ms per present at 1080p on
  the shipped path, measured). `[Capture] Mode=deferred` halves it (2.3 ms) at the price of
  one present of latency; it ships off until a headset run picks the default. A D3D9Ex
  shared surface is NOT possible on this game's device (measured: not a 9Ex device).
- **SteamVR headsets need the shim** (Vive, Index, WMR through SteamVR). 41.0 is OpenXR-only
  and SteamVR ships no 32-bit OpenXR runtime, so `dvr_steamvr32.dll` and `openvr_api.dll` sit
  next to `d3d9.dll` and the mod falls back to them (`[VR] Runtime=steamvr` forces it). Vive
  wands have no face buttons; bind through SteamVR's controller settings. The shim is
  adapted from the BioShock trilogy VR mod and has NOT been run with Dishonored yet.
- **Quest via Virtual Desktop: VDXR must be the active OpenXR runtime** (Virtual Desktop
  Streamer sets it). With SteamVR's runtime active instead, the mod lands on the shim through
  SteamVR, which works but adds a hop.
- **Quit through the game's menu, not by closing the window** [S1]. A menu quit closes the
  OpenXR session cleanly (verified on a Quest 3, 2026-09-03). Closing the window (WM_CLOSE)
  was measured on the dev PC to leave the process alive with one thread for about an hour,
  with the mod's teardown never running; if `Dishonored.exe` stays in Task Manager after a
  quit, say so in the report - it blocks the next Steam launch.
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
