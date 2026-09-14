#include "../src/game/dishonored/yaw_book.h"
#include "../src/game/dishonored/cinematic_handoff_policy.h"
#include <cstdio>
#include <cstdlib>
int main() {
 int count=0;
 auto check=[&](bool pass){++count;if(!pass){printf("FAIL %d\n",count);std::exit(1);}};
 const int32_t head49=8920,turn90=16384;
 YawBook old={head49,0,true};
 YawBookFresh(&old,0,0);check(YawBookBody(&old)==-head49); // reproduces diagonal offset
 YawBook resume={head49,0,true};
 YawBookFresh(&resume,0,0,true);check(YawBookBody(&resume)==0);
 YawBookFresh(&resume,turn90,0);check(YawBookBody(&resume)==turn90);
 YawBookFresh(&resume,turn90,1000);check(YawBookBody(&resume)==turn90);
 YawBookFresh(&resume,2*turn90+1000,500);check(YawBookBody(&resume)==2*turn90);
 YawBookFresh(&resume,-32000,100,true);check(YawBookBody(&resume)==-32000);
 YawBookFresh(&resume,-31900+turn90,0);check(YawBookBody(&resume)==-32000+turn90);
 check(!YawTargetFresh(0,100));check(YawTargetFresh(1000,1150));
 check(!YawTargetFresh(1000,1151));check(!YawTargetFresh(1000,999));
 // Owned PVR dispatches remain activity even though head writes stopped.
 check(CineDispatchRecent(true,10000,10020));
 check(CineDispatchRecent(true,10000,10749));
 check(!CineDispatchRecent(true,10000,10750));
 check(!CineDispatchRecent(false,10000,10020));
 check(!CineDispatchRecent(true,0,10020));
 check(!CineDispatchRecent(true,10000,9999));
 printf("PASS %d cinematic handoff regressions\n",count);
}
