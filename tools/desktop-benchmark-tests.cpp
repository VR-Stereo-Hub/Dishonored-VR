// Exercise the production benchmark, including restoration and rejected populations.
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string>
#include "../src/core/framework/fresh_pair.h"
static double testNow=1;
static std::string testLog;
#define DVR_INFO(...) do { char b[2048]; snprintf(b,sizeof(b),__VA_ARGS__); testLog+=b; testLog+='\n'; } while(0)
namespace dvr::clock { double now_ms() { return testNow; } }
namespace dvr::desktop_eye {
bool off=false,reduced=true;
void set_mirror_off(bool v){off=v;} bool mirror_off(){return off;}
void set_reduced_present(bool v){reduced=v;} bool reduced_present(){return reduced;}
}
namespace dvr::perf {
void desktop_ab_set_enabled(bool); bool ab_command(const char*) {return true;}
static int AbCmpFloat(const void* a,const void* b) {
    return *(const float*)a<*(const float*)b?-1:*(const float*)a>*(const float*)b?1:0;
}
static float AbPct(const float* s,uint32_t n,float p) { return n?s[(uint32_t)(p*(n-1)+.5f)]:0; }
}
#include "../src/core/framework/desktop_benchmark.cpp"
int main() {
    using namespace dvr::perf;
    FreshPair f;
    assert(!f.accept(false,1,2)); assert(!f.accept(true,0,2));
    assert(!f.accept(true,1,1)); assert(f.accept(true,1,2));
    assert(!f.accept(true,1,2)); assert(!f.accept(true,3,2));
    assert(!f.accept(false,3,4)); assert(f.accept(true,3,4));
    assert(!f.accept(true,1,2));
    f={0xfffffffd,0xfffffffe}; assert(f.accept(true,1,2));
    desktop_ab_set_enabled(true);
    desktop_ab_tick(false); testNow=50000; desktop_ab_tick(false);
    assert(desktopSegment==-1 && dvr::desktop_eye::reduced);
    desktop_ab_tick(true); testNow+=9999; desktop_ab_tick(true); assert(desktopSegment==-1);
    testNow+=1; desktop_ab_tick(true); assert(desktopSegment==0 && !dvr::desktop_eye::reduced);
    uint32_t serial=10;
    for(int stage=0;stage<3;++stage) {
        assert(dvr::desktop_eye::off==(stage==1));
        testNow=desktopStart+2999; desktop_ab_submit(true,serial,serial+1); serial+=2; assert(!desktopN);
        testNow=desktopStart+3001; desktop_ab_submit(true,serial,serial+1); assert(!desktopN);
        testNow+=5; desktop_ab_submit(false,102,103); assert(!desktopN);
        testNow+=5; desktop_ab_submit(true,serial,serial+1); assert(!desktopN);
        for(int i=1;i<=40;++i) { testNow+=10; serial+=2; desktop_ab_submit(true,serial,serial+1); }
        assert(desktopN==40 && desktopRejected==2);
        testNow+=6000; serial+=2; desktop_ab_submit(true,serial,serial+1); serial+=2;
        assert(desktopSamples[desktopN-1]==6000); // severe is not discarded
        testNow=desktopStart+30000; desktop_ab_tick(true);
    }
    assert(!desktopArmed && !dvr::desktop_eye::off && dvr::desktop_eye::reduced);
    assert(testLog.find("COMPLETE")!=std::string::npos);
    // Original Off + Reduced is restored on menu loss in the middle segment.
    dvr::desktop_eye::off=true; desktop_ab_set_enabled(true); desktop_ab_tick(true);
    testNow+=10000; desktop_ab_tick(true); testNow+=30000; desktop_ab_tick(true);
    desktop_ab_tick(false); assert(!desktopArmed && dvr::desktop_eye::off && dvr::desktop_eye::reduced);
    // Explicit stop / restart is also a handback, even with no samples.
    desktop_ab_set_enabled(true); desktop_ab_tick(true); testNow+=10000; desktop_ab_tick(true);
    desktop_ab_set_enabled(true); assert(dvr::desktop_eye::off && dvr::desktop_eye::reduced);
    desktop_ab_set_enabled(false);
    // Bounded storage: overflow marks the segment invalid, never writes past its buffer.
    desktop_ab_set_enabled(true); desktop_ab_tick(true); testNow+=10000; desktop_ab_tick(true);
    testNow+=3001;
    for(uint32_t i=0;i<16400;++i) { testNow+=.01; desktop_ab_submit(true,i*2+1,i*2+2); }
    assert(desktopN==16384 && desktopOverflow==15);
    desktop_ab_set_enabled(false);
    puts("PASS: fresh identities, warmup, full/off/full, severe stalls, menu abort and restoration");
}
