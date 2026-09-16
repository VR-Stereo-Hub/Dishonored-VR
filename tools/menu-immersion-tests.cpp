// Production menu module with controlled engine/runtime boundaries; no game.
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "game/dishonored/cinematic_math.h"
#include "game/dishonored/positional_math.h"
#define DVR_LOG_EVERY_MS(...) ((void)0)
using LONG=long;
unsigned threadId=1,g_sdDrawTid=1,epoch=1;
unsigned GetCurrentThreadId(){return threadId;}
bool live=true,buildOk=true,readable=true,headOn=true,blurOn=false,riding=true,runtime=true;
int generation=1,context=6,builds=0,publications=0;
unsigned publishedGen=0;
LONG g_mkLoadEvents=1;
bool g_trackingEnabled=true,g_rotInject=true,g_mainMenu=false,g_gameExiting=false,g_ctLayout=true;
float g_flipPitch=1,g_flipYaw=1,g_flipRoll=1;
uint8_t pc[128]{},cam[128]{},pawn[128]{},world[128]{},game[128]{},pp[128]{};
uint8_t *g_peCtrl=pc,*g_camObj=cam;
uint32_t g_ctPcCamera=4,g_ctPawn=8,g_ctCache=8,g_ctPov=8,g_ctRot=0;
struct CtIdentity { struct {void* obj=nullptr;} value;int gen=0; };
bool IsLiveObject(uint8_t* p){return live && p && (p==pc||p==cam||p==pawn||p==world||p==game||p==pp);}
bool BuildLiveSet(){++builds;return buildOk;}
bool ChSlot(const CtIdentity& id){return IsLiveObject((uint8_t*)id.value.obj)&&id.gen==generation;}
bool ChCapture(uint8_t* p,CtIdentity* id){if(!IsLiveObject(p))return false;id->value.obj=p;id->gen=generation;return true;}
bool CtRead(uint8_t* p,uint32_t off,void* out,size_t size){if(!readable||!IsLiveObject(p)||off+size>128)return false;memcpy(out,p+off,size);return true;}
uint8_t* CtObject(uint8_t* p,uint32_t off){uint8_t* out=nullptr;return CtRead(p,off,&out,sizeof(out))&&IsLiveObject(out)?out:nullptr;}
bool FindPropOffsetChecked(const char*,const char* prop,uint32_t* out){*out=!strcmp(prop,"WorldInfo")?12:!strcmp(prop,"m_UIPPWeight")?16:4;return true;}
int UiSurfaceContext(){return context;}
unsigned UiSurfaceEpoch(){return epoch;}
bool UiSurfaceHeadLook(){return headOn&&riding&&context>=3&&context<=8;}
bool UiSurfaceRidesHud(){return riding;}
namespace dvr::hudlayout {bool menu_no_blur(int c){return blurOn&&c>=3&&c<=8;}}
namespace dvr::vr {bool session_live(){return runtime;}bool cinematic_active(){return false;}}
namespace dvr::stereo {bool wants_projection(){return true;}}
namespace dvr::camera {
 bool eyetest_active(){return false;}bool postest_active(){return false;}
 void position_offset_uu(float* p){p[0]=1;p[1]=2;p[2]=3;}
 void cinematic_position_offset_uu(float* p){p[0]=1;p[1]=2;p[2]=3;}
 int32_t saved[3];bool(*validator)(uint8_t*)=nullptr;
 bool begin_view_scope(uint8_t* p,uint32_t off,const int32_t* rot,const float*,int,bool(*v)(uint8_t*),bool,const float*){
  if(!v(p))return false;validator=v;memcpy(saved,p+off,12);memcpy(p+off,rot,12);return true;}
 bool end_view_scope(){if(!validator(cam))return false;memcpy(cam+16,saved,12);return true;}
}
struct HtSample {float position[3]{1,2,3},rawPosition[3]{1,2,3};float pitch=0,yaw=0,roll=0;unsigned gen=1;double locateMs=1000;bool ok=true,poseOk=true;} sample;
bool HtConsumeSample(HtSample* out){*out=sample;return true;}
double MaimNowMs(){return 1000;}
void CineTraceTick(){}
void Log(const char*,...){}
void HtPublishCameraRecord(int,const HtSample& h,float,float,float){++publications;publishedGen=h.gen;}
static void MenuEffectsTick();
#include "../src/game/dishonored/menu_immersion.cpp"
static int checks=0;
void check(bool ok,const char* why){++checks;if(!ok){printf("FAIL %s\n",why);exit(1);}}
void link(uint8_t* p,unsigned off,uint8_t* obj){memcpy(p+off,&obj,sizeof(obj));}
void weight(float f){memcpy(pp+16,&f,4);}float weight(){float f;memcpy(&f,pp+16,4);return f;}
#include "core/gfx/pause_scene_freshness.h"
#include "core/gfx/hud_menu_lifecycle.h"
int main(){
 dvr::stereo::PauseSceneFreshness fresh;
 check(!fresh.recent(true,3,true,0),"no observed scene cannot authorize pause doubling");
 fresh.complete(4,8,100);
 check(fresh.recent(true,3,true,110),"uploads during prior draw survive an idle between-draw interval");
 check(!fresh.recent(false,3,true,110),"default-off keeps original gate");
 check(!fresh.recent(true,6,true,110),"wheel policy unchanged");
 check(!fresh.recent(true,3,false,110),"head-look disabled refuses freshness exception");
 check(!fresh.recent(true,3,true,200),"100ms expiry refuses stale scene");
 check(!fresh.recent(true,3,true,99),"clock rollback refuses");
 fresh.complete(8,8,195);check(!fresh.recent(true,3,true,200),"silent draw does not renew evidence");
 fresh.clear();check(!fresh.recent(true,3,true,110),"context exit clears evidence");
 dvr::hudlayout::WheelVisualLease visual;
 check(visual.update(true,6,false,1),"wheel pixels own wheel layout");
 check(visual.update(false,0,true,8),"native closing keeps crop after input releases");
 check(visual.update(false,0,false,20),"observer delay starts tail when close is observed");
 check(visual.update(false,0,false,23),"three delayed presents keep wheel layout");
 check(!visual.update(false,0,false,24),"expired visual tail releases gameplay HUD");
 visual.update(true,6,false,30);check(!visual.update(true,3,false,31),"pause replaces wheel immediately");
 visual.update(true,6,false,40);check(!visual.update(false,2,false,41),"loading cannot retain old wheel");
 link(pc,4,cam);link(pc,8,pawn);link(pc,12,world);link(world,4,game);link(game,4,pp);
 MenuHeadBegin(true,true);check(g_mhScope&&builds==1,"first menu refreshes live table and starts scope");MenuHeadEnd();
 sample.yaw=.2f;sample.gen=2;MenuHeadBegin(true,true);
 check(g_mhScope&&g_mhWritten[1]>2000,"head yaw updates rendered camera");
 MenuHeadPublish();check(publishedGen==2&&publications==3,"both eyes publish the exact consumed sample");MenuHeadEnd();
 int32_t restored=1;memcpy(&restored,cam+20,4);check(restored==0,"native camera restored after pair");
 MenuHeadBegin(true,false);check(g_mhScope,"single draw retains tracked camera instead of reverting to native view");MenuHeadEnd();
 MenuHeadBegin(false,false);sample.yaw=.3f;MenuHeadBegin(true,true);
 check(g_mhWritten[1]>3000,"temporary render gap does not reset head reference");MenuHeadEnd();
 const int previousBuilds=builds;++epoch;MenuHeadBegin(true,true);
 check(std::abs(g_mhWritten[1])<2&&builds==previousBuilds+1,"new menu interval revalidates unchanged pointers");MenuHeadEnd();
 live=false;MenuHeadBegin(true,true);check(!g_mhScope,"dead camera cannot be written");live=true;
 ++generation;sample.yaw=.4f;MenuHeadBegin(true,true);check(g_mhScope&&std::abs(g_mhWritten[1])<2,"replaced owner gets fresh reference");MenuHeadEnd();
 context=-1;MenuHeadBegin(true,true);check(!g_mhHave&&!g_mhScope,"gameplay releases menu ownership");
 context=6;++epoch;blurOn=true;weight(.8f);MenuEffectsTick();check(weight()==0&&g_mbBefore==.8f,"UI-only weight suppressed");
 weight(.6f);MenuEffectsTick();check(weight()==0&&g_mbBefore==.6f,"game's latest effect value retained");
 blurOn=false;MenuEffectsTick();check(std::fabs(weight()-.6f)<.001f&&!g_mbHave,"option off restores exact owned value");
 blurOn=true;weight(.7f);MenuEffectsTick();weight(.3f);blurOn=false;MenuEffectsTick();check(std::fabs(weight()-.3f)<.001f,"external recomputation is not overwritten on exit");
 blurOn=true;MenuEffectsTick();live=false;blurOn=false;MenuEffectsTick();check(weight()==0,"dead object is not restored");live=true;
 weight(.9f);threadId=2;blurOn=true;MenuEffectsTick();check(weight()==.9f,"wrong lane cannot write engine memory");threadId=1;
 buildOk=false;MenuEffectsTick();check(weight()==.9f&&!g_mbHave,"failed live table refresh refuses write");
 printf("menu immersion: %d checks PASS\n",checks);
}
