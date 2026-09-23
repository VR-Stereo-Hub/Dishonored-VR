// tools/installer/ui/screens.h - the three screens. Pure ImGui: read the
// ViewState, change `choices` and the UI flags, return what the player asked
// for. No file is touched in here.
#pragma once
#include "model/view_state.h"

namespace dvr::setup::ui {

// Draws the whole root window (title, the current screen, the footer) and
// returns the action, None when nothing was clicked.
UiAction draw(ViewState& v);

} // namespace dvr::setup::ui
