// core/gfx/afw_warp.h - AFW's held-eye warp (VR-39).
//
// Under `stereo afw` every present is its own XR frame: one eye fresh, the other its previous
// image. The compositor reprojects that held image for HEAD ROTATION only, which leaves three
// errors the headset showed: the game's own stick/snap yaw since the image (the world "zooms"),
// the eyes' translation when the head moves or turns (near objects - the hands and weapon - drift
// against the head and settle), and, once the world is yaw-corrected as a whole image, a ghost of
// the hands and weapon (they turn WITH the body, the world does not).
//
// This pass re-renders the held eye into its swapchain image before the frame is submitted, from
// that eye's last image and its own depth, as seen from the FRESH eye's head pose:
//   world pixel  tracking-space point from its own view pose and depth, moved by the body yaw
//                since the image (the world turned against the tracking space), projected into
//                the target view
//   body pixel   (nearer than the body threshold: the player's hands and weapon) the same, with
//                no body yaw: they are fixed in tracking space while the body turns
// The target pixel does not know which it is, so both are solved (a short fixed-point search back
// into the source image through its depth) and the nearer consistent one wins; disocclusions take
// the world estimate. The warped image is then submitted with the target pose, so both eyes of the
// frame claim one head pose and the compositor's own late warp treats them alike.
//
// Depth is the scene target's alpha (linear view depth, depth units; core/gfx/depth_probe.h),
// scaled to metres by [Clarity] MotionDepthScale (uu per unit) over the world scale (uu per metre).
// Walking (the body's translation) is not carried yet: the held eye's world lags a tick of walking
// parallax, as it did before this pass.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace dvr::afw {

struct Pose { float q[4]; float p[3]; };   // OpenXR convention, LOCAL space, metres; q = x y z w

// On by the AFW method; `afw warp on|off`. Off = the rotation-only held eye (the runtime's
// pose-yaw fallback).
void set_enabled(bool on, const char* who);
bool enabled();
// [Stereo] AfwBodyDepth (depth units): nearer is the body. `afw body <units>`.
void set_body_depth(float units, const char* who);
float body_depth();
// uu per metre (the game's [PosTrack] Scale); the depth unit's metres come from it and
// clarity::depth_scale().
void set_world_scale(float uuPerM);

// RENDER (present) thread, at the capture of a fresh eye: keep a copy of its image and what it
// needs to be warped one present later. `targets` are both eyes' view poses of the SAME locate
// generation as this image - the pose the OTHER (held) eye is warped to at this present.
void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg,
                  const Pose targets[2]);

// RENDER thread, before the frame's layer is built: warp the held eye's last image into `dst`
// (that eye's acquired swapchain image, w x h). `fresh` is the eye captured this present; tanH/V
// the symmetric fov the layer claims. True = drawn; `outPose` is the pose to submit it with.
// False (with the reason) = nothing drawn: the caller keeps the rotation-only path.
bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, ID3D11Texture2D* dst,
               uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose, const char** why);

// Is there a held image for `held` (and a fresh one for the other eye) to warp? Checked BEFORE the
// caller acquires the held swapchain image, so an acquired image is never released unwritten.
bool has_held(int held);
// The fallback after an acquire: the held image copied as it is (rotation-only, its own pose).
bool copy_held(ID3D11DeviceContext* ctx, int held, ID3D11Texture2D* dst);

void shutdown();

} // namespace dvr::afw
