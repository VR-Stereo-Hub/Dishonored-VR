#include "core/gfx/hud_native_icon.h"
#include "core/gfx/hud_native_rune.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
struct IDirect3DDevice9{};
using HRESULT=int;
#define FAILED(x) ((x)<0)
#define DVR_LOG_EVERY_MS(...) ((void)0)
static float scale=.7f,state[256][4]{},shadow[256][4]{};static int failRow=-1,writes=0;
bool upright=false,basisValid=true;float basisCo=1,basisSi=0,basisAspect=1;
bool native_basis(float& c,float& s,float& a){c=basisCo;s=basisSi;a=basisAspect;return basisValid;}
namespace dvr::hudlayout {
constexpr int ElObjective=6;
bool g_nativeGameplayReference=false,g_visualRiding=false,g_menuRiding=false,g_nativeObjectives=true;
bool& g_nativeObjectiveUpright=upright;float& g_nativeObjectiveScale=scale;
#include "hud_native_policy.inc"
}
using DWORD=uint32_t;static DWORD nowMs=5000;DWORD GetTickCount(){return nowMs;}
#define SUCCEEDED(x) ((x)>=0)
namespace dvr::hudcap {
bool g_on=true,g_handoffReady=true,g_failed=false;DWORD g_lastRedirectMs=0;
#include "hud_reference_health.inc"
}
namespace dvr::frame {
const float* vs_const_shadow_row(int row){return shadow[row];}
HRESULT orig_set_vs_const(IDirect3DDevice9*,int row,const float* v,int){++writes;if(row==failRow){failRow=-1;return -1;}memcpy(state[row],v,16);return 0;}
}
struct Probe{bool ok=true,transformed=false;float bbox[4]={.7f,.4f,.74f,.44f};float nativePivot[4]={.7f,.4f,.74f,.44f};int xcol[4]={6,7,8,9};};
#include "hud_native_scope.inc"
static unsigned checks=0;
static void check(bool yes,const char* why){++checks;if(!yes){printf("FAIL %s\n",why);exit(1);}}
int main(){
 dvr::hudnative::RunePositions runes;float rp[4];
 for(float aspect:{1.f,16.f/9,2.f}) {
  const float tw=1000*aspect,th=1000,sc=std::fmin(tw/1280,th/720),sx=sc/tw,sy=sc/th;
  runes.clear();runes.update(1,900,350,1280,720,1,100);
  const float x=.5f+260*sx,y=.5f-10*sy;
  for(float size:{40.f,48.f,62.f,64.f}) {
   float box[4]={x-size*.5f*sx,y-size*.5f*sy,x+size*.5f*sx,y+size*.5f*sy};
   check(runes.match(box,100,tw,th,rp),"live rune body recognized without edge learning");
   check(std::fabs(rp[0]-x)<.00001f && std::fabs(rp[1]-y)<.00001f,"all artwork shares native center");
  }
  float title[4]={x-86*sx,y-57*sy,x+86*sx,y-7*sy};
  check(runes.match(title,100,tw,th,rp),"description before any icon matches current native parent");
  check(!runes.match(title,201,tw,th,rp),"stale native snapshot refused");
  runes.update(1,900,350,1280,720,0,101);
  check(!runes.match(title,101,tw,th,rp),"hidden rune withdraws ownership");
  runes.update(1,300,350,1280,720,1,102);
  check(!runes.match(title,102,tw,th,rp),"previous position withdrawn when marker moves");
  runes.clear();check(!runes.match(title,102,tw,th,rp),"level reset clears snapshots");
 }
 // Recorded409 steady sample: native761.02/402.74 -> artwork center .594/.5315.
 runes.clear();runes.update(1,761.02f,402.74f,1280,720,3,500);
 const float recorded[4]={.580f,.518f,.608f,.545f};
 check(runes.match(recorded,500,3012,3122,rp),"recorded409 rune inner art matches native canvas mapping");
 check(std::fabs(rp[0]-.594f)<.001f && std::fabs(rp[1]-.5315f)<.001f,"recorded rendered and native centers agree within rounding");
 const float recordedTitle[4]={.526f,.488f,.661f,.526f};
 check(runes.match(recordedTitle,500,3012,3122,rp),"recorded409 description matches same live parent without icon prerequisite");
 IDirect3DDevice9 dev;Probe p;
 shadow[6][0]=2;shadow[7][1]=-2;shadow[8][2]=1;shadow[9][0]=-1;shadow[9][1]=1;shadow[9][3]=1;
 memcpy(state,shadow,sizeof(state));
 {NativeIconScope scope(&dev,p,6);
  check(std::fabs(state[6][0]-1.4f)<.00001f,"native icon geometry shrinks");
  const float centerX=.72f,centerY=.42f;
  check(std::fabs(state[6][0]*centerX+state[9][0]-.44f)<.00001f,"native x center retained");
  check(std::fabs(state[7][1]*centerY+state[9][1]-.16f)<.00001f,"native y center retained");
  check(state[9][3]==1 && state[8][2]==1,"depth and homogeneous w unchanged");
 }
 check(!memcmp(state,shadow,sizeof(state)),"every shader row restored after native draw");
 failRow=7;{NativeIconScope scope(&dev,p,6);check(!memcmp(state,shadow,sizeof(state)),"partial write failure restores original transform");}
 writes=0;p.xcol[1]=6;{NativeIconScope scope(&dev,p,6);check(writes==0,"ambiguous transform rows refused before writes");}
 p=Probe{};p.transformed=true;{NativeIconScope scope(&dev,p,6);check(writes==0,"pretransformed vertices remain native unchanged");}
 const float edge[4]={.922f,.7f,.978f,.756f},text[4]={.3f,.4f,.5f,.44f};
 check(dvr::hudnative::square_icon(edge,8,10) && dvr::hudnative::edge_icon(edge),"observed marker topology and edge location recognized");
 check(!dvr::hudnative::square_icon(text,8,10),"wide title is not an isolated icon");
 check(!dvr::hudnative::square_icon(edge,24,18),"batched geometry not guessed as a marker");
 dvr::hudnative::Markers markers;
 const float center[4]={.48f,.48f,.52f,.52f};
 check(!markers.observe(45,1,center,8,10),"center sprite alone does not prove objective identity");
 check(markers.observe(45,2,edge,8,10),"edge observation seeds marker content");
 check(markers.observe(45,3,center,8,10),"same marker remains native in center interaction region");
 check(!markers.observe(46,3,center,8,10),"another icon does not inherit marker ownership");
 dvr::hudnative::Markers children;
 children.remember(47,4);
 check(children.known(47,5),"learned inner content survives marker movement");
 check(!children.known(48,5),"unrelated content is not learned");
 check(!children.known(47,2406),"inner content expires");
 children.clear();check(!children.known(47,5),"resource reset clears child keys");
 markers.clear();check(!markers.observe(45,4,center,8,10),"resource reset clears learned content");
 dvr::hudnative::MarkerLabels labels;float pivot[4];
 const float label[4]={.43f,.525f,.57f,.55f},far[4]={.1f,.7f,.3f,.73f};
 labels.marker(center,20);
 check(labels.label(label,20,pivot) && pivot[0]==center[0],"title uses marker pivot in same frame");
 check(labels.label(label,21,pivot),"label before marker uses adjacent draw evidence");
 check(!labels.label(label,22,pivot),"stale marker cannot claim text");
 check(!labels.label(far,20,pivot),"distant interaction remains untouched");
 const float outline[4]={.47f,.47f,.53f,.53f},inner[4]={.475f,.475f,.525f,.525f};
 labels.clear();labels.marker(outline,30);
 check(labels.child(inner,30,pivot) && pivot[0]==outline[0],"rune inner artwork shares outline pivot");
 check(labels.child(inner,31,pivot),"child before outline can use previous frame");
 check(!labels.child(inner,32,pivot),"old outline cannot claim a child");
 const float dot[4]={.498f,.498f,.502f,.502f};check(!labels.child(dot,30,pivot),"tiny reticle cannot join larger outline");
 check(!labels.child(far,30,pivot),"distant HUD cannot join marker");
 const float shifted[4]={.48f,.475f,.53f,.525f};
 check(!labels.child(shifted,30,pivot),"off-center artwork refused");
 labels.clear();check(!labels.child(inner,30,pivot),"reset clears inner association");
 labels.clear();check(!labels.label(label,20,pivot),"menu/reset clears label geometry");
 p=Probe{};p.bbox[0]=.4f;p.bbox[1]=.55f;p.bbox[2]=.6f;p.bbox[3]=.58f;
 for(int i=0;i<4;++i)p.nativePivot[i]=center[i];
 {NativeIconScope scope(&dev,p,6);
  const float labelY=state[7][1]*.565f+state[9][1];
  check(std::fabs(labelY-(-.13f*.7f))<.00001f,"native label spacing scales about marker center, not its own text center");
 }
 check(!memcmp(state,shadow,sizeof(state)),"label scaling restores all shader constants");
 float vp[16]={0,0,0,1, 2,0,0,0, 0,3,0,0, 0,0,1,0},co,si,aspect;
 check(dvr::hudnative::upright_basis(vp,co,si,aspect) && co==1 && si==0,"world up projects upright without head tilt");
 vp[4]=1.7320508f;vp[5]=-1.5f;vp[8]=1;vp[9]=2.5980762f;
 check(dvr::hudnative::upright_basis(vp,co,si,aspect) && std::fabs(si+.5f)<.0001f,"rendered roll projects world up independently of yaw");
 check(std::fabs(aspect-2.f/3)<.0001f,"asymmetric pixel dimensions preserve focal aspect");
 vp[3]=0;check(!dvr::hudnative::upright_basis(vp,co,si,aspect),"non-perspective matrix refused");
 p=Probe{};upright=true;basisCo=0;basisSi=-1;basisAspect=2.f/3;
 {NativeIconScope scope(&dev,p,6);
  const float cx=.72f,cy=.42f;
  check(std::fabs(state[6][0]*cx+state[7][0]*cy+state[9][0]-.44f)<.0001f,"upright transform retains marker target x");
  check(std::fabs(state[6][1]*cx+state[7][1]*cy+state[9][1]-.16f)<.0001f,"upright transform retains marker target y");
  check(state[9][3]==1 && state[8][2]==1,"upright preserves depth and clip w");
 }
 check(!memcmp(state,shadow,sizeof(state)),"upright restores shader state");
 basisValid=false;basisCo=1;basisSi=0;basisAspect=1;
 {NativeIconScope scope(&dev,p,6);check(std::fabs(state[6][0]-1.4f)<.0001f,"unavailable render basis retains accepted sizing");}
 using namespace dvr::hudlayout;
 using namespace dvr::hudcap;
 check(!redirect_healthy(),"no draw does not prove a healthy redirect");
 g_nativeGameplayReference=true;upright=true;writes=0;
 {NativeIconScope scope(&dev,p,6);check(writes==0,"reference bypasses both sizing and roll shader writes");}
 note_native_reference(-1);check(!redirect_healthy(),"failed native draw does not permit menu entry");
 note_native_reference(0);check(redirect_healthy(),"successful intentional native draw allows menu entry");
 g_failed=true;check(!redirect_healthy(),"native reference cannot mask a latched D3D failure");g_failed=false;
 g_handoffReady=false;check(!redirect_healthy(),"native reference requires handoff readiness");g_handoffReady=true;
 g_on=false;check(!redirect_healthy(),"disabled capture does not ride menus");g_on=true;
 g_visualRiding=true;g_menuRiding=true;
 check(!native_gameplay_reference(),"menu and wheel closing visual ownership supersede reference");
 check(native_objective_scale(6)==1 && !native_objective_upright(6),"menu HUD is not objective corrected");
 nowMs+=501;note_native_reference(0);check(!redirect_healthy(),"menu calls cannot refresh native gameplay heartbeat");
 g_visualRiding=false;g_menuRiding=false;g_nativeGameplayReference=false;
 check(native_objective_scale(6)==scale && native_objective_upright(6),"turning reference off restores saved objective settings");
 check(native_objective_scale(-1)==1 && !native_objective_upright(-1),"unidentified draws never inherit objective transforms");
 printf("%u native HUD checks passed\n",checks);
}
