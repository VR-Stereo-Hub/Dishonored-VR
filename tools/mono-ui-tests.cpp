#include "core/vr/mono_anchor.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
int main() {
    int count=0;
    auto check=[&](bool ok){++count;if(!ok){printf("FAIL %d\n",count);std::exit(1);}};
    auto near=[](float a,float b){return std::fabs(a-b)<0.0001f;};
    dvr::mono::Anchor a;
    check(!a.valid);
    check(a.seed({1,2,3,0,0,0,1},2));
    check(near(a.pose.x,1)&&near(a.pose.y,2)&&near(a.pose.z,1));
    a.reset();check(!a.valid);
    check(a.seed({1,2,3,0,0.70710678f,0,0.70710678f},2));
    check(near(a.pose.x,-1)&&near(a.pose.y,2)&&near(a.pose.z,3));
    check(near(a.pose.qx,0)&&near(a.pose.qz,0));
    a.reset();check(!a.seed({0,0,0,0,0,0,0},2));
    check(!a.seed({0,0,0,0,0,0,1},-2));
    check(!a.seed({std::numeric_limits<float>::quiet_NaN(),0,0,0,0,0,1},2));
    check(!a.seed({0,0,0,0.70710678f,0,0,0.70710678f},2));
    dvr::mono::LoadingLease lease;
    check(!lease.update(true,false,false,true)); // autosave notification
    check(lease.update(true,true,false,true));
    check(lease.update(true,false,false,true)); // Continue still onscreen
    check(lease.update(false,false,false,false)); // unreadable cannot release
    check(!lease.update(true,false,false,false));
    check(lease.update(true,false,true,true));
    check(lease.update(true,false,false,true));
    check(!lease.update(true,false,false,false));
    printf("PASS %d anchor/loading checks\n",count);
}
