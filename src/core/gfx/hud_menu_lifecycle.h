#pragma once
#include <cstdint>
namespace dvr::hudlayout {
// Keep the wheel's visual owner through native closing and the delayed capture.
// This owns only HUD pixels, never input or the stereo/scene permission.
struct WheelVisualLease {
    bool wasWheel=false,tail=false;uint32_t released=0;
    bool update(bool riding,int context,bool closing,uint32_t frame) {
        if((riding && context!=6) || context==1 || context==2 || context==9) {*this=WheelVisualLease{};return false;}
        if((riding && context==6) || closing){wasWheel=true;tail=false;return true;}
        if(wasWheel){wasWheel=false;tail=true;released=frame;}
        if(tail && frame-released<=3) return true;
        tail=false;return false;
    }
};
}
