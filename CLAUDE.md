# dishonored-vr - Claude session guide

A VR mod for Dishonored (2012, Arkane; Unreal Engine 3 build 9099, 32-bit, Direct3D 9,
Scaleform UI). It is a `d3d9.dll` proxy dropped next to `Dishonored.exe` plus a forked DXVK
(`dxvk_d3d9.dll`, in `dxvk/`) that does the per-eye stereo rendering. The proxy captures the
game's side-by-side frame, presents it to the headset, drives the game camera from the head
pose, serves the VR controllers as a gamepad, draws its own hands, and aims Blink and
projectiles by hand. Two VR backends behind one pipeline: OpenVR (SteamVR headsets, the path
the original author tuned) and OpenXR (Quest via Virtual Desktop's VDXR, loaded by the mod's
own loader negotiation). Original mod by GingasVR (shipped alpha 38.92, discontinued); this
repo continues it with the author's permission. Single game, one branch: `VR-Main`.

## Hard rules

- **NEVER commit game-derived content**: no decompiled UnrealScript, no extracted assets, no
  frame dumps, captures or crash dumps. `tools/uscript/` and `*.png/*.bmp/*.dmp` are gitignored
  for a reason. Findings go to `docs/dishonored/ENGINE_NOTES.md`, never game code.
- **Every engine address, IAT slot and UE3 field offset lives in
  `src/game/dishonored/patterns.h`** and is documented in ENGINE_NOTES with how it was derived.
  The exe has no ASLR (base 0x400000), so absolute addresses are fine, but every code hook
  byte-verifies its target and refuses on a mismatch. Never copy a number from another game.
- **No em dashes anywhere** (code, strings, ini text, docs, scripts, commits): use `-`.
  PowerShell 5.1 parse errors and log/UI mojibake. `tools\lint.ps1` enforces it.
- **Commit messages**: plain conventional commits (`feat:`/`fix:`/`docs:`/`build:`/`tools:`/
  `chore:`/`refactor:`), imperative, subject <= 72 chars, no trailers, no AI
  attribution.
- **32-bit only.** The CMake guard stops a 64-bit configure; don't fight it.
- **The DXVK fork is gone** (removed in 41.0; git history keeps it under the `dxvk-*` tags).
  Do not bring back a Vulkan translation layer: the game renders natively through D3D9.
- **Retired experiments go to `src/legacy/`** (compiled only with `-DDVR_WITH_LEGACY=ON`), never
  deleted silently. The refactor milestone is behavior-preserving until D1 (headset parity).
- **No code from UEVR** (all rights reserved; concepts only). REFramework (MIT) may be adapted
  with an attribution comment.
- Terminology: "proxy" = `d3d9.dll` (our code); "fork" = `dxvk_d3d9.dll`; "splice" = the fork's
  per-eye draw replay; "twin" = a right-eye render target; "seq" = frame-sequential mode;
  "backend" = OpenVR | OpenXR; "seam" = `command.txt`; "lane" = the thread a write belongs to
  (present thread vs script thread).
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
  draw callback; a probe hook's argument count must equal `ret imm / 4`.
- The original author's process rules (`docs/dishonored/HANDOFF-GINGASVR.md` section 11, each
  one paid for): one behavioral change per build; build on a snapshot confirmed good; when
  something that worked breaks, diff against the working build FIRST; measure before
  theorising (read the artifacts already on disk, including the fork's own
  `Dishonored_d3d9.log`); never ship a guessed constant as a measured one; the packaged
  `dishonored_vr.ini` is a byte copy of the tested machine's ini; motion controls for
  crouching and hands must never stop working; do not attribute logs to machines by drive
  letter, check the build tag. Read the handoff's Traps and Dead ends before writing code.

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
- **Log state CHANGES, not state.** `capture: 4032x2268 -> 2560x1440` earns a `Warn`; the
  same size every frame earns nothing.
- **An instrument that cannot fail its own hypothesis is not evidence.** A counter line must
  also say what would make the counter move, and it must be able to print the unwelcome
  answer. Two heartbeat counters read 0 **by design** for a whole architecture and were read
  as "the hands are dead" by three separate readers, including the original author (40.1).
  If a zero is expected, the line must say so on the line.
- **Name the owner before the result.** Where several subsystems can drive one thing
  (hands, camera, resolution), log WHICH one owns it, then what it did. Otherwise a healthy
  run and a dead one produce identical text.
- **Every refused guard says why, with the values.** A hook that declines, a probe that
  finds nothing, a resolution that is not honoured: log the reason and the numbers that
  produced it. "setres -> (empty reply)" is a dead end; the mode list, the request and the
  result together are a diagnosis.
- **Log the derived number, not just the inputs.** Where geometry decides what the player
  sees (subtended angles, aspect, world scale), log the computed result so a complaint like
  "everything is too big" is arithmetic instead of opinion.

## Session protocol

- **START**: read `docs/STATUS.md`, the current milestone in `docs/ROADMAP.md`, then
  `git log --oneline -10`. Branch off `VR-Main`. Touching engine internals? Read
  `docs/dishonored/ENGINE_NOTES.md` first; new findings go there in the same commit as the code.
- **Validate in the SIMULATOR before asking for a headset.** `tools\xrsim-launch.ps1` runs the
  game against `dvr_xrsim32.dll`, a simulated 32-bit OpenXR runtime that presents as a Quest 3:
  head/hand poses, every controller button, deterministic frame stepping and per-eye compositor
  captures, no headset. It covers the OpenXR lane only; the OpenVR lane needs SteamVR. Catalog:
  `docs/VERIFICATION.md`. Perceptual questions (comfort, judder, world scale, warp) still need
  the headset and the F10 overlay.
- Non-obvious design choices get a dated entry in the decision log at the bottom of
  `docs/ARCHITECTURE.md`.
- **END**: rewrite "Current state" and "Next steps" in `docs/STATUS.md`, append a dated session
  log entry, tick `docs/ROADMAP.md` boxes, commit, push. A session that ends without pushing
  STATUS.md is a failed handoff.

## Build / install / test

```powershell
.\tools\build.ps1 [-Release] [-Legacy]     # d3d9.dll + dvr_xrsim32.dll + xr_hello32.exe (VS-bundled CMake via vswhere)
.\tools\install.ps1 [-Release]             # d3d9.dll + openvr_api.dll -> <game>\Binaries\Win32
.\tools\setup-game-ini.ps1 -Resolution     # ResX=4032 ResY=2268 Fullscreen=False in DishonoredEngine.ini (backs it up)
.\tools\tail-log.ps1 [-Grep "xr:|crash"]   # follow <game>\Binaries\Win32\dishonored_vr.log
.\tools\xrsim-selftest.ps1                 # is the SIMULATOR healthy? (xr_hello32, no mod)
.\tools\xrsim-launch.ps1                   # launch the game on the simulator (forces the OpenXR backend)
.\tools\xrsim-cmd.ps1 "head rot 30 0 0"    # drive the simulated head/hands/controls
.\tools\xrsim-shot.ps1 -Out shot           # per-eye compositor capture + JSON to assert on
.\tools\game-cmd.ps1 "status"              # the mod's command seam (command.h / commands.cpp)
.\tools\status-dump.ps1                    # status.json, pretty-printed
.\tools\exports-check.ps1 build\src\Debug\d3d9.dll ; .\tools\lint.ps1
```

- Game: Steam appid 205100, `<library>\steamapps\common\Dishonored\Binaries\Win32\Dishonored.exe`.
  Scripts resolve it from `DVR_GAME_DIR` or Steam's `libraryfolders.vdf` and throw otherwise.
  **The game is NOT installed on the dev PC as of 2026-09-02**; everything since the refactor is
  build-verified and simulator-verified only (see STATUS).
- Game config: `%USERPROFILE%\Documents\My Games\Dishonored\DishonoredGame\Config\`.
- Files next to the exe: `dishonored_vr.ini`, `dishonored_vr.log` (+`.prev.log`),
  `dishonored_vr_crash.txt`, `disable_vr.txt` (kill switch), `dxvk_stereo.txt` (fork marker).
  Harness files in `%LOCALAPPDATA%\DishonoredVR\`: `command.txt`, `ack.txt`, `status.json`,
  `dumps\`, `xrsim\`. Override with `DVR_DATA_DIR`.
- Env knobs: `DISHONORED_VR_BACKEND=openvr|openxr|auto`, `DVR_LOG=trace`,
  `DVR_LOG_CATS=blink:debug,openxr:trace`, `DVR_SKIP=hands,overlay`, `XR_RUNTIME_JSON`.
- Clean clone needs `git clone --recursive` (submodules under `third_party/`).

## Repo map

- `src/proxy/` - `DllMain` and the nine `d3d9.dll` exports (`d3d9.def`); chains to the fork
- `src/core/` - engine-agnostic VR core: `util/` (log, crash, clock, mem, ini, paths, diag),
  `hooks/` (vtable, IAT, detour), `framework/` (Present/CreateDevice hooks, command seam,
  status.json, VP helpers), `gfx/` (capture, D3D11 present pipeline, eye quads, hand meshes,
  wrist HUD, frame dumps), `vr/` (OpenVR backend, OpenXR loader/backend/pace/input, backend
  probe), `input/` (virtual gamepad, hotkeys), `window/` (render-size hooks), `ui/` (overlay),
  `config/`
- `src/game/dishonored/` - everything that knows an address or a UE3 layout: `patterns.h`,
  `ue3/` (UObject/GNames, ProcessEvent hook), head tracking, hands (SkelControl, graft, arms
  hide), Blink, crouch, melee, motion aim, FOV lever, console, game state, the seam's game words
- `src/legacy/` - retired experiments and one-off diagnostics (off by default)
- `src/mod/` - the unity translation unit: `state/` (types and globals in original order),
  `fwd.h` (every prototype), `dishonoredvr.cpp` (includes the modules). See ARCHITECTURE for
  why this exists and how a module leaves it.
- `src/tools/` - `xrsim/` (the simulated OpenXR runtime), `xr_hello32/` (smoke client)
- `third_party/` - imgui, OpenXR-SDK (submodules), vendored OpenVR headers + `openvr_api.dll`
- `tools/` - the PowerShell harness (`lib/` shared helpers, `xrsim/*.xrs` sequences) and the
  one-shot refactor scripts kept as the record of what changed
- `tests/golden/` - the 38.92 default ini, the export tables
- `docs/` - the project's brain; index below

## Docs index

| File | Purpose |
|---|---|
| `docs/STATUS.md` | **Session handoff**: current state, next steps, blockers, session log |
| `docs/ROADMAP.md` | Milestones D0-D9 with "done when" criteria and checkboxes |
| `docs/ARCHITECTURE.md` | Module design, the frame path, the two backends, thread contracts, the unity build and how modules leave it, decision log |
| `docs/RESEARCH.md` | Engine facts, prior art, VR runtime facts, legal posture, all with sources |
| `docs/VERIFICATION.md` | **Verification catalog**: intent -> tool -> command -> how to read the result; the simulator, the seam, captures, what still needs a human |
| `docs/CODE_REVIEW.md` | Every finding from the review of the original single file, with disposition |
| `docs/KNOWN_ISSUES.md` | User-facing known issues (ships in the zip), each linked to a milestone |
| `docs/TROUBLESHOOTING.md` | User-facing troubleshooting (ships in the zip) |
| `docs/RELEASE_NOTES.md` | Per-version notes |
| `docs/dishonored/ENGINE_NOTES.md` | The reverse-engineering knowledge base: addresses with derivation, class layouts, hook points, the head-coupling chronology, the XR stamp theories, dead ends |
| `docs/dishonored/TESTING.md` | Install/launch loop, flat and simulator checks, headset checklist, crash triage |
| `docs/dishonored/XR_HANDOFF.md` | Single-bug handoff for the unconverged OpenXR/Quest path |
| `docs/dishonored/HANDOFF-GINGASVR.md` | **The original author's handoff** (their build 39.4): what was measured, disproved, the traps, the process rules, the 39.x fixes our base lacks |
