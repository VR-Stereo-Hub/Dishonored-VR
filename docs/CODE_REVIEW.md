# Code review of the original single file (build 38.92)

Line numbers refer to `src/dllmain.cpp` at commit 48766c07. Disposition: **fixed** (this
session), **structural** (fixed by a planned module), **next** (a behavior change that needs
the game to verify), **roadmap** (a milestone), **legacy** (retired code).

| # | Finding | Where | Risk | Disposition |
|---|---|---|---|---|
| 1 | The generated default ini has 16 sections but `LoadConfig` reads 26; `[VR]`, `[Hands]`, `[Blink]`, `[VRHands]`, `[Overlay]`, `[Hud]` keys never appear in a fresh ini, and `OverlaySaveDefaults` writes keys the default file lacks | 2191-2392 vs 2394-3150, 4275-4568 | users cannot discover the knobs; support burden | structural: config table (`write_missing`) |
| 2 | Defaults disagree between the global initializer and the ini default: `DistanceMeters` 1.8 vs 1.6, `Separation` 0.020 vs 0.014, `Convergence` 120 vs 140, `FillScale` 1.0 vs 1.70, `DelaySec` 4.0 vs 1.5; DllMain clamps `SpoofDesktopW/H` to 1280..4096/720..2304, LoadConfig to ..8192 | 484/2419, 1818/2445, 490/2416, 2067/2940, 22947 vs 3093 | a key missing from the ini silently picks a different value than a fresh ini | structural: one default per key in the table |
| 3 | `Version < 9` rewrites the whole ini, destroying the user's tuning | 2403-2407 | data loss on upgrade | structural: additive `write_missing` |
| 4 | Cross-thread plain globals: `g_xrOn` written on the present thread, read on the window and game-input threads; `g_devPose` (3x4 floats) written by the present thread and read unguarded by `HandRelFull`/blink on the game thread (torn reads); `g_xrRun` cleared but the pace thread never joined; `g_xrCs` initialized only on the XR-3 path; `g_vehOn` a plain bool guard | 335/22082/4219/19098, 524/6963/13012, 17029, 22072, 454 | rare wrong hand pose; teardown races | next: atomics/seqlock in the state dissolve; `g_vehOn` fixed (crash module is idempotent) |
| 5 | Per-frame CPU readback of the 4032x2268 SBS backbuffer: `GetRenderTargetData` + row copy + `UpdateSubresource`, ~36 MB each; `g_pixels` never freed | 20115-20133, 20428-20455 | the dominant present-thread cost | roadmap D4 (shared surface from the fork) |
| 6 | `Direct3DCreate9Ex` returns `E_FAIL` silently when the fork lacks the export | 22877 | confusing failure | next: one log line |
| 7 | `strcpy(bs + 1, dll)` can overflow `full[2*MAX_PATH]` when the manifest dir + relative `library_path` exceed 520 bytes; the manifest read stops at 8191 bytes; `applicationName` copies unbounded | 21354, 21369-21371, 21896, 22651 | crash with a long path | next: bounded copies (the probe added in this session uses `strncpy`) |
| 8 | Five VEHs exist, one live; the live one calls `Log` (heap, varargs, `GetModuleFileNameA`) inside the handler | 5896, 6243, 6499, 6841, 21774 | a crash on a corrupted heap loses the fingerprint | fixed: `core/util/crash.cpp` preformats into static storage and writes with `WriteFile`; the four diagnostic VEHs are legacy |
| 9 | FpsCap: `Sleep(ms - 1.5)` then a hot spin with no `timeBeginPeriod` | 20596-20601 | up to 15 ms overshoot, then a no-op spin | next: waitable timer |
| 10 | The synchronous XR-2 path can stall the render thread up to 1 s in `xrWaitSwapchainImage` | 22204, 22125 | hitches when `[VR] XrQuads=0` | roadmap D3 (one path) |
| 11 | OpenVR init (`VR_InitInternal`, launches SteamVR) runs on the render thread inside Present | 20624 -> 19777 | a multi-second stall at first Present | next: log the duration; move to a thread later |
| 12 | `DLL_PROCESS_DETACH` calls `VR_ShutdownInternal` under the loader lock | 22955 | teardown hang risk | next: move to the PreExit standdown |
| 13 | The kill-mask push is duplicated with different log text | 20354-20372 | noise | next |
| 14 | `capture_dump_*.bmp` was written to the CWD | 20203 | game-derived pixels in the game folder | fixed: dumps go to `<data_dir>\dumps` via the seam |
| 15 | `EnsureConfig` is not thread-safe | 3151 | none today (one caller) | next: call-once |
| 16 | `hkXInputGetState` reports a pad on slot 0 before VR is up | 19079 | by design (the game decides pad-or-not once) | documented |
| 17 | `GetAsyncKeyState` hotkeys polled from 27 sites; F9 contested with the game's quickload | 6395-20881 | two sites can consume one key | structural: `core/input/hotkeys` |
| 18 | `g_menuOpen` starts true and is cleared by script events only | 2109 | "stuck mono" is hard to diagnose | fixed: exposed in `status.json` and the `[game] state:` line |
| 19 | The 32-bit registry view (`WOW6432Node`) is what a 32-bit process reads at `HKLM\SOFTWARE\Khronos\OpenXR\1` | 21335 | correct, but why SteamVR-only rigs fall to OpenVR | documented in ARCHITECTURE |
| 20 | `XrRtTryInit` returns on a failed `xrCreateSession`/swapchain without cleanup; the retry skips creation, so a half-built session persists | 21983-22023 | wedged XR bring-up after one failure | next: teardown on failure (in the `IVrBackend` step) |
| 21 | 165 em dashes in strings and comments (one in the generated ini header); "tell Claude!" strings in user-visible log lines | 2196, 19564, 20098, 20120 | style rule; PowerShell 5.1 mojibake | fixed (swept); the "tell Claude" strings remain verbatim until the capture module is rewritten |
| 22 | `dxvk_vr_view` is resolved (StampFix) but no published patch exports it | 20258 | StampFix is inert on every build made from this repo | documented (fork doc); roadmap D3 |
| 23 | The hand-skin material path was `"%s\vrhands\%s.mtl"`: `\v` is a vertical tab and `\%` is not an escape, so the `.mtl` never opened and hand skins had no materials | 3745 | hand models untextured | **fixed** |
| 24 | Backend auto-detect by process snapshot (VD streamer running AND SteamVR not) | 2990-3023 | Quest over Link/Air Link/Steam Link and every non-VD OpenXR headset fell to OpenVR | **fixed**: capability probe |
| 25 | The MinGW build linked `d3dcompiler` and (through ImGui) `shell32` statically, so the proxy adds two static imports to the game's load order; the MSVC build does the same | build.sh | parity, not a regression; `D3DCOMPILER_47.dll` loads at startup | next: make ImGui's compile go through the runtime-loaded compiler |
| 26 | `hkSetVSConstF` is installed on the hottest D3D9 entry point; its only live consumer is the c5 camera-position capture | 17402, 21267 | per-call overhead | next: gate behind the consumer |
| 27 | `RunConsole` leaks the engine-allocated return string by design (freeing with the wrong allocator would be worse) | 16589 | a few bytes per diagnostic | documented |

Review method: the exploration read every section of the file; the extraction scripts
(`tools/split-source.py --check`) proved the bodies unchanged; MSVC `/W3` found the `.mtl`
escape (item 23). Nothing here was run against the game.
