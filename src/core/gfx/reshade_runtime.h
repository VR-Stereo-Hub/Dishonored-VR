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
bool performance_mode();
bool set_performance_mode(bool enabled);
// Render-thread only. Never retain effect/uniform handles across frames or reloads.
reshade::api::effect_runtime* api();
const wchar_t* directory();
inline thread_local bool inside = false; // bypass game classifiers during our graphics calls
void render(IDirect3DDevice9* device);
void reset();
}
