#pragma once
#include <cmath>
namespace dvr::hudmarker {
// Metadata follows the exact capture slot, including empty frames and failures.
constexpr int kMax=6;
struct Regions {
    int count=0;bool overflow=false;
    float rect[kMax][4]{};
    void add(const float* r) {
        if(!r) {overflow=true;return;}
        for(int j=0;j<4;++j) if(!std::isfinite(r[j])) {overflow=true;return;}
        if(r[0]<0 || r[1]<0 || r[2]>1 || r[3]>1 || r[2]<=r[0] || r[3]<=r[1]) {overflow=true;return;}
        for(int i=0;i<count;++i) {
            bool same=true;for(int j=0;j<4;++j) same=same && std::fabs(rect[i][j]-r[j])<.002f;
            if(same) return;
            if(r[0]<rect[i][2] && r[2]>rect[i][0] && r[1]<rect[i][3] && r[3]>rect[i][1]) {
                overflow=true;return; // inseparable pixels: retain complete panel
            }
        }
        if(count==kMax) {overflow=true;return;}
        for(int j=0;j<4;++j) rect[count][j]=r[j];
        ++count;
    }
};
struct Delivery {
    Regions drawing,slot[2],output;
    void copied(int current,bool success) {if(success) slot[current]=drawing;drawing=Regions{};}
    void delivered(int other) {output=slot[other];}
    void reset() {*this=Delivery{};}
};
inline void padded_crop(const float r[4],unsigned w,unsigned h,float out[4]) {
    const float dim[2]={(float)w,(float)h};
    for(int i=0;i<2;++i) {
        // Quantized extents avoid recreating an XR swapchain for subpixel jitter.
        float extent=std::ceil(((r[i+2]-r[i])*dim[i]+4.f)/16.f)*16.f;
        extent=std::fmin(extent,dim[i]);
        float start=std::floor((r[i]+r[i+2])*.5f*dim[i]-extent*.5f+.5f);
        start=std::fmax(0.f,std::fmin(start,dim[i]-extent));
        out[i]=start/dim[i];out[i+2]=(start+extent)/dim[i];
    }
}
inline bool placement(const float r[4],float dist,float tanH,float tanV,
                      float iconPanelWidth,float offset[2],float& width) {
    if(!(dist>.05f && tanH>0 && tanV>0 && iconPanelWidth>0)) return false;
    // Center movement spans the rendered frustum; icon size remains the user's
    // accepted panel pixel scale. Scaling an icon never scales its travel.
    offset[0]=(r[0]+r[2]-1)*dist*tanH;
    offset[1]=(1-r[1]-r[3])*dist*tanV;
    width=(r[2]-r[0])*iconPanelWidth;
    return std::isfinite(offset[0]) && std::isfinite(offset[1]) && std::isfinite(width);
}
inline bool separable(const Regions& regions,unsigned w,unsigned h) {
    if(regions.overflow || !w || !h) return false;
    for(int i=0;i<regions.count;++i) {
        float crop[4];padded_crop(regions.rect[i],w,h,crop);
        for(int j=0;j<regions.count;++j) if(i!=j) {
            const float* r=regions.rect[j];
            if(crop[0]<r[2] && crop[2]>r[0] && crop[1]<r[3] && crop[3]>r[1]) return false;
        }
    }
    return true;
}
inline bool cropped_placement(const float r[4],unsigned w,unsigned h,float dist,float tanH,float tanV,
                              float iconPanelWidth,float crop[4],float offset[2],float& width) {
    if(!w || !h || !placement(r,dist,tanH,tanV,iconPanelWidth,offset,width)) return false;
    padded_crop(r,w,h,crop);
    offset[0]+=(crop[0]+crop[2]-r[0]-r[2])*.5f*iconPanelWidth;
    offset[1]+=(r[1]+r[3]-crop[1]-crop[3])*.5f*iconPanelWidth*(float)h/w;
    width=(crop[2]-crop[0])*iconPanelWidth;
    return true;
}

}
