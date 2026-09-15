#define DVR_CAT ::dvr::log::Cat::perf
#include "core/framework/bridge_profile.h"
#include "core/framework/bridge_profile_policy.h"
#include "core/util/log.h"
#include <windows.h>
#include <d3d11.h>
#include <atomic>
#include <algorithm>

namespace dvr::bridge_profile {
namespace {
constexpr int Slots=16, Samples=512;
struct Slot {
    ID3D11Query *disjoint=nullptr,*a=nullptr,*b=nullptr;
    SlotState state; int eye=0;
};
struct Stats {
    unsigned calls=0,issued=0,resolved=0,latePolls=0,full=0,invalid=0,errors=0,discarded=0;
    unsigned n=0,overflow=0; double sum=0,max=0,values[Samples]={};
};
struct Bank { Slot slot[Slots]; Stats stats[3]; uint64_t calls=0; bool failed=false; };
Bank banks[StageCount];
SampleGate gate;
ID3D11Device* device=nullptr; // owned reference prevents address reuse
std::atomic<bool> on{false},play{false};
std::atomic<unsigned> requestedEpoch{1};
unsigned epoch=0; DWORD thread=0; ULONGLONG logAt=0;
const char* names[]={"conversion","eye-copy"};
void release(ID3D11Query*& q) { if(q) { q->Release(); q=nullptr; } }
void clear_stats() { for(auto& b:banks) for(auto& s:b.stats) s=Stats{}; }
void report() {
    for(int k=0;k<StageCount;++k) for(int e=0;e<3;++e) {
        auto& s=banks[k].stats[e]; if(!s.calls && !s.resolved && !s.errors && !s.discarded) continue;
        double sorted[Samples]; std::copy(s.values,s.values+s.n,sorted); std::sort(sorted,sorted+s.n);
        const double p50=s.n?sorted[(s.n-1)*50/100]:0, p95=s.n?sorted[(s.n-1)*95/100]:0;
        unsigned pending=0; for(auto& q:banks[k].slot) if(q.state.pending && q.eye==e) ++pending;
        DVR_INFO("perf/bridge: %s eye=%d context=%s calls=%u issued=%u resolved=%u pending=%u latePolls=%u full=%u invalid=%u errors=%u discarded=%u GPUms mean=%.4f p50=%.4f p95=%.4f max=%.4f stored=%u overflow=%u; stage interval, not pair latency or additive GPU busy time",
            names[k],e-1,play.load()?"gameplay":"menu",s.calls,s.issued,s.resolved,pending,s.latePolls,s.full,s.invalid,s.errors,s.discarded,
            s.resolved?s.sum/s.resolved:0,p50,p95,s.max,s.n,s.overflow);
    }
    clear_stats();
}
bool make(ID3D11Device* d, Slot& s) {
    D3D11_QUERY_DESC desc={D3D11_QUERY_TIMESTAMP_DISJOINT,0};
    HRESULT hr=d->CreateQuery(&desc,&s.disjoint);
    desc.Query=D3D11_QUERY_TIMESTAMP;
    if(SUCCEEDED(hr)) hr=d->CreateQuery(&desc,&s.a);
    if(SUCCEEDED(hr)) hr=d->CreateQuery(&desc,&s.b);
    if(FAILED(hr)) { release(s.disjoint); release(s.a); release(s.b);
        DVR_WARN("perf/bridge: CreateQuery failed 0x%08lx; this stage disabled until device reset",(unsigned long)hr); return false; }
    return true;
}
void poll(ID3D11DeviceContext* ctx,Bank& b) {
    for(auto& q:b.slot) {
        if(!q.state.pollable(b.calls)) continue;
        auto& s=b.stats[q.eye];
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj={}; UINT64 a=0,z=0;
        HRESULT h=ctx->GetData(q.disjoint,&dj,sizeof(dj),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(h==S_OK) h=ctx->GetData(q.a,&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(h==S_OK) h=ctx->GetData(q.b,&z,sizeof(z),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(h==S_FALSE) { ++s.latePolls; continue; }
        if(FAILED(h)) { ++s.errors; }
        else if(!q.state.current(epoch)) { ++s.discarded; }
        else {
            double ms=0;
            if(!interval_ms(a,z,dj.Frequency,dj.Disjoint!=FALSE,ms)) ++s.invalid;
            else { ++s.resolved; s.sum+=ms; s.max=(std::max)(s.max,ms);
                if(s.n<Samples) s.values[s.n++]=ms; else ++s.overflow; }
        }
        q.state.retire();
    }
}
}
void set_enabled(bool value) {
    if(on.exchange(value)!=value) { ++requestedEpoch; DVR_INFO("perf/bridge: %s; randomized 1/16 stage opportunities, at most one bracket per Present, no query flush/wait",value?"ON":"off"); }
}
bool enabled() { return on.load(); }
void present() { if(enabled()) gate.present(); }
void set_gameplay(bool value) { if(play.exchange(value)!=value) ++requestedEpoch; }
void reset() {
    for(auto& b:banks) { for(auto& q:b.slot) { release(q.disjoint);release(q.a);release(q.b); } b=Bank{}; }
    if(device) { device->Release();device=nullptr; }
    thread=0;epoch=0;logAt=0;
}
int begin(ID3D11Device* d,ID3D11DeviceContext* ctx,Stage stage,int eye) {
    if(!enabled() || !d || !ctx) return -1;
    if(thread && thread!=GetCurrentThreadId()) return -1;
    if(device!=d) { reset(); device=d;device->AddRef();thread=GetCurrentThreadId();
        DVR_INFO("perf/bridge: device epoch started; query results exclude D3D9 scene, XR wait/compositor and encoding"); }
    const auto now=GetTickCount64(); const unsigned wanted=requestedEpoch.load();
    if(epoch!=wanted) { clear_stats();epoch=wanted;logAt=now+3000;
        DVR_INFO("perf/bridge: context boundary -> %s; partial statistics discarded",play.load()?"gameplay":"menu"); }
    auto& b=banks[stage]; ++b.calls; poll(ctx,b);
    if(now>=logAt) { report();logAt=now+3000; }
    auto& stats=b.stats[eye<0?0:eye>0?2:1]; ++stats.calls;
    if(b.failed) return -1;
    if(!gate.take(stage)) return -1;
    for(int i=0;i<Slots;++i) {
        auto& q=b.slot[i]; if(!q.state.available()) continue;
        if(!q.a && !make(d,q)) { b.failed=true; ++stats.errors;return -1; }
        q.eye=eye<0?0:eye>0?2:1; q.state.issue(b.calls,epoch); ++stats.issued;
        ctx->Begin(q.disjoint);ctx->End(q.a);return i;
    }
    ++stats.full;return -1;
}
void end(ID3D11DeviceContext* ctx,Stage stage,int token) {
    auto& q=banks[stage].slot[token];ctx->End(q.b);ctx->End(q.disjoint);
}
}
