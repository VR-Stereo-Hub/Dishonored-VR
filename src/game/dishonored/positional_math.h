// VR-106: positional correction stays in an upright frame at steep pitch.
#pragma once
#include <cmath>
namespace dvr::position_math {
// Re-express right/up/forward from one physical head-yaw frame in another.
// Subtracting vectors from different yaw frames creates motion at fixed position.
inline void reframe_yaw(const float in[3],float from,float to,float out[3]) {
    const float c=std::cos(to-from),s=std::sin(to-from);
    const float r=in[0],f=in[2];
    out[0]=r*c-f*s;out[1]=in[1];out[2]=f*c+r*s;
}
inline void yaw_axes(float yaw,float right[3],float up[3],float forward[3]) {
    const float c=std::cos(yaw),s=std::sin(yaw);
    right[0]=-s;right[1]=c;right[2]=0;
    forward[0]=c;forward[1]=s;forward[2]=0;
    up[0]=up[1]=0;up[2]=1;
}
inline bool pitch_arc(float pitch,float below,float behind,float scale,float out[3]) {
    if (!std::isfinite(pitch) || !std::isfinite(below) || !std::isfinite(behind) || !std::isfinite(scale)) return false;
    // Match the controller's existing +/-16000 rotator pitch limit.
    constexpr float limit=16000.0f*6.2831853071795864769f/65536.0f;
    if(pitch>limit) pitch=limit;
    if(pitch<-limit) pitch=-limit;
    const float s=std::sin(pitch),c=std::cos(pitch);
    out[0]=0;
    out[1]=(below*(c-1)+behind*s)*scale;
    out[2]=(-below*s+behind*(c-1))*scale;
    return true;
}
// f/u are positional axes. The caller retains the unmodified eye-right axis.
inline bool upright_axes(float f[3],const float right[3],float u[3],float pr[3]) {
    const float n=std::hypot(f[0],f[1]);
    if (!std::isfinite(n) || n<=1e-6f) return false;
    pr[0]=-f[1]/n;pr[1]=f[0]/n;pr[2]=0;
    if(pr[0]*right[0]+pr[1]*right[1]<0) {pr[0]=-pr[0];pr[1]=-pr[1];}
    f[0]/=n;f[1]/=n;f[2]=0;
    u[0]=u[1]=0;u[2]=1;
    return true;
}
}
