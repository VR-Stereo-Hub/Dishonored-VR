// core/gfx/hud_capture.h - the HUD's pixels, redirected into sinks (VR-117;
// the abandoned PR #12's hud_capture, generalised from one target to N).
//
// core/gfx/hud_class recognises a HUD-class draw and asks core/gfx/hud_layout
// which SINK it belongs to (one per element group: the whole HUD, or health,
// the menu, ...). begin()/end() bind that sink's private A8R8G8B8 target
// around the draw instead of the backbuffer; at Present every sink in use is
// blitted into one of its two shared surfaces (fenced both ways: a D3D9 event
// after the blit, a D3D11 event after the read, and a D3D11 event never
// completes until the context is FLUSHED), opened on the mod's D3D11 device,
// alpha-repaired (alpha = max(r,g,b): the target clears to black and the HUD
// draws over it, so black is "nothing there" and the compositor gets
// premultiplied coverage) into an R8G8B8A8 texture, and handed to the runtime
// layer's HUD quads through hud_layout's provider. The slot delivered is the
// PREVIOUS one, never the one just written.
//
// Two rules the archaeology paid for and this module keeps:
//   - the clear happens AFTER the copy, every present, unconditionally. The
//     fork cleared lazily at the first redirected draw, and Dishonored's HUD
//     hides elements when they are full, so a carried body's icon stayed on the
//     wrist for minutes after it was dropped.
//   - the gate is the GAME STATE, never the draw. The pause menu and the power
//     wheel are drawn by the same class (measured), so a draw-only gate sweeps
//     the menu onto the panel - the original's inherited bug, HANDOFF 8.4.
//
// Waits are budgeted with QueryPerformanceCounter: GetTickCount's 15 ms tick
// cannot measure a 10 ms budget. Every default-pool object is released in
// on_reset() (38.63: a forgotten one makes the game's Reset fail forever).
#pragma once
#include <stdint.h>
#include "core/gfx/hud_marker.h"
#include <windows.h>
#include <d3d9.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
namespace dvr::status { class Writer; }

namespace dvr::hudcap {

// [Hud] Panel / `hud on|off`. Refuses when patterns.h has no measured fingerprint.
bool enabled();
void set_enabled(bool on);

// [Hud] SlotScale: a sink's texture is the frame's size times this. The window
// subtends about 50 degrees, so half the render's height is already more than
// the headset can show; smaller is cheaper to copy every present.
void  set_slot_scale(float s);
float slot_scale();

// The game side's half of the gate, published once per tick: `arm` = the
// scene verdict (the world is drawing) and no power wheel held; `menuOverride`
// = an in-game screen is riding the window, so the redirect must run although
// the runtime's own gate (an eye-tagged projection present) may be quiet.
void set_game_gate(bool arm, bool menuOverride);

// True while the redirect should run this present. The draw path tests this first.
bool armed();
// Prevent an old screen image from being presented under a new HUD owner.
void invalidate_content();

// Around one HUD-class draw. begin() binds sink `sink`'s target and re-applies
// the viewport SetRenderTarget just reset (the device is PURE: the viewport
// comes from the classifier's shadow); end() puts the game's target and
// viewport back. Neither keeps a reference to an engine object past the call.
bool begin(IDirect3DDevice9* dev, const D3DVIEWPORT9& vp, int sink);
void end(IDirect3DDevice9* dev, IDirect3DSurface9* gameRt, const D3DVIEWPORT9& vp);

// Present: for every sink in use, copy, clear, deliver. Called from the frame
// path BETWEEN the stereo method's end_frame and the runtime's on_present_end,
// so it belongs to no stereo method.
void end_frame(IDirect3DDevice9* dev9, ID3D11Device* dev11, ID3D11DeviceContext* ctx11);

// A sink's delivered texture this present, or null (the gated read the
// provider uses); and the ungated one for `dump hud [sink]`.
ID3D11Texture2D* sink_texture(int sink, ID3D11DeviceContext* ctx);
ID3D11Texture2D* panel_texture(int sink);
ID3D11Texture2D* wheel_part_texture(int sink,int part);
ID3D11Texture2D* vitals_part_texture(int sink,int part);   // VR-142: 0 health, 1 mana
// VR-142: the part as a D3D9 texture for the in-scene draw (the previous present's
// HUD), its screen-fraction rectangle, the split half-plane and its size.
bool vitals_scene_texture(int part, IDirect3DTexture9** tex, float rect[4], float hp[3], unsigned* w, unsigned* h);
void note_marker(int sink,const float* rect);
const dvr::hudmarker::Regions* marker_regions(int sink);

// For the ride predicate (game/dishonored/ue3/ui_surface.cpp): the redirect
// is up and drawing (a sink at the backbuffer's size, the repair pass
// compiled, a redirected draw within the last 500 ms), and whether a D3D
// failure has latched this session (the ride then falls back to the mono
// screen rather than show a window with nothing on it).
// Successful intentional gameplay bypass, distinct from redirected draws.
void note_native_reference(HRESULT result);
bool redirect_healthy();
bool redirect_failed();

void on_reset();
void shutdown();

void log_status();
void status(dvr::status::Writer& w);
bool command(const char* args);   // `hud on|off|status|scale <f>` (the layout words are hud_layout's)

} // namespace dvr::hudcap
