// game/dishonored/camera.h - the per-eye camera seam (41.0).
//
// A stereo method needs three things from the game's camera, per eye: the
// rotation, the field of view and the eye position. Rotation is the head-
// tracking write (ApplyHeadToViewRotation, head_track.cpp: HMD pitch/yaw
// into the ProcessViewRotation parms, measured and shipped since 30.57).
// FOV is the lever (fov_lever.cpp: kLevCtrl/kLevCam every dispatch, with
// 0x53c as the read-only sensor of what the engine rendered). The EYE
// POSITION - +/- IPD/2 along the camera's right axis - is the new write, and
// its write point is UNMEASURED: the POV location is a cache the engine
// recomputes each tick, yet the 38.24 eye clamp writes Z into four of its
// fields on the dispatch cadence and is honoured. `camera eyetest` decides
// which field the renderer reads (see ENGINE_NOTES, "The per-eye camera seam").
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

// SCRIPT LANE, after the lever: write eye_offset_uu() along the camera's
// right row into the selected field (re-based every tick, so a persistent
// field does not accumulate; restored when the eye goes back to 0).
// `camObj` is the live camera object (head_track's g_camObj; null = skip).
bool apply_eye_offset(uint8_t* camObj);

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
