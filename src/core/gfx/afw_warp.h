// core/gfx/afw_warp.h - AFW's held-eye rebuild (VR-39).
//
// Under `stereo afw` every present is its own XR frame: one eye fresh, the other its previous
// image. The compositor reprojects that held image for HEAD ROTATION only, which leaves the errors
// the headset showed: the game's own stick/snap yaw since the image (the world "zooms"), the eyes'
// translation when the head moves or turns (near objects drift against the head and settle), and
// anything that MOVED since the image - above all the hands and weapon, which move with the
// controllers and turn with the body, so any temporal-only reprojection leaves a ghost of them.
//
// This pass rebuilds the held eye into its swapchain image before the frame is submitted, from TWO
// sources, each with its own depth (the prior art: PureDark's AFW for UEVR and the RE Engine uses the
// alternate eye plus the previous frame; Oculus Stereo Shading Reprojection reprojects one eye into
// the other with depth and fills the holes from another source):
//   the fresh eye   this instant, the other viewpoint: the hands and weapon (nearer than the body
//                   threshold) come from here, so both eyes show them at the same moment, and the
//                   world an old hand or a turn uncovered in the held image
//   the held eye    its own last image, carried by the head change and the body yaw since it: the
//                   world, from this eye's own viewpoint (true stereo, its own shading)
// Each is a short fixed-point search back through that image's depth (a near seed for the hands, a
// far one for the world). The rebuilt eye is submitted with the fresh generation's pose, so both
// eyes of the frame claim one head pose and the compositor's own late warp treats them alike.
//
// Depth is the scene target's alpha (linear view depth, depth units; core/gfx/depth_probe.h), copied
// per eye at its capture (the shared ring moves on each present), scaled to metres by [Clarity]
// MotionDepthScale (uu per unit) over the world scale (uu per metre). The held eye's world moves by
// the game's own matrices when both records carry them (walking included), else by the XR poses and
// the body yaw. Not carried: objects that moved in the world between the images (an NPC) in the held
// eye's world, and translucent effects in the hands region (no depth of their own: they take the
// surface behind them).
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
// The fresh eye as a source (default on); `afw stereo on|off`. Off = the held eye alone, the hands by
// the body hypothesis (the first version, kept as the A/B).
void set_stereo(bool on, const char* who);
bool stereo();
// The held eye's world by the game's own view-projection matrices (default on; head, stick yaw and
// walking), checked each present against the XR pose model; `afw matrices on|off`. Off = the XR pose
// and the body yaw alone (walking lags a tick).
void set_matrices(bool on, const char* who);
bool matrices();
// Tints the held eye by source; `afw debug on|off`.
void set_debug(bool on, const char* who);
bool debug();
// [Stereo] AfwBodyDepth (depth units): nearer is the body. `afw body <units>`.
void set_body_depth(float units, const char* who);
float body_depth();
// uu per metre (the game's [PosTrack] Scale); the depth unit's metres come from it and
// clarity::depth_scale().
void set_world_scale(float uuPerM);

// RENDER (present) thread, at the capture of a fresh eye: keep a copy of its image and its depth, and
// what it needs to be used one present later. `targets` are both eyes' view poses of the SAME locate
// generation as this image - the pose the OTHER (held) eye is rebuilt at this present. `vp16`/`c5`
// are the pose record's rendered camera-relative view-projection and c5 (nullptr when the record
// has none).
void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg,
                  const Pose targets[2], const float* vp16, const float* c5);

// RENDER thread, before the frame's layer is built: rebuild the held eye into `dst` (that eye's
// acquired swapchain image, w x h). `fresh` is the eye captured this present; tanH/V the symmetric
// fov the layer claims. True = drawn; `outPose` is the pose to submit it with. False (with the
// reason) = nothing drawn: the caller keeps the rotation-only path.
bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, ID3D11Texture2D* dst,
               uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose, const char** why);

// Is there a held image for `held` (and a fresh one for the other eye) to warp? Checked BEFORE the
// caller acquires the held swapchain image, so an acquired image is never released unwritten.
bool has_held(int held);
// The fallback after an acquire: the held image copied as it is (rotation-only, its own pose).
bool copy_held(ID3D11DeviceContext* ctx, int held, ID3D11Texture2D* dst);

void shutdown();

} // namespace dvr::afw
