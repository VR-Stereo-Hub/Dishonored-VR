#pragma once
#include "core/vr/input_snapshot.h"
#include <atomic>
#include <cmath>
#include <cstdint>

namespace dvr::controller {
// BioShock trilogy s63 numbering, adapted to Dishonored's held shortcuts.
enum Modifier { Off=0, RightRest=1, R3=2, LeftRest=4 }; // value3 retired: grips stay gameplay inputs
// Y-held shortcuts. Quick save / quick load: hold Y, press the partner face button (A or B), in
// gameplay only; while Y is held that button is the shortcut's, never a jump or a crouch. Y alone
// and Y + stick lean are untouched, and X + Y stays the pause chord. Panel: hold Y and click the
// right stick to toggle the F10 panel (anywhere, so it also closes it); that click is not a zoom
// or an elixir.
enum Partner { NoPartner=0, PartnerA=1, PartnerB=2, PartnerX=3, PartnerY=4 };
enum Quick { NoQuick=0, QuickSave=1, QuickLoad=2 };
struct Config { int modifier=RightRest; bool flip=false, pauseChord=true; int quickSave=PartnerA, quickLoad=PartnerB; bool panelChord=true; };
inline Config normalize(Config c) {
    if(c.modifier==3)c.modifier=Off; // old grip configs must never steal the wheel
    if(c.modifier<Off || c.modifier>LeftRest)c.modifier=RightRest;
    if(c.modifier==RightRest)c.flip=false;
    if(c.modifier==LeftRest)c.flip=true;
    // X would be the pause chord and Y is the held button itself: only A and B can partner Y.
    if(c.quickSave!=PartnerA && c.quickSave!=PartnerB)c.quickSave=NoPartner;
    if((c.quickLoad!=PartnerA && c.quickLoad!=PartnerB) || c.quickLoad==c.quickSave)c.quickLoad=NoPartner;
    return c;
}
inline unsigned pack(Config c) {
    c=normalize(c);
    return unsigned(c.modifier) | (c.flip ? 8u : 0u) | (c.pauseChord ? 16u : 0u) |
        (unsigned(c.quickSave) << 5) | (unsigned(c.quickLoad) << 8) | (c.panelChord ? 2048u : 0u);
}
inline Config unpack(unsigned v) {return {int(v&7),bool(v&8),bool(v&16),int((v>>5)&7),int((v>>8)&7),bool(v&2048)};}
inline const char* partner_key(int p) {
    static const char* k[]={"None","A","B","X","Y"};
    return (p>=NoPartner && p<=PartnerY) ? k[p] : "None";
}
inline int parse_partner(const char* v,int fallback) {
    if(!v || !*v)return fallback;
    for(int p=NoPartner;p<=PartnerY;++p){
        const char* k=partner_key(p);int i=0;
        while(k[i] && v[i] && (k[i]|32)==(v[i]|32))++i;
        if(!k[i] && (!v[i] || v[i]==' ' || v[i]==';'))return p;
    }
    return fallback;
}
inline bool* partner(dvr::vr::InputSnapshot& s,int p) {
    switch(p){case PartnerA:return &s.a;case PartnerB:return &s.b;case PartnerX:return &s.x;case PartnerY:return &s.y;default:return nullptr;}
}
inline std::atomic<unsigned> settingBits{pack(Config{})};
inline Config config(){return unpack(settingBits.load());}
inline void configure(Config c){settingBits.store(pack(c));}
constexpr uint16_t Up=0x1, Down=0x2, Left=0x4, Right=0x8, Start=0x10, Back=0x20;
struct Result {uint16_t buttons=0;bool modifier=false, suppressLeft=false,suppressRight=false,lean=false,panel=false;
    int quick=NoQuick, quickRefused=NoQuick;};
struct Composer {
    unsigned settings=~0u;
    bool chordLatch=false, menuWas=false, menuBack=false, panelOwned=false;
    bool quickOwned[3]={false,false,false}, quickWas[3]={false,false,false};
    uint64_t menuSince=0,startUntil=0;
    void reset(){*this=Composer{};}
    Result step(dvr::vr::InputSnapshot& s,Config c,uint64_t now,bool leanGameplay=false) {
        const unsigned bits=pack(c);c=unpack(bits);
        if(settings!=bits){reset();settings=bits;}
        Result out;
        if(!s.active){reset();return out;}
        // Y + right stick click: the F10 panel. Ahead of the R3 modifier, which would take the click.
        if(c.panelChord && s.y && s.clkR && !panelOwned){out.panel=true;panelOwned=true;}
        if(panelOwned){
            if(!s.clkR)panelOwned=false;
            s.clkR=false;
        }
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
        // Y + A / Y + B. s.y is already false while X + Y is the pause chord. A press that
        // starts with Y held in gameplay belongs to the shortcut until it is released, even if Y
        // lets go first; one press is one save. Outside gameplay the press stays the game's.
        for(int q=QuickSave;q<=QuickLoad;++q){
            bool* b=partner(s,q==QuickSave ? c.quickSave : c.quickLoad);
            const bool down=b && *b;
            if(down && !quickWas[q] && s.y){
                if(leanGameplay){out.quick=q;quickOwned[q]=true;}
                else out.quickRefused=q;
            }
            quickWas[q]=down;
            if(!down)quickOwned[q]=false;
            if(quickOwned[q] || (down && s.y && leanGameplay))*b=false;
        }
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
// Presentation needs a short close grace; the native movie animates for250ms.
// A fresh hold, focus loss, another menu or a cinematic cancels the rejection.
struct WheelOwnerRelease {
    uint64_t since=0;
    bool pending=false;
    bool update(bool eligible,uint64_t now) {
        if(!eligible){pending=false;return false;}
        if(!pending || now<since){pending=true;since=now;}
        return now-since>=250;
    }
};
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
