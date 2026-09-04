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
#include "core/vr/openxr_runtime.h"
#include "core/vr/openxr_input.h"
#include "core/framework/frame_hooks.h"
#include "core/framework/perf.h"
#include "core/gfx/stereo.h"
#include "core/gfx/capture.h"
#include "core/gfx/draw_census.h"
#include "core/gfx/frame_id.h"
#include "core/gfx/device_census.h"

#include "core/gfx/d3d9ex.h"
#include "game/dishonored/camera.h"

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

// ---- every function, so the bodies below can be in any order --------------
#include "mod/fwd.h"

// ---- function bodies by subsystem -------------------------------------------
#if !DVR_WITH_LEGACY
#include "legacy/legacy_stubs.inc"
#endif
#define DVR_CAT ::dvr::log::Cat::cfg
#include "core/config/config.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::present
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
#define DVR_CAT ::dvr::log::Cat::menu
#include "game/dishonored/game_state.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/arms_hide.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::hands
#include "game/dishonored/hands/fp_mesh.cpp"
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
#include "game/dishonored/scene_draw.cpp"
#include "game/dishonored/ue3/process_event.cpp"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::script
#include "game/dishonored/ue3/uobject.cpp"
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
