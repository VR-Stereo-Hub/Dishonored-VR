# Known issues

41.x is the FOUNDATION line for the new native-stereo render: it is not a release. The
milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **Stereo ships ON: `[Stereo] Method=reentry`, `Armed=1`** [S2b]. The game's scene is drawn
  twice per tick, once per eye, into a projection layer (HEADSET-VERIFIED on a Quest 3,
  2026-09-03: depth, head tilt, lean, look and crouch). The F10 Display tab's `stereo armed`
  tickbox (ticked) parks the game on the head-locked mono screen without forgetting the
  method; `stereo arm on|off` on the seam. The tick rate halves while stereo runs (the second
  draw is a full scene draw). `stereo aer` is still a design stub.
- **`stereo reentry`: the eyes could desync after a pause/resume** [S2b, fixed at the source,
  headset verdict pending]. The second draw's gates re-decided after the first draw, so the
  resume window produced left tags with no right sibling and the right eye kept a held image.
  The gates are decided once per tick now; `vrpace strict` (off) is the fail-soft that shows
  the fresh eye to both eyes for a frame if it recurs, and the log's `STALE R EYE` line names
  the owner. If you see it: F10 Runtime, `Strict pairs`, and send the log.
- **`stereo reentry`: fast head or player movement judders** [S2b, instrumented, headset
  judges]. The `pair phase` line says whether a pair closes before or after the display slot
  it was predicted for; `vrpace ahead 0|1|2` (F10 Runtime, `Pose look-ahead`) locates the pose
  for the slot the image will reach. Ships at 0 (today's behaviour) until a headset run picks.
- **`stereo reentry`: looking up and down pitches about a point below the eyes** [S2b,
  measured, headset judges]. The ENGINE pitches its camera about a pivot 32 cm below and 6 cm
  behind the eyes (17 cm of backward travel at 30 deg of pitch), which the compositor cannot
  reproject. `[Neck] Mode=cancel` with the measured pivot (the defaults) cancels it; ships
  `off`, F10 Comfort has the three-way (`off` / `add` / `cancel`) and `neck` is the seam word.
  Look up and down at something an arm's length away and pick the mode that keeps it still.
- **The render size, the F10 picker, and sharpness** [S2b]. The Display tab's picker (default:
  the runtime's recommended per-eye size, 2496x2688 on a Quest 3 via VDXR) takes effect at
  the NEXT LAUNCH: the ask goes on the game's command line (`-ResX/-ResY/-FullScreen`), the
  one route this build honours (its own ini and the console's `setres` are both measured
  inert), and `VirtualMode` provides a size your display does not list (the proxy advertises it
  and creates the fullscreen device windowed). A near-square render is what sharpness needs: a
  16:9 frame must claim 137 deg to cover the eye and spends half its pixels outside it; at
  2496x2688 the claim is 108 deg. The catch: the CPU readback at that size costs ~18 ms per
  present on the shipped `[Capture] Mode=sync`; set `Mode=deferred` with it. `res: HONOURED`
  in the log is the verdict; `res modes`, `res <W>x<H>[f|w]`, `res 0x0` on the seam.
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
