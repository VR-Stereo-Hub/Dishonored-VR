#include <atomic>
#include "game/dishonored/cinematic_handoff_policy.h"
#include "game/dishonored/cinematic_math.h"
#include "game/dishonored/cinematic_policy.h"
// VR-70: camera ownership trace and draw-scoped head rotation; after reflection in the unity TU.
namespace {
std::atomic<bool> g_cineTrace{false}, g_cineHead{false}, g_specialHead{false};
uint32_t g_ctPcCamera, g_ctPawn, g_ctActorRot, g_ctCache, g_ctPov;
uint32_t g_ctLoc, g_ctRot, g_ctStyle, g_ctInfluence[3], g_ctWeight;
bool g_ctResolved = false, g_ctLayout = false;
double g_ctNext = 0, g_ctRefresh = 0, g_ctResolveAfter = 0;
uint32_t g_ctSequence = 0;
uint32_t g_ctLockOff[4]={},g_ctLockMask[4]={},g_ctIgnoreMove=0,g_ctIgnoreLook=0;
LONG g_ctHits = 0, g_ctWrites = 0;
bool CtRead(uint8_t* obj, uint32_t off, void* out, size_t size) {
    if (!obj || !IsLiveObject(obj) || !RangeReadable(obj + off, size)) return false;
    memcpy(out, obj + off, size); return true;
}
uint8_t* CtObject(uint8_t* obj, uint32_t off) {
    uint8_t* value = nullptr;
    return off && CtRead(obj, off, &value, sizeof(value)) && IsLiveObject(value) ? value : nullptr;
}
float CtWeight(uint8_t* cam, int index) {
    float value = -1;
    uint8_t* influence = CtObject(cam, g_ctInfluence[index]);
    if (g_ctWeight) CtRead(influence, g_ctWeight, &value, sizeof(value));
    return value;
}
}
static void CineTraceConfigure(const char* ini) {
    g_specialHead.store(GetPrivateProfileIntA("Cine","SpecialHeadLook",0,ini)!=0);
    Log("camera/special: SpecialHeadLook=%d (lean/keyhole final-camera head look)",int(g_specialHead.load()));
    g_cineHead.store(GetPrivateProfileIntA("Cine", "HeadLook", 1, ini) != 0);
    Log("cine/head: %s ([Cine] HeadLook), draw-scoped authored rotation", g_cineHead.load() ? "ON" : "off");
    g_cineTrace.store(GetPrivateProfileIntA("Cine", "Trace", 0, ini) != 0);
    Log("cine/trace: %s ([Cine] Trace), read-only camera ownership at draw entry, 100 ms cadence",
        g_cineTrace.load() ? "ON" : "off");
}
static void CineTraceSet(bool on) {
    g_cineTrace.store(on);
    Log("cine/trace: %s (live); no engine writes", on ? "ON" : "off");
}
static void CineTraceTick() {
    if ((!g_cineTrace.load() && !g_cineHead.load() && !CineFovEnabled() && !CinePitchEnabled() && !CineRollEnabled()) || g_ctResolved || !RflNamesReady()) return;
    const double now = MaimNowMs();
    if (now < g_ctResolveAfter || !IsLiveObject(g_camObj) || !CamStillValid()) return;
    g_ctResolveAfter = now + 5000;
    // Do not memoize an early miss while packages are still being loaded.
    const bool ownerLayout = FindPropOffsetChecked("PlayerController", "PlayerCamera", &g_ctPcCamera) &&
        FindPropOffsetChecked("Controller", "Pawn", &g_ctPawn) &&
        FindPropOffsetChecked("Actor", "Rotation", &g_ctActorRot) &&
        FindPropOffsetChecked("Camera", "CameraCache", &g_ctCache);
    g_ctStyle = RflOffsetOf("Camera", "CameraStyle");
    // Zero is a valid struct-member offset.
    g_ctLayout = ownerLayout && FindPropOffsetChecked("TCameraCache", "POV", &g_ctPov) &&
        FindPropOffsetChecked("TPOV", "Location", &g_ctLoc) &&
        FindPropOffsetChecked("TPOV", "Rotation", &g_ctRot);
    if (!g_ctLayout) {
        Log("cine/trace: reflected camera layout unavailable; no cache read, retry in 5 s");
        return;
    }
    const char* fields[] = {"m_pAnimDrive_Influence", "m_pPlayerControl_Influence", "m_pLook_Influence"};
    for (int i = 0; i < 3; ++i) g_ctInfluence[i] = RflOffsetOf("DishonoredPlayerCamera", fields[i]);
    g_ctWeight = RflOffsetOf("DishonoredCameraInfluence", "m_Weight");
    const char* lockNames[]={"bCinematicMode","bCinemaDisableInputMove","bCinemaDisableInputLook","m_bInputIgnoreInput_Cinematic"};
    for(int i=0;i<4;++i) FindBoolProp(i==3 ? "DishonoredPlayerController" : "PlayerController",lockNames[i],&g_ctLockOff[i],&g_ctLockMask[i]);
    g_ctIgnoreMove=RflOffsetOf("PlayerController","bIgnoreMoveInput");
    g_ctIgnoreLook=RflOffsetOf("PlayerController","bIgnoreLookInput");
    g_ctResolved = true;
    Log("cine/trace: reflected pcCamera=%x pawn=%x actorRot=%x cache=%x pov=%x loc=%x rot=%x layout=%d; "
        "cache location %s measured eye field +%x",
        g_ctPcCamera, g_ctPawn, g_ctActorRot, g_ctCache, g_ctPov, g_ctLoc, g_ctRot, (int)g_ctLayout,
        g_ctLayout && g_ctCache + g_ctPov + g_ctLoc == kPovOffs[0] ? "matches" : "DOES NOT MATCH", kPovOffs[0]);
}
static void CineTraceDraw() {
    if (!g_cineTrace.load()) return;
    const double now = MaimNowMs();
    if (now < g_ctNext) return;
    g_ctNext = now + 100;
    // Read-only; refresh at most once a second, so a pre-load table cannot
    // silently hide newly created camera/state objects for the entire launch.
    int rebuilt = 0;
    uint8_t* rawCam = nullptr;
    if (g_peCtrl && g_ctPcCamera && RangeReadable(g_peCtrl + g_ctPcCamera, sizeof(rawCam)))
        memcpy(&rawCam, g_peCtrl + g_ctPcCamera, sizeof(rawCam));
    if ((!IsLiveObject(g_peCtrl) || !IsLiveObject(rawCam)) && now >= g_ctRefresh) {
        rebuilt = BuildLiveSet() ? 1 : -1; g_ctRefresh = now + 1000;
    }
    uint8_t* pc = IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam = CtObject(pc, g_ctPcCamera);
    uint8_t* pawn = CtObject(pc, g_ctPawn);
    // Read-only effect ownership evidence; do not suppress a guessed HUD rectangle.
    static uint32_t hitOff=0,healthOff=0,healthIndexOff=0,rainOff=0,rainCountOff=0,targetOff=0;
    static double effectNext=0;
    if(now>=effectNext) {
        effectNext=now+1000;
        if(!hitOff)FindPropOffsetChecked("DishonoredPlayerCamera","m_pHitReact_Influence",&hitOff);
        if(!healthOff)FindPropOffsetChecked("DishonoredPlayerPawn","m_pCurHealthLensEffect",&healthOff);
        if(!healthIndexOff)FindPropOffsetChecked("DishonoredPlayerPawn","m_iActiveHealthEffect",&healthIndexOff);
        if(!rainOff)FindPropOffsetChecked("DishonoredPlayerCamera","m_pRainBoxEmitter",&rainOff);
        if(!rainCountOff)FindPropOffsetChecked("DishonoredPlayerCamera","m_NumRainDrops",&rainCountOff);
        if(!targetOff)FindPropOffsetChecked("DishonoredCameraInfluence","m_TargetWeight",&targetOff);
        auto* hit=CtObject(cam,hitOff);auto* health=CtObject(pawn,healthOff);auto* rain=CtObject(cam,rainOff);
        float weight=-1,target=-1;int healthIndex=-1,rainCount=-1;
        if(g_ctWeight)CtRead(hit,g_ctWeight,&weight,4);
        if(targetOff)CtRead(hit,targetOff,&target,4);
        if(healthIndexOff)CtRead(pawn,healthIndexOff,&healthIndex,4);
        if(rainCountOff)CtRead(cam,rainCountOff,&rainCount,4);
        Log("effects/owners: hit=%p weight=%.3f target=%.3f health=%p class=%s index=%d rain=%p class=%s drops=%d layout=%d/%d/%d; read-only, null may mean inactive or unavailable",
            hit,weight,target,health,health?ObjClassName(health):"none",healthIndex,
            rain,rain?ObjClassName(rain):"none",rainCount,int(hitOff!=0),int(healthOff!=0),int(rainOff!=0));
    }
    int32_t pcRot[3] = {}, camRot[3] = {};
    float loc[3] = {}, c5[3] = {}, pos[3] = {}, cinePos[3] = {};
    const bool pcOk = g_ctActorRot && CtRead(pc, g_ctActorRot, pcRot, sizeof(pcRot));
    const bool camOk = g_ctLayout && g_ctCache &&
        CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, loc, sizeof(loc)) &&
        CtRead(cam, g_ctCache + g_ctPov + g_ctRot, camRot, sizeof(camRot));
    uint32_t style[2] = {}, name[2] = {};
    const bool styleOk = g_ctStyle && CtRead(cam, g_ctStyle, style, sizeof(style));
    CtRead(cam, kNameOff, name, sizeof(name));
    const bool c5Ok = dvr::camera::render_pos_world(c5);
    dvr::camera::position_offset_uu(pos);
    dvr::camera::cinematic_position_offset_uu(cinePos);
    HtSample head = {};
    const bool headOk = HtConsumeSample(&head);
    const auto anim = dvr::anim::snapshot();
    const LONG hits = g_pvrHits, writes = g_pvrWrites;
    const uint32_t seq = ++g_ctSequence;
    const char* styleName = styleOk ? RealName(style[0]) : nullptr;
    Log("cine/trace #%u state: pc=%p pawn=%p camera=%p name=%u/%u liveRefresh=%d "
        "menu=%d/%d/%d latch=%d projection=%d runtimeQuad=%d animValid=%d master=%s upper=%s "
        "style=%s influence(anim/player/look)=%.3f/%.3f/%.3f",
        seq, pc, pawn, cam, name[0], name[1], (int)rebuilt,
        (int)g_menuOpen, (int)g_inMenu, (int)g_mainMenu, (int)g_cineNow,
        (int)dvr::stereo::wants_projection(), (int)dvr::vr::cinematic_active(),
        (int)anim.valid, anim.state[0], anim.state[1], styleName ? styleName : "unavailable",
        CtWeight(cam, 0), CtWeight(cam, 1), CtWeight(cam, 2));
    Log("cine/trace #%u input: headOk=%d poseOk=%d gen=%u age=%.1f Hdeg(P/Y/R)=%.2f/%.2f/%.2f "
        "HposXR=%.4f/%.4f/%.4f pvrHits=%ld writes=%ld rotInjectEnabled=%d scriptAge=%.1f injected(P/Y)=%.2f/%.2f",
        seq, (int)headOk, (int)head.poseOk, head.gen, headOk ? now-head.locateMs : -1,
        head.pitch*57.29578f, head.yaw*57.29578f, head.roll*57.29578f, head.px, head.py, head.pz,
        hits-g_ctHits, writes-g_ctWrites, (int)g_rotInject, now-g_scriptHeadMs,
        g_viewPitchRad*57.29578f, g_viewYawRad*57.29578f);
    // Prior render c5 is not synchronized to this draw; never call its delta
    // an acceptance measurement of this particular camera sample.
    Log("cine/trace #%u camera: pcOk=%d pcDeg=%.2f/%.2f/%.2f cacheOk=%d cacheDeg=%.2f/%.2f/%.2f "
        "cachePos=%.3f/%.3f/%.3f posRequestRUF=%.3f/%.3f/%.3f priorRenderOk=%d priorRenderWorld=%.3f/%.3f/%.3f",
        seq, (int)pcOk, pcRot[0]*360.0f/65536, pcRot[1]*360.0f/65536, pcRot[2]*360.0f/65536,
        (int)camOk, camRot[0]*360.0f/65536, camRot[1]*360.0f/65536, camRot[2]*360.0f/65536,
        loc[0], loc[1], loc[2], pos[0], pos[1], pos[2], (int)c5Ok, c5[0], c5[1], c5[2]);
    Log("cine/trace #%u position: gameplayRUF=%.3f/%.3f/%.3f authoredRUF=%.3f/%.3f/%.3f neckMode=%d; requests, not synchronized render measurements",
        seq,pos[0],pos[1],pos[2],cinePos[0],cinePos[1],cinePos[2],g_neckMode);
    int locks[4]={-1,-1,-1,-1};
    for(int i=0;i<4;++i) {
        uint32_t bits=0;
        if(g_ctLockOff[i] && g_ctLockMask[i] && CtRead(pc,g_ctLockOff[i],&bits,sizeof(bits))) locks[i]=(bits&g_ctLockMask[i]) ? 1 : 0;
    }
    uint8_t move=0,look=0;
    const bool moveOk=g_ctIgnoreMove && CtRead(pc,g_ctIgnoreMove,&move,1);
    const bool lookOk=g_ctIgnoreLook && CtRead(pc,g_ctIgnoreLook,&look,1);
    Log("cine/trace #%u controls: cinematic=%d disableMove=%d disableLook=%d ignoreCinematic=%d ignoreMove=%d ignoreLook=%d (-1=unavailable)",
        seq,locks[0],locks[1],locks[2],locks[3],moveOk ? (int)move : -1,lookOk ? (int)look : -1);
    g_ctHits = hits; g_ctWrites = writes;
}

namespace {
struct CtIdentity {
    dvr::menukeep::Identity value;
    uint32_t index=0;
};
CtIdentity g_chOwner[3]; // camera, controller, pawn
bool g_chReference=false, g_chScope=false;
int g_chKind=0;
bool g_chExitPending=false;
double g_chExitReferenceYaw=0,g_chExitScopeMs=0;
LONG g_chLoad=0;
dvr::cine::Matrix g_chRef={};
HtSample g_chHead={},g_chReferenceHead={};
int32_t g_chWritten[3]={};
uint32_t g_chWrites=0, g_chRestores=0, g_chRefused=0;
double g_chNextLog=0, g_chRetry=0, g_chInputUntil=0;
const char* g_chReason="startup";
bool ChSlot(const CtIdentity& id) {
    auto* obj=(uint8_t*)id.value.obj;
    if (!IsLiveObject(obj) || !RangeReadable((void*)kGObjHdr,12)) return false;
    void** objects=*(void***)kGObjHdr;
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if (!objects || id.index>=count || !RangeReadable(objects+id.index,sizeof(void*)) || objects[id.index]!=obj) return false;
    dvr::menukeep::Identity now; MkReadIdentity(obj,&now);
    return now.obj==obj && now.cls==id.value.cls && now.name[0]==id.value.name[0] && now.name[1]==id.value.name[1];
}
bool ChCapture(uint8_t* obj,CtIdentity* out) {
    if (!IsLiveObject(obj) || !RangeReadable((void*)kGObjHdr,12)) return false;
    void** objects=*(void***)kGObjHdr;
    const uint32_t count=*(uint32_t*)(kGObjHdr+4);
    if (!objects || count>4000000 || !RangeReadable(objects,count*sizeof(void*))) return false;
    for(uint32_t i=0;i<count;++i) if(objects[i]==obj) {
        out->index=i; MkReadIdentity(obj,&out->value); return ChSlot(*out);
    }
    return false;
}
bool ChValidate(uint8_t* cam) {
    if (g_chLoad != g_mkLoadEvents || cam!=g_chOwner[0].value.obj || !ChSlot(g_chOwner[0]) ||
        !ChSlot(g_chOwner[1]) || !ChSlot(g_chOwner[2])) return false;
    auto* pc=(uint8_t*)g_chOwner[1].value.obj;
    return pc==g_peCtrl && CtObject(pc,g_ctPcCamera)==cam &&
        CtObject(pc,g_ctPawn)==g_chOwner[2].value.obj;
}
void ChReason(const char* why) {
    if (strcmp(g_chReason,why)) {
        Log("cine/head: %s (writes=%u restored=%u refused=%u menu=%d/%d/%d quad=%d camera=%p load=%ld/%ld)",
            why,g_chWrites,g_chRestores,g_chRefused,(int)g_menuOpen,(int)g_inMenu,(int)g_mainMenu,
            (int)dvr::vr::cinematic_active(),g_camObj,g_chLoad,(LONG)g_mkLoadEvents);
        g_chReason=why;
    }
    // Logging a temporary refusal does not change the reference.
}
void ChReset(const char* why) { ChReason(why); g_chReference=false; g_chInputUntil=0; }
}
// A short lease is issued only by a successful, live draw scope. No engine
// memory is written here. The controller path leaves authored/stick yaw intact.
static bool CineHeadOwnsInput() {
    const auto state=dvr::anim::snapshot();
    const double now=MaimNowMs();
    return g_cineHead.load() && g_trackingEnabled && g_rotInject && g_chReference &&
        now<=g_chInputUntil && now>=g_chInputUntil-100 &&
        state.valid && (dvr::scene_state::cinematic(state.state[0]) ||
            (g_specialHead.load() && dvr::cine::special_camera(state.state[0]))) &&
        !g_menuOpen && !g_inMenu && !g_mainMenu && !g_gameExiting &&
        dvr::vr::session_live() && dvr::stereo::wants_projection() && !dvr::vr::cinematic_active() &&
        ChValidate((uint8_t*)g_chOwner[0].value.obj);
}
static std::atomic<unsigned long long> g_chDispatchMs{0};
static void CineHeadNoteDispatch() { g_chDispatchMs.store(GetTickCount64()); }
static bool CineHeadDispatchFresh() {
    const auto seen=g_chDispatchMs.load(),now=GetTickCount64();
    return CineDispatchRecent(g_cineHead.load(),seen,now);
}
static bool CineHeadEnabled() { return g_cineHead.load(); }
static void CineHeadSet(bool on) {
    g_cineHead.store(on); Log("cine/head: %s (live)",on ? "ON" : "off");
}
static bool SpecialHeadEnabled(){return g_specialHead.load();}
static void SpecialHeadSet(bool on){g_specialHead.store(on);Log("camera/special: SpecialHeadLook=%d (live)",int(on));}
static bool SpecialHeadResumeYaw(int32_t& delta) {
    delta=0;
    if(!g_chExitPending || dvr::camera::second_pass_for_current_thread() || GetCurrentThreadId()!=g_sdDrawTid)return false;
    const auto state=dvr::anim::snapshot();
    if(state.valid && dvr::cine::special_camera(state.state[0]))return false;
    g_chExitPending=false;
    const double now=MaimNowMs();HtSample head{};
    const bool valid=g_specialHead.load() && g_cineHead.load() && g_trackingEnabled && g_rotInject &&
        state.valid && !strcmp(state.state[0],"StatePlayerMasterWalk") &&
        !UiSurfaceBlocks() && !g_menuOpen && !g_inMenu && !g_mainMenu && !g_gameExiting &&
        dvr::vr::session_live() && dvr::stereo::wants_projection() &&
        now>=g_chExitScopeMs && now-g_chExitScopeMs<=1000 &&
        BuildLiveSet() && ChValidate((uint8_t*)g_chOwner[0].value.obj) &&
        HtConsumeSample(&head) && head.ok && head.poseOk && now>=head.locateMs && now-head.locateMs<=100;
    const bool carry=valid && dvr::cine::special_resume_delta(g_chExitReferenceYaw,head.yaw*g_flipYaw,delta);
    Log("camera/special-exit: carry=%d delta=%.3f deg; owner/context/pose revalidated",int(carry),delta*360.f/65536);
    return carry;
}
static void CineHeadPublish() {
    if (g_chScope) HtPublishCameraRecord(3,g_chHead,g_chWritten[1]*360.0f/65536,
                                       g_chWritten[0]*360.0f/65536,g_chWritten[2]*360.0f/65536);
}
static void CineHeadBegin(bool sceneDraw, bool doubleDraw) {
    if (!g_cineHead.load() || !g_trackingEnabled || !g_rotInject) { ChReset("disabled"); return; }
    HtSample head={}; const double now=MaimNowMs();
    const bool poseReady=HtConsumeSample(&head) && head.ok && head.poseOk &&
        now>=head.locateMs && now-head.locateMs<=100 &&
        std::isfinite(head.pitch) && std::isfinite(head.yaw) && std::isfinite(head.roll);
    const bool runtimeReady=dvr::vr::session_live() && dvr::stereo::wants_projection() &&
        !dvr::vr::cinematic_active() && !dvr::camera::eyetest_active() &&
        !dvr::camera::postest_active() && !dvr::camera::pitchtest_active();
    uint8_t* pc=g_ctLayout && IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam=CtObject(pc,g_ctPcCamera); uint8_t* pawn=CtObject(pc,g_ctPawn);
    // HeadLook must recover new level objects even with the trace disabled.
    bool refreshed=false;
    if ((!pc || !cam || !pawn) && sceneDraw && runtimeReady && now>=g_chRetry) {
        refreshed=BuildLiveSet(); g_chRetry=now+1000;
        pc=IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
        cam=CtObject(pc,g_ctPcCamera); pawn=CtObject(pc,g_ctPawn);
        if (refreshed && pc && cam && pawn) g_chRetry=0;
    }
    float animWeight=CtWeight(cam,0), playerWeight=CtWeight(cam,1), lookWeight=CtWeight(cam,2);
    const auto state=dvr::anim::snapshot();
    const int special=state.valid && g_specialHead.load()?dvr::cine::special_camera(state.state[0]):0;
    const bool scripted=state.valid && (dvr::scene_state::cinematic(state.state[0]) || special);
    const int kind=special?special:scripted?3:0;
    if(state.valid && g_chReference && (special || g_chKind==1 || g_chKind==2) && kind!=g_chKind)
        ChReset("camera state changed");
    const bool ownerChanged=g_chReference && !ChValidate((uint8_t*)g_chOwner[0].value.obj);
    // Explicit cinematic states keep one owner across influence blends. Outside
    // those states only fully authored cameras use this scope.
    const bool known=cam && cam==g_camObj && pawn && CamAlive() &&
        animWeight>=0 && playerWeight>=0 && lookWeight>=0;
    const dvr::cine::Conditions conditions={
        g_cineHead.load() && g_trackingEnabled && g_rotInject,
        UiSurfaceBlocks() || g_menuOpen || g_inMenu || g_mainMenu, ownerChanged,
        known, scripted || animWeight>0, dvr::cine::owns_rotation(scripted,animWeight,playerWeight,lookWeight),
        sceneDraw, runtimeReady, poseReady};
    const auto action=dvr::cine::action(conditions);
    if (action==dvr::cine::Action::Reset) {
        ChReset(!conditions.enabled ? "disabled" : conditions.menu ? "menu" :
                ownerChanged ? "owner changed" : "player camera owns rotation"); return;
    }
    if (action==dvr::cine::Action::Hold) {
        ChReason(!known ? "hold: camera ownership unavailable" : !conditions.fullyAuthored ? "hold: camera blend" :
                 !sceneDraw ? "hold: no scene draw" : !runtimeReady ? "hold: runtime" : "hold: stale head pose");
        return;
    }
    // Rebuild only on an actual new ownership interval, never on a single draw.
    // Retry delays apply to failed refreshes, not successful reference capture.
    if (!g_chReference) {
        if (!refreshed && now<g_chRetry) return;
        if (!refreshed && !BuildLiveSet()) { g_chRetry=now+1000; ChReason("hold: live table unavailable"); return; }
        pc=IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
        cam=CtObject(pc,g_ctPcCamera); pawn=CtObject(pc,g_ctPawn);
        animWeight=CtWeight(cam,0); playerWeight=CtWeight(cam,1); lookWeight=CtWeight(cam,2);
        if (!cam || cam!=g_camObj || !pawn || !CamAlive() ||
            !dvr::cine::owns_rotation(scripted,animWeight,playerWeight,lookWeight)) {
            g_chRetry=now+1000; ChReason("hold: refreshed owner unavailable"); return;
        }
    }
    const auto h=dvr::cine::rotation(head.pitch*g_flipPitch,head.yaw*g_flipYaw,head.roll*g_flipRoll);
    if (!g_chReference) {
        g_chExitPending=false; // never carry a prior owner/reference into this interval
        if (!ChCapture(cam,&g_chOwner[0]) || !ChCapture(pc,&g_chOwner[1]) || !ChCapture(pawn,&g_chOwner[2])) {
            ChReset("identity capture refused"); return;
        }
        g_chLoad=g_mkLoadEvents; g_chKind=kind; g_chRef=h; g_chReferenceHead=head; g_chReference=true; g_chReason="active";
        Log("cine/head: entered authored camera=%p pc=%p pawn=%p gen=%u; physical orientation anchored",cam,pc,pawn,head.gen);
    }
    int32_t authored[3]={}; dvr::cine::Matrix composed;
    if (!CtRead(cam,g_ctCache+g_ctPov+g_ctRot,authored,12) ||
        !dvr::cine::compose(authored,g_chRef,h,g_chWritten,&composed)) { ChReset("rotation invalid"); return; }
    if (special || CinePitchEnabled() || CineRollEnabled()) {
        if (!dvr::cine::comfort(authored,
            g_chReferenceHead.pitch*g_flipPitch,g_chReferenceHead.yaw*g_flipYaw,g_chReferenceHead.roll*g_flipRoll,
            head.pitch*g_flipPitch,head.yaw*g_flipYaw,head.roll*g_flipRoll,
            special || CinePitchEnabled(),special || CineRollEnabled(),g_chWritten,&composed)) { ChReset("physical comfort pose unavailable"); return; }
    }
    const float right[3]={(float)composed.m[0][1],(float)composed.m[1][1],(float)composed.m[2][1]};
    g_chHead=head;
    g_chScope=dvr::camera::begin_view_scope(cam,g_ctCache+g_ctPov+g_ctRot,g_chWritten,right,doubleDraw ? -1 : 0,ChValidate,true,head.rawPosition);
    if (!g_chScope) { ++g_chRefused; ChReason("hold: scope write refused"); return; }
    g_chInputUntil=scripted ? now+100 : 0;
    if(special){g_chExitPending=true;g_chExitReferenceYaw=g_chReferenceHead.yaw*g_flipYaw;g_chExitScopeMs=now;}
    ++g_chWrites; CineHeadPublish();
    if(now>=g_chNextLog) {
        g_chNextLog=now+500;
        Log("cine/head: scope=%u gen=%u authored(P/Y/R)=%.2f/%.2f/%.2f composed=%.2f/%.2f/%.2f restored=%u refused=%u double=%d scripted=%d special=%d upright=%d/%d",
            g_chWrites,head.gen,authored[0]*360.0f/65536,authored[1]*360.0f/65536,authored[2]*360.0f/65536,
            g_chWritten[0]*360.0f/65536,g_chWritten[1]*360.0f/65536,g_chWritten[2]*360.0f/65536,g_chRestores,g_chRefused,(int)doubleDraw,(int)scripted,special,(int)CinePitchEnabled(),(int)CineRollEnabled());
    }
}
static void CineHeadEnd() {
    if (!g_chScope) return;
    if (dvr::camera::end_view_scope()) ++g_chRestores;
    else { ++g_chRefused; g_chExitPending=false; ChReset("restore refused: identity or engine field changed"); }
    g_chScope=false;
}
