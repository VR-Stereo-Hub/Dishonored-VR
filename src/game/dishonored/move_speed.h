// game/dishonored/move_speed.h - VR-204: the read-only crouched-walk speed trace.
#pragma once
#include <cstdint>

namespace dvr::movespeed {
void configure(const char* ini);
// Script lane; `pawn` is the validated possessed player pawn or null.
void sample(uint8_t* ctrl, uint8_t* pawn);
}
