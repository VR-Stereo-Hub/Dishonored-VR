#define DVR_CAT ::dvr::log::Cat::perf
#include "core/framework/diagnostic_ab.h"
#include "core/framework/fresh_pair.h"
#include "core/util/log.h"
#include "core/util/clock.h"
#include <atomic>
#include <algorithm>
namespace dvr::diag_ab {
namespace {
std::atomic<bool> suppress{false}, invalid{false};
bool armed=false,playing=false;
int segment=-1;
double waitStart=0,start=0,previous=0;
dvr::perf::FreshPair pair;
float samples[16384];uint32_t n=0,overflow=0,rejected=0;
float medians[3]={},tails[3]={}; bool valid[3]={};
float percentile(float p) { return n?samples[(uint32_t)(p*(n-1)+.5f)]:0; }
void close() {
    std::sort(samples,samples+n);double sum=0;unsigned over16=0,over33=0;
    for(unsigned i=0;i<n;++i) {sum+=samples[i];over16+=samples[i]>1000./60;over33+=samples[i]>1000./30;}
    medians[segment]=percentile(.5f);tails[segment]=percentile(.99f);valid[segment]=n>=32&&!overflow;
    DVR_INFO("perf/diag-ab: segment=%d reduced=%d n=%u rejected=%u overflow=%u valid=%d fresh-pair ms mean=%.3f p50=%.3f p95=%.3f p99=%.3f max=%.3f rate=%.2f/s over16.667=%u over33.333=%u; BOTH captured serials renewed, not eye Presents or display FPS",
        segment+1,segment==1,n,rejected,overflow,valid[segment],n?sum/n:0,medians[segment],percentile(.95f),tails[segment],n?samples[n-1]:0,sum>0?1000*n/sum:0,over16,over33);
}
}
bool reduced() { return suppress.load(std::memory_order_relaxed); }
bool enabled() { return armed; } // configuration, UI and tick run on present lane
void invalidate() { invalid.store(true); }
void set_enabled(bool value) {
    suppress.store(false);armed=value;segment=-1;waitStart=previous=0;pair={};invalid.store(false);
    for(auto& v:valid)v=false;
    DVR_INFO("perf/diag-ab: %s; baseline/reduced/baseline, 10s settle then 3x33s (first3s excluded); masks only ZAccount/PairTrace/FrameId/AttachCensus; original INI flags never changed",
        value?"ARMED waiting for gameplay":"STOPPED original collection restored");
}
void tick(bool gameplay) {
    playing=gameplay;if(!armed)return;
    const double now=dvr::clock::now_ms();
    if(invalid.exchange(false) && segment>=0) {
        DVR_INFO("perf/diag-ab: ABORTED device/level boundary; comparison invalid");set_enabled(false);return;
    }
    if(!gameplay) {
        waitStart=0;
        if(segment>=0) {DVR_INFO("perf/diag-ab: ABORTED gameplay loss; comparison invalid");set_enabled(false);}
        return;
    }
    if(segment<0) {if(!waitStart)waitStart=now;if(now-waitStart<10000)return;}
    else if(now-start<33000)return;
    else close();
    if(++segment==3) {
        DVR_INFO("perf/diag-ab: COMPLETE valid=%d; baseline p50=%.3f/%.3f p99=%.3f/%.3f; reduced p50=%.3f p99=%.3f. Compare with BOTH baselines and verify fixed scene before claiming benefit",
            valid[0]&&valid[1]&&valid[2],medians[0],medians[2],tails[0],tails[2],medians[1],tails[1]);
        set_enabled(false);return;
    }
    start=now;n=overflow=rejected=0;previous=0;
    suppress.store(segment==1);
    DVR_INFO("perf/diag-ab: BEGIN segment=%d reduced=%d duration33s warmup3s",segment+1,segment==1);
}
void submit(bool stereo,uint32_t left,uint32_t right) {
    if(!armed||segment<0||!playing||invalid.load())return;
    double now=dvr::clock::now_ms();bool fresh=pair.accept(stereo,left,right);
    if(now-start<3000) {previous=0;return;}
    if(!fresh) {++rejected;return;}
    if(previous>0) {if(n<16384)samples[n++]=(float)(now-previous);else ++overflow;}
    previous=now;
}
}
