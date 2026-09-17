#include "game/dishonored/objective_marker_policy.h"
#include "core/gfx/hud_native_icon.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <initializer_list>
static unsigned checks=0;
void check(bool yes,const char* why){++checks;if(!yes){printf("FAIL %s\n",why);exit(1);}}
namespace dvr::objectivemarkers {bool active=true;float margin=.12f;bool enabled(){return active;}float inset(){return margin;}}
namespace dvr::vr {bool live=true;bool session_live(){return live;}}
namespace dvr::stereo {bool projection=true;bool wants_projection(){return projection;}}
bool riding=false,g_gameExiting=false;bool UiSurfaceRidesHud(){return riding;}
uint32_t g_taskCalls=0,g_taskMoved=0,g_taskRefused=0;bool liveOwner=true;
bool TaskInputs(void*,int& w,int& h,const char*& reason){w=1000;h=800;reason="host";return liveOwner;}
struct Capture {void* marker;float x,y;uint32_t a,b;float distance;uint32_t flags;unsigned calls=0;} captured;
void __fastcall Original(void* marker,void*,float x,float y,uint32_t a,uint32_t b,float distance,uint32_t flags){
 captured={marker,x,y,a,b,distance,flags,captured.calls+1};
}
using TaskParentFn=void (__thiscall*)(void*,float,float,uint32_t,uint32_t,float,uint32_t);
uintptr_t kTaskParentUpdate=(uintptr_t)&Original;
uintptr_t kTaskParentReturn=1000;
uint8_t kTaskParentCallBytes[5]={0xe8,0,0,0,0};
#define _ReturnAddress() ((void*)kTaskParentReturn)
#define DVR_LOG_EVERY_MS(...) ((void)0)
#include "objective_stub.inc"
#include "objective_fingerprint.inc"
int main(){
 using dvr::objectivemarkers::inset_position;
 for(int w:{1280,1920,3012}) for(int h:{720,1080,3122}) for(int percent:{5,12,20,30}) for(int angle=0;angle<360;angle+=5){
  const float a=angle*.01745329252f,dx=w*std::cos(a),dy=h*std::sin(a);
  float x=w*.5f+dx,y=h*.5f+dy;const float margin=percent/100.f;
  check(inset_position(x,y,w,h,9,margin),"offscreen native task moved");
  check(x>=w*margin-.01f && x<=w*(1-margin)+.01f && y>=h*margin-.01f && y<=h*(1-margin)+.01f,"inside chosen edge");
  check(std::fabs((x-w*.5f)*dy-(y-h*.5f)*dx)<2,"direction from center retained");
 }
 for(uint32_t flags:{0u,1u,2u,8u}) {float x=990,y=400;check(!inset_position(x,y,1000,800,flags,.12f)&&x==990&&y==400,"hidden or on-screen position unchanged");}
 float x=990,y=400;
 check(!inset_position(x,y,0,800,9,.12f),"invalid dimensions refused");
 check(!inset_position(x,y,1000,800,9,std::numeric_limits<float>::quiet_NaN()),"nonfinite setting refused");
 x=500;y=400;check(!inset_position(x,y,1000,800,9,.12f),"center unchanged");
 auto call=[](){TaskParentStub((void*)123,nullptr,990,400,0x12345678,0x87654321,2500,9);};
 call();check(captured.calls==1 && captured.x==880 && captured.y==400,"production wrapper forwards moved parent once");
 check(captured.marker==(void*)123&&captured.a==0x12345678&&captured.b==0x87654321&&captured.distance==2500&&captured.flags==9,"opaque ABI arguments retained");
 liveOwner=false;call();check(captured.x==990 && g_taskRefused==1,"unvalidated owner forwards native unchanged");liveOwner=true;
 dvr::objectivemarkers::active=false;call();check(captured.x==990,"live off retains native");dvr::objectivemarkers::active=true;
 dvr::vr::live=false;call();check(captured.x==990,"session loss retains native");dvr::vr::live=true;
 riding=true;call();check(captured.x==990,"menus retain native");riding=false;
 g_gameExiting=true;call();check(captured.x==990,"shutdown retains native");g_gameExiting=false;
 dvr::stereo::projection=false;call();check(captured.x==990,"mono retains native");
 int32_t rel=(int32_t)(kTaskParentUpdate-kTaskParentReturn);memcpy(kTaskParentCallBytes+1,&rel,4);
 uint8_t copy[5];memcpy(copy,kTaskParentCallBytes,5);check(TaskParentFingerprint(copy),"decoded direct target verified");
 for(int i=0;i<5;++i){copy[i]^=1;check(!TaskParentFingerprint(copy),"each corrupt byte refused");copy[i]^=1;}
 dvr::hudnative::Markers markers;float edge[4]={.86f,.48f,.90f,.52f};
 check(markers.observe(42,1,edge,8,10,.12f),"new candidate edge still learns native icon family");
 edge[0]=.4f;edge[2]=.44f;check(markers.observe(42,2,edge,8,10,.12f),"learned family retained in interior");
 printf("%u objective marker checks passed\n",checks);
}
