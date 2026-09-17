// VR-129: task-only parent update call. Native code owns marker lifetime,
// child direction, distance and visibility. No retained native marker pointers.
#include "game/dishonored/objective_marker_policy.h"
namespace dvr::objectivemarkers {
namespace {std::atomic<bool> on{false};std::atomic<float> margin{.12f};}
bool enabled(){return on.load();}
float inset(){return margin.load();}
void configure(bool active,float value){margin.store(std::isfinite(value)?fmaxf(.05f,fminf(.30f,value)):.12f);on.store(active);}
}
namespace {
using TaskParentFn=void (__thiscall*)(void*,float,float,uint32_t,uint32_t,float,uint32_t);
bool g_taskHook=false,g_taskFailed=false;
uint32_t g_taskCalls=0,g_taskMoved=0,g_taskRefused=0;
LONG g_taskLoad=-1;unsigned g_taskEpoch=~0u;double g_taskRefresh=0;
// This pointer is borrowed from the verified task-update caller, never cached.
bool TaskInputs(void* marker,int& w,int& h,const char*& reason) {
    reason="marker-layout";
    auto* p=(uint8_t*)marker;
    if(!RangeReadable(p,kTaskMarkerParams+4) || *(uintptr_t*)p!=kTaskMarkerVtable) return false;
    reason="owner-refresh";
    auto* owner=*(uint8_t**)(p+kTaskMarkerOwner);
    const unsigned epoch=UiSurfaceEpoch();const double now=MaimNowMs();
    if(g_taskLoad!=g_mkLoadEvents || g_taskEpoch!=epoch || (!IsLiveObject(owner) && now>=g_taskRefresh)) {
        g_taskRefresh=now+1000;
        if(!BuildLiveSet()) return false;
        g_taskLoad=g_mkLoadEvents;g_taskEpoch=epoch;
    }
    // Validate a current UObject owner, not its class spelling or pointer reuse.
    reason="owner-not-live";
    if(!IsLiveObject(owner)) return false;
    reason="params-unreadable";
    auto* params=*(uint8_t**)(p+kTaskMarkerParams);
    if(!RangeReadable(params,kTaskMarkerHeight+4)) return false;
    memcpy(&w,params+kTaskMarkerWidth,4);memcpy(&h,params+kTaskMarkerHeight,4);
    const bool valid=w>=64 && h>=64 && w<=16384 && h<=16384;
    reason=valid?"validated":"dimensions-out-of-range";return valid;
}
__declspec(noinline) void __fastcall TaskParentStub(void* marker,void*,float x,float y,uint32_t a,uint32_t b,float distance,uint32_t flags) {
    const float oldX=x,oldY=y;int w=0,h=0;
    const bool want=dvr::objectivemarkers::enabled() && dvr::vr::session_live() &&
        dvr::stereo::wants_projection() && !UiSurfaceRidesHud() && !g_gameExiting;
    bool valid=false,moved=false;const char* reason="inactive";
    if(want && (uintptr_t)_ReturnAddress()==kTaskParentReturn) {
        valid=TaskInputs(marker,w,h,reason);
        if(valid) moved=dvr::objectivemarkers::inset_position(x,y,w,h,flags,dvr::objectivemarkers::inset());
        else ++g_taskRefused;
    }
    ++g_taskCalls;if(moved)++g_taskMoved;
    ((TaskParentFn)kTaskParentUpdate)(marker,x,y,a,b,distance,flags);
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "hud/task-parent: calls=%u moved=%u refused=%u want=%d ownerValid=%d guard=%s flags=%x dimensions=%dx%d xy=%.2f/%.2f -> %.2f/%.2f distance=%.2f inset=%.3f; native children retained, draw ownership not yet established",
        g_taskCalls,g_taskMoved,g_taskRefused,(int)want,(int)valid,reason,flags,w,h,oldX,oldY,x,y,distance,dvr::objectivemarkers::inset());
}
bool TaskParentFingerprint(const uint8_t* bytes) {
    if(memcmp(bytes,kTaskParentCallBytes,sizeof(kTaskParentCallBytes))) return false;
    int32_t rel=0;memcpy(&rel,bytes+1,4);
    return kTaskParentReturn+rel==kTaskParentUpdate;
}
}
static void ObjectiveMarkersApply() {
    if(!dvr::objectivemarkers::enabled() || g_taskHook || g_taskFailed) return;
    auto* site=(uint8_t*)kTaskParentCall;
    if(!RangeReadable(site,5) || !TaskParentFingerprint(site) ||
       !RangeReadable((void*)kTaskParentUpdate,sizeof(kTaskParentProlog)) ||
       memcmp((void*)kTaskParentUpdate,kTaskParentProlog,sizeof(kTaskParentProlog))) {
        g_taskFailed=true;Log("hud/task-parent: REFUSED verified task call/callee fingerprint; native behavior retained");return;
    }
    DWORD previous=0;
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&previous)) {g_taskFailed=true;Log("hud/task-parent: REFUSED protection error=%lu",GetLastError());return;}
    const int32_t rel=(int32_t)((uintptr_t)&TaskParentStub-kTaskParentReturn);
    memcpy(site+1,&rel,4);DWORD ignored=0;
    const bool restored=VirtualProtect(site,5,previous,&ignored)!=0;
    FlushInstructionCache(GetCurrentProcess(),site,5);g_taskHook=true;
    Log("hud/task-parent: installed task-only parent call; six stack args/ret24; protectionRestored=%d; no scene/stereo changes",(int)restored);
}
