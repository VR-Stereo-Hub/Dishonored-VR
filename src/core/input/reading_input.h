#pragma once
#include <cstdint>
namespace dvr::reading_input {
inline bool continuous(int context,bool wheel,bool active) {
    return active && !wheel && (context==4 || context==5);
}
// Pause (3) and the main menu (1) take the native axes: the game repeats and
// accelerates a held stick itself. VR-102: the main menu was left on the stepped
// pulses, so its save list crawled while the same list in the pause menu was fast.
inline bool pause(int context,bool wheel,bool active) {return active && !wheel && (context==3 || context==1);}
// The incoming axis is already deadzone-shaped. Do not pulse or amplify it:
// the game's native reader owns scroll speed/repeat acceleration.
inline int16_t vertical(int16_t shaped) {return shaped;}
}
