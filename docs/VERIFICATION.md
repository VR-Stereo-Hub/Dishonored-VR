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
| Does the fork export what the proxy reads? | dumpbin | `tools\dxvk-exports-check.ps1 build\dxvk\dxvk_d3d9.dll` | "all 15 required names present"; `dxvk_vr_view` reported optional |
| Is the default ini unchanged? | golden | `python tools\ini-golden.py --check <generated.ini>` | MATCH (after the config-table step, "old + new keys only") |
| Style rules | lint | `tools\lint.ps1` | "lint: clean" |
| Is the SIMULATOR healthy? | xr_hello32 | `tools\xrsim-selftest.ps1` | "SELFTEST PASS: dvr-xrsim ran 60 frames"; a failure here is the sim, not the mod |
| Did the mod load and pick a backend? | log | `tools\log-parse.ps1` | `proxy loaded`, `config: VR backend ...`, `probe:` lines |
| Is the fork loaded? | log | `tail-log.ps1 -Grep "backend:|sbs: fork"` | `backend: DXVK loaded`, `sbs: fork splice counter resolved` |
| Which hooks installed? | log / status | `status-dump.ps1` -> `hooks{}` | `processEvent`, `blinkDir/Dst/Trc`, `pad` true |
| Is the game in gameplay? | log | `[game] state: GAMEPLAY` | the line `boot.ps1` waits for |
| Is the session live on the sim? | log + state.json | `xrsim-launch.ps1` | `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, `frame` advancing |
| Are two eyes submitted? | capture | `xrsim-shot.ps1` -> `ProjViews`, `EyeSeparationM` | 2, ~0.063 |
| Does head rotation move the camera? | capture | `headlook.xrs` | `img-diff` of left eye at yaw 0 vs 35 rises well above the ~0.4 noise floor |
| Stereo depth present? | capture | `stereo.xrs` | left vs right `img-diff` >> 0.4 |
| World rigidity under 6DoF | capture | `world-6dof.xrs` | `ClaimRatioH` ~1.0 at every yaw/pitch, `EyeSeparationM` constant |
| Is the weapon glued to the controller? | capture | `coupling-hand.xrs` | hand-quad bbox moves with the hand, not the head |
| Splices per frame | status | `status.json` -> `counters.splices` | > 0 in a 3D scene, 0 on menus/videos (mono fallback) |
| Frame pacing collapse | capture | `unfocused-pacing.xrs` | `@fps 60` across FOCUSED -> VISIBLE -> FOCUSED |
| Wedge / hang | soak | `tools\soak.ps1 -Minutes 10` | exit 0; 2 = log stalled, 4 = died |
| What was the headset fed? | dump | `game-cmd.ps1 "dump frame"` | `dumps\capture_*.bmp` (SBS), `eye_*_left/right.png` |
| Comfort, judder, world scale, warp | headset | F10 overlay + the user | the verdict; write it in STATUS |

## 2. The simulated runtime (`dvr_xrsim32.dll`)

A purpose-built 32-bit OpenXR runtime (`src/tools/xrsim`, from the BioShock trilogy mod) that
presents as a Meta Quest 3 (2064x2208 per eye, measured VDXR FOVs, IPD 63 mm). It links
nothing from the mod and ships in no release. Selected per process with `XR_RUNTIME_JSON`
(`tools\xrsim-install.ps1` writes the manifest; the registry is never touched). The mod's own
loader negotiation reads that variable first, so no loader is involved.

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
`derived.claimRatioH`, `derived.aimRayMaxDevDeg`, `stats.meanLuma`, `stats.nonBlackPct`).

Hard invariants: no wait in the sim is unbounded (30 s starve grant); the control channel
polls on its own thread (a frame-path poller could never receive the `step` that unblocks it);
`command.txt` older than the sim's start is discarded.

Not modelled: lens distortion, timewarp/ASW, real display cadence, Wi-Fi encode, guardian. A
pacing bug that reproduces in the sim is real; one that does not may still exist on VDXR.

## 3. The end-to-end agent workflow

```powershell
.\tools\xrsim-selftest.ps1                       # 0. the SIM is healthy
.\tools\build.ps1 -Install                       # 1. build + install (needs the game; -SkipDxvk to keep the installed fork)
$g = .\tools\xrsim-launch.ps1                    # 2. launch on the sim; throws unless runtime == dvr-xrsim
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

## 5. Failure modes and gotchas

1. An elevated shell: the Khronos loader ignores `XR_RUNTIME_JSON` there; the mod's own
   negotiation does not, but keep the rule. `xrsim-launch.ps1` refuses.
2. A 64-bit `dvr_xrsim32.dll` is silently skipped by a 32-bit process and the real runtime is
   used; `xrsim-install.ps1` checks the PE machine.
3. The sim launcher must start `Dishonored.exe` directly (`XR_RUNTIME_JSON` is per process);
   Steam does not know about it, so `boot.ps1 -Attach` is mandatory or Steam starts a second
   game.
4. `DISHONORED_VR_BACKEND=openxr` is set by the sim launcher: with no VD/SteamVR running, auto
   would pick OpenVR.
5. `command.txt` written with a BOM corrupts the first token; the scripts use `WriteAllText`.
6. The seam polls from Present: if the game pauses its render loop unfocused
   (`[Screen] KeepAliveUnfocused=0`), commands wait until the window is foregrounded.
7. A `command.txt` older than the process is discarded at the first poll (the log says so).
8. The wrist HUD and the hand models are not drawn in the XR quad/cylinder modes; captures
   of those are expected to lack them (KNOWN_ISSUES).
9. The fork's mono fallback: menus, videos and loading screens submit the whole frame to both
   eyes (`counters.splices == 0`); a stereo assertion on a menu is a false alarm.
10. `game-shot.ps1` uses `PrintWindow` on the D3D9-through-Vulkan window; whether it captures
    non-black is unverified until D1.
11. The game is not installed on the dev PC as of 2026-09-02: rows 8-20 of the table are
    written from the BioShock harness's shape and need their first attended run.

## 6. What still needs a human in a headset

Comfort, judder, warp, world scale, hand placement feel, the wrist HUD's readability, and
anything about Virtual Desktop's own reprojection. Write the verdict in STATUS with the
build id from the log's first line.
