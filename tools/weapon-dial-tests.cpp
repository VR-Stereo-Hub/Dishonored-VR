#include "core/input/weapon_dial.h"
#include "core/vr/hud_anchor.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
static int checks=0;
static void check(bool yes,const char* message) { ++checks; if(!yes) { std::printf("FAIL %s\n",message); std::exit(1); } }
int main() {
 using namespace dvr::weapon_dial;
 float x,y;
 // Every integer angle survives at full and partial deflection, including 7 o'clock.
 for(int degree=0;degree<360;++degree) for(float radius:{.3f,1.f}) {
   const float a=degree*3.14159265f/180, sx=std::cos(a),sy=std::sin(a);
   radial(radius*sx,radius*sy,.18f,x,y);
   check(std::fabs(x*sy-y*sx)<.00001f && x*sx+y*sy>0,"angle preserved");
 }
 radial(.05f,.05f,.18f,x,y);check(x==0 && y==0,"radial neutral");
 radial(2,2,.18f,x,y);check(std::fabs(x*x+y*y-1)<.00001f,"radial clamp");
 radial(std::numeric_limits<float>::quiet_NaN(),0,.18f,x,y);check(x==0 && y==0,"invalid input neutral");
 check(!step_menu(true,true) && step_menu(true,false) && !step_menu(false,false),"wheel exempt, other menus preserved");
 State s; float hand[3]={-.2f,1.f,-.5f}, head[3]={-.2f,1.f,0};
 check(s.update(true,true,hand,head,.12f,.015f,x,y) && x==0 && y==0,"opening hand is center");
 for(int degree=0;degree<360;++degree) {
   const float a=degree*3.14159265f/180;
   hand[0]=-.2f+.12f*std::cos(a);hand[1]=1+.12f*std::sin(a);
   check(s.update(true,true,hand,head,.12f,.015f,x,y),"tracking valid");
   check(std::fabs(x*std::sin(a)-y*std::cos(a))<.00001f,"hand angle preserved");
   check(s.center[0]==-.2f && s.center[1]==1,"center cannot follow hand");
 }
 hand[0]=-.2f; hand[1]=1;hand[2]=-.3f;
 s.update(true,true,hand,head,.12f,.015f,x,y);check(x==0 && y==0,"depth motion ignored");
 s.update(true,false,hand,head,.12f,.015f,x,y);check(!s.valid && x==0 && y==0,"tracking loss neutral");
 check(!s.update(true,true,hand,head,.12f,.015f,x,y),"tracking recovery cannot reseed gesture");
 s.update(false,true,hand,head,.12f,.015f,x,y);check(!s.held,"release resets");
 check(s.update(true,true,hand,head,.12f,.015f,x,y) && s.center[2]==-.3f,"new opening reseeds");
 // Tilted billboard: displacement along its up axis must remain positive vertical.
 s.reset();float p[3]={0,0,-.5f}, eye[3]={0,.5f,0};
 s.update(true,true,p,eye,.12f,.015f,x,y);
 p[1]=.0848528f; p[2]-=.0848528f;
 s.update(true,true,p,eye,.12f,.015f,x,y);check(std::fabs(x)<.00001f && y>.99f,"tilted plane up sign");
 s.reset();p[0]=std::numeric_limits<float>::quiet_NaN();
 check(!s.update(true,true,p,eye,.12f,.015f,x,y) && !s.valid,"nonfinite pose rejected");
 const float crop[4]={.2f,.25f,.8f,.75f};
 auto c=dvr::hudanchor::crop_rect(3012,3122,crop,.42f,0);
 check(c.x>=0 && c.y>=0 && c.x+c.w<=3012 && c.y+c.h<=3122,"crop in texture");
 check(.226f>=crop[0] && .774f<=crop[2] && .275f>=crop[1] && .716f<=crop[3],"measured wheel ring retained");
 check(.79f>crop[3] && .85f>crop[3],"bottom ancillary widgets excluded");

 // New camera-plane mode is independent of opening position.
 const float cameraQ[4]={0,0,0,1};
 for(float side:{-.6f,0.f,.6f}) for(float height:{-.4f,0.f,.4f}) {
   s.reset();float start[3]={side,height,-.5f},eye2[3]={0,0,0};
   s.update(true,true,start,eye2,.04f,.002f,x,y,cameraQ,true);
   start[0]+=.003f;
   check(s.update(true,true,start,eye2,.04f,.002f,x,y,cameraQ,true) && x>.999f && std::fabs(y)<.00001f,"3mm selects full right independent of hand position");
 }
 for(int degree=0;degree<360;++degree) {
   s.reset();float start[3]={0,0,-.5f},eye2[3]={0,0,0};
   s.update(true,true,start,eye2,.04f,.002f,x,y,cameraQ,true);
   const float a=degree*3.14159265f/180;
   start[0]+=.003f*std::cos(a);start[1]+=.003f*std::sin(a);
   s.update(true,true,start,eye2,.04f,.002f,x,y,cameraQ,true);
   check(std::fabs(x*x+y*y-1)<.00001f && std::fabs(x*std::sin(a)-y*std::cos(a))<.00001f,"tiny motion preserves all angles at full scale");
 }
 s.reset();float tiny[3]={0,0,-.5f},eye3[3]={0,0,0};
 s.update(true,true,tiny,eye3,.04f,.002f,x,y,cameraQ,true);tiny[0]=.001f;
 s.update(true,true,tiny,eye3,.04f,.002f,x,y,cameraQ,true);check(x==0 && y==0,"1mm tracking jitter remains neutral");
 // Opening axes and selection must agree after a head turn.
 s.reset();float fixedHand[3]={0,0,-.5f},fixedHead[3]={0,0,0};
 const float turnedQ[4]={0,.7071068f,0,.7071068f};
 s.update(true,true,fixedHand,fixedHead,.04f,.002f,x,y,cameraQ,true);
 fixedHand[0]=.003f;s.update(true,true,fixedHand,fixedHead,.04f,.002f,x,y,turnedQ,true);
 check(x>.999f && std::fabs(y)<.00001f && s.opening.q[3]==1,"head turn cannot rotate dial or selection axes");
 // Opening head pitch/roll cannot tilt the dial or its gesture plane.
 for(float pitch:{-.8f,0.f,.8f}) for(float roll:{-.6f,.6f}) {
   float qp[4],qr[4],qy[4],tilt[4],opening[4];
   dvr::xrmath::quat_axis_angle(1,0,0,pitch,qp);
   dvr::xrmath::quat_axis_angle(0,0,1,roll,qr);
   dvr::xrmath::quat_axis_angle(0,1,0,.7f,qy);
   dvr::xrmath::quat_mul(qp,qr,tilt);dvr::xrmath::quat_mul(qy,tilt,opening);
   s.reset();float start[3]={0,0,-.5f},entryEye[3]={0,0,0},up[3]={0,1,0},out[3];
   check(s.update(true,true,start,entryEye,.04f,.002f,x,y,opening,true),"tilted opening accepted");
   dvr::xrmath::quat_rotate(s.opening.q[0],s.opening.q[1],s.opening.q[2],s.opening.q[3],up,out);
   check(std::fabs(out[0])<.00001f && std::fabs(out[1]-1)<.00001f && std::fabs(out[2])<.00001f,"dial remains vertical");
   start[1]+=.003f;s.update(true,true,start,entryEye,.04f,.002f,x,y,turnedQ,true);
   check(std::fabs(x)<.00001f && y>.999f,"world-up hand motion selects up after tilted entry");
 }
 std::printf("weapon dial: %d checks PASS\n",checks);
}
