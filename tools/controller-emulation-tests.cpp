#include "core/input/controller_emulation.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <initializer_list>
#include <windows.h>
#include <Xinput.h>
using namespace dvr::controller;
using dvr::vr::InputSnapshot;
static int checks=0;
static void check(bool ok,const char* why){++checks;if(!ok){std::printf("FAIL %s\n",why);std::exit(1);}}
static InputSnapshot sample(){InputSnapshot s;s.active=true;return s;}
static WORD face(const InputSnapshot& in){WORD b=0;
#include "controller_face.inc"
return b;}
static void pressModifier(InputSnapshot& s,int m){
    if(m==RightRest)s.restR=true;
    if(m==LeftRest)s.restL=true;
    if(m==R3)s.clkR=true;
    if(m==LeftGrip)s.gripL=1;
}
int main(){
    static_assert(Up==XINPUT_GAMEPAD_DPAD_UP && Down==XINPUT_GAMEPAD_DPAD_DOWN &&
        Left==XINPUT_GAMEPAD_DPAD_LEFT && Right==XINPUT_GAMEPAD_DPAD_RIGHT &&
        Start==XINPUT_GAMEPAD_START && Back==XINPUT_GAMEPAD_BACK);
    const float coords[4][2]={{0,1},{0,-1},{-1,0},{1,0}};
    const unsigned expected[]={Up,Down,Left,Right};
    for(int mode=Off;mode<=LeftRest;++mode) for(int flip=0;flip<2;++flip){
        Config c{mode,flip!=0,true}; Composer p;
        for(int dir=0;dir<4;++dir){
            auto s=sample();pressModifier(s,mode);s.mv[0]=s.lk[0]=.2f;
            auto* axis=flip ? s.lk : s.mv;axis[0]=coords[dir][0];axis[1]=coords[dir][1];
            auto out=p.step(s,c,100);
            check(out.buttons==(mode ? expected[dir] : 0),"all modes/sides/directions");
            check((flip ? s.mv[0] : s.lk[0])==.2f,"opposite stick preserved");
            check(mode ? axis[0]==0 && axis[1]==0 : (axis[0]!=0 || axis[1]!=0),"only selecting stick consumed");
            if(mode==R3)check(!s.clkR,"modifier must not consume elixir");
            if(mode==LeftGrip)check(!s.gripL,"modifier must not open wheel");
            s=sample();out=p.step(s,c,110);check(!out.buttons,"direction releases on neutral/modifier release");
        }
    }
    {Composer p;auto s=sample();s.restR=true;s.mv[0]=.4f;auto o=p.step(s,{},0);
     check(!o.buttons && s.mv[0]==.4f,"rest alone preserves subthreshold walk");
     s=sample();s.restR=true;s.mv[0]=.75f;s.mv[1]=.9f;o=p.step(s,{},1);check(o.buttons==Up,"diagonal chooses dominant Y");
     s=sample();s.restR=true;s.mv[0]=-.9f;s.mv[1]=.75f;o=p.step(s,{},2);check(o.buttons==Left,"diagonal chooses dominant X");
     s=sample();s.restR=true;s.mv[0]=std::numeric_limits<float>::quiet_NaN();s.mv[1]=1;o=p.step(s,{},3);check(!o.buttons,"invalid axes never fabricate direction");}
    {Composer p;auto s=sample();s.menu=true;check(!p.step(s,{},100).buttons,"tap waits for release");
     s=sample();check(p.step(s,{},150).buttons==Start,"menu tap pulses start");
     s=sample();check(p.step(s,{},299).buttons==Start,"start survives engine poll interval");
     s=sample();check(!p.step(s,{},300).buttons,"start pulse expires");}
    {Composer p;auto s=sample();s.menu=true;check(!p.step(s,{},100).buttons,"long hold starts neutral");
     s=sample();s.menu=true;check(p.step(s,{},600).buttons==Back,"hold fallback opens journal");
     s=sample();check(!p.step(s,{},700).buttons,"long hold release never pauses");}
    for(int m=RightRest;m<=LeftRest;++m){Composer p;Config c{m,false,true};auto s=sample();s.menu=true;pressModifier(s,m);
     check(p.step(s,c,100).buttons==Back,"each modifier selects second menu");
     s=sample();s.menu=true;check(p.step(s,c,110).buttons==Back,"release modifier first retains journal gesture");
     s=sample();check(!p.step(s,c,120).buttons,"journal release never pauses");}
    for(bool mod:{false,true}){Composer p;auto s=sample();s.x=s.y=true;s.restR=mod;auto o=p.step(s,{},100);
     check(o.buttons==(mod ? Back : 0),"chord obeys menu modifier");check(!s.x && !s.y,"chord consumes face buttons");
     s=sample();s.y=true;o=p.step(s,{},120);check(!s.y,"trailing Y suppressed");check(o.buttons==(mod ? 0 : Start),"chord release has correct menu output");
     s=sample();p.step(s,{},300);s=sample();s.y=true;o=p.step(s,{},310);check(s.y && !o.buttons,"fresh Y after chord release remains native");}
    {Composer p;auto s=sample();s.x=s.y=true;auto o=p.step(s,{RightRest,false,false},100);
     check(s.x && s.y && !o.buttons,"disabled pause chord forwards both buttons");}
    {Composer p;auto s=sample();s.menu=true;p.step(s,{},100);s={};check(!p.step(s,{},200).buttons,"focus loss neutralizes pending gesture");
     s=sample();check(!p.step(s,{},210).buttons,"refocus cannot complete stale pause tap");}
    {Composer p;auto s=sample();s.menu=true;p.step(s,{},100);s=sample();check(!p.step(s,{Off,false,true},120).buttons,"config change cancels pending gesture");}
    {auto s=sample();s.y=true;check(face(s)==XINPUT_GAMEPAD_Y,"production Y maps Y not START");
     s=sample();s.menu=true;check(!face(s),"physical menu cannot bypass policy");
     s=sample();s.a=s.b=s.x=true;check(face(s)==(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B|XINPUT_GAMEPAD_X),"production face bindings retained");}
    {Composer p;auto s=sample();s.gripL=.75f;check(p.step(s,{LeftGrip,false,true},0).modifier,"grip hysteresis enters");
     s=sample();s.gripL=.60f;check(p.step(s,{LeftGrip,false,true},1).modifier,"grip hysteresis holds");
     s=sample();s.gripL=.50f;check(!p.step(s,{LeftGrip,false,true},2).modifier,"grip hysteresis releases");}
    check(unpack(pack({99,false,true})).modifier==RightRest,"invalid modifier falls back to known default");
    configure({R3,true,false});auto c=config();check(c.modifier==R3 && c.flip && !c.pauseChord,"settings publish complete tuple");
    std::printf("%d controller emulation checks passed\n",checks);
}
