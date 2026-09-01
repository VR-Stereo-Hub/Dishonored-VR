# Third-party notices

| Component | License | Where | Notes |
|---|---|---|---|
| Dishonored-VR (GingasVR) | zlib/libpng (see LICENSE) | the whole repo | The original mod (proxy build 38.92) and the DXVK fork. Continued here with the author's permission (2026-09). |
| DXVK 3.0.2 (Philip Rebohle and contributors) | zlib/libpng | `dxvk/` | Forked; `dxvk/DISHONORED-FORK.md` lists every change. |
| Dear ImGui (docking branch) | MIT | `third_party/imgui` (submodule) | The F10 settings overlay. |
| OpenXR SDK (Khronos) | Apache-2.0 | `third_party/OpenXR-SDK` (submodule) | Headers only in the mod (it negotiates the runtime itself); the static loader is linked only into `xr_hello32`. |
| OpenVR SDK 2.15.6 (Valve) | BSD-3-Clause | `third_party/openvr_headers` | Vendored headers + the 32-bit `openvr_api.dll`; SHA-256 in `PROVENANCE.txt`, verified by `tools\install.ps1` and `package.ps1`. |
| bioshock-trilogy-vr (the user's own project) | MIT | `src/tools/xrsim`, `src/tools/xr_hello32`, `tools/*.ps1`, doc templates | The simulated OpenXR runtime, the smoke client, the harness scripts and the documentation shape. |

Reference only (no code copied): the Helix Mod 3D Vision fix for Dishonored (which shaders break
in stereo), the Mirror's Edge VR mod (D3D9 draw-duplication stereo), REFramework (MIT, may be
adapted with attribution), UEVR (all rights reserved; concepts only).
