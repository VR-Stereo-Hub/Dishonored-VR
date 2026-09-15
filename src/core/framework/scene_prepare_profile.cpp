// Times the engine InitViews boundary without changing its work or retaining objects.
#include "core/framework/scene_prepare_profile.h"
#include "core/hooks/detour.h"
#include "core/util/clock.h"
#include "core/util/log.h"
#include <windows.h>
#include <intrin.h>
#include <atomic>
#include <string.h>
namespace dvr::scene_prepare {
namespace {
using InitViewsFn = uint32_t (__fastcall*)(void*, void*);
InitViewsFn original = nullptr;
dvr::hooks::Detour hook;
bool launchArmed = false;
std::atomic<bool> on{false};
std::atomic<DWORD> owner{0};
std::atomic<uint32_t> foreign{0};
uintptr_t imageBase = 0;
struct Row {
    uintptr_t caller = 0;
    int eye = 0;
    uint64_t calls = 0;
    double total = 0, maximum = 0;
};
Row pending[32], window[96];
uint32_t overflow = 0, intervals[3] = {};
double started = 0;
bool previousOn = false, previousGameplay = false;
Row* find(Row* rows, int count, uintptr_t caller, int eye) {
    for (int i=0;i<count;++i) {
        Row& r=rows[i];
        if (!r.calls || (r.caller==caller && r.eye==eye)) {
            r.caller=caller; r.eye=eye; return &r;
        }
    }
    ++overflow; return nullptr;
}
void clear() {
    for(auto& r:pending) r={};
    for(auto& r:window) r={};
    for(auto& n:intervals) n=0;
    overflow=0; foreign.store(0);
}
uint32_t __fastcall init_views_hook(void* self, void*) {
    if (!on.load(std::memory_order_relaxed)) return original(self,nullptr);
    if (GetCurrentThreadId()!=owner.load(std::memory_order_relaxed)) {
        foreign.fetch_add(1,std::memory_order_relaxed);
        return original(self,nullptr);
    }
    const uintptr_t caller=(uintptr_t)_ReturnAddress()-imageBase;
    const double before=dvr::clock::now_ms();
    const uint32_t result=original(self,nullptr);
    const double elapsed=dvr::clock::now_ms()-before;
    if (Row* r=find(pending,32,caller,0)) {
        ++r->calls; r->total+=elapsed;
        if(elapsed>r->maximum) r->maximum=elapsed;
    }
    return result;
}
}
void configure(bool value) { launchArmed=value; on.store(value); }
bool armed() { return launchArmed; }
bool enabled() { return on.load(); }
void set_enabled(bool value) {
    if(value && !hook.on) {
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: refused ON; hook not installed (arm Perf.ScenePrepareProfile=1 before launch)");
        return;
    }
    on.store(value);
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,"scene-prepare: enabled=%d",value);
}
void install(uintptr_t target,const uint8_t* prefix,uintptr_t moduleBase,size_t signatureLength) {
    if(!launchArmed || hook.on) return;
    // Exactly six bytes: push ebx; mov ebx,esp; sub esp,8. No relocation.
    if(signatureLength<6 || signatureLength>32 || memcmp((void*)target,prefix,signatureLength)!=0) {
        on.store(false);
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: InitViews signature mismatch at %p; no patch",(void*)target);
        return;
    }
    auto* code=(uint8_t*)VirtualAlloc(nullptr,11,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code) {
        on.store(false);
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: trampoline allocation failed, Win32=%lu",GetLastError());
        return;
    }
    memcpy(code,prefix,6); code[6]=0xE9;
    const int32_t jump=(int32_t)(target+6-(uintptr_t)(code+11));
    memcpy(code+7,&jump,4);
    DWORD old=0;
    if(!VirtualProtect(code,11,PAGE_EXECUTE_READ,&old)) {
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: trampoline protection failed, Win32=%lu",GetLastError());
        VirtualFree(code,0,MEM_RELEASE); on.store(false); return;
    }
    FlushInstructionCache(GetCurrentProcess(),code,11);
    original=(InitViewsFn)code; imageBase=moduleBase;
    if(!dvr::hooks::detour_install(hook,"scene-prepare",target,prefix,6,(void*)&init_views_hook)) {
        original=nullptr; VirtualFree(code,0,MEM_RELEASE); on.store(false); return;
    }
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
        "scene-prepare: armed InitViews; whole boundary, render-thread calls; no engine work or synchronization changed");
}
void end_frame(int eye,bool gameplay) {
    owner.store(GetCurrentThreadId(),std::memory_order_relaxed);
    const bool active=enabled();
    if(!active && !previousOn) return;
    const double now=dvr::clock::now_ms();
    if(active!=previousOn || gameplay!=previousGameplay || !started) {
        clear(); started=now; previousOn=active; previousGameplay=gameplay;
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
            "scene-prepare: window reset enabled=%d gameplay=%d; eye labels completed render intervals; no renderer pointers retained",active,gameplay);
        return;
    }
    const int label=eye==-1 ? -1 : eye==1 ? 1 : 0;
    ++intervals[label+1];
    for(auto& p:pending) {
        if(p.calls) if(Row* r=find(window,96,p.caller,label)) {
            r->calls+=p.calls; r->total+=p.total;
            if(p.maximum>r->maximum) r->maximum=p.maximum;
        }
        p={};
    }
    if(now-started<3000) return;
    const double seconds=(now-started)/1000;
    double total=0; uint64_t calls=0;
    for(const auto& r:window) if(r.calls) {
        total+=r.total; calls+=r.calls;
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
            "scene-prepare: InitViews callerRVA=%08x intervalEye=%d gameplay=%d calls=%llu total=%.3fms max=%.3fms window=%.3fs (inclusive CPU wall time, includes waits)",
            (unsigned)r.caller,r.eye,gameplay,(unsigned long long)r.calls,
            r.total,r.maximum,seconds);
    }
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
        "scene-prepare: total calls=%llu wall=%.3fms window=%.3fs L=%u unknown=%u R=%u overflow=%u otherThread=%u (not GPU execution; L/R count completed intervals, not InitViews calls)",
        (unsigned long long)calls,total,seconds,intervals[0],intervals[1],intervals[2],overflow,foreign.load());
    clear(); started=now;
}
} // namespace dvr::scene_prepare
