#pragma once
#include <d3d9.h>
namespace dvr::reshade_runtime {
// The optional manual runtime owns effects only; desktop_eye still owns presentation.
void load_optional();
bool manual();
inline thread_local bool inside = false; // bypass game classifiers during our graphics calls
void render(IDirect3DDevice9* device);
void reset();
}
