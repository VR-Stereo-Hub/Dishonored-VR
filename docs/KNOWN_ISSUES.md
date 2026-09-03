# Known issues

41.x is the FOUNDATION line for the new native-stereo render: it is not a release. The
milestone in brackets is where the fix is planned (docs/ROADMAP.md).

- **Stereo ships ON: `[Stereo] Method=reentry`, `Armed=1`** [S2b]. The game's scene is drawn
  twice per tick, once per eye, into a projection layer (HEADSET-VERIFIED on a Quest 3,
  2026-09-03: depth, head tilt, lean, look and crouch). The F10 Display tab's `stereo armed`
  tickbox (ticked) parks the game on the head-locked mono screen without forgetting the
  method; `stereo arm on|off` on the seam. The tick rate halves while stereo runs (the second
  draw is a full scene draw). `stereo aer` is still a design stub.
- **PERFORMANCE: the frame reaches the headset through a readback, twice per tick** [S1,
  measured and levered 2026-09-03, headset verdict pending]. The tick budget (`perf: tick` and
  `perf: gpu` every 3 s, the F10 Display block) measured the readback as the owner of the tick
  on both sides at the Quest 3 size: 16 ms of GPU copy per present into system memory plus
  7.5 ms of CPU, against 5 ms of actual 3D work per draw. `[Capture] Mode=deferred` ships now
  (27 vs 21 ticks/s on the simulator, one present of latency; `capture mode sync` is the A/B).
  The fix is `[Device] Ex=1` (the F10 Display tickbox, relaunch) with `capture mode shared`:
  the game's device as D3D9Ex, every MANAGED texture shadowed in system memory, the frame kept
  in VRAM; 75-90 ticks/s on the simulator at the Quest 3 size, pace-bound. It is OFF by default
  until a headset run has judged it (it changes how the game creates every texture and buffer;
  the log's `device:` and `device/shadow` lines say what happened). The first headset run on it
  crashed after repeated quickloads (a map in the shadow filled up; fixed 2026-09-03 and
  reproduced clean on the simulator) and had never switched the capture to shared: the F10
  tickbox now selects both for the next launch. `capture mode off` freezes
  the image on purpose (the A/B control); `mark <text>` and the F10 MARK button stamp a felt
  freeze in the log with the ring of presents around it.
- **`stereo reentry`: the eyes can still desync, mostly on load** [S2b, NOT fixed]. The
  one-sided-tag cause was real and is fixed (the simulator proves it), but the headset still
  sees the two eyes disagree on the first load and after some pause/resumes, clearing on its
  own. Session 8 fixed a second source (a present whose capture delivered no frame pushed the
  previous frame's tag again: a stale eye at every capture-mode switch) and made the pace
  guard's eaten tag a named owner on the `STALE EYE` line (`eatenNoFrame`, `eaten=` on the eyes
  line); a simulated focus loss and regain did not reproduce it. If you see it: F10 Runtime
  `Strict pairs`, note the time, send the log; the owner line now says which mechanism.
- **`stereo reentry`: fast head or player movement judders** [S2b, blocked on performance].
  The `pair phase` line and `vrpace ahead 0|1|2` (F10 Runtime, `Pose look-ahead`) are in;
  the headset run could not judge them at 28 ticks/s. Ships at 0.
- **Looking up and down: the engine's own neck** [S2b, FIXED by default]. The engine pitches
  its camera about a pivot 32 cm below and 6 cm behind the eyes; `[Neck] Mode=cancel` with
  the measured pivot ships (headset-judged right, 2026-09-03). `off` and `add` stay on F10
  Comfort as the A/B; re-judge once the frame rate is fixed.
- **The render size, the F10 picker, and sharpness** [S2b, WORKING]. The Display tab's picker
  (default: the runtime's recommended per-eye size, 2496x2688 on a Quest 3 via VDXR; tick
  `VirtualMode` for a size your display does not list) takes effect at the NEXT LAUNCH: the
  ask goes on the game's command line, the one route this build honours (its own ini and the
  console's `setres` are both measured inert). Headset-judged: "pretty sharp" at the Quest 3
  entry. The cost is the readback above: at that size use `[Capture] Mode=deferred` until the
  GPU path lands. `res: HONOURED` in the log is the verdict; `res modes`, `res <W>x<H>[f|w]`,
  `res 0x0` on the seam.
- **Motion controls are OFF by default** [S3]. `[Mode] GamepadOnly=1` makes the VR controllers
  a plain gamepad: no hand models, no motion aim, no motion melee, no motion crouch; Blink aims
  down your view. Head tracking, positional (lean/peek/crouch) tracking and the FOV lever work.
  The hands come back on the winning stereo method; setting `GamepadOnly=0` re-enables the old
  hand code, which is compiled but untested on this render.
- **The wrist HUD and the aim reticle are gone** [S3]. The game's own HUD is on the screen with
  the rest of the frame; the floating panel returns through the runtime layer's HUD quad.
- **The headset image is the game window's resolution, captured once per present** [S1]. A
  bigger window costs a bigger readback on the shipped `deferred` path (10 ms of GPU copy per
  present at the Quest 3 size); `[Device] Ex=1` with `capture mode shared` keeps it in VRAM
  (above). After a level load the mod's menu flag can stay up for a while (the state line reads
  MENU in the level, the headset shows the head-locked screen): open and close the pause menu
  once and the pair stream resumes.
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
