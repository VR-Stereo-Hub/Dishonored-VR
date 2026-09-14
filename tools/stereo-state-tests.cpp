#include "../src/game/dishonored/stereo_state_policy.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void check(bool value,const char* why) { ++checks; if (!value) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); } }
int main() {
    using dvr::scene_state::eligible;
    const char* states[]={"StatePlayerMasterSoiree","StatePlayerMasterInDialog","StatePlayerMasterInScriptedChoice"};
    for (auto state:states) {
        check(eligible(false,true,false,true,true,state,false),"cinematic latch does not suppress a live view");
        check(eligible(false,true,false,false,true,state,true),"live scene covers dialogue view silence");
        check(!eligible(false,true,false,false,true,state,false),"silent scene falls back");
        check(!eligible(false,true,false,true,false,state,true),"stale/invalid state falls back");
        check(!eligible(false,false,false,true,true,state,true),"missing pawn falls back");
        check(!eligible(true,true,true,true,true,state,true),"menu/note beats even strict gameplay");
    }
    check(eligible(false,true,false,true,true,"StatePlayerMasterWalk",false),"walk handoff ignores stale cinematic latch");
    check(!eligible(false,true,false,false,true,"StatePlayerMasterWalk",true),"walk does not bypass loading qualification");
    check(!eligible(false,true,false,true,true,"StatePlayerMasterInStore",true),"store is not dialogue");
    check(!eligible(false,true,false,true,true,"unknown",true),"unknown state is not a cinematic");
    check(eligible(true,true,false,true,false,"unknown",false),"normal strict gameplay remains available without StateWatch");
    check(!eligible(true,false,false,true,true,"StatePlayerMasterWalk",true),"no-pawn beats strict");
    check(eligible(false,true,false,false,true,"StatePlayerMasterWalk",true,true),"closed verified UI and rendered scene survive head-write stall");
    check(!eligible(false,true,false,false,true,"StatePlayerMasterWalk",false,true),"UI clearance alone cannot authorize a silent scene");
    check(!eligible(false,true,true,false,true,"StatePlayerMasterWalk",true,true),"menu beats render fallback");
    check(!eligible(false,false,false,false,true,"StatePlayerMasterWalk",true,true),"live pawn required for render fallback");
    check(!eligible(false,true,false,false,false,"StatePlayerMasterWalk",true,true),"fresh FSM required for render fallback");
    check(!eligible(false,true,false,false,true,"unknown",true,true),"unknown state cannot use render fallback");
    std::printf("%u stereo-state policy checks passed\n",checks);
}
