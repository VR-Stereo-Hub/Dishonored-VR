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
dvr::hooks::Detour hook, cullHook;
using CullFn = uint32_t (__cdecl*)(void*);
CullFn originalCull = nullptr;
size_t familyOffset = 0, reflectionOffset = 0;
struct Invocation {
    void* receiver;
    int reflection;
    uint64_t childCalls = 0;
    double childMs = 0;
};
// Valid only while the original InitViews call is on this thread's stack.
Invocation* activeInvocation = nullptr;
uint32_t ordinal = 0, ordinalClamped = 0, nested = 0, unmatchedCull = 0, classificationChanged = 0;
std::atomic<uint32_t> foreignCull{0};
int reflection_class(void* receiver) {
    __try {
        if (!receiver) return -1;
        auto* family = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(receiver) + familyOffset);
        if (!family) return -1;
        return *reinterpret_cast<uint32_t*>(family + reflectionOffset) != 0 ? 1 : 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return -1; }
}
bool launchArmed = false;
std::atomic<bool> on{false};
std::atomic<DWORD> owner{0};
std::atomic<uint32_t> foreign{0};
uintptr_t imageBase = 0;
struct Row {
    uintptr_t caller = 0;
    int eye = 0, reflection = -1;
    uint32_t order = 0;
    uint64_t calls = 0, childCalls = 0;
    double total = 0, maximum = 0, childMs = 0;
};
Row pending[32], window[96];
uint32_t overflow = 0, intervals[3] = {};
double started = 0;
bool previousOn = false, previousGameplay = false;
Row* find(Row* rows, int count, uintptr_t caller, int eye, int reflection, uint32_t order) {
    for (int i=0;i<count;++i) {
        Row& r=rows[i];
        if (!r.calls || (r.caller==caller && r.eye==eye && r.reflection==reflection && r.order==order)) {
            r.caller=caller; r.eye=eye; r.reflection=reflection; r.order=order; return &r;
        }
    }
    ++overflow; return nullptr;
}
void clear() {
    for(auto& r:pending) r={};
    for(auto& r:window) r={};
    for(auto& n:intervals) n=0;
    overflow=0; foreign.store(0); foreignCull.store(0);
    ordinal=ordinalClamped=nested=unmatchedCull=classificationChanged=0;
}
uint32_t __cdecl cull_hook(void* receiver) {
    if (!on.load(std::memory_order_relaxed)) return originalCull(receiver);
    if (GetCurrentThreadId()!=owner.load(std::memory_order_relaxed)) {
        foreignCull.fetch_add(1,std::memory_order_relaxed);
        return originalCull(receiver);
    }
    const int currentReflection=reflection_class(receiver);
    const double before=dvr::clock::now_ms();
    const uint32_t result=originalCull(receiver);
    const double elapsed=dvr::clock::now_ms()-before;
    if (activeInvocation && activeInvocation->receiver==receiver) {
        if (currentReflection!=activeInvocation->reflection) ++classificationChanged;
        ++activeInvocation->childCalls;
        activeInvocation->childMs+=elapsed;
    } else ++unmatchedCull;
    return result;
}
uint32_t __fastcall init_views_hook(void* self, void*) {
    if (!on.load(std::memory_order_relaxed)) return original(self,nullptr);
    if (GetCurrentThreadId()!=owner.load(std::memory_order_relaxed)) {
        foreign.fetch_add(1,std::memory_order_relaxed);
        return original(self,nullptr);
    }
    const uintptr_t caller=(uintptr_t)_ReturnAddress()-imageBase;
    const int reflection=reflection_class(self);
    const uint32_t order=++ordinal <= 8 ? ordinal : 9;
    if (order==9) ++ordinalClamped;
    Invocation invocation{self,reflection};
    Invocation* previous=activeInvocation;
    if (previous) ++nested;
    activeInvocation=&invocation;
    const double before=dvr::clock::now_ms();
    const uint32_t result=original(self,nullptr);
    const double elapsed=dvr::clock::now_ms()-before;
    activeInvocation=previous;
    if (Row* r=find(pending,32,caller,0,reflection,order)) {
        ++r->calls; r->total+=elapsed;
        r->childCalls+=invocation.childCalls; r->childMs+=invocation.childMs;
        if(elapsed>r->maximum) r->maximum=elapsed;
    }
    return result;
}
}
void configure(bool value) { launchArmed=value; on.store(value); }
bool armed() { return launchArmed; }
bool enabled() { return on.load(); }
void set_enabled(bool value) {
    if(value && (!hook.on || !cullHook.on)) {
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: refused ON; hook not installed (arm Perf.ScenePrepareProfile=1 before launch)");
        return;
    }
    on.store(value);
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,"scene-prepare: enabled=%d",value);
}
void install(uintptr_t target,const uint8_t* prefix,uintptr_t moduleBase,size_t signatureLength) {
    if(!launchArmed || hook.on) return;
    on.store(false);
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
        "scene-prepare: InitViews hook installed; waiting for culling hook; no engine work changed");
}
void install_culling(uintptr_t target,const uint8_t* prefix,size_t signatureLength,
                     size_t familyPointerOffset,size_t reflectionBranchOffset) {
    if(!launchArmed || !hook.on || cullHook.on) return;
    on.store(false);
    // Exactly six bytes: push ebx; mov ebx,esp; sub esp,8. No relocation.
    if(signatureLength<6 || signatureLength>32 || memcmp((void*)target,prefix,signatureLength)!=0) {
        on.store(false);
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Warn,
            "scene-prepare: culling signature mismatch at %p; no patch",(void*)target);
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
    originalCull=(CullFn)code; familyOffset=familyPointerOffset; reflectionOffset=reflectionBranchOffset;
    if(!dvr::hooks::detour_install(cullHook,"scene-culling",target,prefix,6,(void*)&cull_hook)) {
        originalCull=nullptr; VirtualFree(code,0,MEM_RELEASE); on.store(false); return;
    }
    on.store(true);
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
        "scene-prepare: both hooks armed; reflectionBranch=1 engine reflection path, 0 ordinary, -1 unreadable; ordinal is within completed interval; cull time is nested in InitViews");
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
        if(p.calls) if(Row* r=find(window,96,p.caller,label,p.reflection,p.order)) {
            r->calls+=p.calls; r->total+=p.total;
            r->childCalls+=p.childCalls; r->childMs+=p.childMs;
            if(p.maximum>r->maximum) r->maximum=p.maximum;
        }
        p={};
    }
    ordinal=0;
    if(now-started<3000) return;
    const double seconds=(now-started)/1000;
    double total=0; uint64_t calls=0;
    for(const auto& r:window) if(r.calls) {
        total+=r.total; calls+=r.calls;
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
            "scene-prepare: InitViews callerRVA=%08x intervalEye=%d gameplay=%d calls=%llu total=%.3fms max=%.3fms window=%.3fs reflectionBranch=%d ordinal=%u cullCalls=%llu cull=%.3fms (nested cull time; do not add to total)",
            (unsigned)r.caller,r.eye,gameplay,(unsigned long long)r.calls,
            r.total,r.maximum,seconds,r.reflection,r.order,
            (unsigned long long)r.childCalls,r.childMs);
    }
    DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
        "scene-prepare: total calls=%llu wall=%.3fms window=%.3fs L=%u unknown=%u R=%u overflow=%u otherThread=%u cullOtherThread=%u unmatchedCull=%u nestedInit=%u ordinalClamped=%u classificationChanged=%u (not GPU execution; L/R count completed intervals, not InitViews calls)",
        (unsigned long long)calls,total,seconds,intervals[0],intervals[1],intervals[2],overflow,foreign.load(),
        foreignCull.load(),unmatchedCull,nested,ordinalClamped,classificationChanged);
    clear(); started=now;
}
} // namespace dvr::scene_prepare
