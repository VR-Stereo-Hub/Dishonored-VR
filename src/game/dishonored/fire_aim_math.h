// Pure crossbow launch geometry. No engine objects or writes.
#pragma once
#include "game/dishonored/aim_ray.h"
namespace dvr::fireaim {
struct Solution { float origin[3] = {}, target[3] = {}, direction[3] = {}; };
inline float dot(const float* a, const float* b) { return a[0]*b[0]+a[1]*b[1]+a[2]*b[2]; }
inline bool finite3(const float* v) {
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}
inline bool normalize(float* v) {
    const float n = std::sqrt(dot(v,v));
    if (!std::isfinite(n) || n < 0.001f) return false;
    for (int i=0;i<3;++i) v[i]/=n;
    return true;
}
inline void cross(const float* a,const float* b,float* r) {
    r[0]=a[1]*b[2]-a[2]*b[1];r[1]=a[2]*b[0]-a[0]*b[2];r[2]=a[0]*b[1]-a[1]*b[0];
}
inline bool solve(const aim::FireFrame& f, uint64_t now, float yaw, float pitch,
                  const float* camera, float scale, const float* spawn, Solution& out) {
    if (!f.ray.ok || !f.headValid || !f.ray.gen || !f.ray.sampleMs ||
        now < f.ray.sampleMs || now-f.ray.sampleMs > 100 ||
        !std::isfinite(yaw) || !std::isfinite(pitch) || !std::isfinite(scale) ||
        scale < 1 || scale > 400 || !std::isfinite(f.distanceM) ||
        f.distanceM < 0.5f || f.distanceM > 50 || !finite3(camera) || !finite3(spawn) ||
        !finite3(f.headPos) || !finite3(f.ray.originXr) || !finite3(f.ray.dirXr)) return false;
    float q[4], n=0;
    for (int i=0;i<4;++i) { q[i]=f.headQuat[i]; n+=q[i]*q[i]; }
    if (!std::isfinite(n) || n<0.25f || n>4) return false;
    for (float& x:q) x/=std::sqrt(n);
    const float forward[3]={0,0,-1}, worldUp[3]={0,1,0};
    float hf[3],hr[3],hu[3];
    xrmath::quat_rotate(q[0],q[1],q[2],q[3],forward,hf);
    cross(hf,worldUp,hr);
    if (dot(hr,hr)<0.04f || !normalize(hr)) return false;
    cross(hr,hf,hu);
    float rd[3]={f.ray.dirXr[0],f.ray.dirXr[1],f.ray.dirXr[2]};
    if (!normalize(rd)) return false;
    const float F[3]={std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch)};
    const float R[3]={-std::sin(yaw),std::cos(yaw),0};
    const float U[3]={-std::sin(pitch)*std::cos(yaw),-std::sin(pitch)*std::sin(yaw),std::cos(pitch)};
    float delta[3];for(int i=0;i<3;++i) delta[i]=f.ray.originXr[i]-f.headPos[i];
    const float rp[3]={dot(delta,hr),dot(delta,hu),dot(delta,hf)};
    const float rel[3]={dot(rd,hr),dot(rd,hu),dot(rd,hf)};
    Solution s;
    float gap2=0;
    for(int i=0;i<3;++i) {
        s.origin[i]=camera[i]+scale*(R[i]*rp[0]+U[i]*rp[1]+F[i]*rp[2]);
        s.target[i]=s.origin[i]+scale*f.distanceM*(R[i]*rel[0]+U[i]*rel[1]+F[i]*rel[2]);
        s.direction[i]=s.target[i]-spawn[i];
        gap2+=(s.origin[i]-spawn[i])*(s.origin[i]-spawn[i]);
    }
    if (!finite3(s.target) || !std::isfinite(gap2) || gap2>16*scale*scale ||
        dot(s.direction,s.direction)<0.01f*scale*scale || !normalize(s.direction)) return false;
    out=s; return true;
}
} // namespace dvr::fireaim
