// game/dishonored/camera.h - the per-eye camera seam (41.0).
//
// A stereo method needs three things from the game's camera, per eye: the
// rotation, the field of view and the eye position. Rotation is the head-
// tracking write (ApplyHeadToViewRotation, head_track.cpp: HMD pitch/yaw
// into the ProcessViewRotation parms, measured and shipped since 30.57).
// FOV is the lever (fov_lever.cpp: kLevCtrl/kLevCam every dispatch, with
// 0x53c as the read-only sensor of what the engine rendered). The EYE
// POSITION - +/- IPD/2 along the camera's right axis - is the seam's own
// write: camera+0x330 (which holds -position) was MEASURED by `camera
// eyetest` on 2026-09-02 (HONOURED 119/120; ENGINE_NOTES, "The per-eye camera
// seam"). Positional tracking rides the same write on the camera lane (below).
//
// Lanes: the writers run on the SCRIPT lane (the ProcessEvent hook's camera
// pass, right after the lever); the render-side readback (vertex constant c5,
// the camera world position of the draw) arrives on the present thread.
// Everything here is plain state behind those two entry points.
#pragma once
#include <stdint.h>

namespace dvr::status { class Writer; }

namespace dvr::camera {

// Which eye the NEXT game frame renders: -1 left, +1 right, 0 = the game's
// own camera (mono). Set per present from the active stereo method.
void set_eye(int sign);
int  eye();
void  set_ipd_m(float m);          // inter-eye separation (the runtime's views)
float ipd_m();
void  set_world_scale(float uuPerM);   // [PosTrack] Scale: 100 uu/m measured
float world_scale();
float eye_offset_uu();             // = eye * ipd/2 * scale (signed, 0 for mono)

// The field the offset is written to ([Camera] EyeField=): "0x80", "0x90",
// "0xc4", "0x330", "0x350", "0x374" (kCamLoc0/1/2, kPovOffs) or "" / "none".
// Empty until the eyetest has measured one; apply_eye_offset then does nothing
// and says so once.
bool        set_eye_field(const char* name);
const char* eye_field();

// FOV: the lever's target (0 = lever off) and the sensor's last reading.
void  set_fov_deg(float deg);
float fov_deg();
void  note_rendered_fov(float deg);    // fov_lever.cpp, from the 0x53c sensor
float rendered_fov_deg();

// The render-side truth: c5 of the last draw (vs_const_hook.cpp).
void note_render_pos(const float pos[3]);
bool render_pos(float out[3]);

// Positional tracking (lean, crouch, roomscale) on the seam (S1). The offset
// is a VIEW-SPACE displacement in uu (right, up, forward), published every
// present by TrackHead (head_track.cpp) - the seam is its single owner, and
// both lanes read it here:
//   vp      the c0 view-projection patch (LeanVP, core/framework/vs_const.cpp):
//           the shipped path, a matrix patch the renderer's attachments do
//           not follow
//   camera  the camera seam's own write: the offset goes into [Camera]
//           EyeField with the eye offset, in ONE write per dispatch, along
//           the camera object's basis rows (+0x50 forward, +0x60 right,
//           +0x70 up; validated orthonormal before the first write)
// [PosTrack] Lane= picks the lane (default vp); `postrack lane <l>` live.
void  set_position_offset_uu(float right, float up, float fwd);   // present thread
void  position_offset_uu(float out[3]);                           // any thread
enum class PosLane { Vp = 0, Camera = 1 };
bool        set_pos_lane(const char* name);   // "vp" | "camera"
PosLane     pos_lane();
const char* pos_lane_name();

// The 38.24 eye clamp's ceiling (world Z the camera may not rise above),
// published by FovLeverApply while its clamp is live; the camera-lane writer
// caps the position it writes at it. Off = no cap.
void set_eye_ceiling(float zMax, bool on);

// SCRIPT LANE, after the lever: write the eye offset (eye_offset_uu() along
// the right row) plus, on the camera lane, the position offset, into the
// selected field - one write, re-based every tick so a persistent field does
// not accumulate; restored when both offsets go to zero. `camObj` is the live
// camera object (head_track's g_camObj; null = skip).
bool apply_offsets(uint8_t* camObj);
inline bool apply_eye_offset(uint8_t* camObj) { return apply_offsets(camObj); }

// The positional instrument. `camera postest <R> <U> <F>` (uu): the seam
// OVERRIDES the tracked offset with the commanded triple, takes a 45-present
// c5 baseline at zero, then applies it for 120 presents. On the camera lane
// the measured travel is c5's displacement projected on the basis rows (the
// eyetest's arithmetic); on the vp lane the instrument reports how many
// presents the c0 patch actually ran on with that triple (the matrix effect
// is not visible from c5; the picture is: world-6dof.xrs). Stand still.
bool postest_start(float r, float u, float f);
void postest_stop(const char* why);
void postest_present_tick();     // present thread, after the draw
void note_vp_applied();          // vs_const_hook: the c0 patch ran this upload
bool postest_active();

// The instrument. `camera eyetest <uu> [field|all]`: for 120 presents per
// candidate the script lane adds <uu> along right into the candidate and the
// present thread reads c5 back; each candidate gets a HONOURED / DISCARDED /
// INCONCLUSIVE verdict in the log. Stand still in gameplay while it runs.
bool eyetest_start(float uu, const char* field);   // field "all" = every candidate
void eyetest_stop(const char* why);
void eyetest_script_tick(uint8_t* camObj);         // script lane, after the lever
void eyetest_present_tick();                       // present thread, after the draw
bool eyetest_active();

void status(dvr::status::Writer& w);   // status.json "camera"
void log_status();                     // `camera status`

} // namespace dvr::camera
