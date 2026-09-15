#pragma once
#include <cstdint>
namespace dvr::bridge_profile {
inline bool interval_ms(uint64_t start,uint64_t end,uint64_t frequency,bool disjoint,double& ms) {
    if(disjoint || !frequency || end<start) return false;
    ms=double(end-start)*1000.0/double(frequency); return true;
}
struct SampleGate {
    uint32_t random=0x91e10da5; int chosen=-1;
    void present() { random^=random<<13;random^=random>>17;random^=random<<5;
        const auto n=random&15;chosen=n<2?int(n):-1; }
    bool take(int stage) { if(chosen!=stage) return false;chosen=-1;return true; }
};
struct SlotState {
    bool pending=false; uint64_t issued=0; uint32_t epoch=0;
    bool available() const { return !pending; }
    bool pollable(uint64_t now) const { return pending && now>=issued && now-issued>=8; }
    void issue(uint64_t now,uint32_t current) { pending=true; issued=now; epoch=current; }
    void retire() { pending=false; }
    bool current(uint32_t value) const { return epoch==value; }
};
}
