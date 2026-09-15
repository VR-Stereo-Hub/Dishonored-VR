// core/gfx/hud_class.h - the Scaleform HUD draw class: the rule, the census and
// the region probe (VR-117; the census is the abandoned PR #12's draw_census).
//
// Dishonored draws its world into an offscreen scene target and paints the
// whole HUD onto the BACKBUFFER at the tail of the frame (ENGINE_NOTES, "The
// Scaleform HUD draw class, measured"). The rule that names a HUD draw has four
// terms, all measured and all required: rt0 is the backbuffer (or unset), the
// viewport covers the whole target at the origin, depth is off, and the draw
// BLENDS - the fourth term is what leaves the scene resolve (the one opaque
// full-frame draw) writing the world where it belongs.
//
// The device is PURE (no Get* for state), so every value the rule needs is
// shadowed from its setter: this module patches SetViewport, SetRenderState,
// SetTexture, SetVertexDeclaration, SetVertexShader, SetPixelShader, the two
// user-pointer draws and SetStreamSource, and takes the two vertex-buffer
// draws through frame_hooks' INNER draw seam, so it sits innermost of the
// chain (game -> frame_hooks -> the hand census -> here -> D3D): a draw the
// hand census drops never reaches the redirect, and the census decides on the
// game's own state. SetRenderTarget stays frame_hooks' and is reported here.
//
// Three levers, each default OFF except as noted:
//   [Draws] Census   the bucket table and the VERDICT every 3 s (`draws on|off`)
//   [Hud]   Regions  the region probe: each HUD draw's screen rectangle, from
//                    its vertices (user-pointer draws read the pointer; vertex
//                    buffer draws lock the buffer READONLY when it was not
//                    created write-only) transformed by the shadowed vertex
//                    shader constants c0..c3 when the position is not already
//                    pre-transformed. That rectangle is what routes a draw to
//                    an ELEMENT (core/gfx/hud_layout). Off = every draw is "all".
//   the redirect     armed by core/gfx/hud_capture; this module only asks it.
//
// Render-thread only. Checked per present: a Present from another thread than
// the last draw (the game parks its render thread on a focus loss and presents
// from the game thread) is logged and counted, not refused, because the two
// hand off rather than race. No lock anywhere in here.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

namespace dvr::status { class Writer; }

namespace dvr::hudclass {

// hkCreateDevice, after the creation census: patch the setter and UP-draw
// slots and register the inner draw hooks. Idempotent per device.
void install(IDirect3DDevice9* dev);

// Once per present from hkPresent (after pre_tick): closes the present's
// record, refreshes the backbuffer identity, checks the thread assumption,
// prints the 3 s summaries.
void present_tick(IDirect3DDevice9* dev);
void on_reset();
void shutdown();

// SetRenderTarget is frame_hooks'; it reports RT0 here. Pointer value only.
void on_set_render_target(DWORD idx, IDirect3DSurface9* rt);

// The game side lends two counters: the engine's viewport-draw count (one per
// re-entry pass) and the PostRender dispatch count (the event the HUD draws
// from). Their ratio says whether the HUD is drawn once per tick or per eye.
void set_game_counters(uint32_t (*viewportDraws)(), uint32_t (*postRenderDispatches)());

// [Draws] Census / `draws on|off`.
bool census_enabled();
void set_census_enabled(bool on);
// [Hud] Regions / `hud regions on|off`: the region probe and the routing it feeds.
bool regions_enabled();
void set_regions_enabled(bool on);

void log_summary(const char* why);          // the census table and VERDICT
void log_regions(const char* why);          // the region table (HUD-class buckets with their rectangles)
void status(dvr::status::Writer& w);        // status.json "draws"
bool command(const char* args);             // `draws on|off|status|regions|kill <key>|hud|unkill`

} // namespace dvr::hudclass
