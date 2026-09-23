// tools/installer/app/offscreen.h - draw a ViewState into a BMP with no window,
// the way tools/ovl-theme-preview.cpp renders the F10 sample: a D3D11 render
// target (WARP when there is no GPU), four frames so the layout settles, a
// staging copy. This is how every screen is checked without a click.
#pragma once
#include <string>
#include "model/view_state.h"

namespace dvr::setup::app {

constexpr int kLogicalWidth = 760, kLogicalHeight = 640;

// scale is the DPI factor (1.0 = 96 dpi, 1.5 = 144 dpi). Returns false with why.
bool render_offscreen(ViewState& state, float scale, const std::wstring& outBmp, std::string* why);

} // namespace dvr::setup::app
