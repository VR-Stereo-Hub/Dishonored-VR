// Pure launch geometry. No engine objects or writes.
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

// VR-82. The pistol stands its bullet off from the origin ALONG THE AIM
// DIRECTION - spawn = origin + dir * m_fBulletSpawnDistance - so the position
// and the direction are one decision, not two. Correcting only the direction
// would leave the bullet starting up to that distance along the old head
// direction, which can place its first frame inside geometry the aim line
// never crossed.
//
// So reconstruct the origin the engine used, aim from THERE, and re-derive the
// standoff along the corrected direction. This is the engine's own formula with
// our direction substituted; it introduces no constant of its own.
//
// nativeDir must be the direction the standoff was BUILT from, not the one the
// spawn rotation will consume - a later native call can rewrite the latter.
struct StandoffSolution { Solution ray; float origin[3] = {}, spawn[3] = {}; float reach = 0; };
inline bool solve_standoff(const aim::FireFrame& f, uint64_t now, float yaw, float pitch,
                           const float* camera, float scale, const float* nativeSpawn,
                           const float* nativeDir, float standoff, StandoffSolution& out,
                           const char** why = nullptr, float* reachOut = nullptr) {
    // reachOut is filled as soon as the reach is known, INCLUDING on the refusal
    // that the reach causes - a refusal line that could only ever print zero
    // would not be evidence of anything. It stays negative when never computed.
    if (reachOut) *reachOut = -1;
    auto no = [&](const char* reason) { if (why) *why = reason; return false; };
    if (!finite3(nativeSpawn) || !finite3(nativeDir)) return no("spawn or direction not finite");
    if (!std::isfinite(standoff) || standoff < 0 || standoff > 1000)
        return no("bullet spawn distance out of range");
    const float n = dot(nativeDir,nativeDir);
    if (!std::isfinite(n) || n < 0.5f || n > 1.5f) return no("standoff direction is not unit");
    StandoffSolution s;
    for (int i=0;i<3;++i) s.origin[i] = nativeSpawn[i] - nativeDir[i]*standoff;
    if (!finite3(s.origin)) return no("reconstructed origin not finite");
    // Aiming from the reconstructed origin makes solve()'s reach check compare
    // the controller ray's origin against the engine's own, not against a point
    // already displaced by the standoff.
    if (!solve(f,now,yaw,pitch,camera,scale,s.origin,s.ray)) return no("stale/invalid ray, basis or origin");
    float toTarget[3];
    for (int i=0;i<3;++i) toTarget[i] = s.ray.target[i]-s.origin[i];
    s.reach = std::sqrt(dot(toTarget,toTarget));
    if (reachOut) *reachOut = s.reach;
    // A standoff that reaches the aim point would spawn the bullet ON or PAST
    // the thing it is aimed at, and past it the launch direction is reversed.
    // Half the reach is the bound: it keeps the bullet in the near half of its
    // own aim line at every dot distance the lever allows.
    if (!std::isfinite(s.reach) || standoff > 0.5f*s.reach)
        return no("bullet spawn distance reaches the aim point");
    for (int i=0;i<3;++i) s.spawn[i] = s.origin[i] + s.ray.direction[i]*standoff;
    if (!finite3(s.spawn)) return no("standoff position not finite");
    out=s; return true;
}
} // namespace dvr::fireaim
