# Third-party notices

| Component | License | Where | Notes |
|---|---|---|---|
| Dishonored-VR (GingasVR) | zlib/libpng (see LICENSE) | the whole repo | The original mod (proxy build 38.92). Continued here with the author's permission (2026-09). Their DXVK fork (DXVK 3.0.2, zlib/libpng) was removed in 41.0; git history keeps it under the `dxvk-*` tags. |
| Dear ImGui (docking branch) | MIT | `third_party/imgui` (submodule) | The F10 settings overlay. |
| OpenXR SDK (Khronos) | Apache-2.0 | `third_party/OpenXR-SDK` (submodule) | The static loader is linked into `d3d9.dll` (41.0) and `xr_hello32`; no loader DLL ships. |
| OpenVR SDK 2.15.6 (Valve) | BSD-3-Clause | `third_party/openvr_headers` | Vendored headers + the 32-bit `openvr_api.dll`, used by the SteamVR shim (`src/tools/ovrshim/`, ships as `dvr_steamvr32.dll`); SHA-256 in `PROVENANCE.txt`, verified by `tools\install.ps1` and `package.ps1`. |
| bioshock-trilogy-vr (the user's own project) | MIT | `src/core/vr` (the OpenXR runtime layer and action layer), `src/core/util/xr_math.h`, `src/tools/ovrshim`, `src/tools/xrsim`, `src/tools/xr_hello32`, `tools/*.ps1`, doc templates | The VR runtime layer (41.0), the SteamVR shim, the simulated OpenXR runtime, the smoke client, the harness scripts and the documentation shape. |

Adapted code:

- **Bioshock-Remastered-VR by BioVRDev** (https://github.com/BioVRDev/Bioshock-Remastered-VR,
  no license file published) - the SteamVR shim in `src/tools/ovrshim/` is adapted (via the
  bioshock-trilogy-vr project) from their `OpenXRShim/` module (OpenXR-on-OpenVR). Copied and
  adapted with the author's explicit permission, given 2026-08-13 to the bioshock-trilogy-vr
  project (recorded in that repository's `docs/RESEARCH.md`, "BioVRDev/Bioshock-Remastered-VR
  analysis"). Adapted files carry an attribution comment.

Reference only (no code copied): the Helix Mod 3D Vision fix for Dishonored (which shaders break
in stereo), the Mirror's Edge VR mod (D3D9 draw-duplication stereo), REFramework (MIT, may be
adapted with attribution), UEVR (all rights reserved; concepts only).
