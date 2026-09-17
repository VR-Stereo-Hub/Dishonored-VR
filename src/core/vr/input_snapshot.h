#pragma once

namespace dvr::vr {

// The raw controller state the pad bridge composes from. `active` false =
// no data this frame (no session, not FOCUSED, actions unbound).
struct InputSnapshot {
    float mv[2] = {0, 0};      // left thumbstick x, y (XR: +y forward)
    float lk[2] = {0, 0};      // right thumbstick
    float trigL = 0, trigR = 0;
    float gripL = 0, gripR = 0;
    bool  a = false, b = false, x = false, y = false;
    bool  clkL = false, clkR = false;   // stick clicks
    bool  menu = false;                 // physical left menu button
    bool  restL = false, restR = false; // capacitive thumbrest touch
    bool  active = false;
};

} // namespace dvr::vr
