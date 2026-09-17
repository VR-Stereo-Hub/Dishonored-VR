#pragma once
#include <cmath>
namespace dvr::wrist {
// A shallow ellipsoidal closure. t=0 preserves the exact source seam;
// t=1 is one shared tip, in the removed-arm direction. Own geometry only.
inline bool point(const float* rim,const float* center,const float* axis,float depth,
                  float t,float* out,float* normal) {
    if(!std::isfinite(depth) || depth<=0 || !std::isfinite(t) || t<0 || t>1) return false;
    float radius2=0,axis2=0;
    for(int k=0;k<3;++k) {
        if(!std::isfinite(rim[k]) || !std::isfinite(center[k]) || !std::isfinite(axis[k])) return false;
        const float d=rim[k]-center[k];radius2+=d*d;axis2+=axis[k]*axis[k];
    }
    if(radius2<1e-10f || std::fabs(axis2-1)>0.01f) return false;
    const float radius=std::sqrt(radius2),theta=t*1.57079632679f;
    const float c=t==1 ? 0.f : std::cos(theta),s=std::sin(theta);
    float n2=0;
    for(int k=0;k<3;++k) {
        const float radial=rim[k]-center[k];
        out[k]=center[k]+radial*c-axis[k]*depth*s;
        normal[k]=radial*c/(radius*radius)-axis[k]*s/depth;n2+=normal[k]*normal[k];
    }
    if(n2<1e-10f) return false;
    const float inverse=1/std::sqrt(n2);
    for(int k=0;k<3;++k) normal[k]*=inverse;
    return true;
}
}
