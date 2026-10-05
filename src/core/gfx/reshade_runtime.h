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
// Why ReShade is not running although it was installed and enabled for this launch, or
// nullptr. Set by load_optional and by a failed manual runtime creation; F10 shows it.
const char* load_failure();
// Render-thread only. Never retain effect/uniform handles across frames or reloads.
reshade::api::effect_runtime* api();
const wchar_t* directory();
inline thread_local bool inside = false; // bypass game classifiers during our graphics calls
// -1 = no running effect runtime; otherwise the state after the call. on < 0 only reads.
int set_effects(int on);
void render(IDirect3DDevice9* device);
void reset();
}
