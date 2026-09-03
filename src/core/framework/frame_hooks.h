// core/framework/frame_hooks.h - the D3D9 device hooks and the frame path (41.0).
//
// The proxy patches the game's IDirect3D9::CreateDevice, and through it the
// device's Present, Reset, SetVertexShaderConstantF and SetRenderTarget. This
// module owns those hooks and the ORDER of the frame path inside Present:
//
//   pre_tick            the seam poll, status.json, the game-state tick, the
//                       crash filter re-arm (every present, even when disabled)
//   [VR] FpsCap         the even-cadence limiter (XR sessions only)
//   vr::on_present_begin   bring-up, events, xrWaitFrame, xrBeginFrame, locate
//   stereo::begin_frame    the active method learns the head, picks the next eye
//   game_tick           the game side: head pose -> camera write, hands, the
//                       virtual gamepad, dumps, heartbeat, hotkeys
//   stereo::end_frame   the method turns the game's frame into the eye texture
//   vr::on_present_end  copy into the swapchain, layers, xrEndFrame
//
// Everything game-specific lives behind the callbacks (registered from the
// unity build: game/dishonored/present_tick.cpp); this file knows no engine
// address and no game global. All hooks run on the thread that calls Present.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace dvr::frame {

typedef HRESULT (__stdcall *SetVsConstFn)(IDirect3DDevice9*, UINT, const float*, UINT);
typedef HRESULT (__stdcall *SetRenderTargetFn)(IDirect3DDevice9*, DWORD, IDirect3DSurface9*);

struct Callbacks {
    void (*before_create_device)(D3DPRESENT_PARAMETERS* pp) = nullptr;   // may edit pp
    void (*after_create_device)(HRESULT hr, HWND wnd, D3DPRESENT_PARAMETERS* pp) = nullptr;
    void (*before_reset)(D3DPRESENT_PARAMETERS* pp) = nullptr;           // may edit pp
    void (*pre_tick)(IDirect3DDevice9* dev) = nullptr;
    void (*game_tick)(IDirect3DDevice9* dev) = nullptr;
    // Optional detours (they forward through orig_* below).
    SetVsConstFn      set_vs_const = nullptr;
    SetRenderTargetFn set_render_target = nullptr;
    // The mod's D3D11 device for the stereo methods (null = none; flat game).
    ID3D11Device* (*d3d11)(ID3D11DeviceContext** ctx) = nullptr;
    // The game side's verdict "this present is strict gameplay" (a live pawn,
    // no menu, no cinematic): published to the runtime every present while a
    // projection layer is claimed, so its cinematic quad fallback becomes the
    // menu/cutscene gate instead of a permanent trap (it trips on a STALE
    // publish). Null = always true.
    bool (*gameplay_verdict)() = nullptr;
};
void set_callbacks(const Callbacks& cb);

// Patch IDirect3D9::CreateDevice on the object Direct3DCreate9 returned.
bool hook_d3d9(IDirect3D9* d3d);

// The originals, for the registered detours.
HRESULT orig_set_vs_const(IDirect3DDevice9* dev, UINT startReg, const float* data, UINT count);
HRESULT orig_set_render_target(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* rt);

uint32_t count();              // presents so far (the frame number everything stamps)
uint32_t submit_count();       // presents that handed a texture to the runtime
void set_disabled(bool on);    // disable_vr.txt: hooks stay, the VR path does not run
bool disabled();
void set_exiting();            // PreExit: VR stands down, the session comes down next present
bool exiting();
void set_fps_cap(float fps);   // [VR] FpsCap (0 = off)
float fps_cap();
bool xr_live();                // the runtime session is live as of this present

} // namespace dvr::frame
