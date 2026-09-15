// CPU wall-time samples, not GPU time or additive critical-path savings.
#include "core/framework/native_profile.h"
#include "core/framework/native_profile_stats.h"
#include "core/util/clock.h"
#include "core/util/log.h"
#include <windows.h>
#include <atomic>
namespace dvr::native_profile {
namespace {
std::atomic<bool> on{false};
std::atomic<DWORD> owner{0};
Stats stats[Count];
bool wasOn=false, wasGameplay=false;
double windowStart=0;
const char* names[Count]={"indexed-hook-inclusive","primitive-hook-inclusive",
    "native-indexed-call","native-primitive-call","vs-constant-hook-inclusive",
    "native-vs-constant-call","render-target-hook-inclusive","native-render-target-call",
    "VbLock-hook-inclusive",
    "IbLock-hook-inclusive",
    "TexLockRect-hook-inclusive",
    "TexUnlockRect-hook-inclusive",
    "CubeLockRect-hook-inclusive",
    "CubeUnlockRect-hook-inclusive",
    "VolLockBox-hook-inclusive",
    "VolUnlockBox-hook-inclusive",
    "SurfLockRect-hook-inclusive",
    "SurfUnlockRect-hook-inclusive",
    "SetViewport-hook-inclusive",
    "SetRenderState-hook-inclusive",
    "SetTexture-hook-inclusive",
    "SetVertexDeclaration-hook-inclusive",
    "SetVertexShader-hook-inclusive",
    "SetTransform-hook-inclusive",
    "SetPixelShader-hook-inclusive",
    "SetStreamSource-hook-inclusive",
    "DrawPrimitiveUP-hook-inclusive",
    "DrawIndexedPrimitiveUP-hook-inclusive"};
void clear() { for(auto& s:stats) s.clear(); }
}
void set_enabled(bool value) { on.store(value,std::memory_order_relaxed); }
bool enabled() { return on.load(std::memory_order_relaxed); }
Scope::Scope(Kind k):kind(k) {
    if(!enabled() || GetCurrentThreadId()!=owner.load(std::memory_order_relaxed)) return;
    measured=stats[k].visit();
    if(measured) start=dvr::clock::now_ms();
}
void Scope::finish() {
    if(!measured) return;
    stats[kind].record(dvr::clock::now_ms()-start); measured=false;
}
void tick(bool gameplay) {
    owner.store(GetCurrentThreadId(),std::memory_order_relaxed);
    const bool active=enabled();
    if(!active && !wasOn) return;
    const double now=dvr::clock::now_ms();
    if(active!=wasOn || gameplay!=wasGameplay || !windowStart) {
        clear(); windowStart=now; wasOn=active; wasGameplay=gameplay;
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
            "native-profile: enabled=%d gameplay=%d sampling=1/64; transitions discard partial window; "
            "render-thread inclusive CPU wall time, scopes overlap, no GPU attribution",active,gameplay);
        return;
    }
    if(now-windowStart<3000) return;
    const double seconds=(now-windowStart)/1000;
    for(int k=0;k<Count;++k) {
        const auto& s=stats[k];
        DVR_LOG(dvr::log::Cat::perf,dvr::log::Level::Info,
            "native-profile: scope=%s gameplay=%d window=%.3fs calls=%llu samples=%llu "
            "sampleMean=%.3fus sampleMax=%.3fus estimatedInclusive=%.3fms/s; "
            "random 1/64, max sampled only, nested scopes MUST NOT be added",
            names[k],gameplay,seconds,(unsigned long long)s.calls,(unsigned long long)s.samples,
            s.mean()*1000,s.maxMs*1000,seconds>0?s.mean()*s.calls/seconds:0);
    }
    clear(); windowStart=now;
}
}
