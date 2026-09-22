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

// Hands: the cut every release has shipped (-4.90) with the shipped roundness,
// kept at the tester's choice. Cuffs: the V-marked look of the 2026-09-22 run
// (MARKER #2, cut -10.00, roundness 0.570). Forearm: the length that run held
// before switching back (-26.40, roundness 0.570); its V line did not reach the log.
static const Preset kPresets[] = {
    { "Hands",   -4.90f, 0.640f, true },
    { "Cuffs",   -10.0f, 0.570f, true },
    { "Forearm", -26.4f, 0.570f, true },
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
