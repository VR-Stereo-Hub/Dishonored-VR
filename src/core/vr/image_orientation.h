#pragma once
#include "pose_record.h"
#include <cmath>

namespace dvr::pose {
// VR-116: a successful capture carries its camera-input record. Only rotate
// metadata from that same eye; never turn a missing record into identity.
inline bool image_orientation(const Record* r,int eye,float* q) {
    if(!r || !r->id || (eye!=-1 && eye!=1) || r->eye!=eye ||
       !r->track.ok || !r->cam.ok || !r->track.gen) return false;
    const float a[4]={r->track.qx,r->track.qy,r->track.qz,r->track.qw};
    float n=0;for(float v:a) { if(!std::isfinite(v)) return false;n+=v*v; }
    if(n<0.99f || n>1.01f) return false;
    n=std::sqrt(n);for(int i=0;i<4;++i) q[i]=a[i]/n;
    return true;
}
}
