#pragma once
#include <cstdint>
namespace dvr::stereo {
// The previous draw's uploads are evidence too, even if no upload arrived
// in the idle interval after it. Never authorize an unobserved or stale scene.
struct PauseSceneFreshness {
    double uploaded=-1;
    void complete(uint32_t before,uint32_t after,double now) {if(before!=after) uploaded=now;}
    bool recent(bool enabled,int context,bool headLook,double now) const {
        return enabled && context==3 && headLook && uploaded>=0 && now>=uploaded && now-uploaded<100;
    }
    void clear(){uploaded=-1;}
};
}
