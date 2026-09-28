#pragma once
#include "game/dishonored/anim_policy.h"
#include <cstdint>

namespace dvr::anim {
// Render-only entry translation. It is a WORLD VECTOR, not a point: eye and
// camera translations cancel. Native rotation and the authored trajectory stay.
struct OriginTranslation {
    uint64_t episode=0;
    unsigned source=0;
    bool ready=false, refused=false;
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
    hf::Xform local(const hf::Mat3& localToWorld) const {
        hf::Xform out={hf::identity3(),{0,0,0}};
        if(ready)for(int r=0;r<3;++r)for(int c=0;c<3;++c)
            out.t[r]+=localToWorld.m[c*3+r]*world[c];
        return out;
    }
    hf::Xform blend(const hf::Mat3& localToWorld,const hf::Xform& tracked,float weight) const {
        auto out=blend_transform(tracked,weight);
        const auto origin=local(localToWorld);
        for(int i=0;i<3;++i)out.t[i]+=origin.t[i]*(1-weight);
        return out;
    }
};
}
