#pragma once
#include <stdint.h>
namespace dvr::stereo {
// VR-229: rendering inside the previous draw is progress too. Sampling at
// its return discards that progress and can inject a center-eye single draw.
// Game-thread only. One unchanged interval is allowed after observed progress:
// the render thread can lag behind one game tick. A second quiet interval refuses.
struct DrawPresentProgress {
    uint32_t previousEntry = 0, previousReturn = 0;
    bool advanced = false, outsideAdvanced = false;
    bool observedProgress = false, allowed = false;
    unsigned quietEntries = 0;
    void begin(uint32_t present) {
        advanced = present != previousEntry;
        outsideAdvanced = present != previousReturn;
        if (advanced) { observedProgress = true; quietEntries = 0; }
        else if (quietEntries < 2) ++quietEntries;
        allowed = advanced || (observedProgress && quietEntries == 1);
        previousEntry = present;
    }
    void complete(uint32_t present) { previousReturn = present; }
};
}
