#include "../src/game/dishonored/fov_lever_policy.h"
#include "../src/game/dishonored/cinematic_fov_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
unsigned checks=0;
void check(bool v,const char* msg) { ++checks; if(!v) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
bool near(float a,float b) { return std::fabs(a-b)<0.002f; }
float old_target(float sensor,float natural,float target) {
    float t=(sensor>natural?natural:sensor)*(target/natural);
    return t<20?20:(t>160?160:t);
}
int main() {
    using dvr::fov_lever::target;
    // Negative control: delayed engine readback reproduces the released bug.
    float old=110;
    for(int i=0;i<20000;++i) old += 0.1f*(old_target(old,110,108.0666f)-old);
    check(near(old,20),"released recurrence collapses to 20 degrees");
    for(float requested:{40.0f,90.0f,103.0f,108.0666f,110.0f,111.0f,137.0f,160.0f}) {
        for(float blend:{0.01f,0.1f,0.5f,1.0f}) {
            float sensor=110;
            for(int i=0;i<20000;++i) {
                const float written=target(sensor,110,requested);
                check(written>=requested-0.001f,"normal readback never contracts below requested FOV");
                // Multiple dispatches before the engine updates its sensor.
                check(written==target(sensor,110,requested),"delayed readback does not compound dispatches");
                sensor += blend*(written-sensor);
            }
            check(near(sensor,requested),"interpolated readback converges to requested FOV");
        }
    }
    check(near(target(32,110,108.0666f),32),"native narrow zoom passes through without contraction");
    check(near(target(32,75,108),old_target(32,75,108)),"existing expansion zoom scaling is unchanged");
    // A game-authored gradual zoom below the ceiling must not be flattened.
    for(float sensor=108; sensor>=32; sensor-=0.25f)
        check(near(target(sensor,110,108.0666f),sensor),"smooth native zoom retained");
    // Native unzoom supplies new readback, which must reach the new ceiling.
    for(float sensor=32; sensor<=110; sensor+=0.25f)
        check(near(target(sensor,110,108.0666f),sensor<108.0666f?sensor:108.0666f),"native zoom recovery retained");
    check(near(target(111,110,108),108),"target decrease caps once");
    check(near(target(108,110,111),108*(111.0f/110)),"target increase keeps old widening behavior");
    check(near(target(108,108,108),108),"recaptured own output is stable");
    check(near(target(110,110,108.0666f),108.0666f),"fresh level baseline is stable");
    for(float invalid:{0.0f,175.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        check(target(invalid,110,108)==0,"invalid sensor refuses");
    check(target(110,0,108)==0 && target(110,175,108)==0,"invalid baseline refuses");
    check(target(110,110,0)==0 && target(110,110,175)==0,"invalid target refuses");
    // Draw-scoped FOV continues to restore the stable persistent FOV each draw.
    float sensor=110;
    for(int i=0;i<2000;++i) {
        sensor += 0.1f*(target(sensor,110,108.0666f)-sensor);
        float cache=sensor;
        dvr::cine_fov::Scope scope;
        check(scope.begin(&cache,dvr::cine_fov::gameplay_target(cache,108.0666f,103),true),"gameplay scope enters");
        check(scope.end(true) && cache==sensor,"gameplay scope restores persistent source");
    }
    check(near(sensor,108.0666f),"gameplay draw scopes cannot restart narrowing");
    check(near(dvr::cine_fov::gameplay_target(sensor,108.0666f,103),103),"unchanged projection setting converges to 103");
    std::printf("FOV feedback: %u checks passed; old writer collapses, fixed writer remains stable.\n",checks);
}
