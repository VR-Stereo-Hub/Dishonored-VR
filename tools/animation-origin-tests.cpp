#include "game/dishonored/hands/animation_origin.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace dvr;
static int checks=0;
void check(bool b,const char* message){++checks;if(!b){printf("FAIL %d: %s\n",checks,message);std::exit(1);}}
bool near(float a,float b){return fabsf(a-b)<.001f;}
void point(const hf::Xform& x,const float* p,float* q){hf::apply_point(x,p,q);}
int main(){
 const float q[3]={7,-4,12};
 for(int degrees=-180;degrees<=180;degrees+=15){
  const float a=degrees*.0174532925f;
  hf::Mat3 body={{cosf(a),-sinf(a),0,sinf(a),cosf(a),0,0,0,1}};
  hf::Xform tracked={hf::identity3(),{13,-26,8}};
  tracked.r={{0,-1,0,1,0,0,0,0,1}};
  anim::OriginTranslation o;o.sync(1,2,true);check(o.capture(body,tracked,q),"entry captured");
  float expected[3],start[3];point(tracked,q,expected);point(o.local(body),q,start);
  for(int i=0;i<3;++i)check(near(start[i],expected[i]),"entry at tracked right hand despite rotation");
  // The old identity destination must fail the same entry placement requirement.
  check(!near(q[0],expected[0]) || !near(q[1],expected[1]),"old-policy negative control");
  const float next[3]={q[0]+3,q[1]-5,q[2]+9};float moved[3];point(o.local(body),next,moved);
  for(int i=0;i<3;++i)check(near(moved[i]-start[i],next[i]-q[i]),"authored motion retained");
  hf::Xform changed=tracked;changed.t[0]+=100;
  check(!o.capture(body,changed,next),"live controller cannot drag running clip");
  for(int k=0;k<=10;++k){
   const float w=k/10.f;auto out=o.blend(body,tracked,w,q);auto old=anim::blend_transform(tracked,w);auto local=o.local(body);
   float position[3],native[3],target[3];point(out,q,position);point(local,q,native);point(tracked,q,target);
   for(int i=0;i<3;++i)check(near(position[i],native[i]*(1-w)+target[i]*w),"palm follows endpoint segment, never model-origin arc");
   for(int i=0;i<9;++i)check(near(out.r.m[i],old.r.m[i]),"authored rotation policy unchanged");
  }
  // A different local frame (left arm or weapon) must see the SAME world vector.
  hf::Mat3 member={{0,0,1,0,1,0,-1,0,0}};auto local=o.local(member);
  for(int r=0;r<3;++r){float v=0;for(int c=0;c<3;++c)v+=member.m[r*3+c]*local.t[c];check(near(v,o.world[r]),"common offset across hands/weapons");}
  // Eye translation and arbitrary head yaw cannot rotate a latched world vector.
  for(int eye=-1;eye<=1;eye+=2){float before[3]={eye*3.2f,9,-11},after[3];point(local,before,after);for(int i=0;i<3;++i)check(near(after[i]-before[i],local.t[i]),"both eye origins receive equal offset");}
  o.sync(1,3,true);check(!o.ready && o.refused,"source rebuild invalidates");check(!o.capture(body,tracked,q),"no relatch mid-clip after rebuild");
  o.sync(2,3,true);check(o.capture(body,tracked,q),"next episode rearms");
  o.sync(2,3,false);check(!o.ready && o.refused,"menu/lost ownership invalidates");o.sync(2,3,true);check(!o.capture(body,tracked,q),"resume cannot reuse old identity");
  o.sync(3,3,true);check(o.capture(body,tracked,q),"new episode after menu rearms");
 }
 // Regression: a far-from-origin palm with a rotating correction used to
 // describe an arc even when both endpoints were the same point.
 const float farPalm[3]={-33,-150,42};
 hf::Xform rotate={hf::euler_xyz_deg_to_mat(120,-45,90),{20,-40,10}};
 anim::OriginTranslation pivot;pivot.sync(8,1,true);pivot.capture(hf::identity3(),rotate,farPalm);
 auto old=anim::blend_transform(rotate,.5f);auto offset=pivot.local(hf::identity3());
 for(int i=0;i<3;++i)old.t[i]+=.5f*offset.t[i];
 float wrong[3],desired[3],correct[3];point(old,farPalm,wrong);point(rotate,farPalm,desired);
 point(pivot.blend(hf::identity3(),rotate,.5f,farPalm),farPalm,correct);
 float error=0;for(int i=0;i<3;++i){error+=(wrong[i]-desired[i])*(wrong[i]-desired[i]);check(near(correct[i],desired[i]),"rotation pivots at palm");}
 check(error>100,"old interpolation fails far-palm negative control");
 anim::OriginTranslation entry;entry.sync(9,1,true);
 hf::Xform track={hf::identity3(),{0,0,0}};float idle[3]={0,-150,40},raised[3]={0,-70,40};
 check(entry.sample(hf::identity3(),track,idle,1,-1,1,true),"initial idle sample provisional");
 track.t[1]=-80;
 check(entry.sample(hf::identity3(),track,raised,.5f,-1,2,true),"native rise rebased during entry");
 point(entry.blend(hf::identity3(),track,.5f,raised),raised,correct);
 check(near(correct[1],idle[1]),"raised authored entry remains at tracked palm");
 auto other=track;other.t[1]=999;
 check(!entry.sample(hf::identity3(),other,raised,.5f,1,3,true),"other eye cannot reanchor");
 check(!entry.sample(hf::identity3(),other,raised,.5f,-1,2,true),"duplicate pass cannot reanchor");
 check(entry.sample(hf::identity3(),track,raised,0,-1,4,true) && entry.locked,"native entry locks once");
 check(!entry.sample(hf::identity3(),other,raised,0,-1,5,true),"running animation cannot chase controller");
 raised[1]+=10;point(entry.local(hf::identity3()),raised,correct);
 check(near(correct[1],idle[1]+10),"post-entry authored movement retained");
 entry.sync(10,1,true);entry.sample(hf::identity3(),track,idle,.5f,-1,6,true);
 check(!entry.sample(hf::identity3(),other,idle,.75f,-1,7,false) && entry.locked,"return never changes entry origin");
 anim::OriginTranslation invalid;invalid.sync(1,1,true);hf::Xform bad={hf::identity3(),{0,0,0}};bad.t[1]=std::numeric_limits<float>::quiet_NaN();check(!invalid.capture(hf::identity3(),bad,q),"non-finite source rejected");
 printf("animation origin: %d checks PASS\n",checks);return 0;
}
