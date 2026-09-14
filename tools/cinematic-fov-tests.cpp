#include "../src/game/dishonored/cinematic_fov_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
unsigned checks=0;
void check(bool v,const char* msg) { ++checks; if (!v) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
int main() {
    using namespace dvr::cine_fov;
    check(eligible(true,true,false,true,true,108),"cinematic accepts VR target");
    check(!eligible(false,true,false,true,true,108),"disabled preserves authored FOV");
    check(!eligible(true,false,false,true,true,108),"no scene refuses");
    check(!eligible(true,true,true,true,true,108),"menu refuses");
    check(!eligible(true,true,false,false,true,108),"quad refuses");
    check(!eligible(true,true,false,true,false,108),"unknown state refuses");
    check(!eligible(true,true,false,true,true,0),"missing target refuses");
    check(!eligible(true,true,false,true,true,200),"invalid target refuses");
    check(!eligible(true,true,false,true,true,std::numeric_limits<float>::quiet_NaN()),"NaN refuses");
    Scope s; float f=65;
    check(s.begin(&f,108,true) && f==108,"zoom is replaced for the draw");
    check(!s.begin(&f,110,true),"nested scope refuses");
    check(s.end(true) && f==65,"authored FOV restored");
    check(!s.end(true),"second restore refuses");
    check(!s.begin(&f,108,false) && f==65,"identity required before write");
    check(s.begin(&f,108,true),"new scene can enter");
    f=90; check(!s.end(true) && f==90,"engine replacement is not overwritten");
    check(s.begin(&f,108,true),"another scope can enter");
    check(!s.end(false) && f==108,"stale identity never restored");
    f=0; check(!s.begin(&f,108,true),"invalid existing camera FOV refuses");
    check(!s.begin(nullptr,108,true),"missing field refuses");
    std::printf("%u cinematic FOV policy/scope checks passed\n",checks);
}
