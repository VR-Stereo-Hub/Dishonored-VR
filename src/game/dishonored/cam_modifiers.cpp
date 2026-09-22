// game/dishonored/cam_modifiers.cpp - VR-165: WHICH camera modifier is still
// swinging? Read-only. Included by src/mod/dishonoredvr.cpp after
// ue3/reflect.cpp, whose name-keyed resolver this uses.
//
// THE MEASUREMENT THIS EXISTS TO SETTLE. A playtester came off a chain and the
// camera went on swinging as if still on it, for about 23 seconds. Measured
// from their log: the engine's player state machine was clean
// (Climb -> Jump -> Falling -> Walk), OUR positional request was flat in every
// window (0-1 reversals, under 5 uu of range), and the GAME's own camera
// oscillated at ~8 Hz with up to 413 uu of range - five times the amplitude of
// ordinary walking. So the swing is game-side and no clamp on our own writer
// can touch it.
//
// Camera.ModifierList excludes only UE3 CameraModifier objects. Dishonored uses
// a SEPARATE m_InfluenceGroups array. An empty ModifierList never eliminated
// those influences; the source probe below reads that actual graph.
// The old frequency estimates were retracted; see FLICKER_REFERENCE.md.
//
// The probe is kept because that elimination is the result, and because it is
// what will show a modifier arriving if one ever does.
//
// HOW IT READS. `Camera.ModifierList` is a UE3 `array<CameraModifier>`, and
// each `CameraModifier` carries Alpha, TargetAlpha, Priority and bDisabled.
// Every offset comes from the name-keyed resolver (RflOffsetOf), never from a
// hardcoded number: a name outlives a game rebuild and this project has paid
// for copied offsets before. A property that cannot be resolved is reported as
// unresolved on the line rather than read as a zero.
//
// It writes nothing, with ONE exception that only ever runs when asked: the
// `camspring kick|nudge|rest` seam word writes one spring point of one influence
// (VR-165, "Reproduce on demand" below). It retains no pointer across frames
// except the kick watch's influence, which it re-validates with IsLiveObject on
// every sample.
#include <atomic>

namespace {

// Ships ON: the tester plays in a headset and cannot reach a prompt, so a probe
// that has to be asked for is one that never runs. [Diagnostics] CamModProbe.
bool     g_cmOn        = true;
double   g_cmNextMs    = 0.0;
double   g_cmLastMoveMs= 0.0;   // when the camera last moved a lot (the swing)
float    g_cmLastZ     = 0.0f;
bool     g_cmHaveZ     = false;
int      g_cmSwingRuns = 0;     // consecutive fast reversals seen
float    g_cmExtreme   = 0.0f;   // running extreme of the current leg
int      g_cmLeg       = 0;      // -1 falling, +1 rising, 0 unknown
double   g_cmBeatMs    = 0.0;    // the watcher's own heartbeat
double   g_cmPeakHz    = 0.0;
float    g_cmPeakAmp   = 0.0f;
bool     g_cmPrinted   = false;  // has any table printed this session?
int      g_cmReversals = 0;
double   g_cmWindowMs  = 0.0;
float    g_cmZMin=0.0f, g_cmZMax=0.0f;

const int   kCmPeriodMs = 1000;   // one table per second while it has something to say
// A direction change only counts once the camera has actually travelled this
// far back from the leg's extreme. Below this it is sampling noise, not motion.
const float kCmTurnUu   = 2.0f;

// Offsets, resolved once by NAME. 0 = unresolved, and the line says so.
uint32_t g_cmOffList=0, g_cmOffAlpha=0, g_cmOffTarget=0, g_cmOffPrio=0;
uint32_t g_cmOffDisabled=0, g_cmMaskDisabled=0;
bool     g_cmResolved=false;

void CmResolve()
{
    if (g_cmResolved) return;
    g_cmResolved = true;
    g_cmOffList   = RflOffsetOf("Camera", "ModifierList");
    g_cmOffAlpha  = RflOffsetOf("CameraModifier", "Alpha");
    g_cmOffTarget = RflOffsetOf("CameraModifier", "TargetAlpha");
    g_cmOffPrio   = RflOffsetOf("CameraModifier", "Priority");
    FindBoolProp("CameraModifier", "bDisabled", &g_cmOffDisabled, &g_cmMaskDisabled);
    Log("cammod: resolved by name - Camera.ModifierList +0x%x | CameraModifier Alpha +0x%x "
        "TargetAlpha +0x%x Priority +0x%x bDisabled +0x%x/0x%x. A 0 here is UNRESOLVED, "
        "not an offset of zero, and every row below will say so.",
        g_cmOffList, g_cmOffAlpha, g_cmOffTarget, g_cmOffPrio, g_cmOffDisabled, g_cmMaskDisabled);
}

// The camera's Z, for correlating a modifier's weight against the actual
// swing. This reads the SAME name-resolved CameraCache POV location that
// cinematic_trace prints as `cachePos`, so the number in this table and the
// number the oscillation was measured from are the same quantity. No new
// offset is introduced here; if the trace has not resolved its layout yet,
// this reports no Z rather than reading a guessed address.
bool CmCameraZ(uint8_t* cam, float* z)
{
    if (!cam || !g_ctLayout || !g_ctCache) return false;
    float loc[3] = {};
    if (!CtRead(cam, g_ctCache + g_ctPov + g_ctLoc, loc, sizeof(loc))) return false;
    *z = loc[2];
    return true;
}

// ---- VR-165: the widened census ----------------------------------------------
// Every field below is declared in the game's own class dump (DishonoredCameraInfluence,
// DishonoredCamera_PhysicalReact and its DisSpringPoint, _Lean, _BumpSmoother,
// DishonoredPlayerCamera, DishonoredCameraInfluenceGroup's DishonoredVTSettings) and
// resolved here by NAME, once. A 0 is UNRESOLVED and the line prints `?` for it.
// FindBoolProp is a full GObjects scan with no cache, so the bits are resolved once
// and kept; that is the cadence trap RflOffsetOf's own cache exists to prevent.
struct CsnLayout {
    bool done = false;
    uint32_t w = 0, acc = 0, step = 0;
    uint32_t actOff = 0, actMask = 0, slpOff = 0, slpMask = 0, waitOff = 0, waitMask = 0;
    uint32_t sPos = 0, sVel = 0, sPrev = 0, sBound = 0, sPrevVel = 0;          // DisSpringPoint (m_Pos is at 0 legitimately)
    uint32_t stab = 0, str = 0, pivot = 0;                        // PhysicalReact
    uint32_t head = 0, leanAng = 0, leanH = 0, leanPivot = 0, colDir = 0;
    uint32_t colOff = 0, colMask = 0, extOff = 0, extMask = 0;    // Lean
    uint32_t bumpH = 0, bumpLast = 0, compOff = 0, compMask = 0;  // BumpSmoother
    uint32_t vt = 0, nonAddPos = 0;                               // camera m_DishonoredVTSettings.m_NonAdditive_Pos
    uint32_t telOff = 0, telMask = 0, cenOff = 0, cenMask = 0, smOff = 0, smMask = 0, uncOff = 0, uncMask = 0;
    uint32_t status = 0, lastDif = 0, radius = 0, height = 0, tickTag = 0, pass = 0;
    uint32_t dominant = 0;
    uint32_t allowOff = 0, allowMask = 0;                         // m_bAllowCamSmoothingForCollisionPop (the VR-165 fix's switch)
} csn;

void CsnResolve()
{
    if (csn.done || !RflNamesReady()) return;
    csn.done = true;
    // ONE GObjects walk for every name (RflResolveBatch). One lookup at a time cost
    // about 100 ms each and froze the main menu for 3.6 s on the first census
    // (2026-09-22), and pushed the name cache past its old 96-entry limit.
    RflWant w[] = {
        { "DishonoredCameraInfluence", "m_Weight", false },
        { "DishonoredCameraInfluence", "m_fAccumulatedDelta", false },
        { "DishonoredCameraInfluence", "m_fCurFixedTimeStep", false },
        { "DisSpringPoint", "m_Pos", false },
        { "DisSpringPoint", "m_Velocity", false },
        { "DisSpringPoint", "m_Pos_Previous", false },
        { "DisSpringPoint", "m_BoundPos", false },
        { "DishonoredCamera_PhysicalReact", "m_StabilityPoint", false },
        { "DishonoredCamera_PhysicalReact", "m_StrengthPoint", false },
        { "DishonoredCamera_PhysicalReact", "m_CameraPivotOffset", false },
        { "DishonoredCamera_Lean", "m_HeadPoint", false },
        { "DishonoredCamera_Lean", "m_fCurLeanAngle", false },
        { "DishonoredCamera_Lean", "m_fCurLeanHeight", false },
        { "DishonoredCamera_Lean", "m_CurLeanPivot", false },
        { "DishonoredCamera_Lean", "m_CameraCollideDir", false },
        { "DishonoredCamera_BumpSmoother", "m_fLastInterpolatedHeight", false },
        { "DishonoredCamera_BumpSmoother", "m_fDebug_LastOffset", false },
        { "DishonoredPlayerCamera", "m_DishonoredVTSettings", false },
        { "DishonoredVTSettings", "m_NonAdditive_Pos", false },
        { "DishonoredPlayerCamera", "m_CurCollisionStatus", false },
        { "DishonoredPlayerCamera", "m_fLastCollisionDifFromNonAdditive", false },
        { "DishonoredPlayerCamera", "m_fCurCollisionRadius", false },
        { "DishonoredPlayerCamera", "m_fCurCollisionHeight", false },
        { "DishonoredPlayerCamera", "m_TickTag", false },
        { "DishonoredPlayerCamera", "m_PassCount", false },
        { "DishonoredCameraInfluenceGroup", "m_iDominantInfluence", false },
        { "DishonoredCameraInfluence", "m_bActive", true },
        { "DishonoredCameraInfluence", "m_bIsSleeping", true },
        { "DishonoredCameraInfluence", "m_bWaitToBlendOneFrame", true },
        { "DishonoredCamera_Lean", "m_bCameraCollided", true },
        { "DishonoredCamera_Lean", "m_bGotExternalForce", true },
        { "DishonoredCamera_BumpSmoother", "m_bIsCompensating", true },
        { "DishonoredPlayerCamera", "m_bTeleported", true },
        { "DishonoredPlayerCamera", "m_bCollisionEnabled", true },
        { "DishonoredPlayerCamera", "m_bSmoothingSuddenCollision", true },
        { "DishonoredPlayerCamera", "m_bWasUncovered", true },
        { "DisSpringPoint", "m_Velocity_Previous", false },
        { "DishonoredPlayerCamera", "m_bAllowCamSmoothingForCollisionPop", true },
    };
    RflResolveBatch(w, (int)(sizeof(w) / sizeof(w[0])));
    csn.w = w[0].off;
    csn.acc = w[1].off;
    csn.step = w[2].off;
    csn.sPos = w[3].off;
    csn.sVel = w[4].off;
    csn.sPrev = w[5].off;
    csn.sBound = w[6].off;
    csn.stab = w[7].off;
    csn.str = w[8].off;
    csn.pivot = w[9].off;
    csn.head = w[10].off;
    csn.leanAng = w[11].off;
    csn.leanH = w[12].off;
    csn.leanPivot = w[13].off;
    csn.colDir = w[14].off;
    csn.bumpH = w[15].off;
    csn.bumpLast = w[16].off;
    csn.vt = w[17].off;
    csn.nonAddPos = w[18].off;
    csn.status = w[19].off;
    csn.lastDif = w[20].off;
    csn.radius = w[21].off;
    csn.height = w[22].off;
    csn.tickTag = w[23].off;
    csn.pass = w[24].off;
    csn.dominant = w[25].off;
    csn.actOff = w[26].off; csn.actMask = w[26].mask;
    csn.slpOff = w[27].off; csn.slpMask = w[27].mask;
    csn.waitOff = w[28].off; csn.waitMask = w[28].mask;
    csn.colOff = w[29].off; csn.colMask = w[29].mask;
    csn.extOff = w[30].off; csn.extMask = w[30].mask;
    csn.compOff = w[31].off; csn.compMask = w[31].mask;
    csn.telOff = w[32].off; csn.telMask = w[32].mask;
    csn.cenOff = w[33].off; csn.cenMask = w[33].mask;
    csn.smOff = w[34].off; csn.smMask = w[34].mask;
    csn.uncOff = w[35].off; csn.uncMask = w[35].mask;
    csn.sPrevVel = w[36].off;
    csn.allowOff = w[37].off; csn.allowMask = w[37].mask;
    Log("camera/census: resolved by name - influence w+0x%x acc+0x%x step+0x%x active+0x%x/0x%x sleeping+0x%x/0x%x "
        "wait+0x%x/0x%x | DisSpringPoint pos+0x%x vel+0x%x prev+0x%x bound+0x%x | PhysicalReact stab+0x%x str+0x%x pivot+0x%x | "
        "Lean head+0x%x angle+0x%x height+0x%x pivot+0x%x collideDir+0x%x collided+0x%x/0x%x extforce+0x%x/0x%x | "
        "Bump h+0x%x last+0x%x comp+0x%x/0x%x | camera VT+0x%x nonAdditivePos+0x%x teleported+0x%x/0x%x collisionOn+0x%x/0x%x "
        "smoothing+0x%x/0x%x uncovered+0x%x/0x%x status+0x%x lastDif+0x%x radius+0x%x height+0x%x tickTag+0x%x pass+0x%x | "
        "group dominant+0x%x. A 0 is UNRESOLVED (m_Pos alone is legitimately +0x0) and prints `?` below.",
        csn.w, csn.acc, csn.step, csn.actOff, csn.actMask, csn.slpOff, csn.slpMask, csn.waitOff, csn.waitMask,
        csn.sPos, csn.sVel, csn.sPrev, csn.sBound, csn.stab, csn.str, csn.pivot,
        csn.head, csn.leanAng, csn.leanH, csn.leanPivot, csn.colDir, csn.colOff, csn.colMask, csn.extOff, csn.extMask,
        csn.bumpH, csn.bumpLast, csn.compOff, csn.compMask, csn.vt, csn.nonAddPos, csn.telOff, csn.telMask,
        csn.cenOff, csn.cenMask, csn.smOff, csn.smMask, csn.uncOff, csn.uncMask, csn.status, csn.lastDif,
        csn.radius, csn.height, csn.tickTag, csn.pass, csn.dominant);
}

// A bool bit: 1/0, or -1 when unresolved or unreadable.
int CsnBit(const uint8_t* o, uint32_t off, uint32_t mask)
{
    if (!o || !off || !mask || !RangeReadable(o + off, 4)) return -1;
    return (*(const uint32_t*)(o + off) & mask) ? 1 : 0;
}
float CsnF(const uint8_t* o, uint32_t off)
{
    float v = NAN;
    if (o && off && RangeReadable(o + off, 4)) memcpy(&v, o + off, 4);
    return v;
}
bool CsnV(const uint8_t* o, uint32_t off, float v[3], bool zeroOk = false)
{
    v[0] = v[1] = v[2] = NAN;
    if (!o || (!off && !zeroOk) || !RangeReadable(o + off, 12)) return false;
    memcpy(v, o + off, 12);
    return true;
}
float CsnLen(const float v[3]) { return sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
char CsnBitCh(int b) { return b < 0 ? '?' : char('0' + b); }

// Append printf-style, never past the end; a line that fills stops cleanly.
void CsnAp(char* b, int& n, int cap, const char* fmt, ...)
{
    if (n >= cap - 1) return;
    va_list ap; va_start(ap, fmt);
    const int k = _vsnprintf(b + n, cap - n, fmt, ap);
    va_end(ap);
    n = (k < 0 || n + k >= cap) ? cap - 1 : n + k;
    b[n] = 0;
}

// One spring point: position, speed, previous position, bound position.
void CsnPoint(char* b, int& n, int cap, const char* tag, const uint8_t* inf, uint32_t pointOff)
{
    float p[3], v[3], pv[3], bd[3];
    if (!pointOff || !csn.sVel || !CsnV(inf, pointOff + csn.sPos, p, true) || !CsnV(inf, pointOff + csn.sVel, v, true)) {
        CsnAp(b, n, cap, " %s=?", tag); return;
    }
    CsnAp(b, n, cap, " %s=(%.1f,%.1f,%.1f)v%.0f", tag, p[0], p[1], p[2], CsnLen(v));
    if (csn.sPrev && CsnV(inf, pointOff + csn.sPrev, pv, true)) CsnAp(b, n, cap, "p(%.1f,%.1f,%.1f)", pv[0], pv[1], pv[2]);
    if (csn.sBound && CsnV(inf, pointOff + csn.sBound, bd, true)) CsnAp(b, n, cap, "b(%.0f,%.0f,%.0f)", bd[0], bd[1], bd[2]);
}

// The flags every influence carries: weight, active, sleeping, wait, accumulated delta, fixed step.
void CsnFlags(char* b, int& n, int cap, const char* name, const uint8_t* inf)
{
    CsnAp(b, n, cap, " | %s w%.2f a%c s%c b%c acc%.4f st%.4f", name, CsnF(inf, csn.w),
          CsnBitCh(CsnBit(inf, csn.actOff, csn.actMask)), CsnBitCh(CsnBit(inf, csn.slpOff, csn.slpMask)),
          CsnBitCh(CsnBit(inf, csn.waitOff, csn.waitMask)), CsnF(inf, csn.acc), CsnF(inf, csn.step));
}

// Is this spring point away from rest? REST IS m_Pos == m_BoundPos, not m_Pos == 0: read
// offline 2026-09-22 (ENGINE_NOTES "VR-165: the PhysicalReact spring, read offline") - the
// reset (vtable +0x12C) sets pos = bound and velocity 0, the integrator pulls pos toward
// bound, and the at-rest test the engine sleeps on is |pos - bound| and |vel| under an
// epsilon. Used for the `off-rest=` summary: |pos - bound| or |vel| above 1 uu (or uu/s).
// 1 uu is a reading threshold (a hundredth of a metre), not a camera constant, and the raw
// numbers are on the same line.
bool CsnOffRest(const uint8_t* inf, uint32_t pointOff)
{
    float p[3], v[3], b[3] = { 0, 0, 0 };
    if (!pointOff || !csn.sVel || !CsnV(inf, pointOff + csn.sPos, p, true) || !CsnV(inf, pointOff + csn.sVel, v, true)) return false;
    if (csn.sBound) CsnV(inf, pointOff + csn.sBound, b, true);
    const float d[3] = { p[0] - b[0], p[1] - b[1], p[2] - b[2] };
    return CsnLen(d) > 1.0f || CsnLen(v) > 1.0f;
}

// ---- VR-165: reproduce on demand (`camspring`) ---------------------------------
// The seam word runs on the present thread; the springs belong to the script lane,
// where the camera update also runs. A request crosses as one flag and is applied on
// the script lane, exactly as `camshake capture` does. Default: nothing is written.
struct CsnInflKey { const char* key; const char* cls; };
const CsnInflKey kCsnInfl[] = {
    { "physreact", "DishonoredCamera_PhysicalReact" }, { "hitreact", "DishonoredCamera_HitReact" },
    { "shake", "DishonoredCamera_Shake" }, { "recoil", "DishonoredCamera_Recoil" }, { "lean", "DishonoredCamera_Lean" },
};
enum CsnMode { kCsnKick = 1, kCsnNudge = 2, kCsnRest = 3 };
struct CsnRequest {
    std::atomic<int> pending{0};
    int mode = 0; int infl = 0; char point[8] = ""; float v[3] = {}; double watchS = 4.0;
} csnReq;
// The watch after a write: samples every 100 ms and ends in one VERDICT line.
struct CsnWatch {
    bool on = false; int mode = 0; double startMs = 0, endMs = 0, lastOutMs = 0, sleptMs = -1;
    uint8_t* inf = nullptr; uint32_t pointOff = 0; char label[64] = "";
    float p0[3] = {}, e0[3] = {}; bool e0ok = false;
    float peakSpring = 0, peakEye = 0; unsigned samples = 0;
} csnW;

// The base camera minus the pawn, with the mod's own eye/position offset taken out
// (camera.cpp current_base): the game's camera alone. False when any read fails.
bool CsnGameEye(uint8_t* cam, uint8_t* pawn, float out[3])
{
    float base[3], loc[3];
    const uint32_t lo = RflOffsetOf("Actor", "Location");
    if (!cam || !pawn || !dvr::camera::game_base_pos(cam, base) || !CsnV(pawn, lo, loc)) return false;
    for (int k = 0; k < 3; ++k) out[k] = base[k] - loc[k];
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}

} // namespace

// Sample the real source graph on the script lane, never from Present.
// Includes healthy samples and unresolved reads; no threshold can hide a result.
static void CameraSourceTick()
{
    // A `camspring` watch samples at 100 ms whatever the probe switch says: it was asked for.
    if (!g_cmOn && !csnW.on) return;
    static double next=0;
    static unsigned samples=0;
    const double now=MaimNowMs();
    if(now<next) return;
    next=now+(csnW.on ? 100 : 500);
    uint8_t* cam=g_camObj; uint8_t* pawn=g_pePawn;
    if(!cam || !pawn) return;
    // The verbose tables stop at 1800 samples (15 min); the spring census below
    // does not, so a displacement late in a session is still seen. A kick watch
    // prints the census only: ten verbose tables a second would bury it.
    const bool full=samples<1800 && !csnW.on;
    ++samples;
    // The live set is a full GObjects copy and sort; at the watch's 100 ms it is
    // reused for up to 500 ms (VR-160's bounded refresh) instead of rebuilt.
    if(!(csnW.on ? RefreshLiveSet(500) : BuildLiveSet()) || !IsLiveObject(cam) || !IsLiveObject(pawn)) {
        Log("camera/source: sample=%u unavailable: live camera/pawn validation failed",samples); return;
    }
    auto scalar = [](uint8_t* o,const char* cls,const char* prop) -> float {
        float v=NAN; const uint32_t off=RflOffsetOf(cls,prop);
        if(o && off && RangeReadable(o+off,4)) memcpy(&v,o+off,4);
        return v;
    };
    auto vector = [](uint8_t* o,const char* cls,const char* prop,float* v) {
        v[0]=v[1]=v[2]=NAN; const uint32_t off=RflOffsetOf(cls,prop);
        if(!o || !off || !RangeReadable(o+off,12)) return false;
        memcpy(v,o+off,12);
        return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
    };
    float pov[3]={NAN,NAN,NAN},loc[3],vel[3];
    const bool povRead=g_ctLayout && g_ctCache && CtRead(cam,g_ctCache+g_ctPov+g_ctLoc,pov,sizeof(pov));
    const bool povOk=povRead && std::isfinite(pov[0]) && std::isfinite(pov[1]) && std::isfinite(pov[2]);
    const bool locOk=vector(pawn,"Actor","Location",loc);
    vector(pawn,"Actor","Velocity",vel);
    const bool ownerOk=IsLiveObject(g_peCtrl) && g_ctPcCamera && g_ctPawn &&
        CtObject(g_peCtrl,g_ctPcCamera)==cam && CtObject(g_peCtrl,g_ctPawn)==pawn;
    const bool deltaOk=povOk && locOk && ownerOk;
    float delta[3]={NAN,NAN,NAN};
    if(deltaOk) for(int k=0;k<3;++k) delta[k]=pov[k]-loc[k];
    const dvr::anim::Snapshot state=dvr::anim::snapshot();
    if(full) Log("camera/source: sample=%u time=%.0f cam=%p pawn=%p state=%s owner-match=%d "
        "pov-read=%d pawn-read=%d camera-world=%.2f/%.2f/%.2f pawn-world=%.2f/%.2f/%.2f "
        "eye-delta=%s %.2f/%.2f/%.2f velocity=%.2f/%.2f/%.2f EyeHeight=%.2f BaseEyeHeight=%.2f; "
        "sequential cache snapshot, freshness unverified; 500ms samples are not frequency evidence",
        samples,now,cam,pawn,state.state[0],int(ownerOk),int(povOk),int(locOk),
        pov[0],pov[1],pov[2],loc[0],loc[1],loc[2],deltaOk?"computed":"UNAVAILABLE",
        delta[0],delta[1],delta[2],vel[0],vel[1],vel[2],
        scalar(pawn,"Pawn","EyeHeight"),scalar(pawn,"Pawn","BaseEyeHeight"));
    uint8_t* groups=NULL; int32_t count=0;
    const uint32_t off=RflOffsetOf("DishonoredPlayerCamera","m_InfluenceGroups");
    if(!off || !RflArrayAt(cam,off,&groups,&count) || count>16 ||
        (count && !RangeReadable(groups,count*sizeof(void*)))) {
        Log("camera/source: influence groups unavailable count=%d",count); return;
    }
    if(full) Log("camera/source: groups=%d (separate from Camera.ModifierList)",count);
    // VR-165: THE SPRING CENSUS. Measured 2026-09-22: in the bugged state the final
    // POV sat 125-281 uu from the pawn while PlayerControl's own source POV read a
    // normal 77 uu, so the displacement is ADDED after the base camera. The additive
    // influences that keep state are the PhysicalReact springs (PhysicalReact,
    // HitReact, Shake, Recoil: m_StabilityPoint / m_StrengthPoint), Lean's
    // m_HeadPoint and BumpSmoother's interpolated height. Prediction: in a bugged
    // window one of them holds a non-zero position (or keeps moving) that a healthy
    // window never shows, and a reload returns it to rest. If every spring reads
    // at rest while the eye offset is large, the springs are eliminated.
    // WIDENED 2026-09-22 (plan step 1): every stateful influence's active/sleeping/
    // wait bits, fixed-step accumulator, and each spring's previous and bound
    // position; Lean's pivot and collided flag; the camera's own collision
    // smoothing and its non-additive position. Rows are collected here and printed
    // after the walk, two lines per sample (camera/springs, camera/collide).
    CsnResolve();
    uint8_t* stateful[6]={}; uint8_t* reactionGroup=NULL;   // PhysicalReact, HitReact, Shake, Recoil, Lean, BumpSmoother
    static const char* const kStatefulCls[6]={"DishonoredCamera_PhysicalReact","DishonoredCamera_HitReact",
        "DishonoredCamera_Shake","DishonoredCamera_Recoil","DishonoredCamera_Lean","DishonoredCamera_BumpSmoother"};
    for(int g=0;g<count;++g) {
        uint8_t* group=((uint8_t**)groups)[g];
        if(!group) { if(full) Log("camera/source: sample=%u group=%d obj=%p EMPTY null slot",samples,g,group); continue; }
        if(!IsLiveObject(group)) { Log("camera/source: sample=%u group=%d obj=%p REJECTED non-null pointer not live",samples,g,group); continue; }
        uint8_t* influences=NULL; int32_t n=0;
        const uint32_t io=RflOffsetOf("DishonoredCameraInfluenceGroup","m_Influences");
        if(!io || !RflArrayAt(group,io,&influences,&n) || n>64 ||
           (n && !RangeReadable(influences,n*sizeof(void*)))) {
            Log("camera/source: group=%d influences unavailable n=%d",g,n); continue;
        }
        if(full) Log("camera/source: group=%d obj=%p weight=%g influences=%d",g,group,
            scalar(group,"DishonoredCameraInfluenceGroup","m_Weight"),n);
        for(int i=0;i<n;++i) {
            uint8_t* inf=((uint8_t**)influences)[i];
            if(!IsLiveObject(inf)) { Log("camera/source: group=%d row=%d not live",g,i); continue; }
            const char* cls=ObjClassName(inf);
            for(int k=0;k<6 && cls;++k) if(!stateful[k] && !strcmp(cls,kStatefulCls[k])) { stateful[k]=inf; reactionGroup=group; }
            float source[3]={NAN,NAN,NAN}; const char* field="not exposed";
            if(cls && (!strcmp(cls,"DishonoredCamera_PlayerControl") || !strcmp(cls,"DishonoredCamera_AnimDriven"))) field="m_Debug_POV_Location";
            else if(cls && !strcmp(cls,"DishonoredCamera_CrouchMantleOffset")) field="m_StartingOffset";
            else if(cls && !strcmp(cls,"DisCamera_StepUpMantleOffset")) field="m_StepUpStartPos";
            const bool sourceOk=strcmp(field,"not exposed") && vector(inf,cls,field,source);
            const bool worldPov=!strcmp(field,"m_Debug_POV_Location");
            float relative[3]={NAN,NAN,NAN};
            const bool relativeOk=worldPov && sourceOk && locOk && ownerOk;
            if(relativeOk) for(int k=0;k<3;++k) relative[k]=source[k]-loc[k];
            if(worldPov && full) Log("camera/source: sample=%u group=%d row=%d source-world=%.2f/%.2f/%.2f "
                "source-minus-pawn=%s %.2f/%.2f/%.2f (debug field freshness unverified)",
                samples,g,i,source[0],source[1],source[2],relativeOk?"computed":"UNAVAILABLE",
                relative[0],relative[1],relative[2]);
            if(full) Log("camera/source: group=%d row=%d obj=%p class=%s weight=%g target=%g %s=%.2f/%.2f/%.2f",
                g,i,inf,cls?cls:"unavailable",scalar(inf,"DishonoredCameraInfluence","m_Weight"),
                scalar(inf,"DishonoredCameraInfluence","m_TargetWeight"),field,source[0],source[1],source[2]);
        }
    }
    const float eye=deltaOk ? sqrtf(delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]) : NAN;
    // The game's own camera minus the pawn, the mod's eye/position offset taken back
    // out. `eye` above is the field as read, which carries our offset while it persists.
    float ge[3]; const bool geOk=CsnGameEye(cam,pawn,ge);
    // Line 1: the springs. Per influence: weight, a(ctive) s(leeping) b(wait-to-blend)
    // bits, the fixed-step accumulator and step; per spring point: pos, speed,
    // p(revious pos), b(ound pos). off-rest names every point above 1 uu or 1 uu/s.
    static const char* const kShort[6]={"Phys","Hit","Shake","Recoil","Lean","Bump"};
    char ln[1000]; int n=0;
    CsnAp(ln,n,sizeof(ln),"camera/springs: sample=%u state=%s eye=%.1f game-eye=",samples,state.state[0],eye);
    if(geOk) CsnAp(ln,n,sizeof(ln),"(%.1f,%.1f,%.1f)%.1f",ge[0],ge[1],ge[2],CsnLen(ge)); else CsnAp(ln,n,sizeof(ln),"UNAVAILABLE");
    char offRest[96]; int orN=0; offRest[0]=0;
    for(int k=0;k<4;++k) {
        if(!stateful[k]) { CsnAp(ln,n,sizeof(ln)," | %s absent",kShort[k]); continue; }
        CsnFlags(ln,n,sizeof(ln),kShort[k],stateful[k]);
        CsnPoint(ln,n,sizeof(ln),"stab",stateful[k],csn.stab);
        CsnPoint(ln,n,sizeof(ln),"str",stateful[k],csn.str);
        if(CsnOffRest(stateful[k],csn.stab)) CsnAp(offRest,orN,sizeof(offRest)," %s.stab",kShort[k]);
        if(CsnOffRest(stateful[k],csn.str)) CsnAp(offRest,orN,sizeof(offRest)," %s.str",kShort[k]);
    }
    if(stateful[4] && CsnOffRest(stateful[4],csn.head)) CsnAp(offRest,orN,sizeof(offRest)," Lean.head");
    CsnAp(ln,n,sizeof(ln)," | off-rest:%s",offRest[0]?offRest:" none");
    Log("%s",ln);
    // Line 2: the camera's collision smoothing, its non-additive position (what the
    // core group produced before the reaction group is added), Lean and BumpSmoother.
    n=0;
    CsnAp(ln,n,sizeof(ln),"camera/collide: sample=%u teleported%c collisionOn%c smoothing%c allowPopSmooth%c uncovered%c status=",
        samples,CsnBitCh(CsnBit(cam,csn.telOff,csn.telMask)),CsnBitCh(CsnBit(cam,csn.cenOff,csn.cenMask)),
        CsnBitCh(CsnBit(cam,csn.smOff,csn.smMask)),CsnBitCh(CsnBit(cam,csn.allowOff,csn.allowMask)),
        CsnBitCh(CsnBit(cam,csn.uncOff,csn.uncMask)));
    if(csn.status && RangeReadable(cam+csn.status,1)) CsnAp(ln,n,sizeof(ln),"%u",(unsigned)cam[csn.status]); else CsnAp(ln,n,sizeof(ln),"?");
    CsnAp(ln,n,sizeof(ln)," lastDif=%.1f radius=%.1f height=%.1f",CsnF(cam,csn.lastDif),CsnF(cam,csn.radius),CsnF(cam,csn.height));
    if(csn.tickTag && csn.pass && RangeReadable(cam+csn.tickTag,4) && RangeReadable(cam+csn.pass,4))
        CsnAp(ln,n,sizeof(ln)," tick=%d pass=%d",*(int32_t*)(cam+csn.tickTag),*(int32_t*)(cam+csn.pass));
    float na[3];
    if(csn.vt && csn.nonAddPos && CsnV(cam,csn.vt+csn.nonAddPos,na)) {
        CsnAp(ln,n,sizeof(ln)," nonAdditive=(%.1f,%.1f,%.1f)",na[0],na[1],na[2]);
        float base[3];
        if(dvr::camera::game_base_pos(cam,base))
            CsnAp(ln,n,sizeof(ln)," game-minus-nonAdditive=(%.1f,%.1f,%.1f)",base[0]-na[0],base[1]-na[1],base[2]-na[2]);
        if(locOk) CsnAp(ln,n,sizeof(ln)," nonAdditive-minus-pawn=(%.1f,%.1f,%.1f)",na[0]-loc[0],na[1]-loc[1],na[2]-loc[2]);
    } else CsnAp(ln,n,sizeof(ln)," nonAdditive=?");
    if(reactionGroup && csn.dominant && RangeReadable(reactionGroup+csn.dominant,4))
        CsnAp(ln,n,sizeof(ln)," | reaction dominant=%d",*(int32_t*)(reactionGroup+csn.dominant));
    if(uint8_t* lean=stateful[4]) {
        CsnFlags(ln,n,sizeof(ln),"Lean",lean);
        CsnPoint(ln,n,sizeof(ln),"head",lean,csn.head);
        float lp[3],cd[3]; CsnV(lean,csn.leanPivot,lp); CsnV(lean,csn.colDir,cd);
        CsnAp(ln,n,sizeof(ln)," angle=%.2f height=%.1f pivot=(%.1f,%.1f,%.1f) collided%c extforce%c collideDir=(%.2f,%.2f,%.2f)",
            CsnF(lean,csn.leanAng),CsnF(lean,csn.leanH),lp[0],lp[1],lp[2],CsnBitCh(CsnBit(lean,csn.colOff,csn.colMask)),
            CsnBitCh(CsnBit(lean,csn.extOff,csn.extMask)),cd[0],cd[1],cd[2]);
    } else CsnAp(ln,n,sizeof(ln)," | Lean absent");
    if(uint8_t* bump=stateful[5]) {
        CsnFlags(ln,n,sizeof(ln),"Bump",bump);
        CsnAp(ln,n,sizeof(ln)," h=%.1f last=%.1f compensating%c",CsnF(bump,csn.bumpH),CsnF(bump,csn.bumpLast),
            CsnBitCh(CsnBit(bump,csn.compOff,csn.compMask)));
    } else CsnAp(ln,n,sizeof(ln)," | Bump absent");
    Log("%s",ln);
    // One WARN per episode, so a report can be found by grep without a timestamp.
    // By TIME, not sample count: a kick watch samples five times as often.
    static double highSince=0; static bool warned=false;
    if(std::isfinite(eye) && eye>115.0f) {
        if(highSince==0) highSince=now;
        if(!warned && now-highSince>=1500.0) { warned=true;
            DVR_WARN("camera/displaced: the camera has sat %.0f uu from the pawn for 1.5 s (game-eye %.0f uu, the mod's "
                     "own offset removed) - the VR-165 state. The camera/springs and camera/collide lines around this "
                     "one say which influence holds it.",eye,geOk?CsnLen(ge):NAN); } }
    else if(std::isfinite(eye) && eye<105.0f) { if(warned) Log("camera/displaced: back to %.0f uu - episode over",eye); highSince=0; warned=false; }
    // The kick watch: how far the kicked point and the camera went, and whether both came back.
    if(csnW.on) {
        float p[3],v[3]; bool ok=IsLiveObject(csnW.inf) && CsnV(csnW.inf,csnW.pointOff+csn.sPos,p,true) && CsnV(csnW.inf,csnW.pointOff+csn.sVel,v,true);
        ++csnW.samples;
        float dp=NAN,de=NAN,db=NAN;
        const int slp=CsnBit(csnW.inf,csn.slpOff,csn.slpMask);
        if(ok) {
            const float d[3]={p[0]-csnW.p0[0],p[1]-csnW.p0[1],p[2]-csnW.p0[2]}; dp=CsnLen(d);
            if(dp>csnW.peakSpring) csnW.peakSpring=dp;
            float b[3]={0,0,0}; if(csn.sBound) CsnV(csnW.inf,csnW.pointOff+csn.sBound,b,true);
            const float r[3]={p[0]-b[0],p[1]-b[1],p[2]-b[2]}; db=CsnLen(r);
            if(db>1.0f || CsnLen(v)>1.0f) csnW.lastOutMs=now;
            if(slp==1 && csnW.sleptMs<0) csnW.sleptMs=now-csnW.startMs;
            if(slp==0) csnW.sleptMs=-1;
        }
        if(geOk && csnW.e0ok) { const float d[3]={ge[0]-csnW.e0[0],ge[1]-csnW.e0[1],ge[2]-csnW.e0[2]}; de=CsnLen(d); if(de>csnW.peakEye) csnW.peakEye=de; }
        Log("camspring: watch %s t=%.0f ms point-from-start=%.1f uu point-from-bound=%.2f uu speed=%.1f sleeping=%c game-eye-from-start=%.1f uu",
            csnW.label,now-csnW.startMs,dp,db,ok?CsnLen(v):NAN,CsnBitCh(slp),de);
        if(!ok || now>=csnW.endMs) {
            csnW.on=false;
            // A rest write has nothing to move; its question is only whether the point stays at its bound.
            const bool honoured=csnW.mode==kCsnRest || csnW.peakSpring>1.0f;
            const bool settled=ok && db<=1.0f && CsnLen(v)<=1.0f;
            Log("camspring: VERDICT %s - %s. The point went %.1f uu (peak) and %s (%.2f uu from its bound, speed %.1f; the "
                "engine's own sleep bit %s); the game camera went %.1f uu (peak) and ended %.1f uu from where it started; last "
                "more than 1 uu or 1 uu/s off rest at +%.0f ms. Read: a point that never moved is a write the engine did not "
                "honour; a point that settled, slept, and left the camera where it was is a healthy spring (evidence AGAINST it "
                "holding VR-165); a point, a sleep bit or a camera that stays out is the bug reproduced. The camera figure is "
                "only clean with the head still (the simulator).",
                csnW.label,!ok?"ENDED EARLY: the influence is no longer live or unreadable":!honoured?"NOT HONOURED":settled?"SETTLED":"STUCK",
                csnW.peakSpring,settled?"came back":"did NOT come back",db,ok?CsnLen(v):NAN,
                csnW.sleptMs>=0?"set again":"NOT set",csnW.peakEye,de,
                csnW.lastOutMs>0?csnW.lastOutMs-csnW.startMs:0.0);
        }
    }
    if(samples==1800) Log("camera/source: verbose tables stop at 1800 samples; the camera/springs census continues");
}

// Called from the script lane next to the other per-tick readers.
// VR-165 plan step 2: apply a `camspring` request on the script lane, the lane the
// camera update runs on, so the write lands between two updates and never inside one.
static void CamSpringApply()
{
    if (!csnReq.pending.exchange(0)) return;
    const CsnInflKey& key = kCsnInfl[csnReq.infl];
    uint8_t* cam = g_camObj; uint8_t* pawn = g_pePawn;
    if (!RflNamesReady()) { Log("camspring: refused - the name table is not ready yet"); return; }
    CsnResolve();
    if (!cam || !BuildLiveSet() || !IsLiveObject(cam)) { Log("camspring: refused - no live player camera"); return; }
    const bool lean = !strcmp(key.key, "lean");
    if (lean != !strcmp(csnReq.point, "head") && csnReq.point[0]) { Log("camspring: refused - point '%s' does not belong to %s (lean has head; the others stab|str)", csnReq.point, key.key); return; }
    uint32_t pointOff = lean ? csn.head : !strcmp(csnReq.point, "str") ? csn.str : csn.stab;
    const char* pointName = lean ? "head" : !strcmp(csnReq.point, "str") ? "str" : "stab";
    if (!pointOff || !csn.sVel) { Log("camspring: refused - %s.%s did not resolve by name (point +0x%x, m_Velocity +0x%x)", key.key, pointName, pointOff, csn.sVel); return; }
    // Find the influence on the live camera by class, as camshake does.
    uint8_t* inf = nullptr; uint8_t* groups = nullptr; int32_t ng = 0;
    const uint32_t go = RflOffsetOf("DishonoredPlayerCamera", "m_InfluenceGroups"), io = RflOffsetOf("DishonoredCameraInfluenceGroup", "m_Influences");
    if (go && io && RflArrayAt(cam, go, &groups, &ng) && ng > 0 && ng <= 16 && RangeReadable(groups, ng * sizeof(void*)))
        for (int g = 0; g < ng && !inf; ++g) {
            uint8_t* grp = ((uint8_t**)groups)[g]; uint8_t* arr = nullptr; int32_t ni = 0;
            if (!grp || !IsLiveObject(grp) || !RflArrayAt(grp, io, &arr, &ni) || ni <= 0 || ni > 64 || !RangeReadable(arr, ni * sizeof(void*))) continue;
            for (int i = 0; i < ni && !inf; ++i) {
                uint8_t* o = ((uint8_t**)arr)[i];
                const char* cn = o && IsLiveObject(o) ? ObjClassName(o) : nullptr;
                if (cn && !strcmp(cn, key.cls)) inf = o;
            }
        }
    if (!inf) { Log("camspring: refused - no %s on the live camera %p", key.cls, cam); return; }
    // Which points: a named one, or (unnamed) both for the PhysicalReact family, as the
    // engine's own impulse does (0xAC10F0 adds v to BOTH points' velocities).
    const bool both = !lean && !csnReq.point[0];
    uint32_t pts[2] = { pointOff, 0 }; int np = 1;
    if (both) { pts[0] = csn.str; pts[1] = csn.stab; np = 2; pointName = "str+stab"; }
    for (int j = 0; j < np; ++j)
        if (!pts[j] || !RangeReadable(inf + pts[j], 60)) { Log("camspring: refused - %s point +0x%x unreadable", key.key, pts[j]); return; }
    // The watch follows the first point: StrengthPoint when both, because the apply step
    // (vtable +0x140) emits StrengthPoint.m_Pos minus the rotated pivot as the location.
    float* pos = (float*)(inf + pts[0] + csn.sPos); float* vel = (float*)(inf + pts[0] + csn.sVel);
    const float before[6] = { pos[0], pos[1], pos[2], vel[0], vel[1], vel[2] };
    const int sleptBefore = CsnBit(inf, csn.slpOff, csn.slpMask);
    CsnWatch& w = csnW;
    w = CsnWatch();
    memcpy(w.p0, pos, 12);
    w.e0ok = CsnGameEye(cam, pawn, w.e0);
    const char* what = "";
    for (int j = 0; j < np; ++j) {
        float* pp = (float*)(inf + pts[j] + csn.sPos); float* vv = (float*)(inf + pts[j] + csn.sVel);
        if (csnReq.mode == kCsnKick) for (int k = 0; k < 3; ++k) vv[k] += csnReq.v[k];
        else if (csnReq.mode == kCsnNudge) for (int k = 0; k < 3; ++k) pp[k] += csnReq.v[k];
        else if (csn.sBound) {
            // rest, exactly as the engine's reset (+0x12C) leaves a point: pos = bound,
            // velocity 0, previous pos = pos, previous velocity 0.
            memcpy(pp, inf + pts[j] + csn.sBound, 12);
            for (int k = 0; k < 3; ++k) vv[k] = 0.0f;
            if (csn.sPrev) memcpy(inf + pts[j] + csn.sPrev, pp, 12);
            if (csn.sPrevVel) memset(inf + pts[j] + csn.sPrevVel, 0, 12);
        }
    }
    what = csnReq.mode == kCsnKick ? "kick (velocity += v, uu/s)" : csnReq.mode == kCsnNudge ? "nudge (position += v, uu)"
         : csn.sBound ? "rest (pos = bound, velocity 0, previous = pos: the engine's own reset)" : "rest REFUSED (m_BoundPos unresolved)";
    // A sleeping influence is SKIPPED by the group update (0xACFE74 tests bit 2), so a write
    // into one does nothing until it wakes. The engine's impulse clears the bit; so do we.
    if (csnReq.mode != kCsnRest && csn.slpOff && csn.slpMask && RangeReadable(inf + csn.slpOff, 4))
        *(uint32_t*)(inf + csn.slpOff) &= ~csn.slpMask;
    const float wgt = CsnF(inf, csn.w);
    Log("camspring: %s %s.%s on %s %p - before pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) sleeping=%c, v=(%.1f,%.1f,%.1f); "
        "influence weight %.2f%s. Watching %.1f s at 100 ms; a VERDICT line ends it.",
        what, key.key, pointName, key.cls, inf, before[0], before[1], before[2], before[3], before[4], before[5],
        CsnBitCh(sleptBefore), csnReq.v[0], csnReq.v[1], csnReq.v[2], wgt,
        wgt <= 1e-8f ? " - AT OR UNDER THE GROUP'S SKIP THRESHOLD (1e-8, read from the exe): the engine does not run this influence at all (camshake "
                       "holds it? `camshake allow <cat> on` first)" : "",
        csnReq.watchS);
    pointOff = pts[0];
    w.inf = inf; w.pointOff = pointOff; w.mode = csnReq.mode; w.on = true;
    _snprintf_s(w.label, sizeof(w.label), _TRUNCATE, "%s.%s", key.key, pointName);
    w.startMs = MaimNowMs(); w.endMs = w.startMs + 1000.0 * csnReq.watchS;
}

// ---- VR-165: THE FIX - no collision-pop glide in VR ----------------------------------
// The cause (FLICKER_REFERENCE "VR-165: CAUSE FOUND", ENGINE_NOTES "the collision-pop
// smoother reads back the mod's offset"): after a collision pop over 50 uu the camera
// sets m_bSmoothingSuddenCollision and glides back, starting each update from the final
// location it reads back from camera+0x330 - the field the mod writes the head and eye
// offset into. Our offset re-enters every update, the glide settles at 9.5x our offset
// (measured) and never ends.
//
// The engine only STARTS that glide when m_bAllowCamSmoothingForCollisionPop is set (the
// game's own DishonoredCamera.ini switch; the start test is at 0xAD8757). Held clear, a
// pop snaps instead of gliding for a few frames - in a headset a glide the head did not
// make is unrequested motion anyway, the reasoning of VR-172's shake removal - and the
// stuck state cannot arise. A glide already running is ended by clearing
// m_bSmoothingSuddenCollision, which is what the engine's own reset does (0xAD886E).
// Not a clamp: no position is written, only the game's own two bits.
//
// [CameraShake] PopSmoothing: 1 = the game's own glide (the compiled default, per the
// default-OFF lever rule), 0 = the fix. Live: `camshake allow popsmooth on|off`, F10.
// Script lane, the lane the camera update runs on. The slow tick (250 ms) validates the
// camera; the per-dispatch cost is two bit tests.
std::atomic<bool> g_popSmoothAllow{true};
struct PopFix {
    uint8_t* cam = nullptr; bool held = false; bool origAllow = true;
    unsigned rewrites = 0, glidesEnded = 0; double nextSlow = 0.0, nextBeat = 0.0;
} g_pf;

static void CamPopSmoothTick()
{
    const bool fix = !g_popSmoothAllow.load();
    if (!fix && !g_pf.held) return;
    const double now = MaimNowMs();
    if (now >= g_pf.nextSlow) {
        g_pf.nextSlow = now + 250.0;
        if (!RflNamesReady()) return;
        CsnResolve();
        RefreshLiveSet(2000);                          // bounded (VR-160)
        uint8_t* cam = g_camObj;
        const char* cn = cam && IsLiveObject(cam) ? ObjClassName(cam) : nullptr;
        if (!cn || !strstr(cn, "PlayerCamera")) cam = nullptr;
        if (cam != g_pf.cam) {
            if (g_pf.cam) Log("camera/popsmooth: the player camera changed (%p -> %p); nothing written to the old one", g_pf.cam, cam);
            g_pf.cam = cam; g_pf.held = false;
        }
    }
    uint8_t* cam = g_pf.cam;
    if (!cam || !csn.allowOff || !csn.allowMask || !csn.smMask || csn.smOff != csn.allowOff ||
        !RangeReadable(cam + csn.allowOff, 4)) {
        if (fix) DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 30000,
            "camera/popsmooth: the fix is ON but cannot act - camera %p, allow bit +0x%x/0x%x, smoothing bit +0x%x/0x%x "
            "(both must resolve by name, in one word)", cam, csn.allowOff, csn.allowMask, csn.smOff, csn.smMask);
        return;
    }
    uint32_t* bits = (uint32_t*)(cam + csn.allowOff);
    if (!fix) {
        // Released: the game's own value back, once.
        if (g_pf.origAllow) *bits |= csn.allowMask;
        Log("camera/popsmooth: released - m_bAllowCamSmoothingForCollisionPop back to %d (the game's own glide); held "
            "for %u rewrite(s), %u running glide(s) ended", (int)g_pf.origAllow, g_pf.rewrites, g_pf.glidesEnded);
        g_pf.held = false;
        return;
    }
    if (!g_pf.held) {
        g_pf.held = true; g_pf.origAllow = (*bits & csn.allowMask) != 0;
        Log("camera/popsmooth: TAKEN on camera %p - m_bAllowCamSmoothingForCollisionPop was %d, held at 0; a collision pop "
            "now snaps instead of gliding, so the VR-165 stuck glide cannot start ([CameraShake] PopSmoothing=0)",
            cam, (int)g_pf.origAllow);
    }
    if (*bits & csn.allowMask) { *bits &= ~csn.allowMask; ++g_pf.rewrites; }
    if (*bits & csn.smMask) {
        // A glide was running: one already under way when the fix was taken, or one the
        // engine started despite the switch - which would be a result, so it is counted.
        *bits &= ~csn.smMask; ++g_pf.glidesEnded;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "camera/popsmooth: ended a running collision-pop glide (m_bSmoothingSuddenCollision cleared, as the engine's "
            "own reset does) - %u so far. More than one while the switch is held means the engine starts it another way.",
            g_pf.glidesEnded);
    }
    if (now >= g_pf.nextBeat) {
        g_pf.nextBeat = now + 30000.0;
        Log("camera/popsmooth: beat owner=the mod (fix ON) camera=%p allow-bit rewrites=%u glides ended=%u | rewrites stay "
            "at 1 (the take) unless the engine re-reads its config; camera/collide's smoothing must read 0 throughout",
            cam, g_pf.rewrites, g_pf.glidesEnded);
    }
}

static void CamModTick()
{
    CamPopSmoothTick();   // VR-165: the fix (a no-op unless [CameraShake] PopSmoothing=0)
    CamSpringApply();
    CameraSourceTick();
    if (!g_cmOn) return;
    uint8_t* cam = g_camObj;
    if (!cam || !IsLiveObject(cam)) return;
    CmResolve();
    if (!g_cmOffList) return;      // nothing to walk; CmResolve already said so

    const double now = MaimNowMs();

    // Track the swing itself, so the table can be printed WITH the thing it is
    // meant to explain rather than next to it. A reversal only counts when the
    // step is big enough to be motion rather than noise.
    // CORRECTED after run 2. This used to count a reversal per ProcessEvent
    // tick, and ProcessEvent runs far faster than any camera motion - so it
    // reported "249 reversals in 1010 ms", which is not a frequency of
    // anything and cannot be compared against the ~8 Hz the offline analysis
    // measured. A counter whose units are "per dispatch" is not evidence about
    // motion.
    //
    // Count DIRECTION CHANGES OF A REAL EXCURSION instead: a reversal is only
    // counted once the camera has travelled kCmTurnUu since the last one, so
    // the number is half-cycles of actual movement and dividing by the window
    // gives a rate in Hz that means something. Sampling rate no longer changes
    // the answer.
    float z = 0.0f;
    if (CmCameraZ(cam, &z)) {
        if (g_cmHaveZ) {
            // REGRESSION FIXED. The first version of this updated g_cmExtreme
            // toward z on EVERY sample, in whichever direction z had just
            // moved. That made the extreme track z, so `z - extreme` was always
            // about zero and could never cross the threshold: the detector
            // counted nothing and the probe printed no table at all during a
            // reproduction the user confirmed. A counter that cannot fire is
            // worse than the noisy one it replaced, and this one shipped
            // untested because the swing did not occur on the run after it.
            //
            // The extreme belongs to the LEG, and only ever moves further OUT:
            // on a rising leg it is the highest z seen, on a falling leg the
            // lowest. A turn is z retreating from it by more than kCmTurnUu.
            if (g_cmLeg > 0) { if (z > g_cmExtreme) g_cmExtreme = z; }
            else if (g_cmLeg < 0) { if (z < g_cmExtreme) g_cmExtreme = z; }
            else {
                // No leg yet: adopt one as soon as there is real movement.
                if (z > g_cmExtreme + kCmTurnUu) { g_cmLeg = 1; g_cmExtreme = z; }
                else if (z < g_cmExtreme - kCmTurnUu) { g_cmLeg = -1; g_cmExtreme = z; }
            }
            if (g_cmLeg > 0 && z < g_cmExtreme - kCmTurnUu) {
                ++g_cmReversals; g_cmLeg = -1; g_cmExtreme = z;
            } else if (g_cmLeg < 0 && z > g_cmExtreme + kCmTurnUu) {
                ++g_cmReversals; g_cmLeg = 1; g_cmExtreme = z;
            }
            if (z < g_cmZMin) g_cmZMin = z;
            if (z > g_cmZMax) g_cmZMax = z;
        } else { g_cmZMin = g_cmZMax = g_cmExtreme = z; g_cmWindowMs = now; g_cmLeg = 0; }
        g_cmLastZ = z; g_cmHaveZ = true;
    }

    if (now < g_cmNextMs) return;
    g_cmNextMs = now + kCmPeriodMs;

    const double win = now - g_cmWindowMs;
    const float  amp = g_cmZMax - g_cmZMin;
    const int    rev = g_cmReversals;
    // Reset the window for the next second.
    g_cmReversals = 0; g_cmWindowMs = now; g_cmZMin = g_cmZMax = g_cmLastZ;

    // ~8 Hz with tens of uu was the measured chain swing; walking was far below
    // it. Print while swinging, and once more when it stops, so the release is
    // visible too - the release is the half that would identify an owner.
    const double hz = win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0;
    const bool swinging = (hz >= 2.0 && amp > 15.0f);
    if (swinging) { g_cmLastMoveMs = now; ++g_cmSwingRuns; }
    const bool justStopped = !swinging && g_cmSwingRuns > 0 && (now - g_cmLastMoveMs) < 3000.0;
    // The detector must be able to say it is alive. Its first version counted
    // nothing and printed nothing, and "no tables" read exactly like "no
    // swing" - so a broken instrument looked like a clean run. This line makes
    // silence explainable: it reports what the watcher is actually seeing, and
    // the peak it has seen, so a threshold that is never reached is visible
    // rather than invisible.
    if (g_cmPeakHz < hz) g_cmPeakHz = hz;
    if (g_cmPeakAmp < amp) g_cmPeakAmp = amp;
    if (now >= g_cmBeatMs) {
        g_cmBeatMs = now + 30000.0;
        Log("cammod: watching - this window %.1f Hz / %.1f uu; peak since the last beat "
            "%.1f Hz / %.1f uu; a table prints at >= 2.0 Hz AND > 15 uu. %s",
            hz, amp, g_cmPeakHz, g_cmPeakAmp,
            g_cmPrinted ? "Tables have printed this session."
                        : "NO table has printed yet this session.");
        g_cmPeakHz = 0.0; g_cmPeakAmp = 0.0f;
    }
    if (!swinging && !justStopped) { if (!swinging) g_cmSwingRuns = 0; return; }
    g_cmPrinted = true;
    if (!swinging) g_cmSwingRuns = 0;

    uint8_t* data = NULL; int32_t num = 0;
    if (!RflArrayAt(cam, g_cmOffList, &data, &num) || !data || num <= 0 || num > 64) {
        Log("cammod: %s - ModifierList unreadable or empty (num=%d) on camera %p; "
            "the swing cannot be attributed to a modifier from here",
            swinging ? "SWINGING" : "settled", (int)num, (void*)cam);
        return;
    }

    Log("cammod: ---- %s: %.1f Hz (%d turns of >%.0f uu in %.0f ms), %.1f uu of camera Z, %d modifier(s) ----",
        swinging ? "SWINGING" : "SETTLED (the swing just stopped)",
        win > 0.0 ? (rev * 0.5) / (win / 1000.0) : 0.0, rev, kCmTurnUu, win, amp, (int)num);
    for (int i = 0; i < num; ++i) {
        uint8_t* m = *(uint8_t**)(data + i * sizeof(void*));
        if (!m || ((uintptr_t)m & 3) || !RangeReadable(m, kClassOff + 4)) {
            Log("cammod:   [%d] unreadable entry %p", i, (void*)m);
            continue;
        }
        const char* cn = ObjClassName(m);
        char alpha[24] = "unresolved", targ[24] = "unresolved", prio[16] = "unresolved";
        char dis[16] = "unresolved";
        if (g_cmOffAlpha  && RangeReadable(m + g_cmOffAlpha, 4))
            _snprintf(alpha, sizeof(alpha), "%.3f", *(const float*)(m + g_cmOffAlpha));
        if (g_cmOffTarget && RangeReadable(m + g_cmOffTarget, 4))
            _snprintf(targ, sizeof(targ), "%.3f", *(const float*)(m + g_cmOffTarget));
        if (g_cmOffPrio   && RangeReadable(m + g_cmOffPrio, 1))
            _snprintf(prio, sizeof(prio), "%u", (unsigned)*(const uint8_t*)(m + g_cmOffPrio));
        if (g_cmOffDisabled && g_cmMaskDisabled && RangeReadable(m + g_cmOffDisabled, 4))
            _snprintf(dis, sizeof(dis), "%d",
                      (*(const uint32_t*)(m + g_cmOffDisabled) & g_cmMaskDisabled) ? 1 : 0);
        Log("cammod:   [%d] %-44s alpha=%s target=%s prio=%s disabled=%s",
            i, cn ? cn : "(no class name)", alpha, targ, prio, dis);
    }
    Log("cammod: ---- end. A modifier whose alpha stays UP on the SETTLED line is not the "
        "owner; the owner is one that is still up while SWINGING and falls when it settles. "
        "Every alpha reading 'unresolved' means read the resolver line above, not this table. ----");
}

static void CamModConfigure(const char* ini)
{
    g_cmOn = IniFloat(ini, "Diagnostics", "CamModProbe", 1) != 0.0f;
    Log("cammod: camera-modifier probe %s ([Diagnostics] CamModProbe) - read-only, prints only "
        "while the camera is actually swinging", g_cmOn ? "ON" : "off");
}

static bool CamModCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        g_cmOn = b;
        ConfigWriteKey("Diagnostics", "CamModProbe", b ? "1" : "0", "the seam");
        Log("cammod: %s (seam)", b ? "ON" : "off");
        return true;
    }
    Log("cammod: on|off (now %s). Read-only: it names which camera modifier is still "
        "weighted while the camera swings, and writes nothing.", g_cmOn ? "ON" : "off");
    return true;
}

// `camspring kick <influence> [stab|str] <x> <y> <z> [seconds]` adds to one spring
// point's velocity; `nudge` adds to its position; `rest <influence> [stab|str]` zeroes
// it. Influences: physreact hitreact shake recoil lean (lean has one point, head).
// Nothing is written unless asked; the write lands on the script lane.
static bool CamSpringCommand(const char* args)
{
    char sub[16] = {}, inf[24] = {}, pt[16] = {};
    float v[3] = {}, secs = 4.0f;
    const char* a = args ? args : "";
    int got = sscanf(a, "%15s %23s", sub, inf);
    int mode = !strcmp(sub, "kick") ? kCsnKick : !strcmp(sub, "nudge") ? kCsnNudge : !strcmp(sub, "rest") ? kCsnRest : 0;
    int ik = -1;
    for (int i = 0; i < (int)(sizeof(kCsnInfl) / sizeof(kCsnInfl[0])); ++i) if (got == 2 && !strcmp(inf, kCsnInfl[i].key)) ik = i;
    if (mode && ik >= 0) {
        // The point word is optional: a number where it would be means "stab".
        char rest[128] = {}; const char* p = strstr(a, inf) + strlen(inf);
        strncpy_s(rest, p, _TRUNCATE);
        int k = sscanf(rest, "%15s", pt);
        const bool named = k == 1 && (!strcmp(pt, "stab") || !strcmp(pt, "str") || !strcmp(pt, "head"));
        if (!named) pt[0] = 0;   // unnamed: both points (the engine's impulse), or Lean's one head point
        const char* nums = named ? strstr(rest, pt) + strlen(pt) : rest;
        const int nv = sscanf(nums, "%f %f %f %f", &v[0], &v[1], &v[2], &secs);
        if (mode != kCsnRest && nv < 3) { Log("camspring: %s needs x y z (uu/s for kick, uu for nudge)", sub); return true; }
        if (mode == kCsnRest && nv >= 1) secs = v[0];
        if (mode == kCsnRest) v[0] = v[1] = v[2] = 0.0f;
        if (csnReq.pending.load()) { Log("camspring: refused - the last request has not been applied yet (no script-lane tick?)"); return true; }
        csnReq.mode = mode; csnReq.infl = ik; strcpy_s(csnReq.point, pt);
        memcpy(csnReq.v, v, sizeof(v));
        csnReq.watchS = secs < 0.5f ? 0.5 : secs > 30.0f ? 30.0 : secs;
        csnReq.pending.store(1);
        return true;
    }
    Log("camspring: kick <inf> [stab|str] <x> <y> <z> [secs] | nudge <inf> [stab|str] <x> <y> <z> [secs] | rest <inf> [stab|str] [secs]. "
        "inf: physreact hitreact shake recoil lean. No point named = both points, as the engine's own impulse (0xAC10F0) does. "
        "kick/nudge also clear m_bIsSleeping (a sleeping influence is skipped by the group, so a write into one is otherwise "
        "ignored); rest puts pos = bound, velocity 0 (the engine's reset). The spring's axes are the influence's local frame: "
        "the apply step rotates by the view's yaw. Writes only when asked; the watch (100 ms census + a VERDICT line) says "
        "whether the point, the sleep bit and the camera came back.%s",
        csnW.on ? " A watch is running now." : "");
    return true;
}
