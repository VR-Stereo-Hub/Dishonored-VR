// Compile the production attachment writer against a small UE3 fixture. No game is launched.
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "game/dishonored/patterns.h"
#include "game/dishonored/hands/menu_keep.h"
#include "game/dishonored/hands/weapon_frame.h"

using dvr::hf::Xform;
using dvr::hf::xform_mul;
using dvr::hf::identity3;
using dvr::menukeep::Identity;
struct Region { uintptr_t p; size_t size; };
static std::vector<Region> regions;
static std::set<uint8_t*> objects, live;
static std::map<uint8_t*,std::string> classes;
static std::map<uint32_t,std::string> names;
static std::map<uint8_t*,Xform> fixtureNative;
static std::map<std::pair<uint8_t*,uint32_t>,Xform> bones;
static uint8_t *fromFn, *toFn;
static unsigned frames=10, builds=0, boneCalls=0;
static bool failBuild=false, failFrom=false, failTo=false, recursive=false;
static std::function<void()> duringFrom;
static double nowMs=100;
static bool RangeReadable(const void* p,size_t n) {
    const uintptr_t a=(uintptr_t)p;
    for(auto& r:regions)if(a>=r.p && n<=r.size && a-r.p<=r.size-n)return true;
    return false;
}
static bool IsLiveObject(uint8_t* p){return live.count(p)!=0;}
static bool BuildLiveSet(){++builds;if(failBuild){live.clear();return false;}live=objects;return true;}
static bool RefreshLiveSet(uint32_t){return !failBuild;}
static void MkReadIdentity(void* p,Identity* id){
    *id={};if(!RangeReadable(p,kClassOff+4))return;
    auto* o=(uint8_t*)p;id->obj=p;id->cls=*(void**)(o+kClassOff);memcpy(id->name,o+kNameOff,8);
}
static const char* ObjClassName(uint8_t* p){return classes.count(p)?classes[p].c_str():nullptr;}
static const char* RealName(uint32_t n){return names.count(n)?names[n].c_str():"fixture";}
static bool MpFinite(float x){return std::isfinite(x);}
static bool WaReadCompXform(uint8_t* p,dvr::hf::Mat3* r,float* t,float* scale){
    if(!fixtureNative.count(p))return false;*r=fixtureNative[p].r;memcpy(t,fixtureNative[p].t,12);std::fill(scale,scale+3,1.0f);return true;
}
static void Log(const char*,...){}
#define DVR_LOG_FIRST_N(cat,level,count,...) Log(__VA_ARGS__)
namespace dvr { namespace frame { static uint32_t count(){return frames;} }
namespace fireaim {static bool normalize(float* v){float n=sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);if(!std::isfinite(n)||n<1e-8f)return false;for(int i=0;i<3;++i)v[i]/=n;return true;} }
namespace vr {static void input_set_aim_pitch_extra(int,float){} } }
static void CtRotToAxes(const int32_t* r,float* X,float* Y,float* Z){
    const float k=6.28318531f/65536;float p=r[0]*k,y=r[1]*k,z=r[2]*k;
    float sp=sinf(p),cp=cosf(p),sy=sinf(y),cy=cosf(y),sr=sinf(z),cr=cosf(z);
    X[0]=cp*cy;X[1]=cp*sy;X[2]=sp;Y[0]=sr*sp*cy-cr*sy;Y[1]=sr*sp*sy+cr*cy;Y[2]=-sr*cp;
    Z[0]=-(cr*sp*cy+sr*sy);Z[1]=cy*sr-cr*sp*sy;Z[2]=cr*cp;
}
static void CtAxesToRot(const float* X,const float* Y,const float* Z,int32_t* r){
    const float k=65536/6.28318531f;r[0]=(int32_t)(atan2f(X[2],sqrtf(X[0]*X[0]+X[1]*X[1]))*k);
    r[1]=(int32_t)(atan2f(X[1],X[0])*k);r[2]=(int32_t)(atan2f(-Y[2],Z[2])*k);
}
static Xform pose(float x=0,float y=0,float z=0,float yaw=0,float pitch=0,float roll=0,float scale=1){
    int32_t r[3]={(int32_t)(pitch*65536/360),(int32_t)(yaw*65536/360),(int32_t)(roll*65536/360)};
    float X[3],Y[3],Z[3];CtRotToAxes(r,X,Y,Z);Xform a{};a.t[0]=x;a.t[1]=y;a.t[2]=z;
    for(int i=0;i<3;++i){a.r.m[i*3]=X[i]*scale;a.r.m[i*3+1]=Y[i]*scale;a.r.m[i*3+2]=Z[i]*scale;}return a;
}
static Xform inverse(const Xform& a){Xform b{};if(!dvr::wf::inverse(a,&b))std::abort();return b;}
static Xform relpose(const float* t,const int32_t* r){return pose(t[0],t[1],t[2],r[1]*360.0f/65536,r[0]*360.0f/65536,r[2]*360.0f/65536);}
static float distance(const Xform& a,const Xform& b){float ds=0;for(int i=0;i<3;++i){float d=a.t[i]-b.t[i];ds+=d*d;}return sqrtf(ds);}
static bool samePosition(const Xform& a,const Xform& b,float tolerance=.025f){return distance(a,b)<tolerance;}
static bool sameRotation(const Xform& a,const Xform& b,float tolerance=.002f){for(int i=0;i<9;++i)if(fabsf(a.r.m[i]-b.r.m[i])>tolerance)return false;return true;}
static uint8_t* RainFindClassFunction(const char*,const char* name){return strstr(name,"From")?fromFn:toFn;}
static uint32_t RflOffsetOf(const char*,const char*){return 0;}
static bool RflNamesReady(){return true;}
static double MaimNowMs(){return nowMs;}
static bool g_mainMenu=false,g_inMenu=false,g_menuOpen=false,g_indexTuning=false,g_mpItemInHand[2]={},g_peReentry=false;
static uint8_t* g_pePawn;
static float g_viewYawRad=0,g_viewPitchRad=0;
static LONG g_mkLoadEvents=0;
static bool pawnLive=true;
static bool CylTruthLive(){return pawnLive;}
static bool CarryGameAnchor(float*){return false;}
constexpr int WA_MAX_COMP=8;
struct WaComp {uint8_t* obj=nullptr;Identity id;dvr::hf::Mat3 R{};float t[3]{},scale[3]{};bool ok=false,isRef=false,isMember=false;int hand=-1;};
struct WaCommon {Xform D{},L_hand{};uint32_t present=0;WaComp components[WA_MAX_COMP]{};int componentCount=0;bool ok=false;};
static WaCommon g_waCommon[2];
static WaComp g_waComp[WA_MAX_COMP];
static int g_waCompN=0;
static SRWLOCK g_waCompLock=SRWLOCK_INIT,g_waCommonLock=SRWLOCK_INIT;
static void FakePE(void*,void*,void*,void*);
using PFN_ProcessEventCall=void(*)(void*,void*,void*,void*);
#define kProcessEvent ((uintptr_t)&FakePE)
#include "game/dishonored/hands/fx_follow.cpp"
#undef kProcessEvent

static bool correction(int hand,const WaComp& parent,Xform* w,const char** why){
    WaCommon v;float travel=0;
    return FxReadPublished(hand,&v,why)&&FxWorldCorrection(v,parent,g_waComp,g_waCompN,w,&travel,why);
}
static void FakePE(void* obj,void* fn,void* args,void*){
    ++boneCalls;
    if(!g_peReentry)std::abort();
    if(recursive)FxFollowTick();
    const bool to=fn==toFn;
    if((to&&failTo)||(!to&&failFrom))return;
    auto& p=*(FxBoneParms*)args;
    Xform bone=xform_mul(fixtureNative[(uint8_t*)obj],bones[{(uint8_t*)obj,p.nameIdx}]);
    Xform result=xform_mul(to?inverse(bone):bone,relpose(p.inPos,p.inRot));
    memcpy(p.outPos,result.t,12);float X[3],Y[3],Z[3];
    for(int i=0;i<3;++i){X[i]=result.r.m[i*3];Y[i]=result.r.m[i*3+1];Z[i]=result.r.m[i*3+2];}
    CtAxesToRot(X,Y,Z,p.outRot);
    if(!to&&duringFrom){auto f=std::move(duringFrom);duringFrom={};f();}
}
struct Object {
    alignas(16) uint8_t bytes[0x400]{};
    Object(uint32_t name,const char* cls){
        regions.push_back({(uintptr_t)bytes,sizeof(bytes)});objects.insert(bytes);classes[bytes]=cls;
        *(uint32_t*)(bytes+kNameOff)=name;*(void**)(bytes+kClassOff)=bytes+0x380;
    }
    Identity id(){Identity i;MkReadIdentity(bytes,&i);return i;}
};
struct Attachment {uint8_t* comp;uint32_t bone[2];float t[3];int32_t r[3];float scale[3];};
static_assert(sizeof(Attachment)==0x30);
static constexpr uint32_t attOff=0x270; // fixture offset, resolved by the fake reflection setup
static void attach(Object& mesh,Attachment* a,int n){
    regions.push_back({(uintptr_t)a,size_t(n)*sizeof(*a)});
    *(Attachment**)(mesh.bytes+attOff)=a;*(int*)(mesh.bytes+attOff+4)=n;*(int*)(mesh.bytes+attOff+8)=n;
}
static WaComp record(Object& o,bool ref,int hand){WaComp c{};c.obj=o.bytes;c.id=o.id();c.ok=true;c.isRef=ref;c.isMember=!ref;c.hand=hand;auto n=fixtureNative[o.bytes];c.R=n.r;memcpy(c.t,n.t,12);return c;}
static int checks=0,failed=0;
static void check(const char* label,bool ok){++checks;if(!ok){++failed;printf("FAIL %s\n",label);}}
static Xform effect(Object& parent,const Attachment& a){return xform_mul(xform_mul(fixtureNative[parent.bytes],bones[{parent.bytes,a.bone[0]}]),relpose(a.t,a.r));}
static Xform legacy(const WaCommon& v){const auto& c=v.components[0];Xform n{c.R,{c.t[0],c.t[1],c.t[2]}},br{};dvr::wf::bridge(n,v.L_hand,&br);return xform_mul(xform_mul(inverse(br),v.D),br);}

int main(){
    Object pawn(1,"DishonoredPlayerPawn"),arm(2,"DishonoredPlayerSkeletalComponent"),heart(3,"DishonoredItemSkeletalComponent");
    Object glow(4,"DisParticleSystemComponent"),possession(5,"ParticleSystemComponent"),light(6,"PointLightComponent"),ignored(7,"StaticMeshComponent");
    Object fnFrom(8,"Function"),fnTo(9,"Function");fromFn=fnFrom.bytes;toFn=fnTo.bytes;g_pePawn=pawn.bytes;
    names[100]="Root_jnt";names[101]="handAttachment_L_jnt";names[102]="handAttachment_R_jnt";
    Attachment heartAtt[]{ {glow.bytes,{100,0},{8.5f,-.2f,-6.9f},{0,0,0},{1,1,1}} };
    Attachment armAtt[]{
        {possession.bytes,{101,0},{-1.6f,1.1f,-.2f},{0,0,0},{1,1,1}},
        {light.bytes,{102,0},{2,1,3},{0,0,0},{1,1,1}},
        {ignored.bytes,{101,0},{9,8,7},{0,0,0},{1,1,1}}};
    const auto heartOriginal=heartAtt[0],possOriginal=armAtt[0],lightOriginal=armAtt[1],ignoreOriginal=armAtt[2];
    attach(heart,heartAtt,1);attach(arm,armAtt,3);
    g_fxAttOff=attOff;BuildLiveSet();
    bones[{heart.bytes,100}]=pose(0,0,0,12);
    bones[{arm.bytes,101}]=pose(15,-12,8,-18,10);
    bones[{arm.bytes,102}]=pose(16,12,8,20,-10);
    const Xform member=pose(13,-10,7,25),draw=pose(1,2,3,-35,12,-5);
    const Xform local[2]={pose(4,8,-3,35,8,5),pose(-3,7,4,-30,-8,-5)};
    const Xform previous=pose(1000,-2500,300,20);
    fixtureNative[arm.bytes]=previous;fixtureNative[heart.bytes]=xform_mul(previous,member);
    g_waComp[0]=record(arm,true,-1);g_waComp[1]=record(heart,false,0);g_waCompN=2;
    for(int h=0;h<2;++h){auto& v=g_waCommon[h];v.ok=true;v.present=frames;v.L_hand=draw;v.D=xform_mul(xform_mul(draw,local[h]),inverse(draw));v.components[0]=g_waComp[0];v.components[1]=g_waComp[1];v.componentCount=2;}
    Xform W[2];bool have[2]={true,true};const char* why="";
    check("stationary correction available",correction(0,g_waComp[0],&W[0],&why));
    check("stationary matches old behavior",samePosition(W[0],legacy(g_waCommon[0]))&&sameRotation(W[0],legacy(g_waCommon[0])));
    // Translation alone, with fixed hand/head orientation, must not change the attachment relative.
    // The old control shares all inputs except its stale world origin, so it must expose that error.
    auto walking=previous;walking.t[0]+=6;walking.t[1]-=2;
    fixtureNative[arm.bytes]=walking;fixtureNative[heart.bytes]=xform_mul(walking,member);
    correction(0,g_waComp[0],&W[0],&why);
    const auto nativeGlow=xform_mul(xform_mul(fixtureNative[heart.bytes],bones[{heart.bytes,100}]),relpose(heartOriginal.t,heartOriginal.r));
    const auto walkingTarget=xform_mul(xform_mul(xform_mul(xform_mul(walking,local[0]),member),bones[{heart.bytes,100}]),relpose(heartOriginal.t,heartOriginal.r));
    const float walkingOldError=distance(xform_mul(legacy(g_waCommon[0]),nativeGlow),walkingTarget);
    check("straight-walking invariant",samePosition(xform_mul(W[0],nativeGlow),walkingTarget));
    check("straight-walking old control fails",walkingOldError>2);
    printf("straight walking: old control %.6f uu, corrected %.6f uu\n",walkingOldError,distance(xform_mul(W[0],nativeGlow),walkingTarget));
    // Arm has advanced, held mesh has not; the engine updates that mesh after our relative write.
    fixtureNative[heart.bytes]=xform_mul(previous,member);
    const auto oldHeartBone=xform_mul(fixtureNative[heart.bytes],bones[{heart.bytes,100}]);
    const auto wrongRelative=xform_mul(xform_mul(xform_mul(inverse(oldHeartBone),W[0]),oldHeartBone),relpose(heartOriginal.t,heartOriginal.r));
    Xform parentW[2];check("staggered parent correction available",correction(0,g_waComp[1],&parentW[0],&why));
    FxFollowMesh(g_waComp[1],parentW,have,nowMs);
    fixtureNative[heart.bytes]=xform_mul(walking,member);
    const float staggeredOld=distance(xform_mul(xform_mul(fixtureNative[heart.bytes],bones[{heart.bytes,100}]),wrongRelative),walkingTarget);
    check("later parent update preserves corrected effect",samePosition(effect(heart,heartAtt[0]),walkingTarget));
    check("live-arm-only control fails staggered update",staggeredOld>2);
    printf("staggered parent: arm-only control %.6f uu, parent-local correction %.6f uu\n",staggeredOld,distance(effect(heart,heartAtt[0]),walkingTarget));
    have[0]=false;FxFollowMesh(g_waComp[1],parentW,have,nowMs);have[0]=true;
    // Native scale and model scale stay separate; no scale is discarded by the frame conversion.
    for(float scale: {.8f,1.f,1.2f}){
        const Xform n=pose(-300,400,70,50,8,10,scale),l=pose(2,1,3,40,15,8,scale);
        const Xform d=pose(2,3,4,-20,3,-4,1.15f),point=pose(12,-7,5),drawD=xform_mul(xform_mul(l,d),inverse(l));
        Xform w{};check("scaled frame accepted",dvr::fx::world_correction(drawD,l,n,&w));
        check("scaled frame places socket",samePosition(xform_mul(w,xform_mul(n,point)),xform_mul(n,xform_mul(d,point))));
    }
    float worst=0,oldWorst=0;int samples=0;
    for(int angle=-150;angle<=150;angle+=30)for(int travel=-3;travel<=3;++travel){
        ++samples;fixtureNative[arm.bytes]=pose(1000+travel*11.f,-2500+travel*5.f,300+travel*2.f,(float)angle,5,-3);
        fixtureNative[heart.bytes]=xform_mul(fixtureNative[arm.bytes],member);
        for(int h=0;h<2;++h)check("current reference correction",correction(h,g_waComp[0],&W[h],&why));
        FxFollowMesh(g_waComp[0],W,have,nowMs);
        Xform heartW[2];check("held-parent correction",correction(0,g_waComp[1],&heartW[0],&why));
        FxFollowMesh(g_waComp[1],heartW,have,nowMs);
        const Xform wantHeart=xform_mul(xform_mul(xform_mul(xform_mul(fixtureNative[arm.bytes],local[0]),member),bones[{heart.bytes,100}]),relpose(heartOriginal.t,heartOriginal.r));
        const Xform wantPoss=xform_mul(xform_mul(xform_mul(fixtureNative[arm.bytes],local[0]),bones[{arm.bytes,101}]),relpose(possOriginal.t,possOriginal.r));
        const Xform wantLight=xform_mul(xform_mul(xform_mul(fixtureNative[arm.bytes],local[1]),bones[{arm.bytes,102}]),relpose(lightOriginal.t,lightOriginal.r));
        const Xform got=effect(heart,heartAtt[0]);worst=std::max(worst,distance(got,wantHeart));
        const Xform uncorrected=xform_mul(xform_mul(fixtureNative[heart.bytes],bones[{heart.bytes,100}]),relpose(heartOriginal.t,heartOriginal.r));
        oldWorst=std::max(oldWorst,distance(xform_mul(legacy(g_waCommon[0]),uncorrected),wantHeart));
        check("Heart follows locomotion and turns",samePosition(got,wantHeart));
        check("Heart rotation follows parent",sameRotation(got,wantHeart));
        check("Possession follows same correction",samePosition(effect(arm,armAtt[0]),wantPoss));
        check("separate light and right hand follow",samePosition(effect(arm,armAtt[1]),wantLight));
        check("unrelated attachment untouched",!memcmp(&armAtt[2],&ignoreOriginal,sizeof(Attachment)));
        FxFollowMesh(g_waComp[1],heartW,have,nowMs);
        check("repeated follow cannot feed back",samePosition(effect(heart,heartAtt[0]),wantHeart));
    }
    printf("locomotion: %d poses, corrected worst %.6f uu; old world-frame control %.3f uu\n",samples,worst,oldWorst);
    check("negative control reproduces displaced effect",oldWorst>10);
    have[0]=have[1]=false;FxFollowMesh(g_waComp[0],W,have,nowMs);FxFollowMesh(g_waComp[1],W,have,nowMs);
    check("no correction restores native Heart",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));
    check("no correction restores Possession and light",!memcmp(&armAtt[0],&possOriginal,sizeof(Attachment))&&!memcmp(&armAtt[1],&lightOriginal,sizeof(Attachment)));
    have[0]=have[1]=true;
    FxFollowMesh(g_waComp[1],W,have,nowMs);failFrom=true;FxFollowMesh(g_waComp[1],W,have,nowMs);failFrom=false;
    check("failed from-bone restores baseline",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));
    FxFollowMesh(g_waComp[1],W,have,nowMs);failTo=true;FxFollowMesh(g_waComp[1],W,have,nowMs);failTo=false;
    check("failed to-bone restores baseline",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));
    duringFrom=[&]{heartAtt[0].t[0]=19;};FxFollowMesh(g_waComp[1],W,have,nowMs);
    check("engine reattach during call wins",heartAtt[0].t[0]==19);heartAtt[0]=heartOriginal;g_fxN=0;
    Attachment moved=heartOriginal;
    duringFrom=[&]{attach(heart,&moved,1);};FxFollowMesh(g_waComp[1],W,have,nowMs);
    check("array relocation leaves old storage untouched",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));
    check("array relocation updates current element",memcmp(moved.t,heartOriginal.t,12)!=0);
    have[0]=false;FxFollowMesh(g_waComp[1],W,have,nowMs);have[0]=true;attach(heart,heartAtt,1);g_fxN=0;
    duringFrom=[&]{*(int*)(heart.bytes+attOff+4)=0;};FxFollowMesh(g_waComp[1],W,have,nowMs);
    check("detached component is not written",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));attach(heart,heartAtt,1);g_fxN=0;
    FxFollowMesh(g_waComp[1],W,have,nowMs);auto saved=heartAtt[0];objects.erase(glow.bytes);BuildLiveSet();FxRestore(g_fx[0]);
    check("dead object cannot be restored",!memcmp(&saved,&heartAtt[0],sizeof(saved)));objects.insert(glow.bytes);BuildLiveSet();
    *(uint32_t*)(glow.bytes+kNameOff+4)=1;FxRestore(g_fx[0]);check("reused address with new name cannot restore",!memcmp(&saved,&heartAtt[0],sizeof(saved)));
    heartAtt[0]=heartOriginal;heartAtt[0].t[0]=12;FxFollowMesh(g_waComp[1],W,have,nowMs);have[0]=false;FxFollowMesh(g_waComp[1],W,have,nowMs);have[0]=true;
    check("new effect gets new baseline",heartAtt[0].t[0]==12);heartAtt[0]=heartOriginal;g_fxN=0;
    *(uint32_t*)(arm.bytes+kNameOff+4)=1;
    check("reused reference identity refuses correction",!correction(0,g_waComp[0],&W[0],&why));*(uint32_t*)(arm.bytes+kNameOff+4)=0;
    FxFollowMesh(g_waComp[1],W,have,nowMs);saved=heartAtt[0];
    void* originalClass=*(void**)(glow.bytes+kClassOff);*(void**)(glow.bytes+kClassOff)=glow.bytes+0x390;
    FxRestore(*FxFind(glow.bytes));check("reused class cannot restore old effect",!memcmp(&saved,&heartAtt[0],sizeof(saved)));
    *(void**)(glow.bytes+kClassOff)=originalClass;FxRestore(*FxFind(glow.bytes));
    const auto regularW=W[0];W[0]=pose(1000,0,0);FxFollowMesh(g_waComp[1],W,have,nowMs);
    check("oversize correction preserves native relative",!memcmp(&heartAtt[0],&heartOriginal,sizeof(Attachment)));W[0]=regularW;
    frames+=5;check("old publication refused",!correction(0,g_waComp[0],&W[0],&why));
    for(auto& v:g_waCommon)v.present=frames;
    check("lifetime starts with fresh object table",FxPrepareLifetime());
    check("pre-transition publication refused",!correction(0,g_waComp[0],&W[0],&why));
    ++frames;for(auto& v:g_waCommon)v.present=frames;
    check("post-transition publication accepted",correction(0,g_waComp[0],&W[0],&why));
    const unsigned beforeMenu=builds;g_menuOpen=true;FxPrepareLifetime();g_menuOpen=false;FxPrepareLifetime();
    check("both menu boundaries refresh live table",builds==beforeMenu+2);
    failBuild=true;++g_mkLoadEvents;check("failed load revalidation refuses",!FxPrepareLifetime());failBuild=false;
    check("load can recover with fresh table",FxPrepareLifetime());
    check("load requires new draw even for same pawn pointer",!correction(0,g_waComp[0],&W[0],&why));
    ++frames;for(auto& v:g_waCommon)v.present=frames;
    boneCalls=0;recursive=true;FxFollowTick();recursive=false;
    check("nested ProcessEvent does not recurse",boneCalls>0&&boneCalls<=6);
    unsigned first=boneCalls;FxFollowTick();check("same-frame repeat does not double-drive",boneCalls==first);
    Xform bad=draw;bad.r={};check("singular draw refused",!dvr::fx::world_correction(local[0],bad,previous,&W[0]));
    bad=previous;bad.t[0]=NAN;check("nonfinite native frame refused",!dvr::fx::world_correction(local[0],draw,bad,&W[0]));
    printf("fx-follow: %d checks, %d failed\n",checks,failed);return failed?1:0;
}
