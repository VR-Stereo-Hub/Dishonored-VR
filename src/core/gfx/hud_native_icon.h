#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::hudnative {
inline bool square_icon(const float* r,unsigned vertices,unsigned primitives) {
    if(!r || vertices!=8 || primitives!=10) return false;
    const float w=r[2]-r[0],h=r[3]-r[1];
    return w>=.028f && w<=.065f && h>=.028f && h<=.065f && w/h>.85f && w/h<1.18f;
}
inline bool edge_icon(const float* r) {
    const float x=(r[0]+r[2])*.5f,y=(r[1]+r[3])*.5f;
    return std::fabs(x-.05f)<.006f || std::fabs(x-.95f)<.006f ||
           std::fabs(y-.05f)<.006f || std::fabs(y-.95f)<.006f;
}
// An edge-clamped sprite identifies a marker family. Keep that ownership as
// identical content moves through the interaction region; position is not identity.
struct Markers {
    struct Entry {uint64_t key=0;uint32_t seen=0;} entries[64]{};
    void clear(){for(auto& e:entries)e=Entry{};}
    bool observe(uint64_t key,uint32_t frame,const float* r,unsigned vertices,unsigned primitives) {
        if(!key || !r) return false;
        Entry* oldest=&entries[0];
        for(auto& e:entries) {
            if(e.key==key && frame-e.seen<=2400){e.seen=frame;return true;}
            if(!e.key || frame-e.seen>frame-oldest->seen) oldest=&e;
        }
        if(square_icon(r,vertices,primitives) && edge_icon(r)){*oldest={key,frame};return true;}
        return false;
    }
};
// Scale projected geometry about its own center; keep clip w/depth unchanged.
inline bool scale_column(const float* src,const float* rect,float scale,float* dst) {
    if(!(scale>=.25f && scale<=1)) return false;
    const float cx=rect[0]+rect[2]-1,cy=1-rect[1]-rect[3];
    for(int i=0;i<4;++i) if(!std::isfinite(src[i])) return false;
    dst[0]=scale*src[0]+(1-scale)*cx*src[3];
    dst[1]=scale*src[1]+(1-scale)*cy*src[3];
    dst[2]=src[2];dst[3]=src[3];return true;
}
}
