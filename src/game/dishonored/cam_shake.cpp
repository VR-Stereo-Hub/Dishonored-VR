// VR-172: the game's own camera shake, measured and (by default) removed.
// Included after cam_modifiers.cpp: it reuses cinematic_trace's resolved camera
// cache layout (g_ct*) and the name-keyed resolver.
//
// WHAT THE PLAYER FEELS. The game moves the camera by itself: a bob and a roll
// while walking and running, a kick when a weapon fires, a jolt on a sword hit, on
// damage, near an explosion, on landing. On a monitor that is feedback. In a
// headset it is the view moving without the head, which is the one thing a VR
// camera must not do.
//
// WHERE IT LIVES. Not in UE3's camera modifiers: Camera.ModifierList holds one
// CameraModifier_CameraShake whose alpha reads 0 in every sample ever taken.
// Dishonored's camera is built from Arkane "influences":
//   DishonoredPlayerCamera.m_InfluenceGroups -> group.m_Influences ->
//   DishonoredCameraInfluence { m_Weight, m_TargetWeight, m_bActive }
// and the reaction group holds BumpSmoother, PhysicalReact, HitReact, Lean, Shake,
// Recoil, DisCamera_Rumble and DisCamera_Aim. The walking bob is separate again:
// config floats on the camera itself (m_BobAmount, m_RollAmount), beside
// m_fReactionWeight.
//
// THE REACTION INFLUENCES SIT AT WEIGHT 1 ALL THE TIME (every sample of every run):
// an influence is always weighted and only produces motion when the game triggers
// it. So a weight can never say WHICH influence moved the camera. Attribution is an
// A/B: hold one handle at zero, repeat the same action, and see whether the motion
// is gone. That is what `camshake hold` and `camshake capture` are for, and it is
// why the instrument and the feature are the same code.
//
// WHAT EACH HANDLE OWNS, MEASURED ON THE SIMULATOR 2026-09-21 (ENGINE_NOTES "VR-172"
// has the table and the runs). Same staged action, one handle held at zero:
//   landing from a jump   camera dips 45.8 uu and overshoots 8.5   -> PhysicalReact
//   pushing off a jump    camera lags the pawn 10.4 uu for 90 ms   -> BumpSmoother (the stair
//                         smoother doing its job; kept by default, its own category)
//   HitReact              nothing stageable on the simulator reached it. An earlier
//                         reading gave it the push-off; that capture's window had opened
//                         after the takeoff and never saw one. Retracted.
//   a pistol shot         2.84 deg of pitch, 1.0 uu                -> Recoil
//   walking / running     bob and roll                             -> m_BobAmount, m_RollAmount
//                         (already 0 here: the game's head-bob option drives them;
//                         held at 3 the walk swings 12 uu and rolls 9 deg, so live)
//   Shake, Rumble         nothing stageable on the simulator reached them
//   m_fReactionWeight     a master over the whole reaction group: at 0 the landing,
//                         the push-off and the shot are all gone. NOT used by the
//                         feature, because that group also holds Lean, Aim and the
//                         stair smoother. It stays an instrument handle only.
// The engine never rewrote a held weight (one write each, zero fought), so a hold is
// one float written once and then compared.
//
// WHAT THIS DOES NOT REMOVE. With every handle at zero a walk still moves the camera
// 1.5 uu up and down and standing still 0.4 uu, on a half-second period. That height
// matches the PlayerControl influence's own source position: it is the animated
// first-person body the camera rides on, not a shake system, and no handle here owns
// it. VR-175 carries it.
//
// WHAT IS NEVER TOUCHED. DishonoredCamera_AnimDriven, _PlayerControl, DisCamera_Look,
// _Lean, DisCamera_Aim, BumpSmoother and the whole mantle/arm group: they carry
// mantles, takedowns, the keyhole, leaning, stairs and the neck pivot the [Neck]
// cancel is calibrated against. The handle table below is the complete list of
// what this file can write.
//
// LANE: the script lane. CamShakeTick is beside the other per-dispatch readers; the
// capture is fed from ApplyHeadToViewRotation's FRESH branch, once per game tick.
#include <atomic>
#include <cmath>

namespace {

enum CsKind { kCsInfluence, kCsCamFloat };
// cat: the player-facing category a handle belongs to. Filled from measurement.
struct CsHandle {
    const char* key; CsKind kind; const char* name; const char* cat;
    float want = -1.0f;            // < 0 = not held
    uint8_t* obj = nullptr;        // the influence, or the camera for a float
    uint32_t off = 0;              // the float's offset (kCsCamFloat)
    bool have = false; float orig = 0.0f, origTarget = 0.0f;
    unsigned writes = 0, fought = 0;   // fought: the engine had changed our value since the last write
};
CsHandle csH[] = {
    { "shake",     kCsInfluence, "DishonoredCamera_Shake",         "generic" },
    { "recoil",    kCsInfluence, "DishonoredCamera_Recoil",        "fire" },
    { "hitreact",  kCsInfluence, "DishonoredCamera_HitReact",      "hits" },
    { "physreact", kCsInfluence, "DishonoredCamera_PhysicalReact", "landing" },
    { "rumble",    kCsInfluence, "DisCamera_Rumble",               "generic" },
    { "bob",       kCsCamFloat,  "m_BobAmount",                    "walk" },
    { "roll",      kCsCamFloat,  "m_RollAmount",                   "walk" },
    { "reaction",  kCsCamFloat,  "m_fReactionWeight",              "master" },   // instrument only: no category holds it
    { "bump",      kCsInfluence, "DishonoredCamera_BumpSmoother",  "smoother" }, // its category ships ALLOWED: see csCat
};
constexpr int kCsHandles = sizeof(csH) / sizeof(csH[0]);

// The player-facing categories. `allow` = let the game's own motion through.
// `measured` is printed beside each one, so a category the simulator could not
// reach never reads as proven.
struct CsCategory { const char* key; const char* ini; const char* label; const char* measured; bool allowDefault = false; bool allow = false; };
CsCategory csCat[] = {
    { "walk",    "Walk",    "walking and running (bob and roll)",             "measured: live, and already 0 through the game's head-bob option" },
    { "fire",    "Fire",    "firing a weapon (the kick)",                     "measured: a pistol shot, 2.84 deg of pitch" },
    { "landing", "Landing", "landing and physical impulses",                  "measured: a jump landing, 45.8 uu; explosions are by the influence's name only" },
    { "hits",    "Hits",    "hits and jolts",                                 "NOT measured: nothing stageable on the simulator reached it; damage taken is by the influence's name only" },
    { "generic", "Generic", "the game's general shake and rumble",            "NOT measured: nothing stageable on the simulator reached these two" },
    // The stair smoother is NOT a shake: it makes the camera glide over a sudden step
    // instead of snapping with the pawn. It ships allowed. Removing it is for judging
    // in a headset: the one place it reads as unrequested motion is a jump's push-off.
    { "smoother", "Smoother", "the stair and step smoother (on a jump: the push-off lag)", "measured: a jump push-off, the camera lags the pawn 10.4 uu for 90 ms", true },
};
constexpr int kCsCats = sizeof(csCat) / sizeof(csCat[0]);
float csManual[kCsHandles] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // `camshake hold`: the instrument's override, < 0 = none

struct CsLayout { bool resolved = false; uint32_t groups = 0, infl = 0, weight = 0, target = 0, velocity = 0, location = 0; } csL;
uint8_t* csCam = nullptr;          // the camera the cached pointers belong to
double   csNextSlow = 0.0;
char     csStandDown[96] = "";     // why nothing is being held right now ("" = holding)

// ---- the capture ---------------------------------------------------------------
struct CsRow {
    float ms; int32_t in[3], prev[3], delta[3]; int32_t headYaw; uint8_t havePrev;
    float base[3], pawn[3], vel[3]; int32_t pov[3], ctrl[3]; uint8_t baseOk, stateIdx;
};
constexpr int kCsRows = 6000;      // about a minute of game ticks
struct CsCapture {
    bool on = false; double startMs = 0, lenMs = 0; int n = 0; char tag[32] = "";
    unsigned skipped = 0; unsigned serial = 0;
    char states[8][64]; int nStates = 0;
} csC;
CsRow* csRows = nullptr;           // allocated on the first capture, never per sample
// The seam word runs on the PRESENT thread; the capture lives on the script lane.
// A request crosses over as one flag, and the script lane is the only writer of csC.
struct CsRequest { std::atomic<int> pending{0}; double seconds = 5.0; char tag[32] = ""; } csReq;

int cs_state_idx(const char* s) {
    for (int i = 0; i < csC.nStates; ++i) if (!strcmp(csC.states[i], s)) return i;
    if (csC.nStates >= 8) return 7;
    strncpy_s(csC.states[csC.nStates], s, _TRUNCATE);
    return csC.nStates++;
}

void cs_resolve() {
    if (csL.resolved || !RflNamesReady()) return;
    csL.resolved = true;
    csL.groups   = RflOffsetOf("DishonoredPlayerCamera", "m_InfluenceGroups");
    csL.infl     = RflOffsetOf("DishonoredCameraInfluenceGroup", "m_Influences");
    csL.weight   = RflOffsetOf("DishonoredCameraInfluence", "m_Weight");
    csL.target   = RflOffsetOf("DishonoredCameraInfluence", "m_TargetWeight");
    csL.velocity = RflOffsetOf("Actor", "Velocity");
    csL.location = RflOffsetOf("Actor", "Location");
    for (CsHandle& h : csH) if (h.kind == kCsCamFloat) h.off = RflOffsetOf("DishonoredPlayerCamera", h.name);
    Log("camshake: layout by name - camera m_InfluenceGroups +0x%x, group m_Influences +0x%x, influence m_Weight +0x%x "
        "m_TargetWeight +0x%x | camera m_BobAmount +0x%x m_RollAmount +0x%x m_fReactionWeight +0x%x | Actor Location +0x%x "
        "Velocity +0x%x (0 = UNRESOLVED: that handle refuses and says so)", csL.groups, csL.infl, csL.weight, csL.target,
        csH[5].off, csH[6].off, csH[7].off, csL.location, csL.velocity);   // bob, roll, reaction: their rows in csH
}

// Forget every cached pointer WITHOUT writing: the camera they belonged to is gone.
void cs_forget(const char* why) {
    bool any = false;
    for (CsHandle& h : csH) { any = any || h.obj; h.obj = nullptr; h.have = false; }
    if (any) Log("camshake: released every handle without a write - %s", why);
    csCam = nullptr;
}

// The slow tick: is the camera the same live object, and where are the influences?
void cs_find(uint8_t* cam) {
    for (CsHandle& h : csH) if (h.kind == kCsCamFloat) h.obj = h.off ? cam : nullptr;
    uint8_t* groups = nullptr; int32_t ng = 0;
    if (!csL.groups || !csL.infl || !RflArrayAt(cam, csL.groups, &groups, &ng) || ng <= 0 || ng > 16 ||
        !RangeReadable(groups, ng * sizeof(void*))) return;
    for (int g = 0; g < ng; ++g) {
        uint8_t* grp = ((uint8_t**)groups)[g];
        if (!grp || !IsLiveObject(grp)) continue;
        uint8_t* infl = nullptr; int32_t ni = 0;
        if (!RflArrayAt(grp, csL.infl, &infl, &ni) || ni <= 0 || ni > 64 || !RangeReadable(infl, ni * sizeof(void*))) continue;
        for (int i = 0; i < ni; ++i) {
            uint8_t* o = ((uint8_t**)infl)[i];
            if (!o || !IsLiveObject(o)) continue;
            const char* cn = ObjClassName(o);
            if (!cn) continue;
            for (CsHandle& h : csH)
                if (h.kind == kCsInfluence && !strcmp(cn, h.name)) { if (h.obj != o) { h.obj = o; h.have = false; } }
        }
    }
}

float* cs_value(CsHandle& h) {
    if (!h.obj) return nullptr;
    const uint32_t off = h.kind == kCsInfluence ? csL.weight : h.off;
    return off && RangeReadable(h.obj + off, 4) ? (float*)(h.obj + off) : nullptr;
}
void cs_restore(CsHandle& h, const char* why) {
    float* v = cs_value(h);
    if (h.have && v) {
        *v = h.orig;
        if (h.kind == kCsInfluence && csL.target && RangeReadable(h.obj + csL.target, 4)) *(float*)(h.obj + csL.target) = h.origTarget;
        Log("camshake: %s released (%s): restored %.3f after %u write(s), %u of them over an engine rewrite", h.key, why, h.orig, h.writes, h.fought);
    }
    h.have = false; h.writes = 0; h.fought = 0;
}

// The fast path: floats only, through pointers the slow tick validated.
void cs_hold() {
    for (CsHandle& h : csH) {
        if (h.want < 0.0f) { if (h.have) cs_restore(h, "no longer held"); continue; }
        float* v = cs_value(h);
        if (!v) continue;
        if (!h.have) {
            h.have = true; h.orig = *v;
            h.origTarget = (h.kind == kCsInfluence && csL.target && RangeReadable(h.obj + csL.target, 4)) ? *(float*)(h.obj + csL.target) : *v;
            Log("camshake: %s (%s %s) taken: was %.3f, held at %.3f", h.key, h.kind == kCsInfluence ? "influence" : "camera float", h.name, h.orig, h.want);
        } else if (*v != h.want) ++h.fought;
        if (*v != h.want) { *v = h.want; ++h.writes; }
        if (h.kind == kCsInfluence && csL.target && RangeReadable(h.obj + csL.target, 4)) *(float*)(h.obj + csL.target) = h.want;
    }
}

// ---- capture: summary and CSV ------------------------------------------------------
int32_t cs_wrap(int32_t u) { u &= 0xffff; return u > 32767 ? u - 65536 : u; }
struct CsSpan { float lo = 1e30f, hi = -1e30f; void add(float v) { if (v < lo) lo = v; if (v > hi) hi = v; } float pp() const { return hi >= lo ? hi - lo : 0.0f; } };

void cs_finish(const char* why) {
    csC.on = false;
    const int n = csC.n;
    if (n < 8 || !csRows) { Log("camshake: capture '%s' ended (%s) with %d row(s): too few to say anything", csC.tag, why, n); return; }
    // High-pass: each value against the mean of its +-0.25 s neighbourhood, so a walk
    // across the room (metres of travel) does not read as shake. What is left is the
    // motion FASTER than half a second, which is what a shake is.
    CsSpan posHp[3], rotIn[3], povMinusCtrl[3]; double speedSum = 0; int baseRows = 0;
    for (int i = 0; i < n; ++i) {
        const CsRow& r = csRows[i];
        if (r.havePrev) for (int a = 0; a < 3; ++a) {
            int32_t d = cs_wrap(r.in[a] - r.prev[a]);
            if (a == 1) d = cs_wrap(d - r.delta[1]);         // yaw: minus the stick's own turn, if Parms carries it
            rotIn[a].add((float)d * (360.0f / 65536.0f));
        }
        for (int a = 0; a < 3; ++a) povMinusCtrl[a].add((float)cs_wrap(r.pov[a] - r.ctrl[a]) * (360.0f / 65536.0f));
        speedSum += sqrt((double)r.vel[0] * r.vel[0] + (double)r.vel[1] * r.vel[1]);
        if (!r.baseOk) continue;
        ++baseRows;
        double m[3] = {}; int k = 0;
        for (int j = i; j >= 0 && r.ms - csRows[j].ms <= 250.0f; --j) if (csRows[j].baseOk) { for (int a = 0; a < 3; ++a) m[a] += csRows[j].base[a] - csRows[j].pawn[a]; ++k; }
        for (int j = i + 1; j < n && csRows[j].ms - r.ms <= 250.0f; ++j) if (csRows[j].baseOk) { for (int a = 0; a < 3; ++a) m[a] += csRows[j].base[a] - csRows[j].pawn[a]; ++k; }
        for (int a = 0; a < 3; ++a) posHp[a].add((float)((r.base[a] - r.pawn[a]) - m[a] / k));
    }
    char held[200] = ""; int at = 0;
    for (CsHandle& h : csH) if (h.want >= 0.0f && at < (int)sizeof(held) - 24) at += _snprintf_s(held + at, sizeof(held) - at, _TRUNCATE, " %s=%.2f", h.key, h.want);
    char path[MAX_PATH]; _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\camshake-%s-%u.csv", dvr::paths::dumps_dir(), csC.tag, csC.serial);
    FILE* f = nullptr; fopen_s(&f, path, "w");
    if (f) {
        fprintf(f, "ms,inP,inY,inR,prevP,prevY,prevR,havePrev,deltaP,deltaY,deltaR,headYaw,baseX,baseY,baseZ,baseOk,pawnX,pawnY,pawnZ,velX,velY,velZ,povP,povY,povR,ctrlP,ctrlY,ctrlR,state\n");
        for (int i = 0; i < n; ++i) { const CsRow& r = csRows[i];
            fprintf(f, "%.1f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.2f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%s\n",
                r.ms, r.in[0], r.in[1], r.in[2], r.prev[0], r.prev[1], r.prev[2], (int)r.havePrev, r.delta[0], r.delta[1], r.delta[2], r.headYaw,
                r.base[0], r.base[1], r.base[2], (int)r.baseOk, r.pawn[0], r.pawn[1], r.pawn[2], r.vel[0], r.vel[1], r.vel[2],
                r.pov[0], r.pov[1], r.pov[2], r.ctrl[0], r.ctrl[1], r.ctrl[2], csC.states[r.stateIdx < 8 ? r.stateIdx : 7]); }
        fclose(f);
    }
    Log("camshake: CAPTURE '%s' %s - %d game tick(s) over %.1f s (%u dispatch(es) skipped: chain re-stamps, the stereo second pass, "
        "cutscene or menu ownership; by design, they carry no new engine value) | held:%s | mean ground speed %.0f uu/s | "
        "GAME CAMERA POSITION minus pawn, motion faster than 0.5 s, peak to peak: X %.2f Y %.2f Z %.2f uu (%d row(s); 100 uu = 1 m; "
        "the mod's own eye offset is already removed) | ENGINE-ADDED ROTATION arriving at the view-rotation event since our last "
        "write: pitch %.3f yaw %.3f roll %.3f deg peak to peak (yaw is net of the event's own DeltaRot) | camera POV minus controller "
        "rotation: pitch %.3f yaw %.3f roll %.3f deg | rows -> %s",
        csC.tag, why, n, (csRows[n - 1].ms - csRows[0].ms) / 1000.0f, csC.skipped, held[0] ? held : " nothing", speedSum / n,
        posHp[0].pp(), posHp[1].pp(), posHp[2].pp(), baseRows, rotIn[0].pp(), rotIn[1].pp(), rotIn[2].pp(),
        povMinusCtrl[0].pp(), povMinusCtrl[1].pp(), povMinusCtrl[2].pp(), f ? path : "(the CSV could not be written)");
}

} // namespace

static std::atomic<bool> g_camShakeSuppress{true};

// What each handle should be held at right now: the instrument's override first,
// then the feature (0 for a suppressed category), else not held.
static void CamShakePlan(const char* standDown) {
    const bool suppress = g_camShakeSuppress.load() && !(standDown && *standDown);
    for (int i = 0; i < kCsHandles; ++i) {
        float want = csManual[i];
        if (want < 0.0f && suppress)
            for (const CsCategory& c : csCat) if (!strcmp(c.key, csH[i].cat) && !c.allow) want = 0.0f;
        csH[i].want = want;
    }
}

// From ApplyHeadToViewRotation's FRESH branch, once per game tick, BEFORE our write.
static void CamShakeOnViewRot(const int32_t* in, const int32_t* prevWrite, bool havePrev, const int32_t* deltaRot, int32_t headYawU) {
    if (!csC.on || !csRows) return;
    const double now = MaimNowMs();
    if (now - csC.startMs >= csC.lenMs || csC.n >= kCsRows) { cs_finish(csC.n >= kCsRows ? "row budget reached" : "window elapsed"); return; }
    CsRow& r = csRows[csC.n++];
    memset(&r, 0, sizeof(r));
    r.ms = (float)(now - csC.startMs);
    for (int a = 0; a < 3; ++a) { r.in[a] = in[a]; r.prev[a] = prevWrite[a]; r.delta[a] = deltaRot ? deltaRot[a] : 0; }
    r.headYaw = headYawU; r.havePrev = havePrev ? 1 : 0;
    uint8_t* cam = g_camObj; uint8_t* pawn = g_pePawn; uint8_t* ctrl = g_peCtrl;
    r.baseOk = cam && dvr::camera::game_base_pos(cam, r.base) ? 1 : 0;
    if (pawn && csL.location && RangeReadable(pawn + csL.location, 12)) memcpy(r.pawn, pawn + csL.location, 12);
    if (pawn && csL.velocity && RangeReadable(pawn + csL.velocity, 12)) memcpy(r.vel, pawn + csL.velocity, 12);
    if (cam && g_ctLayout && g_ctCache) CtRead(cam, g_ctCache + g_ctPov + g_ctRot, r.pov, sizeof(r.pov));
    if (ctrl && g_ctActorRot && RangeReadable(ctrl + g_ctActorRot, 12)) memcpy(r.ctrl, ctrl + g_ctActorRot, 12);
    const dvr::anim::Snapshot s = dvr::anim::snapshot();
    char st[64]; _snprintf_s(st, sizeof(st), _TRUNCATE, "%s|%s", s.valid ? s.state[0] : "?", s.valid ? s.state[1] : "?");
    r.stateIdx = (uint8_t)cs_state_idx(st);
}
static void CamShakeNoteSkipped() { if (csC.on) ++csC.skipped; }

static void CamShakeTick() {
    const double now = MaimNowMs();
    if (csReq.pending.exchange(0)) {
        if (!csRows) csRows = (CsRow*)calloc(kCsRows, sizeof(CsRow));
        if (!csRows) Log("camshake: capture refused - %d rows could not be allocated", kCsRows);
        else if (!RflNamesReady()) Log("camshake: capture refused - the name table is not ready yet");
        else {
            cs_resolve();
            if (csC.on) cs_finish("a new capture was asked for");
            csC.n = 0; csC.skipped = 0; csC.nStates = 0; ++csC.serial;
            csC.lenMs = 1000.0 * csReq.seconds; strncpy_s(csC.tag, csReq.tag, _TRUNCATE);
            csC.startMs = now; csC.on = true;
            Log("camshake: capture '%s' for %.1f s, one row per game tick from the view-rotation event", csC.tag, csC.lenMs / 1000.0);
        }
    }
    if (csC.on && now - csC.startMs >= csC.lenMs + 500.0) cs_finish("window elapsed (no view-rotation event closed it)");
    // A cutscene owns its camera. The holds are released (each handle gets its own
    // original back) while one runs and taken again after, and the log says so once.
    const char* stand = g_cineNow ? "a cutscene owns the camera" : "";
    if (strcmp(stand, csStandDown)) {
        Log(*stand ? "camshake: STANDING DOWN - %s; every handle gets its original back until it ends"
                   : "camshake: resuming%s - the cutscene ended", stand);
        strncpy_s(csStandDown, stand, _TRUNCATE);
    }
    CamShakePlan(stand);
    bool anyHeld = false;
    for (CsHandle& h : csH) anyHeld = anyHeld || h.want >= 0.0f || h.have;
    static double nextBeat = 0.0;
    if (now >= nextBeat) {
        nextBeat = now + 30000.0;
        char held[160] = ""; int at = 0; unsigned writes = 0, fought = 0, unfound = 0;
        for (CsHandle& h : csH) {
            if (h.want < 0.0f) continue;
            if (!h.obj) ++unfound;
            writes += h.writes; fought += h.fought;
            if (at < (int)sizeof(held) - 20) at += _snprintf_s(held + at, sizeof(held) - at, _TRUNCATE, " %s=%.0f", h.key, h.want);
        }
        Log("camshake: beat owner=%s suppress=%d | held:%s | %u write(s), %u over an engine rewrite, %u handle(s) not found on the live "
            "camera | writes stay low by design: the engine does not rewrite a held weight, so a hold is written once and then only compared",
            *csStandDown ? "the game (standing down)" : anyHeld ? "the mod" : "the game (nothing held)", (int)g_camShakeSuppress.load(),
            held[0] ? held : " nothing", writes, fought, unfound);
    }
    if (!anyHeld) return;
    if (now >= csNextSlow) {
        csNextSlow = now + 250.0;
        if (!RflNamesReady()) return;
        cs_resolve();
        RefreshLiveSet(2000);                           // bounded (VR-160): never a full build per tick
        uint8_t* cam = g_camObj;
        const char* cn = cam && IsLiveObject(cam) ? ObjClassName(cam) : nullptr;
        if (!cn || !strstr(cn, "PlayerCamera")) { cs_forget("no live player camera"); return; }
        if (cam != csCam) { if (csCam) cs_forget("the player camera changed (a level load)"); csCam = cam; }
        cs_find(cam);
    }
    if (!csCam) return;
    cs_hold();
}

static void CamShakeConfigure(const char* ini) {
    g_camShakeSuppress.store(GetPrivateProfileIntA("CameraShake", "Suppress", 1, ini) != 0);
    char line[320] = ""; int at = 0;
    for (CsCategory& c : csCat) {
        c.allow = GetPrivateProfileIntA("CameraShake", c.ini, c.allowDefault ? 1 : 0, ini) != 0;
        at += _snprintf_s(line + at, sizeof(line) - at, _TRUNCATE, " %s=%d", c.ini, (int)c.allow);
    }
    g_popSmoothAllow.store(GetPrivateProfileIntA("CameraShake", "PopSmoothing", 1, ini) != 0);
    Log("config: [CameraShake] Suppress=%d%s PopSmoothing=%d - Suppress=1 removes the game's own camera motion; a category at 1 lets the game's through. "
        "PopSmoothing=0 is the VR-165 fix (no collision-pop glide, which sticks in VR). "
        "'camshake status' says which of them were measured and which are by name only", (int)g_camShakeSuppress.load(), line,
        (int)g_popSmoothAllow.load());
}
static void CamShakeSave(const char* ini) {
    WritePrivateProfileStringA("CameraShake", "Suppress", g_camShakeSuppress.load() ? "1" : "0", ini);
    for (const CsCategory& c : csCat) WritePrivateProfileStringA("CameraShake", c.ini, c.allow ? "1" : "0", ini);
    WritePrivateProfileStringA("CameraShake", "PopSmoothing", g_popSmoothAllow.load() ? "1" : "0", ini);
}
static void CamShakeSuppressSet(bool on, const char* who) {
    g_camShakeSuppress.store(on);
    Log("camshake: suppress=%d by %s (live) - %s", (int)on, who ? who : "?",
        on ? "the game's own camera motion is removed for every category not allowed" : "the game's camera motion is the game's again; every handle gets its original back");
}
static bool CamShakeSuppressed() { return g_camShakeSuppress.load(); }

static void CamShakeReport() {
    Log("camshake: suppress=%d%s%s | popsmooth %s", (int)g_camShakeSuppress.load(), *csStandDown ? " | STANDING DOWN: " : "", csStandDown,
        g_popSmoothAllow.load() ? "ALLOWED (the game's glide; VR-165 can stick)" : "removed (the VR-165 fix)");
    for (const CsCategory& c : csCat)
        Log("camshake: category %-8s %s | %s | %s", c.key, c.allow ? "ALLOWED (the game's own)" : "removed", c.label, c.measured);
    for (CsHandle& h : csH) {
        const float* v = cs_value(h);
        char nowText[16] = "unfound";
        if (v) _snprintf_s(nowText, sizeof(nowText), _TRUNCATE, "%.3f", *v);
        Log("camshake: %-9s %-13s %-32s cat=%-7s now=%s held=%s writes=%u fought=%u%s", h.key, h.kind == kCsInfluence ? "influence" : "camera float",
            h.name, h.cat, nowText, h.want >= 0.0f ? "yes" : "no", h.writes, h.fought,
            h.obj ? "" : "  <-- not found on the live camera yet (a hold finds it within 250 ms)");
    }
}

static bool CamShakeCommand(const char* args) {
    char sub[24] = {}, a[32] = {}, b[32] = {};
    sscanf(args ? args : "", "%23s %31s %31s", sub, a, b);
    bool onOff = false;
    if (DvrOnOff(sub, &onOff)) {
        CamShakeSuppressSet(onOff, "the seam");
        ConfigWriteKey("CameraShake", "Suppress", onOff ? "1" : "0", "the seam");
        return true;
    }
    if (!strcmp(sub, "allow") && !strcmp(a, "popsmooth") && DvrOnOff(b, &onOff)) {
        // VR-165: not a shake category, and never part of `allow all`.
        g_popSmoothAllow.store(onOff);
        ConfigWriteKey("CameraShake", "PopSmoothing", onOff ? "1" : "0", "the seam");
        Log("camshake: allow popsmooth = %d (live) - %s", (int)onOff, onOff ? "the game's collision-pop glide is back (VR-165 can stick again)"
                                                                           : "the VR-165 fix: a collision pop snaps, the glide cannot stick");
        return true;
    }
    if (!strcmp(sub, "allow") && *a && DvrOnOff(b, &onOff)) {
        bool hit = false;
        // "all" is the five kinds of SHAKE. The smoother is not one and is only ever
        // changed by its own name: `allow all off` must not quietly take the stairs with it.
        for (CsCategory& c : csCat) if (!strcmp(a, c.key) || (!strcmp(a, "all") && strcmp(c.key, "smoother"))) {
            c.allow = onOff; hit = true;
            ConfigWriteKey("CameraShake", c.ini, onOff ? "1" : "0", "the seam");
        }
        Log(hit ? "camshake: allow %s = %d (live)" : "camshake: no category called '%s' (walk fire landing hits generic smoother)", a, (int)onOff);
        return true;
    }
    // The instrument: an override on top of the feature, for attribution and for
    // proving a handle live by EXAGGERATION (hold it at 3 and the motion must grow).
    if (!strcmp(sub, "hold") && *a && *b) {
        const float v = (float)atof(b); bool hit = false;
        for (int i = 0; i < kCsHandles; ++i) if (!strcmp(a, "all") || !strcmp(a, csH[i].key) || !strcmp(a, csH[i].cat)) { csManual[i] = v < 0.0f ? 0.0f : v; hit = true; }
        Log(hit ? "camshake: hold %s at %.3f (the instrument's override; until 'camshake release')" : "camshake: no handle or category called '%s'", a, v);
        csNextSlow = 0.0;
        return true;
    }
    if (!strcmp(sub, "release")) {
        for (int i = 0; i < kCsHandles; ++i) if (!*a || !strcmp(a, "all") || !strcmp(a, csH[i].key) || !strcmp(a, csH[i].cat)) csManual[i] = -1.0f;
        Log("camshake: release %s (the override is gone; the feature's own holds stay)", *a ? a : "all");
        return true;
    }
    if (!strcmp(sub, "capture") && *a) {
        csReq.seconds = atof(a) < 0.5 ? 0.5 : atof(a) > 60.0 ? 60.0 : atof(a);
        strncpy_s(csReq.tag, *b ? b : "untagged", _TRUNCATE);
        csReq.pending.store(1);                          // the script lane starts it
        return true;
    }
    if (*sub && strcmp(sub, "status"))
        Log("camshake: on|off | allow <walk|fire|landing|hits|generic|smoother|all> on|off | status | capture <seconds> [tag] | "
            "hold <handle|category|all> <value> | release [handle|category|all] | allow popsmooth on|off (VR-165). Handles: shake recoil hitreact physreact rumble bob roll, bump, and one the feature never holds: reaction");
    CamShakeReport();
    return true;
}

static void CamShakeStatus(dvr::status::Writer& w) {
    w.obj("cameraShake");
    w.kv("suppress", g_camShakeSuppress.load()); w.kv("standingDown", csStandDown[0] != 0);
    for (const CsCategory& c : csCat) w.kv(c.key, c.allow ? "allowed" : "removed");
    unsigned held = 0, unfound = 0, writes = 0, fought = 0;
    for (const CsHandle& h : csH) if (h.want >= 0.0f) { ++held; if (!h.obj) ++unfound; writes += h.writes; fought += h.fought; }
    w.kv("handlesHeld", (unsigned long)held); w.kv("handlesNotFound", (unsigned long)unfound);
    w.kv("writes", (unsigned long)writes); w.kv("engineRewritesFought", (unsigned long)fought);
    w.kv("capturing", csC.on);
    w.kv("popSmoothing", g_popSmoothAllow.load() ? "allowed" : "removed");
    w.end_obj();
}

static void CamShakeDrawUi() {
    if (!ImGui::CollapsingHeader("Camera shake", ImGuiTreeNodeFlags_DefaultOpen)) return;
    bool on = g_camShakeSuppress.load();
    if (ImGui::Checkbox("remove the game's own camera shake (recommended in a headset)", &on)) {
        CamShakeSuppressSet(on, "F10");
        ConfigWriteKey("CameraShake", "Suppress", on ? "1" : "0", "F10");
    }
    if (*csStandDown) ImGui::TextDisabled("standing down: %s", csStandDown);
    ImGui::TextDisabled("Tick a line to let the game move the camera for that again:");
    for (CsCategory& c : csCat) {
        char label[96]; _snprintf_s(label, sizeof(label), _TRUNCATE, "allow: %s", c.label);
        if (ImGui::Checkbox(label, &c.allow)) ConfigWriteKey("CameraShake", c.ini, c.allow ? "1" : "0", "F10");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.measured);
    }
    bool pop = g_popSmoothAllow.load();
    if (ImGui::Checkbox("allow: the collision-pop glide (VR-165: sticks in VR, leave it off)", &pop)) {
        g_popSmoothAllow.store(pop);
        ConfigWriteKey("CameraShake", "PopSmoothing", pop ? "1" : "0", "F10");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("After a chain release or a knockback the game glides the camera back to your head. "
                                                  "In VR that glide reads back the mod's own offset and never finishes: the view stays lifted and swings.");
    ImGui::TextDisabled("Still there with everything removed: about 1.5 cm of body movement while walking.");
}
