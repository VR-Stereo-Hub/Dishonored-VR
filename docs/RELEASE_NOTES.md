# Release notes

## 41.2.0 (unreleased) - the HUD comes back, and cutscenes get a policy

- **The HUD panel, head-locked, default OFF** (`[Hud] Panel=0`, `hud on|off|status|scale <f>`,
  the F10 Display tickbox). The game's own Scaleform HUD is taken out of the frame and shown on
  the runtime layer's head-locked quad - the return of the wrist HUD build 38.92 shipped, which
  41.0 lost with the DXVK fork. The wrist anchor itself comes back with the hands; head-locked is
  what the runtime already knows how to draw. While the panel is on the HUD is not in the eye
  textures and not in the desktop window. It runs only during stereo gameplay: menus, loading
  screens, cutscenes and the mono screen leave the HUD in the frame, the pause menu deliberately.
- **The draw census, default OFF** (`[Draws] Census=0`, `draws on|off|status|kill <key>|hud`,
  `draws unkill`, the F10 Display tickbox). The instrument the panel needed: it buckets every draw
  in a present and prints a table and a VERDICT saying whether the HUD separates from the world
  with no overlap, or that it does not. `draws kill` drops a class so a capture says by PICTURE
  what the class was. The answer for this game is in ENGINE_NOTES: the world is drawn to an
  offscreen target and the whole HUD to the backbuffer, 14 draws per present of 1205.
- **`dump hud`** writes the panel's own texture, which is the one thing that can tell a bad
  redirect from a bad copy from a bad quad.
- **Cutscenes have a policy** (`[Cine] Mode=quad`, `[Cine] HudPanel=1`, `cine
  quad|stereo|hud|latch|status`). `quad` is what has always happened, one image on a head-locked
  quad; `stereo` holds the per-eye projection through the cutscene so it has depth. Neither makes
  the camera follow your head. `HudPanel` decides whether the subtitles ride the panel (one image
  in both eyes) or stay in the frame (where each eye is a different game frame and text can
  double). The runtime layer's own `vrcine` seam is reachable at last - it had no word in this
  game, so its whole vocabulary was dead.
- No key was removed and no default changed. `[Meta] Version` is unchanged, so a tuned ini
  survives, but the new sections only appear in a freshly generated one.

## 41.1.0 (unreleased) - native stereo ships, the four headset faults, the resolution picker

- **THE EYES ARE FIXED (session 9, 2026-09-04, headset-confirmed)**: after a level load or a
  pause the two eyes disagreed - the eye tags paired draws to presents by order, and the order
  broke wherever the game thread ran ahead of the render thread (and on its own every ~2 s on
  the tester's rig), showing each eye the other's draw. The pairing follows the camera step
  measured per present now: inside one tick nothing moves the camera but the eye offset, so the
  second draw sits exactly one IPD along right of the first. Proven by A/B in the headset:
  unticking `c5 pairing` and pausing brought the fault back, ticking it on removed it.
  `[Stereo] C5Pair=1` ships; `reentry c5pair on|off` and the F10 tickbox are the A/B.
- **New defaults, all headset-judged**: `[Screen] RenderWidth=2496 RenderHeight=2688
  VirtualMode=1` (the Quest 3 through VirtualDesktopXR per-eye size, advertised so the game
  actually creates it - a fresh install used to render the game's own size and look soft) and
  `[Tracking] HeightOffsetM=-0.090` written out instead of left implicit. The F10 Display
  picker still writes another headset's size for the next launch, and `res 0x0` asks for none.
- **The one-picture state has its instrument**: the frame-identity trace (`[Perf] FrameId=1`,
  `frameid on|off|status`, status.json `frameid{}`): the `stereo: frameid` line prints, per
  left/right pair, how different the two eyes are at the backbuffer, the shared slot, the eye
  texture and the swapchain image, the camera step between the draws, the side check and the
  picture's own parallax sign. New seam words for the headset run: `reentry rearm [n]`,
  `capture reinit`. `dump eyes` writes a consecutive pair and no longer stalls the game (the
  PNG is encoded on a worker thread; the stall used to re-arm the second draw). The beat line's
  `presentTid` follows the presenting thread; pass-2 eye writes the camera seam refused are
  counted (`p2write refused=`).

- **Performance (session 8)**: `[Capture] Mode=deferred` is the default (27 vs 21 ticks/s at
  the Quest 3 size on the simulator; `capture mode sync` is the A/B). New `[Device] Ex=0|1` and
  `Managed=none|default|dynamic|shadow` (launch-time; `device ex on|off`, `device managed <m>`,
  the F10 Display tickbox): the game's device as D3D9Ex with every MANAGED texture shadowed in
  system memory, which lets `capture mode shared` keep the frame in VRAM (a fenced two-slot
  shared surface; `[Capture] SharedWait=0|1`, `capture sharedwait on|off`); 75-90 ticks/s at the
  Quest 3 size on the simulator. Off by default until a headset run has judged it.
- New instruments: the tick budget (`perf: tick` and `perf: gpu` every 3 s: the render-thread
  split per present, the BeginScene marker, a D3D9 timestamp ring with the readback's own GPU
  time; `[Perf] Instruments=1 GpuQueries=1`, `perf on|off|status|gpu on|off`, status.json
  `perf{}`), `mark <text>` and the F10 MARK button (the freeze marker with the ring of presents),
  the frame-gap line with the phase it sat in and a relative threshold, `capture mode off` (the
  A/B control: the image freezes by design), the creation census (`device census|status`,
  status.json `census{}`), the reentry beat's game-thread period, the head write's refusal
  reasons, `captureCost` in status.json under reentry too.
- Fixed: a present whose capture delivered no frame pushed the previous frame's eye tag again
  (a stale eye at every capture-mode switch); the pace guard's eaten tag is a named owner on
  the `STALE EYE` line (`eatenNoFrame`, `eaten=` on the eyes line).
- **Stereo ships ON**: a fresh ini has `[Stereo] Method=reentry Armed=1` (rung 3 is headset-
  verified). New `Armed=` and the F10 Display `stereo armed` tickbox park the game on the mono
  screen without forgetting the method; `stereo arm on|off`. An ini asking for `reentry` used
  to refuse at boot (the game side registered later); fixed.
- Fixed: the eyes could desync after a pause/resume (the second draw's gates were re-decided
  after the first draw). New `vrpace strict on|off` fail-soft (off), the `stereo: eyes` beat
  line (per-eye image age in presents), the `STALE R EYE` line, `reentry skip2 <n>`.
- New: the pair phase (`xr: pair phase`, the TRACE pairs line, status.json `stereo.pair`) and
  `vrpace ahead 0|1|2` / `vrpace lag 0|1|2`; a new `[Pace]` ini section (`Ahead=0 Strict=0
  Lag=1`, all today's behaviour) that SAVE AS DEFAULTS writes.
- New: `camera pitchtest [deg]` (the engine's own neck: measured 0.321 m below, 0.062 m
  behind the eyes) and the `[Neck] Mode=off|add|cancel` lever with `PivotBelowM/PivotBehindM`
  (the measured pivot as defaults), `neck` on the seam, F10 Comfort buttons and sliders,
  status.json `neck{}`, `camera.ceilClips`.
- Fixed: the `console` seam word (and IntroSkip) returned -1 since 41.0; latched again, and
  the re-entry through the ProcessEvent hook that then overflowed the stack is guarded.
- New: the F10 Display render-resolution picker: `[Screen] RenderWidth/RenderHeight/
  RenderFullscreen` (the names return from 40.x with a new mechanism: the ask goes on the
  game's command line at the next launch through `dishonored_vr_launch.txt`; the game's own
  ini and `setres` are measured inert) and `VirtualMode=0|1` (the proxy advertises a size the
  display lacks and creates the fullscreen device windowed). `res <W>x<H>[f|w] | modes |
  status | virtual on|off | 0x0`; `res: HONOURED` is the verdict. `[Capture] Mode=deferred`
  is the companion at the eye's size.
- New (developers): `tools\xrsim\stale-eye.xrs`, `pause-resume.xrs`, `tools\arming-hammer.ps1`,
  `@key` in `xrsim-run.ps1`, the simulator's per-eye release age (`eyeAgeL/R`,
  `projStaleSubmits`, `endPhaseMs` in state.json) and its QPC time extension; the `res:` lines
  (every adapter mode-list query with its caller). The upgrade note: `[HeadInject] FlipRoll`
  stays 1 (the roll sense is fixed in the tracker, 5513a570).

## 41.1.0 (unreleased) - SequentialReentry on the simulator, the S1 levers

- New: `stereo reentry` - the scene drawn twice per tick, once per eye, submitted as a
  projection layer (ROADMAP S2b). Verified on the simulator (two eyes half an IPD apart,
  presents = 2x ticks, no fault in a soak); awaiting the headset verdict. `[Stereo] Method`
  still ships `mono`. `stereo projection on|off|auto` forces or pins the projection layer
  (on = the mono frame in both eyes of a projection layer, for instruments).
- New: `[Capture] Mode=sync|deferred|shared` (ships `sync`) and `capture mode <m>|status`:
  `deferred` halves the per-present capture cost (measured 5.0 -> 2.3 ms at 1080p) for one
  present of latency and resolves a multisampled backbuffer; `shared` is refused by this
  game's device and says so.
- New: `[PosTrack] Lane=auto|vp|camera` (ships `auto`: the c0 matrix patch on the mono
  screen, the camera seam's own write under a projection layer, where the head's raw
  displacement drives the camera and the head roll is written) and `postrack on|off|lane
  <l>`; `camera postest <R> [U] [F]` measures the travel in uu.
- Fixed: under `[Mode] GamepadOnly=1` the title screen, the main menu and a loading screen
  read as GAMEPLAY (the script-event tracking sat inside the motion-aim block); the
  `[game] state` line now knows the main menu (its own signal) and a `LOADING` state, and
  a level load clears the cinematic latch the title screen leaves behind.
- Fixed: `tools\eye-check.ps1` failed at start (its log-path default ran before the library
  loaded).
- New (developers): the `capture: cost/present` line; the `fov:` line (aspect, lever target,
  vfov, sensor, eye size) under a projection layer; the `reentry <verb>` words (pulse, reset,
  status, census, stack, probe, findstart); the `reentry: beat` and `reentry: pair` lines;
  `tools\xrsim\reentry.xrs`; status.json `capMode`, `capShared`, `stereo.projection/camMode/
  cineActive`, `camera.posLane/...`, `stereo.draw{}`, `mainMenu`. New patterns.h entries for
  the scene-draw root (ENGINE_NOTES "The scene-draw root, derived live").

## 41.0.0 (unreleased) - native stereo foundation

- Removed: the DXVK fork (`dxvk_d3d9.dll`) and the whole `dxvk/` tree. The game renders
  natively through D3D9 again. Git history keeps the fork and its tags (`dxvk-base`,
  `dxvk-m8.2-shipped`, `dxvk-m8.4`, `dxvk-shipped`).

- Removed: the side-by-side present pipeline, the 4032x2268 window spoof, the OpenVR
  backend and the mod's own OpenXR loader/pace thread. The mod is OpenXR-only; SteamVR
  rigs use the `dvr_steamvr32.dll` shim runtime. Stereo is being rebuilt on a per-eye
  camera seam (docs/ROADMAP.md); this build shows the game on a head-locked mono screen.
- New: the mod is OpenXR-only through the runtime layer adopted from the BioShock trilogy VR
  mod; SteamVR rigs use the bundled `dvr_steamvr32.dll` shim (`[VR] Runtime=auto|native|
  steamvr`). `[VR] XrRuntimeJson` selects a runtime manifest for the launch.
- New: the stereo seam - `[Stereo] Method=mono|aer|reentry` and the `stereo <name>|status`
  seam word; 41.0 ships the mono screen (the game on a head-locked quad in both eyes, size
  from `[Screen] DistanceMeters`/`WidthMeters`); `aer` and `reentry` are design stubs that
  refuse with a note. The F10 overlay draws on that screen.
- New: the per-eye camera seam and its instrument: `camera status`, `camera eyetest <uu>
  [field]`, `camera eyefield <name>`, `[Camera] EyeField`.
- New: `[Screen] HeadLocked=1` (the mono screen follows the head; 0 leaves it standing in the
  room) and `[Paths] DataDir=` (where `command.txt`, `status.json`, dumps and the shim manifest
  go; empty = `%LOCALAPPDATA%\DishonoredVR`).
- New (developers): `capture: WxH content bbox ... (FULL|CROPPED)` in the log; `status.json`
  `stereo{}` and `camera{}`; the `stereo: beat` line; seam words `vrpace`, `vrmirror`,
  `vrinput`; the simulator's per-eye source stats and black-eye discriminator; `mono.xrs`;
  `xrsim-launch.ps1 -ViaSteam`.
- Changed: `[Meta] Version` is 10, so an older `dishonored_vr.ini` is rewritten with the
  new defaults on the first launch (the old file is not backed up: copy it first if you
  tuned it). `[Screen] DistanceMeters` defaults to 1.75 and `WidthMeters` to 2.4 (the
  mono screen). `[Screen] FovLever` defaults to 0 (off): 130 filled the old side-by-side
  render; the mono screen shows the game's own FOV.

Upgrading:

- Delete `dxvk_d3d9.dll` and `dxvk_stereo.txt` from the game folder (the installer does).
- Put a normal resolution back in the game's video options if a release before 41.0 set
  4032x2268 (`DishonoredEngine.ini` and the four `[AppCompatBucketN]` sections of
  `DishonoredCompat.ini`); `setup-game-ini.ps1 -Restore` puts the backups back.
- These `dishonored_vr.ini` keys are gone and are ignored if present:
  `[Screen] FillView, GameFOVDeg, FillScale, MenuFillScale, ZoomFillFloor, RenderWidth,
  RenderHeight, PinBackbuffer, SpoofDesktopW, SpoofDesktopH, DesktopWindowW,
  DesktopWindowH, RigidScreen, OverlayScene, EyeCant, WorldScreen, OverlayFollowTau,
  OverlayColor, XrScreenY, XrCylinder, XrFrustumFill, MirrorMode, MirrorAspect, MirrorHud`;
  `[Stereo] Enabled, Register, Separation, Convergence, Transpose` (the section now holds
  `Method`); `[VR] Backend, XrQuads, XrLayer, XrPoseDelay, StampFix, StampLive`;
  `[Mode] ForceTheater`; `[Hud] WristHud, DialogHudOff, DialogHoldMs, PanelHand, PanelSize,
  PanelUp`; `[Reticle] Enabled, DistanceMeters, SizeMeters`; `[Debug] KillMask`;
  `[Input] ClickFallback`.
- The `DISHONORED_VR_BACKEND` and `DISHONORED_VR_XR_BENCH` environment variables do
  nothing any more. `[VR] XrRuntimeJson`, `XrHaptics`, `FpsCap`, `[Screen] FovLever`,
  `KeepAliveUnfocused` and `[HeadTrack] ChainStamp` still work.
- Seam words gone: `layer`, `pace delay|stamp|fix`, `mirror`, `hud`, `dump fork|hud`.

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
