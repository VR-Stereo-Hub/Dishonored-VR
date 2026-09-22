// VR-129: task-only parent update call. Native code owns marker lifetime,
// child direction, distance and visibility. No retained native marker pointers.
#include "game/dishonored/objective_marker_policy.h"
#include "core/gfx/hud_native_rune.h"
#include <mutex>
namespace dvr::objectivemarkers {
namespace {std::atomic<bool> on{false},runes{false};std::atomic<float> margin{.12f},runeMargin{.12f};}
namespace {std::atomic<bool> runeOwnership{false};std::mutex runeMutex;dvr::hudnative::RunePositions runePositions;}
namespace {std::atomic<bool> heartAll{false};}
namespace {std::atomic<bool> awareness{false};std::mutex awarenessMutex;dvr::hudnative::AwarenessPositions awarenessPositions;}
// VR-152: the newest publish, readable without the lock. match_awareness_draw
// runs on the render thread for EVERY gameplay HUD draw, and the common case by
// far is that no awareness meter is live at all - nobody has noticed you. Taking
// a mutex per draw to discover that is contention for nothing. A published
// sample is only usable for 100 ms (AwarenessPositions::match drops anything
// older), so one atomic stamp answers the common case before any lock.
namespace {std::atomic<uint32_t> awarenessStampMs{0};}
// VR-185: the task markers' published positions, same lock-free gate as awareness.
namespace {std::atomic<bool> taskHooked{false};std::mutex taskMutex;dvr::hudnative::TaskPositions taskPositions;std::atomic<uint32_t> taskStampMs{0};}
bool task_ownership(){return taskHooked.load() && on.load();}
void clear_task_positions(){taskStampMs.store(0,std::memory_order_release);std::lock_guard<std::mutex> lock(taskMutex);taskPositions.clear();}
void publish_task(uintptr_t token,float x,float y,int w,int h,uint32_t flags){
    const uint32_t now=GetTickCount();
    {std::lock_guard<std::mutex> lock(taskMutex);taskPositions.update(token,x,y,w,h,flags,now);}
    if(flags&1) taskStampMs.store(now,std::memory_order_release);
}
int match_task_draw(const float* rect,float w,float h,float* pivot,float* offset){
    if(!task_ownership())return 0;
    const uint32_t stamp=taskStampMs.load(std::memory_order_acquire);
    if(!stamp || GetTickCount()-stamp>100)return 0;
    std::lock_guard<std::mutex> lock(taskMutex);return taskPositions.match(rect,GetTickCount(),w,h,pivot,offset);
}
bool task_visible(){
    const uint32_t stamp=taskStampMs.load(std::memory_order_acquire);
    return task_ownership() && stamp && GetTickCount()-stamp<=100;
}
void task_report(float* icon,float* text,unsigned& iconMatched,unsigned& textMatched,unsigned& ambiguous){
    std::lock_guard<std::mutex> lock(taskMutex);
    const auto& a=taskPositions.icon;const auto& b=taskPositions.text;
    icon[0]=a.w;icon[1]=a.h;icon[2]=a.dx;icon[3]=a.dy;text[0]=b.w;text[1]=b.h;text[2]=b.dx;text[3]=b.dy;
    iconMatched=a.matched;textMatched=b.matched;ambiguous=taskPositions.ambiguous;
}
void set_task_hooked(bool hooked){taskHooked.store(hooked);clear_task_positions();}
bool rune_ownership(){return runeOwnership.load();}
void clear_rune_positions(){std::lock_guard<std::mutex> lock(runeMutex);runePositions.clear();}
void configure_rune_ownership(bool active){runeOwnership.store(active);clear_rune_positions();}
void publish_rune(uintptr_t token,float x,float y,int w,int h,uint32_t flags){
    std::lock_guard<std::mutex> lock(runeMutex);runePositions.update(token,x,y,w,h,flags,GetTickCount());
}
bool match_rune_draw(const float* rect,float w,float h,float* pivot){
    if(!runeOwnership.load() || !runes.load())return false;
    std::lock_guard<std::mutex> lock(runeMutex);return runePositions.match(rect,GetTickCount(),w,h,pivot);
}
bool rune_enabled(){return runes.load();}
float rune_inset(){return runeMargin.load();}
void configure_runes(bool active,float value){runeMargin.store(std::isfinite(value)?fmaxf(.05f,fminf(.30f,value)):.12f);runes.store(active);}
bool heart_all_symbols(){return heartAll.load();}
void configure_heart_all_symbols(bool active){heartAll.store(active);clear_rune_positions();}
bool awareness_enabled(){return awareness.load();}
void clear_awareness_positions(){awarenessStampMs.store(0,std::memory_order_release);std::lock_guard<std::mutex> lock(awarenessMutex);awarenessPositions.clear();}
void configure_awareness(bool active){awareness.store(active);clear_awareness_positions();}
void publish_awareness(uintptr_t token,float x,float y,int w,int h,uint32_t flags){
    const uint32_t now=GetTickCount();
    {std::lock_guard<std::mutex> lock(awarenessMutex);awarenessPositions.update(token,x,y,w,h,flags,now);}
    // Only a VISIBLE sample opens the matcher. A hidden or refused instance
    // withdraws its point and must not keep the render thread locking.
    if(flags&1) awarenessStampMs.store(now,std::memory_order_release);
}
bool match_awareness_draw(const float* rect,float w,float h,float* pivot){
    if(!awareness.load(std::memory_order_relaxed))return false;
    // The lock-free gate: nothing published, or nothing published recently
    // enough for match() to accept, so there is nothing to lock for.
    const uint32_t stamp=awarenessStampMs.load(std::memory_order_acquire);
    if(!stamp || GetTickCount()-stamp>100)return false;
    std::lock_guard<std::mutex> lock(awarenessMutex);return awarenessPositions.match(rect,GetTickCount(),w,h,pivot);
}
void awareness_report(float& worstW,float& worstH,float& worstDx,float& worstDy,unsigned& matched,unsigned& ambiguous){
    std::lock_guard<std::mutex> lock(awarenessMutex);
    worstW=awarenessPositions.worstW;worstH=awarenessPositions.worstH;
    worstDx=awarenessPositions.worstDx;worstDy=awarenessPositions.worstDy;
    matched=awarenessPositions.matched;ambiguous=awarenessPositions.ambiguous;
}
bool enabled(){return on.load();}
float inset(){return margin.load();}
void configure(bool active,float value){margin.store(std::isfinite(value)?fmaxf(.05f,fminf(.30f,value)):.12f);on.store(active);}
}
namespace {
using TaskParentFn=void (__thiscall*)(void*,float,float,uint32_t,uint32_t,float,uint32_t);
bool g_taskHook=false,g_taskFailed=false,g_runeHook=false,g_runeFailed=false;
bool g_awareHook=false,g_awareFailed=false;
uint32_t g_awareCalls=0,g_awareMine=0,g_awareRefused=0;
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
        if(g_taskLoad!=g_mkLoadEvents || g_taskEpoch!=epoch){dvr::objectivemarkers::clear_rune_positions();dvr::objectivemarkers::clear_task_positions();}
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
// VR-149: every distinct Heart symbol a run sees, named once, up to eight.
// Bone charms ride the SAME vtable and the SAME update as runes, so the symbol
// is the only discriminator, and its bone-charm spelling has never been read -
// it is authored in the SWF and is not in the image as ANSI or UTF-16. This
// census is how it gets read, and it prints whether the symbol was accepted or
// refused, so the line is useful in both directions rather than only when a
// guess happened to be right.
void HeartSymbolCensus(const wchar_t* text,int count,bool accepted) {
    static uint32_t told[8]; static int toldN=0;
    char name[72]; const int n=dvr::objectivemarkers::heart_symbol_ascii(text,count,name,sizeof(name));
    if(!n) return;
    uint32_t hash=2166136261u;
    for(const char* q=name;*q;++q){hash^=(uint8_t)*q;hash*=16777619u;}
    for(int i=0;i<toldN;++i) if(told[i]==hash) return;
    if(toldN>=8) return;
    told[toldN++]=hash;
    Log("hud/heart-symbol: '%s' (%d chars incl terminator) %s. runeMarker is the "
        "measured rune symbol; every OTHER symbol here is a Heart collectible the "
        "mod is not yet treating, and bone charms are the expected one. [Hud] "
        "NativeHeartAllSymbols=1 (F10 HUD) accepts them all",
        name,count,accepted?"ACCEPTED":"refused");
}
bool RuneInputs(void* marker,int& w,int& h,const char*& reason) {
    if(!MarkerInputs(marker,w,h,reason,kHeartMarkerVtable)) return false;
    // Settings come from the current borrowed native marker, never cached.
    auto* settings=*(uint8_t**)((uint8_t*)marker+kMarkerSettings);
    reason="rune-symbol-layout";
    if(!RangeReadable(settings,kMarkerSymbolCapacity+4)) return false;
    const int count=*(int*)(settings+kMarkerSymbolCount),capacity=*(int*)(settings+kMarkerSymbolCapacity);
    if(!dvr::objectivemarkers::heart_symbol_layout(count,capacity)) return false;
    auto* text=*(wchar_t**)(settings+kMarkerSymbolData);
    if(!RangeReadable(text,count*sizeof(wchar_t))) return false;
    const bool rune=dvr::objectivemarkers::rune_symbol(text,count);
    const bool all=dvr::objectivemarkers::heart_all_symbols();
    if(!rune) HeartSymbolCensus(text,count,all);
    reason="other-heart-marker";
    if(!rune && !all) return false;
    reason=rune?"validated-rune":"validated-heart-symbol";return true;
}
bool AwarenessInputs(void* marker,int& w,int& h,const char*& reason) {
    // No symbol test: kAwarenessMarkerVtable is the awareness family own vtable, not
    // shared the way the Heart vtable is. The owner, params and dimension
    // checks in MarkerInputs are the same ones the other two families use.
    if(!MarkerInputs(marker,w,h,reason,kAwarenessMarkerVtable)) return false;
    reason="validated-awareness";return true;
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
    // VR-185: publish where the engine put this marker (after the inset), so
    // the router claims its icon, title and distance by position. Hidden,
    // inactive and refused instances withdraw their previous sample.
    dvr::objectivemarkers::publish_task((uintptr_t)marker,x,y,w,h,valid?flags:0);
    ++g_taskCalls;if(moved)++g_taskMoved;
    ((TaskParentFn)kTaskParentUpdate)(marker,x,y,a,b,distance,flags);
    // Never pay for a line you do not print: the report takes the position mutex.
    static double nextCensusMs=0;
    const double censusNow=MaimNowMs();
    if(censusNow<nextCensusMs) return;
    nextCensusMs=censusNow+1000.0;
    float icon[4]{},text[4]{};unsigned iconN=0,textN=0,ambiguous=0;
    dvr::objectivemarkers::task_report(icon,text,iconN,textN,ambiguous);
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "hud/task-parent: calls=%u moved=%u refused=%u want=%d ownerValid=%d guard=%s flags=%x dimensions=%dx%d xy=%.2f/%.2f -> %.2f/%.2f distance=%.2f inset=%.3f "
        "| draws claimed by position: icon=%u (widest %.0fx%.0f px at %+.0f/%+.0f; window 96x96 +/-48) text=%u (widest %.0fx%.0f px at %+.0f/%+.0f; window 640x160 dx+/-64 dy-176..+112) ambiguous=%u "
        "(the windows are BOUNDS, these numbers tighten them; text=0 while a titled marker is on screen means its title was not claimed)",
        g_taskCalls,g_taskMoved,g_taskRefused,(int)want,(int)valid,reason,flags,w,h,oldX,oldY,x,y,distance,dvr::objectivemarkers::inset(),
        iconN,icon[0],icon[1],icon[2],icon[3],textN,text[0],text[1],text[2],text[3],ambiguous);
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
    if(dvr::objectivemarkers::rune_ownership()) {
        // Hidden, inactive and refused instances withdraw their previous sample.
        dvr::objectivemarkers::publish_rune((uintptr_t)marker,x,y,w,h,valid?flags:0);
    }
    ++g_runeCalls;if(moved)++g_runeMoved;
    ((TaskParentFn)kTaskParentUpdate)(marker,x,y,a,b,distance,flags);
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "hud/rune-parent: calls=%u moved=%u refused=%u want=%d ownerValid=%d guard=%s flags=%x dimensions=%dx%d xy=%.2f/%.2f -> %.2f/%.2f distance=%.2f inset=%.3f; native children retained, draw ownership not yet established",
        g_runeCalls,g_runeMoved,g_runeRefused,(int)want,(int)valid,reason,flags,w,h,oldX,oldY,x,y,distance,dvr::objectivemarkers::rune_inset());
}
// VR-148: the awareness meter, PUBLISHED ONLY. Unlike task and rune this stub
// insets nothing: a meter that belongs over one particular enemy head has no
// business being dragged to the frame edge, and the whole fault being fixed is
// that it is not where its enemy is. All this does is record where the engine
// put it, so the router can leave its draws in the game own image instead of
// lifting them onto the default window quad.
__declspec(noinline) void __fastcall AwarenessParentStub(void* marker,void*,float x,float y,uint32_t a,uint32_t b,float distance,uint32_t flags) {
    int w=0,h=0;
    const bool want=dvr::objectivemarkers::awareness_enabled() && dvr::vr::session_live() &&
        dvr::stereo::wants_projection() && !UiSurfaceRidesHud() && !g_gameExiting;
    bool valid=false;const char* reason="inactive";
    if(want && (uintptr_t)_ReturnAddress()==kAwarenessParentReturn) {
        valid=AwarenessInputs(marker,w,h,reason);
        if(valid) ++g_awareMine; else ++g_awareRefused;
        // Hidden, inactive and refused instances withdraw their previous sample.
        dvr::objectivemarkers::publish_awareness((uintptr_t)marker,x,y,w,h,valid?flags:0);
    }
    ++g_awareCalls;
    ((TaskParentFn)kTaskParentUpdate)(marker,x,y,a,b,distance,flags);
    // VR-152: NEVER PAY FOR A LINE YOU DO NOT PRINT (CLAUDE.md). awareness_report
    // takes the position mutex, and it was being called on EVERY parent update -
    // 16667 of them in one run - purely to build arguments for a line that prints
    // once a second. Worse, that mutex is the one match_awareness_draw takes per
    // HUD draw on the RENDER thread, so this was cross-thread contention paid at
    // game-thread rate for nothing. The gate now comes first and the report only
    // runs on the call that will actually print.
    static double nextCensusMs=0;
    const double censusNow=MaimNowMs();
    if(censusNow<nextCensusMs) return;
    nextCensusMs=censusNow+1000.0;
    float worstW=0,worstH=0,worstDx=0,worstDy=0;unsigned matched=0,ambiguous=0;
    dvr::objectivemarkers::awareness_report(worstW,worstH,worstDx,worstDy,matched,ambiguous);
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "hud/awareness-parent: calls=%u published=%u refused=%u want=%d guard=%s flags=%x "
        "dimensions=%dx%d xy=%.2f/%.2f distance=%.2f | draws matched=%u ambiguous=%u, "
        "widest accepted %.0fx%.0f authoring px at offset %+.0f/%+.0f (the match window is "
        "160x160 px and +/-96 px and is a BOUND, not a measurement - these numbers are what "
        "would tighten it). matched=0 with published>0 means the meter is being placed but "
        "no draw fell in the window",
        g_awareCalls,g_awareMine,g_awareRefused,(int)want,reason,flags,w,h,x,y,distance,
        matched,ambiguous,worstW,worstH,worstDx,worstDy);
}
bool AwarenessParentFingerprint(const uint8_t* bytes) {
    if(memcmp(bytes,kAwarenessParentCallBytes,sizeof(kAwarenessParentCallBytes))) return false;
    int32_t rel=0;memcpy(&rel,bytes+1,4);
    return kAwarenessParentReturn+rel==kTaskParentUpdate;
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
    if(g_taskHook && !dvr::objectivemarkers::task_ownership()) dvr::objectivemarkers::set_task_hooked(true);
    if(dvr::objectivemarkers::rune_enabled()) NativeMarkerInstall(kRuneParentCall,kRuneParentReturn,&RuneParentFingerprint,(void*)&RuneParentStub,g_runeHook,g_runeFailed,"rune");
    if(dvr::objectivemarkers::awareness_enabled()) NativeMarkerInstall(kAwarenessParentCall,kAwarenessParentReturn,&AwarenessParentFingerprint,(void*)&AwarenessParentStub,g_awareHook,g_awareFailed,"awareness");
}
