# Status

## Current state (2026-09-02, session 1 of the continuation)

The original mod (GingasVR, public release = proxy build 38.92 + DXVK fork M8.2) is
**shipped and unchanged in behavior**; this session turned its one 23k-line file into a
development framework:

- **Builds with VS2022 / CMake** (win32 preset, static CRT, C++20) instead of a MinGW
  cross-compile from Linux. `d3d9.dll` exports exactly the nine undecorated names
  (`tools\exports-check.ps1`); Debug and Release both build; `DVR_WITH_LEGACY` ON and OFF both
  build. Version 40.0.0 (the author's private line already reached 39.4).
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
  CLAUDE.md, and the author's own handoff (`docs/dishonored/HANDOFF-GINGASVR.md`).

**What "ported" means today.** Every function of the original file is in the new tree,
grouped by subsystem, bodies unchanged, and it builds as one program. The internals are only
partly converted to real modules: the ~1,500 original globals still sit in `src/mod/state`
and most modules cannot compile alone. Nothing was dropped.

**The author's handoff changes the picture in three ways** (HANDOFF-GINGASVR.md):

1. Their private builds 39.0-39.4 (rebased onto 38.72, the last owner-confirmed-good
   build) carry fixes our 38.92 base lacks: calibration records banked by asset name
   (39.0, "wiggling weapons"), a closed loop on the pitch write that lets the fallback
   engage when the engine discards it (39.2), **the D3D11 device created on the adapter the
   OpenXR runtime asks for** (39.3; our tree still passes `NULL` and
   `D3D11_RESOURCE_MISC_SHARED`, `src/core/gfx/d3d11_device.cpp`), and the uncovered
   menu-ghost quadrant (39.4). Their 39.x line LACKS the 38.78 focus keep-alive fix, which
   our 38.92 base has.
2. 38.73-38.92 stacked unverified changes (the chain-stamp / StampLive / StampFix
   series); the author considers 38.72 the good base. D1 parity must therefore compare our
   build against 38.72-era behavior plus the 39.x fixes, not against 38.92 alone.
3. The best current explanation for "works only on his PC" (the Quest zoom / no-pitch bug) is
   the dual-GPU adapter mismatch, never confirmed by an affected user; GPU vendor/model/driver
   was never collected from one.

**The game is NOT installed on the dev PC.** Nothing here has run inside Dishonored. The
refactored DLL is build-verified, export-verified and body-verified only.

## Next steps (the plan)

The port finishes opportunistically; the headset work comes first.

1. **Sources from the author.** Ask GingasVR for `dllmain_38.72.cpp`, `dllmain_39.4.cpp` and
   the fork's p53 commit (`45116f2f`, `dxvk_vr_view`) from `G:\back\Dishonored vr\out\`. Diff
   38.72 vs 38.92 (what the rebase dropped) and 38.92 vs 39.4 (what to port). Without them,
   port from the handoff's descriptions.
2. **Port the 39.x fixes onto our tree**, one behavioral change per commit, in this order:
   39.3 adapter LUID (also fixes CODE_REVIEW 28), 39.4 menu quadrant, 39.2 pitch closed loop,
   39.0 calibration bank. Keep the 38.78 keep-alive.
3. **D1 parity with the game installed** (SteamVR lane): `setup-game-ini.ps1 -Resolution`,
   `install.ps1 -SkipDxvk` with the shipped fork, boot, compare the log against a 38.72/39.4
   log from the author's rig. Then the simulator lane: `xrsim-launch.ps1`, `boot.ps1
   -Attach`, `smoke.xrs`, `stereo.xrs`. Note the handoff's trap: a direct exe launch crashes
   at the menu, so the sim launcher must go through Steam with `[VR] XrRuntimeJson` and
   `[VR] Backend=openxr` written to the ini instead of env vars (VERIFICATION gotcha 12).
4. **Get an affected Quest user on a 39.3+-equivalent build** and read the adapter lines in
   their log; collect GPU vendor/model/driver. This is the highest-value single test.
5. **`IVrBackend`** (the Quest work depends on it), then the config table, then the
   remaining modules as they are touched (ARCHITECTURE "how a module leaves the unity build").
6. Port the process rules into practice: one behavioral change per build; diff against the
   last good build first; the packaged ini is a byte copy of the tested machine's ini.

## Blockers

- No game on the dev PC (2026-09-02).
- DXVK build toolchain (meson, ninja, glslangValidator) not installed on the dev PC.
- The 38.72 / 39.4 sources and the p53 fork commit live only in the author's archive.

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
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.
