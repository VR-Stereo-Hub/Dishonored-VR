// VR-70: UE camera rotations, independent of engine memory and runtime state.
#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::cine {
struct Matrix { double m[3][3]; };
inline Matrix rotation(double pitch, double yaw, double roll) {
    const double cp=std::cos(pitch),sp=std::sin(pitch),cy=std::cos(yaw),sy=std::sin(yaw);
    const double cr=std::cos(roll),sr=std::sin(roll);
    return {{{cp*cy, sr*sp*cy-cr*sy, -cr*sp*cy-sr*sy},
             {cp*sy, sr*sp*sy+cr*cy, -cr*sp*sy+sr*cy},
             {sp, -sr*cp, cr*cp}}};
}
inline Matrix multiply(const Matrix& a,const Matrix& b) {
    Matrix c={};
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) for(int k=0;k<3;++k) c.m[i][j]+=a.m[i][k]*b.m[k][j];
    return c;
}
inline Matrix transpose(const Matrix& a) {
    Matrix c={}; for(int i=0;i<3;++i) for(int j=0;j<3;++j) c.m[i][j]=a.m[j][i]; return c;
}
inline bool compose(const int32_t authored[3],const Matrix& reference,const Matrix& head,
                    int32_t out[3],Matrix* basis) {
    constexpr double rad=6.2831853071795864769/65536.0;
    const Matrix a=rotation(authored[0]*rad,authored[1]*rad,authored[2]*rad);
    const Matrix c=multiply(multiply(a,transpose(reference)),head);
    for(int i=0;i<3;++i) for(int j=0;j<3;++j) if(!std::isfinite(c.m[i][j])) return false;
    const double horizontal=std::hypot(c.m[0][0],c.m[1][0]);
    const double pitch=std::atan2(c.m[2][0],horizontal);
    const double yaw=horizontal>1e-7 ? std::atan2(c.m[1][0],c.m[0][0]) : std::atan2(-c.m[0][1],c.m[1][1]);
    const double roll=horizontal>1e-7 ? std::atan2(-c.m[2][1],c.m[2][2]) : 0;
    out[0]=(int32_t)std::lround(pitch/rad); out[1]=(int32_t)std::lround(yaw/rad); out[2]=(int32_t)std::lround(roll/rad);
    // Stereo uses the same quantized orientation the engine receives.
    if(basis) *basis=rotation(out[0]*rad,out[1]*rad,out[2]*rad);
    return true;
}
}
