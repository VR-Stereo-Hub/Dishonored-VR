// VR-129: task-only parent update call. Native code owns marker lifetime,
// child direction, distance and visibility. No retained native marker pointers.
#include "game/dishonored/objective_marker_policy.h"
namespace dvr::objectivemarkers {
namespace {std::atomic<bool> on{false},runes{false};std::atomic<float> margin{.12f},runeMargin{.12f};}
bool rune_enabled(){return runes.load();}
float rune_inset(){return runeMargin.load();}
void configure_runes(bool active,float value){runeMargin.store(std::isfinite(value)?fmaxf(.05f,fminf(.30f,value)):.12f);runes.store(active);}
bool enabled(){return on.load();}
float inset(){return margin.load();}
void configure(bool active,float value){margin.store(std::isfinite(value)?fmaxf(.05f,fminf(.30f,value)):.12f);on.store(active);}
}
namespace {
using TaskParentFn=void (__thiscall*)(void*,float,float,uint32_t,uint32_t,float,uint32_t);
bool g_taskHook=false,g_taskFailed=false,g_runeHook=false,g_runeFailed=false;
uint32_t g_runeCalls=0,g_runeMoved=0,g_runeRefused=0;
uint32_t g_taskCalls=0,g_taskMoved=0,g_taskRefused=0;
LONG g_taskLoad=-1;unsigned g_taskEpoch=~0u;double g_taskRefresh=0;
// This pointer is borrowed from the verified task-update caller, never cached.
bool MarkerInputs(void* marker,int& w,int& h,const char*& reason,uintptr_t vtable) {
    reason="marker-layout";
    auto* p=(uint8_t*)marker;
    if(!RangeReadable(p,kTaskMarkerParams+4) || *(uintptr_t*)p!=vtable) return false;
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
bool TaskInputs(void* marker,int& w,int& h,const char*& reason) {
    return MarkerInputs(marker,w,h,reason,kTaskMarkerVtable);
}
bool RuneInputs(void* marker,int& w,int& h,const char*& reason) {
    if(!MarkerInputs(marker,w,h,reason,kHeartMarkerVtable)) return false;
    // Settings come from the current borrowed native marker, never cached.
    auto* settings=*(uint8_t**)((uint8_t*)marker+kMarkerSettings);
    reason="rune-symbol-layout";
    if(!RangeReadable(settings,kMarkerSymbolCapacity+4)) return false;
    const int count=*(int*)(settings+kMarkerSymbolCount),capacity=*(int*)(settings+kMarkerSymbolCapacity);
    if(count!=11 || capacity<count || capacity>4096) return false;
    auto* text=*(wchar_t**)(settings+kMarkerSymbolData);
    if(!RangeReadable(text,count*sizeof(wchar_t))) return false;
    reason="other-heart-marker";
    if(!dvr::objectivemarkers::rune_symbol(text,count)) return false;
    reason="validated-rune";return true;
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
__declspec(noinline) void __fastcall RuneParentStub(void* marker,void*,float x,float y,uint32_t a,uint32_t b,float distance,uint32_t flags) {
    const float oldX=x,oldY=y;int w=0,h=0;
    const bool want=dvr::objectivemarkers::rune_enabled() && dvr::vr::session_live() &&
        dvr::stereo::wants_projection() && !UiSurfaceRidesHud() && !g_gameExiting;
    bool valid=false,moved=false;const char* reason="inactive";
    if(want && (uintptr_t)_ReturnAddress()==kRuneParentReturn) {
        valid=RuneInputs(marker,w,h,reason);
        if(valid) moved=dvr::objectivemarkers::inset_position(x,y,w,h,flags,dvr::objectivemarkers::rune_inset());
        else ++g_runeRefused;
    }
    ++g_runeCalls;if(moved)++g_runeMoved;
    ((TaskParentFn)kTaskParentUpdate)(marker,x,y,a,b,distance,flags);
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "hud/rune-parent: calls=%u moved=%u refused=%u want=%d ownerValid=%d guard=%s flags=%x dimensions=%dx%d xy=%.2f/%.2f -> %.2f/%.2f distance=%.2f inset=%.3f; native children retained, draw ownership not yet established",
        g_runeCalls,g_runeMoved,g_runeRefused,(int)want,(int)valid,reason,flags,w,h,oldX,oldY,x,y,distance,dvr::objectivemarkers::rune_inset());
}
bool TaskParentFingerprint(const uint8_t* bytes) {
    if(memcmp(bytes,kTaskParentCallBytes,sizeof(kTaskParentCallBytes))) return false;
    int32_t rel=0;memcpy(&rel,bytes+1,4);
    return kTaskParentReturn+rel==kTaskParentUpdate;
}
bool RuneParentFingerprint(const uint8_t* bytes) {
    if(memcmp(bytes,kRuneParentCallBytes,sizeof(kRuneParentCallBytes))) return false;
    int32_t rel=0;memcpy(&rel,bytes+1,4);
    return kRuneParentReturn+rel==kTaskParentUpdate;
}
}
static void NativeMarkerInstall(uintptr_t address,uintptr_t returnAddress,bool (*fingerprint)(const uint8_t*),void* stub,bool& hooked,bool& failed,const char* kind) {
    if(hooked || failed) return;
    auto* site=(uint8_t*)address;
    if(!RangeReadable(site,5) || !fingerprint(site) ||
       !RangeReadable((void*)kTaskParentUpdate,sizeof(kTaskParentProlog)) ||
       memcmp((void*)kTaskParentUpdate,kTaskParentProlog,sizeof(kTaskParentProlog))) {
        failed=true;Log("hud/%s-parent: REFUSED call/callee fingerprint; native behavior retained",kind);return;
    }
    DWORD previous=0;
    if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&previous)) {failed=true;Log("hud/%s-parent: REFUSED protection error=%lu",kind,GetLastError());return;}
    const int32_t rel=(int32_t)((uintptr_t)stub-returnAddress);
    memcpy(site+1,&rel,4);DWORD ignored=0;
    const bool restored=VirtualProtect(site,5,previous,&ignored)!=0;
    FlushInstructionCache(GetCurrentProcess(),site,5);hooked=true;
    Log("hud/%s-parent: installed native parent call; six stack args/ret24; protectionRestored=%d; no scene/stereo changes",kind,(int)restored);
}
static void ObjectiveMarkersApply() {
    if(dvr::objectivemarkers::enabled()) NativeMarkerInstall(kTaskParentCall,kTaskParentReturn,&TaskParentFingerprint,(void*)&TaskParentStub,g_taskHook,g_taskFailed,"task");
    if(dvr::objectivemarkers::rune_enabled()) NativeMarkerInstall(kRuneParentCall,kRuneParentReturn,&RuneParentFingerprint,(void*)&RuneParentStub,g_runeHook,g_runeFailed,"rune");
}
