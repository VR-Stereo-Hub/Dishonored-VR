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
// VR-178: camera evidence belongs to one uninterrupted riding-menu interval.
struct MenuSceneFreshness {
    double uploaded=-1;
    int context=-1;
    uint32_t epoch=0, load=0;
    void begin(bool eligible,int nextContext,uint32_t nextEpoch,uint32_t nextLoad) {
        if(!eligible || context!=nextContext || epoch!=nextEpoch || load!=nextLoad) uploaded=-1;
        context=eligible ? nextContext : -1;
        epoch=nextEpoch; load=nextLoad;
    }
    void complete(uint32_t before,uint32_t after,double now) {
        if(context>=3 && context<=8 && before!=after) uploaded=now;
    }
    bool recent(bool enabled,int currentContext,uint32_t currentEpoch,uint32_t currentLoad,
                bool headLook,double now) const {
        return enabled && headLook && context>=3 && context<=8 && context==currentContext &&
            epoch==currentEpoch && load==currentLoad && uploaded>=0 && now>=uploaded && now-uploaded<100;
    }
};
}
