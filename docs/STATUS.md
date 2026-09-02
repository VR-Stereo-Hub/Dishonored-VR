# Status

## Current state (2026-09-02, session 5: the native-stereo foundation, 41.0)

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
  per-candidate c5 baseline (commit `cf9ec6f2`). **The redesigned eyetest has not run yet.**

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
   block was transposed. Fixed (`d43eea11`), selftest PASS; **the head-lock re-measure on the
   fixed simulator has not run yet.** BioShock's eye legs never saw it (projection layers rotate
   rays in the shader).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Not verified**: the headset (Quest 3 via VDXR) - the user's run; the SteamVR shim with
Dishonored; the eyetest verdicts; `mono.xrs` end to end on the fixed simulator (run 8's
`capNonBlackL` read 16 against the then-25 threshold; the threshold is now 10 with the measured
numbers, and the run is pending); `head_track`/`pad_bridge` as real modules (deferred, S1).

## Next steps (one paragraph per developer)

**Both, first, after a reboot of the dev PC** (the stuck `Dishonored.exe` pid 13452 blocks Steam
launches and holds the build's `dvr_xrsim32.dll`): `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini already
carries `[Paths] DataDir=D:\dvr-data`); `tools\xrsim-run.ps1 -Path tools\xrsim\mono.xrs
-Dir D:\dvr-data\xrsim` (expect PASS, ~16% per eye, equal bboxes); the yaw pair
(`xrsim-cmd "head rot 30 0 0"` + `xrsim-shot`): the composite bbox must now be IDENTICAL at
yaw 0 and 30 (head-locked) while `dump capture` content changes (the game camera turned); then
`game-cmd "camera eyetest 100"` standing still in gameplay and copy the six verdicts and the
`<field> reads (...) c5 (...)` lines into ENGINE_NOTES "The per-eye camera seam". Copy
`dishonored_vr.log` out before every relaunch.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The method needs an honoured
eye field from the eyetest; if every candidate reads DISCARDED, the c0 view-projection
translation (`LeanVP`) is the fallback write for the eye offset (it already moves the render
for lean) and AER can drive it per tick from `camera::eye_offset_uu()`. Acceptance: `stereo aer`
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

## Blockers

- **A stuck `Dishonored.exe` (pid 13452) on the dev PC** since the WM_CLOSE quit test: one
  thread, unkillable, holds `d3d9.dll` and `build\src\Debug\dvr_xrsim32.dll` (both renamed
  aside), and Steam refuses to launch the game while it exists. A reboot clears it.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The eyetest verdicts and the head-lock re-measure are pending the reboot (above).
- The headset run needs the user.

## Session log

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
