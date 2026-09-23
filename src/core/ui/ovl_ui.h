// core/ui/ovl_ui.h - VR-196: the F10 panel's view level, its theme and its drawing helpers.
//
// The panel shows three tiers. Basic is what a player tunes to their body and taste,
// Advanced is preference detail, and Debug is fixes that should stay on, A/B levers,
// instruments and readouts. Each tier shows everything below it. Sections are collapsing
// headers, closed by default; a control that is not self-explanatory carries a hover tip.
//
// The theme (VR-197) follows Dishonored's own look: ink-dark panels, bone-white text, brass
// accents like whale-oil lamplight, oxblood for the active state, a classical serif for
// titles and headings over a plain sans for the body, and notes-style parchment tooltips.
//
// Its own translation unit because the sub-panels live in several (the unity build,
// hud_layout.cpp, aim_ray.cpp, openxr_runtime.cpp). ImGui only, called from the overlay's
// draw callback (load_fonts and apply_theme from its one-time init).
#pragma once

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
// The fonts: Segoe UI for the body, Constantia (then Georgia, Palatino) for headings, loaded
// from the Windows font folder. A missing file falls back to ImGui's own font and says so.
void load_fonts();
void apply_theme();              // colours and metrics; call before ScaleAllSizes
ImFont* heading_font();          // null when no serif was found
bool tab(const char* label);     // BeginTabItem with the label in the heading serif
// The panel's title row: the name in the heading serif, centred, over a brass rule with a
// diamond at its middle.
void title(const char* text);
// A thin brass rule with a centre diamond, full width.
void ornament();
// Always-visible controller shortcut card, shared with the offscreen preview.
void controller_hint();
// A selectable pill (the view selector): brass when selected. Returns true when clicked.
bool pill(const char* label, bool selected);
// Push/pop the colours of the panel's primary action button (brass on ink).
void push_primary();
void pop_primary();
}
