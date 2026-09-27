#include "core/gfx/hud_layout.h"
#include "core/input/reading_input.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include "core/gfx/hud_marker.h"
#include <initializer_list>
namespace dvr::hudlayout {
static dvr::hudalpha::Bank g_alphaBank;
static auto& g_alpha=g_alphaBank.general;static bool g_wheelParts=false,g_dialOn=true;
static bool g_visualRiding=false;static int g_ridingContext=-1;
static ElementCfg g_el[ElCount]{};
struct Sink {int anchor=-1;bool crop=false;int element=-1;} g_sink[kMaxSinks];
int element_for_context(int c) {return c==4?ElNote:c==5?ElJournal:c==6?ElWheel:ElPause;}
#include "hud_alpha_selector.inc"
#include "hud_wheel_coverage.inc"
}
#include "hud_alpha_capture.inc"
using SHORT=int16_t;
static double testTime=0;
static double MaimNowMs(){return testTime;}
#include "hud_menu_step.inc"
namespace dvr::hudcap {
struct Sink {bool slotValid[2]={true,true},delivered=true,clearBeforeDraw=false;unsigned redirected=5;dvr::hudmarker::Delivery markers;};
static Sink g_sink[3];
#include "hud_invalidate.inc"
}
static unsigned checks=0;
static void check(bool v,const char* why){++checks;if(!v){std::printf("FAIL %s\n",why);std::exit(1);}}
int main(){
 using namespace dvr::hudlayout;
 dvr::hudcap::invalidate_content();
 for(const auto& s:dvr::hudcap::g_sink)
   check(!s.slotValid[0] && !s.slotValid[1] && !s.delivered && s.redirected==0 && s.clearBeforeDraw,
         "menu owner change invalidates both delayed images and clears target before reuse");
 g_alphaBank.general={2,2,.2f,.6f,.4f};
 g_alphaBank.special[0]={0,3,0,.5f,1};
 g_alphaBank.special[1]={1,1.2f,.1f,.7f,.3f};
 g_alphaBank.special[2]={2,1.5f,.4f,.8f,.9f};
 g_sink[0]={AnchorWindow,false,-1};g_sink[1]={AnchorWindow,true,ElPrompt};g_sink[2]={AnchorHandR,true,ElVitals};
 g_el[ElWheel].anchor=g_el[ElNote].anchor=g_el[ElJournal].anchor=AnchorWindow;
 g_visualRiding=true;g_ridingContext=6;
 check(alpha_for_sink(0).gain==3 && !alpha_force_wanted(0),"wheel uses its own profile and capture mode");
 g_ridingContext=4;auto note=alpha_for_sink(0);
 check(note.gamma==.7f && note.mode==1 && alpha_force_wanted(0),"reading coverage is forced from reading alpha");
 g_ridingContext=5;auto journal=alpha_for_sink(0);
 check(journal.mode==note.mode && journal.gamma==note.gamma && journal.gain==note.gain,"notes books and journal share profile");
 g_alphaBank.special[3]={1,1.3f,.2f,.9f,.8f};g_el[ElPause].anchor=AnchorWindow;
 g_ridingContext=3;check(alpha_for_sink(0).gain==1.3f && alpha_force_wanted(0),"pause owns independent alpha and coverage");
 g_alphaBank.reset_general();check(alpha_for_sink(0).gamma==.9f,"general reset preserves pause alpha");
 g_alphaBank.general={2,2,.2f,.6f,.4f};
 g_visualRiding=false;
 check(alpha_for_sink(1).gain==1.5f && alpha_force_wanted(1),"interaction profile selects its private sink");
 check(alpha_for_sink(2).gain==2 && alpha_force_wanted(2),"unscoped HUD uses general alpha");
 g_alphaBank.reset_general();auto orig=alpha_for_sink(2);
 check(orig.mode==0 && orig.gain==1 && orig.floorA==0 && orig.gamma==1 && orig.mixK==1,"original general reset restores all five fields");
 check(!alpha_force_wanted(2) && alpha_force_wanted(1),"general reset preserves interaction capture coverage");
 check(alpha_for_sink(1).gamma==.8f,"general reset preserves interaction gamma");
 g_visualRiding=true;g_ridingContext=4;
 check(alpha_for_sink(0).gamma==.7f && alpha_force_wanted(0),"general reset preserves reading gamma and coverage");
 g_ridingContext=6;check(alpha_for_sink(0).gain==3 && alpha_for_sink(0).gamma==.5f,"general reset preserves wheel");
 using dvr::reading_input::continuous;using dvr::reading_input::vertical;
 for(int context=-1;context<=8;++context) for(bool active:{false,true}) for(bool wheel:{false,true})
   check(continuous(context,wheel,active)==(active&&!wheel&&(context==4||context==5)),"continuous reading is strictly context scoped");
 for(int c=-1;c<=8;++c) for(bool active:{false,true}) for(bool wheel:{false,true})
   check(dvr::reading_input::pause(c,wheel,active)==(active&&!wheel&&(c==1||c==3||c==7)),"native stick axes: main menu, pause and store only");
 int oldPulses=0,newSamples=0;
 for(int i=0;i<120;++i){testTime=i*1000./120;if(MenuStep(-32767,1))++oldPulses;if(vertical(-32767)==-32767)++newSamples;}
 check(oldPulses<10 && newSamples==120,"one-second reading hold keeps 120 analog samples instead of sparse menu pulses");
 check(vertical(0)==0 && vertical(16384)==16384 && vertical(-16384)==-16384,"neutral partial speed and direction stay native");
 g_visualRiding=true;g_ridingContext=6;g_wheelParts=true;
 g_alphaBank.special[0].mode=AlphaRepair;g_alpha.mode=AlphaRepair;g_alphaBank.special[4]={AlphaCaptured,.6f,.18f,1.16f,1};
 check(alpha_force_wanted(0),"side panels obtain coverage even when wheel uses repair alpha");
 check(wheel_parts_alpha().gain==.6f && wheel_parts_alpha().gamma==1.16f,"both side crops select shared dedicated alpha");
 g_alphaBank.reset_general();check(wheel_parts_alpha().floorA==.18f,"general reset preserves side panel settings");
 check(alpha_for_sink(0).mode==AlphaRepair,"side alpha does not replace wheel alpha");
 g_wheelParts=false;check(!alpha_force_wanted(0),"disabling side panels restores original wheel capture policy");
 g_wheelParts=true;g_visualRiding=false;check(!wheel_parts_for_sink(0),"no side panels outside wheel visual lease");
 g_visualRiding=true;g_ridingContext=4;check(!wheel_parts_for_sink(0),"notes cannot inherit wheel side panels");
 std::printf("%u HUD controls checks passed; old menu pulses %d/120, reading %d/120\n",checks,oldPulses,newSamples);
}
