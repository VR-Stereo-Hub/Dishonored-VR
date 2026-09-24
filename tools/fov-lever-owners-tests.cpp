#include <cstdint>
#include <cstdio>
#include <cstdlib>
using LONG=long;
struct Identity { uint8_t* obj=nullptr; unsigned serial=0; };
struct CtIdentity { Identity value; };
uint8_t camStorage, pcStorage;
uint8_t* g_camObj=&camStorage;
uint8_t* g_peCtrl=&pcStorage;
LONG g_mkLoadEvents=1;
float g_fovNatural=110;
uint32_t g_ctPcCamera=1;
unsigned uiEpoch=0, serial=1, rebuilds=0;
double clockMs=2000;
bool table=false, buildOk=true, objects=true, ownership=true, captureOk=true, layout=true;
unsigned UiSurfaceEpoch() { return uiEpoch; }
double MaimNowMs() { return clockMs; }
bool IsLiveObject(uint8_t* p) { return table && objects && (p==g_camObj || p==g_peCtrl); }
bool ChSlot(const CtIdentity& id) { return IsLiveObject(id.value.obj) && id.value.serial==serial; }
uint8_t* CtObject(uint8_t*, uint32_t off) { return ownership && off && IsLiveObject(g_camObj)?g_camObj:nullptr; }
bool ChCapture(uint8_t* p,CtIdentity* out) { if(!captureOk || !IsLiveObject(p)) return false; out->value={p,serial}; return true; }
bool BuildLiveSet() { ++rebuilds; table=buildOk; return buildOk; }
bool FindPropOffsetChecked(const char*,const char*,uint32_t* out) { if(!layout) return false; *out=1; return true; }
template<class... T> void Log(const char*,T...) {}
#include "../src/game/dishonored/fov_lever_owners.cpp"
unsigned checks=0;
void check(bool v,const char* msg) { ++checks; if(!v) { std::fprintf(stderr,"FAIL: %s\n",msg); std::exit(1); } }
int main() {
    check(FovLeverOwnersReady() && rebuilds==1 && g_fovNatural==0,"initial ownership needs fresh live table and new baseline");
    g_fovNatural=110;
    check(FovLeverOwnersReady() && rebuilds==1,"steady dispatch validates without rebuilding");
    ++uiEpoch;
    check(FovLeverOwnersReady() && rebuilds==2 && g_fovNatural==110,"menu epoch revalidates same identity, retains baseline");
    ++g_mkLoadEvents; table=false;
    check(FovLeverOwnersReady() && rebuilds==3 && g_fovNatural==0,"load refreshes table even at unchanged pointers");
    g_fovNatural=110; ++serial;
    check(FovLeverOwnersReady() && g_fovNatural==0,"same pointer with new identity recaptures baseline");
    ownership=false;
    check(!FovLeverOwnersReady(),"wrong controller camera refuses");
    unsigned before=rebuilds;
    check(!FovLeverOwnersReady() && rebuilds==before,"failed refresh retry is bounded");
    ownership=true; clockMs+=1001;
    check(FovLeverOwnersReady(),"valid ownership recovers");
    objects=false;
    check(!FovLeverOwnersReady(),"freed object refuses despite unchanged pointers");
    objects=true; clockMs+=1001;
    check(FovLeverOwnersReady(),"live replacement recovers");
    ++uiEpoch; buildOk=false;
    check(!FovLeverOwnersReady(),"failed menu live-table refresh refuses");
    buildOk=true; clockMs+=1001;
    check(FovLeverOwnersReady(),"menu table rebuild recovers");
    ++g_mkLoadEvents; captureOk=false;
    check(!FovLeverOwnersReady(),"identity capture failure refuses");
    captureOk=true; clockMs+=1001;
    check(FovLeverOwnersReady(),"identity capture recovers");
    ++uiEpoch; g_ctPcCamera=0; layout=false;
    check(!FovLeverOwnersReady(),"missing ownership layout refuses");
    layout=true; clockMs+=1001;
    check(FovLeverOwnersReady(),"late-loaded reflected layout recovers");
    std::printf("FOV ownership: %u checks passed.\n",checks);
}
