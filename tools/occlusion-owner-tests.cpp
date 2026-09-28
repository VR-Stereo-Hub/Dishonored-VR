#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#define Log(...) ((void)0)
#define DVR_LOG_EVERY_MS(...) ((void)0)
static uint8_t* g_peCtrl=nullptr,*g_pcObj=nullptr;
static LONG g_mkLoadEvents=0;
static unsigned epoch=1,builds=0;
static bool refreshOk=true,allocationChangesController=false;
static uint8_t controllerA[256]{},controllerB[256]{},playerA[256]{},playerB[256]{};
static int leftA,leftB,rightState,engineReplacement;
static constexpr uint32_t playerOffset=16,kLocalPlayerViewStateOff=136;
static std::unordered_map<void*,unsigned> identities;
struct CtIdentity {struct {void* obj=nullptr;}value;unsigned version=0;};
static bool IsLiveObject(uint8_t* p){return identities.count(p)!=0;}
static bool ChSlot(const CtIdentity& id){return IsLiveObject((uint8_t*)id.value.obj) && identities[id.value.obj]==id.version;}
static bool ChCapture(uint8_t* p,CtIdentity* out){if(!IsLiveObject(p))return false;out->value.obj=p;out->version=identities[p];return true;}
static bool BuildLiveSet(){++builds;return refreshOk;}
static unsigned UiSurfaceEpoch(){return epoch;}
static bool RangeReadable(const void* p,size_t){return p!=nullptr;}
static const char* ObjClassName(uint8_t*){return "LocalPlayer";}
static uint32_t FindPropOffset(const char*,const char* field){return strcmp(field,"Player")?kLocalPlayerViewStateOff:playerOffset;}
static void* Allocate(){if(allocationChangesController)g_peCtrl=controllerA;return &rightState;}
static uintptr_t kAllocateViewState=(uintptr_t)&Allocate;
static uint8_t bytes[16]{};
static uintptr_t kOcclReaderViewSetup=(uintptr_t)bytes,kOcclReaderDepthPass=(uintptr_t)bytes,kLocalPlayerAllocSite=(uintptr_t)bytes;
static uint8_t kOcclReaderViewSetupBytes[1]{},kOcclReaderDepthPassBytes[1]{},kAllocateViewStatePrefix[1]{},kLocalPlayerAllocSiteBytes[1]{};
static uint32_t ignoreQueries=0;
static uintptr_t kIgnoreAllOcclusionQueries=(uintptr_t)&ignoreQueries;
static bool SafeRead32(uintptr_t p,uint32_t* v){*v=*(uint32_t*)p;return true;}
static void ConfigWriteKey(const char*,const char*,const char*,const char*){}
#include "game/dishonored/stereo_occlusion.cpp"
static void*& view(uint8_t* p){return *(void**)(p+kLocalPlayerViewStateOff);}
static void bind(uint8_t* c,uint8_t* p){*(uint8_t**)(c+playerOffset)=p;}
static unsigned checks=0;
static void check(bool b,const char* msg){++checks;if(!b){printf("FAIL %u: %s\n",checks,msg);std::exit(1);}}
static void reset(){
 identities.clear();identities[controllerA]=identities[controllerB]=identities[playerA]=identities[playerB]=1;
 bind(controllerA,playerA);bind(controllerB,playerB);view(playerA)=&leftA;view(playerB)=&leftB;
 g_pcObj=controllerA;g_peCtrl=controllerB;g_mkLoadEvents=0;epoch=1;builds=0;refreshOk=true;allocationChangesController=false;
 g_occlMode=OCCL_PEREYE;g_occlRefused=0;g_occlLp=nullptr;g_occlLeft=nullptr;g_occlRight=nullptr;g_occlSwapped=false;
 g_occlHaveOwners=false;g_occlOwners[0]={};g_occlOwners[1]={};g_occlOwnerRetry=0;g_occlPcPlayerOff=0;
 g_occlSwaps=0;g_occlAttempts=g_occlRestores=g_occlRestoreRefused=0;
}
int main(){
 reset();OcclusionPass2Begin();
 check(view(playerB)==&rightState && view(playerA)==&leftA,"event controller wins over still-live scan controller");
 check(builds==1,"new owner refreshes current object table");OcclusionPass2End();
 check(view(playerB)==&leftB && g_occlRestores==1,"right state restored after pass");
 OcclusionPass2Begin();OcclusionPass2End();check(builds==1,"steady draws validate slots without rebuilding");
 ++epoch;OcclusionPass2Begin();OcclusionPass2End();check(builds==2,"menu epoch revalidates even unchanged pointers");
 ++g_mkLoadEvents;g_peCtrl=controllerA;OcclusionPass2Begin();
 check(view(playerA)==&rightState && view(playerB)==&leftB,"save load switches to new gameplay owner");OcclusionPass2End();
 check(view(playerA)==&leftA,"new owner restores its own state");
 reset();identities.erase(controllerB);OcclusionPass2Begin();check(view(playerA)==&leftA && !g_occlSwapped,"dead event owner never falls back to stale scan owner");
 OcclusionPass2Begin();check(builds==1,"unavailable owner refresh bounded to one per second");
 reset();refreshOk=false;OcclusionPass2Begin();check(!g_occlSwapped && view(playerB)==&leftB,"failed live table blocks write");
 reset();OcclusionPass2Begin();identities.erase(playerB);OcclusionPass2End();
 check(view(playerB)==&rightState && g_occlRestoreRefused==1,"freed LocalPlayer not written during restore");
 reset();OcclusionPass2Begin();++identities[playerB];OcclusionPass2End();
 check(g_occlRestoreRefused==1 && view(playerB)==&rightState,"same address with new identity not restored");
 reset();OcclusionPass2Begin();view(playerB)=&engineReplacement;OcclusionPass2End();
 check(view(playerB)==&engineReplacement,"engine replacement never overwritten");
 reset();OcclusionPass2Begin();++epoch;OcclusionPass2End();check(builds==2 && view(playerB)==&leftB,"menu inside draw refreshes before restore");
 reset();OcclusionPass2Begin();++g_mkLoadEvents;refreshOk=false;OcclusionPass2End();
 check(g_occlRestoreRefused==1 && view(playerB)==&rightState,"load with unavailable live table refuses restore");
 reset();allocationChangesController=true;OcclusionPass2Begin();check(!g_occlSwapped && view(playerB)==&leftB,"allocator callback owner change prevents write");
 reset();OcclusionPass2Begin();OcclusionPass2End();bind(controllerB,playerA);OcclusionPass2Begin();
 check(builds==2 && view(playerA)==&rightState,"new LocalPlayer reference refreshes and adopts");OcclusionPass2End();
 reset();g_occlMode=OCCL_NATIVE;OcclusionPass2Begin();check(!g_occlAttempts && view(playerB)==&leftB,"native mode unchanged");
 g_occlMode=OCCL_OFF;OcclusionPass2Begin();check(!g_occlAttempts,"off mode unchanged");
 printf("production occlusion owner: %u checks PASS\n",checks);return 0;
}
