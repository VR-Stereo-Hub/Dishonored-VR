#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::hudnative {
// Render-lane continuity for already identified small rune artwork. Fallback
// does not renew its lease or retain an image, location, or engine object.
struct RuneIconContinuity {
    struct Entry {uint64_t key=0;uint32_t frame=0,ms=0;} entries[32]{};
    void clear(){for(auto& e:entries)e=Entry{};}
    static bool icon(const float* r,unsigned vertices,unsigned primitives) {
        if(!r || vertices!=8 || primitives!=10)return false;
        for(int k=0;k<4;++k)if(!std::isfinite(r[k]))return false;
        const float w=r[2]-r[0],h=r[3]-r[1];
        return w>=.01f && h>=.01f && w<=.07f && h<=.07f && w/h>.8f && w/h<1.25f;
    }
    bool route(uint64_t key,uint32_t frame,uint32_t ms,const float* r,
               unsigned vertices,unsigned primitives,bool confirmed) {
        if(!key || !icon(r,vertices,primitives))return false;
        Entry* oldest=&entries[0];
        for(auto& e:entries){
            if(e.key==key){
                if(confirmed)e={key,frame,ms};
                return confirmed || (frame-e.frame<=2 && ms-e.ms<=100);
            }
            if(!e.key || frame-e.frame>frame-oldest->frame)oldest=&e;
        }
        if(confirmed){*oldest={key,frame,ms};return true;}
        return false;
    }
};
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
// VR-148: the awareness meter's positions, published by the native parent hook.
//
// Same numeric-snapshot contract as RunePositions: nothing here is an engine
// pointer, the token is only an identity, and the render thread never
// dereferences anything the callback borrowed.
//
// What is DIFFERENT is the tolerance. The rune matcher's window is tuned to
// artwork that was measured - 40 px icon, 48..62 px pulse, 64 px locator,
// description 31.65 px above. The awareness meter's artwork has NOT been
// measured, so inventing a tight window here would be shipping a guessed
// constant as a measured one. Instead the window is deliberately generous and
// `worst` records the largest accepted draw in authoring pixels, so the first
// run with an alerted guard on screen reports the real size and the window can
// be tightened against a measurement instead of against a hope.
struct AwarenessPositions {
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
    // VR-152: CONST, and it records nothing. It used to mutate its own census
    // counters, which forced every caller to hold the writer's lock. The render
    // thread now matches against a private per-frame copy and reports what it
    // accepted through `out`, so the per-draw lock is gone entirely.
    struct Accepted { float w=0,h=0,dx=0,dy=0; bool ambiguous=false; };
    bool match(const float* r,uint32_t ms,float targetW,float targetH,float* pivot,
               Accepted* out=nullptr) const {
        if(!r || targetW<1 || targetH<1) return false;
        for(int k=0;k<4;++k)if(!std::isfinite(r[k]))return false;
        const float rw=r[2]-r[0],rh=r[3]-r[1];
        if(rw<=0 || rh<=0)return false;
        const Point* best=nullptr;float bestD=1e9f,bx=0,by=0,bw=0,bh=0,bdx=0,bdy=0;
        for(const auto& p:points){
            if(!p.token || !p.visible || ms-p.ms>100)continue;
            // Scaleform fits the 1280x720 authoring canvas inside the target.
            const float scale=std::fmin(targetW/p.w,targetH/p.h);
            const float sx=scale/targetW,sy=scale/targetH;
            const float x=.5f+(p.x-p.w*.5f)*sx,y=.5f+(p.y-p.h*.5f)*sy;
            const float cx=(r[0]+r[2])*.5f,cy=(r[1]+r[3])*.5f;
            const float dx=(cx-x)/sx,dy=(cy-y)/sy;
            const float pw=rw/sx,ph=rh/sy;
            // An awareness meter is a small badge on its enemy's head. 160 px
            // of extent and 96 px of offset is wide enough to hold it plus a
            // fade pulse without reaching the next enemy; it is NOT a claim
            // about the artwork.
            if(pw>160 || ph>160 || std::fabs(dx)>96 || std::fabs(dy)>96)continue;
            const float d=dx*dx+dy*dy;
            // Two published markers equally close cannot both own the draw.
            if(best && std::fabs(d-bestD)<1) {if(out)out->ambiguous=true;return false;}
            if(d<bestD){best=&p;bestD=d;bx=x;by=y;bw=pw;bh=ph;bdx=dx;bdy=dy;}
        }
        if(!best)return false;
        if(out){out->w=bw;out->h=bh;out->dx=bdx;out->dy=bdy;}
        pivot[0]=pivot[2]=bx;pivot[1]=pivot[3]=by;return true;
    }
};
}
