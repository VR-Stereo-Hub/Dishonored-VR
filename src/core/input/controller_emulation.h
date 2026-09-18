#pragma once
#include "core/vr/input_snapshot.h"
#include <atomic>
#include <cmath>
#include <cstdint>

namespace dvr::controller {
// BioShock trilogy s63 numbering, adapted to Dishonored's held shortcuts.
enum Modifier { Off=0, RightRest=1, R3=2, LeftRest=4 }; // value3 retired: grips stay gameplay inputs
struct Config { int modifier=RightRest; bool flip=false, pauseChord=true; };
inline Config normalize(Config c) {
    if(c.modifier==3)c.modifier=Off; // old grip configs must never steal the wheel
    if(c.modifier<Off || c.modifier>LeftRest)c.modifier=RightRest;
    if(c.modifier==RightRest)c.flip=false;
    if(c.modifier==LeftRest)c.flip=true;
    return c;
}
inline unsigned pack(Config c) {
    c=normalize(c);
    return unsigned(c.modifier) | (c.flip ? 8u : 0u) | (c.pauseChord ? 16u : 0u);
}
inline Config unpack(unsigned v) {return {int(v&7),bool(v&8),bool(v&16)};}
inline std::atomic<unsigned> settingBits{pack(Config{})};
inline Config config(){return unpack(settingBits.load());}
inline void configure(Config c){settingBits.store(pack(c));}
constexpr uint16_t Up=0x1, Down=0x2, Left=0x4, Right=0x8, Start=0x10, Back=0x20;
struct Result {uint16_t buttons=0;bool modifier=false, suppressLeft=false,suppressRight=false,lean=false;};
struct Composer {
    unsigned settings=~0u;
    bool chordLatch=false, menuWas=false, menuBack=false;
    uint64_t menuSince=0,startUntil=0;
    void reset(){*this=Composer{};}
    Result step(dvr::vr::InputSnapshot& s,Config c,uint64_t now,bool leanGameplay=false) {
        const unsigned bits=pack(c);c=unpack(bits);
        if(settings!=bits){reset();settings=bits;}
        Result out;
        if(!s.active){reset();return out;}
        switch(c.modifier){
        case RightRest:out.modifier=s.restR;break;
        case R3:out.modifier=s.clkR; s.clkR=false;break;
        case LeftRest:out.modifier=s.restL;break;
        default:break;
        }
        const bool chord=c.pauseChord && s.x && s.y;
        if(chord)chordLatch=true;
        else if(!s.x && !s.y)chordLatch=false;
        if(chordLatch){s.x=false;s.y=false;}
        const bool menu=s.menu || chord;
        // Same tap/hold/menu-modifier shape as BS1. Once BACK owns a gesture,
        // releasing its modifier first cannot turn its tail into a pause tap.
        if(menu){
            if(!menuWas){menuSince=now;menuBack=false;startUntil=0;}
            if(out.modifier || now-menuSince>=500)menuBack=true;
            if(menuBack){out.buttons|=Back;startUntil=0;}
        }else if(menuWas && !menuBack) startUntil=now+150;
        menuWas=menu;
        if(now<startUntil)out.buttons|=Start;
        // Y is on the left controller. In gameplay its opposite stick feeds
        // the native lean axes, taking precedence over D-pad selection.
        out.lean=leanGameplay && s.y;
        if(out.modifier && !out.lean){
            float* axis=c.flip ? s.lk : s.mv;
            const float ax=std::fabs(axis[0]),ay=std::fabs(axis[1]);
            uint16_t direction=0;
            if(std::isfinite(ax) && std::isfinite(ay)){
                if(ay>=.65f && ay>=ax)direction=axis[1]>0 ? Up : Down;
                else if(ax>=.65f && ax>ay)direction=axis[0]>0 ? Right : Left;
            }
            // Held, not pulses: Dishonored selects on press and uses on release.
            out.buttons|=direction;
            // As in BS1, merely resting a thumb must not kill sub-threshold input.
            if(direction || c.modifier==R3){
                axis[0]=axis[1]=0;
                out.suppressLeft=!c.flip;out.suppressRight=c.flip;
            }
        }
        return out;
    }
};
// A released physical wheel gesture cannot keep owning controller axes merely
// because the game's UI open bit stayed set after an interrupted action.
inline bool released_wheel(bool active,bool reportedWheel,bool held,bool scriptMenu,bool cinematic) {
    return active && reportedWheel && !held && !scriptMenu && !cinematic;
}
// Final boundary after menu shaping: preserve native vertical menu scrolling,
// and route right-stick lean into the game's left axes without also turning.
inline void final_axes(const Result& r,bool menu,bool wheel,int16_t rightX,int16_t rightY,
                       int16_t& lx,int16_t& ly,int16_t& rx,int16_t& ry) {
    if(menu && !wheel)ry=rightY;
    if(r.suppressLeft && !wheel)lx=ly=0;
    if(r.suppressRight)rx=ry=0;
    if(r.lean){lx=rightX;ly=rightY;rx=ry=0;}
}
} // namespace dvr::controller
