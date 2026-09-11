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
    std::printf("PASS: %u actual-module copy, lifecycle, failure and history assertions\n",checks);
}
