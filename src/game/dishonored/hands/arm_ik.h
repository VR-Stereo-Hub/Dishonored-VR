// Pure full-arm math. Coordinates share one orthonormal solve frame and unit.
// Reach/pole design follows the MIT BioShock left-hand fork; see ARM_IK.md.
#pragma once
#include "hand_frame.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace dvr::ik {
using hf::Mat3;
using hf::Xform;
struct Vec {
    float x=0, y=0, z=0;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec operator*(float k) const { return {x*k,y*k,z*k}; }
};
inline float dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float length(Vec a){return sqrtf(dot(a,a));}
inline bool finite(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline Vec vec(const float* p){return {p[0],p[1],p[2]};}
inline void put(Vec a,float* p){p[0]=a.x;p[1]=a.y;p[2]=a.z;}
inline bool unit(Vec& a){float n=length(a);if(!std::isfinite(n)||n<1e-6f)return false;a=a*(1/n);return true;}
inline Vec rotate(const Mat3& m,Vec v){float p[3],r[3];put(v,p);hf::mulv3(m,p,r);return vec(r);}
inline Vec point(const Xform& m,Vec v){return rotate(m.r,v)+vec(m.t);}
inline Vec across(Vec v,Vec axis){return v-axis*dot(v,axis);}
inline Vec fallback(Vec axis){
    Vec v=fabsf(axis.x)<0.6f?Vec{1,0,0}:Vec{0,1,0};
    v=across(v,axis);unit(v);return v;
}
inline Mat3 frame(Vec direction,Vec normal){
    unit(direction);normal=across(normal,direction);
    if(!unit(normal))normal=fallback(direction);
    Vec side=cross(normal,direction);
    return {{direction.x,side.x,normal.x,direction.y,side.y,normal.y,direction.z,side.z,normal.z}};
}
inline Mat3 frame_delta(Vec refDirection,Vec refNormal,Vec direction,Vec normal){
    return hf::mul3(frame(direction,normal),hf::transpose3(frame(refDirection,refNormal)));
}
inline Mat3 axis_angle(Vec a,float angle){
    if(!unit(a))return hf::identity3();
    float c=cosf(angle),s=sinf(angle),v=1-c;
    return {{c+a.x*a.x*v,a.x*a.y*v-a.z*s,a.x*a.z*v+a.y*s,
             a.y*a.x*v+a.z*s,c+a.y*a.y*v,a.y*a.z*v-a.x*s,
             a.z*a.x*v-a.y*s,a.z*a.y*v+a.x*s,c+a.z*a.z*v}};
}
inline float wrap(float a){return std::remainder(a,6.28318530718f);}
inline float unwrap(float a,float previous){return previous+wrap(a-previous);}

// A reference skin matrix maps mesh space to the solved space. Its translation
// is determined by an actual reference joint, never a skin-weight centroid.
inline Xform skin(Mat3 rotation,float scale,Vec reference,Vec target){
    Xform out{};out.r=rotation;for(float& v:out.r.m)v*=scale;
    put(target-rotate(out.r,reference),out.t);return out;
}
struct Settings {
    float forwardCm=-6, rightCm=0, upCm=-20, widthCm=36;
    float lengthScale=1, elbowOut=0.6f;
};
struct Body {
    Vec origin,forward,right,up;
};
inline Mat3 tracking_to_local(const Mat3& localToWorld,const Mat3& viewBasis,const Mat3& head){
    Mat3 flip=hf::identity3();flip.m[8]=-1;
    return hf::mul3(hf::transpose3(localToWorld),hf::mul3(viewBasis,hf::mul3(flip,hf::transpose3(head))));
}
inline Vec shoulder(const Body& b,const Settings& s,int hand,float unitsPerCm){
    return b.origin+(b.forward*s.forwardCm+b.right*(s.rightCm+(hand?1.f:-1.f)*s.widthCm*.5f)+b.up*s.upCm)*unitsPerCm;
}
struct Solution {Vec shoulder,elbow,wrist,pole;float shoulderShift=0;};
// No global state: the caller supplies a pole from this arm's body-frame history.
inline bool solve(Vec nominal,Vec wrist,Vec pole,Vec outward,Vec priorPole,
                  float upper,float lower,float margin,Solution& out){
    if(!finite(nominal)||!finite(wrist)||!finite(pole)||!finite(outward)||!finite(priorPole)||
       !std::isfinite(upper)||!std::isfinite(lower)||!std::isfinite(margin)||
       upper<=1e-4f||lower<=1e-4f||margin<0)return false;
    Vec n=wrist-nominal;float raw=length(n);
    if(!unit(n)){n=pole;if(!unit(n))n={0,0,-1};}
    float high=(upper+lower)*.995f;
    float low=std::max(fabsf(upper-lower)*1.05f+margin,(upper+lower)*.4f);
    if(low>=high)return false;
    float d=std::clamp(raw,low,high);
    Vec s=wrist-n*d;
    Vec p=across(pole,n);
    // Body-relative history dominates only near pole singularities.
    if(length(p)<.25f*length(pole))p=p+across(priorPole,n)*(.3f*length(pole));
    if(!unit(p)){p=across(outward,n);if(!unit(p))p=fallback(n);}
    Vec o=across(outward,n);
    if(unit(o)){float v=dot(p,o);if(v<.25f){p=p+o*(.25f-v);unit(p);}}
    float cosine=std::clamp((upper*upper+d*d-lower*lower)/(2*upper*d),-1.f,1.f);
    Vec e=s+n*(upper*cosine)+p*(upper*sqrtf(std::max(0.f,1-cosine*cosine)));
    out={s,e,wrist,p,length(s-nominal)};
    return finite(e)&&finite(s);
}

// The wrist's skin rotation already cancels the authored inverse bind. Compare
// it to the solved forearm skin rotation, not to an assumed bone-local up axis.
inline float twist(const Mat3& forearm,const Mat3& wrist,Vec axis){
    Mat3 delta=hf::mul3(wrist,hf::transpose3(forearm));const float* m=delta.m;
    float q[4]{};float trace=m[0]+m[4]+m[8];
    if(trace>0){float s=sqrtf(trace+1)*2;q[3]=s*.25f;q[0]=(m[7]-m[5])/s;q[1]=(m[2]-m[6])/s;q[2]=(m[3]-m[1])/s;}
    else {
        int i=m[4]>m[0]?1:0;if(m[8]>m[i*3+i])i=2;int j=(i+1)%3,k=(i+2)%3;
        float s=sqrtf(std::max(0.f,1+m[i*3+i]-m[j*3+j]-m[k*3+k]))*2;
        if(s<1e-6f)return 0;
        q[i]=s*.25f;q[j]=(m[j*3+i]+m[i*3+j])/s;q[k]=(m[k*3+i]+m[i*3+k])/s;q[3]=(m[k*3+j]-m[j*3+k])/s;
    }
    float v=dot({q[0],q[1],q[2]},axis);
    if(fabsf(v)+fabsf(q[3])<1e-6f)return 0;
    return wrap(2*atan2f(v,q[3]));
}
struct BodyYaw {
    float yaw=0;bool valid=false;
    void reset(){valid=false;}
    float update(float headYaw,float seconds){
        if(!valid){yaw=headYaw;valid=true;return yaw;}
        float error=wrap(headYaw-yaw),limit=25.f/57.2957795f;
        yaw+=error-std::clamp(error,-limit,limit);
        yaw+=wrap(headYaw-yaw)*(1-expf(-std::clamp(seconds,0.f,.1f)/1.5f));
        yaw=wrap(yaw);return yaw;
    }
};
struct PoseFrame {
    bool valid=false,fresh=false,written[2]{};
    uint32_t gen=0;
    float yaw=0,twist[2]{};
    Vec prior[2];
};
struct PoseHistory {
    BodyYaw yaw;
    PoseFrame frames[8];
    uint32_t next=0,lastGen=0;
    uint32_t advanced=0,reused=0,old=0;
    double stamp=0;
    Vec prior[2];float twist[2]{};
    PoseFrame& acquire(uint32_t gen,float headYaw,double nowMs){
        for(auto& f:frames)if(f.valid&&f.gen==gen){++reused;return f;}
        bool newer=!stamp||(int32_t)(gen-lastGen)>0;
        bool fresh=newer&&stamp&&nowMs>=stamp&&nowMs-stamp<250;
        auto& f=frames[next++%8];f=PoseFrame{};f.valid=true;f.gen=gen;f.fresh=fresh;
        if(newer){
            ++advanced;
            if(!fresh)yaw.reset();
            f.yaw=yaw.update(headYaw,fresh?(float)(nowMs-stamp)*.001f:0);
            for(int h=0;h<2;++h){f.prior[h]=fresh?prior[h]:Vec{};f.twist[h]=fresh?twist[h]:0;}
            lastGen=gen;stamp=nowMs;
        }else {++old;f.yaw=headYaw;} // an old queued view cannot rewind current history
        return f;
    }
    void commit(PoseFrame& f,int hand,Vec bodyPole,float angle){
        if(hand<0||hand>1||f.written[hand])return;
        if(f.gen==lastGen){prior[hand]=bodyPole;twist[hand]=angle;}
        f.written[hand]=true;
    }
};
} // namespace dvr::ik
