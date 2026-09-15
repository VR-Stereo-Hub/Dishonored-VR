#pragma once
#include <stdint.h>
namespace dvr::native_profile {
// Render-thread-owned. Random sampling avoids locking onto alternating eye/draw phases.
struct Stats {
    uint64_t calls=0, samples=0;
    double sumMs=0, maxMs=0;
    uint32_t random=0x9e3779b9u;
    bool visit() {
        ++calls;
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        return (random & 63u) == 0;
    }
    void record(double ms) { ++samples; sumMs+=ms; if(ms>maxMs) maxMs=ms; }
    double mean() const { return samples ? sumMs/samples : 0; }
    void clear() { calls=samples=0; sumMs=maxMs=0; } // keep RNG progressing across windows
};
}
