#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <string>
#include "core/framework/diagnostic_ab.h"
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/util/log.h"
static double testNow=1;
static std::string text;
namespace dvr::clock { double now_ms(){return testNow;} }
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT]={};
void write(Cat,Level,const char* fmt,...) {char b[2048];va_list a;va_start(a,fmt);vsnprintf(b,sizeof b,fmt,a);va_end(a);text+=b;text+='\n';}
}
#include "../src/core/framework/diagnostic_ab.cpp"
unsigned checks=0;
void check(bool b,const char* why){++checks;if(!b){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
int main(){
    using namespace dvr::diag_ab;
    for(auto& level:dvr::log::g_levels)level=(uint8_t)dvr::log::Level::Info;
    dvr::perf::FreshPair f;
    check(!f.accept(false,1,2),"failed submit");check(!f.accept(true,0,2),"missing eye");
    check(!f.accept(true,1,1),"same content");check(f.accept(true,1,2),"fresh pair");
    check(!f.accept(true,1,2)&&!f.accept(true,3,2),"held or one-eye-only");
    check(f.accept(true,3,4)&&!f.accept(true,1,2),"renewal not rollback");
    f={0xfffffffd,0xfffffffe};check(f.accept(true,1,2),"serial wrap");
    set_enabled(true);tick(false);testNow=50000;tick(false);check(segment==-1&&!reduced(),"menu waits");
    tick(true);testNow+=9999;tick(true);check(segment==-1,"settle enforced");testNow++;tick(true);
    uint32_t serial=10;
    for(int phase=0;phase<3;++phase){
        check(segment==phase&&reduced()==(phase==1),"ABA mask");
        testNow=start+2999;submit(true,serial,serial+1);serial+=2;check(n==0,"warmup excluded");
        testNow=start+3001;submit(true,serial,serial+1);check(n==0,"first sample anchors");
        testNow+=5;submit(false,serial+2,serial+3);testNow+=5;submit(true,serial,serial+1);
        for(int i=0;i<40;++i){testNow+=10;serial+=2;submit(true,serial,serial+1);}
        check(n==40&&rejected==2,"only successful fresh pairs counted");
        testNow+=6000;serial+=2;submit(true,serial,serial+1);serial+=2;
        check(samples[n-1]==6000,"severe stall kept");
        testNow=start+33000;tick(true);
    }
    check(!enabled()&&!reduced(),"completion restores collection");check(text.find("COMPLETE valid=1")!=std::string::npos,"all windows valid");
    set_enabled(true);tick(true);testNow+=10000;tick(true);testNow+=33000;tick(true);
    check(reduced(),"alternative started");tick(false);check(!enabled()&&!reduced(),"menu abort restores");
    set_enabled(true);tick(true);testNow+=10000;tick(true);testNow+=33000;tick(true);
    invalidate();tick(true);check(!enabled()&&!reduced(),"level/device invalidation restores");
    set_enabled(true);tick(true);testNow+=10000;tick(true);testNow+=33000;tick(true);
    set_enabled(true);check(!reduced()&&segment==-1,"restart restores");set_enabled(false);
    set_enabled(true);tick(true);testNow+=10000;tick(true);testNow=start+3001;
    for(unsigned i=0;i<16400;++i){testNow+=.01;submit(true,i*2+1,i*2+2);}
    check(n==16384&&overflow==15,"bounded samples");close();check(!valid[0],"overflow invalidates result");
    set_enabled(false);check(!reduced(),"explicit stop restores");
    printf("diagnostic A/B: %u checks passed\n",checks);
}
