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
 std::printf("weapon dial: %d checks PASS\n",checks);
}
