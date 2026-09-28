#include "game/dishonored/anim_state.h"
#include "game/dishonored/hands/animation_origin.h"
#include <cstdio>
#include <cstdlib>
using namespace dvr;
static unsigned long long now=1000,renderFrame=1;
static float g_skcWorldScale=100,g_mpDriveGain=1;
static unsigned g_mpSrcGen=7;static int g_mpEyeState=-1;
static bool g_menuOpen=false,g_inMenu=false,g_mainMenu=false,blocked=false,leverOn=true;
static anim::Snapshot state;static float weights[2]={1,1};
unsigned long long GetTickCount64(){return now;}
bool UiSurfaceBlocks(){return blocked;}
namespace dvr::frame {unsigned long long count(){return ::renderFrame;}}
namespace dvr::anim {
 Snapshot snapshot(){return ::state;}
 bool enabled(){return true;}
 bool hand_origin_enabled(){return leverOn;}
 bool hand_origin_trace(){return false;}
 float weight_for(int h){return weights[h];}
}
#define Log(...) ((void)0)
#define DVR_LOG_EVERY_MS(...) ((void)0)
struct MpDrawCtx {bool ok=true,poseOk=true;float t[3]{},r[3]{1,0,0},u[3]{0,1,0},f[3]{0,0,1};hf::Mat3 R_L=hf::identity3();struct {bool ok[2]={true,true};unsigned gen=3;float ruf[2][3]{};}pose;};
static hf::Xform published[2];static int publishes[2]{};
void WaPublishCommon(int h,const MpDrawCtx*,const hf::Xform& x){published[h]=x;++publishes[h];}
#include "animation_origin_route.inc"
static int checks=0;
void check(bool b,const char* msg){++checks;if(!b){printf("FAIL %d: %s\n",checks,msg);std::exit(1);}}
void next(){++renderFrame;now+=10;state.stamp=now;}
void entry(unsigned id){next();state.valid=true;state.game=true;state.handMask=3;state.originEpisode=id;state.originAt=now;}
int main(){
 MpDrawCtx c;float q[3]={2,3,4};hf::Xform tracked={hf::identity3(),{17,-8,3}},out;
 state.valid=true;state.stamp=now;
 check(!MpAnimRoute(),"ordinary gameplay untouched");
 entry(1);weights[0]=weights[1]=.75f;
 auto left=MpAnimBlend(&c,0,q,tracked);check(!g_animOrigin.ready,"left cannot pick origin");
 g_mpEyeState=0;MpAnimBlend(&c,1,q,tracked);check(!g_animOrigin.ready,"unknown eye cannot capture");
 g_mpEyeState=-1;auto right=MpAnimBlend(&c,1,q,tracked);check(g_animOrigin.ready,"right entry captured");check(fabsf(right.t[0]-17)<.001f,"capture keeps tracked entry position");
 left=MpAnimBlend(&c,0,q,tracked);check(fabsf(left.t[0]-right.t[0])<.001f,"left consumes same entry offset");
 next();weights[0]=weights[1]=0;g_mpEyeState=1;
 check(MpAnimNative(&c,1,&out),"native path remains aligned in other eye");
 check(fabsf(out.t[0]-17)<.001f && publishes[1]==1,"weapon gets native common correction");
 next();g_mpEyeState=-1;MpAnimBlend(&c,1,q,tracked);
 check(g_animOrigin.locked,"authoring eye locks at native endpoint");
 tracked.t[0]=99;MpAnimBlend(&c,1,q,tracked);check(fabsf(g_animOrigin.world[0]-17)<.001f,"controller changes do not retarget animation");
 next();state.game=false;weights[0]=weights[1]=.5f;
 check(MpAnimReady(),"classifier release preserves origin through return blend");
 auto returning=MpAnimBlend(&c,1,q,tracked);
 check(fabsf(returning.t[0]-58)<.001f,"return blends origin into current controller target");
 next();state.handMask=0;weights[0]=weights[1]=1;
 check(!MpAnimReady(),"finished return releases origin");
 entry(6);MpAnimBlend(&c,1,q,tracked);
 next();g_menuOpen=true;check(!MpAnimReady(),"menu clears entry");next();g_menuOpen=false;check(!MpAnimRoute(),"same action after menu stays refused");
 entry(2);MpAnimBlend(&c,1,q,tracked);check(MpAnimReady(),"new action rearms");
 ++g_mpSrcGen;check(!MpAnimReady(),"source rebuild clears in same frame");
 entry(3);now+=151;state.stamp=now;check(!MpAnimRoute(),"missing entry sample does not capture late");
 entry(4);weights[0]=1;weights[1]=0;state.handMask=2;MpAnimBlend(&c,1,q,tracked);
 auto freeLeft=MpAnimBlend(&c,0,q,tracked);check(fabsf(freeLeft.t[0]-tracked.t[0])<.001f,"right-only ownership leaves left tracked");
 check(!MpAnimNative(&c,0,&out),"left never handed back in right-only action");
 next();leverOn=false;check(!MpAnimReady(),"live off invalidates");next();leverOn=true;check(!MpAnimReady(),"live on cannot reuse old action");
 entry(5);c.pose.ok[1]=false;MpAnimBlend(&c,1,q,tracked);check(!MpAnimReady(),"tracking loss blocks capture");c.pose.ok[1]=true;MpAnimBlend(&c,1,q,tracked);check(MpAnimReady(),"valid entry sample can capture");
 next();state.stamp=now-151;check(!MpAnimReady(),"stale animation ownership invalidates");
 printf("production animation-origin routing: %d checks PASS\n",checks);return 0;
}
