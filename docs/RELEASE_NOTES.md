# Release notes

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
