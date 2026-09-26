// Semantic HUD ownership copied with queued rendering. No retained engine objects.
#pragma once
#include <cstdint>
namespace dvr::hudowner {
struct Owner {
    int element=-1;
    uint32_t generation=0;
    uintptr_t root=0;
    bool marker=false;
    bool pivotValid=false;
    float pivot[2]{};
    explicit operator bool() const { return root!=0 && generation!=0; }
};
void configure(bool enabled);
bool enabled();
bool active();
Owner current();
void note_route(bool identified);
}
