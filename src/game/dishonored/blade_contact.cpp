// game/dishonored/blade_contact.cpp - included by src/mod/dishonoredvr.cpp (unity build).
// VR-173: THE HELD BLADE IN THE GAME'S WORLD.
//
// What the player would get from it: a sword attack timed by the blade reaching
// something. The blade is known in the hand (hands/blade_axis.cpp). This file carries it
// into the game's world units on the script lane, which is where the engine can be asked
// what lies along it.
//
// SCRIPT LANE ONLY, and read-only: nothing here writes an engine field.
//
// PREREQUISITE 3, THE BRIDGE, CHECKED BY TWO ROUTES TO ONE POINT ([Blade] World, off):
//
//   the XR route    the sword hand's grip pose -> the palm-frame blade -> XR LOCAL metres
//                   -> dvr::fireaim::point_to_world, the arithmetic every hand-aimed
//                   feature's ray origin already takes
//   the draw route  the same palm-frame blade, carried the way the DRAW went and back
//                   across the weapon path's coordinate bridge (hands/blade_axis.cpp)
//
// The draw route holds no headset pose and the XR route holds no draw transform. If the
// bridge has the wrong scale, the wrong anchor or the wrong frames, they part.
//
// COST. Off: two atomic reads per script dispatch. On: once per blade frame (a hand
// sample), a handful of dot products; the comparison line is printed twice a second.
#include "game/dishonored/fire_aim_math.h"
#include "game/dishonored/blade_math.h"

#ifdef DVR_CAT
#undef DVR_CAT
#endif
#define DVR_CAT ::dvr::log::Cat::melee

static std::atomic<bool> g_bwOn{false};          // [Blade] World
// TWO DELIBERATE ERRORS, so the comparison can be shown to disagree. Neither is saved, and
// both are instrument-only: nothing but this file's own world blade reads them.
//   anchor render   anchor the XR route on the last render sample instead of the game
//                   camera. That sample is whichever scene draw uploaded last (an eye, or
//                   a shadow pass), which is why the hand rays left it (VR-181).
//   skew <percent>  scale the XR route's world units by this much. At 5 percent a point
//                   80 uu from the head must come out 4 uu from where the draw put it.
static std::atomic<bool>  g_bwAnchorRender{false};
static std::atomic<float> g_bwSkewPct{0.0f};

struct BwSide { unsigned n = 0; double sumTip = 0, sumBase = 0; float maxTip = 0, maxBase = 0; float lastTip = 0, lastBase = 0; };
struct BwStats {
    BwSide eye[2];
    unsigned frames = 0, refused = 0, stale = 0;
    float lastXrTip[3] = {}, lastXrBase[3] = {}, lastCamera[3] = {};
    float lastLenUU = 0;
    char  why[160] = "off";
} g_bwS;

// The blade's base and tip in game world units, from the blade frame the present lane
// published. `why` names a refusal. Script lane.
static bool BladeWorld(const dvr::hands::BladeFrame& bf, float* baseW, float* tipW, float* cameraW, const char** why)
{
    if (!bf.ok) { *why = bf.why; return false; }
    const uint64_t now = GetTickCount64();
    if (now < bf.sampleMs || now - bf.sampleMs > 100) { *why = "the blade frame is more than 100 ms old"; return false; }
    float camera[3];
    // The anchor every hand ray uses (interact_aim.cpp): the game camera's own base, or
    // the render sample when [Aim] HandRayGameAnchor=0.
    const bool wantGame = g_hrGameAnchor.load() && !g_bwAnchorRender.load();
    if (!(wantGame && GameCameraAnchor(camera)) && !dvr::camera::render_pos_world(camera)) {
        *why = "no world camera position"; return false;
    }
    dvr::fireaim::Frames fr;
    if (!dvr::fireaim::frames(bf.headQuat, g_viewYawRad, g_viewPitchRad, fr)) {
        *why = "the head looks straight up or down: no horizon to build the frames from"; return false;
    }
    if (!std::isfinite(g_posScaleUU) || g_posScaleUU < 1 || g_posScaleUU > 400) { *why = "the world scale is out of range"; return false; }
    const float scale = g_posScaleUU * (1.0f + 0.01f * g_bwSkewPct.load());
    dvr::fireaim::point_to_world(fr, bf.headPos, bf.baseXr, camera, scale, baseW);
    dvr::fireaim::point_to_world(fr, bf.headPos, bf.tipXr, camera, scale, tipW);
    if (!dvr::fireaim::finite3(baseW) || !dvr::fireaim::finite3(tipW)) { *why = "a world point is not finite"; return false; }
    if (cameraW) memcpy(cameraW, camera, 12);
    return true;
}

static void BwTick()
{
    static uint32_t lastGen = 0;
    const dvr::hands::BladeFrame bf = dvr::hands::blade_frame();
    if (bf.ok && bf.handGen == lastGen) return;        // one comparison per hand sample
    lastGen = bf.handGen;
    float baseW[3], tipW[3], cam[3]; const char* why = "";
    if (!BladeWorld(bf, baseW, tipW, cam, &why)) {
        ++g_bwS.refused;
        _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "%s", why);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000, "blade/world: no world blade - %s", why);
        return;
    }
    ++g_bwS.frames;
    memcpy(g_bwS.lastXrTip, tipW, 12); memcpy(g_bwS.lastXrBase, baseW, 12); memcpy(g_bwS.lastCamera, cam, 12);
    { const float d[3] = { tipW[0]-baseW[0], tipW[1]-baseW[1], tipW[2]-baseW[2] }; g_bwS.lastLenUU = dvr::blade::len3(d); }
    const dvr::hands::BladeSnapshot s = dvr::hands::blade_snapshot(g_waSwordHand);
    const uint64_t now = GetTickCount64();
    bool any = false;
    for (int e = 0; e < 2; ++e) {
        if (!s.drawnOk[e] || now < s.drawnMs[e] || now - s.drawnMs[e] > 100) continue;
        any = true;
        const float dt[3] = { tipW[0]-s.drawnTipWorld[e][0], tipW[1]-s.drawnTipWorld[e][1], tipW[2]-s.drawnTipWorld[e][2] };
        const float db[3] = { baseW[0]-s.drawnBaseWorld[e][0], baseW[1]-s.drawnBaseWorld[e][1], baseW[2]-s.drawnBaseWorld[e][2] };
        BwSide& b = g_bwS.eye[e];
        b.lastTip = dvr::blade::len3(dt); b.lastBase = dvr::blade::len3(db);
        ++b.n; b.sumTip += b.lastTip; b.sumBase += b.lastBase;
        if (b.lastTip > b.maxTip) b.maxTip = b.lastTip;
        if (b.lastBase > b.maxBase) b.maxBase = b.lastBase;
    }
    if (!any) { ++g_bwS.stale; _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "no draw of the sword in the last 100 ms to compare with"); }
    else _snprintf_s(g_bwS.why, sizeof(g_bwS.why), _TRUNCATE, "comparing");
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
        "blade/world: XR route tip (%.1f %.1f %.1f) base (%.1f %.1f %.1f) uu, %.1f uu apart | draw route, left eye tip (%.1f %.1f %.1f) "
        "%s, right eye tip (%.1f %.1f %.1f) %s | XR against draw: left tip %.2f base %.2f uu, right tip %.2f base %.2f uu | head in the "
        "world (%.1f %.1f %.1f), view yaw %.2f pitch %.2f deg, %.0f uu per metre, live tip %.4f m off the constant | 100 uu = 1 m",
        tipW[0], tipW[1], tipW[2], baseW[0], baseW[1], baseW[2], g_bwS.lastLenUU,
        s.drawnTipWorld[0][0], s.drawnTipWorld[0][1], s.drawnTipWorld[0][2], s.drawnOk[0] ? "ok" : "NONE",
        s.drawnTipWorld[1][0], s.drawnTipWorld[1][1], s.drawnTipWorld[1][2], s.drawnOk[1] ? "ok" : "NONE",
        g_bwS.eye[0].lastTip, g_bwS.eye[0].lastBase, g_bwS.eye[1].lastTip, g_bwS.eye[1].lastBase,
        cam[0], cam[1], cam[2], g_viewYawRad * 57.29578f, g_viewPitchRad * 57.29578f, g_posScaleUU, bf.liveApartM);
}

static void BwReport()
{
    const BwSide& l = g_bwS.eye[0]; const BwSide& r = g_bwS.eye[1];
    if (g_bwAnchorRender.load() || g_bwSkewPct.load() != 0.0f)
        DVR_WARN("blade/world: A DELIBERATE ERROR IS ON (anchor %s, skew %+.1f %%): the figures below are expected to DISAGREE",
                 g_bwAnchorRender.load() ? "RENDER sample" : "game camera", g_bwSkewPct.load());
    Log("blade/world: %s | %u blade frame(s) carried, %u refused, %u with no fresh draw | XR route against the draw route, "
        "LEFT eye's draw: tip mean %.2f max %.2f uu, base mean %.2f max %.2f uu over %u; RIGHT eye's: tip mean %.2f max %.2f uu, "
        "base mean %.2f max %.2f uu over %u | last XR tip (%.1f %.1f %.1f), blade %.1f uu | %s. The two routes share the "
        "palm-frame blade and nothing else: a wrong scale, anchor or frame shows here. 0 over 0 means nothing was compared, "
        "not that they agree",
        g_bwOn.load() ? "ON" : "off", g_bwS.frames, g_bwS.refused, g_bwS.stale,
        l.n ? l.sumTip / l.n : 0.0, l.maxTip, l.n ? l.sumBase / l.n : 0.0, l.maxBase, l.n,
        r.n ? r.sumTip / r.n : 0.0, r.maxTip, r.n ? r.sumBase / r.n : 0.0, r.maxBase, r.n,
        g_bwS.lastXrTip[0], g_bwS.lastXrTip[1], g_bwS.lastXrTip[2], g_bwS.lastLenUU, g_bwS.why);
}

// The script tick runs on EVERY ProcessEvent dispatch. Never nested, and once per frame
// at most (TRAPS, VR-182).
static void BladeContactTick()
{
    if (!g_bwOn.load(std::memory_order_relaxed)) return;
    static bool inside = false;
    if (inside) return;
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    inside = true;
    if (g_bwOn.load()) BwTick();
    inside = false;
}

static bool BladeContactCommand(const char* a, const char* b, const char* c)
{
    if (!strcmp(a, "world")) {
        if (!strcmp(b, "on") || !strcmp(b, "off")) {
            g_bwOn.store(!strcmp(b, "on"));
            Log("blade/world: %s - %s", g_bwOn.load() ? "ON" : "off", g_bwOn.load()
                ? "the held blade is carried into world units each hand sample and compared with where the renderer drew it; it needs `blade on` and a latched blade"
                : "nothing is carried");
            return true;
        }
        if (!strcmp(b, "reset")) { g_bwS = BwStats{}; Log("blade/world: the comparison is cleared"); return true; }
        if (!strcmp(b, "anchor") && (!strcmp(c, "render") || !strcmp(c, "game"))) {
            g_bwAnchorRender.store(!strcmp(c, "render"));
            Log("blade/world: the XR route is anchored on the %s%s", g_bwAnchorRender.load() ? "LAST RENDER SAMPLE" : "game camera",
                g_bwAnchorRender.load() ? " - a DELIBERATE error for the instrument, never saved" : " (as every hand ray is)");
            return true;
        }
        if (!strcmp(b, "skew") && *c) {
            float v = (float)atof(c); v = v < -50 ? -50 : v > 50 ? 50 : v;
            g_bwSkewPct.store(v);
            Log("blade/world: the XR route's world scale is off by %+.1f %% - a DELIBERATE error for the instrument, never saved; 0 puts it back", v);
            return true;
        }
        BwReport();
        return true;
    }
    Log("blade: world on|off|status|reset | world anchor game|render | world skew <percent>");
    return true;
}
static void BladeContactConfigure(const char* ini)
{
    g_bwOn.store(IniFloat(ini, "Blade", "World", 0) != 0.0f);
    Log("config: [Blade] World=%d - the held blade carried into the game's world units on the script lane and checked "
        "against where the renderer drew it (VR-173). Read-only; off, nothing runs", (int)g_bwOn.load());
}
static void BladeContactSave(const char* ini)
{
    WritePrivateProfileStringA("Blade", "World", g_bwOn.load() ? "1" : "0", ini);
}
static void BladeContactStatus(dvr::status::Writer& w)
{
    w.obj("bladeWorld");
    w.kv("on", g_bwOn.load());
    w.kv("frames", (unsigned long)g_bwS.frames); w.kv("refused", (unsigned long)g_bwS.refused);
    w.kv("noFreshDraw", (unsigned long)g_bwS.stale);
    const char* name[2] = { "left", "right" };
    for (int e = 0; e < 2; ++e) {
        const BwSide& s = g_bwS.eye[e];
        w.obj(name[e]);
        w.kv("n", (unsigned long)s.n);
        w.kv("tipMeanUU", s.n ? s.sumTip / s.n : 0.0); w.kv("tipMaxUU", (double)s.maxTip);
        w.kv("baseMeanUU", s.n ? s.sumBase / s.n : 0.0); w.kv("baseMaxUU", (double)s.maxBase);
        w.end_obj();
    }
    w.kv("bladeUU", (double)g_bwS.lastLenUU);
    w.kv("why", g_bwS.why);
    w.end_obj();
}

#undef DVR_CAT
