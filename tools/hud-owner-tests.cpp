#include "core/gfx/hud_owner_queue.h"
#include "core/gfx/hud_layout.h"
#include "game/dishonored/patterns.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <cstring>
using namespace dvr::hudowner;
static unsigned checks=0;
static void check(bool yes,const char* why) {++checks;if(!yes){printf("FAIL %s\n",why);exit(1);}}
static CommandOwners<> commands;
static std::atomic<uint32_t> generation{7},sent{0},received{0},overflow{0},displays{0};
static std::atomic<bool> requested{true},hooked{true};
static thread_local Owner sourceOwner,renderOwner;
static thread_local bool replaying=false;
static Owner mapped;
static Owner Lookup(void* p) {return (uintptr_t)p==mapped.root ? mapped : Owner{};}
static void* displayOriginal=nullptr;static void* publishOriginal=nullptr;
static uintptr_t executeReturn=0;
using namespace dvr::hudlayout;
struct Array {uint8_t** data;int count,capacity;};
static uint8_t hudStorage[128]{},targetStorage[32]{};
static uint8_t* hud=hudStorage;static bool targetLive=true,hudLive=true;
static uint32_t fields[6]={0,8,16,28,40,52};
static bool IsLiveObject(const void* p){return (p==hud && hudLive) || (p==targetStorage && targetLive);}
static bool RangeReadable(const void* p,size_t n) {
    MEMORY_BASIC_INFORMATION m{};const auto a=(uintptr_t)p;
    return a>=0x10000 && a+n>=a && VirtualQuery(p,&m,sizeof(m)) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
static bool CtRead(uint8_t* p,uint32_t off,void* out,size_t n) {
    if(!IsLiveObject(p) || !RangeReadable(p+off,n))return false;memcpy(out,p+off,n);return true;
}
#include "hud_owner_dispatch.inc"
static int phase=0;
static Owner observed;
static bool faultCommand=false;
static uint32_t __fastcall NativeCommand(void*,void*) {observed=renderOwner;if(faultCommand)RaiseException(0xe0000001,0,0,nullptr);return 28;}
static void __fastcall NativePublish(void* allocation,void*) {
    const auto command=((uintptr_t*)allocation)[1];
    check(Execute((void*)command,(void*)NativeCommand)==28,"queue consumer forwards native command size");
}
static void __fastcall NativeDisplay(void*,void*,void*) {
    if(phase==3) {RaiseException(0xe0000001,0,0,nullptr);return;}
    if(phase==0) {observed=sourceOwner;return;}
    if(phase==1) {
        phase=0;Display((void*)0x3330,nullptr,nullptr);
        check(observed.root==mapped.root,"unmapped child inherits exact native parent");
        return;
    }
    uintptr_t allocation[2]={kHudRenderQueue,0x5550};Publish(allocation,nullptr);
    check(observed.root==mapped.root,"metadata is available before native queue publication");
}
__declspec(naked) static void Continued() {__asm {pop esi} __asm {ret}}
__declspec(naked) static uint32_t RunSite(void*,void*) {
    __asm {
        push esi
        mov ecx,dword ptr [esp+8]
        mov edx,dword ptr [esp+12]
        jmp ExecuteStub
    }
}
static bool DisplayFault() {
    __try {Display((void*)mapped.root,nullptr,nullptr);}
    __except(EXCEPTION_EXECUTE_HANDLER){return true;}
    return false;
}
static bool ExecuteFault() {
    __try {Execute((void*)0x9990,(void*)NativeCommand);}
    __except(EXCEPTION_EXECUTE_HANDLER){return true;}
    return false;
}
int main() {
    // Real native member reader: a marker target differs from its containing HUD.
    uint8_t markerBytes[64]{},handle[16]{},character[16]{},clips[512]{};
    uint8_t* markerArray[1]={markerBytes};Array list{markerArray,1,1};
    uint8_t* clipPointer=clips;memcpy(hud+fields[1],&clipPointer,4);
    uint8_t* target=targetStorage;memcpy(markerBytes+kTaskMarkerOwner,&target,4);
    uint8_t* hp=handle;uint8_t* cp=character;uint32_t managedDisplay=0x48;
    memcpy(markerBytes+kMarkerGfxType,&managedDisplay,4);memcpy(markerBytes+kMarkerGfxHandle,&hp,4);
    memcpy(handle+kGfxResolvedCharacter,&cp,4);
    const uintptr_t tables[]={kTaskMarkerVtable,kHeartMarkerVtable,kAwarenessMarkerVtable};
    for(int family=1;family<=3;++family) {
        memcpy(hud+fields[family+1],&list,sizeof(list));memcpy(markerBytes,&tables[family-1],4);
        check(Character(Value(family,0))==(uintptr_t)character,"live task/collectible/enemy target distinct from HUD is recognized");
        targetLive=false;check(!Value(family,0),"dead target refused even while marker remains in HUD array");targetLive=true;
        hudLive=false;check(!Value(family,0),"dead containing HUD refused");hudLive=true;
        const uint32_t wrong=0;memcpy(markerBytes,&wrong,4);
        check(!Value(family,0),"wrong native marker family refused");
        check(!Value(family,1) && !Value(family,-1),"stale array index refused");
    }
    memcpy(markerBytes,&tables[0],4);markerArray[0]=nullptr;
    check(!Value(1,0),"withdrawn marker cannot retain ownership");markerArray[0]=markerBytes;
    memset(handle+kGfxResolvedCharacter,0,4);check(!Character(Value(1,0)),"unresolved GFx handles never invoke resolver");
    memcpy(handle+kGfxResolvedCharacter,&cp,4);
    check(Value(0,2)==clips+2*kHudValueSize && !Value(0,32) && !Value(0,-1),"clip table membership bounds");
    check(elements[2]==ElPrompt && elements[6]==ElPrompt,"talk/name/action stay on the accepted central prompt");
    check(elements[4]==ElDefault && elements[7]==elements[4] && elements[8]==elements[4] && elements[9]==elements[4],"context/mantle/QTE share sneak panel placement");
    check(elements[12]==ElReticle && elements[3]==ElVitals,"cooking and hand vitals retain separate semantic placement");
    Owner a;a.root=0x2000;a.generation=7;a.element=3;
    check(!commands.put(0,a),"null command refused");check(!commands.put(4,Owner{}),"unidentified owner refused");
    check(commands.put(16,a),"identified command queued");
    check(!commands.take(20,7),"neighboring command cannot inherit ownership");
    check(commands.take(16,7).root==a.root,"correct command restores exact owner");
    check(!commands.take(16,7),"replay retires ownership");
    check(commands.put(16,a),"same queue address can be reused after replay");
    check(!commands.take(16,8),"old generation refused and retired");
    check(commands.put(16,a),"old-generation retirement frees slot");
    check(!commands.put(16,a),"duplicate publication refused");
    check(!commands.take(16,7),"duplicate address cannot leak previous ownership");
    static CommandOwners<8> tinyQueue;
    for(unsigned i=1;i<=8;++i)check(tinyQueue.put(i*32,a),"bounded collision probe admits available slot");
    check(!tinyQueue.put(9*32,a),"full table refuses without overwriting");
    for(unsigned i=1;i<=8;++i)check(tinyQueue.take(i*32,7).root==a.root,"collision deletion preserves later records");
    displayOriginal=(void*)NativeDisplay;publishOriginal=(void*)NativePublish;mapped=a;
    phase=0;Display((void*)a.root,nullptr,nullptr);
    check(observed.root==a.root && !sourceOwner,"top level synchronous owner scope restores");
    phase=1;Display((void*)a.root,nullptr,nullptr);check(!sourceOwner,"nested display restores empty outer scope");
    phase=2;Display((void*)a.root,nullptr,nullptr);
    check(!renderOwner && !replaying,"queued replay cannot leak into the next draw");
    sourceOwner=a;uintptr_t allocation[2]={kHudRenderQueue+16,0x6660};Publish(allocation,nullptr);
    check(!observed,"other native queue forwards without HUD ownership");allocation[0]=kHudRenderQueue;sourceOwner={};
    check(commands.put(0x6660,a),"stale owned address prepared");
    Publish(allocation,nullptr);check(!observed && !commands.pending(),"unowned reuse retires previous widget before publication");
    requested=false;sourceOwner=a;check(commands.put(0x6660,a),"toggle-off pending record prepared");
    Publish(allocation,nullptr);check(!observed,"disabled routing cannot publish retained TLS ownership");
    requested=true;sourceOwner={};
    uintptr_t methods[2]={0,(uintptr_t)NativeCommand};executeReturn=(uintptr_t)Continued;
    check(commands.put(0x7770,a),"assembly test command tagged");
    check(RunSite((void*)0x7770,methods)==28 && observed.root==a.root,"actual x86 replay stub preserves thiscall argument and result");
    check(RunSite((void*)0x7780,methods)==28 && !observed,"actual x86 stub clears owner for unknown command");
    phase=3;check(DisplayFault() && !sourceOwner,"native display exception restores owner scope");phase=0;
    check(commands.put(0x9990,a),"exception replay command tagged");faultCommand=true;
    check(ExecuteFault() && !renderOwner && !replaying && !commands.pending(),"native replay exception retires metadata and restores TLS");faultCommand=false;
    static CommandOwners<> concurrent;
    std::atomic<bool> bad{false};constexpr unsigned count=100000;
    std::atomic<unsigned> acknowledged{0};
    std::thread producer([&]{for(unsigned i=1;i<=count;++i){
        while(acknowledged.load(std::memory_order_acquire)!=i-1)std::this_thread::yield();
        Owner o=a;o.root=i+16;if(!concurrent.put(0x10000,o))bad=true;
    }});
    for(unsigned i=1;i<=count;++i){Owner got;while(!(got=concurrent.take(0x10000,7)))std::this_thread::yield();
        if(got.root!=i+16)bad=true;acknowledged.store(i,std::memory_order_release);}
    producer.join();check(!bad,"100000 cross-thread address reuses preserve payload identity");
    LARGE_INTEGER f,t0,t1;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&t0);
    for(unsigned i=0;i<1000000;++i){commands.put(0x10000,a);commands.take(0x10000,7);}
    QueryPerformanceCounter(&t1);
    const double roundTrip=(t1.QuadPart-t0.QuadPart)*1e9/(double)f.QuadPart/1000000;
    QueryPerformanceCounter(&t0);
    for(unsigned i=0;i<1000000;++i) Execute((void*)0x10000,(void*)NativeCommand);
    QueryPerformanceCounter(&t1);
    printf("Unowned production replay wrapper %.2f ns (host, trivial native command)\n",
        (t1.QuadPart-t0.QuadPart)*1e9/(double)f.QuadPart/1000000);
    printf("%u semantic ownership checks passed; 100000 concurrent transfers; queue round trip %.2f ns (host)\n",checks,
        roundTrip);
}
