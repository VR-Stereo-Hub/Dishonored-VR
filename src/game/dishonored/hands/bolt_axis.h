// Geometry of an identified, rigid loaded bolt. No asset-specific coordinates.
#pragma once
#include "game/dishonored/hands/hand_frame.h"
#include <cmath>
namespace dvr::hf {
struct BoltAxis { float center[3]={}, dir[3]={}, low=0, high=0, ratio=0; };
// VR-57: the variance ratio is a PARAMETER now, because two different things are
// being fitted. A loaded bolt is nearly one-dimensional and must clear 16:1 - that
// strictness is what stops a weapon body being mistaken for a barrel. A weapon body
// fitted deliberately, as the fallback for a weapon with no visible projectile,
// only needs a dominant axis; a pistol has a grip at right angles to its barrel and
// will never reach 16:1. The caller says which it is asking for, and the ORIGIN it
// uses differs too: a bolt is aimed from its tip, a weapon body from its centre,
// which is insensitive to how long the mesh happens to be.
inline bool bolt_axis_ratio(const float (*points)[3], int n, float minRatio, BoltAxis& result) {
    if(n<16 || n>1024) return false;
    BoltAxis a;
    for(int j=0;j<n;++j) for(int i=0;i<3;++i) {
        if(!std::isfinite(points[j][i])) return false;
        a.center[i]+=points[j][i]/n;
    }
    float cov[3][3]={};
    for(int j=0;j<n;++j) for(int r=0;r<3;++r) for(int c=0;c<3;++c)
        cov[r][c]+=(points[j][r]-a.center[r])*(points[j][c]-a.center[c])/n;
    int seed=0;for(int i=1;i<3;++i) if(cov[i][i]>cov[seed][seed]) seed=i;
    a.dir[seed]=1;
    for(int k=0;k<32;++k) {
        float v[3]={},len=0;
        for(int r=0;r<3;++r){for(int c=0;c<3;++c)v[r]+=cov[r][c]*a.dir[c];len+=v[r]*v[r];}
        if(!std::isfinite(len)||len<1e-12f)return false;
        for(int i=0;i<3;++i)a.dir[i]=v[i]/std::sqrt(len);
    }
    float along=0;for(int r=0;r<3;++r)for(int c=0;c<3;++c)along+=a.dir[r]*cov[r][c]*a.dir[c];
    const float transverse=cov[0][0]+cov[1][1]+cov[2][2]-along;
    a.ratio=along/(transverse>1e-6f?transverse:1e-6f);
    if(a.ratio<minRatio || !std::isfinite(a.ratio))return false;
    a.low=1e20f;a.high=-1e20f;
    for(int j=0;j<n;++j){float t=0;for(int i=0;i<3;++i)t+=(points[j][i]-a.center[i])*a.dir[i];
        if(t<a.low)a.low=t;if(t>a.high)a.high=t;}
    if(a.high-a.low<0.01f)return false;
    result=a;return true;
}
// The original name, at the strict bolt threshold, so every existing caller and
// test keeps its exact meaning.
inline bool bolt_axis(const float (*points)[3], int n, BoltAxis& result) {
    return bolt_axis_ratio(points, n, 16.0f, result);
}
inline void bolt_tip(const BoltAxis& a, int sign, float* point, float* direction) {
    for(int i=0;i<3;++i){point[i]=a.center[i]+a.dir[i]*(sign>0?a.high:a.low);direction[i]=a.dir[i]*sign;}
}
inline bool palm_ray_to_xr(const Mat3& controller,const Mat3& grip,const float* palm,
                           const float* trimDeg,const float* trimM,const float* originPalm,
                           const float* dirPalm,float* origin,float* dir) {
    const Mat3 q=mul3(controller,grip);
    const Mat3 rotated=mul3(q,euler_xyz_deg_to_mat(trimDeg[0],trimDeg[1],trimDeg[2]));
    float translated[3],offset[3];mulv3(q,trimM,translated);mulv3(rotated,originPalm,offset);
    mulv3(rotated,dirPalm,dir);float n=0;
    for(int i=0;i<3;++i){origin[i]=palm[i]+translated[i]+offset[i];n+=dir[i]*dir[i];
        if(!std::isfinite(origin[i]))return false;}
    if(!std::isfinite(n)||n<1e-8f)return false;
    for(int i=0;i<3;++i)dir[i]/=std::sqrt(n);
    return true;
}
} // namespace dvr::hf
