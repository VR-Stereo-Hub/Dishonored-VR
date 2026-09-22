// core/ui/ovl_ui.h - VR-196: the F10 panel's view level and its three drawing helpers.
//
// The panel shows three tiers. Basic is what a player tunes to their body and taste,
// Advanced is preference detail, and Debug is fixes that should stay on, A/B levers,
// instruments and readouts. Each tier shows everything below it. Every control carries a
// hover description (tip), and every section is a collapsing header, closed by default.
//
// Its own translation unit because the sub-panels live in several (the unity build,
// hud_layout.cpp, aim_ray.cpp, openxr_runtime.cpp). ImGui only, called from the overlay's
// draw callback.
#pragma once

namespace dvr::ovl {
enum Tier { Basic = 0, Advanced = 1, Debug = 2 };
int level();                     // the current view, Basic by default
void set_level(int tier);        // clamps; the caller saves [Overlay] Level
const char* level_name(int tier);  // "basic" | "advanced" | "debug"
int parse_level(const char* s, int fallback);
inline bool show(int tier) { return level() >= tier; }
// A wrapped hover description for the item just submitted (works on disabled items too).
void tip(const char* text);
// A collapsing header, closed by default, drawn only at `tier` and above; its tip
// describes the whole section. Returns true while open.
bool section(const char* name, int tier, const char* tipText, bool defaultOpen = false);
}
