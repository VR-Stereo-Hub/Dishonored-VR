// core/gfx/draw_census.h - what a Dishonored present is made of (41.2, session 10).
//
// The wrist HUD 38.92 shipped was separated from the frame by the DXVK fork,
// which classified every DrawPrimitiveUP and DrawIndexedPrimitiveUP against a
// ladder of rejects and redirected the survivors (ENGINE_NOTES, "The HUD and
// the cinematics: what the original did"). The game renders natively now, so
// before anything can be redirected the question has to be asked again on THIS
// path, and nothing in this project has ever measured Dishonored's Scaleform
// draws: which entry points the HUD uses, whether a stable fingerprint exists,
// and whether it separates HUD from world with NO overlap.
//
// This module is that instrument. It patches the draw calls and the Set* calls
// whose state the classifier needs (the device is PURE, so no Get* answers for
// state and every value must be shadowed from the setter), buckets each
// present's draws, and prints a 3 s summary ending in a VERDICT line that can
// print the unwelcome answer: "NO CLEAN SEPARATOR". `draws kill` then suppresses
// a class so the picture says whether the class was the HUD - the project's rule
// for identifying a render pass is to make it MOVE.
//
// Default OFF ([Draws] Census=0, `draws on|off|status`, the F10 Display
// tickbox): the hooks install always and forward with one bool test until the
// lever is on. Render-thread only: draws, Present, the seam poll and Reset all
// run on the thread that presents, so there is no lock in here - and the thread
// assumption is CHECKED per present, not assumed.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

namespace dvr::status { class Writer; }

namespace dvr::draws {

// hkCreateDevice, after the creation census: patch the draw and state slots.
// Idempotent per device (PatchVtable refuses a re-patch).
void install(IDirect3DDevice9* dev);

// Once per present from hkPresent (after pre_tick): closes the present's
// record, checks the render-thread assumption, prints the 3 s summary.
void present_tick(IDirect3DDevice9* dev);

// hkReset: every cached pointer identity dies with the device's resources.
void on_reset();

// The game is going away: stop tracking, drop the caches.
void shutdown();

// SetRenderTarget is already hooked by frame_hooks; it reports RT0 here
// instead of this module patching the same slot twice. Pointer value only:
// no reference is ever taken on an engine D3D object inside a detour.
void on_set_render_target(DWORD idx, IDirect3DSurface9* rt);

// [Draws] Census / `draws on|off`. Refuses (loudly, with the values) when a
// hook is missing or the draws are not on the presenting thread.
bool enabled();
void set_enabled(bool on);

// The window's table and its VERDICT, at Info; `why` names the trigger.
void log_summary(const char* why);
// status.json "draws" object.
void status(dvr::status::Writer& w);
// The `draws` seam word: "on", "off", "kill <key>|hud", "unkill", anything else
// prints the summary now.
bool command(const char* args);

} // namespace dvr::draws
