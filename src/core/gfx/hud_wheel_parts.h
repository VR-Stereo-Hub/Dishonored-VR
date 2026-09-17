#pragma once
#include <cmath>
namespace dvr::wheelparts {
// Corner envelopes for the controller layout. Its scripts anchor the controls
// to the safe-area bottom while their artwork retains a pixel aspect ratio.
// These are adjustable candidate bounds, not GFx instance identification.
inline bool crop(unsigned part,unsigned width,unsigned height,const float* box,float* out) {
    if(part>1 || !width || !height) return false;
    for(int i=0;i<4;++i) if(!std::isfinite(box[i])) return false;
    // box = left, right, bottom (UV), height as a fraction of image width.
    if(box[0]<0 || box[1]>1 || box[1]-box[0]<.01f || box[2]<=0 || box[2]>1 || box[3]<=0) return false;
    out[0]=box[0];out[2]=box[1];out[3]=box[2];
    out[1]=std::fmax(0.f,box[2]-box[3]*width/height);
    return out[3]-out[1]>.01f;
}
}
