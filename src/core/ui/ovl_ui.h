// core/ui/ovl_ui.h - VR-196: the F10 panel's view level, its theme and its drawing helpers.
//
// The panel shows three tiers. Basic is what a player tunes to their body and taste,
// Advanced is preference detail, and Debug is fixes that should stay on, A/B levers,
// instruments and readouts. Each tier shows everything below it. Sections are collapsing
// headers, closed by default; a control that is not self-explanatory carries a hover tip.
//
// The theme (VR-197) follows Dishonored's own look: ink-dark panels, bone-white text, brass
// accents like whale-oil lamplight, oxblood for the active state, a classical serif for
// titles, headings and body, and notes-style parchment tooltips.
//
// Its own translation unit because the sub-panels live in several (the unity build,
// hud_layout.cpp, aim_ray.cpp, openxr_runtime.cpp). ImGui and embedded D3D11 artwork, called from the overlay's
// draw callback (load_fonts and apply_theme from its one-time init).
#pragma once

#include "imgui.h"
struct ID3D11Device;
struct ImFont;

namespace dvr::ovl {
enum Tier { Basic = 0, Advanced = 1, Debug = 2 };
int level();                     // the current view, Basic by default
void set_level(int tier);        // clamps; the caller saves [Overlay] Level
const char* level_name(int tier);  // "basic" | "advanced" | "debug"
int parse_level(const char* s, int fallback);
inline bool show(int tier) { return level() >= tier; }
// A wrapped, parchment-styled hover description for the item just submitted (works on
// disabled items too).
void tip(const char* text);
// A collapsing header in the heading serif, closed by default, drawn only at `tier` and
// above; its tip describes the whole section. Returns true while open.
bool section(const char* name, int tier, const char* tipText, bool defaultOpen = false);

// ---- the theme (VR-197) ----
// The fonts: Times body/italic, Perpetua Titling Light title, with fallbacks loaded
// from the Windows font folder. A missing file falls back to ImGui's own font and says so.
void load_fonts();
// PNG artwork is embedded in the DLL; no game-directory assets or paths required.
void load_art(ID3D11Device* device);
void release_art();
void backdrop();
float body_footer_height();
void begin_body(const char* id);
void end_body();
void note(const char* text, float width = 0.0f);
bool button(const char* label, const ImVec2& size = ImVec2(0, 0));
bool checkbox(const char* label, bool* value);
bool radio_button(const char* label, int* value, int choice);
bool radio_button(const char* label, bool selected);
bool slider_float(const char* label, float* value, float min, float max, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
bool slider_int(const char* label, int* value, int min, int max, const char* format = "%d", ImGuiSliderFlags flags = 0);

void apply_theme();              // colours and metrics; call before ScaleAllSizes
ImFont* heading_font();          // null when no serif was found
bool tab(const char* label);     // BeginTabItem with the label in the heading serif
// The panel's title row: the name in the heading serif, left aligned, over a brass rule with a
// diamond at its middle.
void title(const char* text);
// A thin brass rule with a centre diamond, full width.
void ornament();
// Always-visible controller shortcut card, shared with the offscreen preview.
void controller_hint();
// A selectable pill (the view selector): brass when selected. Returns true when clicked.
bool pill(const char* label, bool selected, float requestedWidth = 0.0f);
// Push/pop the colours of the panel's primary action button (brass on ink).
void push_primary();
void pop_primary();
}
