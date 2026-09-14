#include "game/dishonored/positional_math.h"
#include <cmath>
#include <cstdio>
#include <limits>
#include <initializer_list>
using namespace dvr::position_math;
constexpr float rad=3.14159265358979323846f/180;
int failed=0;
void check(bool b,const char* n){std::printf("%s %s\n",b?"PASS":"FAIL",n);if(!b)++failed;}
// Independent rotated basis in tracking coordinates: X right, Y up, Z back.
void pose(float p,float y,float r,float m[3][3]) {
    float cp=std::cos(p),sp=std::sin(p),cy=std::cos(y),sy=std::sin(y),cr=std::cos(r),sr=std::sin(r);
    float R[3]={cy,0,sy},U[3]={-sy*sp,cp,cy*sp},B[3]={-sy*cp,-sp,cy*cp};
    for(int i=0;i<3;++i){m[i][0]=R[i]*cr-U[i]*sr;m[i][1]=R[i]*sr+U[i]*cr;m[i][2]=B[i];}
}
// Prior formula, retained here as a falsifiable regression control.
void legacy(float p,float y,float r,float b,float f,float out[3]) {
    float m[3][3];pose(p,y,r,m);float up[3]={m[0][1],m[1][1],m[2][1]};
    const float n=std::hypot(m[0][2],m[2][2]);
    if(n>0.2f){float R[3]={m[2][2]/n,0,-m[0][2]/n};up[0]=m[1][2]*R[2];up[1]=m[2][2]*R[0]-m[0][2]*R[2];up[2]=-m[1][2]*R[0];}
    float d[3]={b*up[0]-f*m[0][2]-f*std::sin(y),b*up[1]-f*m[1][2]-b,b*up[2]-f*m[2][2]+f*std::cos(y)};
    out[0]=(d[0]*std::cos(y)+d[2]*std::sin(y))*100;
    out[1]=d[1]*100;out[2]=(d[0]*std::sin(y)-d[2]*std::cos(y))*100;
}
int main(){
    float a[3],b[3],c[3];
    check(pitch_arc(0,.321f,.062f,100,a)&&a[0]==0&&a[1]==0&&a[2]==0,"level rotation adds no correction");
    bool parity=true;
    for(float p:{-70.f,-30.f,30.f,70.f})for(float y:{-150.f,0.f,140.f})for(float r:{-60.f,0.f,60.f}){
        legacy(p*rad,y*rad,r*rad,.321f,.062f,a);pitch_arc(p*rad,.321f,.062f,100,b);
        for(int i=0;i<3;++i)parity=parity&&std::fabs(a[i]-b[i])<.0001f;
    }
    check(parity,"ordinary pitch cancellation matches prior roll-free model across yaw and roll");
    legacy(85*rad,0,-40*rad,.321f,.062f,a);legacy(85*rad,0,40*rad,.321f,.062f,b);
    std::printf("legacy standing 85deg pitch, -40/+40 roll: lateral %.3f / %.3f uu\n",a[0],b[0]);
    check(std::fabs(a[0]-b[0])>40,"legacy near-pole fallback reproduces a large lateral arc");
    legacy(85*rad,0,0,.321f,.062f,c);
    check(std::fabs(c[1]-a[1])>.5f,"legacy roll also bends vertical position into an arc");
    legacy(85*rad,0,-40*rad,0,0,a);legacy(85*rad,0,40*rad,0,0,b);
    check(a[0]==0&&a[1]==0&&a[2]==0&&b[0]==0&&b[1]==0&&b[2]==0,"zero crouch pivot has no modeled arc");
    bool invariant=true;
    for(float p:{-87.f,-85.f,-79.f,79.f,85.f,87.f}){
        pitch_arc(p*rad,.321f,.062f,100,a);
        for(float roll:{-60.f,0.f,60.f}){
            // Camera basis at this pitch/roll, UE axes X forward, Y right, Z up.
            float cp=std::cos(p*rad),sp=std::sin(p*rad),cr=std::cos(roll*rad),sr=std::sin(roll*rad);
            float F[3]={cp,0,sp},R[3]={sr*sp,cr,-sr*cp},U[3]={-cr*sp,sr,cr*cp},PR[3];
            invariant=upright_axes(F,R,U,PR)&&invariant;
            for(int i=0;i<3;++i)b[i]=PR[i]*a[0]+U[i]*a[1]+F[i]*a[2];
            invariant=invariant&&std::fabs(b[0]-a[2])<.0001f&&std::fabs(b[1])<.0001f&&std::fabs(b[2]-a[1])<.0001f;
        }
    }
    check(invariant,"steep pitched roll cannot rotate neck correction into a smile arc");
    float F[3]={.0871557f,0,.996195f},R[3]={.5f,.8660254f,-.0435779f},U[3]={-.86273f,.5f,.07548f},PR[3];
    upright_axes(F,R,U,PR);
    check(U[0]==0&&U[1]==0&&U[2]==1&&PR[2]==0&&F[2]==0,"tracked height stays world-up at steep pitch");
    check(std::fabs(R[2]+.0435779f)<1e-7f,"stereo eye-right axis remains rolled");
    float poleF[3]={0,0,1},poleR[3]={0,1,0},poleU[3]={-1,0,0};
    check(!upright_axes(poleF,poleR,poleU,PR),"undefined exact-pole heading refuses rather than using rolled translation");
    pitch_arc(90*rad,.321f,.062f,100,a);pitch_arc(16000*6.28318530718f/65536,.321f,.062f,100,b);
    check(a[1]==b[1]&&a[2]==b[2],"correction respects the controller pitch clamp");
    check(!pitch_arc(std::numeric_limits<float>::quiet_NaN(),.321f,.062f,100,a),"invalid physical pitch is refused");
    pitch_arc(78.45f*rad,.321f,.062f,100,a);pitch_arc(78.48f*rad,.321f,.062f,100,b);
    check(std::fabs(a[1]-b[1])<.03f&&std::fabs(a[2]-b[2])<.03f,"old .2 threshold no longer introduces a positional discontinuity");
    return failed?1:0;
}
