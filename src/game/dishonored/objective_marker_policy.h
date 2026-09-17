#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::objectivemarkers {
// Only visible offscreen task markers move. Preserve the ray from screen center.
inline bool inset_position(float& x,float& y,int w,int h,uint32_t flags,float inset) {
    if((flags&9)!=9 || !std::isfinite(x) || !std::isfinite(y) ||
       !std::isfinite(inset) || inset<.05f || inset>.30f || w<64 || h<64 || w>16384 || h>16384) return false;
    const float dx=x-w*.5f,dy=y-h*.5f;
    float t=1;
    if(std::fabs(dx)>w*(.5f-inset)) t=w*(.5f-inset)/std::fabs(dx);
    if(std::fabs(dy)>h*(.5f-inset)) t=std::fmin(t,h*(.5f-inset)/std::fabs(dy));
    if(!(t<1)) return false;
    x=w*.5f+dx*t;y=h*.5f+dy*t;return true;
}
bool enabled();
float inset();
void configure(bool on,float margin);
}
