# Dishonored VR DLSS helper - notices

`dvr_dlss_host64.exe` is the 64-bit helper that runs NVIDIA DLSS/DLAA or AMD FSR for the Dishonored VR mod.
The game and the mod are 32-bit; NVIDIA's DLSS runtime exists only as 64-bit, so the mod
starts this helper and shares each eye image with it.

## NVIDIA DLSS

`nvngx_dlss.dll` (version 310.7.0.0) is NVIDIA's DLSS runtime from the NVIDIA DLSS SDK
(https://github.com/NVIDIA/DLSS, tag v310.7.0), redistributed under NVIDIA's license, which
is included here as `NVIDIA-DLSS-LICENSE.txt`.

This software contains source code provided by NVIDIA Corporation.

This mod is not sponsored or endorsed by NVIDIA. DLSS needs an NVIDIA RTX GPU.

## AMD FidelityFX (FSR)

`amd_fidelityfx_loader_dx12.dll` (2.3.0.2740) and `amd_fidelityfx_upscaler_dx12.dll` (4.1.1.2740:
FSR 4 on the AMD GPUs that support it, FSR 3.1 on the others) are AMD's signed prebuilt FidelityFX
API DLLs from the FidelityFX SDK (https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK, tag
v2.3.0), MIT-licensed; the license is included here as `AMD-FIDELITYFX-LICENSE.md`.

This mod is not sponsored or endorsed by AMD. FSR runs on any D3D12 GPU.

## Design credit

The approach (NGX in a 64-bit helper process, textures and fences shared from the 32-bit game
by NT handle, one DLSS history per eye) follows three MIT-licensed projects:

- the BioShock VR DLSS/DLAA community fork, https://github.com/Beren5556/BioShock-VR-DLSS-DLAA
- DLSS5-Feeder, https://github.com/jlrouzies-fr/DLSS5-Feeder, which that fork's helper derives from
- dlss5-bridge, https://github.com/NIGos/dlss5-bridge, portions of which DLSS5-Feeder carries

This helper is a smaller rewrite for this mod, not a copy of their code; its NGX call sequence
(project-id initialisation, capability query, preset hints, the create and evaluate helpers,
the fault guards) follows theirs.
