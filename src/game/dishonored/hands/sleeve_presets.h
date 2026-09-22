// game/dishonored/hands/sleeve_presets.h - how much sleeve the drawn hands keep.
//
// A preset is the wrist cut ([Hands] WristCutA/B: mesh units from the hand bone,
// negative = up the arm) and the rounding of the cut end ([Hands]
// RoundedWristDepth). F10 > Hands > Sleeve picks one; the Sleeve length slider
// and the numpad knob make a Custom one. Pure, no engine.
#pragma once
#include <math.h>

namespace dvr::sleeve {

struct Preset { const char* name; float cut; float roundness; bool measured; };

// Hands: the cut every release has shipped (-4.90) with the shipped roundness.
// Cuffs and Forearm are PROVISIONAL until a headset run marks the tested
// lengths with V (the marker line prints the cut and roundness to bake here).
static const Preset kPresets[] = {
    { "Hands",   -4.90f, 0.640f, true  },
    { "Cuffs",   -7.50f, 0.300f, false },
    { "Forearm", -12.0f, 0.300f, false },
};
static const int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

// The preset the current values are, or -1 (Custom).
inline int match(float cutA, float cutB, float roundness) {
    for (int i = 0; i < kPresetCount; ++i)
        if (fabsf(cutA - kPresets[i].cut) < .05f && fabsf(cutB - kPresets[i].cut) < .05f &&
            fabsf(roundness - kPresets[i].roundness) < .005f) return i;
    return -1;
}

} // namespace dvr::sleeve
