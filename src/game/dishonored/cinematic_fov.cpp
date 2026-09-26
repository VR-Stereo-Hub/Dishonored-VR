// VR-50: scoped final camera FOV for authored scenes and gameplay; after cinematic_trace.cpp.
#include "game/dishonored/cinematic_fov_policy.h"
#include "game/dishonored/stereo_state_policy.h"
namespace {
std::atomic<bool> g_cineFov{false};
std::atomic<float> g_projectionFov{103.0f}; // Explicit user-requested default; 0 restores headset-derived FOV.
// 2026-09-25: authored scenes frame with the same [Screen] ProjectionFov as gameplay. Since
// VR-227 a locked scene is held at the headset-derived FOV (108 on the dev rig) while
// gameplay renders at ProjectionFov (103), so every cutscene is framed differently from the
// play around it. [Cine] MatchGameplayFov=0 restores the headset-derived scene FOV.
std::atomic<bool> g_cineMatch{true};
bool g_cfGameplayScope=false; // Script/draw lane only; the render lane uses CfPublish.
CtIdentity g_cfOwner[3];
bool g_cfHaveOwner=false;
LONG g_cfLoad=0;
unsigned g_cfEpoch=0; // VR-228: revalidate retained camera ownership across menus.
uint32_t g_cfOffset=0,g_cfWrites=0,g_cfRestores=0,g_cfRefused=0;
dvr::cine_fov::Scope g_cfScope;
dvr::cine_fov::ExitBridge g_cfBridge;
double g_cfRetry=0,g_cfLog=0,g_cfResolveAfter=0;
const char* g_cfReason="startup";
SRWLOCK g_cfLock=SRWLOCK_INIT;
float g_cfClaim=0;
unsigned long long g_cfStamp=0;
void CfPublish(float fov) {
    AcquireSRWLockExclusive(&g_cfLock); g_cfClaim=fov; g_cfStamp=fov>0?GetTickCount64():0; ReleaseSRWLockExclusive(&g_cfLock);
}
void CfRefuse(const char* reason) {
    CfPublish(0);
    if (strcmp(g_cfReason,reason)) { g_cfReason=reason; Log("cine/fov: %s (writes=%u restores=%u refused=%u)",reason,g_cfWrites,g_cfRestores,g_cfRefused); }
}
bool CfValidate() {
    if (!g_cfHaveOwner || g_cfLoad!=g_mkLoadEvents || g_cfEpoch!=UiSurfaceEpoch() || !ChSlot(g_cfOwner[0]) ||
        !ChSlot(g_cfOwner[1]) || !ChSlot(g_cfOwner[2])) return false;
    if (!IsLiveObject((uint8_t*)g_cfOwner[0].value.obj) ||
        !IsLiveObject((uint8_t*)g_cfOwner[1].value.obj) ||
        !IsLiveObject((uint8_t*)g_cfOwner[2].value.obj)) return false;
    auto* pc=(uint8_t*)g_cfOwner[1].value.obj;
    return pc==g_peCtrl && CtObject(pc,g_ctPcCamera)==g_cfOwner[0].value.obj &&
        CtObject(pc,g_ctPawn)==g_cfOwner[2].value.obj;
}
}
static bool ProjectionFovScopeActive() {
    return GetCurrentThreadId()==g_sdDrawTid && g_cfGameplayScope && g_cfScope.field;
}
static float ProjectionFovGet() { return g_projectionFov.load(); }
static void ProjectionFovSet(float fov) {
    if (!std::isfinite(fov) || (fov!=0 && (fov<60 || fov>120))) {
        Log("projectionfov: refused %.2f; use 60..120 or off",fov); return;
    }
    g_projectionFov.store(fov); CfPublish(0);
    Log("projectionfov: %.2f deg (0=headset-derived; scoped gameplay view, proportional zoom retained)",fov);
}
static bool CineFovEnabled() { return g_cineFov.load(); }
static bool CineFovMatchEnabled() { return g_cineMatch.load(); }
static void CineFovMatchSet(bool on) {
    g_cineMatch.store(on); CfPublish(0);
    Log("cine/fov: MatchGameplayFov=%d (live; %s)", on ? 1 : 0,
        on ? "a locked scene frames at [Screen] ProjectionFov, the same as gameplay"
           : "a locked scene frames at the headset-derived FOV (the VR-227 behaviour)");
}
static void CineFovSet(bool on) {
    g_cineFov.store(on); if (!on) CfPublish(0);
    Log("cine/fov: %s (live; final scene FOV, gameplay zoom unchanged)",on?"ON":"off");
}
static void CineFovConfigure(const char* ini) {
    CineFovSet(GetPrivateProfileIntA("Cine","LockFov",1,ini)!=0);
    ProjectionFovSet(IniFloat(ini,"Screen","ProjectionFov",103.0f));
    CineFovMatchSet(GetPrivateProfileIntA("Cine","MatchGameplayFov",1,ini)!=0);
}
static float CineFovClaim() {
    if (!CineFovEnabled() && ProjectionFovGet()==0) return 0;
    AcquireSRWLockShared(&g_cfLock);
    const auto now=GetTickCount64();
    const float result=g_cfStamp && now>=g_cfStamp && now-g_cfStamp<=150 ? g_cfClaim : 0;
    ReleaseSRWLockShared(&g_cfLock); return result;
}
static float CineFovScopeTarget() { return g_cfScope.field && GetCurrentThreadId()==g_sdDrawTid ? g_cfScope.written : 0; }
static void CineFovBegin(bool scene) {
    if (g_cfScope.field) { ++g_cfRefused; return; }
    g_cfGameplayScope=false;
    const auto state=dvr::anim::snapshot();
    const bool menu=UiSurfaceBlocks() || g_menuOpen || g_inMenu || g_mainMenu || g_gameExiting ||
        (g_uiNoteOpen && MaimNowMs()-g_uiPollMs<500);
    const bool menuFovAllowed=UiSurfaceHeadLook() && !g_mainMenu && !g_gameExiting;
    const bool projection=dvr::stereo::wants_projection() && dvr::vr::session_live() &&
        !dvr::vr::cinematic_active() && !dvr::camera::eyetest_active() && !dvr::camera::postest_active();
    const float target=dvr::camera::fov_deg();
    const double now=MaimNowMs();
    const bool ready=dvr::cine_fov::eligible(CineFovEnabled(),scene,menu,projection,state.valid,target,menuFovAllowed);
    // The store is a scene too: opened from a conversation over the dialogue's
    // narrow camera, and left with that camera still blending back. Counting it
    // keeps the exit bridge primed, so leaving it holds the projection FOV until
    // the game's own FOV has returned instead of drawing the small box.
    const bool store=!strcmp(state.state[0],"StatePlayerMasterInStore");
    const bool authored=ready && (dvr::scene_state::cinematic(state.state[0]) || store);
    const bool walking=!strcmp(state.state[0],"StatePlayerMasterWalk") ||
        !strcmp(state.state[0],"StatePlayerMasterFalling") || !strcmp(state.state[0],"StatePlayerMasterJump");
    const bool keep=g_cfBridge.update(authored,ready && walking && CfValidate(),
        dvr::camera::rendered_fov_deg(),target,GetTickCount64());
    const float requested=ProjectionFovGet();
    const bool gameplay=!keep && !dvr::scene_state::cinematic(state.state[0]) &&
        dvr::cine_fov::eligible(requested>0,scene,menu && !menuFovAllowed,projection,state.valid,target);
    if (!keep && !gameplay) {
        if (g_cfHaveOwner) Log("cine/fov: released writes=%u restores=%u refused=%u master=%s menu=%d scene=%d projection=%d stateValid=%d target=%.2f requested=%.2f",g_cfWrites,g_cfRestores,g_cfRefused,state.state[0],menu,scene,projection,state.valid,target,requested);
        g_cfHaveOwner=false; CfPublish(0); return;
    }
    CineTraceTick();
    if (!g_ctLayout) { CfRefuse("camera layout unavailable"); return; }
    if (!g_cfOffset) {
        if (now<g_cfResolveAfter) return;
        g_cfResolveAfter=now+5000;
        uint32_t member=0;
        if (!FindPropOffsetChecked("TPOV","FOV",&member)) { CfRefuse("FOV reflection unavailable; retry in5s"); return; }
        g_cfOffset=g_ctCache+g_ctPov+member;
        Log("cine/fov: reflected Camera.CameraCache.POV.FOV=%x",g_cfOffset);
    }
    if (!CfValidate()) {
        g_cfHaveOwner=false;
        if (now<g_cfRetry) { CfRefuse("identity refresh retry pending"); return; }
        g_cfRetry=now+1000;
        if (!BuildLiveSet()) { CfRefuse("live-object table refresh refused"); return; }
        auto* pc=IsLiveObject(g_peCtrl)?g_peCtrl:nullptr;
        auto* cam=CtObject(pc,g_ctPcCamera); auto* pawn=CtObject(pc,g_ctPawn);
        if (!cam || cam!=g_camObj || !pawn || !ChCapture(cam,&g_cfOwner[0]) ||
            !ChCapture(pc,&g_cfOwner[1]) || !ChCapture(pawn,&g_cfOwner[2])) { CfRefuse("camera/controller/pawn identity unavailable"); return; }
        g_cfLoad=g_mkLoadEvents; g_cfEpoch=UiSurfaceEpoch(); g_cfHaveOwner=true; g_cfRetry=0;
    }
    auto* cam=(uint8_t*)g_cfOwner[0].value.obj;
    float* field=(float*)(cam+g_cfOffset);
    // A store is opened from a conversation, and the game keeps the dialogue's
    // narrow camera (about 32 deg) under it. Scaled as gameplay zoom that drew
    // the world behind the floating store panel in a small box (42 deg claimed).
    // The store is not a zoom: frame it like a scene.
    const float drawTarget=gameplay && !store && RangeReadable(field,4)
        ? dvr::cine_fov::gameplay_target(*field,target,requested)
        : (CineFovMatchEnabled() && requested>0 ? requested : target);   // a scene frames like gameplay
    if (!RangeReadable(field,4) || !g_cfScope.begin(field,drawTarget,CfValidate())) {
        ++g_cfRefused; CfRefuse("scope write refused: identity, field or FOV"); return;
    }
    g_cfGameplayScope=gameplay;
    ++g_cfWrites; g_cfReason="active"; CfPublish(drawTarget);
    if (now>=g_cfLog) {
        g_cfLog=now+500;
        Log("cine/fov: master=%s dialog=%d cache %.2f -> %.2f sensor=%.2f writes=%u restored=%u refused=%u exitBridge=%d gameplay=%d requested=%.2f; scoped claim, verify rendered acceptance",
            state.state[0],state.dialogState,g_cfScope.before,drawTarget,dvr::camera::rendered_fov_deg(),g_cfWrites,g_cfRestores,g_cfRefused,keep&&!authored,gameplay,requested);
    }
}
static void CineFovEnd() {
    g_cfGameplayScope=false;
    if (!g_cfScope.field) return;
    if (g_cfScope.end(CfValidate() && RangeReadable(g_cfScope.field,4))) ++g_cfRestores;
    else { ++g_cfRefused; g_cfHaveOwner=false; CfPublish(0); Log("cine/fov: restore refused: identity or camera FOV changed"); }
}
