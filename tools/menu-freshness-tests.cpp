#include <cstdio>
#include <cstdlib>
#include "core/gfx/pause_scene_freshness.h"
static int checks=0;
void check(bool ok,const char* why){++checks;if(!ok){printf("FAIL %s\n",why);exit(1);}}
int main(){
 dvr::stereo::PauseSceneFreshness fresh;
 // VR-178: replay the same render-during-draw schedule in every riding context.
 for(int c=3;c<=8;++c) {
  dvr::stereo::MenuSceneFreshness menu;
  menu.begin(true,c,1,1);
  check(!menu.recent(true,c,1,1,true,100),"unobserved menu refuses");
  menu.complete(4,8,100);
  check(menu.recent(true,c,1,1,true,110),"prior-draw uploads authorize current riding menu");
  check(!menu.recent(false,c,1,1,true,110),"menu lever off refuses");
  check(!menu.recent(true,c,1,1,false,110),"head look off refuses");
  check(!menu.recent(true,c,1,1,true,99),"menu clock rollback refuses");
  check(menu.recent(true,c,1,1,true,199),"recent evidence lasts less than100ms");
  check(!menu.recent(true,c,1,1,true,200),"menu evidence expires at100ms");
  menu.complete(8,8,195);
  check(!menu.recent(true,c,1,1,true,200),"silent menu draws cannot renew evidence");
  menu.begin(true,c,2,1);
  check(!menu.recent(true,c,2,1,true,110),"new menu epoch clears evidence even at same context");
  menu.complete(8,9,110);menu.begin(true,c,2,2);
  check(!menu.recent(true,c,2,2,true,120),"level load clears evidence");
  menu.complete(9,10,120);menu.begin(false,c,2,2);menu.complete(10,11,125);
  check(!menu.recent(true,c,2,2,true,130),"ineligible draw cannot seed menu evidence");
 }
 dvr::stereo::MenuSceneFreshness menu;
 menu.begin(true,5,1,1);menu.complete(1,2,100);
 check(!menu.recent(true,6,1,1,true,110),"journal evidence cannot authorize wheel");
 check(!menu.recent(true,5,2,1,true,110),"mid-draw epoch change refuses evidence");
 check(!menu.recent(true,5,1,2,true,110),"mid-draw load change refuses evidence");
 menu.begin(true,6,1,1);
 check(!menu.recent(true,6,1,1,true,110),"changed context discards previous menu evidence");
 menu.begin(true,2,1,1);menu.complete(1,2,100);
 check(!menu.recent(true,2,1,1,true,110),"loading is never a riding-menu exception");
 fresh.complete(1,2,100);
 check(!fresh.recent(true,5,true,110) && !fresh.recent(true,6,true,110),"negative control: old pause policy rejects live journal and wheel");
 printf("menu freshness: %d checks PASS\n",checks);
}
