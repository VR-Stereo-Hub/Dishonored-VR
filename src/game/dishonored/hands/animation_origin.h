#pragma once
#include "game/dishonored/anim_policy.h"
#include <cstdint>

namespace dvr::anim {
// Render-only entry translation. It is a WORLD VECTOR, not a point: eye and
// camera translations cancel. Native rotation and the authored trajectory stay.
struct OriginTranslation {
    uint64_t episode=0;
    unsigned source=0;
    bool ready=false, refused=false, locked=false;
    int entryEye=0;
    uint64_t sampleFrame=~uint64_t(0);
    float lastWeight=1;
    float world[3]{};
    void sync(uint64_t id,unsigned generation,bool valid) {
        if(id!=episode) { *this=OriginTranslation{}; episode=id; source=generation; }
        if(!valid || (ready && source!=generation)) { ready=false; refused=true; }
        if(!ready) source=generation;
    }
    bool capture(const hf::Mat3& localToWorld,const hf::Xform& tracked,
                 const float* nativePalm) {
        if(!episode || ready || refused || !hf::basis_is_orthonormal(localToWorld,.02f))return false;
        float moved[3];hf::apply_point(tracked,nativePalm,moved);
        float delta[3];for(int i=0;i<3;++i)delta[i]=moved[i]-nativePalm[i];
        for(int r=0;r<3;++r) {
            world[r]=0;for(int c=0;c<3;++c)world[r]+=localToWorld.m[r*3+c]*delta[c];
            if(!std::isfinite(world[r])) {refused=true;return false;}
        }
        ready=true;return true;
    }
    // State entry can precede the first authored palette. During the existing
    // entry blend, keep the right palm at its tracked target while rotation
    // transfers. Freeze the translation at the first fully native sample.
    // One eye and one draw per frame author it; other passes only consume it.
    bool sample(const hf::Mat3& basis,const hf::Xform& tracked,const float* palm,
                float weight,int eye,uint64_t frame,bool entering) {
        if(refused || locked || !eye)return false;
        if(ready && (!entering || weight>lastWeight+.0001f)) {locked=true;return false;}
        if(!entering || (ready && (eye!=entryEye || frame==sampleFrame)))return false;
        OriginTranslation next=*this;next.ready=false;
        if(!next.capture(basis,tracked,palm))return false;
        *this=next;entryEye=eye;sampleFrame=frame;lastWeight=weight;
        locked=weight<=.0001f;return true;
    }
    hf::Xform local(const hf::Mat3& localToWorld) const {
        hf::Xform out={hf::identity3(),{0,0,0}};
        if(ready)for(int r=0;r<3;++r)for(int c=0;c<3;++c)
            out.t[r]+=localToWorld.m[c*3+r]*world[c];
        return out;
    }
    hf::Xform blend(const hf::Mat3& localToWorld,const hf::Xform& tracked,float weight,const float* palm) const {
        auto out=blend_transform(tracked,weight);
        const auto origin=local(localToWorld);
        if(weight>=1)return tracked;
        if(weight<=0)return origin;
        float target[3];hf::apply_point(tracked,palm,target);
        for(int i=0;i<3;++i) {
            const float desired=(palm[i]+origin.t[i])*(1-weight)+target[i]*weight;
            float rotated=0;for(int j=0;j<3;++j)rotated+=out.r.m[i*3+j]*palm[j];
            out.t[i]=desired-rotated;
        }
        return out;
    }
};
}
