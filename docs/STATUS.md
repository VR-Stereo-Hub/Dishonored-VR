# Status

## Current state (2026-09-02, session 1 of the continuation)

The original mod (GingasVR, proxy build 38.92 + DXVK fork M8.2) is **shipped and unchanged in
behavior**; this session turned its one 23k-line file into a development framework:

- **Builds with VS2022 / CMake** (win32 preset, static CRT, C++20) instead of a MinGW
  cross-compile from Linux. `d3d9.dll` exports exactly the nine undecorated names
  (`tools\exports-check.ps1`); Debug and Release both build; `DVR_WITH_LEGACY` ON and OFF both
  build.
- **Module tree** under `src/proxy`, `src/core`, `src/game/dishonored`, `src/legacy`, produced by
  `tools/split-source.py` with every function body verbatim (`--check` proves it against commit
  48766c07). Still ONE translation unit (`src/mod/dishonoredvr.cpp`) with the original globals
  in `src/mod/state/`; the utilities (log, crash, clock, mem, ini, paths, hooks, command seam,
  status) are real modules with headers. See ARCHITECTURE "The unity build".
- **DXVK fork restored in-repo** under `dxvk/` from this repo's own history: DXVK 3.0.2 +
  the 52 patches as commits (tags `dxvk-base`, `dxvk-m8.2-shipped`, `dxvk-m8.4`,
  `dxvk-shipped` = M8.2 + M8.4 reverted, matching what proxy 38.92 targets). Not built yet on
  this PC (needs meson, ninja, glslangValidator; `tools\build-dxvk.ps1` is written but unrun).
- **Logging/debugging**: leveled, tagged log with ring buffer and `.prev.log` rotation; crash
  fingerprint + minidumps; `command.txt` seam with ack; `status.json`; frame/eye/HUD dumps; F10
  overlay Log tab; `[game] state:` transition line.
- **Backend probe** replaces the process-snapshot auto-detect (Quest over Link/Air Link/Steam
  Link fell through to OpenVR before).
- **Simulator** (`dvr_xrsim32.dll`, Quest-3-shaped 32-bit OpenXR runtime) builds and passes
  `tools\xrsim-selftest.ps1` on this PC (60 frames, FOCUSED, 0 errors).
- **Harness** copied and adapted from the BioShock trilogy mod: build/install/uninstall/package,
  launch/boot/game-cmd/key/click/shot/batch, xrsim-*, img-diff, soak, eye-check, exports and
  lint checks, log-parse, status-dump, setup-game-ini.
- **Docs** written: this file, ROADMAP, ARCHITECTURE, RESEARCH, VERIFICATION, CODE_REVIEW,
  KNOWN_ISSUES, TROUBLESHOOTING, RELEASE_NOTES, dishonored/ENGINE_NOTES, TESTING, XR_HANDOFF,
  CLAUDE.md.

**Not done from the plan** (deferred, in priority order): the table-driven config module
(Phase 3: `[VR]`, `[Hands]` and other keys are still missing from the generated default ini);
the `IVrBackend` interface (Phase 5: both backends still share globals and `g_xrOn` switches);
dissolving `src/mod/state` globals into module state (Phase 9); the mechanical review fixes
that touch behavior (CODE_REVIEW items marked "next").

**The game is NOT installed on the dev PC.** Nothing here has run inside Dishonored. The
refactored DLL is build-verified, export-verified and body-verified only; the first thing the
next session with the game does is D1 (parity boot on the SteamVR lane, then the simulator).

## Next steps

1. Install Dishonored; `tools\setup-game-ini.ps1 -Resolution`; `tools\build.ps1 -Install
   -SkipDxvk` with the SHIPPED `dxvk_d3d9.dll` from the release zip (or build the fork:
   install meson/ninja/glslang, `tools\build-dxvk.ps1`); boot on the SteamVR lane and compare
   `dishonored_vr.log` against a 38.92 log (same hook installs, splice counts, fps). That is D1.
2. Simulator lane: `tools\xrsim-launch.ps1`, confirm `xr: runtime "dvr-xrsim"` and `xr:
   pipeline READY`, `tools\boot.ps1 -Attach`, then `smoke.xrs` and `stereo.xrs`. Fix the
   boot predicate and the menu key sequence on the first attended run.
3. Phase 3 config table (docs/ARCHITECTURE "Config"), then the `[Log]` keys and the missing
   sections land in users' inis via `write_missing`.
4. D3: the OpenXR handoff (`docs/dishonored/XR_HANDOFF.md`).

## Blockers

- No game on the dev PC (2026-09-02).
- DXVK build toolchain (meson, ninja, glslangValidator) not installed on the dev PC.

## Session log

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (StampFix inert); the hand-skin `.mtl` path used `\v` and `\%` escapes so materials
never loaded (fixed); 165 em dashes swept. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes.
