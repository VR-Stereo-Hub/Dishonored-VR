// core/gfx/hud_capture.h - the HUD on a head-locked panel (41.2, session 10).
//
// Build 38.92 shipped a wrist HUD whose pixels the DXVK fork separated from the
// frame; 41.0 removed the fork and the HUD went with it. The measurement that
// makes a port possible is in ENGINE_NOTES, "The Scaleform HUD draw class":
// Dishonored draws its world into an offscreen scene target and paints the whole
// HUD onto the BACKBUFFER at the tail of the frame, so the render target alone
// separates the two, with no overlap, proven by picture.
//
// This module takes that class. core/gfx/draw_census classifies each draw and
// calls begin()/end() around the ones that qualify, which bind a private
// offscreen target instead of the backbuffer; at Present the target is blitted
// into a shared surface, cleared, opened on the mod's D3D11 device and handed to
// the runtime layer's head-locked HUD quad through set_hud_texture_provider.
//
// Default OFF ([Hud] Panel=0, `hud on|off|status|scale <f>`, the F10 Display
// tickbox). It REFUSES, loudly and with its values, when the fingerprint in
// patterns.h is not marked measured. While it runs, the HUD is not in the eye
// textures and not in the desktop window: that is what a redirect means.
//
// Two rules the archaeology paid for and this module keeps:
//   - the clear happens AFTER the copy, every present, unconditionally. The
//     fork cleared lazily at the first redirected draw, and Dishonored's HUD
//     hides elements when they are full, so a carried body's icon stayed on the
//     wrist for minutes after it was dropped.
//   - the gate is the GAME STATE, never the draw. The pause menu is drawn by the
//     same class (measured), so a draw-only gate would sweep the menu onto the
//     panel - the original's inherited bug, HANDOFF-GINGASVR 8.4.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
namespace dvr::status { class Writer; }

namespace dvr::hudcap {

// [Hud] Panel / `hud on|off`. Refuses when the fingerprint is not measured.
bool enabled();
void set_enabled(bool on);

// [Hud] SlotScale: the panel's texture is the frame's size times this. The
// panel subtends about 50 degrees, so half the render's height is already more
// than the headset can show; smaller is cheaper to copy every present.
void  set_slot_scale(float s);
float slot_scale();

// The game side's half of the gate, published once per present: strict gameplay
// (a live pawn, no menu, no main menu, no cinematic, a live view pipeline) and
// no power wheel held. The other half is the runtime's own gate (dvr::hud::gate).
void set_game_gate(bool on);

// True while the redirect should run. The draw path tests this first.
bool armed();

// Called by the classifier around a HUD-class draw. begin() binds the private
// target and restores the viewport SetRenderTarget just reset; end() puts the
// game's target and viewport back. Neither keeps a reference to an engine
// object beyond the call.
bool begin(IDirect3DDevice9* dev, const D3DVIEWPORT9& vp);
void end(IDirect3DDevice9* dev, IDirect3DSurface9* gameRt, const D3DVIEWPORT9& vp);

// Present: copy the target, clear it, hand the result to the runtime. Called
// from the frame path between the stereo method's end_frame and the runtime's
// on_present_end, so it is method-independent.
void end_frame(IDirect3DDevice9* dev9, ID3D11Device* dev11, ID3D11DeviceContext* ctx11);

// The panel's texture whatever the gate says, for `dump hud`. The provider
// below is the gated version the runtime calls.
ID3D11Texture2D* panel_texture();

// The provider the runtime layer calls (null = no HUD this present).
ID3D11Texture2D* provider_texture(ID3D11DeviceContext* ctx);

void on_reset();
void shutdown();

void log_status();
void status(dvr::status::Writer& w);
bool command(const char* args);   // `hud on|off|status|scale <f>`

} // namespace dvr::hudcap
