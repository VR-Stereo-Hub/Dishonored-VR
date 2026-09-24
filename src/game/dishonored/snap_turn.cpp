// game/dishonored/snap_turn.cpp - snap turn (VR-219). Included by the unity build right
// after head_track.cpp (it rides that file's yaw bookkeeping) and before pad_bridge.cpp's
// caller through the header.
//
// WHAT THE PLAYER SEES. With `[Turning] SnapTurn=1` a push of the right stick past
// SnapThreshold turns them by SnapAngle degrees at once, and the stick has to come back
// under SnapRearm before the next step. The body turns with the view, so the hands, the
// sword, the aim ray and the HUD anchors stay where they were in front of the player, and a
// hit or an interact lands on what is now in front. Off (the default) is the game's own
// smooth turn, byte for byte.
//
// WHY THIS SHAPE. The smooth turn is the game's: the right stick reaches it as the pad's RX
// axis and the engine integrates it into the controller rotation; the mod never writes yaw
// for it. The mod's one yaw writer is ApplyHeadToViewRotation (head_track.cpp, script lane),
// whose fresh branch adds the head delta to the view rotation and books it as HEAD yaw
// (YawPublish; body = view - head contribution, yaw_book.h). A snap is a one-shot BODY delta
// written in that same branch: rot[1] += headDeltaU + snapU, YawPublish(viewInU + snapU,
// headDeltaU). Both facing modes then turn the pawn (head-based movement passes the view
// facing through; character mode's FaceRotationHandler follows the body target), and every
// consumer of the view yaw is computed after the add. Not a pad pulse (that would depend on
// the game's look sensitivity and on frame timing), not a pawn Rotation write (measured
// futile: head_track.cpp "VR-30" notes).
//
// LANES. The present lane detects the stick edge and queues the step; it eats RX only while
// the script camera writer is fresh, so wherever the writer is not writing (a menu, a book,
// a cinematic, a keyhole, the retired fallback) the stick stays the game's and turns
// smoothly. The script lane takes the step exactly once, in the fresh branch, so the 2 ms
// re-stamp and the second-eye replay cannot apply it twice. A step nobody takes within 500
// ms is dropped and said so. The next fresh dispatch reads what the engine handed back
// against what was written: a verified write is not an honoured one.
#include "game/dishonored/snap_turn.h"
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace dvr::snap {
namespace {

// ---- the levers (F10 and the seam write them; both lanes read) ----
std::atomic<bool> g_on{false};
float g_angleDeg  = 45.0f;   // [Turning] SnapAngle
float g_threshold = 0.6f;    // [Turning] SnapThreshold: the push that fires a step
float g_rearm     = 0.3f;    // [Turning] SnapRearm: the stick must fall under this before the next
int   g_repeatMs  = 0;       // [Turning] SnapRepeatMs: 0 = one step per push

// ---- the detector (present lane only, except `armed`, which a live toggle resets) ----
std::atomic<bool> d_armed{true};
double d_lastFireMs = 0.0;
bool   d_laneTurn   = false;
char   d_laneWhy[96] = "off";

// ---- the handoff (present lane queues, script lane takes) ----
std::atomic<int32_t>   h_pendingU{0};
std::atomic<long long> h_queuedMs{0};

// ---- counters (either lane bumps, readouts read) ----
std::atomic<unsigned> c_fired{0}, c_applied{0}, c_dropped{0}, c_honoured{0}, c_notHonoured{0};

// ---- the last step (script lane writes, readouts read; aligned 32-bit, torn reads impossible) ----
std::atomic<int32_t> e_lastU{0}, e_viewBefore{0}, e_viewAfter{0}, e_bodyAfter{0};

// ---- the honour check (script lane only) ----
bool    t_took       = false;   // take_pending ran in this fresh write; note_written closes it
int32_t t_headDelta  = 0;
int32_t hon_snapU    = 0;
bool    hon_pending  = false;

// ---- since-mark (the `snapturn mark` word; the sim's gate reads the deltas) ----
struct Mark { bool have = false; int32_t view = 0, body = 0, pawn = 0; bool pawnHave = false; float headRad = 0.0f; } m_mark;

inline double deg(int32_t u) { return (double)u * 360.0 / 65536.0; }
inline int32_t wrapU(int32_t a, int32_t b) { return (int32_t)(int16_t)(uint16_t)((uint32_t)a - (uint32_t)b); }   // a - b in -180..180 deg
inline double wrapRadDeg(float a, float b) {
    double d = (double)a - (double)b;
    while (d >  3.14159265358979) d -= 6.28318530717959;
    while (d < -3.14159265358979) d += 6.28318530717959;
    return d * 180.0 / 3.14159265358979;
}
inline int32_t stepU() { return (int32_t)lroundf(g_angleDeg * 65536.0f / 360.0f); }
float clampf(float v, float lo, float hi, const char* key) {
    if (v >= lo && v <= hi) return v;
    const float c = v < lo ? lo : hi;
    Log("config: [Turning] %s=%.3f is outside %.2f..%.2f - clamped to %.2f", key, v, lo, hi, c);
    return c;
}
float ini_float(const char* ini, const char* key, float def) {
    char buf[64] = "";
    GetPrivateProfileStringA("Turning", key, "", buf, sizeof(buf), ini);
    if (!buf[0]) return def;
    const float v = (float)atof(buf);
    return std::isfinite(v) ? v : def;
}

void fire(int sign, const char* how, float x, double nowMs) {
    const int32_t u = sign * stepU();
    const int32_t total = h_pendingU.fetch_add(u) + u;
    h_queuedMs.store((long long)nowMs);
    d_lastFireMs = nowMs;
    ++c_fired;
    Log("snap: FIRED %+.0f deg (%+d U) on the stick's %s (x=%.2f) -> queued for the head write (pending %+d U)",
        (double)sign * g_angleDeg, u, how, x, total);
}

void write_keys(const char* ini) {
    char v[32];
    WritePrivateProfileStringA("Turning", "SnapTurn", g_on.load() ? "1" : "0", ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_angleDeg);   WritePrivateProfileStringA("Turning", "SnapAngle", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_threshold);  WritePrivateProfileStringA("Turning", "SnapThreshold", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_rearm);      WritePrivateProfileStringA("Turning", "SnapRearm", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", g_repeatMs);     WritePrivateProfileStringA("Turning", "SnapRepeatMs", v, ini);
}

void report() {
    Log("snap: %s | angle %.0f deg (%d U) threshold %.2f rearm %.2f repeat %d ms | steps %u applied %u dropped %u honoured %u NOT honoured %u | "
        "lane %s (%s) armed %d pending %+d U | last %+.1f deg: view %d -> %d U, body %d U | view %.1f body %.1f head %.1f deg%s",
        g_on.load() ? "ON" : "off", g_angleDeg, stepU(), g_threshold, g_rearm, g_repeatMs,
        c_fired.load(), c_applied.load(), c_dropped.load(), c_honoured.load(), c_notHonoured.load(),
        d_laneTurn ? "TURN" : "blocked", d_laneWhy, (int)d_armed.load(), h_pendingU.load(),
        deg(e_lastU.load()), e_viewBefore.load(), e_viewAfter.load(), e_bodyAfter.load(),
        deg(g_yawViewOut), deg(g_yawBodyTarget), (double)g_hmdYaw * 180.0 / 3.14159265358979,
        g_yawValid ? "" : " (yaw book not valid yet)");
    if (m_mark.have)
        Log("snap: since mark: view %+.1f body %+.1f head %+.1f pawn %s deg", deg(wrapU(g_yawViewOut, m_mark.view)),
            deg(wrapU(g_yawBodyTarget, m_mark.body)), wrapRadDeg(g_hmdYaw, m_mark.headRad),
            m_mark.pawnHave && g_afCenHave ? "" : "n/a");
}

// "Start measuring here": the since-mark yaws AND the counters, so a sequence can run
// again without a relaunch and still assert `applied eq 4`.
void mark(const char* who) {
    m_mark.view = g_yawViewOut; m_mark.body = g_yawBodyTarget; m_mark.headRad = g_hmdYaw;
    m_mark.pawnHave = g_afCenHave; m_mark.pawn = g_afCenPawn; m_mark.have = true;
    const unsigned f = c_fired.exchange(0), a = c_applied.exchange(0), d = c_dropped.exchange(0),
                   h = c_honoured.exchange(0), n = c_notHonoured.exchange(0);
    Log("snap: mark by %s - view %d U body %d U head %.1f deg pawn %s; counters zeroed (were steps %u applied %u dropped %u honoured %u not honoured %u)",
        who, m_mark.view, m_mark.body, (double)m_mark.headRad * 180.0 / 3.14159265358979,
        m_mark.pawnHave ? "read" : "not read (arms census not running)", f, a, d, h, n);
}

} // namespace

bool enabled() { return g_on.load(); }

void set_enabled(bool on, const char* who) {
    g_on.store(on);
    d_armed.store(false);   // a stick already deflected at the toggle must come back under rearm first
    Log("snap: %s by %s (live) - %s", on ? "ON" : "off", who ? who : "?",
        on ? "the right stick turns the body and the view in fixed steps; the game's smooth turn is off in gameplay"
           : "the game's own smooth turn; the stick reaches it unchanged");
}

bool present_tick(float rawX, bool rxWouldTurn, bool controllersActive, double nowMs) {
    const bool on = g_on.load();
    // The stale watchdog runs whether or not the lever is still on: a step queued just before
    // the writer stopped (or before `snapturn off`) must not sit there until the next fresh write.
    if (h_pendingU.load() != 0 && nowMs - (double)h_queuedMs.load() > 500.0) {
        const int32_t took = h_pendingU.exchange(0);
        if (took) {
            ++c_dropped;
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 30000,
                "snap: NO CONSUMER - %+d U (%+.0f deg) queued %.0f ms ago and no fresh head write took it "
                "(script head ok=%d age=%.0f ms, uiBlocks=%d, cine=%d, menu=%d/%d) - dropped. Snap turn needs the script camera writer",
                took, deg(took), nowMs - (double)h_queuedMs.load(), (int)g_scriptHeadOK, nowMs - g_scriptHeadMs,
                (int)UiSurfaceBlocks(), (int)CineHeadOwnsInput(), (int)g_menuOpen, (int)g_inMenu);
        }
    }
    const bool scriptFresh = g_scriptHeadOK && (nowMs - g_scriptHeadMs) < 250.0;
    const bool lane = on && controllersActive && scriptFresh;
    const char* why = !on ? "off" : !controllersActive ? "no controllers"
                    : !scriptFresh ? "the script camera writer is not fresh (menu, book, cinematic, keyhole or the fallback)" : "turn";
    if (lane != d_laneTurn) {
        d_laneTurn = lane;
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Debug, "snap: lane %s (%s)", lane ? "TURN" : "blocked", why);
    }
    strncpy_s(d_laneWhy, sizeof(d_laneWhy), why, _TRUNCATE);
    const float ax = fabsf(rawX);
    if (!lane) {
        // The stick is the game's here (smooth turn, or navigation). The detector tracks the
        // stick anyway: a stick still deflected when the lane reopens is DISARMED, and it
        // re-arms only once it has come back under rearm, so a push held into and out of a
        // menu cannot fire a step the player did not push for. (The first cut re-armed here
        // but never disarmed, and the sim's pause-menu leg fired on the resume: snap-turn.xrs
        // step 7 is that fault, kept.)
        d_armed.store(ax < g_rearm);
        return false;
    }
    if (!rxWouldTurn) {
        // Someone upstream took the stick for navigation (the wheel, the D-pad flip, the F10
        // pointer) or it is centred. Nothing to eat, nothing to fire; the same arm rule as above.
        d_armed.store(ax < g_rearm);
        return false;
    }
    const int sign = rawX > 0.0f ? 1 : -1;
    if (d_armed.load() && ax >= g_threshold) { fire(sign, "edge", rawX, nowMs); d_armed.store(false); }
    else if (!d_armed.load() && ax < g_rearm) d_armed.store(true);
    else if (!d_armed.load() && g_repeatMs > 0 && ax >= g_threshold && nowMs - d_lastFireMs >= (double)g_repeatMs) fire(sign, "hold-repeat", rawX, nowMs);
    return true;   // the whole lane: a push under threshold must not smooth-turn either
}

int32_t take_pending(int32_t viewInU, int32_t headDeltaU) {
    const int32_t u = h_pendingU.exchange(0);
    if (!u) return 0;
    ++c_applied;
    e_lastU.store(u); e_viewBefore.store(viewInU);
    t_took = true; t_headDelta = headDeltaU;
    hon_snapU = u; hon_pending = true;
    return u;
}

void note_written(int32_t viewOutU, int32_t bodyTargetU) {
    if (!t_took) return;
    t_took = false;
    e_viewAfter.store(viewOutU); e_bodyAfter.store(bodyTargetU);
    Log("snap: APPLIED %+.1f deg by the script head write: view yaw %d -> %d U (%.1f -> %.1f deg), body target %d U (%.1f deg), head delta this frame %+d U",
        deg(hon_snapU), e_viewBefore.load(), viewOutU, deg(e_viewBefore.load()), deg(viewOutU), bodyTargetU, deg(bodyTargetU), t_headDelta);
}

void note_incoming(int32_t incomingU, bool havePrevWrite, int32_t prevWriteU) {
    if (!hon_pending) return;
    hon_pending = false;
    if (!havePrevWrite) return;
    // RX is eaten while the lane is open, so the engine adds nothing of its own between our
    // write and this dispatch: the difference is what it did to the step. A kick or a shake
    // would show here too, and would be a few hundred U, not a step's worth.
    const int32_t drift = wrapU(incomingU, prevWriteU);
    if (labs((long)drift) < labs((long)hon_snapU) / 2) {
        ++c_honoured;
        Log("snap: HONOURED - the engine handed back the snapped yaw (drift %+d U = %+.2f deg)", drift, deg(drift));
    } else {
        ++c_notHonoured;
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn, "snap: NOT HONOURED - the engine returned %+d of the %+d U written (drift %+d U = %+.1f deg): the step was smoothed or undone",
                hon_snapU + drift, hon_snapU, drift, deg(drift));
    }
}

void drop_pending(const char* why) {
    const int32_t u = h_pendingU.exchange(0);
    if (!u) return;
    ++c_dropped;
    Log("snap: DROPPED %+d U (%+.0f deg) - %s", u, deg(u), why ? why : "?");
}

void fallback_owns_camera() {
    if (!g_on.load()) return;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 30000,
        "snap: the direct fallback owns the camera - snap turn is inert here and the stick turns smoothly");
}

void configure(const char* ini) {
    g_on.store(GetPrivateProfileIntA("Turning", "SnapTurn", 0, ini) != 0);
    g_angleDeg  = clampf(ini_float(ini, "SnapAngle", 45.0f), 5.0f, 180.0f, "SnapAngle");
    g_threshold = clampf(ini_float(ini, "SnapThreshold", 0.6f), 0.30f, 0.95f, "SnapThreshold");
    g_rearm     = clampf(ini_float(ini, "SnapRearm", 0.3f), 0.05f, g_threshold - 0.05f, "SnapRearm");
    int rep = GetPrivateProfileIntA("Turning", "SnapRepeatMs", 0, ini);
    if (rep != 0 && (rep < 100 || rep > 2000)) { Log("config: [Turning] SnapRepeatMs=%d is outside 100..2000 (or 0) - clamped", rep); rep = rep < 100 ? 100 : 2000; }
    g_repeatMs = rep;
    d_armed.store(false);
    Log("config: [Turning] SnapTurn=%d SnapAngle=%.0f (%d U) SnapThreshold=%.2f SnapRearm=%.2f SnapRepeatMs=%d - 0 = the game's smooth stick turn; "
        "1 = a push of the right stick turns the body and the view together in fixed steps. Live: 'snapturn on|off', F10 > Controls > Turning",
        (int)g_on.load(), g_angleDeg, stepU(), g_threshold, g_rearm, g_repeatMs);
}

void save(const char* ini) { write_keys(ini); }

bool command(const char* args) {
    char sub[24] = {}, a[32] = {};
    sscanf_s(args ? args : "", "%23s %31s", sub, (unsigned)sizeof(sub), a, (unsigned)sizeof(a));
    bool onOff = false;
    if (DvrOnOff(sub, &onOff)) {
        set_enabled(onOff, "the seam");
        ConfigWriteKey("Turning", "SnapTurn", onOff ? "1" : "0", "the seam");
        return true;
    }
    if (!strcmp(sub, "angle") && *a) {
        g_angleDeg = clampf((float)atof(a), 5.0f, 180.0f, "SnapAngle");
        char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_angleDeg);
        ConfigWriteKey("Turning", "SnapAngle", v, "the seam");
        Log("snap: angle %.0f deg (%d U) (live)", g_angleDeg, stepU());
        return true;
    }
    if (!strcmp(sub, "threshold") && *a) {
        g_threshold = clampf((float)atof(a), 0.30f, 0.95f, "SnapThreshold");
        if (g_rearm > g_threshold - 0.05f) g_rearm = g_threshold - 0.05f;
        char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_threshold);
        ConfigWriteKey("Turning", "SnapThreshold", v, "the seam");
        Log("snap: threshold %.2f rearm %.2f (live)", g_threshold, g_rearm);
        return true;
    }
    if (!strcmp(sub, "rearm") && *a) {
        g_rearm = clampf((float)atof(a), 0.05f, g_threshold - 0.05f, "SnapRearm");
        char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_rearm);
        ConfigWriteKey("Turning", "SnapRearm", v, "the seam");
        Log("snap: rearm %.2f (live)", g_rearm);
        return true;
    }
    if (!strcmp(sub, "repeat") && *a) {
        int rep = atoi(a);
        if (rep != 0 && rep < 100) rep = 100; if (rep > 2000) rep = 2000;
        g_repeatMs = rep;
        char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", g_repeatMs);
        ConfigWriteKey("Turning", "SnapRepeatMs", v, "the seam");
        Log("snap: repeat %d ms (live; 0 = one step per push)", g_repeatMs);
        return true;
    }
    if (!strcmp(sub, "fire")) {
        // The consumer alone, with no stick: a headset A/B with the controller at rest, and the
        // sim's proof that the script lane applies what the present lane queues.
        const int sign = !strcmp(a, "left") ? -1 : 1;
        fire(sign, "seam word", 0.0f, MaimNowMs());
        return true;
    }
    if (!strcmp(sub, "mark")) { mark("the seam"); return true; }
    if (*sub && strcmp(sub, "status"))
        Log("snap: snapturn on|off | angle <deg> | threshold <0.30..0.95> | rearm <0.05..threshold> | repeat <ms, 0 = one per push> | fire [left|right] | mark | status");
    report();
    return true;
}

void status(dvr::status::Writer& w) {
    w.obj("snapTurn");
    w.kv("enabled", g_on.load());
    w.kv("angleDeg", (double)g_angleDeg); w.kv("threshold", (double)g_threshold); w.kv("rearm", (double)g_rearm); w.kv("repeatMs", g_repeatMs);
    w.kv("fired", (unsigned long)c_fired.load()); w.kv("applied", (unsigned long)c_applied.load());
    w.kv("dropped", (unsigned long)c_dropped.load()); w.kv("honoured", (unsigned long)c_honoured.load());
    w.kv("notHonoured", (unsigned long)c_notHonoured.load());
    w.kv("pendingU", (int)h_pendingU.load()); w.kv("armed", d_armed.load());
    w.kv("lane", !g_on.load() ? "off" : d_laneTurn ? "turn" : "blocked");
    w.kv("lastDeg", deg(e_lastU.load())); w.kv("lastViewBeforeU", (int)e_viewBefore.load()); w.kv("lastViewAfterU", (int)e_viewAfter.load());
    w.kv("yawBookValid", (bool)g_yawValid);
    w.kv("viewYawDeg", deg(g_yawViewOut)); w.kv("bodyYawDeg", deg(g_yawBodyTarget));
    w.kv("headYawDeg", (double)g_hmdYaw * 180.0 / 3.14159265358979);
    w.kv("marked", m_mark.have);
    if (m_mark.have) {
        w.kv("viewSinceMarkDeg", deg(wrapU(g_yawViewOut, m_mark.view)));
        w.kv("bodySinceMarkDeg", deg(wrapU(g_yawBodyTarget, m_mark.body)));
        w.kv("headSinceMarkDeg", wrapRadDeg(g_hmdYaw, m_mark.headRad));
        if (m_mark.pawnHave && g_afCenHave) w.kv("pawnYawSinceMarkDeg", deg(wrapU(g_afCenPawn, m_mark.pawn)));   // the engine's own pawn yaw: ground truth that the body turned
    }
    w.end_obj();
}

void draw_ui() {
    namespace ov = dvr::ovl;
    if (!ov::section("Turning", ov::Basic, "How the right stick turns you.")) return;
    bool on = g_on.load();
    if (ov::checkbox("Snap turn", &on)) {
        set_enabled(on, "F10");
        ConfigWriteKey("Turning", "SnapTurn", on ? "1" : "0", "F10");
    }
    ov::tip("Turn in fixed steps instead of smoothly, which many people find more comfortable in a headset. "
            "Your body turns with the view, so your hands, your aim and the HUD stay where they were in front of you. "
            "Off = the game's smooth turn.");
    float angle = g_angleDeg;
    if (ov::slider_float("Step size (degrees)", &angle, 15.0f, 90.0f, "%.0f")) g_angleDeg = angle;
    ov::tip("How far one push turns you. 45 is the usual choice; 30 for finer aiming, 90 to face straight behind in two pushes.");
    if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_angleDeg); ConfigWriteKey("Turning", "SnapAngle", v, "F10"); }
    if (ov::show(ov::Advanced)) {
        float th = g_threshold;
        if (ov::slider_float("Stick push needed", &th, 0.30f, 0.95f, "%.2f")) { g_threshold = th; if (g_rearm > g_threshold - 0.05f) g_rearm = g_threshold - 0.05f; }
        ov::tip("How far the stick has to go before a step fires. Lower fires sooner; higher needs a firm push.");
        if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_threshold); ConfigWriteKey("Turning", "SnapThreshold", v, "F10"); }
        float re = g_rearm;
        if (ov::slider_float("Release before the next step", &re, 0.05f, 0.60f, "%.2f")) g_rearm = re > g_threshold - 0.05f ? g_threshold - 0.05f : re;
        ov::tip("The stick has to come back under this before another step can fire. Keeps one push at one step.");
        if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_rearm); ConfigWriteKey("Turning", "SnapRearm", v, "F10"); }
        int rep = g_repeatMs;
        if (ov::slider_int("Repeat while held (ms, 0 = one step per push)", &rep, 0, 1000)) g_repeatMs = rep == 0 ? 0 : rep < 100 ? 100 : rep;
        ov::tip("Hold the stick past the threshold and a step repeats every this many milliseconds. 0 = never; let go and push again.");
        if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", g_repeatMs); ConfigWriteKey("Turning", "SnapRepeatMs", v, "F10"); }
    }
    if (ov::show(ov::Debug)) {
        ImGui::TextDisabled("steps %u applied %u dropped %u honoured %u not honoured %u | last %+.0f deg view %d -> %d U | lane %s pending %+d U",
            c_fired.load(), c_applied.load(), c_dropped.load(), c_honoured.load(), c_notHonoured.load(),
            deg(e_lastU.load()), e_viewBefore.load(), e_viewAfter.load(), !g_on.load() ? "off" : d_laneTurn ? "turn" : "blocked", h_pendingU.load());
        if (ov::button("Step right (no stick)")) fire(1, "F10", 0.0f, MaimNowMs());
        ImGui::SameLine();
        if (ov::button("Step left (no stick)")) fire(-1, "F10", 0.0f, MaimNowMs());
        ov::tip("Queues one step without the stick: the consumer alone, for an A/B with the controller at rest.");
    }
}

} // namespace dvr::snap
