# Architecture

## Overview

```
Dishonored.exe (UE3 9099, x86, D3D9)
 |-- imports d3d9.dll ............ OUR PROXY (src/, built as d3d9.dll); chains to System32\d3d9.dll
 |     |-- hooks IDirect3D9::CreateDevice and the device's Present, Reset,
 |     |     SetVertexShaderConstantF, SetRenderTarget (core/framework/frame_hooks)
 |     |-- hooks engine code at fixed addresses (UObject::ProcessEvent, the four Blink sites)
 |     |-- hooks the exe's XInput import slots (the virtual gamepad)
 |     |-- own D3D11 device on the adapter the runtime names (core/gfx/d3d11_device):
 |     |     the stereo method's output texture, the F10 overlay, dumps, hand meshes (uncalled)
 |     |-- THE STEREO SEAM (core/gfx/stereo): the active method turns the game's frame into
 |     |     one eye-tagged D3D11 texture per present - mono (rung 1, shipped) | aer | reentry
 |     |-- THE CAMERA SEAM (game/dishonored/camera): rotation, FOV and the per-eye offset,
 |     |     written on the script lane
 |     `-- THE RUNTIME LAYER (core/vr/openxr_runtime, from the BioShock trilogy mod): OpenXR
 |           instance, session, swapchains, pacing, head/hand poses, actions, layers, xrEndFrame;
 |           the static Khronos loader is linked in. Runtime = VDXR (Quest via Virtual Desktop),
 |           any native 32-bit runtime, or dvr_steamvr32.dll (the SteamVR shim, OpenXR-on-OpenVR)
 `-- imports xinput1_3.dll (the real one; our hook lives in the exe's IAT slots)
```

Everything the mod does for the player, in the order a frame sees it
(`core/framework/frame_hooks.h` is the authority for step 3):

1. **Game thread, script lane** - `UObject::ProcessEvent` is detoured (`kProcessEvent`,
   `game/dishonored/ue3/process_event.cpp`). `PeHandler` latches the live pawn, menu and
   cinematic state from named events, writes the HMD pitch/yaw into `ProcessViewRotation`'s
   parms (`ApplyHeadToViewRotation`, head tracking 1:1), runs the FOV lever
   (`FovLeverApply`, its target from `camera::fov_deg`), then the camera seam's script-lane
   pass (`camera::eyetest_script_tick`, `camera::apply_eye_offset` - the per-eye offset for
   the stereo methods that render one eye per tick), the SkelControl hand nodes, the crouch
   collision writes, the console requests, the intro skip.
2. **Game thread, input** - `hkXInputGetState` (IAT slot `kXIGetSlot`) serves a synthesized
   `XINPUT_STATE` composed from the runtime layer's `InputSnapshot` (`core/input/pad_bridge.cpp`);
   rumble comes back out as haptics through the action layer.
3. **The present thread** - `hkPresent` in `core/framework/frame_hooks.cpp`, in this order:
   - `pre_tick` (game side): the command seam poll, `status.json`, the `[game] state:` tick,
     the crash filter re-arm - every present, even with VR disabled;
   - `[VR] FpsCap`, the even-cadence limiter (XR sessions only);
   - `vr::on_present_begin`: bring-up on a cooldown, event pump, `xrWaitFrame` (this is what
     paces the game to the headset), `xrBeginFrame`, the head and view locate, the action sync;
   - `stereo::begin_frame(FrameInput)`: the active method learns the head, fov, IPD and picks
     the eye the NEXT game frame renders (`eye_for_next_frame`);
   - `game_tick` (game side, `game/dishonored/present_tick.cpp`): the head pose into
     `TrackHead` (positional tracking, crouch, lean, roomscale), the hand poses into the pose
     slots, the camera seam's `set_eye/ipd/world_scale` and the eyetest readback, the virtual
     gamepad, frame dumps, the heartbeat, the hotkeys;
   - `stereo::end_frame(FrameDevices) -> FrameOutput`: the method captures the game's frame
     (`core/gfx/capture`: `GetRenderTargetData` -> heap -> `UpdateSubresource`, BGRA), blits
     it into its R8G8B8A8 output (`core/gfx/blit_quad`), draws the F10 overlay into it, and
     hands the texture out with its eye tag (mono: 0);
   - `vr::on_present_end(tex)`: `CopyResource` into the swapchain the tag selects, the layers
     (the head-locked quad at `[Screen] DistanceMeters`/`WidthMeters` for a mono tag, a
     projection layer for eye tags, laser/aim-dot/HUD quads when armed), `xrEndFrame`.
4. **The game's own Present** runs last; the desktop window shows the game's frame as always.

## The stereo ladder

One seam, several methods, compared by measurement (docs/ROADMAP.md S1-S3). Each rung must
prove something the next one builds on:

| Rung | Method | What it proves | Cost | Where |
|---|---|---|---|---|
| 1 | `mono` - the game frame on a head-locked quad, both eyes | the whole path headset-to-eye: capture, D3D11, swapchain, compositor, pacing, head tracking, the gamepad | one readback per present | `core/gfx/mono_screen.cpp`, shipped |
| 2 | `aer` - AlternateEye: each tick renders ONE eye (the camera seam offsets +/- IPD/2), the compositor holds the other eye's last frame | geometric stereo and the eye-offset write point, cheaply; half temporal rate per eye | none on the game | `core/gfx/aer.cpp`, design stub |
| 3 | `reentry` - SequentialReentry: the engine's scene draw is called a second time per tick with the other eye's camera | per-eye native effects at full rate; the big bet (needs the draw root, a gated and guarded second call) | one extra scene draw per tick | `core/gfx/reentry.cpp`, design stub |

`IStereo` (stereo.h): `begin_frame(FrameInput)`, `eye_for_next_frame()`, `end_frame(FrameDevices,
FrameOutput&)`, `on_reset()`, `shutdown()`, `status()`. `FrameOutput` is one texture and one
eye tag per present; the runtime layer's eye-tag ring (`sr_push_eye`) and held-eye machinery
(`current_eye_sign`) exist for rungs 2-3 unchanged from BioShock. `[Stereo] Method=` picks the
rung at load; `stereo <name>` switches live and fails soft; the `stereo: beat` line every 3 s
says which eyes flowed (`L/s R/s mono/s none/s`) with the by-design zeroes named.

## The runtime layer

`core/vr/openxr_runtime.{h,cpp}` and `openxr_input.{h,cpp}` are the BioShock trilogy mod's
files with the namespace renamed and two seams for a D3D9 host, marked `41.0 (Dishonored)`
in the source:

- **The device provider.** BioShock borrowed the game's D3D11 device from its swapchain;
  here the mod provides one (`set_device_provider`), created on the adapter LUID
  `xrGetD3D11GraphicsRequirementsKHR` names (`core/gfx/d3d11_device.cpp`; the author's 39.3
  fix: a device on the wrong GPU succeeds at every call and shows nothing).
- **The frame texture.** `on_present_end(ID3D11Texture2D*)` takes the stereo method's output
  instead of reading a DXGI backbuffer; the swapchains are created from the first texture's
  size and rebuilt (through the queued `on_resize` path) when it changes. The desktop mirror
  and the window HUD composite are inert (the flat window is the game's own D3D9 backbuffer).

Kept verbatim: the pace thread and the `vrpace` seam (thread/detach/feed/sync/spike/simidle),
the session state machine and the deferred teardown, the pose-tag audit, the per-eye FOV and
recommended eye size, the laser/aim-dot quads, the HUD quad with its texture-provider seam
(S3: the wrist HUD comes back through it), the vrrec sim-hand overlay, the `[pair]` probe.
The gameswf HUD-capture surface the layer reads is `core/vr/hud_stub` (every reader answers
"nothing"). The action layer publishes a raw `InputSnapshot` (sticks, triggers, grips, buttons,
clicks, menu; the X+Y menu chord; the both-clicks recenter chord) and located hand poses;
composition into Dishonored's gamepad stays in `core/input/pad_bridge.cpp`; haptics queue from
any thread and apply inside `input_sync` on the present thread.

## Runtime selection

`[VR] Runtime=auto|native|steamvr` (default auto). The static loader reads `XR_RUNTIME_JSON`
first, then the 32-bit `ActiveRuntime` registry key; `[VR] XrRuntimeJson=` sets the variable
for the process when the environment carries none (the simulator through Steam,
`xrsim-launch.ps1 -ViaSteam`). `auto` tries the native runtime and, when no 32-bit runtime
answers, writes a manifest for the shim under `%LOCALAPPDATA%\DishonoredVR\steamvr\` and
points `XR_RUNTIME_JSON` at it; `steamvr` forces the shim; `native` never falls back. The
shim (`src/tools/ovrshim`, ships as `dvr_steamvr32.dll` with Valve's `openvr_api.dll`) is a
complete 32-bit OpenXR runtime over OpenVR: D3D11 swapchains, poses, the action profiles for
Index, Vive, Touch and WMR, logging to `%LOCALAPPDATA%\DishonoredVR\ovrshim.log`.

## The camera seam

`game/dishonored/camera.h`: `set_eye(sign)` (from the active method, every present),
`set_ipd_m`, `set_world_scale` (`[PosTrack] Scale`, 100 uu/m measured), `eye_offset_uu()`,
`set_fov_deg`/`fov_deg` (the lever's target), `rendered_fov_deg` (the 0x53c sensor),
`note_render_pos`/`render_pos` (c5, the draw's camera position), `apply_eye_offset(camObj)`
on the script lane into `[Camera] EyeField`, and the `camera eyetest` instrument that decides
which field the renderer honours (ENGINE_NOTES "The per-eye camera seam: write points").
Rotation and FOV are measured write points; the lateral eye offset is not, and the seam says
so once when a method asks for an eye with no field measured. Positional tracking (lean,
crouch, roomscale) still rides the c0 view-projection patch (`LeanVP` in
`core/framework/vs_const.cpp`) until the write point is known (ROADMAP S1).

## Directory contract

- `src/proxy/` - only `DllMain` (loader-lock safe: paths, clock, log, kill switch, two early
  ini ints, the pad hook) and the nine exports; `Direct3DCreate9` registers the stereo methods,
  loads the config, brings the runtime layer's instance up, installs the frame hooks.
- `src/core/` - knows D3D9/D3D11, OpenXR, Win32, ImGui, files. Never a UObject, never an
  address. `core/framework/frame_hooks` knows no game global: the game side registers
  callbacks.
- `src/game/dishonored/` - everything that knows an address or a UE3 layout. `patterns.h` is
  the ONLY file with a fixed address; `ue3/` is the reflection layer; `camera` is the per-eye
  seam; `present_tick.cpp` is the game side of the frame path; the rest are features.
- `src/legacy/` - retired experiments and one-off diagnostics, compiled only with
  `DVR_WITH_LEGACY`. The live code's calls into them resolve to no-op stand-ins.
- `src/tools/` - the simulated OpenXR runtime, the SteamVR shim and the smoke client; they link
  nothing from the mod.

## The unity build, and how a module leaves it

The original mod was one translation unit with ~1,500 file-scope statics. `tools/split-source.py`
cut it into the module tree without changing a function body: `src/mod/state/NN_*.inc` hold
every non-function item in the original order, `src/mod/fwd.h` declares every function, and
`src/mod/dishonoredvr.cpp` includes state, prototypes, then the module files with a `DVR_CAT`
log category around each.

Real translation units today (`DVR_REAL_SOURCES` in `src/CMakeLists.txt`): `core/util/*`,
`core/hooks/*`, `core/framework/{command,status,frame_hooks}`, `core/gfx/{stereo,capture,
blit_quad,mono_screen,aer,reentry}`, `core/vr/{openxr_runtime,openxr_input,hud_stub}`,
`game/dishonored/camera`. A module leaves the unity build in four steps:

1. Write `module.h` with a `dvr::<ns>` API and, if the old function names are used widely,
   inline wrappers with the old names (`RangeReadable` -> `dvr::mem::range_readable`).
2. Rewrite `module.cpp` to own its state (`static` inside the .cpp, or a struct); where it
   must reach unity globals, take a callback (`frame_hooks` registers `Callbacks` from
   `present_tick.cpp`) rather than a header full of externs.
3. Remove its prototypes from `fwd.h` and its globals from the state chunk; add the header to
   the header block of `dishonoredvr.cpp`; drop the `.cpp` include; add the files to
   `DVR_REAL_SOURCES`.
4. Build both legacy configurations; commit.

Remaining order (each one a session-sized step): `head_track` and `pad_bridge` (S1), the
config table (`core/config`), the game features, and finally dissolving the state chunks into
module-owned structs with atomics for the cross-thread members.

## Threads

| Thread | Runs | Owns |
|---|---|---|
| game thread (script lane) | `PeHandler` and everything it calls (head write, lever, the camera seam's pass, hands, crouch, console); `hkXInputGetState` from the game's input poll | every write into engine memory |
| present thread | `hkPresent` and the whole frame path above: the seam poll, status, `vr::on_present_begin/end`, the stereo method, `TrackHead`, the pad composition, the overlay draw, the eyetest readback | the D3D9 capture, the D3D11 device, EVERY OpenXR runtime call (the 38.10 race: VDXR trashed its heap when two threads called in) |
| XR pace thread | `xrWaitFrame` by request from the present thread (`vrpace thread on`), nothing else | nothing; a request/response worker |
| window thread | the game window's subclass (keep-alive, ImGui input) | window messages |

Cross-thread state is plain globals guarded by convention (`g_xrOn`, `g_devPose`...), plus
the atomics inside the runtime layer and the snapshot lock in the action layer. The
state-dissolve step makes the rest atomics.

## Config

`dishonored_vr.ini` next to the exe (`core/config/config.cpp`: `LoadConfig`,
`WriteDefaultIni`, `OverlaySaveDefaults`; `[Meta] Version=10` since 41.0, an older file is
rewritten with the defaults). The keys the new render reads: `[Stereo] Method`, `[Camera]
EyeField`, `[Screen] DistanceMeters/WidthMeters/FovLever/KeepAliveUnfocused`, `[VR]
Runtime/XrRuntimeJson/XrHaptics/FpsCap`, `[PosTrack] *`, `[HeadTrack] *`, `[Mode]
GamepadOnly`. `tests/golden/dishonored_vr.ini` is generated from `WriteDefaultIni` by
`tools/ini-golden.py`; a change to the literal without a regenerated golden fails `--check`.
The table-driven config module is still the plan (CODE_REVIEW item 1).

## Files the mod writes

Next to the exe (user-facing): `dishonored_vr.ini`, `dishonored_vr.log`, `dishonored_vr.prev.log`,
`dishonored_vr_crash.txt`; read there: `disable_vr.txt`, `vrhands\*.obj`. In
`%LOCALAPPDATA%\DishonoredVR\`: `command.txt`, `ack.txt`, `status.json`, `dumps\`, `xrsim\`,
`steamvr\dvr_steamvr32.json` (the shim manifest), `ovrshim.log`, `pacetrace.log` (`vrpace
trace`). `core/util/paths.h` is the one place that knows this.

## Known costs

- The per-present CPU readback of the game window (`GetRenderTargetData` + row copy +
  `UpdateSubresource`; 8 MB each way at 1080p). Structural until S1 replaces it with a D3D9Ex
  shared surface opened on the D3D11 side.
- `xrWaitFrame` runs inline on the present thread by default and paces the game to the
  headset; `vrpace thread on` moves it to the pace thread (the BioShock session-34 fix).
- `hkSetVSConstF` is on the hottest D3D9 entry point: the c5 camera capture, the c0 lean patch
  and the (uncalled) hand drives live there.

## Decision log

### 2026-09-02 - session 5 (native stereo foundation)

- **The DXVK fork is removed, DXVK included.** Four headset sessions showed the wide
  side-by-side design cannot be tuned: render size, FOV lever and frame aspect are one
  coupled setting, injected display modes crop, the world sits at half the menu's sampling
  density. The game renders natively through D3D9; stereo is rebuilt on a per-eye camera
  seam. Git history keeps the fork and its tags (`dxvk-base`, `dxvk-m8.2-shipped`,
  `dxvk-m8.4`, `dxvk-shipped`); nothing moved to `src/legacy` this time (user's call).
- **OpenXR only, with the SteamVR shim.** The OpenVR backend, the backend probe and the
  mod's own loader negotiation, pace thread and action layer are removed; one runtime
  layer serves every headset, and SteamVR rigs go through `dvr_steamvr32.dll` (BioShock's
  ovrshim). Two backends behind one pipeline were the reason the Quest path never converged.
- **The BioShock runtime layer is adopted verbatim behind two seams** (the device provider
  and the frame texture), with its gameswf HUD-capture dependency stubbed rather than cut
  out: a 5k-line proven layer whose fixes should keep porting between the projects. The
  static Khronos loader is linked into the proxy (no loader DLL ships; API layers are
  64-bit-only in practice and lost nothing).
- **Stereo is a seam with named methods, not a switch.** `core/gfx/stereo.h` registers
  methods by name; a method that refuses leaves the previous one running; the beat line
  names the by-design zeroes. Two developers build rungs 2 and 3 on the same foundation
  and S3 compares them - the ladder from the BioShock project, made explicit here.
- **The frame path's ORDER lives in a real module** (`core/framework/frame_hooks`) with the
  game side behind callbacks (`present_tick.cpp`); the 38.92 present-hook bodies moved
  verbatim. `head_track` and `pad_bridge` stay in the unity build for now: their global
  fan-out is the D1-era refactor's job, and converting them was not needed for any S0
  deliverable (ROADMAP S1 lists it).
- **Positional tracking stays on the c0 `LeanVP` patch** until `camera eyetest` names the
  eye-offset write point; the rotation write and the FOV lever are the measured seams,
  the lateral offset is the hypothesis the instrument tests (ENGINE_NOTES).
- **Hand meshes stay compiled but uncalled; the wrist HUD panel and the reticle are
  removed** (user's instruction). Both return on the winning method through the runtime
  layer's HUD quad and laser/aim-dot quads (S3).
- **The F10 overlay draws into the method's output texture** (the mono screen shows it),
  through a callback the game side registers - ImGui only from the overlay's own draw.
- **Version 41.0.0, `[Meta] Version=10`.** The render path is new and every removed key
  would otherwise linger in users' inis (RELEASE_NOTES "Upgrading" lists them).

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
