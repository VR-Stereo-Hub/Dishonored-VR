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
// Each source is first carried into the held eye's view as a coarse depth-tested mesh (so the nearest
// surface at every texel is known, thin and near objects included), then refined per pixel through
// its depth; the two are compared in the held eye's own depth. A held point the fresh eye sees
// through (it moved) is stale and gives way. The rebuilt eye is submitted with the fresh generation's
// pose, so both eyes of the frame claim one head pose and the compositor's late warp treats them alike.
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
// Where an image came from, for the diagnostics: its pose record, that record's camera writer, when the
// camera was written and when the image was captured (GetTickCount64-style ms), and its DLSS jitter.
struct HandPose { bool ok = false; float p[3] = {0, 0, 0}; float q[4] = {0, 0, 0, 1}; };   // a grip, tracking space
struct CaptureMeta {
    uint32_t recId = 0; int writer = 0; double writeMs = 0, captureMs = 0;
    float jitter[2] = {0, 0}; uint32_t jitterDraws = 0;
    int64_t displayTime = 0; // XrTime of the real submission containing this native image, nanoseconds
};

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
// The last rebuild's matrix verdict: 0 unused, 1 used, 2 no matrices, 3-6 refused by the basis,
// turn, eye or camera check, 7 switched off (diagnostics and the host test).
int matrix_verdict();
// The foreground (the player's arms and weapon) is drawn with the game camera's FOV while the world
// uses the mod's projection; the rebuild reprojects pixels nearer than the foreground depth with the
// former. `set_fg_fov` is fed from the camera's FOV sensor; `afw fg on|off`, `afw fgdepth <units>`.
void set_fg_fov(float deg);
// Run 15: the game image of this grab BEFORE the mod's own layers (objective markers, the aim laser, the F10 panel), from
// the stereo method, once per present. The held eye's hands and weapon come from the fresh eye; from its composed image
// they carried that eye's UI into the other eye (text on the sword). `afw clean on|off` (default on).
bool clean_wanted();
void note_clean(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* frame, uint32_t grabSerial);
void set_clean(bool on, const char* who);
bool clean_on();
// Run 21: the game side reports whether both controllers are still (present thread, once per present); while they
// are, the held eye keeps its own shading on the weapon. `afw stillshade on|off` (default on).
void note_hands_still(bool still);
void set_still_shade(bool on, const char* who);
bool still_shade();
// Run 22: `afw edgehands on|off` (on).
void set_edge_hands(bool on, const char* who);
bool edge_hands();
// Run 25: `afw heldhands on|off` (on) - the held eye's hands moved by their controllers (the grips note_hands records).
void set_held_hands(bool on, const char* who);
bool held_hands();
// Run 18: the stale test's relative tolerance, `afw stale <0.005..0.1>`.
void set_stale(float rel, const char* who);
float stale();
void set_fg(bool on, const char* who);
bool fg();
void set_fg_depth(float units, const char* who);
// A pixel whose nearer candidate misses by less than this (texels) takes it instead of the fill; `afw nearmiss`.
void set_near_miss(float texels, const char* who);
float near_miss();
// The foreground (arms and weapon) from what the foreground pass drew (default on): `afw fgmask on|off`.
// Off = nearer than `afw fgdepth`.
void set_fg_mask(bool on, const char* who);
bool fg_mask();
// Still hands and weapon from the held eye's own image (its own shading) when the fresh eye agrees on their
// depth and colour within `limit` (0..1; 0 = off); `afw ownhands <limit>`.
void set_own_hands(float limit, const char* who);
float own_hands();
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
// are the pose record's rendered camera-relative view-projection and c5, `rotator` its camera
// (pitch, yaw, roll in degrees); nullptr when the record has none.
void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg,
                  const Pose targets[2], const float* vp16, const float* c5, const float* rotator,
                  const CaptureMeta* meta);

// RENDER thread, before the frame's layer is built: rebuild the held eye into `dst` (that eye's
// acquired swapchain image, w x h). `fresh` is the eye captured this present and `freshSerial` the
// grab serial the capture delivered this present (a record from an earlier present is refused);
// tanH/V the symmetric fov the layer claims. True = drawn; `outPose` is the pose to submit it with.
// False (with the reason) = nothing drawn: the caller keeps the rotation-only path.
bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, uint32_t freshSerial,
               ID3D11Texture2D* dst, uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose,
               const char** why);

// Is there a fresh image in the other eye to rebuild `held` from (the held eye's own image is
// optional)? Checked BEFORE the caller acquires the held swapchain image, so an acquired image is
// never released unwritten.
bool has_held(int held);
// The fallback after an acquire: the held image copied as it is (rotation-only, its own pose), or
// the fresh one when the held eye has none.
bool copy_held(ID3D11DeviceContext* ctx, int held, ID3D11Texture2D* dst);

// Diagnostics: capture `presents` consecutive rebuilds (1..32) starting `delayMs` from now into a new
// folder under `dumpsRoot` (`afw dump`, the F10 button). Game output, local only.
void request_dump(int presents, uint32_t delayMs, const char* dumpsRoot, const char* who);

// VR-39: the depth layer (XR_KHR_composition_layer_depth, `[VR] SubmitDepth`). While wanted, each rebuild also
// writes the rebuilt eye's depth in its target view. write_xr_depth fills an XR depth swapchain image (a D3D11
// depth format, w x h) with what `eye`'s layer shows: the rebuild's depth when `rebuilt`, else the eye's own
// depth snapshot (the image shown as captured). Standard depth: 0 at nearM, 1 at farM (metres).
void set_xr_depth_wanted(bool on);

// The mod's own spacewarp (MSW, `[VR] ModSpacewarp`): rebuild `eye` for a display slot the game did not fill, from
// its own last image and depth, at the slot's eye position (tracking, metres). The image keeps its own orientation;
// the body's walking and turning are extrapolated on the images' submission timeline to displayTime (XrTime).
// Both motions go into the WORLD pixels; hands and submitted orientation do not inherit the body turn.
// nowMs/captureMs are the legacy host/replay clock, used only when displayTime is zero. Writes dst (w x h).
bool synth_eye(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* dst, uint32_t w, uint32_t h,
               float tanH, float tanV, const float targetPos[3], double nowMs, Pose* outPose, const char** why,
               const HandPose* slotHands = nullptr, int64_t displayTime = 0);
// The grip poses the eye's last image drew its hands from (after note_capture), and whether a synthesized slot
// moves the hands by their controllers' motion since (`vrpace msw hands on|off`, default off).
void note_hands(int eye, const HandPose hands[2]);
void set_synth_hands(bool on);
bool synth_hands();
void set_synth_extrapolate(bool on);
// The synthesized slot's seed grid step in source texels (2..16; `vrpace msw grid <n>`).
void set_synth_grid(int step);
int synth_grid();
// An eye's own last image copied as it is into dst, and the pose it was rendered from (MSW's fallback).
bool copy_own(ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* dst, Pose* pose);
bool synth_extrapolate();
// VR-39 MSW guards (PLAN-mod-spacewarp section 7). Off by default: `[VR] ModSpacewarpGuard`, `vrpace msw guard on|off`.
// On: a slot whose two held images straddle a jump (camera speed over `maxSpeed` uu/s - a Blink, a teleport, a cut - or
// a body-turn rate over `maxRate` deg/s - a snap turn) re-submits the last real frame instead of synthesizing; below
// those ceilings the per-eye turn is clamped to `maxTurn` deg. `vrpace msw maxspeed|maxturnrate|maxturn <n>`.
void set_synth_guard(bool on);
bool synth_guard();
void set_synth_limits(float maxSpeedUUs, float maxRateDegS, float maxTurnDeg);   // a non-positive value keeps the current
void synth_limits(float* maxSpeedUUs, float* maxRateDegS, float* maxTurnDeg);
// MSW thread, under the frame mutex, before any eye is built: why this slot must re-submit the last real frame (the
// guard saw a jump between the held images), or nullptr. Always nullptr with the guard off.
const char* synth_hold_reason(bool displayClock);
// The stick stop (`[VR] ModSpacewarpStickStop`, `vrpace msw stickstop on|off`, default off): no turn is extrapolated
// while the right-stick turn the game receives is zero. note_turn_stick: PRESENT lane, the pad bridge's final
// composed right-stick X (-1..1, zero when a menu, the F10 pointer or snap turn took the stick).
void set_synth_stick_stop(bool on);
bool synth_stick_stop();
void note_turn_stick(float rx);
bool write_xr_depth(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, bool rebuilt, ID3D11Texture2D* dst,
                    uint32_t dxgiFormat, uint32_t w, uint32_t h, float nearM, float farM, const char** why);
const char* dump_status();

void shutdown();

} // namespace dvr::afw
