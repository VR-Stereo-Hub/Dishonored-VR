#include "../src/game/dishonored/cinematic_fov_policy.h"
#include "../src/game/dishonored/anim_policy.h"
#include "../src/game/dishonored/stereo_state_policy.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
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
    // VR-228: pausing an authored narrow scene must retain the draw lock
    // when the verified menu explicitly permits stereo head look.
    check(!eligible(true,true,true,true,true,108),"old menu gate reproduces scope release");
    check(eligible(true,true,true,true,true,108,true),"riding head-look menu retains cinematic scope");
    check(!eligible(false,true,true,true,true,108,true),"menu permission cannot enable disabled lock");
    check(!eligible(true,true,true,false,true,108,true),"menu permission cannot enable mono projection");
    check(!eligible(true,true,true,true,false,108,true),"menu permission cannot revive invalid state");
    check(!eligible(true,false,true,true,true,108,true),"menu permission cannot invent a scene draw");
    check(!eligible(true,true,true,true,true,0,true),"menu permission still requires valid target");
    Scope paused; float nativePause=41.2f;
    check(paused.begin(&nativePause,108,true) && nativePause==108,"narrow paused camera widened for draw");
    check(paused.end(true) && std::fabs(nativePause-41.2f)<0.001f,"paused native camera restored after draw");
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
    ExitBridge bridge;
    check(bridge.update(true,false,52,108,1000),"cinematic primes exit protection");
    check(bridge.update(false,true,52,108,1100),"gameplay exit retains wide draw while native FOV is narrow");
    check(bridge.update(false,true,90,108,1800),"native recovery remains covered");
    check(!bridge.update(false,true,107.6f,108,2225),"converged native FOV releases without shrink");
    check(!bridge.update(false,true,52,108,2300),"ordinary gameplay zoom cannot initiate protection");
    bridge.update(true,false,52,108,3000);
    check(!bridge.update(false,false,52,108,3100),"menu/load/changed identity cancels protection");
    bridge.update(true,false,52,108,4000); bridge.update(false,true,52,108,4100);
    check(!bridge.update(false,true,52,108,7100),"stalled sensor expires at bounded deadline");
    bridge.update(true,false,52,108,8000);
    check(!bridge.update(false,true,0,108,8100),"invalid sensor refuses tail");
    const float wide=108.0666f;
    const float rad=0.017453292519943295f;
    check(std::fabs(gameplay_target(wide,wide,90)-90)<0.0001f,"normal gameplay requests exactly 90");
    check(gameplay_target(wide,wide,0)==0,"off does not request a scope");
    check(gameplay_target(wide,wide,150)==0,"unsupported request refuses");
    check(gameplay_target(0,wide,90)==0,"missing source refuses");
    check(gameplay_target(wide,0,90)==0,"missing headset target refuses");
    check(gameplay_target(wide,wide,std::numeric_limits<float>::quiet_NaN())==0,"NaN request refuses");
    const float zoom=gameplay_target(40,wide,90);
    const float beforeMag=std::tan(wide*rad/2)/std::tan(40*rad/2);
    const float afterMag=std::tan(90*rad/2)/std::tan(zoom*rad/2);
    check(std::fabs(beforeMag-afterMag)<0.0001f,"authored optical zoom magnification preserved");
    float native=wide;
    for(unsigned i=0;i<10000;++i) {
        Scope world;
        check(world.begin(&native,gameplay_target(native,wide,90),true),"gameplay scope enters");
        check(std::fabs(native-90)<0.0001f,"both eye reads remain at target");
        check(world.end(true) && native==wide,"source restored without accumulating feedback");
    }
    using dvr::scene_state::cinematic;
    for (auto state:{"StatePlayerMasterSoiree","StatePlayerMasterInDialog","StatePlayerMasterInScriptedChoice"})
        check(cinematic(state),"cinematic handback state classified");
    check(!cinematic("StatePlayerMasterWalk"),"walk releases cinematic ownership");
    check(!cinematic("StatePlayerMasterInStore"),"store does not enter cinematic handback");
    dvr::anim::Handoff classify, blend;
    classify.update(true,true,true,1000,250,0);
    blend.update(true,classify.game,true,1000,0,150);
    check(classify.game && blend.value(1150,150)==0,"native pose reached after entry blend");
    classify.update(true,false,true,1200,250,0);
    check(classify.game,"short state gap holds ownership");
    classify.update(true,false,true,1450,250,0);
    blend.update(true,classify.game,true,1450,0,150);
    check(!classify.game && blend.value(1600,150)==1,"controller pose restored after exit blend");
    classify.update(false,true,true,1700,250,0);
    check(!classify.game,"invalid snapshot cannot own hands");
    check(!dvr::anim::fresh(1000,1151),"stale snapshot expires");
    std::printf("%u cinematic FOV and handback checks passed\n",checks);
}
