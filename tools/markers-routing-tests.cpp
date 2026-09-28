#include "core/gfx/hud_layout.h"
#include "core/gfx/hud_owner.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace dvr::hudlayout;
static dvr::hudowner::Owner fixture;
namespace dvr::hudowner {
bool active(){return true;} Owner current(){return fixture;} void note_route(bool){}
}
static bool g_visualRiding=false,g_nativeObjectives=true;
static unsigned g_routeFrame=0,g_routeOverflow=0,g_presentNo=1,g_routeCounts[ElCount]{},g_seen[ElCount]{},g_lastRouted[ElCount]{};
static int g_elementSink[ElCount]{},g_sinkOf[AnchorCount][2]{};
struct Element{int anchor=AnchorFrame;}g_el[ElCount];
static bool crop_eligible(int){return false;}
static int acquire_sink(int,bool,int){return 2;}
#include "semantic_route.inc"
static unsigned checks=0;
static void check(bool v,const char* msg){++checks;if(!v){printf("FAIL %s\n",msg);exit(1);}}
int main(){
 for(int e: {ElObjective,ElDetection,ElPrompt}) for(int marker=0;marker<2;++marker)
 for(int native=0;native<2;++native) for(int frame=0;frame<2;++frame){
   fixture={};fixture.root=1;fixture.generation=1;fixture.element=e;fixture.marker=marker!=0;
   fixture.pivotValid=true;fixture.pivot[0]=.2f;fixture.pivot[1]=.7f;
   g_nativeObjectives=native!=0;g_el[e].anchor=frame?AnchorFrame:0;
   bool sharp=true;int element=-1;float pivot[4]{};const int sink=route(&element,pivot,&sharp);
   const bool expected=marker && (native || e==ElDetection);
   check(sharp==expected,"semantic marker handoff matches owner and native policy");
   check((sink==-1)==(expected||frame),"original native versus sink routing preserved");
   check(element==e && pivot[0]==.2f && pivot[3]==.7f,"element and per-eye pivot preserved");
 }
 fixture={};bool sharp=true;int e=0;check(route(&e,nullptr,&sharp)==-1 && !sharp && e==-1,"unknown owner remains native, never marker overlay");
 g_visualRiding=true;sharp=true;check(route(&e,nullptr,&sharp)==-2 && !sharp,"riding menu cannot acquire native marker overlay");
 printf("semantic marker routing: %u checks PASS\n",checks);
}
