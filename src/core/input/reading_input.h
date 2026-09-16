#pragma once
#include <cstdint>
namespace dvr::reading_input {
inline bool continuous(int context,bool wheel,bool active) {
    return active && !wheel && (context==4 || context==5);
}
// The incoming axis is already deadzone-shaped. Do not pulse or amplify it:
// the game's native reader owns scroll speed/repeat acceleration.
inline int16_t vertical(int16_t shaped) {return shaped;}
}
