#include "core/gfx/hud_native_icon.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
struct IDirect3DDevice9{};
using HRESULT=int;
#define FAILED(x) ((x)<0)
#define DVR_LOG_EVERY_MS(...) ((void)0)
static float scale=.7f,state[256][4]{},shadow[256][4]{};static int failRow=-1,writes=0;
namespace dvr::hudlayout {float native_objective_scale(int){return scale;}}
namespace dvr::frame {
const float* vs_const_shadow_row(int row){return shadow[row];}
HRESULT orig_set_vs_const(IDirect3DDevice9*,int row,const float* v,int){++writes;if(row==failRow){failRow=-1;return -1;}memcpy(state[row],v,16);return 0;}
}
struct Probe{bool ok=true,transformed=false;float bbox[4]={.7f,.4f,.74f,.44f};int xcol[4]={6,7,8,9};};
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
 printf("%u native HUD checks passed\n",checks);
}
