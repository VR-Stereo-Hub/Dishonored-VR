#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <limits>
#include "game/dishonored/ue3/name_index_cache.h"

static bool g_pawnFromController = true;
static bool g_nameIndexCacheOn = true;
static uint32_t g_cylPawnOff = 16, g_cylHOff = 32, g_cylCompOff = 20;
static int g_cylTried = 1;
static uint8_t* g_cylMeasuredPawn = nullptr;
static double g_cylOkMs = 0, clockMs = 10000;
static float g_cylLast = 0;
static uint8_t *g_pePawn = nullptr, *g_peCtrl = nullptr;
struct Fake { alignas(8) uint8_t data[128]{}; bool live=true, readable=true; const char* cls="DishonoredPlayerPawn"; };
static Fake ctrl, pawn, oldPawn, comp;
static Fake* objects[] = {&ctrl,&pawn,&oldPawn,&comp};
static bool RangeReadable(const void* p, size_t n) {
    if (!p) return false;
    for (auto* o:objects) if ((uintptr_t)p >= (uintptr_t)o->data && (uintptr_t)p < (uintptr_t)o->data+128)
        return o->readable && (uintptr_t)p+n <= (uintptr_t)o->data+128;
    return true; // test-owned name-pool header
}
static bool IsLiveObject(uint8_t* p) { for(auto* o:objects) if(p==o->data) return o->live; return false; }
static const char* ObjClassName(uint8_t* p) { for(auto* o:objects) if(p==o->data) return o->cls; return nullptr; }
static bool LooksLikeObj(uint8_t* p) { return p && ObjClassName(p) && RangeReadable(p,64); }
static double MaimNowMs() { return clockMs; }
static void Log(const char*, ...) {}
static uint32_t FindPropOffset(const char* cls,const char*) { return !strcmp(cls,"Controller")?16:!strcmp(cls,"Actor")?20:32; }
static std::vector<std::string> names;
static uint32_t nameCount=0;
static void* nameHeader[2]{};
static const uintptr_t kGNamesData=(uintptr_t)nameHeader, kGNamesNum=(uintptr_t)&nameCount;
static bool refreshLive=false;
static unsigned rebuilds=0;
static bool BuildLiveSet() { ++rebuilds; if(refreshLive) for(auto* o:objects) o->live=true; return refreshLive; }
static unsigned reads=0;
static const char* NameFromIndex(uint32_t i) { ++reads; return i<names.size() && !names[i].empty()?names[i].c_str():nullptr; }
static bool PrintableName(const char* s) { return s && strlen(s)<64; }
#include "load_startup_body.inc"
static int fails=0, checks=0;
static void check(bool yes,const char* msg) { ++checks; if(!yes) { ++fails; printf("FAIL: %s\n",msg); } }
static void putPtr(Fake& f,unsigned off,uint8_t* p) { memcpy(f.data+off,&p,sizeof(p)); }
static void height(float v) { memcpy(comp.data+32,&v,4); }
int main(int argc,char** argv) {
    const bool legacyPawn=argc>1 && atoi(argv[1]), legacyNames=argc>2 && atoi(argv[2]);
    g_pawnFromController=!legacyPawn;
    ctrl.cls="DishonoredPlayerController"; comp.cls="CylinderComponent";
    g_peCtrl=ctrl.data; putPtr(ctrl,16,pawn.data); putPtr(pawn,20,comp.data); putPtr(oldPawn,20,comp.data); height(87.5f);
    check(PawnCollisionHeight()==87.5f && g_cylOkMs==clockMs,"stationary load before ANY pawn event establishes capsule liveness");
    g_pePawn=oldPawn.data;
    check(PawnForCollision()==pawn.data,"controller possession outranks previous event pawn");
    putPtr(ctrl,16,nullptr);
    check(PawnCollisionHeight()<0 && g_cylOkMs==0,"unpossess invalidates liveness rather than using old pawn");
    putPtr(ctrl,16,pawn.data); pawn.live=false;
    check(PawnCollisionHeight()<0,"unlisted pawn rejected"); pawn.live=true;
    pawn.cls="DisRat"; check(PawnCollisionHeight()<0,"non-player possession rejected"); pawn.cls="DishonoredPlayerPawnProxy";
    check(PawnCollisionHeight()<0,"proxy rejected"); pawn.cls="DishonoredPlayerPawn";
    comp.live=false; check(PawnCollisionHeight()<0,"unlisted capsule rejected"); comp.live=true;
    ctrl.readable=false; check(PawnCollisionHeight()<0,"unreadable controller rejected"); ctrl.readable=true;
    height(std::numeric_limits<float>::quiet_NaN()); check(PawnCollisionHeight()<0,"NaN capsule rejected");
    height(500); check(PawnCollisionHeight()<0,"out-of-range capsule rejected");
    height(65); check(PawnCollisionHeight()==65,"normal crouch still readable");
    g_pawnFromController=true; putPtr(oldPawn,20,nullptr); g_pePawn=oldPawn.data;
    check(PawnSetCollisionHeight(50) && ReadPawnCollisionHeight(pawn.data)==50,"deep crouch writes current possessed capsule, not old event pawn");
    check(!PawnSetCollisionHeight(1000),"new writer rejects out-of-range deep crouch value");
    putPtr(ctrl,16,nullptr); check(!PawnSetCollisionHeight(50),"deep crouch cannot write after unpossess");
    putPtr(ctrl,16,pawn.data); height(65);
    g_pawnFromController=false; g_pePawn=pawn.data; g_peCtrl=nullptr;
    check(PawnCollisionHeight()==65,"A/B off retains original event-latched path");
    g_pePawn=nullptr; check(PawnCollisionHeight()<0,"legacy path reproduces no-event load failure");

    g_pawnFromController=true; g_peCtrl=ctrl.data; g_pePawn=nullptr;
    ctrl.live=false; pawn.live=false; comp.live=false; refreshLive=true; clockMs+=2000;
    PawnCollisionTick();
    check(g_cylOkMs==clockMs && rebuilds==1,"script sample refreshes pre-load table without head/hand/animation drive");
    putPtr(ctrl,16,nullptr); clockMs+=2000; refreshLive=false; PawnCollisionTick();
    check(g_cylOkMs==0,"script sample clears unpossessed liveness");
    const unsigned before=rebuilds; clockMs+=60; PawnCollisionTick();
    check(rebuilds==before,"unavailable capsule rebuild retries bounded to one per second");
    g_pawnFromController=false; clockMs+=2000; PawnCollisionTick();
    check(rebuilds==before,"lever off performs no script-lane work");
    g_pawnFromController=true; g_cylHOff=g_cylCompOff=g_cylPawnOff=0; putPtr(ctrl,16,pawn.data); clockMs+=2000;
    check(PawnCollisionHeight()==65,"failed early reflection can resolve on a later sample");
    names.emplace_back("None"); for(unsigned i=1;i<5000;++i) names.emplace_back("Name"+std::to_string(i)); nameCount=(uint32_t)names.size();
    g_nameIndexCacheOn=!legacyNames;
    auto find=[&](const char* wanted) { return FindNameIdx(wanted); };
    check(find("Name4999")==4999,"cold lookup agrees with original scan");
    reads=0; check(find("Name4000")==4000,"warm lookup returns another discovered name");
    check(reads<=16,"warm lookup avoids thousands of repeated name reads"); printf("warm lookup engine reads: %u\n",reads);
    names[4000]="Reused";
    check(find("Name4000")==~uint32_t(0),"recycled ID cannot return stale name");
    check(find("Reused")==4000,"replacement name discovered");
    check(find("Later")==~uint32_t(0),"missing name fails honestly"); names.emplace_back("Later"); ++nameCount;
    check(find("Later")==5000,"missing lookup does not poison later name growth");
    names[4999].clear(); check(find("Name4999")==~uint32_t(0),"unreadable cached entry falls back");
    dvr::ue3::NameIndexCache<2> tiny;
    for(uint32_t i=1;i<20;++i) tiny.remember(names[i].c_str(),i);
    for(uint32_t i=1;i<20;++i) { auto id=tiny.find(names[i].c_str(),NameFromIndex); check(id==i || id==~uint32_t(0),"full cache never invents an answer"); }
    printf("load-startup: %d checks, %d failed\n",checks,fails);
    return fails?1:0;
}
