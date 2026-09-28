// core/gfx/clarity.h - image clarity and anti-aliasing on the eye image, between
// the game's captured frame and the swapchain (docs/dishonored/PERFORMANCE.md,
// "Anti-aliasing and clarity"). Every lever ships OFF; with all of them off the
// method's plain blit runs exactly as before.
//
//   Resolve   when the render is larger than the runtime's recommended size (the
//             F10 resolution above ~100 %), filter it down to that size here with
//             a kernel that reads every rendered pixel, instead of handing the
//             compositor an oversized image it samples with one bilinear tap
//   Temporal  blend each eye with its own previous frame, reprojected by the
//             camera motion from matching depth when MotionVectors is enabled,
//             rotation otherwise; unmodelled motion is clipped to the current frame.
//             Head micro-motion is the sub-pixel jitter. Experimental
//   Sharpen   contrast-adaptive sharpening on the result (0 = off)
//
// The GPU side is clarity_gpu (no mod dependencies; tools/clarity-gpu-host.ps1
// runs it on the host GPU against synthetic images).
#pragma once
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;

namespace dvr::clarity {

void  set_resolve(bool on, const char* who);
bool  resolve_on();
void  set_temporal(bool on, const char* who);
bool  temporal_on();
void set_motion(bool on, const char* who);
bool motion_on();
void set_depth_scale(float scale, const char* who);
float depth_scale();
void  set_blend(float currentWeight, const char* who);   // 0.05..0.5, the new frame's weight
float blend();
void  set_sharpen(float amount, const char* who);        // 0..1
float sharpen();
bool  any_on();

// The eye texture's size for a capture of w x h: the resolve target while the
// resolve is on and the render exceeds the runtime's recommended pixel count,
// else w x h.
void output_size(uint32_t w, uint32_t h, uint32_t* ow, uint32_t* oh);

// Draw the capture `src` (w x h, the game's gamma-encoded pixels) into `dst`
// (ow x oh) through the enabled passes. eyeSign/recId name the pixels' eye and
// the pose record they were rendered with (0 = untagged: no temporal blend).
// False = nothing drawn (all off, shaders unavailable, a pass refused): the
// caller blits as before.
bool draw(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
          uint32_t w, uint32_t h, ID3D11RenderTargetView* dst, uint32_t ow, uint32_t oh,
          int eyeSign, uint32_t recId);

// Present-thread lifecycle invalidation. Keeps allocations for short capture gaps.
void invalidate();
void shutdown();
// One line for F10: what ran in the last few seconds.
const char* summary();
// The seam word: `clarity [status | resolve on|off | temporal on|off | blend <f> | sharpen <f>]`.
bool command(const char* args);

// Which camera position the vectors use: the pose record's WRITTEN position (0) or the c5 the
// image was RENDERED with (1). `dlss pos record|render`.
void set_pos_source(int src, const char* who);
int  pos_source();
// Diagnostic: multipliers on the DLSS guide translation per previous-camera axis (forward,
// right, up), 1 1 1 = as derived. `dlss taxis <f> <r> <u>`. Not saved.
void set_translation_axes(float f, float r, float u, const char* who);
// DLSS guides: nearer than this (depth units) is the player's own arms/weapon, whose vectors
// keep rotation and drop walking parallax. 0 = off. `dlss body <z>`, [Clarity] DlssBodyDepth.
void set_body_depth(float z, const char* who);
// DLSS vectors from the game's own view-projection matrices (1, default) or the rotator/FOV
// reconstruction (0). `dlss vp on|off`.
void set_use_vp(bool on, const char* who);
bool use_vp();
float body_depth();

// Motion vectors step 3: measure the game's depth scale from camera motion ([Diagnostics] MotionCalib).
void set_calib(bool on, const char* who);

} // namespace dvr::clarity
