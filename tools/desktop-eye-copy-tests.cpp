// Compile the actual copy module against a deterministic D3D9 device double.
#include "../src/core/gfx/desktop_eye.cpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace dvr::desktop_eye;
static uint32_t serial=0;
static unsigned checks=0;
static void check(bool v,const char* why) { ++checks; if(!v) { std::fprintf(stderr,"FAIL: %s\n",why); std::exit(1); } }
static Record present(IDirect3DDevice9& d,int draw,int tag,bool publish=true) {
    d.bb.initialized=true; d.bb.pixels=draw ? draw : -1;
    begin_present(++serial);
    if(publish) note_drawn_eye(draw);
    on_present(tag);
    Record r; check(record_for(serial,r),"history identity"); return r;
}
static HRESULT __stdcall nativePresent(IDirect3DDevice9* d,const RECT*,const RECT*,HWND,const RGNDATA*) {
    ++d->presents;
    if (d->nextPresent==D3D_OK) d->frontPixels=d->bb.pixels;
    // Model DISCARD: the captured eye must be independent of what remains
    // after a real Present. Each following scene replaces every pixel.
    d->bb.pixels=999; d->bb.initialized=false;
    return d->nextPresent;
}
static Record frame(IDirect3DDevice9& d,int draw,int tag,bool ready=true,bool callback=true,
                    const RECT* src=nullptr,const RECT* dst=nullptr,HWND wnd=nullptr,const RGNDATA* dirty=nullptr,
                    bool xrReady=false) {
    d.bb.initialized=true; d.bb.pixels=draw ? draw : 7;
    begin_present(++serial); note_drawn_eye(draw);
    const int captured=d.bb.pixels; ++d.captures;
    if(callback) on_present(tag);
    HRESULT hr=dvr::desktop_eye::present(nativePresent,&d,src,dst,wnd,dirty,ready,xrReady);
    Record r; check(record_for(serial,r),"tail history identity");
    check(captured==(draw ? draw : 7),"capture retains this eye across desktop operations");
    check(hr==(r.nativeCalled ? d.nextPresent : D3D_OK),"native errors survive; skip returns success");
    return r;
}
static void reduction_tests() {
    IDirect3DDevice9 d;
    set_device(&d); set_source("draw","test"); set_enabled(true);
    check(!reduced_present(),"compiled default is off");
    frame(d,-1,1); frame(d,1,-1);
    check(d.presents==2 && d.snaps==1 && d.blits==1,"off preserves two Presents and both mirror copies");
    set_reduced_present(true);
    check(frame(d,1,-1).nativeCalled,"toggle cannot inherit pre-toggle permission");
    const unsigned before=d.presents, snaps=d.snaps, blits=d.blits, captures=d.captures;
    for(int i=0;i<100;++i) {
        check(frame(d,-1,1).nativeCalled,"left always presents");
        auto r=frame(d,1,-1);
        check(r.action=='K' && !r.nativeCalled && r.shown==-1,"right keeps the displayed left");
        check(d.frontPixels==-1 && d.bb.pixels==1,"skip does not overwrite current right pixels");
    }
    check(d.presents-before==100 && d.snaps-snaps==100 && d.blits-blits==0 && d.captures-captures==200,
        "100 pairs remove 100 native calls and 100 restores without removing any capture");
    check(frame(d,1,-1).nativeCalled,"cannot skip two consecutive calls");
    frame(d,-1,1);
    check(frame(d,0,0).nativeCalled,"unknown always presents");
    check(frame(d,1,-1).nativeCalled,"unknown consumes predecessor");
    frame(d,-1,1);
    check(frame(d,1,-1,false).nativeCalled,"XR failure/mono at final tail falls back");
    check(d.frontPixels==-1,"failed tail eligibility still restores left before native Present");
    frame(d,-1,1);
    check(frame(d,1,-1,true,false).nativeCalled,"missing runtime callback never suppresses");
    check(frame(d,1,-1).nativeCalled,"missing callback consumes predecessor");
    RECT rect; RGNDATA dirty;
    frame(d,-1,1); check(frame(d,1,-1,true,true,&rect).nativeCalled,"source rect preserved");
    frame(d,-1,1); check(frame(d,1,-1,true,true,nullptr,&rect).nativeCalled,"destination rect preserved");
    frame(d,-1,1); check(frame(d,1,-1,true,true,nullptr,nullptr,&d).nativeCalled,"window override preserved");
    frame(d,-1,1); check(frame(d,1,-1,true,true,nullptr,nullptr,nullptr,&dirty).nativeCalled,"dirty region preserved");
    for(HRESULT hr : {E_FAIL, HRESULT(1)}) {
        d.nextPresent=hr; frame(d,-1,1); d.nextPresent=D3D_OK;
        check(frame(d,1,-1).nativeCalled,"only exact D3D_OK from left permits skip, not success statuses");
    }
    d.failSnap=true; frame(d,-1,1); d.failSnap=false;
    check(frame(d,1,-1).nativeCalled,"failed left snapshot cannot authorize skip");
    frame(d,-1,1); d.failGet=true;
    check(frame(d,1,-1).nativeCalled,"GetBackBuffer failure cannot authorize skip"); d.failGet=false;
    frame(d,-1,1); d.bb.desc.Width++;
    check(frame(d,1,-1).nativeCalled,"resize invalidates preceding left permission");
    frame(d,-1,1); on_reset();
    check(frame(d,1,-1).nativeCalled,"Reset invalidates preceding left permission");
    frame(d,-1,1); set_enabled(false);
    check(frame(d,1,-1).nativeCalled,"pin disabled forces native Present"); set_enabled(true);
    frame(d,-1,1); set_source("tag","test");
    check(frame(d,1,-1).nativeCalled,"legacy delivered-tag source never suppresses"); set_source("draw","test");
    for(int variant=0;variant<7;++variant) {
        on_reset(); d.swap.pp=D3DPRESENT_PARAMETERS{}; d.failSwap=false; d.swap.fail=false;
        switch(variant) {
        case 0:d.swap.pp.Windowed=false;break; case 1:d.swap.pp.SwapEffect=3;break;
        case 2:d.swap.pp.BackBufferCount=2;break; case 3:d.swap.pp.MultiSampleType=2;break;
        case 4:d.swap.pp.PresentationInterval=1;break; case 5:d.failSwap=true;break; case 6:d.swap.fail=true;break;
        }
        frame(d,-1,1);
        check(frame(d,1,-1).nativeCalled,"unsupported/failed parameter query forces native Present");
    }
    on_reset(); d.swap.pp=D3DPRESENT_PARAMETERS{}; d.failSwap=false; d.swap.fail=false;
    frame(d,-1,1); set_reduced_present(false);
    auto off=frame(d,1,-1);
    check(off.nativeCalled && off.action=='B',"off immediately restores accepted copy path");
    set_reduced_present(true); frame(d,-1,1);
    IDirect3DDevice9 other; set_device(&other);
    check(frame(other,1,-1).nativeCalled,"device replacement cannot reuse left token");
    frame(other,-1,1);
    serial+=3;
    check(frame(other,1,-1).nativeCalled,"bypassed hook gap cannot reuse left token");
    // Every length-eight ternary sequence, both delayed and current tags.
    for(int lag : {0,1}) for(unsigned bits=0;bits<6561;++bits) {
        on_reset(); unsigned v=bits; int previous=0; bool skipped=false;
        for(int i=0;i<8;++i) {
            const int draw=int(v%3)-1; v/=3;
            auto r=frame(other,draw,lag ? previous : draw);
            check(r.nativeCalled || (draw==1 && previous==-1 && !skipped),"exhaustive omission only follows a real left");
            if(!r.nativeCalled) check(other.frontPixels==-1,"exhaustive omitted call holds left desktop");
            skipped=!r.nativeCalled; previous=draw;
        }
    }
    set_reduced_present(false); shutdown();
}
static void mirror_off_tests() {
    IDirect3DDevice9 d; set_device(&d); set_enabled(true); set_source("draw","test");
    check(!mirror_off(),"mirror-off default is false");
    frame(d,-1,1);
    auto offFrame = [&](int draw, bool xr=true, bool callback=true) {
        return frame(d,draw,draw,false,callback,nullptr,nullptr,nullptr,nullptr,xr);
    };
    set_mirror_off(true);
    const unsigned presents=d.presents, snaps=d.snaps, blits=d.blits, captures=d.captures;
    for(int i=0;i<100;++i) {
        auto r=offFrame(i%3-1);
        check(!r.nativeCalled && r.action=='O',"off omits desktop calls for left/right/mono XR frames");
        check(d.frontPixels==-1,"off retains the last desktop image");
    }
    check(d.presents==presents && d.snaps==snaps && d.blits==blits && d.captures==captures+100,
        "off removes all mirror copies and Presents while every headset capture remains");
    check(d.lastQuery && d.lastQuery->flushes==100,"off explicitly submits every skipped frame");
    d.lastQuery->result=S_FALSE;
    const unsigned issues=d.lastQuery->issues;
    offFrame(-1); offFrame(1);
    check(d.lastQuery->issues==issues+2,"each flush covers current work, abandoning old result without waiting");
    d.lastQuery->result=D3D_OK;
    check(offFrame(0).action=='O',"completed event continues desktop-free submission");
    check(offFrame(1,false).nativeCalled,"XR capture/session failure restores native Present");
    check(offFrame(1,true,false).nativeCalled,"missing runtime hook restores native Present");
    d.lastQuery->failGet=true;
    check(offFrame(-1).nativeCalled,"failed flush does not fake native success");
    d.lastQuery->failGet=false;
    check(offFrame(1).nativeCalled,"flush refusal persists until reset or mode toggle");
    on_reset(); d.failQuery=true;
    check(offFrame(-1).nativeCalled,"unsupported event query restores native path");
    d.failQuery=false; on_reset();
    offFrame(-1); d.lastQuery->failIssue=true;
    check(offFrame(1).nativeCalled,"failed query issue restores native path");
    set_mirror_off(false); set_mirror_off(true);
    check(offFrame(1).action=='O',"mode toggle recovers query refusal");
    RECT rect;
    check(frame(d,1,1,false,true,&rect,nullptr,nullptr,nullptr,true).nativeCalled,
        "off never suppresses custom Present arguments");
    on_reset(); d.swap.pp.Windowed=false;
    check(offFrame(-1).nativeCalled,"off refuses unsupported presentation parameters");
    d.swap.pp=D3DPRESENT_PARAMETERS{}; on_reset();
    set_reduced_present(true);
    check(offFrame(-1).action=='O' && offFrame(1).action=='O',"off takes precedence over reduction");
    set_mirror_off(false);
    check(frame(d,1,-1).nativeCalled,"leaving off cannot reuse old left snapshot or permission");
    frame(d,-1,1); check(!frame(d,1,-1).nativeCalled,"reduced mode rearms on a fresh displayed left");
    set_reduced_present(false); shutdown();
}
int main() {
    IDirect3DDevice9 d;
    set_device(&d); set_source("draw","test");
    check(present(d,1,-1).action=='N',"startup R cannot copy uninitialized target");
    for(int draw : {-1,1,0,-1,1,0,-1,1}) {
        static int tag=0;
        present(d,draw,tag); tag=draw;
        check(d.bb.pixels==-1,"actual delayed copy must hold left pixels");
    }
    check(d.uninitializedReads==0,"no empty target reads");
    on_reset();
    check(present(d,1,-1).action=='N',"Reset does not reuse lifetime snap count");
    present(d,-1,1);
    d.bb.desc.Width=200;
    check(present(d,1,-1).action=='N',"resize invalidates held image");
    present(d,-1,1);
    for(int i=0;i<3;++i) check(present(d,0,0).action=='B',"unknown hold");
    check(present(d,0,0).action=='N',"menu releases on fourth unknown");
    check(present(d,1,-1).action=='N',"menu expired image stays invalid");
    present(d,-1,1);
    set_enabled(false); present(d,1,-1); check(d.bb.pixels==1,"disabled passes raw frame");
    set_enabled(true); check(present(d,1,-1).action=='N',"enable starts empty");
    present(d,-1,1);
    set_source("tag","test");
    check(present(d,1,-1).action=='S',"legacy snapshots right pixels on delayed left tag");
    check(present(d,-1,1).shown==1,"legacy blits held right pixels");
    check(present(d,-1,0).shown==-1,"legacy zero tag leaks raw left pixels");
    set_source("draw","test");
    check(present(d,1,-1).action=='N',"source switch cannot reuse legacy right snapshot");
    d.failSnap=true; check(present(d,-1,1).action=='F',"snapshot failure visible");
    d.failSnap=false; check(present(d,1,-1).action=='N',"failed snapshot not valid");
    present(d,-1,1);
    d.failBlit=true; check(present(d,1,-1).action=='F',"blit failure visible");
    d.failBlit=false; check(present(d,1,-1).action=='B',"blit retry retains good source");
    check(present(d,0,0,false).draw==0,"failed method does not inherit prior draw identity");
    IDirect3DDevice9 other;
    set_device(&other);
    check(present(other,1,-1).action=='N',"new device starts without valid pixels");
    on_reset(); other.failCreate=true;
    check(present(other,-1,1).action=='F',"allocation failure visible");
    other.failCreate=false;
    check(present(other,1,-1).action=='F',"refusal remains until Reset");
    on_reset(); present(other,-1,1);
    check(present(other,1,-1).shown==-1,"Reset recovers allocation refusal");
    check(!set_source("invalid","test") && !std::strcmp(source_name(),"draw"),"invalid lever cannot change policy");
    check(d.uninitializedReads==0 && other.uninitializedReads==0,"all paths avoid uninitialized copies");
    begin_present(++serial); Record missing;
    check(record_for(serial,missing) && missing.action=='?',"missed callback is distinguishable");
    shutdown();
    reduction_tests();
    mirror_off_tests();
    std::printf("PASS: %u actual-module copy, lifecycle, failure and history assertions\n",checks);
}
