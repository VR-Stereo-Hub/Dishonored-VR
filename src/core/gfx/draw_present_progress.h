#pragma once
#include <stdint.h>
namespace dvr::stereo {
// VR-229: rendering inside the previous draw is progress too. Sampling at
// its return discards that progress and can inject a center-eye single draw.
// Game-thread only; a genuinely unchanged present counter still refuses.
struct DrawPresentProgress {
    uint32_t previousEntry = 0, previousReturn = 0;
    bool advanced = false, outsideAdvanced = false;
    void begin(uint32_t present) {
        advanced = present != previousEntry;
        outsideAdvanced = present != previousReturn;
        previousEntry = present;
    }
    void complete(uint32_t present) { previousReturn = present; }
};
}
