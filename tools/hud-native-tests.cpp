#include "core/gfx/hud_native_icon.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
 markers.clear();check(!markers.observe(45,4,center,8,10),"resource reset clears learned content");
 dvr::hudnative::MarkerLabels labels;float pivot[4];
 const float label[4]={.43f,.525f,.57f,.55f},far[4]={.1f,.7f,.3f,.73f};
 labels.marker(center,20);
 check(labels.label(label,20,pivot) && pivot[0]==center[0],"title uses marker pivot in same frame");
 check(labels.label(label,21,pivot),"label before marker uses adjacent draw evidence");
 check(!labels.label(label,22,pivot),"stale marker cannot claim text");
 check(!labels.label(far,20,pivot),"distant interaction remains untouched");
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
