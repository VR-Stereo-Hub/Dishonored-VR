#pragma once
#include <cstring>
#include <cctype>
#include <cmath>
#include "hands/hand_frame.h"
namespace dvr::anim {
inline bool fresh(unsigned long long stamp,unsigned long long now) { return stamp && now>=stamp && now-stamp<=150; }
inline bool listed(const char* list, const char* name) {
    if (!name || !*name) return false;
    while (*list) {
        while (*list == ',' || std::isspace((unsigned char)*list)) ++list;
        const char* end = list;
        while (*end && *end != ',') ++end;
        const char* trimmed = end;
        while (trimmed > list && std::isspace((unsigned char)trimmed[-1])) --trimmed;
        if (size_t(trimmed-list) == std::strlen(name) && !std::strncmp(list,name,trimmed-list)) return true;
        list = end;
    }
    return false;
}
struct Handoff {
    bool game = false, releasing = false;
    unsigned long long releaseAt = 0, blendAt = 0;
    float from = 1, target = 1;
    float value(unsigned long long now, unsigned duration) const {
        if (!duration || now >= blendAt + duration) return target;
        const float t = now > blendAt ? float(now-blendAt)/duration : 0;
        return from + (target-from)*t;
    }
    void update(bool valid, bool match, bool enabled, unsigned long long now,
                unsigned releaseMs, unsigned blendMs) {
        if (!valid || !enabled) { *this = Handoff{}; return; }
        if (match) { game = true; releasing = false; }
        else if (game) {
            if (!releasing) { releasing = true; releaseAt = now; }
            if (now-releaseAt >= releaseMs) { game = false; releasing = false; }
        }
        const float next = game ? 0.0f : 1.0f;
        if (next != target) { from = value(now,blendMs); target = next; blendAt = now; }
    }
};
// Slerp the proper rotation from identity and linearly interpolate uniform
// scale/translation. Refuse shear/reflection instead of collapsing a limb.
inline hf::Xform blend_transform(const hf::Xform& input, float weight) {
    if (weight>=1) return input;
    hf::Xform out={hf::identity3(),{0,0,0}};
    if (weight<=0) return out;
    const float scale=sqrtf(input.r.m[0]*input.r.m[0]+input.r.m[3]*input.r.m[3]+input.r.m[6]*input.r.m[6]);
    if (!(scale>0.0001f) || !std::isfinite(scale)) return out;
    hf::Mat3 r=input.r;
    for(int i=0;i<9;++i) r.m[i]/=scale;
    if (!hf::is_rotation(r,0.02f)) return out;
    float q[4]={};
    const float trace=r.m[0]+r.m[4]+r.m[8];
    if(trace>0) {
        const float s=sqrtf(trace+1)*2; q[3]=s/4;
        q[0]=(r.m[7]-r.m[5])/s; q[1]=(r.m[2]-r.m[6])/s; q[2]=(r.m[3]-r.m[1])/s;
    } else {
        int i=r.m[4]>r.m[0]?1:0; if(r.m[8]>r.m[i*3+i]) i=2;
        const int j=(i+1)%3,k=(i+2)%3;
        const float s=sqrtf(1+r.m[i*3+i]-r.m[j*3+j]-r.m[k*3+k])*2;
        q[i]=s/4; q[j]=(r.m[j*3+i]+r.m[i*3+j])/s;
        q[k]=(r.m[k*3+i]+r.m[i*3+k])/s; q[3]=(r.m[k*3+j]-r.m[j*3+k])/s;
    }
    if(q[3]<0) for(float& v:q) v=-v;
    float norm=0; for(float v:q) norm+=v*v; norm=sqrtf(norm);
    for(float& v:q) v/=norm;
    const float angle=acosf(fminf(1.0f,fmaxf(0.0f,q[3])));
    const float mul=angle>0.00001f?sinf(angle*weight)/sinf(angle):weight;
    const float x=q[0]*mul,y=q[1]*mul,z=q[2]*mul,w=cosf(angle*weight);
    out.r={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),
            2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),
            2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
    for(int i=0;i<9;++i) out.r.m[i]*=1+(scale-1)*weight;
    for(int i=0;i<3;++i) out.t[i]=input.t[i]*weight;
    return out;
}
}


