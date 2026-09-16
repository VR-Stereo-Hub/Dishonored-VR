// VR-107/VR-108, VR-74/VR-71: current UI ownership, not background scene activity.
// Read-only. Root identity is checked against its current GObjects slot; child
// identities are read afresh from the engine/player/world/UI-manager chain.
#include "core/vr/mono_anchor.h"
#include "game/dishonored/movie_completion.h"
namespace {
std::atomic<bool> g_usEnabled{false},g_usBlocked{true};
// VR-117: the owner on top rides the HUD window (the projection stays up).
std::atomic<bool> g_usRides{false};
std::atomic<bool> g_usWheelActive{false};
std::atomic<int> g_usActiveContext{-1};
std::atomic<unsigned> g_usContextEpoch{0};
dvr::ui_ride::RideLatch g_usRideLatch;
SRWLOCK g_usLock=SRWLOCK_INIT;
CtIdentity g_usEngine;
uint32_t g_usScan=0;
double g_usTickNext=0;
double g_usNext=0,g_usRefresh=0,g_usResolveAt=0;
bool g_usResolved=false;
uint32_t g_usPlayers,g_usActor,g_usWorld,g_usGame,g_usManager,g_usOverlay;
uint32_t g_usMode,g_usTransition,g_usMovie,g_usStarted,g_usStartedMask,g_usScreen,g_usOpen,g_usOpenMask;
uint32_t g_usHints=0,g_usHintsMask=0,g_usNote=0,g_usNoteMask=0,g_usWheel=0,g_usWheelMask=0;
uint32_t g_usMenus[10]={};
const char* g_usProps[]={"m_pMainMenu","m_pPauseMenu","m_pNote","m_pJournal","m_pPowerWheel","m_pStore","m_pMissionStats","m_pChallengeMenu","m_pBrief","m_pResultsMenu"};
const dvr::mono::Context g_usKinds[]={dvr::mono::MainMenu,dvr::mono::Pause,dvr::mono::Note,dvr::mono::Journal,dvr::mono::Wheel,dvr::mono::Store,dvr::mono::MissionStats,dvr::mono::Other,dvr::mono::Other,dvr::mono::Other};
dvr::mono::LoadingLease g_usLoading;
void UsPublish(dvr::mono::Context context,bool blocked,bool known,int screen,int movie,int mode) {
    // VR-117: may this owner RIDE the HUD window instead of forcing the mono
    // quad? Decided once per blocked interval (a health flap mid-menu must not
    // flip the picture), only for the contexts that opted in, only while the
    // redirect is up and drawing. The input class (UiSurfaceBlocks) does not
    // change: a riding menu still parks the head-mouse and the pad shaping.
    const bool windowOn=dvr::hudcap::enabled() &&
        dvr::hudlayout::screen_can_ride((int)context);   // VR-120: the screen's own row and anchor
    const bool want=dvr::ui_ride::rides(g_usEnabled.load(),blocked,context,dvr::hudlayout::menu_context_mask(),
                                        dvr::hudlayout::menu_in_window(),windowOn,dvr::hudcap::redirect_healthy());
    const bool rides=g_usRideLatch.update(context,blocked,want,dvr::hudcap::redirect_failed());
    const int inputContext=known && blocked ? (int)context : -1;
    if(g_usActiveContext.exchange(inputContext)!=inputContext) g_usContextEpoch.fetch_add(1);
    g_usWheelActive.store(known && blocked && context == dvr::mono::Wheel);
    g_usBlocked.store(blocked);
    g_usRides.store(rides);
    dvr::hudlayout::set_menu_riding(rides,(int)context);
    dvr::vr::set_mono_context(context,g_usEnabled.load() && blocked && !rides);
    static int last=-1;
    const int key=(int)context+32*blocked+64*known+128*rides;
    if(key!=last) {
        Log("ui/surface: context=%s blocked=%d known=%d rides=%d mainScreen=%d loadingMovie=%d saveLoadMode=%d lease=%d; background rendering cannot authorize stereo/input",
            dvr::mono::names[context],(int)blocked,(int)known,(int)rides,screen,movie,mode,(int)g_usLoading.active);
        if(blocked && dvr::ui_ride::context_can_ride(context)) {
            if(rides) Log("ui/ride: %s -> RIDING the HUD window (the world stays on the projection; input stays parked)",dvr::mono::names[context]);
            else Log("ui/ride: %s refused - menuInWindow=%d optIn=%d window=%d healthy=%d failed=%d guard=%d (the mono screen takes it)",
                     dvr::mono::names[context],(int)dvr::hudlayout::menu_in_window(),
                     (int)((dvr::hudlayout::menu_context_mask()>>(unsigned)context)&1u),(int)windowOn,
                     (int)dvr::hudcap::redirect_healthy(),(int)dvr::hudcap::redirect_failed(),(int)g_usEnabled.load());
        }
        last=key;
    }
}
// The native Pointer is a persistent, non-UObject movie service. Never call
// through it or retain it. Read the exact draw-gate field only for known code.
bool UsMoviePresent(uint8_t* overlay,void* movie,bool& presenting) {
    presenting=false;
    if(!movie) return true;
    uintptr_t vt=0,query=0;
    if(!RangeReadable(movie,sizeof(vt))) return false;
    memcpy(&vt,movie,sizeof(vt));
    if(vt!=kBinkServiceVtable && vt!=kNullMovieServiceVtable) return false;
    if(!RangeReadable((void*)(vt+kMoviePresentSlot),sizeof(query))) return false;
    memcpy(&query,(void*)(vt+kMoviePresentSlot),sizeof(query));
    if(vt==kBinkServiceVtable) {
        if(query!=kBinkPresentQuery || !RangeReadable((void*)query,sizeof(kBinkPresentQueryBytes)) ||
           memcmp((void*)query,kBinkPresentQueryBytes,sizeof(kBinkPresentQueryBytes))) return false;
        uint32_t active=0;
        auto* field=(uint8_t*)movie+kBinkPresentActive;
        if(!RangeReadable(field,sizeof(active))) return false;
        memcpy(&active,field,sizeof(active));
        if(active>1) return false;
        // Build252 proved the overlay-enabled flag stays set in gameplay.
        // Engine.WaitMovie waits for this manual-reset completion event instead.
        if(!RangeReadable((void*)kMovieEventCreate,sizeof(kMovieEventCreateBytes)) ||
           memcmp((void*)kMovieEventCreate,kMovieEventCreateBytes,sizeof(kMovieEventCreateBytes)) ||
           !RangeReadable((void*)kMovieEventWait,sizeof(kMovieEventWaitBytes)) ||
           memcmp((void*)kMovieEventWait,kMovieEventWaitBytes,sizeof(kMovieEventWaitBytes))) return false;
        uint8_t* event=nullptr;
        if(!RangeReadable((uint8_t*)movie+kMovieCompletionEvent,sizeof(event))) return false;
        memcpy(&event,(uint8_t*)movie+kMovieCompletionEvent,sizeof(event));
        if(!event || !RangeReadable(event,kMovieEventHandle+sizeof(HANDLE))) return false;
        uintptr_t eventVt=0,wait=0; HANDLE handle=nullptr;
        memcpy(&eventVt,event,sizeof(eventVt));
        if(eventVt!=kMovieEventVtable || !RangeReadable((void*)(eventVt+kMovieEventWaitSlot),sizeof(wait))) return false;
        memcpy(&wait,(void*)(eventVt+kMovieEventWaitSlot),sizeof(wait));
        if(wait!=kMovieEventWait) return false;
        memcpy(&handle,event+kMovieEventHandle,sizeof(handle));
        // Manual-reset means observing completion does not consume the signal.
        if(!dvr::movie::observe_completion(handle,presenting)) return false;
        uint8_t* eventAfter=nullptr; HANDLE handleAfter=nullptr;
        if(!RangeReadable((uint8_t*)movie+kMovieCompletionEvent,sizeof(eventAfter)) ||
           !RangeReadable(event,kMovieEventHandle+sizeof(handleAfter))) return false;
        memcpy(&eventAfter,(uint8_t*)movie+kMovieCompletionEvent,sizeof(eventAfter));
        memcpy(&handleAfter,event+kMovieEventHandle,sizeof(handleAfter));
        if(eventAfter!=event || handleAfter!=handle) return false;
        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,2000,
            "ui/movie-completion: overlayEnabled=%u finished=%d presenting=%d; zero-time manual-reset event observation",
            active,(int)!presenting,(int)presenting);
    } else if(query!=kNullMoviePresentQuery || !RangeReadable((void*)query,sizeof(kNullMoviePresentQueryBytes)) ||
              memcmp((void*)query,kNullMoviePresentQueryBytes,sizeof(kNullMoviePresentQueryBytes))) return false;
    void* current=nullptr; uintptr_t currentVt=0;
    if(!CtRead(overlay,g_usMovie,&current,sizeof(current)) || current!=movie ||
       !RangeReadable(movie,sizeof(currentVt))) return false;
    memcpy(&currentVt,movie,sizeof(currentVt));
    return currentVt==vt;
}
bool UsResolve() {
    if(g_usResolved) return true;
    const double now=MaimNowMs();
    if(now<g_usResolveAt || !RflNamesReady()) return false;
    g_usResolveAt=now+5000;
    struct Field { const char* cls; const char* prop; uint32_t* out; };
    const Field fields[]={
        {"Engine","GamePlayers",&g_usPlayers},{"Player","Actor",&g_usActor},
        {"Actor","WorldInfo",&g_usWorld},{"WorldInfo","Game",&g_usGame},
        {"DishonoredGameInfo","m_pGlobalUIManager",&g_usManager},
        {"DishonoredEngine","m_pBinkOverlayManager",&g_usOverlay},
        {"DishonoredEngine","m_SaveLoadMode",&g_usMode},{"Engine","TransitionType",&g_usTransition},
        {"DisBinkOverlayManager","m_pBinkMovie",&g_usMovie},
        {"DisGFxMoviePlayerMainMenu","m_Screen",&g_usScreen}};
    bool ok=true;
    for(const auto& f:fields) if(!*f.out && !FindPropOffsetChecked(f.cls,f.prop,f.out)) {
        ok=false; Log("ui/surface: missing property %s.%s",f.cls,f.prop);
    }
    if(!g_usNoteMask) ok=FindBoolProp("DisGFxMoviePlayerNote","m_bNoteVisible",&g_usNote,&g_usNoteMask) && ok;
    if(!g_usWheelMask) ok=FindBoolProp("DisGFxMoviePlayerPowerWheel","m_bWheelIsOpen",&g_usWheel,&g_usWheelMask) && ok;
    if(!g_usOpenMask) ok=FindBoolProp("GFxMoviePlayer","bMovieIsOpen",&g_usOpen,&g_usOpenMask) && ok;
    if(!g_usHintsMask) ok=FindBoolProp("DisBinkOverlayManager","m_bShowMapNameAndHints",&g_usHints,&g_usHintsMask) && ok;
    if(!g_usStartedMask) ok=FindBoolProp("DisBinkOverlayManager","m_bLoadingStarted",&g_usStarted,&g_usStartedMask) && ok;
    for(int i=0;i<10;++i) if(!g_usMenus[i]) {
        bool found=FindPropOffsetChecked("DisGlobalUIManager",g_usProps[i],&g_usMenus[i]);
        if(i<7) ok=found && ok; // DLC holder is optional.
    }
    g_usResolved=ok;
    Log("ui/surface: reflected root/movie layout %s; read-only, retry missing fields in 5 s",ok?"ready":"unavailable");
    return ok;
}
}
static bool UiSurfaceEnabled() { return g_usEnabled.load(); }
static unsigned UiSurfaceEpoch() { return g_usContextEpoch.load(); }
static int UiSurfaceContext() { return g_usEnabled.load() ? g_usActiveContext.load() : -1; }
static bool UiSurfaceHeadLook() { return g_usRides.load() && dvr::hudlayout::menu_head_look(UiSurfaceContext()); }
static bool UiSurfaceWheel() { return g_usEnabled.load() && g_usWheelActive.load(); }
static bool UiSurfaceBlocks() { return g_usEnabled.load() && g_usBlocked.load(); }
// VR-117: the presentation class. Blocked AND riding = the projection stays
// up with the screen on the HUD window; blocked and NOT riding = today's mono
// quad. Readers that decide what the HEADSET SHOWS use OwnsPresentation;
// readers that decide what the PLAYER MAY DO keep UiSurfaceBlocks.
static bool UiSurfaceRidesHud() { return g_usEnabled.load() && g_usBlocked.load() && g_usRides.load(); }
static bool UiSurfaceOwnsPresentation() { return UiSurfaceBlocks() && !g_usRides.load(); }
static void UiSurfaceSet(bool on) {
    g_usEnabled.store(on);
    if(!on) { g_usRides.store(false); dvr::hudlayout::set_menu_riding(false,-1); }
    dvr::vr::set_mono_context(dvr::mono::Other,on && g_usBlocked.load() && !g_usRides.load());
    Log("ui/surface: guard=%d (live)",(int)on);
}
static void UiSurfaceConfigure(const char* ini) {
    UiSurfaceSet(GetPrivateProfileIntA("Menu","SurfaceGuard",1,ini)!=0);
    uint32_t mask=0;
    for(unsigned i=0;i<dvr::mono::Count;++i) {
        char key[64]; _snprintf(key,sizeof(key),"Anchor%s",dvr::mono::names[i]);
        if(GetPrivateProfileIntA("Screen",key,1,ini)) mask|=1u<<i;
    }
    dvr::vr::set_mono_anchor(GetPrivateProfileIntA("Screen","AnchorMono",1,ini)!=0,mask);
}
static void UiSurfacePoll() {
    if((!g_usEnabled.load() && !dvr::vr::mono_anchor_enabled()) || !TryAcquireSRWLockExclusive(&g_usLock)) return;
    struct Unlock { ~Unlock(){ReleaseSRWLockExclusive(&g_usLock);} } unlock;
    const double now=MaimNowMs();
    if(now<g_usNext) return;
    g_usNext=now+50;
    if(!g_usResolved) { UsPublish(dvr::mono::Other,true,false,-1,-1,-1); return; }
    if(now>=g_usRefresh) { BuildLiveSet(); g_usRefresh=now+1000; }
    if(!ChSlot(g_usEngine)) { UsPublish(dvr::mono::Other,true,false,-1,-1,-1); return; }
    auto* engine=(uint8_t*)g_usEngine.value.obj;
    uint8_t mode=0,transition=0; uint32_t started=0,hints=0; void* movie=nullptr;
    auto* overlay=CtObject(engine,g_usOverlay);
    bool loadKnown=CtRead(engine,g_usMode,&mode,1) && CtRead(engine,g_usTransition,&transition,1) &&
        overlay && CtRead(overlay,g_usStarted,&started,4) && CtRead(overlay,g_usMovie,&movie,sizeof(movie)) && CtRead(overlay,g_usHints,&hints,4);
    bool presenting=false;
    loadKnown=loadKnown && UsMoviePresent(overlay,movie,presenting);
    const bool loading=g_usLoading.update(loadKnown,mode==2 || transition==2 || transition==4 || transition==5,
        (started&g_usStartedMask)!=0 || (hints&g_usHintsMask)!=0,presenting);
    DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,2000,
        "ui/loading: known=%d mode=%u transition=%u service=%p presenting=%d started=%d hints=%d lease=%d; service existence is not visibility",
        (int)loadKnown,(unsigned)mode,(unsigned)transition,movie,(int)presenting,
        (int)((started&g_usStartedMask)!=0),(int)((hints&g_usHintsMask)!=0),(int)loading);
    if(loading) { UsPublish(dvr::mono::Loading,true,loadKnown,-1,presenting?1:0,mode); return; }
    struct Array { uint8_t** data; int count,capacity; } players={};
    uint8_t* player=nullptr;
    if(CtRead(engine,g_usPlayers,&players,sizeof(players)) && players.count>0 && players.count<=4 &&
       players.capacity>=players.count && RangeReadable(players.data,sizeof(player))) memcpy(&player,players.data,sizeof(player));
    auto* pc=CtObject(player,g_usActor);
    auto* world=CtObject(pc,g_usWorld);
    auto* game=CtObject(world,g_usGame);
    auto* manager=CtObject(game,g_usManager);
    bool known=manager && loadKnown;
    if(!known) DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,5000,
        "ui/surface: refusing unknown chain engine=%p player=%p pc=%p world=%p game=%p manager=%p overlay=%p loadKnown=%d",
        engine,player,pc,world,game,manager,overlay,(int)loadKnown);
    int screen=-1;
    dvr::mono::Context context=dvr::mono::Other;
    bool blocked=false;
    for(int i=0;manager && i<10;++i) {
        if(!g_usMenus[i]) continue;
        uint8_t* obj=nullptr;
        if(!CtRead(manager,g_usMenus[i],&obj,sizeof(obj))) { known=false; continue; }
        if(!obj) continue;
        uint32_t bits=0;
        if(!CtRead(obj,g_usOpen,&bits,4)) { known=false; continue; }
        if(i==2 || i==4) {
            const uint32_t off=i==2?g_usNote:g_usWheel;
            const uint32_t mask=i==2?g_usNoteMask:g_usWheelMask;
            if(!CtRead(obj,off,&bits,4)) {known=false;continue;}
            if(!(bits&mask)) continue;
        } else if(!(bits&g_usOpenMask)) continue;
        if(i==0) {
            uint8_t value=0;
            if(!CtRead(obj,g_usScreen,&value,1)) { known=false; continue; }
            screen=value;
            if(value==0) continue;
            if(value>3) { known=false; continue; }
        }
        blocked=true; context=g_usKinds[i]; break;
    }
    if(!known && !blocked) { blocked=true; context=dvr::mono::Other; }
    if(!blocked && g_cineNow) context=dvr::mono::Cinematic;
    UsPublish(context,blocked,known,screen,presenting?1:0,mode);
}
static void UiSurfaceTick() {
    if((!g_usEnabled.load() && !dvr::vr::mono_anchor_enabled()) || !TryAcquireSRWLockExclusive(&g_usLock)) return;
    struct Unlock { ~Unlock(){ReleaseSRWLockExclusive(&g_usLock);} } unlock;
    const double now=MaimNowMs();
    if(now<g_usTickNext) return;
    g_usTickNext=now+16;
    if(!UsResolve() || ChSlot(g_usEngine) || !RangeReadable((void*)kGObjHdr,12)) return;
    auto** objects=*(uint8_t***)kGObjHdr;
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if(!objects || count>4000000) return;
    // Bounded discovery; never a full object scan every frame.
    for(unsigned budget=0;budget<1024 && count;++budget) {
        if(g_usScan>=count) {g_usScan=0; break;}
        const auto index=g_usScan++;
        if(!RangeReadable(objects+index,sizeof(void*))) break;
        auto* obj=objects[index];
        if(!IsLiveObject(obj)) continue;
        const char* cls=ObjClassName(obj);
        if(!cls || strcmp(cls,"DishonoredEngine")) continue;
        const char* name=RealName(*(uint32_t*)(obj+kNameOff));
        if(!name || strstr(name,"Default__")) continue;
        MkReadIdentity(obj,&g_usEngine.value); g_usEngine.index=index;
        Log("ui/surface: engine identity discovered in slot %u",index); break;
    }
}
