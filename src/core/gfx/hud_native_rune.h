#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::hudnative {
// Numeric snapshots only. The native callback validates each borrowed instance;
// no engine pointer is dereferenced or trusted later on the render thread.
struct RunePositions {
    struct Point {uintptr_t token=0;float x=0,y=0,w=0,h=0;uint32_t ms=0;bool visible=false;};
    Point points[32]{};
    void clear(){for(auto& p:points)p=Point{};}
    void update(uintptr_t token,float x,float y,int w,int h,uint32_t flags,uint32_t ms) {
        Point* slot=&points[0];
        for(auto& p:points){
            if(p.token==token){slot=&p;break;}
            if(!p.token || ms-p.ms>ms-slot->ms)slot=&p;
        }
        const bool visible=(flags&1) && w>=64 && h>=64 && std::isfinite(x) && std::isfinite(y);
        *slot={token,x,y,(float)w,(float)h,ms,visible};
    }
    bool match(const float* r,uint32_t ms,float targetW,float targetH,float* pivot) const {
        if(!r || targetW<1 || targetH<1) return false;
        for(int k=0;k<4;++k)if(!std::isfinite(r[k]))return false;
        const float rw=r[2]-r[0],rh=r[3]-r[1];
        if(rw<=0 || rh<=0)return false;
        const Point* best=nullptr;float bestD=1e9f,bx=0,by=0;
        for(const auto& p:points){
            if(!p.token || !p.visible || ms-p.ms>100)continue;
            // Scaleform fits the 1280x720 authoring canvas inside the target.
            const float scale=std::fmin(targetW/p.w,targetH/p.h);
            const float sx=scale/targetW,sy=scale/targetH;
            const float x=.5f+(p.x-p.w*.5f)*sx,y=.5f+(p.y-p.h*.5f)*sy;
            const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f;
            const float dx=(cx-x)/sx,dy=(cy-y)/sy;
            // Flash: 40px art, 48..62px pulse, 64px locator; description
            // centered 31.65px above, background 172x50px, font21 + shadow.
            // 12px allowance covers callback/draw phase and shadow edges.
            const bool icon=rw/sx<=88 && rh/sy<=88 && std::fabs(dx)<=24 && std::fabs(dy)<=48;
            const bool label=rw/sx<=624 && rh/sy<=66 && rw>rh*1.4f &&
                std::fabs(dx)<=48 && dy>=-76 && dy<=12;
            if(!icon && !label)continue;
            const float d=dx*dx+dy*dy;
            if(best && std::fabs(d-bestD)<1) return false;
            if(d<bestD){best=&p;bestD=d;bx=x;by=y;}
        }
        if(!best)return false;
        pivot[0]=pivot[2]=bx;pivot[1]=pivot[3]=by;return true;
    }
};
}
