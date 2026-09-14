#include <atomic>
// VR-70: read-only camera ownership trace, included after reflection in the unity TU.
namespace {
std::atomic<bool> g_cineTrace{false};
uint32_t g_ctPcCamera, g_ctPawn, g_ctActorRot, g_ctCache, g_ctPov;
uint32_t g_ctLoc, g_ctRot, g_ctStyle, g_ctInfluence[3], g_ctWeight;
bool g_ctResolved = false, g_ctLayout = false;
double g_ctNext = 0, g_ctRefresh = 0, g_ctResolveAfter = 0;
uint32_t g_ctSequence = 0;
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
    g_cineTrace.store(GetPrivateProfileIntA("Cine", "Trace", 0, ini) != 0);
    Log("cine/trace: %s ([Cine] Trace), read-only camera ownership at draw entry, 100 ms cadence",
        g_cineTrace.load() ? "ON" : "off");
}
static void CineTraceSet(bool on) {
    g_cineTrace.store(on);
    Log("cine/trace: %s (live); no engine writes", on ? "ON" : "off");
}
static void CineTraceTick() {
    if (!g_cineTrace.load() || g_ctResolved || !RflNamesReady()) return;
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
    int32_t pcRot[3] = {}, camRot[3] = {};
    float loc[3] = {}, c5[3] = {}, pos[3] = {};
    const bool pcOk = g_ctActorRot && CtRead(pc, g_ctActorRot, pcRot, sizeof(pcRot));
    const bool camOk = g_ctLayout && g_ctCache &&
        CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, loc, sizeof(loc)) &&
        CtRead(cam, g_ctCache + g_ctPov + g_ctRot, camRot, sizeof(camRot));
    uint32_t style[2] = {}, name[2] = {};
    const bool styleOk = g_ctStyle && CtRead(cam, g_ctStyle, style, sizeof(style));
    CtRead(cam, kNameOff, name, sizeof(name));
    const bool c5Ok = dvr::camera::render_pos_world(c5);
    dvr::camera::position_offset_uu(pos);
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
    g_ctHits = hits; g_ctWrites = writes;
}
