# Known issues

41.x is the FOUNDATION line for the new native-stereo render: it is not a release. The
milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **Stereo ships OFF: the game is on a head-locked screen, the same image in both eyes**
  [S2, S3]. The DXVK side-by-side render of releases up to 40.x is gone; the shipped method
  shows the game's own frame on a flat screen in front of you (`[Screen] DistanceMeters`,
  `WidthMeters`; F10 has the sliders). Turning your head turns the GAME camera as before, the
  screen itself stays in front of your eyes. `stereo reentry` (41.1) draws the scene twice
  per tick, once per eye, into a projection layer. Two headset runs (2026-09-03, Quest 3)
  found three faults, all fixed in the tree and none yet re-judged in a headset: the pairing
  check dropped left-eye tags while walking (both frames in both eyes, alternating - fixed,
  the second run reported stereo good); the head's displacement and roll were not driven
  into the game camera under the projection layer; and the camera field's SIGN was inverted,
  so every lean, crouch and per-eye offset went the wrong way (ENGINE_NOTES, "The camera
  field holds the POSITION"); and the head roll was written in the wrong sense (UE3 rolls
  positive right-ear-down), measured and fixed by picture. The eye offset was inverted with
  the field's sign, so the depth in those two runs was inside-out: judge the depth again,
  near objects should read near. A one-off "frames pinned in front of me until alt-tab"
  now has an instrument: `gameplay verdict:` in the log names the gate that dropped the
  layer to the head-locked quad.
  The tick rate halves while it runs (the second draw is a full scene draw). `stereo aer`
  is still a design stub.
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
