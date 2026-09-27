// core/gfx/dlss.h - NVIDIA DLAA on each eye image (PERFORMANCE.md, "DLSS/DLAA through an x64
// NGX helper"). Default OFF; `dlss on|off` and F10 Advanced > Display > Clarity switch it live.
//
// The capture (the game's gamma-encoded frame, render size) goes through a DLSS feature of its
// own eye in the x64 helper (dlss_client.h), fed with guides built from the shared scene depth
// and the eye's two pose records (dlss_gpu.h). The result replaces the capture as the source of
// clarity's resolve and sharpen; the custom temporal AA does not also run. Hands and the F10
// panel are drawn on top afterwards, untouched.
//
// Phase 1 limits, stated where they matter: DLAA only (render == output); no projection jitter
// (the image still moves a little every frame with the head, which is what reconstruction has
// to work with); camera-only motion vectors (moving characters, hands, particles carry the
// camera's vector); the depth is an ordering, not the engine's device depth. Needs an NVIDIA
// RTX GPU and <game>\Binaries\Win32\dvr_dlss\ (the helper and nvngx_dlss.dll). Anything
// missing or refused leaves the normal path running and says why in the log.
#pragma once
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dvr::dlss {

struct GuideParams;

enum Mode { ModeOff = 0, ModeDlaa = 1 };
void set_mode(int mode, const char* who);
int  mode();
// NVSDK_NGX_DLSS_Hint_Render_Preset: 0 = the helper's pick (K, the 310.x transformer).
void set_preset(int preset, const char* who);
int  preset();
// The anti-smear mask (dlss_gpu.h, "bias"): pixels the camera vectors do not explain take the
// current colour. Off by default: the host test found DLSS already rejects large unexplained
// motion by itself and the mask cannot see the sub-pixel errors that do smear; kept as a live
// A/B (`dlss mask on|off`) for the headset.
// lo/hi: the colour excess (0..1) where the mask starts and saturates.
void set_mask(bool on, const char* who);
bool mask_on();
void set_mask_range(float lo, float hi, const char* who);
float mask_lo();
float mask_hi();

// Present thread. The reconstructed eye image (w x h, RGBA8, gamma-encoded like the capture),
// or null: the helper is not ready or refused, and the caller keeps its normal path.
// `reset`: this eye's history does not belong to this image (cut, load, record gap).
ID3D11ShaderResourceView* run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
                              uint32_t w, uint32_t h, int eye, const GuideParams& g, bool reset);
// True while DLAA owns the eye image (the custom temporal AA and its depth request stand down).
bool active();
// Present thread, every draw while the mode is off: releases the helper and every shared
// texture once DLAA has been switched off (off returns the memory, not just the frame time).
void idle();
void shutdown();                 // stops the helper; the next run() starts it again
const char* summary();           // one line for F10
bool command(const char* args);  // `dlss [status | on | off | retry | preset <n> | mask on|off | maskrange <lo> <hi>]`

} // namespace dvr::dlss
