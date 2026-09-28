// core/gfx/dlss.h - NVIDIA DLAA on each eye image (PERFORMANCE.md, "DLSS/DLAA through an x64
// NGX helper"). Default OFF; `dlss on|off` and F10 Advanced > Display > Clarity switch it live.
//
// The capture (the game's gamma-encoded frame, render size) goes through a DLSS feature of its
// own eye in the x64 helper (dlss_client.h), fed with guides built from the shared scene depth
// and the eye's two pose records (dlss_gpu.h). The result replaces the capture as the source of
// clarity's resolve and sharpen; the custom temporal AA does not also run. Hands and the F10
// panel are drawn on top afterwards, untouched.
//
// Super Resolution (phase 2): DlssQuality > 0 renders the game at the output size divided by the
// mode's per-axis ratio and reconstructs the output. The OUTPUT is the headset resolution the
// player chose ([Clarity] DlssOutputWidth/Height, set from the F10 resolution while SR is on);
// the game's own render size ([Screen] RenderWidth/Height) becomes the reduced one, driven by
// the game side (viewport_resize.cpp) through the proven live-resize path. Turning SR off, or
// DLSS failing, resizes back to the output.
//
// Phase 1 limits, stated where they matter: no projection jitter
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
// The upscaler behind the mode (2026-09-27): NVIDIA DLSS (NGX) or AMD FSR (FidelityFX API: FSR 4
// where the AMD runtime offers it, FSR 3.1 elsewhere). Same helper, inputs, jitter and modes;
// switching restarts the helper. [Clarity] Upscaler=0|1, `dlss backend dlss|fsr`.
enum Backend { BackendDlss = 0, BackendFsr = 1 };
void set_backend(int b, const char* who);
int  backend();
const char* backend_name();           // "DLSS" or "FSR"
void set_fsr_version(int v, const char* who);   // 0 = the runtime's default, else 1-based in its list
int  fsr_version();
const char* runtime_name();           // FSR: the provider in use, once the helper is up; "" otherwise
const char* offered_versions();       // FSR: every version the runtime offers ("3.1.5, 2.3.4"); "" until the helper is up
// Quality: 0 DLAA (render = output), 1 Quality (1.5x per axis), 2 Balanced (1.72x),
// 3 Performance (2x), 4 Ultra Performance (3x), 5 Ultra Quality (1.3x). Ultra Quality is 5, not
// inserted after DLAA, so every saved DlssQuality keeps its meaning; the F10 list shows the modes
// in ratio order (kQualityOrder).
enum Quality { QDlaa = 0, QQuality, QBalanced, QPerformance, QUltra, QUltraQuality, QCount };
const int kQualityOrder[QCount] = {QDlaa, QUltraQuality, QQuality, QBalanced, QPerformance, QUltra};
void set_quality(int q, const char* who);
int  quality();
float ratio();                        // per axis, 1 for DLAA
const char* quality_name(int q);
// The SR output (the headset resolution). 0x0 = none recorded.
void set_output(uint32_t w, uint32_t h, const char* who);
bool output(uint32_t* w, uint32_t* h);
// The render size SR uses for an output (even, same aspect within a pixel).
void render_for(uint32_t ow, uint32_t oh, uint32_t* w, uint32_t* h);
// True, with the output size, when an eye image of w x h is SR's reduced render.
bool sr_output_for(uint32_t w, uint32_t h, uint32_t* ow, uint32_t* oh);
bool failed();                        // the helper is unavailable (the game side restores the output size)
// The model: 0 transformer (preset K, best image, ~2 ms per eye at 2750x2850 output), 1 fast
// (the CNN presets E for Super Resolution and F for DLAA, ~0.9 ms per eye). DlssPreset, when
// nonzero, overrides both with one raw NVSDK_NGX_DLSS_Hint_Render_Preset value.
void set_model(int m, const char* who);
int  model();
// The model list F10 offers: each entry is a (model, preset) pair written to DlssModel/DlssPreset.
// Preset 16 = NVIDIA's recommended preset per mode (K for DLAA/Ultra Quality/Quality/Balanced, M for
// Performance, L for Ultra Performance), set explicitly so a runtime update cannot change it.
struct ModelChoice { const char* name; int model; int preset; const char* tip; };
const int kPresetPerMode = 16;
extern const ModelChoice kModelChoices[];
extern const int kModelChoiceCount;
int  model_choice();                  // index into kModelChoices for the current setting, -1 = a custom raw preset
void set_model_choice(int i, const char* who);
// Diagnostics off the per-frame path unless asked for: the vector audit and flow check
// (`dlss audit on`), which also keep a copy of every eye image.
void set_audit(bool on, const char* who);
bool audit_on();
// NVSDK_NGX_DLSS_Hint_Render_Preset: 0 = from the model above.
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
// ow x oh: the output (equal to w x h for DLAA). The result is ow x oh.
ID3D11ShaderResourceView* run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
                              uint32_t w, uint32_t h, uint32_t ow, uint32_t oh, int eye, const GuideParams& g, bool reset);
// True while DLAA owns the eye image (the custom temporal AA and its depth request stand down).
bool active();
// Present thread, every draw while the mode is off: releases the helper and every shared
// texture once DLAA has been switched off (off returns the memory, not just the frame time).
void idle();
void shutdown();                 // stops the helper; the next run() starts it again
const char* summary();           // one line for F10
bool command(const char* args);  // `dlss [status | on | off | retry | preset <n> | mask on|off | maskrange <lo> <hi>]`

} // namespace dvr::dlss
