#pragma once
#include <d3d9.h>
namespace reshade::api { struct effect_runtime; }
namespace dvr::reshade_runtime {
// The optional manual runtime owns effects only; desktop_eye still owns presentation.
void load_optional();
bool manual();
bool installed();
bool controls_supported();
bool enabled_next_start();
bool set_enabled_next_start(bool enabled);
bool performance_mode();              // what the running runtime read, not what was last asked
bool set_performance_mode(bool enabled);
// [ReShade] LoadAllEffects. Off (default): only the selected preset's effects are loaded.
bool load_all_effects();
bool set_load_all_effects(bool enabled);
// Set when a setting changed in F10 could not be applied without a restart, or nullptr.
const char* live_note();
// Why ReShade is not running although it was installed and enabled for this launch, or
// nullptr. Set by load_optional and by a failed manual runtime creation; F10 shows it.
const char* load_failure();
// Render-thread only. Never retain effect/uniform handles across frames or reloads.
reshade::api::effect_runtime* api();
const wchar_t* directory();
inline thread_local bool inside = false; // bypass game classifiers during our graphics calls
void render(IDirect3DDevice9* device);
void reset();
}
