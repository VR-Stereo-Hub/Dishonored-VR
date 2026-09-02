# Architecture

## Overview

```
Dishonored.exe (UE3 9099, x86, D3D9)
 |-- imports d3d9.dll ............ OUR PROXY (src/, built as d3d9.dll)
 |     |-- chains to dxvk_d3d9.dll ... THE FORK (dxvk/): D3D9 -> Vulkan, per-eye stereo splice
 |     |     (falls back to System32\d3d9.dll when the fork is absent)
 |     |-- hooks IDirect3D9 / IDirect3DDevice9 vtable slots (CreateDevice, Present, Reset,
 |     |     SetVertexShaderConstantF, SetRenderTarget, adapter mode enumeration)
 |     |-- hooks engine code at fixed addresses (UObject::ProcessEvent, the four Blink sites)
 |     |-- hooks the exe's XInput and user32 import slots (virtual gamepad, render-size hold)
 |     |-- own D3D11 device: eye render targets, projected screen quads, OBJ hand meshes,
 |     |     wrist HUD panel, ImGui overlay, frame dumps
 |     `-- one of two VR backends:
 |           OpenVR  (openvr_api.dll next to the exe -> SteamVR; WaitGetPoses / Submit)
 |           OpenXR  (loader negotiation, no openxr_loader.dll; a detached pace thread owns
 |                    every runtime call; XR_KHR_D3D11_enable swapchains)
 `-- imports xinput1_3.dll (the real one; our hook lives in the exe's IAT slots)
```

Everything the mod does for the player, in the order a frame sees it:

1. **Game thread, script lane** - `UObject::ProcessEvent` is detoured (`kProcessEvent`,
   `game/dishonored/ue3/process_event.cpp`). `PeHandler` latches the live pawn, menu and
   cinematic state from named events, writes the HMD pitch/yaw into `ProcessViewRotation`'s
   parms (`ApplyHeadToViewRotation`, head tracking 1:1), drives the SkelControl hand nodes,
   runs the FOV lever, the crouch collision writes, the console requests (`RunConsole`), the
   intro skip.
2. **Game thread, input** - `hkXInputGetState` (IAT slot `kXIGetSlot`) serves a synthesized
   `XINPUT_STATE` composed from the VR controllers (`core/input/pad_bridge.cpp`); rumble comes
   back out as haptics.
3. **Render thread, Present** - `hkPresent` (`core/framework/frame_hooks.cpp`) polls the
   command seam, writes `status.json`, then `VRFrame`: reads the fork's exports (splice count,
   live projection, HUD texture), captures the SBS backbuffer (`GetRenderTargetData`, a CPU
   readback, see "Known costs"), uploads it to D3D11, runs `TrackHead` (6DoF, crouch, lean,
   roomscale, recenter), `MotionAimTick`, `MeleeTick`, `BlinkTick` and the hand ticks, then
   `RenderEyesAndSubmit`: per eye, the projected screen quad + hands + wrist HUD + reticle +
   overlay into the eye RT, then the backend submit (OpenVR `Submit`, or `XrRtPublish` to the
   pace thread).
4. **The fork** (`dxvk/src/d3d9/d3d9_device.cpp`) replays qualifying draws once per eye into
   the halves of the 4032x2268 backbuffer with a per-eye shifted view-projection, keeps
   right-eye twins of every full-size render target, fixes the screen-space passes (light
   shafts, shadows, reflections, additive lights), and redirects UI draws to an exported HUD
   texture. Contract: the `dxvk_vr_*` data exports (`dxvk/DISHONORED-FORK.md`).

## Directory contract

- `src/proxy/` - only `DllMain` (loader-lock safe: paths, clock, log, kill switch, two early
  ini ints, the two hook families that must precede the first D3D call) and the nine exports.
- `src/core/` - knows D3D9/D3D11, OpenVR/OpenXR, Win32, ImGui, files. Never a UObject, never
  an address.
- `src/game/dishonored/` - everything that knows an address or a UE3 layout. `patterns.h` is
  the ONLY file with a fixed address; `ue3/` is the reflection layer (GObjects/GNames walk,
  `FindPropOffset`, ProcessEvent-by-name); the rest are features.
- `src/legacy/` - retired experiments and one-off diagnostics, compiled only with
  `DVR_WITH_LEGACY`. The live code's calls into them resolve to no-op stand-ins
  (`legacy_stubs.inc`). Their globals stay in `src/mod/state` (harmless, keeps ini keys inert
  rather than unknown).
- `src/tools/` - the simulated OpenXR runtime and the smoke client; link nothing from the mod.

## The unity build, and how a module leaves it

The original mod was one translation unit with ~1,500 file-scope statics. `tools/split-source.py`
cut it into the module tree WITHOUT changing a function body: `src/mod/state/NN_*.inc` hold
every non-function item (includes, types, globals, forward declarations) in the original order
(order matters: initializers and inline uses), `src/mod/fwd.h` declares every function so the
bodies can live in any file, and `src/mod/dishonoredvr.cpp` includes state, prototypes, then the
module files with a `DVR_CAT` log category around each. Same statics, same linkage, same
program. `tools/split-source.py --check` re-parses the tree and compares every body with the
original commit.

A module becomes a real translation unit in four steps, as `core/util/*`, `core/hooks/*`,
`core/framework/{command,status}` already did:

1. Write `module.h` with a `dvr::<ns>` API and, if the old function names are used widely,
   inline wrappers with the old names (`RangeReadable` -> `dvr::mem::range_readable`).
2. Rewrite `module.cpp` to own its state (`static` inside the .cpp, or a struct).
3. Remove its prototypes from `fwd.h` and its globals from the state chunk; add the header to
   the header block of `dishonoredvr.cpp`; drop the `.cpp` include; add the files to
   `DVR_REAL_SOURCES` in `src/CMakeLists.txt`.
4. Build both legacy configurations; run `--check`; commit.

Order of the remaining conversions (each one is a session-sized step): config table
(`core/config`), the VR backends behind `IVrBackend` (`core/vr`), the present pipeline
(`core/gfx`), the game features, and finally dissolving the state chunks into `core/state.h`
structs with atomics for the cross-thread members.

## Threads

| Thread | Runs | Owns |
|---|---|---|
| game thread (script lane) | `PeHandler` and everything it calls; `hkXInputGetState` from the game's input poll | every write into engine memory (rotators, SkelControls, collision, console) |
| render thread (present lane) | `hkPresent`, `VRFrame`, `RenderEyesAndSubmit`, `TrackHead`, the seam, status, the overlay draw | the D3D9 capture, the D3D11 pipeline, the OpenVR calls |
| XR pace thread | `XrPaceThread`: events, `xrWaitFrame`/`Begin`/`EndFrame`, input sync, haptics, swapchain copies | every OpenXR runtime call (VDXR raced when the render thread called into it) |
| window thread | `OverlayWndProc` subclass (keep-alive, WM_SIZE, MINMAXINFO) | window messages |

Cross-thread state today is plain globals (`g_xrOn`, `g_devPose`, `g_vrReady`...), guarded by
convention and, for the XR publish, `g_xrCs`. CODE_REVIEW lists the torn-read risks; the
state-dissolve step makes them atomics.

## The VR backends (today, and the target)

Today both backends share the present pipeline and switch on globals: `g_xrBackend` (chosen
once by `core/vr/backend_probe.cpp`), `g_xrOn` (session live), `g_sys/g_comp` (OpenVR fn
tables). The OpenVR path: `TryInitVR` -> `WaitGetPoses` in `VRFrame` -> `Submit` per eye
(with `VRTextureWithPose_t` on Quest-family HMDs). The OpenXR path: `XrRtTryInit` (instance,
system, session, swapchains) -> the pace thread publishes poses and consumes eye textures
(`XrRtPublish`), or the synchronous XR-2 path when `[VR] XrQuads=0`.

Target (`IVrBackend`, planned): `init/shutdown/state`, `acquireFrame(FrameInput&)` (head and
hand poses, per-eye FOV tangents, IPD, input snapshot), `submit(EyeSubmit)` (eye textures +
the rendered pose stamp), `haptic`, `wantsFlatEye`. The present pipeline keeps capture, quads,
hands, HUD, overlay; the backend keeps pose source and delivery. The pace thread stays private
to the OpenXR backend.

## Backend selection

env `DISHONORED_VR_BACKEND` > ini `[VR] Backend` > auto. Auto asks the runtimes: negotiate a
32-bit OpenXR runtime (`XR_RUNTIME_JSON` > `[VR] XrRuntimeJson` > the WOW6432Node
`ActiveRuntime`), create an instance, `xrGetSystem(HMD)` -> OpenXR; else `openvr_api.dll` +
`VR_IsRuntimeInstalled` + `VR_IsHmdPresent` -> OpenVR; else OpenVR anyway (the historical
default, `TryInitVR` keeps retrying SteamVR). SteamVR registers no 32-bit OpenXR runtime, so a
SteamVR rig lands on OpenVR; a VDXR rig lands on OpenXR even with SteamVR running.

## Config

`dishonored_vr.ini` next to the exe, read by `LoadConfig` (`core/config/config.cpp`, 757 lines,
~330 keys in 26 sections) and written by `WriteDefaultIni` (16 sections) and
`OverlaySaveDefaults`. The mismatch (`[VR]`, `[Hands]`, `[Blink]`, `[Hud]`, `[Overlay]`,
`[VRHands]`... never appear in a fresh ini) is CODE_REVIEW item 1; the fix is the table-driven
module (one `CfgEntry` per key: section, key, type, pointer, default, clamp, flags, help;
`load`, `write_defaults`, `write_missing`, `save_live`) with `tests/golden/dishonored_vr.ini` as
the regression. `[Log] Level/Cats` are read in `EnsureConfig` today.

## Files the mod writes

Next to the exe (user-facing, users depend on the locations): `dishonored_vr.ini`,
`dishonored_vr.log`, `dishonored_vr.prev.log`, `dishonored_vr_crash.txt`; read there:
`disable_vr.txt`, `dxvk_stereo.txt`, `dxvk_framemap.txt`, `vrhands\*.obj`. In
`%LOCALAPPDATA%\DishonoredVR\` (harness, bulk): `command.txt`, `ack.txt`, `status.json`,
`dumps\`, `xrsim\`. `core/util/paths.h` is the one place that knows this.

## Known costs

- The per-frame CPU readback of the 4032x2268 SBS backbuffer (`GetRenderTargetData` +
  row copy + `UpdateSubresource`, ~36 MB each way). Structural; D4 replaces it with a shared
  surface exported by the fork.
- The OpenVR init (`VR_InitInternal`) runs on the render thread inside Present.
- `hkSetVSConstF` is on the hottest D3D9 entry point; the only live consumer is a camera
  position capture.

## Decision log

### 2026-09-02 - session 5 (native stereo foundation)

- **The DXVK fork is removed, DXVK included.** Four headset sessions showed the wide
  side-by-side design cannot be tuned: render size, FOV lever and frame aspect are one
  coupled setting, injected display modes crop, the world sits at half the menu's sampling
  density. The game renders natively through D3D9; stereo is rebuilt on a per-eye camera
  seam. Git history keeps the fork and its tags (`dxvk-base`, `dxvk-m8.2-shipped`,
  `dxvk-m8.4`, `dxvk-shipped`); nothing moved to `src/legacy` this time (user's call).

### 2026-09-02 - session 1

- **Keep the DXVK fork as the stereo path; restore its source in-repo.** A proxy-level stereo
  (Mirror's Edge VR style draw duplication) is feasible but the 52 patches are mostly the
  artifact fixes on top of stereo (twins, shafts, shadows, reflections, HUD redirect), all of
  which would need re-deriving with no headset. The fork touches one file, so restoring it
  makes the stereo code reviewable. Proxy-level stereo is a backlog evaluation.
- **CMake + MSVC** replaces the MinGW cross-compile: one toolchain with the simulator, PDBs, VS
  on the dev PC. Cost: the four naked stubs rewritten in Intel syntax; `d3d9.def` for the
  undecorated exports.
- **Unity split first, real modules incrementally.** A 23k-line file with 1,500 statics cannot
  be turned into headers in one step without behavior risk and the game is not installed to
  test. The unity build keeps the program identical while the tree becomes navigable; each
  module leaves the unity list when it gets a real header.
- **Both backends kept.** The OpenVR path is the headset-accepted baseline; the OpenXR path is
  the one to fix (XR_HANDOFF). The BioShock mod's OpenXR-only + SteamVR shim architecture is
  the alternative if the native OpenVR code becomes a burden.
- **Retired code compiled out, not deleted** (user's call): `src/legacy` behind
  `DVR_WITH_LEGACY`; helpers the live path uses were moved out.
- **OpenXR without the Khronos loader.** The mod's own negotiation reads env > ini > registry
  exactly like the loader, so `XR_RUNTIME_JSON` selects the simulator unchanged and nothing
  extra ships. API layers are the only loss and are 64-bit-only in practice.
- **Backend auto-detect by capability probe, not process names.** The process snapshot picked
  OpenXR only for Virtual Desktop with SteamVR absent, which is why Quest over Link/Air
  Link/Steam Link failed. Asking `xrGetSystem` and `VR_IsHmdPresent` answers the real question.
- **Data-dir split.** User-facing files stay next to the exe (users, README, the fork's relative
  markers); harness and bulk files go to `%LOCALAPPDATA%\DishonoredVR` (may be read-only under
  Program Files; game-derived captures must never sit where a user zips a bug report).
- **No MinHook.** Every hook is a vtable slot, an IAT slot or a byte-verified 5-byte detour with
  a hand-built stub; `core/hooks` keeps them. Add MinHook only when an arbitrary function hook
  is needed.
- **Version 40.0.0** continues the author's build-tag line so user reports stay orderable;
  `dvr_version.h` is generated from CMake + `git describe`. (First chosen as 39.0.0; the
  author's handoff showed their private line already reached 39.4, so 40 avoids two
  different "39.x" binaries in the wild.)
- **The author's handoff is a first-class source.** `docs/dishonored/HANDOFF-GINGASVR.md`
  (2026-09-01, their build 39.4) records fixes our 38.92 base lacks (calibration bank by
  asset name, the pitch kept/discarded loop, the D3D11 adapter LUID fix, the menu ghost
  quadrant), a dead-ends table and process rules. Their 39.x line was rebased onto 38.72
  because 38.73-38.92 stacked unverified changes; our base is 38.92, so D1 parity compares
  against BOTH 38.72-era behavior and the 39.x fixes, not against 38.92 alone.
