#pragma once
#include <cstdint>
namespace dvr::drop {
// Discovery may be slow; reading an already found attack decision must not be.
struct DiscoverySchedule {
    uintptr_t pawn = 0;
    uint32_t scan = 0, count = 0;
    uint64_t next = 0;
    bool begin(uint64_t now, uintptr_t player, uint32_t size) {
        if (player != pawn || size < count) { scan = 0; next = 0; }
        if (size > count) next = 0; // newly loaded objects can contain our context
        pawn = player; count = size;
        if (now < next) return false;
        next = now + 20;
        return true;
    }
    void exhausted(uint64_t now) { scan = 0; next = now + 1000; }
    void invalidate() { next = 0; }
};
}
