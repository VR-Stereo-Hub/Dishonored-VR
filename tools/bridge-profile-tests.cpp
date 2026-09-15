#include "core/framework/bridge_profile_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
using namespace dvr::bridge_profile;
unsigned n=0;
void check(bool v,const char* why) { ++n;if(!v) { std::fprintf(stderr,"FAIL %s\n",why);std::exit(1); } }
int main() {
    double ms=-1;
    check(interval_ms(100,600,100000,false,ms) && std::fabs(ms-5)<1e-9,"frequency conversion");
    check(!interval_ms(1,2,0,false,ms),"zero frequency rejected");
    check(!interval_ms(2,1,100,false,ms),"inverted clock rejected");
    check(!interval_ms(1,2,100,true,ms),"disjoint rejected");
    check(interval_ms(42,42,100,false,ms) && ms==0,"zero duration valid");
    SlotState ring[16];
    for(auto& q:ring) q.issue(100,7);
    int free=0;for(auto& q:ring) free+=q.available();check(free==0,"full ring cannot be reused");
    check(!ring[0].pollable(107) && ring[0].pollable(108),"minimum result age");
    check(!ring[0].pollable(99),"backward serial not aged");
    check(ring[0].current(7) && !ring[0].current(8),"menu/toggle invalidates pending result");
    check(!ring[0].available(),"late result keeps slot occupied");
    ring[0].retire();check(ring[0].available(),"resolved/error result can retire");
    ring[0].issue(200,8);check(ring[0].current(8)&&!ring[0].pollable(207),"reuse gets fresh identity");
    for(auto& q:ring) q=SlotState{};
    free=0;for(auto& q:ring) free+=q.available();check(free==16,"device reset empties ring");
    SampleGate g;unsigned counts[2][2]={};bool unique=true;
    for(int i=0;i<100000;++i) { g.present();bool a=g.take(0),b=g.take(1);
        unique &= !(a&&b) && !g.take(0) && !g.take(1);
        if(a) ++counts[0][i&1];if(b) ++counts[1][i&1]; }
    check(unique,"at most one stage bracket per present");
    for(auto& stage:counts)for(auto count:stage)check(count>2800&&count<3450,"both stages and eyes sampled without stride alias");
    std::printf("bridge profile: %u checks passed\n",n);
}
