# Status

## Current state (2026-09-02, session 7c: the resolution fault is the FOV CLAIM)

**For the S2b developer.** Your `bbd04fec` is built, installed and was run in a headset by
the user. Rung 3 boots into stereo and runs. Two faults dominate what he saw, and the first
one is now understood.

### 1. THE RESOLUTION FAULT IS THE FOV CLAIM, NOT THE RENDER SIZE

The tester found it by arming the FOV lever from F10: the world became "a tiny square really
far away" with **"extremely good resolution and depth"**. Same render, same headset, only the
claimed FOV changed - a controlled experiment, and it says the render size was never the
problem.

At 16:9 the layer must claim **137 deg** to cover the eye's ~110 deg vertical
(`tanH = tanV * aspect`), and the log agrees the engine really renders it
(`fovaudit ... hfov 136.99 deg, src=readback`). The cost, on this rig (eye 2496x2688,
half-angles h=54 v=55):

| render | claim | centre px/deg | covers the eye |
|---|---:|---:|---|
| 3840x2160 | 137 (auto) | **13.2** | yes |
| 3840x2160 | 100 (lever) | **28.2** | ~47% vertically - the tiny square |
| the eye | 108 | 15.8 | - |

**Over half the resolution is spent on periphery outside the frustum.** It also explains the
null result that ate an hour: 1080p and 4K look IDENTICAL in the headset, because both are
16:9 and get squeezed by the same factor.

**The fix is the render ASPECT.** The eye is 0.928; at that aspect the covering claim is
~105.6 deg and there is no trade at all. Measured this session by probe (launch, read
`CreateDevice`, kill):

| requested | got |
|---|---|
| windowed 2496x2688 | 1304x1405 |
| windowed 2560x2880 | 1248x1405 |
| windowed 3840x2160 | 2497x1405 |
| **fullscreen 5120x1440** | **5120x1440 - honoured exactly** |

Windowed is hard-capped at **1405 rows** here (desktop 5120x1440 at 125% DPI, minus the
caption). Fullscreen takes a real display mode verbatim, so a near-square render is reachable
if a near-square MODE exists. Three routes, cheapest first: a virtual display at ~2560x2688
(this machine has three virtual display adapters, untested); the size spoof `99d4f576`
removed (`res_spoof.cpp`, 594 lines, recoverable verbatim from `99d4f576^`, and the original
author calls the `GetClientRect` lie load-bearing); or pin `FovLever` ~105-115 and accept a
border. ENGINE_NOTES carries the arithmetic and the BRVR comparison - BRVR claims the GAME's
fov and never inflates it, which is why a modest windowed render looks fine there.

**Whatever is chosen, the capture cost scales with it**: 3840x2160 measures **14.4 ms per
present** (`31.6 MB each way, mode=sync, lock=9616us`). Near-square 2496x2688 is 26.8 MB.
S1's readback item has to land before that is playable.

### 2. HEAD TILT: the roll write is honoured, so it is not just a sign

`FlipRoll=-1` was applied and the telemetry shows the write negated AND kept
(`roll ON incoming=172 wrote=152`, incoming tracking the previous write). Tilt was still
wrong, which leaves DOUBLE APPLICATION: the compositor rotates the image for head roll and
`ApplyHeadToViewRotation` writes roll too, and `rollNow` is FORCED true under
`stereo::wants_projection()` so `[HeadTrack] Roll=0` cannot switch it off.

A three-way A/B is in for it, because the question is three-way: `headroll 1|-1|off` on the
seam, and the same three buttons on the **F10 Comfort tab** (the tester is in a headset).
`g_rollForceOff` gates `rollNow`. **NOT YET RUN** - one run answers it.

### Also landed this session

- **`[Stereo] Method=reentry` could never take at boot.** `EnsureConfig()` selects the method
  inside `Direct3DCreate9`, and `DvrInstallFrameHooks()` registers the scene-draw hooks later
  in that same function, so the ini always refused with "the game side has not registered".
  `select()` now remembers a not-yet-available method and `set_reentry_hooks()` retries it.
  VERIFIED in a headset run: `'reentry' was asked for before its game side was up - retrying
  now` -> `method mono -> reentry`, beat `L/s == R/s == out/s / 2`.
- The game config in `Documents` was still carrying pre-41.0 leftovers (2750x2850 in
  DishonoredEngine.ini and all four AppCompat buckets); reset to a real size, console bind
  added, `MaxAnisotropy` 4 -> 16, `MaxShadowResolution` 800 -> 2048. **`MaxMultisamples` left
  at 1 on purpose** - ENGINE_NOTES records a `GetRenderTargetData` failure on a multisampled
  backbuffer, so MSAA would likely break the capture path.
- `tools/boot.ps1` passed `Enter` positionally to `game-key.ps1`, whose first positional
  parameter is `-GamePath`, so unattended booting never pressed a key.

### The AER branch is parked

`aer-rendering` is pushed and complete. Its findings that still apply to rung 3 are in
ENGINE_NOTES ("Three things BRVR does that our AER does not"): the projection layer should
carry the pose the image was RENDERED from (BRVR: "a major flicker fix"), and anything flat
must take ONE eye. The world-advance-per-eye-pair one does NOT apply to reentry - rung 3
draws both eyes from one tick, which is the argument that settled the ladder.

### Next run, in order

1. F10 Comfort -> head tilt: `match` / `oppose` / `no write`, tilting both ways after each.
2. `fov 100` -> `115` -> `125` -> `137` -> `0` and report where sharp turns into filled. That
   crossover is the number the aspect fix is sized against.
3. `capture mode deferred` (14.4 ms -> should roughly halve).

## Current state (2026-09-03, session 6: S2b, SequentialReentry on the simulator, 41.1)

**Two headset runs of rung 3 (runs 30 and 34, Quest 3 via VDXR, the user). Run 34: stereo
good, head motion still wrong - and the cause is measured (run 35): the camera field's SIGN
was inverted, so every lean, crouch and per-eye offset went the WRONG WAY. A differential
picture test against the headset-proven c0 patch settles it (ENGINE_NOTES, "The camera field
holds the POSITION, c5 is its negation"): the field holds the camera's world position and c5
is its negation; session 5's eyetest had read c5 as the position. Fixed, verified on two
axes by picture, both instruments still HONOURED, the doubling unaffected. The eye offset
was inverted with it, so the stereo depth in both headset runs was inside-out - it needs
re-judging. NOT yet re-tested in a headset. Run 30's two findings (below) are also fixed.**

**Run 30 FAILED: both frames alternating in both eyes, lean reversed, a second motion on pitch.** Both causes are in the
log and fixed in the tree (ENGINE_NOTES "The scene-draw root", the headset paragraph), NOT
yet re-judged: (1) the method's pairing check dropped a third of the LEFT tags while the
player walked (the engine moves the camera a tick after the write; the check is telemetry
now, the ring's order pairs the eyes); (2) under a projection layer the compositor moves
the image for the head's real displacement and roll, and the game camera followed neither
(the tuned lean with its deadzone and re-centring bleed, `[HeadTrack] Roll=0`); now the
raw head displacement drives the camera lane in the yaw-only frame (`[PosTrack] Lane=auto`,
30 cm reads +29.4 uu held steady on the simulator, run 32) and the roll is written. A
third fix from the same runs: the cinematic latch could stick CINEMATIC across a level load
(the second draw gated off for the whole level, run 32); it clears when the main menu goes
and on a loading screen. The doubling itself worked on the headset: draws/s 54 = 2nd/s 54,
presents 108, pair pacing live, the pair line 6.08 uu. Everything below stands as measured
on the simulator.

**Rung 3 renders true per-eye stereo on the simulator.** `stereo reentry` patches the one
gameplay call of the viewport draw root (derived live this session: `kViewportDraw
0x5fc5b0`, called `push 1; call` from UGameEngine::Tick at `0x6330da`; ENGINE_NOTES "The
scene-draw root, derived live") and calls the root a second time per tick with eye +1
written into camera+0x330 between the passes. Measured in the sewers level (runs 27-29,
the simulator at 90 Hz, 1920x1080): `reentry: beat draws/s=53 2nd/s=53 presents/s=106`,
`stereo: beat method=reentry out/s=104 L/s=52 R/s=52 mono/s=0`, the second call 220-470 us,
every gate's skip counter 0, no fault in a 90 s soak, and the pair line **the +1 present's
c5 sits (0.02 6.17 0.00) uu from the -1 present's (ipd*scale = 6.17)** on every pair. The
capture pair shows the parallax on the near pipe. `reentry.xrs` and `stereo.xrs` pass (two
projection views, both eyes ~70 % non-black), eye-check legs 0-1 pass (legs 2-5 carry
BioShock's bands: KNOWN_ISSUES), `stereo mono` restores the call site. `[Stereo] Method`
still ships `mono`; the headset verdict is the user's (below).

**The S1 levers, measured (all ship OFF):**

- Capture: the shipped path costs ~5 ms per present at 1080p, all of it `LockRect` waiting
  on the queued readback; the D3D9Ex shared surface is REFUSED by this device (not 9Ex);
  `[Capture] Mode=deferred` (queue the readback, lock it one present later) measures
  2.3 ms with `mono.xrs` passing (runs 16-19; ENGINE_NOTES "The capture cost, measured").
- Positional tracking on the camera seam (`[PosTrack] Lane=camera`): `camera postest`
  HONOURED within 1-2 % on all three axes in gameplay (+30.0 right, -24.4 up, +39.7
  forward); the basis rows +0x50/+0x60/+0x70 read orthonormal (run 20 on the attract
  camera, run 21 in the sewers).
- FOV from the render size: under a projection layer the lever target follows the
  runtime's circumscribed hfov (137.0 at 16:9) and the layer claims the 0x53c sensor
  (`fovaudit src=readback`, the sensor ramping 136->137 within a second). Measured at
  16:9 only: the game did not take `ResX/ResY` from its ini, so the second aspect is open.

**Found on the way (each fixed):** under `[Mode] GamepadOnly=1` every script-event tracker
sat inside the motion-aim block, so the title screen, the main menu and a loading screen
read GAMEPLAY (the earlier "gameplay" instrument runs were on the attract camera; the
loading screen's static camera reads DISCARDED for every write); the runtime's projection
path had no caller in this game (camera mode was an overlay checkbox and a stale verdict
pins the quad); the cinematic latch the title screen toggles ON survived a level load; the
tag ring cleared every few seconds because the game thread runs a frame ahead of the render
thread; `eye-check.ps1` failed at start (a param default calling the library early).

**Not verified**: the headset (fusion, per-eye reflections, flicker on fast motion, the
tick rate halving); the SteamVR shim with Dishonored; `aer` (Developer A's stub, untouched).

## Next steps (one paragraph per developer)

**The user (the headset run, Quest 3 via VDXR)**: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active, reach gameplay, then `tools\game-cmd.ps1 "stereo reentry"`
(`$env:DVR_DATA_DIR='D:\dvr-data'`). Expect: the world in stereo at full frame rate
per eye, the game's tick halved (the F10 overlay and the log's `heartbeat: GAME=... ticks=`
say by how much), reflections and post effects per eye. Judge fusion, world scale, and
fast head motion; then `capture mode deferred` (one present of latency for 2.7 ms back);
then `postrack lane camera` (lean/crouch through the camera write). Quit through the menu.
Send `dishonored_vr.log`; the verdict picks the defaults for `[Stereo] Method`,
`[Capture] Mode` and `[PosTrack] Lane`.

**Developer B (S2b follow-ups)**: the eye-check bands recalibrated on this game after the
headset verdict; the second aspect (how the game takes its render size now that the window
machinery is gone; `setres` through the console seam is the lead); the tick-rate cost
(the second draw is a full scene draw: measure on the headset rig, consider a per-eye
resolution below the window's); the F10 overlay is drawn into both eye textures at
infinity; the frame-path items still per present under two presents per tick (TrackHead,
the pad composition) are harmless but wasteful.

**Developer A (AlternateEye)**: `core/gfx/aer.cpp` untouched; the seam now carries
`wants_projection()` (return true), the projection arming and the FOV handoff for free,
and `capture::set_pending_tag/delivered_tag` for the eye tag under `deferred` capture.

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam`, foreground the window,
then `tools\game-key.ps1 -Key Return` three times to reach the sewers (VERIFICATION gotcha
15), `tools\xrsim-run.ps1 -Path tools\xrsim\reentry.xrs -Dir D:\dvr-data\xrsim`. Stop the
game with Stop-Process, never WM_CLOSE. Copy `dishonored_vr.log` to `D:\dvr-data\logs`
before every relaunch (this session's runs are `42-run16..29-*.log` there).

## Blockers

- The headset verdict on rung 3 needs the user.
- The second aspect for the FOV law needs a way to change the render size (KNOWN_ISSUES).
- **WM_CLOSE leaves a stuck `Dishonored.exe`** (session 5): close a healthy game with
  `Stop-Process`; a menu quit is clean on the Quest (run 15).

## Session log

### 2026-09-03 - session 6: S2b - the capture cost, the lanes, the root, the second draw

Branch `claude/s2b-stereo-scene-draw-a341c5`, seven commits on `VR-Main` (24b22390): the
capture cost measured and the modes, the pipelined deferred capture, positional tracking on
the camera seam, the projection claim and the FOV handoff with the state-gate fixes, the
root derivation and the second draw, the docs. Runs on the dev PC (simulator lane, logs in
`D:\dvr-data\logs\42-run*.log`):

| Run | What | Result |
|---|---|---|
| 16 | capture modes | probe: shared REFUSED; sync 5.4 ms, deferred (first form) 5.2 ms: no gain; `mono.xrs` PASS both |
| 17 | user-memory surface | REFUSED (D3DERR_INVALIDCALL), fell back to sync |
| 18 | the lock split | sync: lock 2.4-3.1 ms, copy 0.7, upload 1.5; deferred first form: the lock still waits |
| 19 | deferred pipelined | lock 0, total 2.25-2.4 ms; `mono.xrs` PASS |
| 20 | postest | camera lane HONOURED on all axes - on the attract camera (the state mislabel found in run 21) |
| 21 | projection on the mono screen | two views, sensor 137, claim readback; the pictures were the title screen, then the loading screen (DISCARDED there), then the sewers: eyetest 120/120, postest +30.0 in real gameplay |
| 22-24 | the state gate | menu/cine tracking hoisted, main-menu flag, LOADING state, the cinematic latch cleared on a new pawn: title MENU -> quad, load LOADING -> quad, level GAMEPLAY -> projection |
| 25 | 1440x1440 | the game stayed 1920x1080 with ResX/ResY=1440 in both ini places; second aspect open |
| 26 | census + scrapes | PVR from one site once per present; render thread presents; the draw chain to the HUD PostRender |
| 27 | tick chain + probes | both chains under UGameEngine::Tick; the root 0x5fc5b0 named from the bytes at 0x6330da; pe-xref confirms every edge |
| 28 | first light | pulse: 3 second draws at 218-414 us, presents +1 each; `stereo reentry`: L/s 54 R/s 53, pair c5 travel 6.17 uu, no fault; the ring cleared every few seconds |
| 29 | the ring fix + soak | L/s 52 R/s 52 mono 0, ringCleared 0, 90 s clean; `reentry.xrs` 11/11 |
| 30 | **the headset (Quest 3, VDXR, the user)** | the doubling ran (draws 54 = 2nd 54, presents 108, pair 6.08 uu) but L/s=36 R/s=54 mono/s=18: left tags dropped by the position check while walking -> both frames in both eyes; lean reversed, a second motion on pitch (the head's displacement and roll not driven under the projection layer) |
| 31 | the fixes, sim | tags never dropped: L/s 53 R/s 53 mono 0; `reentry.xrs` 11/11; the lean under projection read 13.7 uu for 30 cm (the reference had crept) |
| 32 | the stable reference | 30 cm -> +29.4 uu held for 10 s, crouch/forward signs right, postest HONOURED; CINEMATIC stuck across the load (the pawn latched before the title toggle) |
| 33 | the cinematic latch | `cine: latch cleared - leaving the main menu`, LOADING -> GAMEPLAY, L/s 53 R/s 53 mono 0 |

### 2026-09-02 - session 5: the state as session 5 left it (archived)

**The render is restarted on a native D3D9 game.** The DXVK fork, the side-by-side present
pipeline, the 4032x2268 window machinery, the OpenVR backend and the mod's own OpenXR
loader/pace thread/input are removed (one commit each, so `git revert` restores one piece;
history keeps the fork under the `dxvk-*` tags). The BioShock trilogy mod's OpenXR runtime
layer is the single backend (`core/vr/openxr_runtime`, verbatim behind two D3D9 seams: the
device provider and the frame texture), the static Khronos loader is linked into `d3d9.dll`,
and SteamVR rigs go through the bundled `dvr_steamvr32.dll` shim. Stereo is a SEAM with named
methods (`core/gfx/stereo.h`: `[Stereo] Method=mono|aer|reentry`, `stereo <name>` live):
the mono screen (rung 1) works, `aer` and `reentry` are registered design stubs with their
notes. The per-eye camera seam (`game/dishonored/camera`) carries rotation (measured), FOV
(measured) and the lateral eye offset (unmeasured, with the `camera eyetest` instrument).
Version 41.0.0, `[Meta] Version=10`. ARCHITECTURE and ROADMAP (S0-S3) describe it.

**Verified on the dev PC (the game IS installed here, `D:\SteamLibrary`), simulator lane**,
build `g4fb67333` and later, 2026-09-02 evening, eight runs:

- `xrsim-selftest.ps1` PASS; `xrsim-launch.ps1 -ViaSteam` reaches `xr: instance created on
  runtime 'dvr-xrsim'`, `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, session FOCUSED,
  `xr: first frame submitted to the headset (1600x900 quad)`, frames advancing at the sim's
  90 Hz (`stereo: beat method=mono out/s=90`).
- `status.json`: `state GAMEPLAY` (the game auto-continues into the last save), `stereo.method
  mono`, `framesOut` advancing, capture bbox `100% x 100%`, 97% non-black, `camera.c5ok true`.
- `stereo aer` / `stereo reentry` refuse with their note and mono keeps running; `stereo mono`
  is a no-op; `camera status` prints.
- `xrsim-shot`: a quad layer whose SOURCE reads 97.2% non-black in BOTH views with a full bbox;
  the composite reads L 37.95% / R 37.98% (world-locked quad, run 7) then L 16.4% / R 16.3%
  (head-locked quad, run 8), no `COMPOSITOR fault` / `APP fault` line. The session-4 black
  left eye did not reproduce; the simulator now attributes it if it does.
- `dump frame` writes `capture_*.bmp` (5.7 MB) and `eye_*_mono.png`; `soak.ps1 -Minutes 3`
  exit 0 (PASS, no wedge, no dumps); the crash file carries the run headers.
- `camera eyetest 100` in gameplay: run 7 wrote nothing (the lever off = no camera revalidation;
  fixed), run 8 wrote all six candidates and measured a CONSTANT offset between the draw's c5
  and each field (+6620 uu for 0x80/0x90/0xc4, +14140 uu for 0x330/0x350/0x374 along right):
  the fields are not c5's quantity in c5's frame, so the measure was redesigned around a
  per-candidate c5 baseline (commit `cf9ec6f2`). Runs 10-11 (after the stuck process cleared
  on its own, no reboot): **camera+0x330 HONOURED 119/120** (+99.2 uu of the asked +100; it
  holds -c5 exactly, so the write is negated), the other five DISCARDED. The eye-offset write
  point is measured; `[Camera] EyeField=0x330` is the default (ENGINE_NOTES has the table).
- `mono.xrs` PASS (both eyes 12.9%, equal bboxes, no fault line) and the head-lock pair on the
  fixed simulator: the composite bbox is IDENTICAL at yaw 0 and yaw 30 (run 9).

**Found on the way** (each fixed in its own commit, all measured, none guessed):

1. The game calls `Direct3DCreate9` twice; the second `init_instance` failed with
   `XR_ERROR_LIMIT_REACHED` and the fallback chain declared VR off (guarded).
2. The handoff's trap 6 is real: a DIRECT exe launch crashes at the main menu
   (`Dishonored.exe+0x60907e` reading NULL, thread "other", right after
   `DisGFxMoviePlayerMainMenu Start`); a Steam launch survives it. `xrsim-launch.ps1 -ViaSteam`
   exists for this and is the only way to run the simulator with the game here.
3. The agent's shell on this PC VIRTUALIZES writes under the user profile: files the harness
   wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) existed for the
   shell and a game it launched directly, and not for a game launched through Steam (its
   listing held only game-written entries; a WMI-created `dir` agreed). `[Paths] DataDir=` in the
   ini and `DVR_DATA_DIR` for the scripts point both at `D:\dvr-data`; the simulator takes its
   state dir from the manifest's directory (VERIFICATION gotcha 14).
4. The loader's property store beats the environment: `init_instance` hands `[VR]
   XrRuntimeJson` to `xrInitializeLoaderKHR` (XR_EXT_loader_init_properties) as well, and logs
   whether the manifest is readable and its library loads.
5. The config's version rewrite dropped `[VR] XrRuntimeJson`; it now carries `XrRuntimeJson`,
   `Runtime` and `DataDir` over.
6. The fresh ini armed `FovLever=130` (the side-by-side value) and wrote it 600 times per 3 s
   into a 90-deg camera; the lever ships off on the mono screen.
7. The mono quad sat in BioShock's world-locked LOCAL space; it is head-locked now
   (`[Screen] HeadLocked=1`).
8. The SIMULATOR composited quads in the wrong place (60 px outward per eye at yaw 0, 460 px
   of swing at yaw 30): the cbuffer matrix was read column-major and the view matrix's rotation
   block was transposed. Fixed (`d43eea11`), selftest PASS, and the re-measure passed (run 9:
   identical bboxes at yaw 0 and 30). BioShock's eye legs never saw it (projection layers
   rotate rays in the shader).
10. The c5 capture only caught an upload STARTING at register 5; after the two device Resets
   a level load brings, the engine batches it into a c0 x128 block and the seam saw no c5 for
   a run (run 9). Any block covering c5 feeds it now (`dd10da09`).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Headset: verified** (run 13, 2026-09-03): Quest 3 through VirtualDesktopXR, the game on the
head-locked screen in both eyes, head tracking and the gamepad working. The quit crashed on
that run (the 38.79 class: VD's thread through a freed d3d11 pointer after PreExit with the
session still open). The handler had sat inside the motion-aim block since 38.79 and never
ran under GamepadOnly; hoisted, it closes the session from PreExit and the third quit (run
15) was clean: `shutdown: game PreExit`, `xr: session teardown`, `instance destroyed`,
`proxy unloading`, no exception.

**Not verified**: the SteamVR shim with
Dishonored; `apply_eye_offset` driving a real per-eye render (no method asks for an eye yet);
`head_track`/`pad_bridge` as real modules (deferred, S1).

#### Session 5 next steps (superseded by the list above)

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini carries
`[Paths] DataDir=D:\dvr-data`), foreground the window, `tools\xrsim-run.ps1 -Path
tools\xrsim\mono.xrs -Dir D:\dvr-data\xrsim`. Close the game with Stop-Process while it is
healthy, never with WM_CLOSE (blocker below). Copy `dishonored_vr.log` out before every
relaunch. The eyetest is done: the eye offset writes into camera+0x330 in negated form
(`camera::apply_eye_offset`); `camera eyetest 100` re-measures it on any build.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The eye field is measured
(0x330), so the method only has to alternate `eye_for_next_frame()` and tag each present; the
seam writes the offset on the script lane. Acceptance: `stereo aer`
accepted, the beat line `L/s == R/s == out/s / 2`, `stereo.xrs`, `eye-check.ps1` legs 0-5, the
runtime's pair probe clean.

**Developer B (SequentialReentry, S2b)**: read `core/gfx/reentry.cpp`. Task one is the
scene-draw root (caller census at `ApplyHeadToViewRotation`, live stack scrape, identify the
pass by making it MOVE with the eyetest as the mover); every address to `patterns.h` with its
derivation in ENGINE_NOTES; the second call deny-by-default and SEH-guarded.

**The user**: the headset run on Quest 3 via VDXR: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active (SteamVR not running), expect the game on a head-locked
screen in both eyes, head rotation turning the view, the gamepad working; F10 for the screen
size; send `dishonored_vr.log`. Quit through the game's own menu and report whether the process
lingers.

#### Session 5 blockers (superseded)

- **WM_CLOSE leaves a stuck `Dishonored.exe`** (one thread, unkillable, holds `d3d9.dll` and
  the build's `dvr_xrsim32.dll`, Steam refuses a relaunch). Pid 13452 cleared on its own after
  about an hour, no reboot. Until the quit path is understood, close a healthy game with
  `Stop-Process`, which works.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The headset run needs the user.

### 2026-09-02 - session 5: the native-stereo foundation (41.0)

The decision (docs/ARCHITECTURE.md decision log, session 5): four headset sessions showed the
DXVK side-by-side design cannot be tuned; the game renders natively again and stereo is rebuilt
as a ladder of methods on one seam, two developers taking rungs 2 and 3. One PR
(`claude/native-stereo-foundation-77e2b6` -> `VR-Main`), 27 commits: seven removals, the
static loader, the runtime layer, the shim, the stereo seam + mono screen, the camera seam +
eyetest, the stubs, the simulator instruments, the harness, the docs, then the fixes the
first runs demanded (above, "Found on the way").

Runs on the dev PC (simulator lane; logs in `D:\dvr-data\logs\41-run*.log`):

| Run | Launch | Result |
|---|---|---|
| 1 | direct exe | instance on dvr-xrsim, then init_instance twice -> VR off; the game CRASHED at the main menu (`Dishonored.exe+0x60907e`, trap 6) |
| 2 | Steam | the ini rewrite dropped XrRuntimeJson -> VDXR (no headset), flat |
| 3 | Steam | env var set but the loader answered RUNTIME_UNAVAILABLE |
| 4 | Steam | loader property override set; manifest "path not found" (err 3) - the sandbox finding |
| 5 | Steam | the path probe: the game sees 5 entries where the shell sees 7 |
| 6 | Steam (no VR) | an AV in `d3d9.dll+0x87c95` after a Reset + RTD failure, three times, survived |
| 7 | Steam, `D:\dvr-data` | **dvr-xrsim, FOCUSED, first frame submitted, GAMEPLAY, both eyes 38% non-black, soak PASS 3 min**; eyetest NOT WRITTEN (null camera) |
| 8 | Steam | head-locked quad 16% per eye but swinging with yaw (the sim's quad math); eyetest wrote, measured the field/c5 offsets; WM_CLOSE -> the stuck process |
| 9 | Steam (after the process cleared) | `mono.xrs` PASS; head-lock pair IDENTICAL bboxes on the fixed sim; no c5 (the c0 x128 block) |
| 10 | Steam | c5 back; eyetest: 0x330 reads -c5 and moves c5 by -98.7 uu (75/76), the rest discarded |
| 11 | Steam | sign-aware seam: **0x330 HONOURED 119/120 (+99.2 uu)**, five DISCARDED; `camera eyefield 0x330` |
| 12 | Steam + Quest 3 (VDXR) | flat: a stale `[VR] XrRuntimeJson` (the sim manifest) made the loader fail; fixed to warn and ignore (`21e1cb64`) |
| 14 | Steam + Quest 3 (VDXR) | quit crashed again: the PreExit handler never ran (it lived inside the motion-aim block, off under GamepadOnly) - hoisted (`cf506ba4`) |
| 15 | Steam + Quest 3 (VDXR) | **clean quit**: `shutdown: game PreExit`, session teardown, instance destroyed, proxy unloading, no exception |
| 13 | Steam + Quest 3 (VDXR) | **THE HEADSET RUN: VirtualDesktopXR, Meta Quest 3, FOCUSED, READY, the screen in both eyes following the head, the gamepad working (user's report)**; quitting through the menu crashed 2.3 s after PreExit (EIP DEDEDEDE in d3d11.dll on VD's thread, the session still open) - teardown moved to the PreExit handler |

### 2026-09-02 - session 4e: gamepad-only, and the three rendering symptoms

Headset run at 4032x2268 requested / `capture: 3840x2160` actual. The tester reported three
things and they turn out to be one geometry. Full derivation in ENGINE_NOTES, "The three
rendering symptoms, and the one geometry that ties them".

1. **"Super pixelated, but the pause menu is huge like it's at full resolution."** Both
   halves are the same fact: SBS gives the WORLD half the frame width per eye
   (`per eye 1920x2160`) while a MONO menu frame samples the whole 3840 across the same quad.
   The menu is drawn at exactly twice the world's horizontal sampling density. That is the
   cleanest confirmation of the SBS packing anyone has produced, and it is not a bug - but it
   means the frame must be at least `2 x eyeWidth = 4992` columns for a 1:1 world. At 3840 the
   world sits at 77% of the panel.
2. **The fisheye is `FovLever`.** It does not only size the quad, it WRITES the game camera's
   FOV (`fov_lever.cpp`), so `FovLever=130` makes the game render 130 deg horizontal - the log
   agrees (`MEASURED render FOV ... = 130.0 deg`). A 130 deg rectilinear frame shown across a
   94 deg frustum stretches the edges. The author already knew: `frame_hooks.cpp` disarms the
   lever on overshoot, commented *"rather than leave the user in a fisheye"*.
3. **The black bottom border cannot be tuned away at 16:9.** Filling a 99 deg vertical
   frustum needs `lever = 2*atan(tan(v/2)*aspect)`: 128.6 at 16:9, 114.6 at 4:3, 100.5 near
   square. So the fisheye and the border are the SAME setting pulled in opposite directions,
   and at 16:9 nothing satisfies both. The tester found that empirically. A taller frame is
   not a preference, it is the only way out - which is exactly what they asked for.

**Next single change: `3840x2880` (4:3) with `FovLever` ~115.** Same per-eye width as now, so
no sharpness regression, +33% pixels, and it should visibly ease the fisheye.

**Two corrections to 4c, both mine.** (a) "Must be a real display mode" was too strong -
3840x2160 WAS honoured. The real rule is narrower: **`PinBackbuffer=1` causes the crop**; a
size the game rejects merely falls back, harmlessly, as long as the pin is off. Both effects
were present at 2850x2750, which made them look like one. (b) There is no 2560x1440 cap -
that was read from the run before the pin was turned off. 4032x2268 is still not honoured
(empty `setres` replies), so **trust `capture:`, never the requested number**.

**Applied this session:**

- **`[Mode] GamepadOnly=1`, new and default ON** (`config.cpp`). Turns off SkelControl hand
  writes, hand mesh, motion aim, motion melee, motion crouch and controller Blink aim, and
  scales no hand or weapon model. Head tracking, positional tracking, the FOV lever and the
  virtual gamepad keep running - this is NOT the `XR_SAFE` bisector, which also stops the
  head. It logs loudly and `status.json` gains `gamepadOnly` so the zeroes below it read as
  BY DESIGN rather than as failures. The author's rule that motion crouch and hands "must
  never stop working" is respected: nothing is retired, it is one key, set `GamepadOnly=0`.
- **The tester's tuned values are now the repo defaults**, in both the generated ini text and
  the `IniFloat` fallbacks, so a fresh install comes up where the headset testing left off.
- **`[PosTrack] Scale` default 50 -> 98.** This closes 40.2b: the tester tuned world scale by
  feel and landed on 98, within 2% of the 100 derived from the movement constants, arrived at
  independently and without seeing the number. That is the cross-check 40.2b was waiting for.
- Hand trims and `HandSize` reset to neutral, per the tester's "no scaling or changing the
  default hand/weapon models".

Build clean, exports 9/9 undecorated, lint clean, RelWithDebInfo installed. The fork and
`dxvk_stereo.txt` are untouched.

### 2026-09-02 - session 4d: the lever is half of the resolution setting

**The 3840x2160 run was full-frame but letterboxed** - tester: "almost sort of right again,
only problem was that the resolution was rectangular so it didn't fill my view". Both halves
of that are now explained, and one of them was my error.

**My error.** Session 4's restore set `FovLever=100`, correct for the near-square 2850x2750
it was paired with. Session 4c then changed the render to 16:9 and left the lever at 100.
The frustum-fill branch takes its vertical extent as `tan(fovDeg/2)/aspect`, so at 16:9 with
lever 100 the quad clamps to **67.7 deg inside a 99 deg frustum** - letterboxed by
construction. At lever 130 the same 16:9 render fills edge to edge with ~9% black at the
bottom. Changing the render aspect without changing the lever is NOT a one-variable change:
the pair is the variable. Table in ENGINE_NOTES, "FovLever IS the vertical fill lever".

**The requested resolution is not being honoured at all.** The mod asked for 3840x2160; the
log says `capture: 2560x1440`. With `PinBackbuffer=0` that is harmless - buffer and content
agree, no crop, which is why 4c's fix worked - but **this rig cannot render above 2560x1440**
(desktop 5120x1440 caps it). Every "4032x2268" run here is really 2560x1440. Trust the
logged `capture:` number, never the requested one.

**F10 SAVE AS DEFAULTS was finally pressed** (the thing session 3c said had never happened).
The tester's tuning is now persisted and snapshotted to
`tests/golden/f10-tuned-2026-09-02.ini` and `<game>\...\dishonored_vr.ini.f10-saved-0247`:

| key | default | tuned |
|---|---|---|
| `[Tracking] HeightOffsetM` | 0.000 | 0.040 |
| `[Screen] FillScale` | 1.00 | **0.74** |
| `[Screen] DistanceMeters` | 1.60 | 1.67 |
| `[PosTrack] Scale` | 50.0 | **100.0** |
| `CrouchToggle` | 1 | 0 |
| `XrLayer` | (absent) | proj |
| `StampFix` | (absent) | 0 |

`[PosTrack] Scale=100.0` matches the measured 100 uu/m from commit 60235b86 - the tester
converged on the measured value by feel, which is a good cross-check on that measurement.

**Applied on request: GingasVR's resolution default**, as the coherent pair -
`RenderWidth/Height=4032x2268`, `SpoofDesktopW/H=4096x2304`, **`FovLever=130`** - plus
`DishonoredEngine.ini` and all four AppCompat buckets at 4032x2268. `PinBackbuffer` stays 0
(her ini has no such key) and `MenuFillScale` stays 1.00 (the 4b fix). Every F10 value above
is preserved. Backups `.pre-gingasres` / `.f10-saved-0247`. NOT YET TESTED.

**Expect this**: the render will probably clamp to 2560x1440 again (fine, still 16:9), and at
lever 130 the quad should fill horizontally and to the top with ~9% black at the bottom -
**but `FillScale=0.74` will still present it at 74% of that**. F2 raises FillScale live in
the headset; that single knob is the difference between 74% and full. Judge the lever first,
then the fill.

### 2026-09-02 - session 4c: THE RENDER WAS NEVER 2850x2750

**This is the root cause.** The tester sent a desktop-mirror screenshot of the main menu with
the picture in the top-left of the window. The mirror blits the backbuffer's LEFT HALF
pillarboxed (`frame_hooks.cpp:415-460`), so its horizontal placement is expected - but the
picture filled only the top ~52% of the window, and that is not.

**Measured, six capture dumps, non-black bounding box:**

| requested buffer | actual content | real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | FULL |
| 2560x1440 | 2560x1440 | yes | FULL |
| 3840x2160 | 3840x2160 | yes | FULL |
| 4032x2268 (GingasVR's) | 3024x1440 | no | CROPPED |
| 2750x2850 | 2750x2200 | no | CROPPED |
| **2850x2750 (our "known good")** | **2560x1440** | no | **CROPPED** |

At 2850x2750 the game draws exactly **2560x1440 into the top-left** and leaves the rest
black. So the whole session's geometry was applied to a frame that is half empty. It
explains all three symptoms at once: tiny (content covers ~90% x 52% of the quad), top-left
(it is literally there), and the eyes not fusing (the SBS halves meet at x=1280, not the
x=1425 the split assumes, so each eye gets part of the other's view plus black).

**Why nothing caught it.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 while the game
renders at the size it asked for (`CreateDevice the game asked for 2560x1440`). The mod then
spoofs `GetClientRect` to 2850x2750 and the setres path reads that spoof back, concluding
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. A
check reading our own spoof cannot fail its own hypothesis, so the engine-side resize never
ran and `capture: 2850x2750` was logged for a half-empty frame.

**`PinBackbuffer` is ours, not GingasVR's.** Her tuned ini (`.pre-2750`) has no such line.
Every ini since session 2 sets it to 1.

**This is probably the central open bug.** 4032x2268 is not a standard mode either and
cropped here too. Whether an injected mode is honoured depends on the machine's GPU, driver
and desktop mode - this rig's desktop is 5120x1440, and two of the three cropped captures
came back exactly 1440 tall. It predicts affected users have a desktop shorter than the
requested render height, and it is falsifiable by asking one for their desktop resolution.

**The Documents folder was ruled out**, on the tester's suggestion: `DishonoredEngine.ini` is
vanilla plus the intended VR lines, all four AppCompat buckets were already correct, and no
file in the Config directory contains 2560 or 1440.

**Applied for testing** (one coherent change: use a resolution the display actually offers):
`RenderWidth/Height 2850x2750 -> 3840x2160`, `SpoofDesktopW/H -> 3840x2160`,
`PinBackbuffer 1 -> 0`, plus `DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` to
3840x2160. Per-eye half 1920x2160 = aspect 0.889, the same per-eye aspect as GingasVR's
4032x2268. Backups: `.pre-realmode` next to each of the three files. NOT YET TESTED.
Fallback if 3840x2160 is not honoured: 2560x1440, identical per-eye aspect, and the game
asked for it itself.

**Also confirmed this session**: the 4b MenuFillScale fix works. The 02:35 run logged 28 quad
rebuilds, all at `fill=1.00` / 100.0 x 98.0 deg, none at 0.60. The size pumping is gone.

**Next instrument to build**: nothing compares the captured frame's real content extent to
the buffer size. A non-black bounding-box check on the capture, logged once per resolution
change, turns this class of bug into one line. The setres check must also stop reading the
mod's own `GetClientRect` spoof.

### 2026-09-02 - session 4b: the world size PUMPS, and MenuFillScale is why

A headset run on the restored known-good ini (02:23, VirtualDesktopXR + Quest 3) reported
"still rendering tiny and in the top left corner". Its log names the cause outright, so this
did not need a new instrument. Full derivation in ENGINE_NOTES, "MenuFillScale pumps the
world size during GAMEPLAY".

**The number.** The quad subtends **71.1 x 69.2 deg** inside a frustum of **94.0 x 99.0 deg**
- about half its solid angle. That is "tiny", fully explained, and nothing to do with
resolution, adapter, world scale or convergence. 40 of the run's 46 quad rebuilds were at
`fill=0.60`; the run ended there.

**The mechanism.** `MenuFillScale=0.60` and the `XrFrustumFill` gate are driven by the SAME
condition (`g_menuOpen || g_inMenu || g_sbsMonoNow`), and a change in it forces a rebuild.
The menu flag flaps during gameplay (the `Req_SaveSlotInfos` save-slot polls, already
documented at `present.cpp:613-616`), so the world size pumped 100 -> 71 -> 100 -> 71 -> 100
-> 71 deg across the six seconds before the crash, all after gameplay had started. The
`sbs:` line proves it is the MENU flag and not the splice counter: its last transition is
well before the pumping began.

**Why the Index never saw it.** OpenVR has one geometry path, so `MenuFillScale` only ever
dimmed a menu. `XrFrustumFill` (38.13) added a second path for the OpenXR port without making
the transition continuous, so on Quest the same flap swaps the whole quad construction
mid-gameplay. The tester's own read - Index/SteamVR was the tuned target, OpenXR/Quest a
later port - is exactly right here.

**Applied, config only, one variable, no rebuild**: `[Screen] MenuFillScale 0.60 -> 1.00`
(backup `.pre-menufill`). The menu branch now builds the same 100.0 x 98.0 deg quad as
gameplay, so a flap cannot change the world size. Cost: menu edges crop, which is what 32.4
added the key to avoid. NOT YET TESTED.

**A falsifiable prediction, and it contradicts session 3c.** Worked from the logged frustum,
the authored quad's vertical border must be SYMMETRIC, ~21% black top and ~21% bottom, with
the world in the middle 57.6%; horizontally the left eye gets 29.8% black on the temple side
and 5.6% on the nasal side (mirrored in the right eye - the rigid-screen design). Session 3c
recorded "top ~54%, bottom half black". The next `dump eyes` settles it: symmetric borders
retire that contradiction as a misread dump; a real black bottom half falsifies this model.

**The run also CRASHED** at 02:23:49, wild instruction pointer, minidump at
`%LOCALAPPDATA%\DishonoredVR\dumps\dvr_20260902_022349.dmp`. Untriaged, separate lane.

**Good news in the same log**: the fork's projection export resolved this time -
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 100.0 deg`.
The landscape fix (session 3b) worked; world scale is no longer an assumed constant.

### 2026-09-02 - session 4: reverted to the known-good point

No launches, no code changes. Session 3c left the rig one key away from its own
confirmed-good configuration and that key was an open, unevaluated experiment; this
restores the documented point so the next run starts from a known baseline.

**What was actually different.** Exactly one line: `FovLever=130` vs `100`. Everything
else already matched - `RenderWidth/Height=2850x2750`, `SpoofDesktopW/H=2816x2880`,
`PinBackbuffer=1`, `GameFOVDeg=100`, `FillScale=1.00`, `[PosTrack] Scale=50.0`, the game
ini at 2850x2750 on both `[SystemSettings]` and `[SystemSettingsEditor]`, and all four
`[AppCompatBucket1..4]` at 2850x2750. `setup-game-ini.ps1` did not need re-running.

**The snapshot checks out.** `tests\golden\known-good-2850x2750-lever100.ini`, the game
folder's `dishonored_vr.ini.KNOWN-GOOD` and `dishonored_vr.ini.pre-lever130` (the ini as it
actually ran when the tester reported "the eyes seem to overlap correctly and provide
depth") are all identical. The snapshot is a truthful record of the run, not a
reconstruction - worth stating, because it was written at 00:54 while the live file was
already at `FovLever=130`.

**Restored** by byte copy, verified with `cmp` against both snapshots. Prior state saved as
`dishonored_vr.ini.pre-restore-known-good`.

**`FovLever=130` is untried, not disproved - and it was the well-motivated direction.**
ENGINE_NOTES "FovLever and the render size are ONE setting" has it the other way round from
how session 3c's ordering reads: at lever **100** the clamp limit is 1.91 m against a
frustum reaching 2.20 m horizontally and 2.29 m vertically, so it fires on **all four
sides**, and `dump eyes` at lever 100 confirmed the world inset with a ~9-10% border on
every side. At lever **130** the limit is 3.43 m, outside the frustum edge, so nothing
clamps and the quad fills the eye. 130 is also GingasVR's own tuned value.

**So the restore knowingly reinstates the bordered configuration.** That is the right call -
lever 100 at 2850x2750 is the only point a tester has ever confirmed fuses with depth, and
an unevaluated experiment is not a baseline - but the border is a KNOWN artifact of this
baseline, not a new symptom, and "the render window is halfway up my vision" must be judged
against that. Lever 130 stays queued as the next one-variable change once the gameplay dump
is in hand.

**Caveat that still stands**: no F10 tuning has ever been persisted (SAVE AS DEFAULTS was
never pressed), so this ini is the only reproducible configuration that exists.

**Untouched deliberately**: the proxy, the fork, `dxvk_stereo.txt`, and the stale
`command.txt` in `%LOCALAPPDATA%\DishonoredVR\` (`command.cpp:153` discards and clears a
command file older than the process, so the next run's log should show that branch firing -
a free check of the new seam diagnostics).

**Next**: unchanged from session 3c. Launch, reach GAMEPLAY with the window focused, then
`tools\game-cmd.ps1 "dump eyes"` and read the two PNGs. The unexplained contradiction is
still the lead: the world occupied only the top ~54% of the eye render target while the
measured frustum (55 deg down against 44 deg up) says the quad should overflow vertically.
That dump was a MENU frame and needs confirming in gameplay before anything is changed.

### 2026-09-02 - session 3c: the seam went deaf, and the eye texture is half black

**START HERE.** The bug is not fixed and the last measurement is incomplete.

**The one thing to do first**: launch, get into GAMEPLAY (not a menu, window focused),
then `tools\game-cmd.ps1 "dump eyes"` and look at the two PNGs in
`%LOCALAPPDATA%\DishonoredVR\dumps\`. Everything below is waiting on that image.

**The live symptom** (tester, Quest 3 + VirtualDesktopXR): eyes will not fuse, and the
image sits high - "the render window is halfway up my vision so I only see the bottom
half of it". Earlier in the session, "everything looked tiny".

**The measurement that matters.** A `dump eyes` caught mid-session shows the world
occupying only the **top ~54% of the eye render target, bottom half pure black**, in both
eyes. That is our own D3D11 pass, before the compositor. It was a MENU frame (the dump
caught a paused game), so it needs confirming in gameplay - but the vertical placement is
the lead. The measured frustum is
`eye frustums: L[-1.376 0.839 -1.428 0.966] ex=-0.0316 | R[-0.839 1.376 -1.428 0.966]`,
slots `[left, right, down, up]`: **down-biased, 55 deg down against 44 deg up**. Worked by
hand at these settings the quad should span y -2.36..+1.62 against a frustum of
-2.29..+1.55, i.e. it should OVERFLOW the eye vertically, not sit in the top half. That
contradiction is unexplained and is the next thing to chase.

Same dump showed the two eyes holding **different content** (different horizontal extents,
different fragments of the same menu text). That is the mono/stereo UV race on menu frames
- the fork stops splicing, the frame goes mono, each eye still takes its own half. Probably
menu-only: in gameplay the splice count is 5000+ and the halves are a real stereo pair.

**The command seam went deaf and blocked the session.** `status.json` stale for 18 minutes
and `command.txt` unread, while the Present hook ran at 62 fps with 251 `[present]` lines.
Two `dump eyes` commands did nothing and left no trace. `poll()` had FIVE silent returns,
so the failure was indistinguishable from the command never being written. **Fixed and
installed**: each guard now names itself with the path, the size and GetLastError. If the
seam is still deaf next session the log will say which branch refuses.

**Theories killed this session (do not re-run them):**

- The `CopyResource` size mismatch (step 0a) - eye RTs and XR eye size matched exactly.
- The pace thread as the crash victim - the faulting thread is `(other)`, not `xr-pace`.
- The black left eye - a SIMULATOR defect, not the mod; `dump eyes` shows the left eye
  texture full. See VERIFICATION "Known simulator defects".
- Eye cant - `g_eyeRot` is declared identity and the XR path correctly leaves it alone.
- The clock/rate gate - `MaimNowMs` is fine (the 5 s `depth:` line printed 50 times against
  the 3 s heartbeat's 81). Note the log uses `GetTickCount` while the gates use QPC via
  `dvr::clock`, so an advancing log proves NOTHING about the gate clock.
- A game restart clearing the seam - it did not.

**THE KNOWN-GOOD STATE, AND HOW TO GET BACK TO IT.** This rig's best result so far -
tester: *"the eyes seem to overlap correctly and provide depth, there is no freeze"* - came
from `2850x2750` + `FovLever=100` + `Scale=50`, with the main scene splicing (5202). It is
worth more than GingasVR's own values, which come from a different machine and are the
subject of the project's central open bug ("works only on her PC").

Snapshotted byte-for-byte in two places, so it survives a wiped game folder:

- `tests\golden\known-good-2850x2750-lever100.ini` (in the repo)
- `<game>\Binaries\Win32\dishonored_vr.ini.KNOWN-GOOD`

Restore = copy either over `<game>\Binaries\Win32\dishonored_vr.ini`, then
`tools\setup-game-ini.ps1 -Resolution -Width 2850 -Height 2750` for the game ini and the
four AppCompat buckets. The keys, if you ever need to rebuild it by hand:
`[Screen] RenderWidth=2850 RenderHeight=2750 SpoofDesktopW=2816 SpoofDesktopH=2880`
`PinBackbuffer=1 GameFOVDeg=100 FovLever=100 FillScale=1.00`, `[PosTrack] Scale=50.0`.

**WARNING - the snapshot does NOT contain any F10 tuning, and neither did any run.** The
overlay's sliders are live-only until someone presses **"SAVE AS DEFAULTS"**
(`overlay.cpp`, top of the panel), which calls `OverlaySaveDefaults` and writes ~90 keys -
world scale, fill, screen distance, height offset, menu fill, wrist HUD, and the whole hand
/ graft / blink block. It was never pressed, so every F10 adjustment the tester made across
this session was lost at process exit; the only thing that persisted was the hand
calibration, which `skelcontrol.cpp` writes on its own. **Procedure from now on: tune in
F10, press SAVE AS DEFAULTS, then re-snapshot the ini.** Otherwise a good configuration
cannot be reproduced, which is exactly what happened here.

**Config as left, LIVE right now**: ~~`FovLever=130`~~ **RESTORED to the known-good
snapshot (session 4)**. The live `dishonored_vr.ini` is now byte-identical to both
`tests\golden\known-good-2850x2750-lever100.ini` and the game folder's
`dishonored_vr.ini.KNOWN-GOOD` (`cmp` clean against both), i.e. `FovLever=100`. The
`FovLever=130` experiment was applied but never evaluated - the seam went deaf before a
dump could be taken - so it was discarded rather than judged; it remains untried, not
disproved. The pre-restore state is saved as `dishonored_vr.ini.pre-restore-known-good`.
Game ini + all 4 AppCompat buckets verified still at 2850x2750 (`DishonoredEngine.ini`
lines 1081/1141, `DishonoredCompat.ini` buckets 1-4), so no `setup-game-ini.ps1` re-run
was needed.
Backups in the game folder: `.pre-2750` (GingasVR's own tuned ini), `.pre-landscape`,
`.pre-scale100`, `.pre-gingas-restore`, `.pre-rollback`, `.pre-lever130`,
`.pre-restore-known-good`.

**Installed**: RelWithDebInfo proxy only, 00:50, carrying the seam diagnostics. The fork
(20:24) and `dxvk_stereo.txt` (13:52) are untouched and must stay that way - one variable.
Re-verified session 4: the installed `d3d9.dll` is md5-identical to
`build\src\RelWithDebInfo\d3d9.dll`, and the tree is clean at `112105b7`, so source,
build and install all agree. The fork and `dxvk_stereo.txt` timestamps are unchanged.

**Do not repeat these mistakes.** Restoring GingasVR's baseline I changed render size, FOV
lever and world scale in ONE step, so "misaligned" was unattributable; her values also come
from a different machine, and the project's central open bug is that her build works only
on her PC. This rig now has its own confirmed-good point (2850x2750, splices 5202, tester:
"eyes overlap correctly and provide depth") which is worth more than her numbers. Change
one thing per run, and get the gameplay dump before changing anything at all.

### 2026-09-01 - session 3b: the render was PORTRAIT, so there was no stereo

A headset run mid-session reported "the eyes are suuuuper far off and they both appear to
be zoomed in", worse than before. Its log is the **first surviving headset log** and is
archived outside the game folder. Cause found, fix applied, not yet tested.

**Session 2's resolution fix set the render to 2750x2850, which is portrait, and the DXVK
fork refuses to splice the main scene on a portrait viewport.** `d3d9_device.cpp:4381`
sets the refusal reason `"rt-portrait"`; the per-eye splice at `:4578` runs only when that
reason is `SPLICE`. So the world was drawn **mono** across the full frame while the proxy
handed each eye a different **half** of it - unrelated views that cannot fuse, each
magnified 2x by the stretch onto the quad. Exactly the report.

**The splice counter lies about it.** Light shafts, shadows and the M8.1 quarter light pass
splice under different conditions and kept working, so `splices=85` while the main scene
never spliced once - which kept `g_sbsMonoNow` false and the half-frame UVs on. The fork's
own log shows only effects splices.

**The same gate kills the FOV measurement.** `dxvk_vr_proj`'s publish (`:5996`) is also
gated on `Width > Height`, so `g_liveFovX` was 0 all session, the frustum-fill path fell
back silently to the ini constant `GameFOVDeg=100`, and an assumed number set world scale.

**This falsifies session 2's "the eyes ARE a stereo pair".** 32.7 mean-abs-diff static /
11.5 after a head turn is exactly what two different halves of one mono frame produce. That
test could not distinguish a stereo pair from two unrelated crops, so it could never have
failed its own hypothesis.

**Applied**: `2850x2750` - the same two numbers swapped. Same pixel cost, landscape by
100 px, full-frame aspect 1.036 so the quad subtends 100 x 98 deg at `FovLever=100`.
Changed in `tools/setup-game-ini.ps1` (defaults + a header section on why landscape is
mandatory), applied to `DishonoredEngine.ini` and `DishonoredCompat.ini` via the tool (both
backed up), and to the game folder's `dishonored_vr.ini` (backup `.pre-landscape`).

**Instruments added so this cannot hide again**: a portrait capture logs an Error naming
the fork's own refusal string and the fix (`present.cpp`); the frustum-fill path now says
every 10 s whether world scale comes from the MEASURED render FOV or from the assumed ini
constant (`eye_quads.cpp`).

**Installed**: RelWithDebInfo proxy only (`install.ps1 -Release -SkipDxvk`) - the fork and
`dxvk_stereo.txt` are untouched, so the resolution is the only render-path variable. The
proxy's other changes (session 3 below) are the shutdown/pace lane and logging, which
cannot confound the zoom result.

**Not yet tested.** If the fix worked the portrait Error is absent and the eyes fuse; if
the Error appears, the resolution did not take and AppCompat is overwriting it again.

### 2026-09-01 - session 3: the crash fingerprint was misread, and why

No launches: everything here comes from artifacts already on disk plus the source. The
game is installed on this PC, but nothing was run.

**Two of session 2's conclusions are instrument bugs, not engine facts.**

1. **The exit crash is an EXECUTE fault, not a freed-memory write.**
   `ExceptionInformation[0]` is three-valued (0 read, 1 write, 8 execute/DEP) and the
   fingerprinter tested it for truth, so every execute fault has printed as "writing". The
   records prove it: `ExceptionAddress == ExceptionInformation[1] == 0xDEDEDEDE` with the
   module resolving to `?`. A data write would have left `ExceptionAddress` inside
   `d3d11.dll`. **EIP landed in freed memory: a call through a poisoned code pointer.**
2. **The faulting thread is not the pace thread.** All three records say `(other)`, which
   `thread_name()` returns only for a tid in no registered slot; `present` and `xr-pace`
   both register at entry. The faulter is a third-party worker (d3d11, driver, runtime).
   The pace thread can be the cause, but instrumenting it as the victim will find nothing.

**Two evidence channels were dead and are now fixed.**

- **`dumps\` was empty by construction.** 3 `EXCEPTION` lines, 0 `minidump` lines: proof
  that `unhandled()` never ran, because UE3's own filter/SEH frame consumes the fault
  before `SetUnhandledExceptionFilter` fires. The dump is now taken from the **vectored**
  handler (which always runs), gated on the instruction pointer resolving to no loaded
  module - fatal-only by construction, and falsifiable: an ordinary in-module fault
  produces no dump and disproves the wild-EIP reading. `dbghelp.dll` is resolved at
  `install()` time so the VEH never touches the loader lock.
- **The crash file had no run identity.** `FILE_APPEND_DATA` / `OPEN_ALWAYS` with nothing
  separating runs, and `dvr-xrsim` and VDXR produce byte-identical fingerprint text, so
  the three records cannot be attributed to a backend at all. Now one header per run
  (clock, version, build id, pid, backend + runtime name).
- **Log rotation is one deep**, so two simulator runs erased both headset logs; the
  survivors contain no `EXCEPTION` and no `PreExit`. Copy the log out before each launch.

**The author read this fault correctly and session 2 inverted it.** The 38.79 comments say
"EIP dededede" and "a call through freed memory". 38.79 acted on that by standing the
**game** thread down at `PreExit`, which was right but not the whole path - it left the
pace thread running with nobody waiting for it. Closed below.

**Two pace-lane defects fixed** (steps 0b and 0c):

- **`XR_TIMEOUT_EXPIRED` is a success code.** `XrResult` is negative for failure only, so
  `XR_FAILED()` is false for it and `!XR_FAILED(xrWaitSwapchainImage(...))` ran
  `CopyResource` into an image the compositor had not finished reading - a race with the
  runtime on the one resource the headset displays, invisible because every call returns
  success. Now `== XR_SUCCESS`. In the same block `g_xrpShown` advanced **before** the
  copies, so a frame lost to a timeout was dropped permanently instead of retried; it now
  advances only once both eyes actually received the content.
- **`XrPaceStop()` joins the pace thread**, bounded at 750 ms, replacing the bare
  `g_xrRun = 0`. On expiry the thread is left running on purpose - `TerminateThread` would
  orphan `g_xrCs` and abandon an acquired swapchain image, which is worse than the race -
  and the error line is the instrument: a fault after it means the pace lane is still the
  suspect, a fault without it means the thread was already gone and it is not. The event
  pump's inner `while` now tests `g_xrRun` so an event backlog cannot hold the loop past a
  stop request.

**Changed** (8 files, uncommitted): `src/core/util/crash.cpp` and `crash.h` (three-valued
AV decode; run header; `set_context`; shared `write_dump` with the wild-EIP gate),
`src/core/vr/openxr_backend.cpp` (names the runtime in the crash context),
`src/core/vr/openxr_pace.cpp` (the wait fix, the retry fix, `XrPaceStop`),
`src/game/dishonored/ue3/process_event.cpp` (`PreExit` joins), `src/mod/fwd.h`,
`docs/dishonored/ENGINE_NOTES.md`, `docs/STATUS.md`. Verified: Debug, RelWithDebInfo and
`-Legacy` all build, and both DLLs carry the new strings; `lint.ps1` clean; exports 9/9
undecorated. **Not run in the game, in the simulator, or in a headset.**

**Next**: step 0a - the `CopyResource` size mismatch, which is still only a code-reading
hypothesis. Then the full `xrRequestExitSession` / `xrDestroySession` shutdown.

### 2026-09-02 - session 2: instrumentation, resolution, and a real headset

**First session with the game actually installed and a real Quest + Virtual Desktop
headset on the other end.** Environment: proxy and fork both built from source with
MSVC (meson + ninja + glslang 16.5.0 standalone, no Vulkan SDK); `tools\build-dxvk.ps1`
needed two fixes to run at all on a PC with VS 2026 installed next to VS 2022.

**Fixed and verified**

- **Resolution.** `ResX` in `DishonoredEngine.ini` never held: UE3 AppCompat picks an
  `[AppCompatBucketN]` at startup and writes that bucket's ResX/ResY over
  `[SystemSettings]`. Buckets 3 and 4 ship 1600x900, and any GPU newer than the 2012
  table lands in one, so every modern machine started at 1600x900 forever. Fix: set all
  four buckets; `tools\setup-game-ini.ps1 -Resolution` now does this and defaults to
  2750x2850. Measured before/after: `CreateDevice (1600x900)` -> `CreateDevice
  (2750x2850)`, `capture: 2750x2850` on the FIRST device creation, no setres needed.
- **`setres` is a dead end.** Measured `setres 2750x2850w -> "(empty reply)"` with NO
  device Reset following. New `[Screen] PinBackbuffer=1` (default OFF) sets the size in
  the present parameters at CreateDevice instead. The 32.57 "image in the corner"
  objection is answered by the GetClientRect hook that landed later.
- **World scale.** `W` is pinned to the rendered FOV and `H = W / frameAspect`, so a
  squarer render makes a taller virtual screen than the lenses can show and the player
  sees a magnified middle. 2750x2850 at FovLever=130 subtends 100x132 deg; at 100 it is
  100x102, which matches the headset. FovLever set to 100.
- **The mono/stereo UV race.** `BuildEyeQuads` BAKES the sampling UVs, but the rebuild
  only fired on an aspect change or a menu toggle. `g_sbsMonoNow` flips during gameplay
  whenever the fork's splice count dips, so a mono frame could be sampled with stereo
  UVs and each eye got a different half of one mono image. Now a change in frame kind
  forces a rebuild, exactly like the menu flag.

**Instrumentation added** (see the new "Logging" section in `CLAUDE.md`)

- Full DXGI adapter enumeration, the LUID the runtime asks for, and the adapter read
  back OUT of the finished device with an Error-level mismatch line.
- `RESOLUTION CHANGED MID-SESSION` at Warn, `quad: ... subtends AxB deg` per rebuild
  with a Warn past 110 deg vertical, `res: the game asked for WxH`, and a `skc/gate:`
  line for the hand drive.
- The hands heartbeat now names the OWNER and reports that owner's counter.

**Corrected beliefs** (all three were believed and are wrong)

1. "The hand graft never attaches." It attaches fine: `OWNER=SkelControl writes=~406/3s`
   in gameplay. The old heartbeat tracked two counters that read 0 BY DESIGN - one a
   retired subsystem, one the legacy drive that is deliberately stood down. Three
   readers including the original author concluded "the hands are dead" from a healthy
   run.
2. "The 39.3 adapter bug needs two GPUs." DXGI enumerates the SAME RTX 4070 Ti SUPER
   twice on this PC (virtual display drivers), so the default adapter is not stable on
   single-GPU machines either. **But on the real VDXR run the runtime asked for
   adapter[0], which IS the default, so the LUID mismatch is NOT the cause of the
   symptoms on this rig.** 40.1 still fixes the class of bug and makes it visible.
3. "The eyes are not a stereo pair." Measured 32.7 mean-abs-diff when static, but 11.5
   after a head turn, which is normal parallax. Stereo works; the divergence is a
   symptom of the freeze, not the disease.

**Still broken: the freeze-then-rescale.** Reported again after all of the above. What
is ruled out: the resolution (it now stays 2750x2850), the adapter (matched), the FOV
(100), the mono/stereo UV race (fixed), and the hand drive (working). What is NOT ruled
out and is where to look next:

- The **shutdown crash is a teardown race** and may share a root with the freeze: three
  runs ended with `EXCEPTION 0xc0000005 writing 0xDEDEDEDE` inside `d3d11.dll`, two
  threads at once, immediately after `PreExit` stops the pace thread. 0xDEDEDEDE is
  freed-memory poison. The detached pace thread is touching released D3D11 objects.
- The pace thread owns every runtime call while the game thread owns capture and
  UpdateSubresource on the SAME `g_ctx11`; ID3D11DeviceContext is NOT thread-safe.
  `ID3D10Multithread` is enabled but that protects the device, not a stale pointer.
- Instrument the frame path next: log around the Reset/teardown boundary and around
  every `g_ctx11` use from the pace lane, and get a minidump analysed from
  `%LOCALAPPDATA%\DishonoredVR\dumps`.

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.
