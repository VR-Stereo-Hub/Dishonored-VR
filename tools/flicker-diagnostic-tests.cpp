// Compile the real recorder with deterministic capture/runtime/log dependencies.
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include "core/util/log.h"
#include "core/gfx/capture.h"
#include "core/gfx/frame_id.h"
#include "core/vr/openxr_runtime.h"
static uint64_t clockMs=0;
static uint64_t TestNowMs() {return clockMs;}
static std::vector<std::string> lines;
static dvr::vr::PairProbe probe;
static uint32_t readTimeout=0,writeTimeout=0,bursts=0;
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT]={};
void write(Cat,Level,const char* fmt,...) {
    char text[8192];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);
    lines.emplace_back(text);
}
}
namespace dvr::capture {
uint32_t read_timeouts(){return readTimeout;}
uint32_t fence_timeouts(){return writeTimeout;}
const char* mode_name(){return "shared";}
bool shared_wait(){return false;}
}
namespace dvr::vr {void pair_probe_peek(PairProbe* out){*out=probe;}}
namespace dvr::frameid {void diagnostic_burst(){++bursts;}}
#undef DVR_CAT
#define GetTickCount64 TestNowMs
#include "core/gfx/flicker_diagnostic.cpp"
#undef GetTickCount64
static int failures=0,checks=0;
void check(bool x,const char* why){++checks;if(!x){++failures;printf("FAIL: %s\n",why);}}
bool contains(const char* s){for(auto& l:lines)if(l.find(s)!=std::string::npos)return true;return false;}
int main(){
    for(auto& l:dvr::log::g_levels)l=(uint8_t)dvr::log::Level::Info;
    using namespace dvr::flicker;
    Method m;m.present=7;m.fresh=true;m.delivered=-1;m.draw=12;m.rec=42;m.deliveredSerial=99;m.deliveredRec=42;
    m.poseOk=true;m.poseEye=-1;m.gen=8;m.c5ok=true;m.c5[0]=3100;
    float c5[3]={3100,-7400,1100};camera_upload(c5,0,6);camera_upload(c5,0,6);
    method(m);Runtime r;r.present=7;r.outcome=2;r.eye=-1;r.target=0;r.serial[0]=99;
    finish(r);
    check(contains("P7")&&contains("delivery99/-1")&&contains("rec42"),"method/capture identity survives real formatter");
    check(contains("outcome2")&&contains("content99/0"),"held-open XR tail is separate from submitted stereo");
    check(contains("uploads2 distinctStored1")&&contains("2*(3100.000,-7400.000,1100.000)@c0/6"),"camera census reaches collected log");
    check(bursts==1,"history window requests correlated pixel burst");
    for(uint32_t i=8;i<40;++i){
        clockMs+=10;m.present=i;m.delivered=1;m.expire=2;m.owed=-1;m.action=16;method(m);
        r.present=i;r.outcome=3;r.eye=1;r.target=1;r.layers=1;r.newLayer=true;r.stereo=true;
        r.acquired=r.waited=r.released=true;r.acq=r.wait=r.release=0;r.copied=true;
        r.serial[1]=100+i;++probe.stalePresL;++probe.stereoSubmits;finish(r);
    }
    check(totals[0]==32 && totals[1]==32 && totals[2]==31 && totals[6]==32,
          "suppressed fault frames still contribute exact populations");
    check(bursts==1,"fault spam cannot remove recurring window rate bound");
    clockMs=6000;r.present=41;r.acq=-1;r.wait=-2;r.release=-3;r.end=-4;
    ++probe.eatenNoFrame;++readTimeout;finish(r); // deliberately no method record
    check(contains("P41 at0.000")&&contains("methodP0"),"missing method does not reuse previous capture provenance");
    check(contains("results-1/-2/-3 end-4"),"actual API failures reach the log");
    check(totals[4]==1 && totals[5]==1,"API/fence and no-frame failures separately counted");
    clockMs+=10;r.present=42;r.acquired=r.waited=r.released=false;r.end=0;r.layers=0;r.newLayer=false;
    finish(r);check(totals[7]==1,"zero-layer or saved-layer fallback has its own population");
    check(bursts==2,"late failure window opens after startup windows without lifetime cap");
    for(auto& l:lines)check(l.size()<1024,"record fits production logger's 1024-byte buffer");
    printf("flicker diagnostic recorder: %d checks, %d failures, %zu log records\n",checks,failures,lines.size());
    return failures?1:0;
}
