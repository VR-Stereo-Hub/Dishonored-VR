// VR-137: camera lens effects (the low-health red vignette, blood, rat bite,
// spit, possession entry), measured and optionally brought toward the eyes.
// Included after rain_control.cpp (it reuses RainFindClassFunction and the
// trace's camera-cache layout).
//
// Every one of these is an EmitterCameraLensEffectBase: a particle actor the
// camera keeps in Camera.CameraLensEffects and re-places each frame through the
// native UpdateLocation(CamLoc, CamRot, CamFOVDeg) at DistFromCamera (default
// 90 uu) along the view. The player pawn's own m_pCurHealthLensEffect is the
// health one. In the headset this is a sheet at a fixed depth in front of both
// eyes.
//
//  1. MEASURE, while [Lens] Trace=1: each live effect's class, DistFromCamera,
//     BaseFOV, DrawScale and its ACTUAL position in the camera frame, logged
//     when the set changes. The measured forward distance against
//     DistFromCamera says whether this build scales the distance by FOV.
//  2. MOVE, with [Lens] Distance > 0 (code default 0 = native): DistFromCamera
//     is written on each live effect. With [Lens] KeepSize=1 the actor is
//     rescaled by the same ratio through the native Actor.SetDrawScale (so the
//     components follow), keeping its angular size; KeepSize=0 lets a nearer
//     effect grow outward into the periphery.
//
// LANE: the script lane, 100 ms. Every sample re-reads the camera's array from
// the live controller. The originals are kept per effect pointer only while
// that pointer is in the camera's array THIS sample; nothing outside the
// array is ever written.
#include <atomic>
#include <cmath>

// Code defaults = the tester's preferred settings (installed470's session, run470
// final state): the lens effects at 2 uu, no rescale, native placement (follow
// off - installed476 tried it on and the tester preferred 470's look).
// Distance=0 gives back the native 90 uu sheet.
static std::atomic<int> g_lensDistUu{2};
static std::atomic<bool> g_lensKeepSize{false}, g_lensTrace{true}, g_lensFollow{false};
static std::atomic<int> g_lensRainPct{100};

static void LensDistanceSet(int uu) {
    if (uu < 0) uu = 0;
    if (uu > 500) uu = 500;
    g_lensDistUu.store(uu);
    Log("lens: distance=%d uu (%s)", uu, uu ? "written to DistFromCamera on every live camera lens effect" : "native, untouched");
}
static void LensKeepSizeSet(bool on) {
    g_lensKeepSize.store(on);
    Log("lens: keepsize=%d (%s)", on ? 1 : 0, on ? "rescale by the distance ratio: same angular size" : "no rescale: a nearer effect spreads outward");
}
static int LensDistance() { return g_lensDistUu.load(); }
static bool LensKeepSize() { return g_lensKeepSize.load(); }
static bool LensTraceEnabled() { return g_lensTrace.load(); }
static void LensFollowSet(bool on) {
    g_lensFollow.store(on);
    Log("lens: followhead=%d (%s)", on ? 1 : 0, on ? "re-placed from each eye's rendered camera before that eye draws"
                                                   : "native: placed once per tick from the game camera");
}
static void LensRainPctSet(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    g_lensRainPct.store(pct);
    Log("lens: rain strength=%d%% (%s)", pct, pct >= 100 ? "native, untouched"
        : "the looping lens effect's own fade weight (its 'Fade' particle parameter) is capped here every draw");
}
static bool LensFollowHead() { return g_lensFollow.load(); }
static int LensRainPct() { return g_lensRainPct.load(); }
static void LensConfigure(const char* ini) {
    g_lensTrace.store(GetPrivateProfileIntA("Lens", "Trace", 1, ini) != 0);
    LensKeepSizeSet(GetPrivateProfileIntA("Lens", "KeepSize", 0, ini) != 0);
    LensDistanceSet(GetPrivateProfileIntA("Lens", "Distance", 2, ini));
    LensFollowSet(GetPrivateProfileIntA("Lens", "FollowHead", 0, ini) != 0);
    LensRainPctSet(GetPrivateProfileIntA("Lens", "RainStrength", 100, ini));
}

namespace {
struct LensOrig { uint8_t* fx; float dist, scale; bool touched; };
LensOrig g_lensOrig[16];
int g_lensOrigN = 0;
}

static void LensTick() {
    const int want = g_lensDistUu.load();
    const bool trace = g_lensTrace.load();
    if (!trace && want <= 0 && !g_lensOrigN) return;
    static unsigned long long next = 0;
    const unsigned long long now = GetTickCount64();
    if (now < next) return;
    next = now + 100;
    if (!RflNamesReady()) return;

    static bool resolved = false;
    static uint32_t pcCamOff = 0, arrOff = 0, distOff = 0, fovOff = 0, scaleOff = 0, locOff = 0,
                    pawnOff = 0, healthOff = 0;
    static uint8_t* fnSetDrawScale = nullptr;
    if (!resolved) {
        resolved = true;
        pcCamOff  = RflOffsetOf("PlayerController", "PlayerCamera");
        arrOff    = RflOffsetOf("Camera", "CameraLensEffects");
        distOff   = RflOffsetOf("EmitterCameraLensEffectBase", "DistFromCamera");
        fovOff    = RflOffsetOf("EmitterCameraLensEffectBase", "BaseFOV");
        scaleOff  = RflOffsetOf("Actor", "DrawScale");
        locOff    = RflOffsetOf("Actor", "Location");
        pawnOff   = RflOffsetOf("Controller", "Pawn");
        healthOff = RflOffsetOf("DishonoredPlayerPawn", "m_pCurHealthLensEffect");
        fnSetDrawScale = RainFindClassFunction("Actor", "SetDrawScale");
        Log("lens: layout pcCamera=+0x%x CameraLensEffects=+0x%x DistFromCamera=+0x%x BaseFOV=+0x%x DrawScale=+0x%x "
            "Location=+0x%x healthLens=+0x%x SetDrawScale=%p%s",
            pcCamOff, arrOff, distOff, fovOff, scaleOff, locOff, healthOff, (void*)fnSetDrawScale,
            (pcCamOff && arrOff && distOff) ? "" : "  <-- a move needs the camera array and DistFromCamera; it will refuse");
    }

    uint8_t* ctrl = IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam = RainPtr(ctrl, pcCamOff);
    if (cam && (!IsLiveObject(cam) || !ObjClassName(cam) || !strstr(ObjClassName(cam), "Camera"))) cam = nullptr;
    uint8_t* data = nullptr; int32_t num = 0;
    if (cam && !RflArrayAt(cam, arrOff, &data, &num)) num = 0;
    if (num > 16) num = 16;

    // The pawn's health lens, for identity only (it should also be in the array).
    uint8_t* pawn = RainPtr(ctrl, pawnOff);
    uint8_t* health = (pawn && IsLiveObject(pawn)) ? RainPtr(pawn, healthOff) : nullptr;
    if (health && !IsLiveObject(health)) health = nullptr;

    for (int i = 0; i < g_lensOrigN; ++i) g_lensOrig[i].touched = false;
    float cloc[3] = {}; int32_t crot[3] = {};
    const bool cOk = cam && g_ctLayout && g_ctCache &&
        RangeReadable(cam + g_ctCache + g_ctPov + g_ctLoc, 12) && RangeReadable(cam + g_ctCache + g_ctPov + g_ctRot, 12) &&
        (memcpy(cloc, cam + g_ctCache + g_ctPov + g_ctLoc, 12), memcpy(crot, cam + g_ctCache + g_ctPov + g_ctRot, 12), true);

    static uint8_t* lastSet[16] = {}; static int lastN = -1;
    bool setChanged = num != lastN;
    for (int i = 0; i < num && !setChanged; ++i) setChanged = lastSet[i] != ((uint8_t**)data)[i];
    // Positions are logged 300 ms after the set (or the lever) changes, once the
    // camera has placed the new effect at least once.
    static unsigned long long logAt = 0; static int lastWant = -1;
    if (setChanged || want != lastWant) { logAt = now + 300; lastWant = want; }
    const bool doLog = trace && logAt && now >= logAt && num > 0;

    for (int i = 0; i < num; ++i) {
        uint8_t* fx = ((uint8_t**)data)[i];
        const char* cn = fx ? ObjClassName(fx) : nullptr;
        if (!fx || !IsLiveObject(fx) || !cn || !strstr(cn, "LensEffect") || !RangeReadable(fx + distOff, 4)) continue;
        float* dist = (float*)(fx + distOff);
        float scale = scaleOff && RangeReadable(fx + scaleOff, 4) ? *(float*)(fx + scaleOff) : -1;
        LensOrig* o = nullptr;
        for (int k = 0; k < g_lensOrigN; ++k) if (g_lensOrig[k].fx == fx) { o = &g_lensOrig[k]; break; }
        if (!o && want > 0 && g_lensOrigN < 16) { o = &g_lensOrig[g_lensOrigN++]; *o = { fx, *dist, scale, false }; }
        if (o) o->touched = true;

        if (want > 0 && o && o->dist > 1.0f) {
            if (*dist != (float)want) *dist = (float)want;
            const float target = LensKeepSize() ? o->scale * (float)want / o->dist : o->scale;
            if (fnSetDrawScale && o->scale > 0 && fabsf(scale - target) > 0.001f * fabsf(target) + 1e-5f) {
                struct { float NewScale; } parms = { target };
                g_peReentry = true;
                ((PFN_ProcessEventCall)kProcessEvent)(fx, fnSetDrawScale, &parms, NULL);
                g_peReentry = false;
            }
        } else if (want <= 0 && o) {
            *dist = o->dist;
            if (fnSetDrawScale && o->scale > 0 && fabsf(scale - o->scale) > 1e-5f) {
                struct { float NewScale; } parms = { o->scale };
                g_peReentry = true;
                ((PFN_ProcessEventCall)kProcessEvent)(fx, fnSetDrawScale, &parms, NULL);
                g_peReentry = false;
            }
            Log("lens: restored %s %p DistFromCamera %.1f DrawScale %.3f", cn, (void*)fx, o->dist, o->scale);
            o->fx = nullptr;   // compacted below
        }

        if (doLog) {
            float f = 0, r = 0, u = 0, dd = -1, eloc[3] = {};
            if (cOk && locOff && RangeReadable(fx + locOff, 12)) {
                memcpy(eloc, fx + locOff, 12);
                const double k = 3.14159265358979 / 32768.0, p = crot[0] * k, y = crot[1] * k;
                const double d[3] = { eloc[0] - cloc[0], eloc[1] - cloc[1], eloc[2] - cloc[2] };
                const double fw[3] = { cos(p) * cos(y), cos(p) * sin(y), sin(p) };
                const double rt[3] = { -sin(y), cos(y), 0 };
                const double up[3] = { fw[1] * rt[2] - fw[2] * rt[1], fw[2] * rt[0] - fw[0] * rt[2], fw[0] * rt[1] - fw[1] * rt[0] };
                f = (float)(d[0] * fw[0] + d[1] * fw[1] + d[2] * fw[2]);
                r = (float)(d[0] * rt[0] + d[1] * rt[1] + d[2] * rt[2]);
                u = (float)(d[0] * up[0] + d[1] * up[1] + d[2] * up[2]);
                dd = (float)sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            }
            const float baseFov = fovOff && RangeReadable(fx + fovOff, 4) ? *(float*)(fx + fovOff) : -1;
            Log("lens/fx: [%d/%d] %s %p%s DistFromCamera=%.1f (native %.1f) BaseFOV=%.1f DrawScale=%.3f | in camera frame "
                "fwd=%.1f right=%.1f up=%.1f dist=%.1f uu%s",
                i, num, cn, (void*)fx, fx == health ? " (the pawn's HEALTH lens)" : "", *dist, o ? o->dist : *dist,
                baseFov, scale, f, r, u, dd,
                dd < 0 ? " (position unavailable)" : " (measured fwd vs DistFromCamera says whether this build scales distance by FOV)");
        }
    }
    // Drop every original whose effect is no longer in the camera's array: it is
    // gone or not ours, and it is never written again.
    int w = 0;
    for (int i = 0; i < g_lensOrigN; ++i) if (g_lensOrig[i].fx && g_lensOrig[i].touched) g_lensOrig[w++] = g_lensOrig[i];
    g_lensOrigN = w;
    if (doLog || (logAt && now >= logAt)) logAt = 0;
    if (setChanged) {
        lastN = num;
        for (int i = 0; i < num; ++i) lastSet[i] = ((uint8_t**)data)[i];
        if (trace) Log("lens: camera %p holds %d lens effect(s); health lens %p", (void*)cam, num, (void*)health);
    }
}

// VR-137 follow-head. The camera places every lens effect ONCE per tick through
// the native UpdateLocation(CamLoc, CamRot, FOV), from the game camera before the
// mod writes each eye's position (the eye field is the cache POV location,
// +0x330). At 1 uu that sheet is fixed in the world while each eye sits 3 uu to
// one side of it and the tracked head moves: it does not follow the head. Here
// the same native call runs again right before each eye's draw, from that eye's
// rendered cache POV, so the effect sits where the game would put it for the view
// actually drawn. UpdateLocation ends in ForceUpdateComponents, so the move
// reaches this draw. Particles the system spawned in world space stay where they
// were born; only the emitter and its local-space sprites follow.
//
// Rain strength: DisEmitterCameraLensEffect_Looping drives its particle system's
// 'Fade' parameter from m_fCurFadeWeight, ramped by m_fFadeSpeed each tick; a
// cap written every draw holds it near RainStrength% (overshoot = one tick's ramp).
// DRAW LANE only (the viewport draw thread, where the scene draw hook runs).
static void LensFollowEye(int eye) {
    if (!g_lensFollow.load() && g_lensRainPct.load() >= 100) return;
    if (GetCurrentThreadId() != g_sdDrawTid || !RflNamesReady() || !g_ctLayout || !g_ctCache) return;
    static bool resolved = false;
    static uint32_t pcCamOff = 0, arrOff = 0, fovOff = 0, fadeOff = 0;
    static uint8_t* fnUpdate = nullptr;
    static uint32_t moves = 0, refusals = 0, caps = 0;
    if (!resolved) {
        resolved = true;
        pcCamOff = RflOffsetOf("PlayerController", "PlayerCamera");
        arrOff   = RflOffsetOf("Camera", "CameraLensEffects");
        FindPropOffsetChecked("TPOV", "FOV", &fovOff);
        fadeOff  = RflOffsetOf("DisEmitterCameraLensEffect_Looping", "m_fCurFadeWeight");
        fnUpdate = RainFindClassFunction("EmitterCameraLensEffectBase", "UpdateLocation");
        Log("lens/follow: layout PlayerCamera=+0x%x CameraLensEffects=+0x%x TPOV.FOV=+0x%x m_fCurFadeWeight=+0x%x "
            "UpdateLocation=%p%s", pcCamOff, arrOff, fovOff, fadeOff, (void*)fnUpdate,
            (pcCamOff && arrOff && fovOff && fnUpdate) ? "" : "  <-- incomplete: follow-head refuses (native placement stays)");
    }
    uint8_t* ctrl = IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam = RainPtr(ctrl, pcCamOff);
    if (!cam || !IsLiveObject(cam)) return;
    uint8_t* data = nullptr; int32_t num = 0;
    if (!RflArrayAt(cam, arrOff, &data, &num) || num <= 0) return;
    if (num > 16) num = 16;
    uint8_t* pov = cam + g_ctCache + g_ctPov;
    struct { float loc[3]; int32_t rot[3]; float fov; } parms{};
    const bool povOk = fovOff && RangeReadable(pov, fovOff + 4) &&
        (memcpy(parms.loc, pov + g_ctLoc, 12), memcpy(parms.rot, pov + g_ctRot, 12), memcpy(&parms.fov, pov + fovOff, 4), true) &&
        std::isfinite(parms.loc[0] + parms.loc[1] + parms.loc[2]) && parms.fov > 10.f && parms.fov < 170.f;
    const int pct = g_lensRainPct.load();
    for (int i = 0; i < num; ++i) {
        uint8_t* fx = ((uint8_t**)data)[i];
        const char* cn = fx && IsLiveObject(fx) ? ObjClassName(fx) : nullptr;
        if (!cn || !strstr(cn, "LensEffect")) continue;
        if (pct < 100 && fadeOff && !strcmp(cn, "DisEmitterCameraLensEffect_Looping") && RangeReadable(fx + fadeOff, 4)) {
            float* w = (float*)(fx + fadeOff);
            const float cap = pct / 100.f;
            if (std::isfinite(*w) && *w > cap) { *w = cap; ++caps; }
        }
        if (!g_lensFollow.load()) continue;
        if (!povOk || !fnUpdate) { ++refusals; continue; }
        auto p = parms;   // UpdateLocation takes const out params; never hand it ours to keep
        g_peReentry = true;
        ((PFN_ProcessEventCall)kProcessEvent)(fx, fnUpdate, &p, NULL);
        g_peReentry = false;
        ++moves;
    }
    DVR_LOG_EVERY_MS(DVR_CAT, dvr::log::Level::Info, 5000,
        "lens/follow: eye %+d re-placed %u time(s) from the rendered POV (loc %.1f %.1f %.1f fov %.1f), refused %u, "
        "rain strength caps %u (%d%%); zero moves with lens effects present means the native placement is what you see",
        eye, moves, parms.loc[0], parms.loc[1], parms.loc[2], parms.fov, refusals, caps, pct);
}
