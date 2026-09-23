// tools/installer/model/fake_states.h - named ViewStates for --render, so every
// screen can be drawn and judged without a game, a headset or a click.
#pragma once
#include <string>
#include <vector>
#include "model/view_state.h"

namespace dvr::setup {

std::vector<std::string> fake_state_names();
bool fake_state(const std::string& name, ViewState* out);

} // namespace dvr::setup
