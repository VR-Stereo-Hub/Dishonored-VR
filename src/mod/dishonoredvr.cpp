// Dishonored VR - unity translation unit (Phase 1 of the refactor).
//
// The mod was one 23k-line file. tools/split-source.py cut it into the module
// tree below without changing a single function body: state (includes, types,
// globals) in the original order, then a prototype for every function, then
// the bodies grouped by subsystem. Everything is still ONE translation unit
// with the same static linkage, so the program is byte-for-byte the same;
// later phases give each module its own header and translation unit and
// drop it from this list.
//
// Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

// ---- state: includes, types, globals, macros (original order) ----------
#include "mod/state/00_mod_prelude.inc"

// ---- modules with their own translation unit: headers only ------------------
#include "dvr_version.h"
#include "core/util/log.h"
#include "core/util/clock.h"
#include "core/util/mem.h"
#include "core/util/ini.h"
#include "core/util/paths.h"
#include "core/util/diag.h"
#include "core/util/crash.h"
#include "core/hooks/vtable.h"
#include "core/hooks/iat.h"
#include "core/hooks/detour.h"
#include "core/framework/command.h"
#include "core/framework/status.h"
#include "game/dishonored/patterns.h"
#include "game/dishonored/aim_ray.h"
#include "game/dishonored/anim_state.h"
#include "game/dishonored/swing.h"
#include "game/dishonored/drop_assist.h"
#include "game/dishonored/snap_turn.h"   // VR-219: pad_bridge (present lane) and head_track (script lane) both call it
#include "core/vr/openxr_runtime.h"
#include "core/vr/openxr_input.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/perf.h"
#include "core/framework/native_profile.h"
#include "core/framework/query_wait_profile.h"
#include "core/framework/scene_prepare_profile.h"
#include "core/framework/bridge_profile.h"
#include "core/framework/diagnostic_ab.h"
#include "core/gfx/stereo.h"
#include "core/gfx/desktop_eye.h"
#include "core/vr/pose_record.h"
#include "core/gfx/capture.h"
#include "core/gfx/gpu_memory.h"
#include "core/vr/hud_stub.h"
#include "core/ui/ovl_ui.h"
#include "core/gfx/hud_class.h"
#include "core/gfx/hud_capture.h"
#include "core/gfx/hud_layout.h"
#include "game/dishonored/ui_ride_policy.h"
#include "core/gfx/frame_id.h"
#include "core/gfx/device_census.h"
#include "core/gfx/clarity.h"
#include "core/gfx/sampler_force.h"

#include "core/gfx/d3d9ex.h"
#include "game/dishonored/camera.h"
#include "game/dishonored/z_account.h"
#include "game/dishonored/hands/hand_frame.h"
#include "game/dishonored/hands/weapon_frame.h"
#include "game/dishonored/hands/hand_frame_test.h"
#include "game/dishonored/hands/menu_keep.h"

#include "mod/state/01_proxy_proxy_state.inc"
#include "mod/state/02_legacy_vs_scan.inc"
#include "mod/state/04_core_gfx_d3d11_device.inc"
#include "mod/state/05_core_gfx_d3d9_capture.inc"
#include "mod/state/06_game_dishonored_head_track.inc"
#include "mod/state/07_game_dishonored_patterns.inc"
#include "mod/state/08_game_dishonored_fov_lever.inc"
#include "mod/state/09_legacy_spacebases.inc"
#include "mod/state/10_legacy_rtd_drive.inc"
#include "mod/state/11_core_gfx_hand_mesh.inc"
#include "mod/state/12_game_dishonored_hands_skelcontrol.inc"
#include "mod/state/13_game_dishonored_game_state.inc"
#include "mod/state/14_core_util_log.inc"
#include "mod/state/15_core_config_config.inc"
#include "mod/state/18_core_gfx_hand_mesh.inc"
#include "mod/state/20_core_window_game_window.inc"
#include "mod/state/23_game_dishonored_ue3_uobject.inc"
#include "mod/state/24_game_dishonored_head_track.inc"
#include "mod/state/25_legacy_camera_tracer.inc"
#include "mod/state/26_legacy_fire_tracer.inc"
#include "mod/state/27_game_dishonored_motion_aim.inc"
#include "mod/state/28_legacy_aim_watch.inc"
#include "mod/state/29_game_dishonored_motion_aim.inc"
#include "mod/state/30_legacy_fp_mesh.inc"
#include "mod/state/31_legacy_camera_hook.inc"
#include "mod/state/32_game_dishonored_head_track.inc"
#include "mod/state/33_game_dishonored_ue3_uobject.inc"
#include "mod/state/34_legacy_fp_mesh.inc"
#include "mod/state/35_game_dishonored_ue3_uobject.inc"
#include "mod/state/36_legacy_fp_mesh.inc"
#include "mod/state/37_game_dishonored_hands_fp_mesh.inc"
#include "mod/state/38_legacy_rtd_drive.inc"
#include "mod/state/39_game_dishonored_hands_fp_mesh.inc"
#include "mod/state/40_game_dishonored_hands_arms_hide.inc"
#include "mod/state/41_game_dishonored_ue3_uobject.inc"
#include "mod/state/42_game_dishonored_crouch.inc"
#include "mod/state/43_game_dishonored_hands_skelcontrol.inc"
#include "mod/state/44_game_dishonored_blink.inc"
#include "mod/state/45_game_dishonored_hands_skelcontrol.inc"
#include "mod/state/46_legacy_cam_seam.inc"
#include "mod/state/48_legacy_cam_seam.inc"
#include "mod/state/49_game_dishonored_head_track.inc"
#include "mod/state/50_game_dishonored_fov_lever.inc"
#include "mod/state/51_legacy_spacebases.inc"
#include "mod/state/52_game_dishonored_head_track.inc"
#include "mod/state/53_core_input_pad_bridge.inc"
#include "mod/state/54_game_dishonored_arm_follow.inc"
#include "mod/state/55_game_dishonored_hands_mesh_split.inc"
#if DVR_WITH_LEGACY
#include "legacy/vr33/57_game_dishonored_hands_weapon_id.inc"
#endif
#include "mod/state/57b_game_dishonored_hands_weapon_attach.inc"
#include "mod/state/56_game_dishonored_hands_pose_report.inc"
#if DVR_WITH_LEGACY
#include "legacy/vr33/57_game_dishonored_hands_bone_query.inc"
#endif
#if DVR_WITH_LEGACY
#include "legacy/vr33/58_game_dishonored_hands_hand_move.inc"
#endif
#include "mod/state/59_game_dishonored_hands_palette_capture.inc"
#include "mod/state/60_game_dishonored_ue3_reflect.inc"
#include "mod/state/61_game_dishonored_startup.inc"
#include "mod/state/62_game_dishonored_ue3_ui_state.inc"
#include "mod/state/63_game_dishonored_aim_seam.inc"

// ---- every function, so the bodies below can be in any order --------------
#include "mod/fwd.h"
#include "game/dishonored/ue3/gobj_walk.h"   // VR-102: cheap GObjects walks; needs fwd.h

// ---- function bodies by subsystem -------------------------------------------
#if !DVR_WITH_LEGACY
#include "legacy/legacy_stubs.inc"
#endif
#define DVR_CAT ::dvr::log::Cat::openxr
#include "core/vr/apilayer_guard.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::cfg
#include "core/config/config.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::present
#include "core/framework/perf_ab.cpp"
#include "core/framework/desktop_benchmark.cpp"
#include "core/framework/vs_const_hook.cpp"
#include "game/dishonored/present_tick.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::present
#include "core/framework/vs_const.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/d3d11_device.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "core/gfx/hand_mesh.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::pad
#include "core/input/hotkeys.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::pad
#include "core/input/pad_bridge.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::overlay
#include "core/ui/overlay.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::res
#include "core/window/game_window.cpp"
#include "core/window/render_size.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::blink
#include "game/dishonored/blink.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::blink
#include "game/dishonored/blink_stubs.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/block_state.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::console
#include "game/dishonored/console.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::crouch
#include "game/dishonored/crouch.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::fov
#include "game/dishonored/fov_lever.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::armfollow
#include "game/dishonored/arm_follow.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::menu
#include "game/dishonored/game_state.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/arms_hide.cpp"
#include "game/dishonored/hands/mat_hide.cpp"
#include "game/dishonored/hands/mesh_split.cpp"
#include "game/dishonored/hands/pose_report.cpp"
#if DVR_WITH_LEGACY
#include "legacy/vr33/bone_query.cpp"
#endif
#include "game/dishonored/hands/palette_capture.cpp"
#if DVR_WITH_LEGACY
#include "legacy/vr33/hand_move.cpp"
#endif
#if DVR_WITH_LEGACY
#include "legacy/vr33/weapon_id.cpp"
#endif
#include "game/dishonored/hands/weapon_mirror.cpp"   // VR-138: called from weapon_attach.cpp
#include "game/dishonored/hands/weapon_attach.cpp"
#include "game/dishonored/hands/draw_census.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/fp_mesh.cpp"
#include "game/dishonored/hands/menu_keep.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::graft
#include "game/dishonored/hands/graft.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/hand_pose.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/skelcontrol.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::head
#include "game/dishonored/head_track.cpp"
#include "game/dishonored/snap_turn.cpp"   // VR-219: snap turn; rides head_track's yaw book
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::melee
#include "game/dishonored/melee.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::aim
#include "game/dishonored/motion_aim.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::core
#include "game/dishonored/shared/ue_math.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::script
#include "game/dishonored/scene_probe.cpp"
#include "game/dishonored/viewport_resize.cpp"
#include "game/dishonored/scene_draw.cpp"
#include "game/dishonored/ue3/process_event.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::script
#include "game/dishonored/ue3/uobject.cpp"
#include "game/dishonored/ue3/reflect.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::cfg
// VR-157: read-only; needs uobject.cpp's FindFunctionObj and console.cpp's RunConsole.
#include "game/dishonored/game_opts.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::script
#include "game/dishonored/anim_state.cpp"
#include "game/dishonored/drop_assist.cpp"      // drop takedowns: sampled from the anim tick
#include "game/dishonored/move_speed.cpp"     // VR-204: after anim_state (its caller) and reflect
#include "game/dishonored/possession_state.cpp"   // VR-135: before its one consumer
#include "game/dishonored/stereo_state.cpp"
#include "game/dishonored/cinematic_trace.cpp"
#include "game/dishonored/fov_lever_owners.cpp"
#include "game/dishonored/cam_modifiers.cpp"   // VR-165: needs cinematic_trace's resolved camera cache
#include "game/dishonored/cam_shake.cpp"       // VR-172: the game's own camera shake; after the trace's camera cache
#include "game/dishonored/swing_trace.cpp"    // VR-165: raw present-rate series
#include "game/dishonored/aim_source.cpp"     // VR-166: who shares the power-aim helper
#include "game/dishonored/rain_control.cpp"   // VR-136: after the trace's camera-cache layout
#include "game/dishonored/stereo_occlusion.cpp"   // VR-79: per-eye occlusion culling
#include "game/dishonored/trail_control.cpp"  // VR-171: the sword's swing trail; after anim_state and reflect
#include "game/dishonored/lens_control.cpp"   // VR-137: after rain_control (shared helpers)
#include "game/dishonored/cinematic_fov.cpp"
#include "game/dishonored/cinematic_pitch.cpp"
#include "game/dishonored/menu_immersion.cpp"
#include "game/dishonored/cinematic_letterbox.cpp"
#include "game/dishonored/hud_owner.cpp"
#include "game/dishonored/objective_markers.cpp"
#include "game/dishonored/ue3/prop_watch.cpp"
#if DVR_WITH_LEGACY
#include "legacy/interact_focus.cpp"   // VR-85: retired, see TRAPS
#endif

// VR-57: the fire-direction probe (read-only); needs reflect.cpp's resolver above.
#include "game/dishonored/aim_seam.cpp"
#include "game/dishonored/fire_aim.cpp"
#include "game/dishonored/interact_aim.cpp"   // VR-166: interaction aimed by hand
#include "game/dishonored/throw_aim.cpp"      // VR-166: grenades aimed by hand
#include "game/dishonored/hands/fx_follow.cpp" // VR-182: after weapon_attach (its snapshot) and throw_aim (rotator maths)
#include "game/dishonored/power_aim.cpp"      // VR-44: Windblast, Possession, Swarm by hand
#include "game/dishonored/ue3/ui_state.cpp"
#include "game/dishonored/ue3/ui_surface.cpp"
#include "game/dishonored/startup.cpp"
#undef DVR_CAT
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/aim_watch.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/cam_seam.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/camera_hook.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/camera_tracer.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/fire_tracer.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/fp_mesh.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/rtd_drive.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/spacebases.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/ue3_probe.cpp"
#undef DVR_CAT
#endif
#if DVR_WITH_LEGACY
#define DVR_CAT ::dvr::log::Cat::legacy
#include "legacy/vs_scan.cpp"
#undef DVR_CAT
#endif
#define DVR_CAT ::dvr::log::Cat::proxy
#include "proxy/d3d9_exports.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::proxy
#include "proxy/dllmain.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::capture
#include "core/gfx/frame_dump.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::cmd
#include "game/dishonored/commands.cpp"
#undef DVR_CAT
