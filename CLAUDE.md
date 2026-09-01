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
  `chore:`/`refactor:`/`dxvk:`), imperative, subject <= 72 chars, no trailers, no AI
  attribution.
- **32-bit only.** The CMake guard stops a 64-bit configure; don't fight it.
- **The DXVK fork is patched only in `dxvk/` as ordinary commits**, and `dxvk/DISHONORED-FORK.md`
  (export table, patch map) is updated in the same commit. The 15+1 `dxvk_vr_*` exports are a
  contract with `src/`; change both sides together.
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
.\tools\build-dxvk.ps1                     # the fork -> build\dxvk\dxvk_d3d9.dll (meson + MSVC x86; needs meson, ninja, glslangValidator)
.\tools\install.ps1 [-Release]             # d3d9.dll + dxvk_d3d9.dll + openvr_api.dll -> <game>\Binaries\Win32
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
- Clean clone needs `git clone --recursive` (submodules under `third_party/` and `dxvk/`).

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
- `dxvk/` - DXVK 3.0.2 + the stereo fork as commits (`DISHONORED-FORK.md`)
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
