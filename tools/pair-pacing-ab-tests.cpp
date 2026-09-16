// Tests the production automatic benchmark without a game or XR runtime.
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "core/gfx/desktop_eye.h"
static double testNow=1000;
static bool testOff=true,testReduced=false,testSync=false;
static unsigned testHz=0,testDelays=0;
static int64_t testPeriod=8333333;
static std::vector<std::string> messages;
static void record(const char* format,...) {
    char text[2048]; va_list args; va_start(args,format);
    vsnprintf(text,sizeof(text),format,args); va_end(args); messages.emplace_back(text);
}
#define DVR_INFO(...) record(__VA_ARGS__)
namespace dvr::clock { double now_ms() {return testNow;} }
namespace dvr::desktop_eye {
void set_mirror_off(bool v) {testOff=v;} bool mirror_off() {return testOff;}
void set_reduced_present(bool v) {testReduced=v;} bool reduced_present() {return testReduced;}
}
namespace dvr::vr {
void set_pace_sync(bool v) {testSync=v;} bool pace_sync() {return testSync;}
void set_pace_sync_hz(unsigned v) {testHz=v;} unsigned pace_sync_hz() {return testHz;}
uint32_t pace_sync_delays() {return testDelays;}
int64_t display_period_ns() {return testPeriod;}
}
namespace dvr::perf {
void desktop_ab_set_enabled(bool);
bool ab_command(const char*) {return true;}
static int AbCmpFloat(const void* a,const void* b) {
    const float x=*(const float*)a,y=*(const float*)b;return (x>y)-(x<y);
}
static float AbPct(const float* p,unsigned n,float q) {return n?p[(unsigned)((n-1)*q)]:0;}
}
#include "../src/core/framework/desktop_benchmark.cpp"
using namespace dvr::perf;
static unsigned checks=0,failed=0,serial=0;
static void check(bool ok,const char* what) {
    ++checks;if(!ok){++failed;printf("FAIL %s\n",what);}
}
static void reset(bool pacing=true) {
    desktop_ab_set_enabled(false);testNow+=200000;testOff=true;testReduced=false;
    testSync=false;testHz=0;testDelays=0;testPeriod=8333333;
    desktop_ab_set_reduced(false);desktop_ab_set_pacing(pacing);messages.clear();
}
static void start() {
    desktop_ab_set_enabled(true);desktop_ab_tick(true);
    testNow+=desktop_ab_pacing()?30000:10000;desktop_ab_tick(true);
}
static void sample(double interval=10) {
    testNow+=interval;serial+=2;desktop_ab_submit(true,serial-1,serial);
}
static void fill(double interval=10) {
    testNow=desktopStart+3000;
    for(unsigned i=0;i<100;++i) sample(interval);
}
static void next() {testNow=desktopStart+30000;desktop_ab_tick(true);}
static bool logged(const char* needle) {
    for(const auto& s:messages)if(s.find(needle)!=std::string::npos)return true;return false;
}
int main() {
    reset();check(!desktop_ab_enabled(),"default diagnostic inactive");
    testOff=false;testReduced=true;testSync=true;testHz=75;
    start();check(desktopSegment==0 && testOff && !testReduced && !testSync,"baseline forces off/unpaced");
    testNow=desktopStart+100;sample();check(desktopN==0,"warmup excluded");
    fill();check(desktopN==99,"fresh pair intervals counted");
    auto n=desktopN;desktop_ab_submit(true,serial-1,serial);check(desktopN==n,"held pair rejected");
    next();check(testSync && testHz==90 && testOff,"100 pair/s baseline derives 90 Hz");
    fill(1000.0/90.0);testDelays=77;next();
    check(!testSync && testOff && desktopSegment==2,"return baseline remains mirror off");
    check(logged("delay-events=77"),"actual gate delay count logged");
    fill();next();check(!desktop_ab_enabled() && !testOff && testReduced && testSync && testHz==75,"complete restores original modes and target");
    reset();start();fill();next();desktop_ab_tick(false);
    check(!desktop_ab_enabled() && testOff && !testSync && testHz==0,"menu abort restores pacing");
    reset();start();fill();next();testOff=false;desktop_ab_tick(true);
    check(!desktop_ab_enabled() && logged("external mirror/pacing change"),"external mirror change invalidates trial");
    reset();start();fill();next();testHz=80;desktop_ab_tick(true);
    check(!desktop_ab_enabled(),"external target change aborts");
    reset();start();next();check(!desktop_ab_enabled() && logged("REFUSED target"),"no baseline samples refuses cap");
    reset();start();fill(200);next();check(!desktop_ab_enabled(),"too slow baseline refuses unsupported target");
    reset();start();fill();desktopOverflow=1;next();check(!desktop_ab_enabled(),"overflow refuses target");
    reset();start();fill(5);next();check(testHz==120,"target cannot exceed measured headset refresh");
    reset();start();fill();sample(5000);check(desktopSamples[desktopN-1]==5000,"long stalls are retained");
    reset();start();fill();next();fill();next();check(logged("delay-events=0"),"ineffective pacing observable");
    reset(false);start();check(!testOff && !testReduced,"old Full baseline preserved");
    fill();next();check(testOff && !testSync,"old Off alternative unchanged");
    fill();next();check(!testOff,"old Full return unchanged");
    reset(false);desktop_ab_set_reduced(true);start();fill();next();
    check(!testOff && testReduced,"old Reduced alternative unchanged");
    reset();desktop_ab_set_enabled(true);desktop_ab_tick(false);testNow+=60000;
    desktop_ab_tick(true);check(desktopSegment==-1,"wait for gameplay before settling");
    testNow+=29999;desktop_ab_tick(true);check(desktopSegment==-1,"full 30-second positioning grace");
    testNow+=1;desktop_ab_tick(true);check(desktopSegment==0,"start after grace");
    desktop_ab_set_enabled(false);check(testOff&&!testSync,"manual stop restores modes");
    printf("pair pacing benchmark: %u checks, %u failures\n",checks,failed);
    return failed?1:0;
}
