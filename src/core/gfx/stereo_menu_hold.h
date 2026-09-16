#pragma once
namespace dvr::stereo {
struct MenuGapHold {
    double lastStereoMs=-1;
    bool hold(int delivered,bool menu,double now) {
        if(delivered) {lastStereoMs=now;return false;}
        return menu && lastStereoMs>=0 && now>=lastStereoMs && now-lastStereoMs<150;
    }
    void clear() {lastStereoMs=-1;}
};
}
