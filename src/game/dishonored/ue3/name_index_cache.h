#pragma once
#include <cstdint>
#include <cstring>

namespace dvr::ue3 {
// Positive hints only: IDs, never engine pointers or negative results. A hit
// is re-read from GNames; replacement or recycled IDs cannot name another object.
// Full buckets fall back to the original scan. Storage and probes are bounded.
template<unsigned Capacity = 65536> class NameIndexCache {
    struct Entry { uint32_t hash = 0, index = 0; } entries_[Capacity]{};
    static uint32_t hash(const char* text) {
        uint32_t h = 2166136261u;
        for (; *text; ++text) h = (h ^ static_cast<unsigned char>(*text)) * 16777619u;
        return h;
    }
public:
    template<class Read> uint32_t find(const char* name, Read read) const {
        const uint32_t h = hash(name);
        for (unsigned n = 0; n < 16 && n < Capacity; ++n) {
            const Entry& e = entries_[(h + n) % Capacity];
            if (!e.index) break;
            if (e.hash != h) continue;
            const char* actual = read(e.index);
            if (actual && !std::strcmp(actual, name)) return e.index;
        }
        return ~uint32_t(0);
    }
    void remember(const char* name, uint32_t index) {
        if (!index) return;
        const uint32_t h = hash(name);
        for (unsigned n = 0; n < 16 && n < Capacity; ++n) {
            Entry& e = entries_[(h + n) % Capacity];
            if (!e.index || (e.hash == h && e.index == index)) {
                e = {h, index};
                return;
            }
        }
    }
};
}
