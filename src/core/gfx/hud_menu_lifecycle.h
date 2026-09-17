#pragma once
#include <cstdint>
namespace dvr::hudlayout {
// Keep the wheel's visual owner through native closing and the delayed capture.
// This owns only HUD pixels, never input or the stereo/scene permission.
struct WheelVisualLease {
    bool wasWheel=false,tail=false;uint32_t released=0;double releasedMs=0;
    bool update(bool riding,int context,bool closing,uint32_t frame,double nowMs=0,bool timed=false) {
        if((riding && context!=6) || context==1 || context==2 || context==9) {*this=WheelVisualLease{};return false;}
        if(riding && context==6){wasWheel=true;tail=false;return true;}
        if(!timed) {
            if(closing){wasWheel=true;tail=false;return true;}
            if(wasWheel){wasWheel=false;tail=true;released=frame;}
        } else {
            // Exported native movie closes wheel/background/potions over250ms.
            // Input release starts that interval; closing observations extend
            // only the delayed-present tail, never input or scene permission.
            if(wasWheel){wasWheel=false;tail=true;released=frame;releasedMs=nowMs;}
            if(tail && closing){released=frame;return true;}
            if(tail && nowMs>=releasedMs && nowMs-releasedMs<250) return true;
        }
        if(tail && frame-released<=3) return true;
        tail=false;return false;
    }
};
}
