## The sword's swing trail is hidden (2026-09-21)

The swoosh the game draws on a sword attack follows its own attack animation, not the
blade in your hand, so in the headset it hung in the air beside the sword. It is now
hidden for the player's swings (enemies keep theirs). New section `[SwordTrail]`:
`Hide=1`, `Trace=1`, `Template=Sword_Trail`. Live: `swordtrail on|off`, or the checkbox
in F10 > Controls > Motion sword.

## Motion sword (2026-09-20, judged in the headset on one rig)

Swinging the right controller swings the sword again. The old detector never fired
on the stereo render; it is kept as `[Melee] Detector=sustain` and now works, and the
new default `edge` detector fires the instant your hand speed crosses `EdgeSpeed`, the way
the BioShock mod's wrench does. F10 > Controls > Motion sword shows your PEAK hand
speed and the gate that is stopping a swing. New `[Melee]` keys (all optional, the
old ones are untouched): `Detector`, `EdgeSpeed`, `RearmSpeed`, `PulseMs`,
`PulseMinPolls`, `HeadRel`, `Median`, `RequireSword`, `Output`, `HonourMs`,
`HonourHaptic`. Tuning guide: `docs/dishonored/PHYSICAL_SWING.md`.

The sneak kill: a fast stab behind an unaware guard is a swing, and the game makes it
the stealth kill. `[Melee] Stab=1` (on by default) also lets a slow, deliberate stab
while you are crouched press the attack, which the game turns into the stealth kill
behind an unaware guard. New keys `Stab`, `StabArm`, `StabSpeed`, `StabTravelM`,
`StabRatio`, `StabForward`, `StabWindowMs`, `ShoulderRightM/DownM/BackM`, and
`StabStyle` (`plunge`, the default: a raised fist driven down, for the reverse grip
the sword sits in; or `thrust`) with `StabStartBelowM`.

## Misc fixes candidate (2026-09-17)

F10 Animations adds saved arm-visibility choices for all40 player action states.
F10 View adds an experimental natural head-look option for lean/keyholes.
Texture allocation failures now log memory pressure; crash prevention is pending.

## Controller and reading controls (2026-09-17)

F10 Controls supports cross-hand D-pad modifiers, alternate menu and pause chord.
Y uses the right stick for native lean; menu scrolling is restored. Notes/books
use the accepted fixed hand attachment; its preferred angle is Reading tilt0.

## Unreleased

### VR game defaults

Restore these settings at startup: Kill Cam off, Head Bob0, Chain Climbing
Relative off, Crosshair Style off, Auto Aim off, Aim Assist off, Model Details
high, Light Shafts off, Antialiasing MLAA and Rat Shadows off. F10 Advanced's
Apply VR defaults at startup toggle saves an opt-out. Changes made after startup
remain for that session. Audio preferences are preserved.

The chain-camera displacement bug (VR-165) remains open; this preset is not a
fix for it. Head bob off at startup is headset-confirmed. All ten profile targets
match the accepted run; individual graphics effects are not independently proven.

### Reading tilt

- One live, saved reading tilt slider for notes, books and journal in F10 HUD.
  Adjusts pitch around the existing attachment center. Default0 preserves the
  accepted hand attachment. Replaces the unaccepted calibration controls.

### Controller emulation (headset test pending)

- Hold Y with the right stick for native lean; grips retain existing actions.
- Thumbrest modifiers automatically pair with the opposite stick.
- Restore continuous right-stick vertical scrolling in native non-wheel menus.

- F10 Controls: selectable D-pad modifier, left/right stick flip and X+Y pause
  chord. Modifier plus menu/chord opens the journal; menu hold is a fallback.
- Restore Y to native lean/adrenaline instead of pause. Existing controller
  settings are preserved; selecting R3 or left grip explicitly reserves its
  previous health-hold or weapon-wheel action.

### Wheel side panels and rounded wrists

- Independently position D-pad and health/mana panels alongside the hand weapon
  dial, with one shared alpha group in F10 > HUD > Weapon wheel side panels.
- Rounded wrist ends with live roundness control in F10 > Hands.
- Preserve wheel visuals through the native closing animation. Expose native
  objective scale and an optional native-HUD comparison; comparison defaults off.
- Promote the complete accepted F10 profile, including final shared side alpha.
  Objective ownership misses and rare exit zoom remain tracked under VR-129.

### HUD follow-up

- Shared alpha for notes/books/journal and independent interaction alpha; original
  general-alpha reset preserves all specialized groups, including wheel alpha.
- Continuous reading vertical stick input replaces sparse menu pulses.
- Reader panels follow grip rotation; wheel entry and closing retain their own
  origin, crop and alpha. Pause has independent alpha and continuous navigation.
- Interaction grouping survives crouch transitions; alpha correction preserves hue.
  Native objective icons retain game tracking; nearby text association is heuristic.
- Scoped pause scene freshness removes reported world-depth interruptions. Menu
  exit heading can carry the physical head turn into gameplay once. Experimental
  controls retain repository defaults; pause hand-size variation remains open.

### Accepted weapon dial and reading panels

- World-space left-hand weapon dial supports continuous stick/hand direction,
  camera-facing orientation, circular crop, independent size/distance and alpha.
- Notes/journal follow the left hand with independent width, depth and horizontal
  placement. Scoped menu head look and UI-blur controls preserve world rendering.
- Bounded stereo gap hold removes reported reading flicker and greatly reduces wheel
  flicker. Residual head-motion left-eye hand/weapon flicker is tracked in VR-128.
- HUD content continuity and private element textures reduce plane switching;
  objective/interaction grouping remains in VR-127. The complete accepted F10 profile
  is now the generated, packaged and golden default, at explicit user request.

### Performance and accepted profile

- Scoped gameplay FOV defaults to103 degrees. F10 offers live FOV adjustment and
  aspect-preserving total-pixel resolution scaling with an explicit live Set button.
  Resolution defaults to120% (3012x3122 per eye).
- Desktop mirror defaults off, including capture-gap refresh during a running XR
  session. Graphics-error/stopped-session fallback remains. Strict mode was accepted
  in headset testing with zero desktop Presents across621 recorded windows.
- The complete accepted saved HUD/hand/crouch profile is now the generated and
  packaged INI default, including its existing diagnostic settings. Pair pacing
  and automatic benchmarks remain off. Earlier all-window HUD defaults below are
  historical and superseded by this profile.
- Fixed an idle F10 Display control overwriting the automatic FOV target and causing
  repeated projection-scale changes. Research and failed routes are in PERFORMANCE.md.

### Added

- **A real alpha for the HUD quads (VR-119).** The quads' transparency can now come
  from the sink's own coverage instead of `max(r,g,b)`: `[Hud] AlphaMode=captured`
  makes the redirect force the coverage blend equation on every HUD draw, so dark
  strokes (text outlines, the bars' edges) keep their weight; `mix` takes the larger
  of the two. Alpha gain, an alpha floor for thin strokes, a gamma nudge and a
  backdrop plate per anchor (`Backdrop.window`, `Backdrop.hand`) are F10 sliders on
  the HUD tab and `hud alpha ...` words. Ships at `repair` with every control at
  identity (the 41.2 picture); `dump hud` now writes the alpha channel as a grey PNG.
- **Every HUD element on its own anchor (VR-120).** The HUD tab is now a table: each
  element (the vitals, the reticle, the interaction prompt, and rows for the
  equipment, subtitles, objective marker, toasts, tutorials, detection arrows, skip
  gauge and dark vision to be named as they are measured) and each in-game screen
  (pause, note, journal, wheel, store, mission stats) rides `off`, `frame`, the
  head-locked `window`, the `world`-parked window, or the `handL` / `handR` panel,
  with its own placement; the two hands have their own size, lift, orientation and
  tilt (`[Hud] HandL.*`, `HandR.*`). An element the mod has not named rides `default`
  and shows in `hud list` with a count. Ships with everything on the window, so the
  picture is VR-117's until the headset judges the split; `[Hud] Regions=0` is the
  one-quad A/B. VR-117's single-hand keys are read once and rewritten.
- **The HUD's element census (VR-118).** The region probe reads each HUD draw's
  rectangle through the vertex shader's own transform (found by disassembling the
  shader at first sight, never by a hard-coded register), and `draws regions` lists
  the elements it sees by rectangle and frequency. `draws vsdump` writes a HUD
  shader's disassembly under the data directory.

- **The HUD on its anchors (VR-117).** The game's HUD leaves the eye textures and
  is shown on quads the headset composites: a window in front of you (head-locked,
  or parked in the room where you recentred) and the tracked hand, the return of
  build 38.92's wrist HUD. Which anchor each element rides, where it sits, and the
  window's and hand panel's own placement are all F10 controls on the new HUD tab
  and `[Hud]` keys. In-game screens (pause, journal, notes, store, mission stats)
  ride the window with the world in stereo behind them, and resuming never drops
  the projection; the main menu and loading screens keep the mono screen. Ships ON
  (`[Hud] Panel=1`) with the window preset (1.25 m wide at 1.30 m) and the whole
  HUD as one element; per-element routing (`[Hud] Regions`) waits on the region
  measurement. `hud off` puts the HUD back in the frame.

### Changed

- Image-linked world head orientation is enabled by default after repeated
  headset confirmation at 120 Hz. Physical head turns remain substantially
  smoother even with uneven game frame delivery. The tested INI ships verbatim;
  invalid image records retain numeric-lag fallback. Hand timing is unchanged.

- The complete September 14 accepted INI and saved F10 profile are now the generated
  and packaged defaults, by maintainer request. This includes diagnostic flags,
  calibration values and the configured data directory. Existing INIs keep their
  overrides; the config version is unchanged.

- **A fresh install now gets the headset-confirmed configuration (VR-72).** The
  generated default ini and the loader fallbacks were set from the tested machine's
  ini: world pose lag 2, both automatic A/B experiments off, the bone palette and
  weapon placement on with the per-eye weapon offset, native projectile aim, hands
  and motion controls on (`[Mode] GamepadOnly=0`), and the F10 panel's saved keys.
  An existing ini is not rewritten (no config-version bump), so it keeps its values;
  delete it to take the new defaults, then recapture your own hand calibration.

### Fixed

- **Crouched, the view pitches with the head again (VR-122).** On a crouch the hands' tuck
  (the arms handed back to the game's crouch animation) also switched off the camera's own
  look-at bone control, so the crouched render stayed level while the head pitched and the
  world appeared to move with the head. The tuck now leaves the camera control alone
  (`[Hands] CrawlTuckCamera=0`; `1` is the old behaviour, `hands tuckcam on|off` live).
  With that control kept the crouched camera pitches about the same neck as standing, so
  the crouched pivot keys (`[Neck] CrouchPivot*`, VR-78) now apply only while the tuck has
  released the camera control; otherwise a crouch keeps the standing pivot. Measured on
  the simulator: crouched residual at 30 deg matches standing to 0.05 uu. The run now logs
  `config: [Hands] CrawlTuck=.. CrawlTuckCamera=..` and names what each tuck did to the
  camera control.

- Cinematic free look stays natural through tilted authored cameras and dialogue
  framing. Pitch/roll comfort controls preserve physical head motion; cinematic
  zoom and its exit blend retain a full-size view. Native hands/arms and mantle
  animation resume game control during those actions, then return to tracking.
- Head-based movement avoids the post-cutscene heading drift in the retained
  character-oriented mode. Standing pitch compensation no longer adds a curved
  translation when rolling while looking steeply up or down.
- Mono screens anchor in front of the player, with per-context exceptions and
  recenter. Current menu ownership prevents the underground main-menu camera
  and stolen controller navigation. Loading remains mono through Continue,
  then releases to stereo without the observed post-load mono interruption.
- Crossbow tracking survives cinematic level travel. Weapon-specific lens
  cancellation preserves strict identity checks; identity roundoff bypass and
  same-view pass consistency resolve the observed partial-surface transparency.
- These accepted settings and the complete saved diagnostic profile now default
  on as recorded in dishonored/STACK_ACCEPTANCE.md. Existing INI overrides remain.

- Pausing after a crouched save reload no longer receives corruption from the
  crawl-release writer (VR-96). It validates current object identity before
  restoring strengths to cached hand controls.
- Startup reaches stereo without a jump/crouch. Fast notes remain responsive
  across reloads, and a confirmed adjacent R/0 capture sequence no longer places
  the left image in the right eye. Initial weapon tracking can still stall.

- **Looking up or down while crouched no longer moves the view (VR-78).** Crouched,
  the game does not swing its camera about a neck, but the mod was still cancelling
  that swing, so looking down pushed the view back and up and looking up pulled it
  forward and down. The crouched pivot is now its own setting,
  `[Neck] CrouchPivotBelowM` / `CrouchPivotBehindM`, shipped at 0. An existing ini
  without those keys takes the fix automatically; `-1` restores the old behaviour.

- **Weapon eye correction survives queued draws and downward camera clamps
  (VR-69, PR #33).** The restored render-side decision no longer resets when a
  newer script tick changes its stereo state. A Z-only ceiling clamp now preserves
  ownership of the previous camera offset, preventing repeated eye displacement.
  The controlled headset build retained stable head turns and weapon swaps and
  eliminated the reported descent/crouch flicker. Brief startup settling remains.

- **The arms and weapon no longer follow your head (VR-30).** Turning your head
  leaves the viewmodel where it is; the right stick still turns you; doing both
  at once works. The mod now intercepts the engine's own body-facing operation
  and asks it to face a heading with your head's contribution removed, instead
  of writing the pawn's rotation behind the engine's back.
  `[Camera] ArmBodyFacing=1` enables it; `arms facing off` is the live A/B.

### Upgrading

- Retired `PaletteEyeAlternate` and `PaletteEyeFromPass` settings no longer select
  the active weapon eye decision. `PaletteEyeOffset` remains available.
- A fresh ini now aims projectiles natively (`[MotionAim] Enabled=0`, VR-72). An
  existing ini keeps its own value; set it to 0 for the game's reticle direction.
  Post-reset shot verification is still pending.

- **`[Camera] BodyYawLock` is removed.** It was measured futile (4 of 186 writes
  survived to the next dispatch) and is superseded by `ArmBodyFacing`. Delete the
  line from your ini; it is ignored if left.
- `[Camera] ArmStripMeshRot` is new and ships OFF. It is kept only as the
  reproducible A/B for a documented dead end and has no effect worth enabling.

# Release notes

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

### Pending validation: cinematic FOV and native arms

Default-off LockFov requests a stable VR field of view during cinematic and
dialogue scenes. Default-off CinematicHandBack gives the game control of hands,
weapons and arm visibility during those scenes. Combined candidate230 awaits
headset evaluation; not a release or confirmed result. See VR-50 and VR-104.

### Pending combined follow-up

Cinematic FOV override now covers the native zoom's exit blend. Optional LockPitch
suppresses forced animation tilt while preserving physical headset tilt and
camera height. Optional MantleHandBack returns mantle hands to native animation.
These follow-ups await a combined headset test; no release is declared.

Pending cinematic candidate: independent authored roll suppression, upright
head-turn composition on tilted cameras, and final head tracking across dialogue
camera blends. Headset validation pending; no release declared.

Pending VR-106 candidate: default-off UprightPitchArc removes near-vertical roll leakage from standing neck compensation and positional axes. Headset acceptance pending.
