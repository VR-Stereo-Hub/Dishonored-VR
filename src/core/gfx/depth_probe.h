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
bool command(const char* args);  // `depthprobe [on|off|now]`

} // namespace dvr::depthprobe
