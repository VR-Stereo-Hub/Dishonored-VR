#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::hudnative {
inline bool square_icon(const float* r,unsigned vertices,unsigned primitives) {
    if(!r || vertices!=8 || primitives!=10) return false;
    const float w=r[2]-r[0],h=r[3]-r[1];
    return w>=.028f && w<=.065f && h>=.028f && h<=.065f && w/h>.85f && w/h<1.18f;
}
inline bool edge_icon(const float* r,float inset=.05f) {
    const float x=(r[0]+r[2])*.5f,y=(r[1]+r[3])*.5f;
    return std::fabs(x-inset)<.006f || std::fabs(x-(1-inset))<.006f ||
           std::fabs(y-inset)<.006f || std::fabs(y-(1-inset))<.006f;
}
// An edge-clamped sprite identifies a marker family. Keep that ownership as
// identical content moves through the interaction region; position is not identity.
struct MarkerLabels {
    struct Region {float rect[4]{};uint32_t frame=0;bool valid=false;} regions[16]{};
    unsigned next=0;
    void clear(){for(auto& r:regions)r=Region{};next=0;}
    void marker(const float* box,uint32_t frame) {
        for(auto& r:regions) if(r.valid && r.frame==frame && std::fabs(r.rect[0]-box[0])<.001f && std::fabs(r.rect[1]-box[1])<.001f) return;
        auto& r=regions[next++%16];for(int k=0;k<4;++k)r.rect[k]=box[k];r.frame=frame;r.valid=true;
    }
    bool label(const float* box,uint32_t frame,float* pivot) const {
        if(!box) return false;
        const float w=box[2]-box[0],h=box[3]-box[1];
        if(!(w>.008f && w<.35f && h>.003f && h<.075f && w>h*1.4f)) return false;
        const float x=(box[0]+box[2])*.5f,y=(box[1]+box[3])*.5f;
        const Region* best=nullptr;float bestD=1e9f;bool ambiguous=false;
        for(const auto& r:regions) {
            if(!r.valid || frame-r.frame>1) continue;
            const float rx=(r.rect[0]+r.rect[2])*.5f,ry=(r.rect[1]+r.rect[3])*.5f;
            // Native title/distance are short, horizontally centred runs above
            // or below their icon. Never absorb distant HUD or a full screen.
            const float dx=std::fabs(x-rx),dy=std::fabs(y-ry);
            if(dx>.045f || dy>.10f || box[2]<r.rect[0] || box[0]>r.rect[2]) continue;
            const float d=dx*dx+dy*dy;
            if(best && std::fabs(rx-(best->rect[0]+best->rect[2])*.5f)<.01f &&
               std::fabs(ry-(best->rect[1]+best->rect[3])*.5f)<.01f) {
                if(r.frame>best->frame){best=&r;bestD=d;}continue;
            }
            if(best && std::fabs(d-bestD)<.0004f) {ambiguous=true;continue;}
            if(d<bestD){best=&r;bestD=d;ambiguous=false;}
        }
        if(!best || ambiguous) return false;
        for(int k=0;k<4;++k)pivot[k]=best->rect[k];return true;
    }
};
struct Markers {
    struct Entry {uint64_t key=0;uint32_t seen=0;} entries[64]{};
    void clear(){for(auto& e:entries)e=Entry{};}
    bool observe(uint64_t key,uint32_t frame,const float* r,unsigned vertices,unsigned primitives,float inset=.05f) {
        if(!key || !r) return false;
        Entry* oldest=&entries[0];
        for(auto& e:entries) {
            if(e.key==key && frame-e.seen<=2400){e.seen=frame;return true;}
            if(!e.key || frame-e.seen>frame-oldest->seen) oldest=&e;
        }
        if(square_icon(r,vertices,primitives) && (edge_icon(r) || edge_icon(r,inset))){*oldest={key,frame};return true;}
        return false;
    }
};
// Reuse the symmetric perspective basis checks used by the hand draw reader.
// World +Z projected into the rendered camera gives upright screen direction.
inline bool upright_basis(const float* vp,float& co,float& si,float& aspect) {
    float r[3],u[3],f[3],rn=0,un=0,fn=0;
    for(int i=0;i<16;++i) if(!std::isfinite(vp[i])) return false;
    for(int i=0;i<3;++i){r[i]=vp[i*4];u[i]=vp[i*4+1];f[i]=vp[i*4+3];rn+=r[i]*r[i];un+=u[i]*u[i];fn+=f[i]*f[i];}
    rn=std::sqrt(rn);un=std::sqrt(un);fn=std::sqrt(fn);
    if(rn<1e-4f || un<1e-4f || std::fabs(fn-1)>.01f) return false;
    float ru=0,rf=0,uf=0;
    for(int i=0;i<3;++i){r[i]/=rn;u[i]/=un;ru+=r[i]*u[i];rf+=r[i]*f[i];uf+=u[i]*f[i];}
    if(std::fabs(ru)>.02f || std::fabs(rf)>.02f || std::fabs(uf)>.02f) return false;
    const float len=std::sqrt(r[2]*r[2]+u[2]*u[2]);
    if(len<.15f) return false; // looking nearly vertical: upright is singular
    co=u[2]/len;si=-r[2]/len;aspect=rn/un;
    return aspect>.1f && aspect<10;
}
inline bool transform_column(const float* src,const float* rect,float scale,float co,float si,float aspect,float* dst) {
    if(!(scale>=.25f && scale<=1) || !std::isfinite(co) || !std::isfinite(si) || !(aspect>.1f && aspect<10)) return false;
    const float cx=rect[0]+rect[2]-1,cy=1-rect[1]-rect[3];
    for(int i=0;i<4;++i) if(!std::isfinite(src[i])) return false;
    const float x=src[0]-cx*src[3],y=src[1]-cy*src[3];
    dst[0]=scale*(co*x-si*aspect*y)+cx*src[3];
    dst[1]=scale*(si*x/aspect+co*y)+cy*src[3];
    dst[2]=src[2];dst[3]=src[3];return true;
}
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
