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
static void MenuEffectsTick() {
    if(GetCurrentThreadId()!=g_sdDrawTid) return; // engine game/draw lane only
    const int context=UiSurfaceContext();
    const bool want=UiSurfaceRidesHud() && dvr::hudlayout::menu_no_blur(context) && !g_gameExiting;
    auto* old=(uint8_t*)g_mbOwner.value.obj;
    if(g_mbHave && (!want || UiSurfaceEpoch()!=g_mbEpoch || context!=g_mbContext || !MbOwnerValid(old))) {
        float value=0;
        if(BuildLiveSet() && MbOwnerValid(old) && CtRead(old,g_mbWeight,&value,4) && value==0 && MbOwnerValid(old))
            memcpy(old+g_mbWeight,&g_mbBefore,4);
        g_mbHave=false;
    }
    if(!want) return;
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
