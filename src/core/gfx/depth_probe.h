// core/gfx/depth_probe.h - where does the game keep its scene depth? (the first step of
// docs/dishonored/PLAN-motion-vectors-dlss.md). READ-ONLY: nothing the game draws changes.
//
// Hypothesis: UE3 on D3D9 keeps linear scene depth in the ALPHA of its floating-point
// scene colour target (the exe has no INTZ/RAWZ depth-texture path). The probe remembers
// every floating-point render-target texture the game creates at eye size, and a few times
// a run reads a 5x5 grid of texels from each (small StretchRect copies, one readback) and
// logs colour and alpha. Depth predicts: alpha positive, small where the hands are (lower
// middle), large on distant walls, and changing when the player walks toward something.
// A target whose alpha is constant, zero or 1.0 everywhere fails the hypothesis, and says so.
#pragma once
#include <windows.h>
#include <d3d9.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace dvr::depthprobe {

// device_census's CreateTexture hook: every texture the game creates passes through.
void note_texture(IDirect3DTexture9* tex, UINT w, UINT h, DWORD usage, D3DFORMAT fmt);
// Once per game Present (render thread), before any of our own writers.
void tick(IDirect3DDevice9* dev, UINT backW, UINT backH);
// Release every reference the probe holds (Reset, exit): they are DEFAULT-pool objects.
void on_reset();
void set_enabled(bool on, const char* who);
bool enabled();
void request(const char* who);   // one probe at the next present
bool command(const char* args);  // `depthprobe [on|off|now]`, `depthprobe share on|off`

// Step 2: the depth SHARED to our D3D11 device. Each present the scene target (the eye-size
// RGBA16F whose alpha is depth) is copied into a D3D9 texture opened on D3D11, fenced by an
// event query. Every 5 s the same present is read both ways (D3D9 from the game's target,
// D3D11 from the shared copy) and the two grids compared: the transport is proven when they
// agree to the texel. Off by default; [Diagnostics] DepthShare.
void share_tick(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx11, UINT backW, UINT backH);
void set_share(bool on, const char* who);
bool share_on();

} // namespace dvr::depthprobe
