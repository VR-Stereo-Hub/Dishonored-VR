// core/gfx/sampler_force.h - texture filtering levers on the game's D3D9 sampler
// states (the SetSamplerState detour frame_hooks installs). Both ship OFF: off,
// every call passes through untouched.
//
//   Anisotropy     raise the anisotropic degree of the samplers the game ALREADY
//                  makes anisotropic (its texture groups default to aniso with
//                  [SystemSettings] MaxAnisotropy=4). Floors and walls seen at a
//                  grazing angle, which VR looks at constantly, keep their detail
//   TrilinearMips  on those same samplers, blend between mip levels (the groups'
//                  default is a POINT mip filter, which draws a visible seam where
//                  the level changes and walks with the head)
//
// Only samplers whose MINFILTER the game set to ANISOTROPIC are touched, so
// render targets, lookup tables and the post-process chain never are.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

namespace dvr::samplers {

typedef HRESULT (__stdcall *PFN_SetSamplerState)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

void set_anisotropy(int degree, const char* who);   // 0 = the game's own, else 2..16
int  anisotropy();
void set_trilinear(bool on, const char* who);
bool trilinear();

// The detour body; `orig` is the device's own SetSamplerState.
HRESULT set_sampler_state(IDirect3DDevice9* dev, DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value,
                          PFN_SetSamplerState orig);
void on_reset();    // a Reset returns every sampler state to its default
void log_status(const char* why);
bool command(const char* args);   // `aniso <0|2|4|8|16>`, `aniso trilinear on|off`, `aniso` = status

} // namespace dvr::samplers
