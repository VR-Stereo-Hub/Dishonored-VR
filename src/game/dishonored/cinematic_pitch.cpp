// VR-105: suppress authored pitch only. Height and horizontal camera motion remain.
namespace {
std::atomic<bool> g_cinePitch{false},g_cineRoll{false};
CtIdentity g_cpOwner[3];
bool g_cpHaveOwner=false,g_cpScope=false;
LONG g_cpLoad=0;
HtSample g_cpHead={};
int32_t g_cpWritten[3]={};
double g_cpRetry=0,g_cpLog=0;
uint32_t g_cpWrites=0,g_cpRestores=0,g_cpRefused=0;
bool CpValidate(uint8_t* cam) {
    if (!g_cpHaveOwner || g_cpLoad!=g_mkLoadEvents || cam!=g_cpOwner[0].value.obj ||
        !ChSlot(g_cpOwner[0]) || !ChSlot(g_cpOwner[1]) || !ChSlot(g_cpOwner[2])) return false;
    auto* pc=(uint8_t*)g_cpOwner[1].value.obj;
    return pc==g_peCtrl && CtObject(pc,g_ctPcCamera)==cam && CtObject(pc,g_ctPawn)==g_cpOwner[2].value.obj;
}
}
static bool CineRollEnabled() { return g_cineRoll.load(); }
static void CineRollSet(bool on) {
    g_cineRoll.store(on); Log("cine/roll: %s (physical HMD roll; authored cinematic roll suppressed)",on?"ON":"off");
}
static bool CinePitchEnabled() { return g_cinePitch.load(); }
static void CinePitchSet(bool on) {
    g_cinePitch.store(on); Log("cine/pitch: %s (physical HMD pitch; authored yaw/roll and height retained)",on?"ON":"off");
}
static void CinePitchConfigure(const char* ini) {
    CinePitchSet(GetPrivateProfileIntA("Cine","LockPitch",0,ini)!=0);
    CineRollSet(GetPrivateProfileIntA("Cine","LockRoll",0,ini)!=0);
}
static void CinePitchPublish() {
    if (g_cpScope) HtPublishCameraRecord(3,g_cpHead,g_cpWritten[1]*360.0f/65536,
        g_cpWritten[0]*360.0f/65536,g_cpWritten[2]*360.0f/65536);
}
static void CinePitchBegin(bool scene,bool doubleDraw) {
    // Full authored head-look already replaced its composed pitch before writing.
    if (g_chScope) { g_cpHaveOwner=false; return; }
    const auto state=dvr::anim::snapshot();
    const double now=MaimNowMs();
    const bool menu=g_menuOpen || g_inMenu || g_mainMenu || g_gameExiting ||
        (g_uiNoteOpen && now-g_uiPollMs<500);
    const bool animation=state.valid && (state.game || dvr::scene_state::cinematic(state.state[0]));
    const bool rollLock=CineRollEnabled() && state.valid && dvr::scene_state::cinematic(state.state[0]);
    const bool ready=(CinePitchEnabled() || rollLock) && scene && animation && !menu &&
        dvr::stereo::wants_projection() && dvr::vr::session_live() && !dvr::vr::cinematic_active() &&
        !dvr::camera::eyetest_active() && !dvr::camera::postest_active() && !dvr::camera::pitchtest_active();
    if (!ready) { g_cpHaveOwner=false; return; }
    HtSample head={};
    if (!HtConsumeSample(&head) || !head.ok || !head.poseOk || now<head.locateMs || now-head.locateMs>100 ||
        !std::isfinite(head.pitch) || !std::isfinite(head.roll)) return;
    CineTraceTick();
    if (!g_ctLayout) return;
    if (!CpValidate((uint8_t*)g_cpOwner[0].value.obj)) {
        g_cpHaveOwner=false;
        if (now<g_cpRetry) return;
        g_cpRetry=now+1000;
        if (!BuildLiveSet()) { Log("cine/pitch: identity refresh refused"); return; }
        auto* pc=IsLiveObject(g_peCtrl)?g_peCtrl:nullptr;
        auto* cam=CtObject(pc,g_ctPcCamera); auto* pawn=CtObject(pc,g_ctPawn);
        if (!cam || cam!=g_camObj || !pawn || !ChCapture(cam,&g_cpOwner[0]) ||
            !ChCapture(pc,&g_cpOwner[1]) || !ChCapture(pawn,&g_cpOwner[2])) {
            Log("cine/pitch: current camera/controller/pawn unavailable"); return;
        }
        g_cpLoad=g_mkLoadEvents; g_cpHaveOwner=true; g_cpRetry=0;
    }
    auto* cam=(uint8_t*)g_cpOwner[0].value.obj;
    if (!CtRead(cam,g_ctCache+g_ctPov+g_ctRot,g_cpWritten,12)) return;
    const int32_t authoredPitch=g_cpWritten[0];
    if (CinePitchEnabled() && !dvr::cine::physical_pitch(g_cpWritten,head.pitch*g_flipPitch)) return;
    if (rollLock) g_cpWritten[2]=(int32_t)std::lround(head.roll*g_flipRoll*65536.0/6.2831853071795864769);
    constexpr double radians=6.2831853071795864769/65536.0;
    const auto basis=dvr::cine::rotation(g_cpWritten[0]*radians,g_cpWritten[1]*radians,g_cpWritten[2]*radians);
    const float right[3]={(float)basis.m[0][1],(float)basis.m[1][1],(float)basis.m[2][1]};
    g_cpScope=dvr::camera::begin_view_scope(cam,g_ctCache+g_ctPov+g_ctRot,g_cpWritten,right,doubleDraw?-1:0,CpValidate,false);
    if (!g_cpScope) {
        ++g_cpRefused;
        DVR_LOG_EVERY_MS(DVR_CAT,dvr::log::Level::Info,1000,"cine/pitch: scope refused: camera identity/field unavailable"); return;
    }
    ++g_cpWrites; g_cpHead=head; CinePitchPublish();
    if(now>=g_cpLog) {
        g_cpLog=now+500;
        Log("cine/pitch: master=%s authored=%.2f physical=%.2f yaw=%.2f roll=%.2f writes=%u restores=%u refused=%u; translation policy unchanged",
            state.state[0],authoredPitch*360.0f/65536,g_cpWritten[0]*360.0f/65536,
            g_cpWritten[1]*360.0f/65536,g_cpWritten[2]*360.0f/65536,g_cpWrites,g_cpRestores,g_cpRefused);
    }
}
static void CinePitchEnd() {
    if (!g_cpScope) return;
    if(dvr::camera::end_view_scope()) ++g_cpRestores;
    else { ++g_cpRefused; g_cpHaveOwner=false; Log("cine/pitch: restore refused: identity or field changed"); }
    g_cpScope=false;
}
