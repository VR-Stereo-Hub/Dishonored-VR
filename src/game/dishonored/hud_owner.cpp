// Widget identity remains numeric after publication. Engine reads stay on the
// native display/script lane; the render lane only consumes our copied payload.
#include "core/gfx/hud_owner.h"
#include "core/gfx/hud_owner_queue.h"
#include "core/gfx/hud_layout.h"
#include <algorithm>
namespace dvr::hudowner {
namespace {
std::atomic<bool> requested{false},hooked{false},available{false};
std::atomic<uint32_t> generation{1};
CommandOwners<> commands;
thread_local Owner sourceOwner,renderOwner;
thread_local bool replaying=false;
std::atomic<uint32_t> quickCaptured{0};
std::atomic<uint32_t> sent{0},received{0},overflow{0},taggedDraws{0},unknownDraws{0},displays{0};
SRWLOCK rootsLock=SRWLOCK_INIT;
struct Root {uintptr_t character=0;int family=0,index=0;Owner owner;};
Root roots[288]{};unsigned rootCount=0;
struct Point {uintptr_t root=0;float x=0,y=0,w=0,h=0;DWORD time=0;};
Point points[96]{};
uint8_t* hud=nullptr;dvr::menukeep::Identity hudIdentity;
uint8_t* quickManager=nullptr;uint8_t* quickWheel=nullptr;
dvr::menukeep::Identity quickIdentity;
uintptr_t quickView=0;
uint32_t quickWheelField=0,movieField=0,quickModeField=0;double quickResolveAfter=0;
uint32_t fields[6]{}; // manager HUD, clip table, four native marker arrays
LONG load=-1;unsigned epoch=~0u;DWORD refreshed=0;double resolveAfter=0;
dvr::hooks::Detour displayHook,publishHook,executeHook;
void* displayOriginal=nullptr;void* publishOriginal=nullptr;
uintptr_t executeReturn=kHudQueueExecuteReturn;
bool refused=false;
using namespace dvr::hudlayout;
// Native movie-clip enum order, verified against the initializer and Flash roots.
// Only crosshair info/use/talk share the central prompt. Context text, mantle
// icons and QTE share the player-state/default panel at their authored positions.
const int elements[32]={ElReticle,ElReticle,ElPrompt,ElVitals,ElDefault,ElVignette,
    ElPrompt,ElDefault,ElDefault,ElDefault,ElReticle,ElReticle,ElReticle,ElToast,ElToast,
    ElTutorial,ElTutorial,ElToast,ElToast,ElToast,ElVignette,ElVignette,ElVignette,
    ElVignette,ElVignette,ElDefault,ElDefault,ElDefault,ElDefault,ElSubtitles,ElDefault,ElSkipGauge};
struct Array {uint8_t** data;int count,capacity;};
uintptr_t Character(const uint8_t* value) {
    if(!value || !RangeReadable(value,kHudValueSize)) return 0;
    uint32_t type=0;uint8_t* handle=nullptr;
    memcpy(&type,value+kHudValueType,4);memcpy(&handle,value+kHudValueHandle,4);
    if((type&0x8f)!=8 || !handle || !RangeReadable(handle,kGfxResolvedCharacter+4)) return 0;
    uintptr_t result=0;memcpy(&result,handle+kGfxResolvedCharacter,4);return result;
}
bool SameHud() {
    if(load!=g_mkLoadEvents || epoch!=UiSurfaceEpoch() || !hud || !IsLiveObject(hud)) return false;
    dvr::menukeep::Identity now;MkReadIdentity(hud,&now);
    return now.obj==hudIdentity.obj && now.cls==hudIdentity.cls &&
        now.name[0]==hudIdentity.name[0] && now.name[1]==hudIdentity.name[1];
}
const uint8_t* Value(int family,int index) {
    if(family==0) {
        if(index<0 || index>=32) return nullptr;
        uint8_t* values=nullptr;
        return CtRead(hud,fields[1],&values,4) && values ? values+index*kHudValueSize : nullptr;
    }
    Array a{};
    if(family<1 || family>4 || !CtRead(hud,fields[family+1],&a,sizeof(a)) ||
        a.count<0 || a.count>256 || a.capacity<a.count || index>=a.count || index<0 ||
        !a.data || !RangeReadable(a.data+index,4)) return nullptr;
    uint8_t* marker=a.data[index];
    if(!marker || !RangeReadable(marker,kMarkerGfxHandle+4)) return nullptr;
    uint8_t* owner=nullptr;memcpy(&owner,marker+kTaskMarkerOwner,4);
    // +8 is the task/collectible/enemy UObject, not the owning HUD. Native
    // constructors BCE380/BCE750/BCEBD0 establish this relationship. The current
    // live HUD array establishes membership; independently validate its target.
    if(!IsLiveObject(owner)) return nullptr;
    uintptr_t table=0;memcpy(&table,marker,4);
    const uintptr_t expected=family==1?kTaskMarkerVtable:family==2?kHeartMarkerVtable:family==3?kAwarenessMarkerVtable:0;
    return (!expected || table==expected) ? marker+kMarkerGfxInterface : nullptr;
}
uintptr_t MovieView(uint8_t* object) {
    uint8_t* movie=nullptr;uintptr_t view=0;
    if(!movieField || !CtRead(object,movieField,&movie,4) || !movie ||
       !RangeReadable(movie+kGfxMovieView,4)) return 0;
    memcpy(&view,movie+kGfxMovieView,4);
    uintptr_t table=0;
    if(!view || !RangeReadable((void*)view,4)) return 0;
    memcpy(&table,(void*)view,4);
    return table==kGfxMovieRootVtable ? view : 0;
}
uintptr_t SpriteMovie(void* character) {
    // The current Display receiver is a borrowed GFxSprite. Its +BC member
    // is the movie root (constructor DF5240 and getter B27BE0), not +90's
    // resource definition. No VirtualQuery on every displayed child.
    uintptr_t view=0;
    __try {if(character) memcpy(&view,(uint8_t*)character+kGfxSpriteMovie,4);}
    __except(EXCEPTION_EXECUTE_HANDLER) {view=0;}
    return view;
}
int RefreshQuickMovie(uint8_t* manager) {
    quickView=0;quickManager=nullptr;quickWheel=nullptr;
    auto* wheel=quickWheelField ? CtObject(manager,quickWheelField) : nullptr;
    int mode=-1;
    if(wheel && quickModeField) CtRead(wheel,quickModeField,&mode,4);
    if(mode==kGfxQuickPotionMode) {
        const uintptr_t view=MovieView(wheel);
        if(view) {
            quickManager=manager;quickWheel=wheel;MkReadIdentity(wheel,&quickIdentity);
            quickView=view;
        }
    }
    return mode;
}
Owner QuickPotionOwner(void* character) {
    Owner result;
    if(!quickView || SpriteMovie(character)!=quickView || !SameHud()) return result;
    // A movie address is not liveness. Recheck the current manager member,
    // live wheel identity, mode and movie view before tagging native work.
    if(!quickManager || CtObject(quickManager,quickWheelField)!=quickWheel || !IsLiveObject(quickWheel)) return result;
    dvr::menukeep::Identity identity;MkReadIdentity(quickWheel,&identity);
    int mode=0;
    if(identity.obj!=quickIdentity.obj || identity.cls!=quickIdentity.cls ||
       identity.name[0]!=quickIdentity.name[0] || identity.name[1]!=quickIdentity.name[1] ||
       !CtRead(quickWheel,quickModeField,&mode,4) || mode!=kGfxQuickPotionMode || MovieView(quickWheel)!=quickView) return result;
    result.root=(uintptr_t)character;result.generation=generation.load(std::memory_order_relaxed);
    result.element=ElDefault;quickCaptured.fetch_add(1,std::memory_order_relaxed);return result;
}
Owner Lookup(void* character) {
    Owner result;
    if(!available.load(std::memory_order_relaxed)) return result;
    AcquireSRWLockShared(&rootsLock);
    if(GetTickCount()-refreshed<250) {
        const Root* found=std::lower_bound(roots,roots+rootCount,(uintptr_t)character,
            [](const Root& r,uintptr_t key){return r.character<key;});
        if(found!=roots+rootCount && found->character==(uintptr_t)character && found->owner) {
            const Root& r=*found;
            // Re-read membership from the live owner. An unchanged character
            // pointer alone cannot authorize identity across a menu or load.
            if(SameHud() && Character(Value(r.family,r.index))==r.character) {
                result=r.owner;
                if(result.marker) for(const auto& p:points) if(p.root==r.character && GetTickCount()-p.time<100 && p.w>0 && p.h>0) {
                    const float tw=(float)dvr::capture::width(),th=(float)dvr::capture::height();
                    if(tw>0 && th>0) {
                        const float scale=std::fmin(tw/p.w,th/p.h);
                        result.pivot[0]=.5f+(p.x-p.w*.5f)*scale/tw;
                        result.pivot[1]=.5f+(p.y-p.h*.5f)*scale/th;
                        result.pivotValid=true;
                    }
                    break;
                }
            }
        }
    }
    if(!result && !sourceOwner && GetTickCount()-refreshed<250) result=QuickPotionOwner(character);
    ReleaseSRWLockShared(&rootsLock);return result;
}
void __fastcall Display(void* self,void*,void* context) {
    const Owner previous=sourceOwner;
    if(requested.load(std::memory_order_relaxed) && hooked.load(std::memory_order_relaxed)) {
        const Owner owner=Lookup(self);
        if(owner) {sourceOwner=owner;displays.fetch_add(1,std::memory_order_relaxed);}
    }
    __try { ((void(__thiscall*)(void*,void*))displayOriginal)(self,context); }
    __finally {sourceOwner=previous;}
}
void __fastcall Publish(void* allocation,void*) {
    const bool owned=sourceOwner && requested.load(std::memory_order_relaxed);
    if(owned || commands.pending()) {
        uintptr_t queue=0,command=0;
        // Borrowed native allocation record, valid for this call. Avoid a
        // VirtualQuery per command; malformed reads cannot publish metadata.
        __try {memcpy(&queue,allocation,4);memcpy(&command,(uint8_t*)allocation+kHudQueueAllocationCommand,4);}
        __except(EXCEPTION_EXECUTE_HANDLER) {queue=0;command=0;}
        if(queue==kHudRenderQueue && command) {
            if(!owned) commands.take(command,0); // Retire stale ownership before unowned address reuse.
            else if(commands.put(command,sourceOwner)) sent.fetch_add(1,std::memory_order_relaxed);
            else overflow.fetch_add(1,std::memory_order_relaxed);
        }
    }
    ((void(__thiscall*)(void*))publishOriginal)(allocation);
}
uint32_t __cdecl Execute(void* command,void* method) {
    const Owner previous=renderOwner;const bool priorReplay=replaying;
    renderOwner=commands.take((uintptr_t)command,generation.load(std::memory_order_relaxed));replaying=true;
    if(renderOwner) received.fetch_add(1,std::memory_order_relaxed);
    uint32_t size=0;
    __try {size=((uint32_t(__thiscall*)(void*))method)(command);}
    __finally {renderOwner=previous;replaying=priorReplay;}
    return size;
}
__declspec(naked) void ExecuteStub() {
    __asm {
        mov esi,ecx
        push dword ptr [edx+4]
        push ecx
        call Execute
        add esp,8
        jmp dword ptr [executeReturn]
    }
}
bool Fingerprint(uintptr_t at,const uint8_t* bytes,size_t n) {
    return RangeReadable((void*)at,n) && !memcmp((void*)at,bytes,n);
}
void* Trampoline(uintptr_t at,const uint8_t* bytes,size_t n) {
    auto* code=(uint8_t*)VirtualAlloc(nullptr,n+5,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!code) return nullptr;
    memcpy(code,bytes,n);code[n]=0xe9;
    const int32_t jump=(int32_t)(at+n-(uintptr_t)(code+n+5));memcpy(code+n+1,&jump,4);
    DWORD old=0;if(!VirtualProtect(code,n+5,PAGE_EXECUTE_READ,&old)) {VirtualFree(code,0,MEM_RELEASE);return nullptr;}
    FlushInstructionCache(GetCurrentProcess(),code,n+5);return code;
}
void Install() {
    if(hooked.load() || refused || !requested.load()) return;
    refused=true;
    if(!RangeReadable((void*)kHudRenderThreadActive,4) || *(uint32_t*)kHudRenderThreadActive) {
        Log("hud/semantic: REFUSED late hook install; arm at startup before render thread starts");return;
    }
    if(!Fingerprint(kHudSpriteDisplay,kHudSpriteDisplayBytes,sizeof(kHudSpriteDisplayBytes)) ||
       !Fingerprint(kHudQueuePublish,kHudQueuePublishBytes,sizeof(kHudQueuePublishBytes)) ||
       !Fingerprint(kHudQueueExecuteSite,kHudQueueExecuteBytes,sizeof(kHudQueueExecuteBytes))) {
        Log("hud/semantic: REFUSED display/publication/replay fingerprint; prior routing retained");return;
    }
    displayOriginal=Trampoline(kHudSpriteDisplay,kHudSpriteDisplayBytes,sizeof(kHudSpriteDisplayBytes));
    publishOriginal=Trampoline(kHudQueuePublish,kHudQueuePublishBytes,sizeof(kHudQueuePublishBytes));
    if(!displayOriginal || !publishOriginal) {Log("hud/semantic: REFUSED trampoline allocation");return;}
    // Install the consumer first. Until all hooks exist, no source owner is sent.
    const bool ok=dvr::hooks::detour_install(executeHook,"hud/owner-replay",kHudQueueExecuteSite,kHudQueueExecuteBytes,sizeof(kHudQueueExecuteBytes),(void*)ExecuteStub) &&
        dvr::hooks::detour_install(publishHook,"hud/owner-publish",kHudQueuePublish,kHudQueuePublishBytes,sizeof(kHudQueuePublishBytes),(void*)Publish) &&
        dvr::hooks::detour_install(displayHook,"hud/owner-display",kHudSpriteDisplay,kHudSpriteDisplayBytes,sizeof(kHudSpriteDisplayBytes),(void*)Display);
    hooked.store(ok);
    Log("hud/semantic: hooks=%d; native child ownership copied before queue publication; no engine object writes",(int)ok);
}
}
void configure(bool on) {
    if(requested.exchange(on)!=on) generation.fetch_add(1);
    if(!on) available.store(false);
    else Install();
}
bool enabled(){return requested.load(std::memory_order_relaxed);}
bool active(){return enabled() && hooked.load(std::memory_order_relaxed) && available.load(std::memory_order_relaxed);}
Owner current(){const Owner o=replaying?renderOwner:sourceOwner;return active() && o.generation==generation.load(std::memory_order_relaxed) ? o : Owner{};}
void note_route(bool known){(known?taggedDraws:unknownDraws).fetch_add(1,std::memory_order_relaxed);}
// Called on the existing validated UI-manager poll, not from a D3D draw.
void poll(uint8_t* manager) {
    if(!enabled()) return;
    Install();if(!hooked.load()) return;
    const double now=MaimNowMs();
    if(!fields[0] || !fields[1] || !fields[2] || !fields[3] || !fields[4] || !fields[5]) {
        if(now<resolveAfter) return;resolveAfter=now+5000;
        const char* props[]={"m_pHUD","m_pMovieClips","m_TaskMarkers","m_HeartMarkers","m_AwarenessMarkers","m_GrenadeMarkers"};
        for(int i=0;i<6;++i) if(!fields[i]) FindPropOffsetChecked(i?"DisGFxMoviePlayerHUD":"DisGlobalUIManager",props[i],&fields[i]);
        if(!fields[0] || !fields[1] || !fields[2] || !fields[3] || !fields[4] || !fields[5]) {
            Log("hud/semantic: REFUSED unresolved HUD/clip/marker properties; retry in 5s");return;
        }
    }
    if((!quickWheelField || !movieField || !quickModeField) && now>=quickResolveAfter) {
        quickResolveAfter=now+5000;
        if(!quickWheelField) FindPropOffsetChecked("DisGlobalUIManager","m_pPowerWheel",&quickWheelField);
        if(!movieField) FindPropOffsetChecked("GFxMoviePlayer","pMovie",&movieField);
        if(!quickModeField) FindPropOffsetChecked("DisGFxMoviePlayerPowerWheel","m_Mode",&quickModeField);
    }
    AcquireSRWLockExclusive(&rootsLock);
    quickView=0;quickManager=nullptr;quickWheel=nullptr;
    if(load!=g_mkLoadEvents || epoch!=UiSurfaceEpoch()) {
        available.store(false);rootCount=0;memset(points,0,sizeof(points));
        if(!BuildLiveSet()) {ReleaseSRWLockExclusive(&rootsLock);return;}
        generation.fetch_add(1);load=g_mkLoadEvents;epoch=UiSurfaceEpoch();
    }
    hud=CtObject(manager,fields[0]);rootCount=0;
    if(hud) {
        MkReadIdentity(hud,&hudIdentity);
        for(int family=0;family<5;++family) {
            int count=32;
            if(family) {Array a{};count=CtRead(hud,fields[family+1],&a,sizeof(a)) && a.count>=0 && a.count<=256 && a.capacity>=a.count ? a.count : 0;}
            for(int i=0;i<count && rootCount<288;++i) {
                const uintptr_t root=Character(Value(family,i));if(!root) continue;
                uintptr_t table=0,display=0;
                if(!RangeReadable((void*)root,4)) continue;memcpy(&table,(void*)root,4);
                if(!table || !RangeReadable((void*)(table+kHudSpriteDisplaySlot),4)) continue;
                memcpy(&display,(void*)(table+kHudSpriteDisplaySlot),4);
                if(display!=kHudSpriteDisplay) continue; // Unsupported display subclasses stay native.
                Owner owner;owner.element=family ? (family==3?ElDetection:family==4?-1:ElObjective) : elements[i];
                owner.marker=family!=0;owner.root=root;owner.generation=generation.load();
                roots[rootCount++]={root,family,i,owner};
            }
        }
    }
    std::sort(roots,roots+rootCount,[](const Root& a,const Root& b){return a.character<b.character;});
    unsigned ambiguous=0;
    for(unsigned i=0;i<rootCount;) {
        unsigned end=i+1;while(end<rootCount && roots[end].character==roots[i].character) ++end;
        if(end-i>1) for(unsigned j=i;j<end;++j) {roots[j].owner={};++ambiguous;}
        i=end;
    }
    unsigned required=0;
    for(unsigned i=0;i<rootCount;++i) if(roots[i].family==0 && roots[i].owner) {
        const int index=roots[i].index;
        if(index==2) required|=1;if(index==6) required|=2;if(index==12) required|=4;
    }
    // Only the current potion movie can authorize its Display receivers.
    // Other movies' clips have no bearing on this ownership relationship.
    const int quickMode=RefreshQuickMovie(manager);
    refreshed=GetTickCount();available.store(required==7);
    static double reportAfter=0;
    const bool report=now>=reportAfter && ::dvr::log::enabled(DVR_CAT,::dvr::log::Level::Info);
    unsigned families[5]{},withPivot=0;
    if(report) {
        reportAfter=now+3000;
        for(unsigned i=0;i<rootCount;++i) if(roots[i].owner) ++families[roots[i].family];
        const DWORD tick=GetTickCount();
        for(const auto& p:points) if(p.root && tick-p.time<100) ++withPivot;
    }
    const unsigned count=rootCount;const bool quickReady=quickView!=0;ReleaseSRWLockExclusive(&rootsLock);
    if(report) DVR_LOG(DVR_CAT,::dvr::log::Level::Info,
        "hud/semantic: roots=%u active=%d required=%x ambiguous=%u clips/task/heart/aware/grenade=%u/%u/%u/%u/%u pivots=%u quickMode=%d quickReady=%d quickCaptured=%u display=%u queued=%u replayed=%u overflow=%u HUD-known=%u HUD-native-fallback=%u; cumulative, misses stay native",
        count,(int)available.load(),required,ambiguous,families[0],families[1],families[2],families[3],families[4],withPivot,quickMode,(int)quickReady,quickCaptured.load(),displays.load(),sent.load(),received.load(),overflow.load(),taggedDraws.load(),unknownDraws.load());
}
void marker(void* native,float x,float y,int w,int h) {
    if(!active() || w<=0 || h<=0 || !std::isfinite(x) || !std::isfinite(y)) return;
    const uintptr_t root=Character((uint8_t*)native+kMarkerGfxInterface);if(!root) return;
    AcquireSRWLockExclusive(&rootsLock);Point* slot=&points[0];const DWORD now=GetTickCount();
    for(auto& p:points){if(p.root==root){slot=&p;break;}if(!p.root || now-p.time>now-slot->time)slot=&p;}
    *slot={root,x,y,(float)w,(float)h,now};ReleaseSRWLockExclusive(&rootsLock);
}
}
