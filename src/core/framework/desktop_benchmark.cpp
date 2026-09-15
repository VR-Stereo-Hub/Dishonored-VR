// Included by the unity build. All calls run on the Present thread.
#include "core/framework/fresh_pair.h"
#include "core/gfx/desktop_eye.h"
namespace dvr::perf {
namespace {
bool desktopArmed=false, desktopPlaying=false, desktopSaved=false;
bool desktopWasOff=false, desktopWasReduced=false;
int desktopSegment=-1;
double desktopWait=0, desktopStart=0, desktopPrevious=0;
FreshPair desktopPair;
float desktopSamples[16384], desktopMedians[3]={}, desktopTails[3]={};
uint32_t desktopN=0, desktopOverflow=0, desktopRejected=0;
void desktop_restore() {
    if (desktopSaved) {
        dvr::desktop_eye::set_mirror_off(desktopWasOff);
        dvr::desktop_eye::set_reduced_present(desktopWasReduced);
    }
    desktopSaved=false;
}
void desktop_close() {
    qsort(desktopSamples,desktopN,sizeof(float),AbCmpFloat);
    double sum=0;
    uint32_t over8=0,over16=0,over33=0;
    for(uint32_t i=0;i<desktopN;++i) {
        const float ms=desktopSamples[i]; sum+=ms;
        over8+=ms>1000.0/120.0; over16+=ms>1000.0/60.0; over33+=ms>1000.0/30.0;
    }
    desktopMedians[desktopSegment]=AbPct(desktopSamples,desktopN,.5f);
    desktopTails[desktopSegment]=AbPct(desktopSamples,desktopN,.99f);
    DVR_INFO("perf/desktop-ab: segment=%d mode=%s n=%u overflow=%u rejected-submits=%u "
             "fresh-pair interval ms p50=%.3f p95=%.3f p99=%.3f p99.9=%.3f max=%.3f "
             "mean=%.3f rate=%.2f/s over8.333=%u over16.667=%u over33.333=%u valid=%d; "
             "successful submissions with BOTH captured serials renewed, not display FPS",
             desktopSegment+1,desktopSegment==1?"off":"full",desktopN,desktopOverflow,desktopRejected,
             desktopMedians[desktopSegment],AbPct(desktopSamples,desktopN,.95f),
             desktopTails[desktopSegment],AbPct(desktopSamples,desktopN,.999f),
             desktopN?desktopSamples[desktopN-1]:0,desktopN?sum/desktopN:0,
             sum>0?1000.0*desktopN/sum:0,over8,over16,over33,desktopN>=32&&!desktopOverflow);
}
} // namespace
bool desktop_ab_enabled() { return desktopArmed; }
void desktop_ab_set_enabled(bool on) {
    if (on) ab_command("off"); // never combine with the historical latency sweep
    desktop_restore(); desktopArmed=on; desktopSegment=-1;
    desktopWait=desktopPrevious=0; desktopPair={};
    DVR_INFO("perf/desktop-ab: %s; Full/Off/Full, 10s settle then three 30s segments, "
             "discard first 3s each; menu/load aborts; original desktop mode restored",
             on?"ARMED, waiting for gameplay":"STOPPED");
}
void desktop_ab_tick(bool gameplay) {
    desktopPlaying=gameplay;
    if(!desktopArmed) return;
    const double now=dvr::clock::now_ms();
    if(!gameplay) {
        desktopWait=0;
        if(desktopSegment>=0) {
            DVR_INFO("perf/desktop-ab: ABORTED by gameplay loss; partial comparison invalid");
            desktop_ab_set_enabled(false);
        }
        return;
    }
    if(desktopSegment<0) {
        if(!desktopWait) desktopWait=now;
        if(now-desktopWait<10000) return;
        desktopWasOff=dvr::desktop_eye::mirror_off();
        desktopWasReduced=dvr::desktop_eye::reduced_present(); desktopSaved=true;
    } else if(now-desktopStart<30000) return;
    else desktop_close();
    if(++desktopSegment==3) {
        DVR_INFO("perf/desktop-ab: COMPLETE. Full baselines p50=%.3f/%.3f p99=%.3f/%.3f; "
                 "Off p50=%.3f p99=%.3f. Compare each metric against baseline spread; "
                 "check validity, desktop fallback counters and scene stability before claiming benefit.",
                 desktopMedians[0],desktopMedians[2],desktopTails[0],desktopTails[2],
                 desktopMedians[1],desktopTails[1]);
        desktop_ab_set_enabled(false); return;
    }
    desktopStart=now; desktopN=desktopOverflow=desktopRejected=0; desktopPrevious=0;
    // Keep accepted identities across boundaries: a held pair is still held.
    dvr::desktop_eye::set_reduced_present(false);
    dvr::desktop_eye::set_mirror_off(desktopSegment==1);
    DVR_INFO("perf/desktop-ab: BEGIN segment=%d mode=%s duration=30s warmup=3s",
             desktopSegment+1,desktopSegment==1?"off":"full");
}
void desktop_ab_submit(bool stereoSubmitted,uint32_t left,uint32_t right) {
    if(!desktopArmed||desktopSegment<0||!desktopPlaying) return;
    const double now=dvr::clock::now_ms();
    const bool fresh=desktopPair.accept(stereoSubmitted,left,right);
    if(now-desktopStart<3000) { desktopPrevious=0; return; }
    if(!fresh) { ++desktopRejected; return; }
    if(desktopPrevious>0) {
        const float interval=(float)(now-desktopPrevious);
        // No upper cutoff: severe stalls belong in the distribution too.
        if(desktopN<16384) desktopSamples[desktopN++]=interval;
        else ++desktopOverflow;
    }
    desktopPrevious=now;
}
} // namespace dvr::perf
