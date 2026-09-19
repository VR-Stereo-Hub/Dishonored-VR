// VR-126: render-only menu camera overlay. Included after cinematic camera helpers.
namespace {
CtIdentity g_mhOwner[3];
bool g_mhHave=false,g_mhScope=false;
LONG g_mhLoad=0;
int g_mhContext=-1;
unsigned g_mhEpoch=0;
dvr::cine::Matrix g_mhRef;
HtSample g_mhHead{};
float g_mhEntryCorrection[3]{},g_mhEntryYaw=0;
int32_t g_mhWritten[3]{},g_mhBase[3]{};
bool g_mhResume=false;double g_mhLastScopeMs=-1;
uint32_t g_mhWrites=0,g_mhRestores=0,g_mhRefused=0,g_mhSingles=0;
bool MhValidate(uint8_t* cam) {
    if(!g_mhHave || g_mhLoad!=g_mkLoadEvents || cam!=g_mhOwner[0].value.obj ||
       !ChSlot(g_mhOwner[0]) || !ChSlot(g_mhOwner[1]) || !ChSlot(g_mhOwner[2])) return false;
    auto* pc=(uint8_t*)g_mhOwner[1].value.obj;
    return pc==g_peCtrl && CtObject(pc,g_ctPcCamera)==cam && CtObject(pc,g_ctPawn)==g_mhOwner[2].value.obj;
}
}
static void MenuHeadPublish() {
    if(g_mhScope) HtPublishCameraRecord(3,g_mhHead,g_mhWritten[1]*360.f/65536,
        g_mhWritten[0]*360.f/65536,g_mhWritten[2]*360.f/65536);
}
static void MenuHeadBegin(bool scene,bool doubleDraw) {
    MenuEffectsTick();
    const int context=UiSurfaceContext();
    const bool enabled=UiSurfaceHeadLook() && g_trackingEnabled && g_rotInject && !g_mainMenu && !g_gameExiting;
    if(!enabled) {
        if(!g_mhResume || !dvr::hudlayout::menu_exit_heading() || UiSurfaceBlocks() ||
           MaimNowMs()<g_mhLastScopeMs || MaimNowMs()-g_mhLastScopeMs>1000) {
            g_mhResume=false;g_mhHave=false;g_mhContext=-1;
        }
        return;
    }
    const bool ready=scene &&
        !g_mainMenu && !g_gameExiting && dvr::vr::session_live() && dvr::stereo::wants_projection() &&
        !dvr::vr::cinematic_active() && !dvr::camera::eyetest_active() && !dvr::camera::postest_active();
    if(!ready) {
        DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,"menu/head: waiting context=%d scene=%d double=%d",context,scene,doubleDraw);
        return; // A temporary render gap cannot re-seed physical orientation.
    }
    HtSample head{}; const double now=MaimNowMs();
    if(!HtConsumeSample(&head) || !head.ok || !head.poseOk || now<head.locateMs || now-head.locateMs>100 || !std::isfinite(head.pitch+head.yaw+head.roll)) return;
    CineTraceTick(); if(!g_ctLayout) return;
    const auto h=dvr::cine::rotation(head.pitch*g_flipPitch,head.yaw*g_flipYaw,head.roll*g_flipRoll);
    if(UiSurfaceEpoch()!=g_mhEpoch || context!=g_mhContext || !MhValidate((uint8_t*)g_mhOwner[0].value.obj)) {
        g_mhHave=false;
        if(!BuildLiveSet()) return;
        auto* pc=IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
        auto* cam=CtObject(pc,g_ctPcCamera);auto* pawn=CtObject(pc,g_ctPawn);
        if(!cam || cam!=g_camObj || !pawn || !ChCapture(cam,&g_mhOwner[0]) ||
           !ChCapture(pc,&g_mhOwner[1]) || !ChCapture(pawn,&g_mhOwner[2])) return;
        g_mhHave=true;g_mhLoad=g_mkLoadEvents;g_mhContext=context;g_mhEpoch=UiSurfaceEpoch();g_mhRef=h;
        g_mhEntryYaw=head.yaw;
        for(int i=0;i<3;++i) g_mhEntryCorrection[i]=head.position[i]-head.rawPosition[i];
        Log("menu/head: context=%d acquired current camera/controller/pawn; render-only head look",context);
    }
    auto* cam=(uint8_t*)g_mhOwner[0].value.obj;
    int32_t base[3]{};dvr::cine::Matrix composed;
    if(!CtRead(cam,g_ctCache+g_ctPov+g_ctRot,base,12) ||
       !dvr::cine::compose(base,g_mhRef,h,g_mhWritten,&composed)) return;
    memcpy(g_mhBase,base,sizeof(base));
    const float right[3]={(float)composed.m[0][1],(float)composed.m[1][1],(float)composed.m[2][1]};
    float pos[3];
    dvr::position_math::reframe_yaw(g_mhEntryCorrection,g_mhEntryYaw,head.yaw,pos);
    for(int i=0;i<3;++i) pos[i]+=head.rawPosition[i];
    g_mhScope=dvr::camera::begin_view_scope(cam,g_ctCache+g_ctPov+g_ctRot,g_mhWritten,right,doubleDraw?-1:0,MhValidate,false,pos);
    if(g_mhScope) { ++g_mhWrites;if(!doubleDraw) ++g_mhSingles;g_mhHead=head;MenuHeadPublish(); } else ++g_mhRefused;
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,500,
        "menu/head: context=%d scope=%d base=%d/%d/%d out=%d/%d/%d gen=%u writes=%u restores=%u refused=%u double=%d singles=%u pos=%.3f/%.3f/%.3f",
        context,g_mhScope,base[0],base[1],base[2],g_mhWritten[0],g_mhWritten[1],g_mhWritten[2],head.gen,g_mhWrites,g_mhRestores,g_mhRefused,(int)doubleDraw,g_mhSingles,pos[0],pos[1],pos[2]);
}
static void MenuHeadEnd() {
    if(!g_mhScope) return;
    if(dvr::camera::end_view_scope()) {++g_mhRestores;g_mhResume=true;g_mhLastScopeMs=MaimNowMs();}
    else { ++g_mhRefused;g_mhHave=false;g_mhResume=false;Log("menu/head: restore refused: identity or engine field changed"); }
    g_mhScope=false;
}


// Hand the menu's physical turn to the existing gameplay rotation writer once.
// No engine writes here. Rebuild and revalidate retained owners even if addresses
// are unchanged; loads, authored cameras and stale head poses refuse the handoff.
static bool MenuHeadResumeYaw(int32_t& delta) {
    delta=0;
    if(!g_mhResume || UiSurfaceBlocks() || dvr::camera::second_pass_for_current_thread()) return false;
    if(GetCurrentThreadId()!=g_sdDrawTid) return false;
    g_mhResume=false;
    const double now=MaimNowMs();HtSample head{};
    const bool valid=dvr::hudlayout::menu_exit_heading() && g_trackingEnabled && g_rotInject &&
        !g_mainMenu && !g_gameExiting && !CineActive() && dvr::vr::session_live() &&
        now>=g_mhLastScopeMs && now-g_mhLastScopeMs<=1000 &&
        BuildLiveSet() && MhValidate((uint8_t*)g_mhOwner[0].value.obj) &&
        HtConsumeSample(&head) && head.ok && head.poseOk && now>=head.locateMs && now-head.locateMs<=100;
    int32_t desired[3]{};
    const bool composed=valid && dvr::cine::compose(g_mhBase,g_mhRef,
        dvr::cine::rotation(head.pitch*g_flipPitch,head.yaw*g_flipYaw,head.roll*g_flipRoll),desired,nullptr);
    g_mhHave=false;g_mhContext=-1;
    if(!composed) {Log("menu/exit: yaw handoff refused: option, ownership, context or pose changed");return false;}
    delta=(int32_t)std::remainder((double)desired[1]-g_mhBase[1],65536.0);
    Log("menu/exit: carry yaw %.3f deg once into gameplay; owner revalidated, head gen=%u",delta*360.f/65536,head.gen);
    return true;
}

// The game owns all effect timelines. Suppress only its UI blend while selected;
// retain the last nonzero game value and restore only our exact zero on exit.
namespace {
CtIdentity g_mbOwner;
bool g_mbHave=false;
LONG g_mbLoad=0;
int g_mbContext=-1;
unsigned g_mbEpoch=0;
float g_mbBefore=0;
uint32_t g_mbWorld=0,g_mbGame=0,g_mbManager=0,g_mbWeight=0,g_mbWrites=0;
double g_mbResolveAt=0;
bool MbOwnerValid(uint8_t* manager) {
    if(!g_mbHave || g_mbLoad!=g_mkLoadEvents || manager!=g_mbOwner.value.obj || !ChSlot(g_mbOwner)) return false;
    auto* world=CtObject(IsLiveObject(g_peCtrl)?g_peCtrl:nullptr,g_mbWorld);
    return CtObject(CtObject(world,g_mbGame),g_mbManager)==manager;
}
}
// VR-140 instrument, read-only. The black world is drawn and post-processed to
// black at the backbuffer ~400 ms after a re-opened wheel movie finally closes,
// and the UI weight read 0 throughout (run470). This logs the game's whole
// post-process state machine and the camera's fade/colour-scale fields: every
// change of an effect's state or request count, and a full snapshot when the
// frameid judge sees THE EYES BECOME ONE PICTURE (then every 3 s while it holds).
// It can fail its hypothesis: a black run whose snapshot shows every effect
// Stopped, no request held, FadeAmount 0 and ColorScale 1 clears the game's
// post-process and camera fade, and points at our own draws.
namespace {
enum PwField { PwReq,PwState,PwUiDur,PwUiOut,PwUiIn,PwUiW,PwKsW,PwKsDur,PwBend,PwKo,PwFadeAmt,PwFadeColor,
               PwCamPpA,PwColorScale,PwFadeAlpha,PwFadeLeft,PwPpTargets,PwCount };
struct PwDef { const char* cls; const char* prop; };
const PwDef kPwDefs[PwCount]={
    {"DisPostProcessManager","m_RequiredEffects"},{"DisPostProcessManager","m_EffectStates"},
    {"DisPostProcessManager","m_UIStateDuration"},{"DisPostProcessManager","m_UIPPFadeOutTime"},
    {"DisPostProcessManager","m_UIPPFadeInTime"},{"DisPostProcessManager","m_UIPPWeight"},
    {"DisPostProcessManager","m_KismetPPWeight"},{"DisPostProcessManager","m_KismetStateDuration"},
    {"DisPostProcessManager","m_BendTimeIntensity"},{"DisPostProcessManager","m_KnockOutTimer"},
    {"Camera","FadeAmount"},{"Camera","FadeColor"},{"Camera","CamOverridePostProcessAlpha"},
    {"Camera","ColorScale"},{"Camera","FadeAlpha"},{"Camera","FadeTimeRemaining"},
    {"DishonoredPlayerCamera","m_PostProcessTargets"}};
constexpr int kPwEffects=23; // eEffectPp through Epp_MAX
uint32_t g_pwOff[PwCount]{},g_pwFadeOff=0,g_pwFadeMask=0,g_pwScaleOff=0,g_pwScaleMask=0;
int g_pwResolved=0; bool g_pwBoolsDone=false,g_pwOne=false,g_pwHaveLast=false;
double g_pwNext=0,g_pwOneNext=0;
int32_t g_pwReq[kPwEffects]{}; uint8_t g_pwState[kPwEffects]{};
}
static void PpWatchSnapshot(const char* why,uint8_t* manager,uint8_t* cam) {
    int32_t req[kPwEffects]{}; uint8_t st[kPwEffects]{};
    const bool mOk=manager && CtRead(manager,g_pwOff[PwReq],req,sizeof(req)) && CtRead(manager,g_pwOff[PwState],st,sizeof(st));
    char effects[23*16]="none"; int n=0;
    for(int i=0;mOk && i<kPwEffects;++i) if(req[i] || st[i])
        n+=snprintf(effects+n,sizeof(effects)-n,"%s%d:r%d/s%d",n?" ":"",i,req[i],st[i]);
    auto f=[](uint8_t* o,uint32_t off){float v=-999;if(o&&off)CtRead(o,off,&v,4);return v;};
    float scale[3]{-999,-999,-999},alpha[2]{-999,-999}; uint8_t color[4]{}; uint32_t bits[2]{}; int32_t targets[2]{-1,-1};
    if(cam) { CtRead(cam,g_pwOff[PwColorScale],scale,12);CtRead(cam,g_pwOff[PwFadeAlpha],alpha,8);
              CtRead(cam,g_pwOff[PwFadeColor],color,4);CtRead(cam,g_pwOff[PwPpTargets],targets,8);
              if(g_pwFadeOff)CtRead(cam,g_pwFadeOff,&bits[0],4); if(g_pwScaleOff)CtRead(cam,g_pwScaleOff,&bits[1],4); }
    Log("pp/watch: %s | manager=%p effects(index:required/state, 0 stop 1 warm 2 run 3 cool 4 abort)=%s | "
        "ui dur=%.3f out=%.3f in=%.3f weight=%.3f | kismet weight=%.3f dur=%.3f | bend=%.3f ko=%.3f",
        why,manager,mOk?effects:"unreadable",f(manager,g_pwOff[PwUiDur]),f(manager,g_pwOff[PwUiOut]),f(manager,g_pwOff[PwUiIn]),
        f(manager,g_pwOff[PwUiW]),f(manager,g_pwOff[PwKsW]),f(manager,g_pwOff[PwKsDur]),f(manager,g_pwOff[PwBend]),f(manager,g_pwOff[PwKo]));
    Log("pp/watch: %s | camera=%p fading=%d amount=%.3f color=%u,%u,%u,%u alpha=%.3f/%.3f left=%.3f | colorScaling=%d scale=%.3f/%.3f/%.3f | "
        "camPpAlpha=%.3f ppTargets=%d (-999/-1 = unreadable; a black world under the game's own fade reads fading=1 amount>0 or scale near 0)",
        why,cam,g_pwFadeMask?(bits[0]&g_pwFadeMask)!=0:-1,f(cam,g_pwOff[PwFadeAmt]),color[2],color[1],color[0],color[3],alpha[0],alpha[1],
        f(cam,g_pwOff[PwFadeLeft]),g_pwScaleMask?(bits[1]&g_pwScaleMask)!=0:-1,scale[0],scale[1],scale[2],f(cam,g_pwOff[PwCamPpA]),targets[1]);
}
static void PpWatchTick() {
    const double now=MaimNowMs();
    if(now<g_pwNext) return;
    g_pwNext=now+100;
    if(g_pwResolved<PwCount) { // one GObjects walk per tick, never a burst
        if(!FindPropOffsetChecked(kPwDefs[g_pwResolved].cls,kPwDefs[g_pwResolved].prop,&g_pwOff[g_pwResolved])) {
            Log("pp/watch: %s.%s not reflected - instrument off (no write either way)",kPwDefs[g_pwResolved].cls,kPwDefs[g_pwResolved].prop);
            g_pwResolved=PwCount+1; return;
        }
        if(++g_pwResolved==PwCount) Log("pp/watch: armed (read-only; states/required +%x/+%x, camera FadeAmount +%x)",
                                        g_pwOff[PwReq],g_pwOff[PwState],g_pwOff[PwFadeAmt]);
        return;
    }
    if(g_pwResolved!=PwCount || !g_mbWorld || !g_mbGame || !g_mbManager) return;
    if(!g_pwBoolsDone) { g_pwBoolsDone=true; FindBoolProp("Camera","bEnableFading",&g_pwFadeOff,&g_pwFadeMask);
                         FindBoolProp("Camera","bEnableColorScaling",&g_pwScaleOff,&g_pwScaleMask); return; }
    auto* world=CtObject(IsLiveObject(g_peCtrl)?g_peCtrl:nullptr,g_mbWorld);
    auto* manager=CtObject(CtObject(world,g_mbGame),g_mbManager);
    auto* cam=CtObject(IsLiveObject(g_peCtrl)?g_peCtrl:nullptr,g_ctPcCamera);
    int32_t req[kPwEffects]{}; uint8_t st[kPwEffects]{};
    if(manager && CtRead(manager,g_pwOff[PwReq],req,sizeof(req)) && CtRead(manager,g_pwOff[PwState],st,sizeof(st))) {
        if(!g_pwHaveLast || memcmp(req,g_pwReq,sizeof(req)) || memcmp(st,g_pwState,sizeof(st))) {
            char why[96]; snprintf(why,sizeof(why),"CHANGED (menu=%d ride=%d context=%d)",(int)(g_menuOpen||g_inMenu),
                                   (int)UiSurfaceRidesHud(),UiSurfaceContext());
            PpWatchSnapshot(why,manager,cam);
            memcpy(g_pwReq,req,sizeof(req));memcpy(g_pwState,st,sizeof(st));g_pwHaveLast=true;
        }
    }
    const bool one=dvr::frameid::last().onePicture;
    if(one && (!g_pwOne || now>=g_pwOneNext)) { PpWatchSnapshot(g_pwOne?"ONE PICTURE still":"AT ONE PICTURE",manager,cam); g_pwOneNext=now+3000; }
    else if(!one && g_pwOne) PpWatchSnapshot("TWO PICTURES again",manager,cam);
    g_pwOne=one;
}
static void MenuEffectsTick() {
    if(GetCurrentThreadId()!=g_sdDrawTid) return; // engine game/draw lane only
    PpWatchTick();
    const int context=UiSurfaceContext();
    const bool want=UiSurfaceRidesHud() && dvr::hudlayout::menu_no_blur(context) && !g_gameExiting;
    auto* old=(uint8_t*)g_mbOwner.value.obj;
    // 2026-09-18 blackout: the exit used to write back the game's last nonzero
    // weight (1.0 for the wheel). A wheel opened and closed within 62 ms of a
    // previous close (run467, 4001703..4001765) left that restore landing after
    // the game had finished its own fade, so the full menu post-process stayed
    // on the world: black, with the HUD markers, Dark Vision silhouettes and the
    // pause menu still drawing over it. The game drives this weight itself on
    // every open and close, so the exit now writes NOTHING: the worst case is a
    // skipped fade, never a stuck one.
    if(g_mbHave && (!want || UiSurfaceEpoch()!=g_mbEpoch || context!=g_mbContext || !MbOwnerValid(old))) {
        float value=-1;
        if(MbOwnerValid(old)) CtRead(old,g_mbWeight,&value,4);
        Log("menu/blur: released context=%d (weight now %.3f, game value before %.3f) - no restore write; the game owns its fade",
            g_mbContext,value,g_mbBefore);
        g_mbHave=false;
    }
    if(!want) {
        // Read-only watchdog: in gameplay the UI weight belongs at 0. A value
        // held high with no menu is the blackout's signature, from any cause.
        static double next=0,highSince=0; static bool warned=false;
        const double now=MaimNowMs();
        if(!g_mbWeight || !g_mbWorld || !g_mbGame || !g_mbManager || now<next) return;
        next=now+500;
        if(UiSurfaceBlocks() || UiSurfaceRidesHud() || g_menuOpen || g_inMenu || g_mainMenu) { highSince=0; warned=false; return; }
        auto* world=CtObject(IsLiveObject(g_peCtrl)?g_peCtrl:nullptr,g_mbWorld);
        auto* manager=CtObject(CtObject(world,g_mbGame),g_mbManager);
        float value=0;
        if(!manager || !CtRead(manager,g_mbWeight,&value,4)) return;
        if(!(value<=0.5f)) { // NaN too: a non-finite weight is as stuck as a high one
            if(!highSince) highSince=now;
            if(!warned && now-highSince>1500) {
                warned=true;
                DVR_WARN("menu/blur: UI post-process weight %.3f held for %.1f s in GAMEPLAY with no menu - the world is under the "
                         "menu effect (the black-screen signature). Read-only: nothing written.",value,(now-highSince)/1000.0);
            }
        } else { highSince=0; warned=false; }
        return;
    }
    if(!g_mbWeight || !g_mbWorld || !g_mbGame || !g_mbManager) {
        if(MaimNowMs()<g_mbResolveAt) return;
        g_mbResolveAt=MaimNowMs()+5000;
        if(!FindPropOffsetChecked("Actor","WorldInfo",&g_mbWorld) ||
           !FindPropOffsetChecked("WorldInfo","Game",&g_mbGame) ||
           !FindPropOffsetChecked("DishonoredGameInfo","m_pPpManager",&g_mbManager) ||
           !FindPropOffsetChecked("DisPostProcessManager","m_UIPPWeight",&g_mbWeight)) {
            Log("menu/blur: UI blend reflection unavailable; no write, retry in5s");return;
        }
        Log("menu/blur: reflected UI-only blend weight +%x; other effect weights untouched",g_mbWeight);
    }
    if(!g_mbHave) {
        if(!BuildLiveSet()) return;
        auto* world=CtObject(IsLiveObject(g_peCtrl)?g_peCtrl:nullptr,g_mbWorld);
        auto* manager=CtObject(CtObject(world,g_mbGame),g_mbManager);
        float before=0;
        if(!manager || !ChCapture(manager,&g_mbOwner) || !CtRead(manager,g_mbWeight,&before,4) ||
           !std::isfinite(before) || before<0 || before>1) return;
        g_mbHave=true;g_mbLoad=g_mkLoadEvents;g_mbContext=context;g_mbEpoch=UiSurfaceEpoch();g_mbBefore=before;
    }
    auto* manager=(uint8_t*)g_mbOwner.value.obj;
    float value=0;
    if(!MbOwnerValid(manager) || !CtRead(manager,g_mbWeight,&value,4) || !std::isfinite(value) || value<0 || value>1) return;
    if(value!=0 && MbOwnerValid(manager)) { g_mbBefore=value;const float zero=0;memcpy(manager+g_mbWeight,&zero,4);++g_mbWrites; }
    DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,
        "menu/blur: context=%d observed=%.3f suppressed UI weight; writes=%u retained=%.3f; visual acceptance pending",
        context,value,g_mbWrites,g_mbBefore);
}
