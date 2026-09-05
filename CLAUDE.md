# dishonored-vr - Claude session guide

A VR mod for Dishonored (2012, Arkane; Unreal Engine 3 build 9099, 32-bit, Direct3D 9,
Scaleform UI). It is a `d3d9.dll` proxy dropped next to `Dishonored.exe`. The game renders
natively through the system D3D9; once per Present the active STEREO METHOD turns the
game's frame into an eye-tagged D3D11 texture (41.0: the mono screen, the same image in both
eyes; alternate-eye and scene-draw re-entry are the two methods being built and compared),
and the OpenXR runtime layer adopted from the BioShock trilogy VR mod submits it. The proxy
drives the game camera from the head pose on the script lane, serves the VR controllers as a
gamepad, and (when the motion controls are back on) draws its own hands and aims Blink and
projectiles by hand. One backend: OpenXR - VDXR (Quest via Virtual Desktop), any native
32-bit runtime, or the bundled `dvr_steamvr32.dll` shim for SteamVR rigs. Original mod by
GingasVR (shipped alpha 38.92, discontinued); this repo continues it with the author's
permission. Single game, one branch: `VR-Main`.

## Hard rules

- **NEVER commit game-derived content**: no decompiled UnrealScript, no extracted assets, no
  frame dumps, captures or crash dumps. `tools/uscript/` and `*.png/*.bmp/*.dmp` are gitignored
  for a reason. Findings go to `docs/dishonored/ENGINE_NOTES.md`, never game code.
- **Every engine address, IAT slot and UE3 field offset lives in
  `src/game/dishonored/patterns.h`** and is documented in ENGINE_NOTES with how it was derived.
  The exe has no ASLR (base 0x400000), so absolute addresses are fine, but every code hook
  byte-verifies its target and refuses on a mismatch. Never copy a number from another game.
- **NEVER quote a chat verbatim in anything published to GitHub.** Not in commit messages, PR
  titles or bodies, issues, comments, code comments, or any file under `docs/`. This covers
  the tester's words, the maintainers' words and any other conversation the work came from.
  **Report the OBSERVATION, not the sentence**: "no ghosting reported at 90 Hz, still reported
  at 120" carries every fact "it was still bad" does, survives being read by a stranger, and
  cannot embarrass the person who said it. A perceptual report is evidence and belongs in the
  record - its exact wording never is. Numbers, log lines, ini keys and the game's own shipped
  comments are not chat and stay quotable.
- **No em dashes anywhere** (code, strings, ini text, docs, scripts, commits): use `-`.
  PowerShell 5.1 parse errors and log/UI mojibake. `tools\lint.ps1` enforces it.
- **Commit messages**: plain conventional commits (`feat:`/`fix:`/`docs:`/`build:`/`tools:`/
  `chore:`/`refactor:`), imperative, subject <= 72 chars, no trailers, no AI
  attribution.
- **32-bit only.** The CMake guard stops a 64-bit configure; don't fight it.
- **The DXVK fork is gone** (removed in 41.0; git history keeps it under the `dxvk-*` tags).
  Do not bring back a Vulkan translation layer: the game renders natively through D3D9.
- **The runtime layer stays as close to the BioShock copy as the D3D9 host allows.**
  `core/vr/openxr_runtime.cpp` is a 5k-line proven layer; the two D3D9 seams (the device
  provider, the frame texture) are marked `41.0 (Dishonored)`. Fixes port between the two
  projects only while the rest stays verbatim; `core/vr/hud_stub` keeps the coupling out.
- **Every new render lever ships default OFF with a live A/B toggle.** A stereo method is a
  lever: it registers by name, `stereo <name>` switches live, a method that refuses leaves
  the previous one running (fail soft).
- **Retired experiments go to `src/legacy/`** (compiled only with `-DDVR_WITH_LEGACY=ON`), never
  deleted silently. The 41.0 removals are the exception, made on purpose: one commit each so
  `git revert` restores one piece.
- **No code from UEVR** (all rights reserved; concepts only). REFramework (MIT) may be adapted
  with an attribution comment.
- Terminology: "proxy" = `d3d9.dll` (our code); "runtime layer" = `core/vr/openxr_runtime`
  (the OpenXR instance/session/pacing/poses/layers); "shim" = `dvr_steamvr32.dll`
  (OpenXR-on-OpenVR for SteamVR rigs); "method" = a stereo strategy behind `core/gfx/stereo.h`
  (mono | aer | reentry); "rung" = its place on the ladder; "the stereo seam" = that interface;
  "the camera seam" = `game/dishonored/camera.h` (rotation, FOV, eye offset per eye); "eye tag" =
  the -1/+1/0 a method attaches to a present; "seam" alone = `command.txt`; "lane" = the thread
  a write belongs to (present thread vs script thread).
- The engineering rules carried over from the BioShock trilogy mod (33 sessions of cost):
  a verified write is not an honoured one (acceptance is a measured downstream effect); never
  copy a number between games; an instrument that cannot fail its own hypothesis is not
  evidence; sample strided and vote, a frame is not homogeneous; a measurement carries the
  identity of what it measured; a counter is not evidence until you know its population;
  measure first; prefer a falsifiable prediction over another capture; derive lens laws at more
  than one aspect; identify a render pass by making it MOVE; every new render lever ships
  default OFF with a live A/B toggle; one ray (anything that claims to point where shots go
  derives from the identical ray); engine-side writes let attachments follow for free, matrix
  patches do not; fail soft (a failed hook logs and the game runs flat); stopping and handing
  back are different operations; never take a reference to an engine D3D object inside a
  detour; backbuffer detectors sample before our own writers; ImGui only from the overlay's
  draw callback; a probe hook's argument count must equal `ret imm / 4`; the present thread
  owns every runtime call (the pace thread runs `xrWaitFrame` by request only).
- The original author's process rules (`docs/dishonored/HANDOFF-GINGASVR.md` section 11, each
  one paid for): one behavioral change per build; build on a snapshot confirmed good; when
  something that worked breaks, diff against the working build FIRST; measure before
  theorising (read the artifacts already on disk); never ship a guessed constant as a
  measured one; the packaged `dishonored_vr.ini` is a byte copy of the tested machine's ini;
  motion controls for crouching and hands must never stop working; do not attribute logs to
  machines by drive letter, check the build tag. Read the handoff's Traps and Dead ends
  before writing code.

## Logging

**Log generously. The log is the only instrument a remote tester can send back.**
Most of this project's open bugs live on machines we cannot attach a debugger to, so a run
that reproduces a fault and explains nothing is a wasted run and a wasted tester. Every
session writes `dishonored_vr.log` next to the exe (previous run rotated to
`dishonored_vr.prev.log`); `dvr::log::init` runs from `DllMain` before anything else can
fail, and **must never be gated** on VR bring-up, a config read, or a headset being present.
A run always produces a log, even one that dies in the first second.

Extensive does not mean noisy. The rules that buy volume without cost:

- **Use the levels.** `Error`/`Warn` are for things a player or a maintainer must act on;
  `Info` is the readable narrative of a run and is what a bug report contains; `Debug`/
  `Trace` belong to a lane under investigation and stay off until asked for
  (`DVR_LOG=trace`, `DVR_LOG_CATS=hands:debug,openxr:trace`).
- **Never pay for a line you do not print.** `DVR_LOG` and friends evaluate their arguments
  only after the per-category threshold passes, so put the work INSIDE the call. Computing
  into a local first, or reading engine memory to build an argument, spends the cost
  unconditionally and defeats the gate.
- **Nothing unbounded in a per-frame or per-draw path.** Use `DVR_LOG_EVERY_MS`,
  `DVR_LOG_ONCE` or `DVR_LOG_FIRST_N`. An ungated per-frame `Info` line is both a
  performance bug and a way to bury the three lines that mattered.
- **Log state CHANGES, not state.** `capture: RESOLUTION CHANGED MID-SESSION 1920x1080 ->
  2560x1440` earns a `Warn`; the same size every frame earns nothing.
- **An instrument that cannot fail its own hypothesis is not evidence.** A counter line must
  also say what would make the counter move, and it must be able to print the unwelcome
  answer. Two heartbeat counters read 0 **by design** for a whole architecture and were read
  as "the hands are dead" by three separate readers, including the original author (40.1).
  If a zero is expected, the line must say so on the line (the `stereo: beat` line does:
  `L/s=0 R/s=0 ... (L/R read 0 by design on the mono screen)`).
- **Name the owner before the result.** Where several subsystems can drive one thing
  (hands, camera, the frame), log WHICH one owns it, then what it did. Otherwise a healthy
  run and a dead one produce identical text.
- **Every refused guard says why, with the values.** A hook that declines, a probe that
  finds nothing, a method that refuses: log the reason and the numbers that produced it.
- **Log the derived number, not just the inputs.** Where geometry decides what the player
  sees (subtended angles, aspect, world scale, the capture's bbox), log the computed result
  so a complaint like "everything is too big" is arithmetic instead of opinion.

## Session protocol

- **START**: read `docs/STATUS.md`, the current milestone in `docs/ROADMAP.md`, then
  `git log --oneline -10`. Branch off `VR-Main`. Touching engine internals? Read
  `docs/dishonored/ENGINE_NOTES.md` first; new findings go there in the same commit as the code.
- **Validate in the SIMULATOR before asking for a headset.** `tools\xrsim-launch.ps1` runs the
  game against `dvr_xrsim32.dll`, a simulated 32-bit OpenXR runtime that presents as a Quest 3:
  head/hand poses, every controller button, deterministic frame stepping and per-eye compositor
  captures with source stats, no headset. Catalog: `docs/VERIFICATION.md`. The SteamVR shim
  needs SteamVR. Perceptual questions (comfort, judder, world scale, warp) still need the
  headset and the F10 overlay.
- Non-obvious design choices get a dated entry in the decision log at the bottom of
  `docs/ARCHITECTURE.md`.
- **END**: rewrite "Current state" and "Next steps" in `docs/STATUS.md`, append a dated session
  log entry, tick `docs/ROADMAP.md` boxes, commit, push. A session that ends without pushing
  STATUS.md is a failed handoff. Copy the headset log out before every relaunch (rotation is
  one deep).

## Build / install / test

```powershell
.\tools\build.ps1 [-Release] [-Legacy]     # d3d9.dll + dvr_steamvr32.dll + dvr_xrsim32.dll + xr_hello32.exe (VS-bundled CMake via vswhere)
.\tools\install.ps1 [-Release]             # d3d9.dll + dvr_steamvr32.dll + openvr_api.dll -> <game>\Binaries\Win32
.\tools\setup-game-ini.ps1 -Console        # the F1 console bind in DishonoredInput.ini (backs it up)
.\tools\tail-log.ps1 [-Grep "xr:|crash"]   # follow <game>\Binaries\Win32\dishonored_vr.log
.\tools\xrsim-selftest.ps1                 # is the SIMULATOR healthy? (xr_hello32, no mod)
.\tools\xrsim-launch.ps1 [-ViaSteam]       # launch the game on the simulator (-ViaSteam: manifest through [VR] XrRuntimeJson)
.\tools\xrsim-cmd.ps1 "head rot 30 0 0"    # drive the simulated head/hands/controls
.\tools\xrsim-shot.ps1 -Out shot           # per-eye compositor capture + JSON to assert on
.\tools\xrsim-run.ps1 -Path tools\xrsim\mono.xrs   # the rung-1 gate: both eyes non-black
.\tools\game-cmd.ps1 "stereo status"       # the mod's command seam (command.h / commands.cpp); also camera, vrpace, vrinput
.\tools\status-dump.ps1                    # status.json, pretty-printed
.\tools\exports-check.ps1 build\src\Debug\d3d9.dll ; .\tools\lint.ps1
```

- Game: Steam appid 205100, `<library>\steamapps\common\Dishonored\Binaries\Win32\Dishonored.exe`.
  Scripts resolve it from `DVR_GAME_DIR` or Steam's `libraryfolders.vdf` and throw otherwise.
  STATUS says whether the game is installed on the dev PC and what has run on it.
- Game config: `%USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\`.
- Files next to the exe: `dishonored_vr.ini`, `dishonored_vr.log` (+`.prev.log`),
  `dishonored_vr_crash.txt`, `disable_vr.txt` (kill switch). Harness files in
  `%LOCALAPPDATA%\DishonoredVR\`: `command.txt`, `ack.txt`, `status.json`, `dumps\`, `xrsim\`,
  `steamvr\` (the shim's manifest), `ovrshim.log`. Override with `DVR_DATA_DIR`.
- Env knobs: `DVR_LOG=trace`, `DVR_LOG_CATS=blink:debug,openxr:trace`, `DVR_SKIP=hands,overlay`,
  `XR_RUNTIME_JSON` (read first by the loader; `[VR] XrRuntimeJson` sets it when absent),
  `DVR_XRSIM_DIR`. `[Paths] DataDir=` in the ini moves the mod's data dir (the harness reads
  `DVR_DATA_DIR`); on this dev PC both point at `D:\dvr-data` (VERIFICATION gotcha 14).
- Clean clone needs `git clone --recursive` (submodules under `third_party/`).

## Repo map

- `src/proxy/` - `DllMain` and the nine `d3d9.dll` exports (`d3d9.def`); chains to System32
- `src/core/` - engine-agnostic VR core: `util/` (log, crash, clock, mem, ini, paths, diag,
  xr_math), `hooks/` (vtable, IAT, detour), `framework/` (frame_hooks = the D3D9 hooks and
  the frame path's order; vs_const_hook; command seam; status.json), `gfx/` (stereo = the seam
  and registry, capture, blit_quad, mono_screen, aer and reentry stubs, d3d11_device, hand
  meshes, frame dumps), `vr/` (openxr_runtime = the runtime layer, openxr_input = the action
  layer, hud_stub), `input/` (virtual gamepad, hotkeys), `window/` (the game window's
  subclass), `ui/` (overlay), `config/`
- `src/game/dishonored/` - everything that knows an address or a UE3 layout: `patterns.h`,
  `ue3/` (UObject/GNames, ProcessEvent hook), camera (the per-eye seam + eyetest), head
  tracking, present_tick (the game side of the frame path), hands (SkelControl, graft, arms
  hide), Blink, crouch, melee, motion aim, FOV lever, console, game state, the seam's game words
- `src/legacy/` - retired experiments and one-off diagnostics (off by default)
- `src/mod/` - the unity translation unit: `state/` (types and globals in original order),
  `fwd.h` (every prototype), `dishonoredvr.cpp` (includes the modules). See ARCHITECTURE for
  why this exists and how a module leaves it.
- `src/tools/` - `xrsim/` (the simulated OpenXR runtime), `ovrshim/` (the SteamVR shim),
  `xr_hello32/` (smoke client)
- `third_party/` - imgui, OpenXR-SDK (submodules; the static loader links into the proxy),
  vendored OpenVR headers + `openvr_api.dll` (for the shim)
- `tools/` - the PowerShell harness (`lib/` shared helpers, `xrsim/*.xrs` sequences) and the
  one-shot refactor scripts kept as the record of what changed
- `tests/golden/` - the default ini (generated from WriteDefaultIni), the export table
- `docs/` - the project's brain; index below

## Docs index

| File | Purpose |
|---|---|
| `docs/STATUS.md` | **Session handoff**: current state, next steps (one paragraph per developer), blockers, session log |
| `docs/ROADMAP.md` | Milestones S0-S3 (the stereo ladder) with "done when" criteria and checkboxes; the carried D-items after |
| `docs/ARCHITECTURE.md` | The frame path, the stereo ladder, the runtime layer, the camera seam, thread contracts, the unity build and how modules leave it, decision log |
| `docs/RESEARCH.md` | Engine facts, prior art, VR runtime facts, legal posture, all with sources |
| `docs/VERIFICATION.md` | **Verification catalog**: intent -> tool -> command -> how to read the result; the simulator and its instruments, the seam, captures, what still needs a human |
| `docs/CODE_REVIEW.md` | Every finding from the review of the original single file, with disposition |
| `docs/KNOWN_ISSUES.md` | User-facing known issues (ships in the zip), each linked to a milestone |
| `docs/TROUBLESHOOTING.md` | User-facing troubleshooting (ships in the zip) |
| `docs/RELEASE_NOTES.md` | Per-version notes; 41.0 "Upgrading" lists every removed key |
| `docs/dishonored/ENGINE_NOTES.md` | The reverse-engineering knowledge base: addresses with derivation, class layouts, hook points, the per-eye camera seam's write points, the head-coupling chronology, dead ends |
| `docs/dishonored/TESTING.md` | Install/launch loop, flat and simulator checks, headset checklist, crash triage |
| `docs/dishonored/XR_HANDOFF.md` | The pre-41.0 OpenXR/Quest presentation bug (historical; the pipeline it describes is gone) |
| `docs/dishonored/HANDOFF-GINGASVR.md` | **The original author's handoff** (their build 39.4): what was measured, disproved, the traps, the process rules, the 39.x fixes our base lacks |
