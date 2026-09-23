// tools/installer/ui/widgets.h - the launcher's few building blocks on top of
// the F10 theme (core/ui/ovl_ui.h): the page header, a status slot of fixed
// height so a long path wraps inside it instead of pushing the choices around,
// a step row with a brass or oxblood mark, and the footer whose buttons never
// move. Colours come from the theme's own style slots, not from a second copy
// of the palette.
#pragma once
#include "imgui.h"
#include "model/installer.h"

namespace dvr::setup::ui {

ImVec4 col_bone();      // ImGuiCol_Text
ImVec4 col_faded();     // ImGuiCol_TextDisabled
ImVec4 col_brass();     // ImGuiCol_SliderGrab
ImVec4 col_brass_hi();  // ImGuiCol_CheckMark
ImVec4 col_oxblood();   // the primary action's button colour

// "Dishonored VR" over the brass rule, then a subtitle line in faded ink.
void page_header(const char* subtitle);
// A collapsing section in the heading serif, open by default.
bool heading(const char* name, const char* tip = nullptr, bool defaultOpen = true);
// Wrapped text inside a child of exactly `lines` text lines; longer text scrolls.
void status_slot(const char* id, int lines, const char* text, const ImVec4* colour = nullptr);
void wrapped(const char* text);
void wrapped_faded(const char* text);
// A small diamond in the margin: brass for Ok, hollow for Skipped, brass-dim for
// Warn, oxblood for Failed. Advances the cursor onto the same line.
void mark(StepStatus status);
// A step's title in bone and its detail in faded ink, wrapped.
void step_row(const StepResult& step);
// A row of pills; returns the index clicked, or -1.
int pill_row(const char* const* labels, int count, int selected, const char* const* tips = nullptr);
// A full-width notice in brass on umber (a Debug build, a legacy payload).
void banner(const char* text);
// A spinning brass arc with a label, centred in the remaining space.
void spinner(const char* text);
// Moves the cursor to the bottom so `height` pixels remain, draws the ornament.
void footer_begin(float height);
// Buttons laid out from the right edge: call for the rightmost first.
bool footer_button(const char* label, bool primary = false, bool enabled = true);
bool button(const char* label, bool primary = false, bool enabled = true, float width = 0.0f);
// One line; a path too wide for the room is shown from its tail with a tooltip.
void path_text(const char* text);

} // namespace dvr::setup::ui
