# The Dishonored VR DXVK fork (dxvk_d3d9.dll)

This directory is DXVK 3.0.2 (upstream commit 3a4c6fa3, tag `dxvk-base`) plus the
Dishonored VR stereo work, kept as ordinary git commits on top of it. The mod's
proxy (`d3d9.dll`, built from `src/`) loads the built fork as `dxvk_d3d9.dll`
from the game folder and talks to it only through the exported data symbols
listed below. Everything the fork adds lives in `src/d3d9/d3d9_device.cpp`
(plus one field in `src/d3d9/d3d9_shader.h` from M7.3).

## What the fork does for the game

- **Stereo splice**: qualifying draws are replayed once per eye into the halves
  of the 4032x2268 backbuffer with a per-eye shifted view-projection
  (`dxvk_vr_sep`, `dxvk_vr_conv`), so the player gets real binocular depth.
- **Right-eye twins** (M4.x): every full-size render target and depth buffer
  gets a right-eye twin, clears are mirrored into it, each eye renders at full
  size.
- **Frame-sequential mode** (M5.1, `seq=1` in `dxvk_stereo.txt`): one whole eye
  per frame, `dxvk_vr_eyesign` tells the proxy which.
- **Per-eye fixes** for the screen-space passes that break in stereo: light
  shafts (M5.3/M5.4), modulated shadows and shadow masks (M5.6-M6.4,
  `dxvk_vr_shadowfix`), the mirrored-camera planar reflection (M6.6,
  `dxvk_vr_reflect`), additive light passes and their attenuation uv (M6.8,
  M7.0, `dxvk_vr_uvfix`), the quarter-res light passes (M8.1/M8.2).
- **Wrist HUD redirect** (M6.0/M6.1): UI draws go to an exported HUD texture
  (`dxvk_vr_hudtex`) that the proxy shows on the player's wrist.
- **Diagnostics**: marker-armed frame maps (`dxvk_framemap.txt`), on-demand
  frame dumps (`dxvk_vr_dumpreq`), a live draw-class kill mask
  (`dxvk_vr_killmask`, the artifact bisector).

## Marker files (read by the fork with a relative fopen, so CWD = game exe dir)

| File | Effect |
|---|---|
| `dxvk_stereo.txt` | present = side-by-side stereo on; containing `seq=1` = frame-sequential |
| `dxvk_framemap.txt` | present = full-frame map dumps at frames 1200/6000/12000 (diagnostic) |

## Export contract

The proxy resolves these by `GetProcAddress` (see `src/core/vr/dxvk_bridge.cpp`).
They are `extern "C" __declspec(dllexport) volatile` data symbols, so no .def
entry is needed. Changing any of them means changing the proxy in the same
commit and updating this table.

| Export | Type | Proxy use |
|---|---|---|
| `dxvk_vr_seq` | uint32 | frame-sequential mode flag |
| `dxvk_vr_eyesign` | float | which eye the last seq frame was |
| `dxvk_vr_splices` | uint32 | splice count this frame (0 = mono scene, both eyes get the full frame) |
| `dxvk_vr_proj` | float[2] | live projection scales (FOV-true screen sizing) |
| `dxvk_vr_sep` | float | stereo separation, written by the proxy from IPD x world scale |
| `dxvk_vr_conv` | float | convergence distance |
| `dxvk_vr_dumpreq` | uint32 | request a frame dump (Scroll Lock) |
| `dxvk_vr_mark` | float[8] | Blink marker locator |
| `dxvk_vr_markkill` | uint32 | Blink marker draw isolation (F8) |
| `dxvk_vr_uvfix` | uint32 | per-eye screen-uv fix (Pause) |
| `dxvk_vr_hudwrist` | uint32 | HUD redirect on/off |
| `dxvk_vr_hudtex` | IDirect3DTexture9* | the redirected HUD texture |
| `dxvk_vr_shadowfix` | uint32 | shadow splice mode |
| `dxvk_vr_reflect` | uint32 | reflection pass kill switch |
| `dxvk_vr_killmask` | uint32 | draw-class kill mask |
| `dxvk_vr_view` | float[] | **NOT IN THIS TREE.** Proxy 38.84 (`[VR] StampFix`) resolves it and reads the VP w-row for the rendered pitch. The published patch series never had it (it was "p53" in the author's private tree). The proxy tolerates the NULL; StampFix stays inert until this export is re-derived. |

Exported but unused by proxy 38.92: `dxvk_vr_twin`, `dxvk_vr_twinrender`,
`dxvk_vr_showtwin`, `dxvk_vr_shaftfix`, `dxvk_vr_lightsplit`,
`dxvk_vr_lightswap`, `dxvk_vr_markkilled`.

`tools/dxvk-exports-check.ps1` verifies the 15 required names against
`tests/golden/dxvk-vr-exports.txt` and reports `dxvk_vr_view` as optional.

## Shipped state and tags

| Tag | Content |
|---|---|
| `dxvk-base` | upstream DXVK 3.0.2 (3a4c6fa3) |
| `dxvk-m8.2-shipped` | after patch 0049 (M8.2), `d3d9_device.cpp` blob `b32c4799` - the state the release binary was built from |
| `dxvk-m8.4` | after patch 0052 (M8.2 + M8.4; 0052 reverts M8.3) |
| `dxvk-shipped` | `dxvk-m8.4` plus a revert of M8.4, blob `b32c4799` again - what `HEAD` builds |

The proxy (since its build 38.55) clears the fork's HUD texture itself right
after reading it, which is why M8.4's PresentEx-time clear is reverted: the two
clears together were a suspect for the chain artifact. Re-evaluating M8.4 is a
roadmap item, not a default.

## Patch to commit map

The original release shipped the fork as 52 `.patch` files. They were applied
in order as the commits `dxvk-base..dxvk-m8.4` (author GingasVR, original
subjects). `git log --oneline dxvk-base..dxvk-m8.4` reproduces the table; the
patch files can be regenerated with
`git format-patch --relative=dxvk/ -o fork-patches dxvk-base..dxvk-m8.4 -- dxvk/`.

| # | Milestone | Subject |
|---|---|---|
| 0001 | M2 | frame-map instrumentation (marker-armed full-frame dumps) |
| 0002 | M3 | stereo splice v1 (marker-armed per-eye draw replay) |
| 0003 | M3.1 | splice-count export, mirrored-VP skip, UP-path instrumentation |
| 0004 | M3.2 | world-quad splice via c6 identity test, UI both-eyes duplication |
| 0005 | M3.3 | depth-test state replaces c6 heuristic for splice/UI gating |
| 0006 | M3.4 | revert to proven M3.1 draw behavior; instrument depth state |
| 0007 | M3.5 | measured gates - splice depth-tested small quads, dup non-RT UP draws |
| 0008 | M3.6 | export live projection scales for FOV-true presentation |
| 0009 | M3.7 | live-writable stereo separation/convergence exports |
| 0010 | M3.8 | on-demand frame dump + per-draw splice verdict |
| 0011 | M3.9 | verdict-log the UP paths too, on fmActive and uncapped |
| 0012 | M3.10 | splice world-space UP effects - the fire fix |
| 0013 | M3.11 | splice the Blink marker; give DrawPrimitive a splice at all |
| 0014 | M3.12 | dump shader constants for the marker-shaped draw |
| 0015 | M4.1 | render-target identity + depth bindings in the frame dump |
| 0016 | M4.2 | drop the depth-binding log site added in M4.1 |
| 0017 | M4.3 | allocate a right-eye twin for every full-size render target |
| 0018 | M4.3b | switch twin allocation from the dxvk_stereo.txt marker |
| 0019 | M4.4 | twin render target TEXTURES, not surfaces |
| 0020 | M4.5 | never log from a static initialiser |
| 0021 | M4.6 | twin only viewport-derived targets |
| 0022 | M4.7 | twin the depth buffers too |
| 0023 | M4.8 | mirror Clear into the colour and depth twins |
| 0024 | M4.9 | render each eye at FULL size into the twins, plus a desktop preview |
| 0025 | M5.1 | frame-sequential per-eye - one WHOLE eye per frame, full resolution |
| 0026 | M5.2 | dump the mono post chain's pixel shaders |
| 0027 | M5.3 | light shafts spliced per eye, against NAMED registers |
| 0028 | M5.4 | shaft splice clips with the scissor, not the viewport |
| 0029 | M5.5 | full pixel-shader census in the frame map |
| 0030 | M6.0 | wrist HUD - redirect UI (kUpDup) draws to an exported HUD texture |
| 0031 | M6.1 | wrist-HUD redirect census + hudskip exclusion list |
| 0032 | M5.6 | per-eye modulated-shadow projection, derived from the census |
| 0033 | M5.7 | splice the shadow frustum in the UP path where it actually draws |
| 0034 | M5.8 | recognize the shadow-projection FAMILY by constant-table content |
| 0035 | M6.2 | shadow-splice census (ps + blend/z state, once per launch) |
| 0036 | M6.3 | recognize shadow-ONLY variants (ScreenToShadowMatrix without ScreenToWorld) |
| 0037 | M6.4 | shadow MASK passes join the world splice |
| 0038 | M6.5 | shadowfix isolation modes (1..4) + uv-remap evidence log + shadow VS bytecode capture |
| 0039 | M6.6 | dxvk_vr_reflect kill-switch for the mirrored-camera planar reflection pass |
| 0040 | M6.7 | live draw-class kill mask - the artifact bisector |
| 0041 | M6.8 | per-shader ScreenPositionScaleBias register (CTAB) for the uv remap |
| 0042 | M6.9 | light-pass probe - save additive light shader bytecode at the draw |
| 0043 | M7.0 | shear the PS ViewProjectionMatrix per eye (light passes re-project in the pixel shader) |
| 0044 | M7.1 | reflect=0 also clears the reflection region per frame |
| 0045 | M7.2 | raise shader id map 2048 to 8192 |
| 0046 | M7.3 | per-shader stereo metadata ON the shader object (touches d3d9_shader.h) |
| 0047 | M8.0 | VPM shear gated to additive draws only + vpm reg bounds guard |
| 0048 | M8.1 | splice the det<0 quarter-res light pass per-eye within its region |
| 0049 | M8.2 | det>0 quarter-region pass joins the M8.1 splice - **shipped** |
| 0050 | M8.3 | accept the quarter-region SPSB signature and make the per-eye uv remap generic (reverted by 0052) |
| 0051 | M8.4 | clear the HUD RT at PresentEx when no redirected draw arrived (reverted on `dxvk-shipped`) |
| 0052 | - | Revert M8.3 |

## Building the fork

`tools\build-dxvk.ps1` from the repo root. Requirements on a Windows box:
Visual Studio 2022 with the C++ workload, Python 3 with `pip install meson ninja`,
and `glslangValidator.exe` on PATH (from the Vulkan SDK). DXVK's `meson.build`
supports MSVC (it is how upstream CI builds x86 on Windows); AVX must stay off.
The original binary was produced with MinGW-w64 from Linux using
`build-win32.txt` as the meson cross file, which remains the reference
toolchain. Output: `build\dxvk\dxvk_d3d9.dll` (the meson target is `d3d9`,
renamed on copy). The DXVK submodules under `dxvk/include` and
`dxvk/subprojects` must be checked out (`git submodule update --init --recursive`).

Rules: patch the fork only here, as commits; keep this file's export table and
patch map in the same commit as any fork change; never log from a static
initialiser (M4.5 lesson).
