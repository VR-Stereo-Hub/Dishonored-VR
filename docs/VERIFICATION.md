# Verification catalog - what can be checked, and by which tool

Three tiers. **Numeric**: a number from a file or a log, asserted by a script. **Flat visual**:
a screenshot or a compositor capture, compared by `img-diff`. **Headset**: a human. Push every
question as far up this list as it will go; asking the user to put the headset on for a
question the simulator could answer is a wasted session.

## 1. The decision table

| Intent | Tool | Command | Read it as |
|---|---|---|---|
| Does the tree build? | CMake | `tools\build.ps1` (and `-Legacy`) | exit 0 both ways |
| Are the exports right? | dumpbin | `tools\exports-check.ps1 build\src\Debug\d3d9.dll` | "exports OK: 9 names, all undecorated" |
| Did the split change a body? | parser | `python tools\split-source.py --check` | "changed 16" = the Phase 0 MSVC edits only (9 exports, 6 `_ReturnAddress` sites, the `.mtl` fix); anything else is a regression |
| Is the default ini unchanged? | golden | `python tools\ini-golden.py --check` | MATCH (the golden is generated from the working tree's WriteDefaultIni) |
| Style rules | lint | `tools\lint.ps1` | "lint: clean" |
| Is the SIMULATOR healthy? | xr_hello32 | `tools\xrsim-selftest.ps1` | "SELFTEST PASS: dvr-xrsim ran 60 frames"; a failure here is the sim, not the mod |
| Did the mod load and which runtime answered? | log | `tools\log-parse.ps1`, `tail-log.ps1 -Grep "xr:"` | `proxy loaded`, the runtime layer's instance line, `xr: runtime "<name>" - session live` |
| Which stereo method runs? | log / status | `game-cmd.ps1 "stereo status"`, `status.json` -> `stereo{}` | `stereo: method=mono framesOut=N`, the `stereo: beat` line every 3 s |
| Is the capture the whole game window? | log | `tail-log.ps1 -Grep "capture:"` | `capture: WxH content bbox [..]-[..] = 100% x 100% (FULL)`; CROPPED names the corner-image class |
| Which hooks installed? | log / status | `status-dump.ps1` -> `hooks{}` | `processEvent`, `blinkDir/Dst/Trc`, `pad` true |
| Is the game in gameplay? | log | `[game] state: GAMEPLAY` | the line `boot.ps1` waits for |
| Is the session live on the sim? | log + state.json | `xrsim-launch.ps1` | `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, `frame` advancing |
| Is the mono screen in BOTH eyes? | capture | `xrsim-run.ps1 -Path tools\xrsim\mono.xrs` | `quadLayers >= 1`, `capNonBlackL/R >= 10` (the default head-locked quad is ~16% of a Quest 3 eye; its `src` reads ~97%), `stats.bboxL == bboxR` within the ~12 px eye parallax; a black eye is attributed in `xrsim.log` (COMPOSITOR vs APP fault) |
| Are two eyes submitted? (stereo methods, S2) | capture | `xrsim-shot.ps1` -> `ProjViews`, `EyeSeparationM` | 2, ~0.063 |
| Which camera field does the renderer honour? | log | `game-cmd.ps1 "camera eyetest 100"` in gameplay, standing still | `camera/eyetest: <field> ... HONOURED|DISCARDED|INCONCLUSIVE`, then `DONE` with the field for `[Camera] EyeField` (ENGINE_NOTES, the per-eye camera seam) |
| Are the two eyes paired? (S2) | log | `tools\eye-check.ps1` leg 0 | `stereo: beat ... L/s=N R/s=N`, both flowing and within 80% |
| Does head rotation move the camera? | capture | `headlook.xrs` | `img-diff` of left eye at yaw 0 vs 35 rises well above the ~0.4 noise floor |
| Stereo depth present? | capture | `stereo.xrs` | left vs right `img-diff` >> 0.4 |
| World rigidity under 6DoF | capture | `world-6dof.xrs` | `ClaimRatioH` ~1.0 at every yaw/pitch, `EyeSeparationM` constant |
| Is the weapon glued to the controller? | capture | `coupling-hand.xrs` | hand-quad bbox moves with the hand, not the head |
| Frame pacing collapse | capture | `unfocused-pacing.xrs` | `@fps 60` across FOCUSED -> VISIBLE -> FOCUSED |
| Wedge / hang | soak | `tools\soak.ps1 -Minutes 10` | exit 0; 2 = log stalled, 4 = died |
| What was the headset fed? | dump | `game-cmd.ps1 "dump frame"` | `dumps\capture_*.bmp` (the game window), `eye_*_mono|left|right.png` (the method's output) |
| Comfort, judder, world scale, warp | headset | F10 overlay + the user | the verdict; write it in STATUS |

## 2. The simulated runtime (`dvr_xrsim32.dll`)

A purpose-built 32-bit OpenXR runtime (`src/tools/xrsim`, from the BioShock trilogy mod) that
presents as a Meta Quest 3 (2064x2208 per eye, measured VDXR FOVs, IPD 63 mm). It links
nothing from the mod and ships in no release. Selected per process with `XR_RUNTIME_JSON`
(`tools\xrsim-install.ps1` writes the manifest; the registry is never touched). The static
OpenXR loader inside `d3d9.dll` reads that variable first; for a launch through Steam, which
cannot carry it, `xrsim-launch.ps1 -ViaSteam` puts the manifest in `[VR] XrRuntimeJson` and
the mod sets the variable for its own process.

What the agent drives (`tools\xrsim-cmd.ps1`, one atomic batch per write, acknowledged by
`cmdSeq`): `head pos|rot|pose|move|movelocal|turn|to|orbit|height|valid`, `recenter`,
`hand <l|r> grip|aim pose|pos|rot|point|follow head|offset|aimtrim|valid`, `hands reset`,
`btn a|b|x|y|menu down|up|press [ms|Nf]`, `click`, `thumbrest`, `trigger`, `grip`, `stick`,
`input clear`, `pace free|step|turbo`, `step [n]`, `refresh <hz>`, `idle`, `state <name>`,
`focus lose|regain|policy|norender|throttle`, `hazard nosystem|waitfail|beginfail|endfail|
swapchainfail|attachfail|clear`, `instanceloss`, `ipd`, `fov`, `worldscale`, `shot [name]`,
`capture next|every|off|size`, `compose always|oncapture`, `reset`, `status`, `log <text>`.

What it writes: `state.json` (frame counters, session state, pace mode, layers last frame,
head/hand poses, controls, errors) and per shot `capture\<name>_left.png`, `_right.png`,
`_sbs.png`, `<name>.json` (layers with type/space/size/pose, `derived.eyeSeparationM`,
`derived.claimRatioH`, `derived.aimRayMaxDevDeg`, `stats.meanLuma`, `stats.nonBlackPct`,
`stats.bboxL/R` (41.0), and per layer `src[]` (the source image's `nonBlackPct`, `bbox`,
`image`, `releasedOnFrame`)). `state.json` also carries `quadLayers`, `capNonBlackL/R`.

Hard invariants: no wait in the sim is unbounded (30 s starve grant); the control channel
polls on its own thread (a frame-path poller could never receive the `step` that unblocks it);
`command.txt` older than the sim's start is discarded.

Not modelled: lens distortion, timewarp/ASW, real display cadence, Wi-Fi encode, guardian. A
pacing bug that reproduces in the sim is real; one that does not may still exist on VDXR.

### KNOWN SIMULATOR DEFECTS - and the instruments that now name the side (2026-09-02, 41.0)

Found in session 4 while chasing a reported "eyes misaligned" bug; both were caught only
because the tester contradicted the capture. 41.0 adds the instruments that make the next
occurrence self-diagnosing; neither defect has been re-run since (the game was not installed
on the dev PC when 41.0 was built), so the status of each is "instrumented, unverified".

1. **The left eye composited BLACK while the application's left eye was correct.** Measured
   then: `stats.meanLumaL = 0.00`, `nonBlackPctL = 0.00`, `_left.png` a uniform image, while
   `nonBlackPctR` read 71.1 on the same frame and the mod's own `dump eyes` held a full left
   image. The compositor's projection shader answers a NaN or non-unit pose, or a fov whose
   edges cross, with a silent black eye; the likeliest cause was the old pace loop tagging the
   left view with a pose from an uninitialised ring slot. That loop is gone (the runtime layer
   tags every view from `xrLocateViews`), and the simulator now says which side is at fault:
   - every capture reads back each layer's SOURCE image and writes `layers[i].src[]` into the
     JSON (`nonBlackPct`, `bbox`, `image`, `releasedOnFrame`);
   - a composited eye that is black while its source is not logs
     `xrsim: eye L composited BLACK from a source that is N% non-black (layer i ..., pose norm,
     fov) - COMPOSITOR fault`; a black source logs `... - APP fault`;
   - `xrEndFrame` validates every projection view's pose (unit quaternion) and fov (ordered
     edges, under 178 deg) and logs `xrsim: xrEndFrame layer i view v carries a BAD POSE|FOV
     (...)` with the value;
   - `stats.bboxL/R` (the composited eye's non-black box) in the JSON, and `quadLayers`,
     `capNonBlackL`, `capNonBlackR` in `state.json`, so `tools\xrsim\mono.xrs` asserts on
     both eyes without a human reading a PNG.
   The gate for trusting per-eye captures again: `mono.xrs` passes on the build (both eyes at
   least 10% non-black - the default head-locked quad is ~16% of the eye - with `bboxL ==
   bboxR` within the eye parallax) and no COMPOSITOR-fault line in `xrsim.log`. **Passed
   2026-09-02 on 41.0** (session 5, run 9, the fixed simulator): L 12.9% / R 12.9%, identical
   bboxes at yaw 0 and yaw 30, source 97% in both views, no fault line (16.4 / 16.3 and
   37.95 / 37.98 on the runs before the simulator's quad math was fixed).

2. **`dump eyes` wrote the eye textures blue-tinted.** The writer picks the WIC pixel format
   from the texture's DXGI format (R8G8B8A8 -> RGBA, B8G8R8A8 -> BGRA); the stereo output
   texture is R8G8B8A8 (`eye_<frame>_mono.png` on the mono screen). Unverified since the change:
   compare the colours of `dump capture` and `dump eyes` on the first run, and read colour from
   `dump capture` until they agree.

Two confounds worth knowing when driving the sim, neither a defect:

- **An unfocused game window stops rendering the world.** A capture taken while the game is
  in the background shows HUD elements on black and no geometry. Foreground the window
  before capturing (`nonBlackPctR` went 20.1 -> 71.1 on nothing but a focus change).
- **`xrsim-launch.ps1` throws "the simulator is loaded but produced no frames in 1 s"** while
  the game is in fact alive and running fine. The frame-liveness check fires before the game
  has reached a rendering state. Check for the process and read the mod log before believing
  the launcher; the handoff's trap 6 (a direct exe launch crashes at the menu) did NOT
  reproduce here - the game reached a level and stayed there.

## 3. The end-to-end agent workflow

```powershell
.\tools\xrsim-selftest.ps1                       # 0. the SIM is healthy
.\tools\build.ps1; .\tools\install.ps1           # 1. build + install (needs the game)
$g = .\tools\xrsim-launch.ps1                    # 2. launch on the sim; throws unless runtime == dvr-xrsim (-ViaSteam if a direct launch dies)
.\tools\boot.ps1 -Attach                         # 3. reach gameplay (-Attach is mandatory in sim mode)
.\tools\game-cmd.ps1 "status"                    # 4. the seam works; read status.json
.\tools\xrsim-cmd.ps1 "reset" "head rot 0 0 0"   # 5. drive the rig and capture
$a = .\tools\xrsim-shot.ps1 -Out "$env:TEMP\dvr\head_0"
.\tools\xrsim-cmd.ps1 "head rot 35 0 0"
$b = .\tools\xrsim-shot.ps1 -Out "$env:TEMP\dvr\head_35"
$a.ProjViews -eq 2; $a.EyeSeparationM             # 6. assert with numbers
.\tools\img-diff.ps1 -A $a.Left -B $a.Right      #    stereo: >> 0.4
.\tools\img-diff.ps1 -A $a.Left -B $b.Left       #    head look moved the camera
```

Or scripted: `.\tools\xrsim-run.ps1 -Path .\tools\xrsim\headlook.xrs`. The `@mod` directive
routes lines to the mod's seam through `game-cmd.ps1`.

## 4. Numeric thresholds

Standing-still noise ~0.4 mean-abs-diff; a real FOV change 4-7; `nonBlackPct > 50` in
gameplay; frames/s near `refreshHz` (a collapse to ~10/s is a pacing bug); `eyeSeparationM ==`
IPD; `errors` and `endsOutOfOrder` must be 0.

**Per-eye `...L` statistics are trusted only after `mono.xrs` passes on the build** (section 2,
defect 1). A black eye is then attributed by the `COMPOSITOR fault` / `APP fault` line in
`xrsim.log`; the mod's `dump eyes` and `dump capture` remain the independent second opinion.

## 5. Failure modes and gotchas

1. An elevated shell: the Khronos loader ignores `XR_RUNTIME_JSON` there. `xrsim-launch.ps1`
   refuses.
2. A 64-bit `dvr_xrsim32.dll` is silently skipped by a 32-bit process and the real runtime is
   used; `xrsim-install.ps1` checks the PE machine.
3. `XR_RUNTIME_JSON` is per process: the sim launcher starts `Dishonored.exe` directly, so
   `boot.ps1 -Attach` is mandatory or Steam starts a second game. Where a direct launch dies
   at the menu (the author's trap 6; it did NOT reproduce in session 4), `xrsim-launch.ps1
   -ViaSteam` puts the manifest in `[VR] XrRuntimeJson` and launches through Steam; the ini
   is restored once the runtime-name assertion has passed.
4. `command.txt` written with a BOM corrupts the first token; the scripts use `WriteAllText`.
5. The seam polls from Present: if the game pauses its render loop unfocused
   (`[Screen] KeepAliveUnfocused=0`), commands wait until the window is foregrounded.
6. A `command.txt` older than the process is discarded at the first poll (the log says so).
7. The hand models, the wrist HUD and the aim reticle are not drawn on the mono screen
   (41.0: the hands are compiled but uncalled, the HUD is not captured); captures are
   expected to lack them until S3.
8. `xrsim-launch.ps1` can throw "the simulator is loaded but produced no frames in 1 s"
   while the game is alive and running normally - the frame-liveness check fires before the
   game reaches a rendering state. Check for the process and read the mod log before
   believing it. (Measured 2026-09-02; the game had reached a level and stayed there.)
9. Foreground the game window before any capture. Unfocused, the world stops rendering and
   the shot shows HUD on black - `nonBlackPctR` went 20.1 -> 71.1 on nothing but a focus
   change. See also gotcha 5.
10. Judge stereo, eye parity or per-eye coverage from a simulator capture only after
    `mono.xrs` has passed on the build (section 2, defect 1); a black eye is then attributed
    by the `COMPOSITOR fault` / `APP fault` line.
11. Menus, videos and loading screens are on the mono screen like everything else in 41.0;
    a stereo method (S2) decides what it does with them, and a stereo assertion on a menu
    is a false alarm until that decision is written down.
12. `game-shot.ps1` uses `PrintWindow` on the game's own D3D9 window; whether it captures
    non-black is unverified until the first attended run on 41.0.
13. The game was not installed on the dev PC when 41.0 was built: the rows of the table
    that need the game are written from the code and the BioShock harness's shape and need
    their first attended run (STATUS records what has run).
14. **The agent's shell on the dev PC virtualizes writes under the user profile.** Files the
    harness wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) were
    visible to the shell and to a game it launched directly, and absent for a game launched
    through Steam (its listing of the directory held only the entries game runs had written;
    a WMI-created `dir` agreed). Measured 2026-09-02 on the first 41.0 runs. Point both sides
    at a real location: `DVR_DATA_DIR=D:\dvr-data` for the scripts and `[Paths]
    DataDir=D:\dvr-data` in `dishonored_vr.ini` for the mod; the simulator follows the
    manifest's directory when `DVR_XRSIM_DIR` cannot reach it.


Comfort, judder, warp, world scale, the mono screen's size and distance, fusion once a
stereo method runs, hand placement feel, and anything about Virtual Desktop's own
reprojection. Write the verdict in STATUS with the build id from the log's first line.
