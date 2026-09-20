// game/dishonored/melee.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// 36.6: the health-elixir long-press. Gameplay only - in menus A is
// confirm and holding it must never drink; a press that starts gated (or
// wanders into a menu/wheel mid-hold) is poisoned until released. Interact
// itself passes through untouched; the potion fires ON TOP at the hold
// threshold, once per press, with a left-hand haptic as the confirmation.
static void HealthElixirTick(bool held)
{
    static double t0 = 0.0;
    static bool   fired = false;
    if (!g_elixirOn) return;
    if (!held) { t0 = 0.0; fired = false; return; }
    if (g_menuOpen || g_inMenu || g_wheelHeld || !g_handMesh) {
        t0 = 0.0; fired = true;          // poison this press
        return;
    }
    double now = MaimNowMs();
    if (t0 == 0.0) { t0 = now; return; }
    if (!fired && now - t0 >= (double)g_elixirHoldMs) {
        fired = true;
        INPUT in[2]; memset(in, 0, sizeof(in));
        in[0].type = INPUT_KEYBOARD; in[0].ki.wVk = (WORD)g_elixirVk;
        in[1].type = INPUT_KEYBOARD; in[1].ki.wVk = (WORD)g_elixirVk;
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        MaimHaptic(0, 0.6f, 0.12f);      // left thump = potion went down
        Log("elixir: health potion (%.0f ms held)", now - t0);
    }
}


// ---- The motion sword (VR-37) ------------------------------------------------
//
// Swing the right controller and Corvo swings the sword. The decision is the pure
// core in swing_core.h; this is the game side: which gates say a swing is allowed
// right now, the attack pulse on the virtual pad, the check that the game really
// started an attack, and the levers and words for tuning it in the headset.
//
// Present lane throughout (UpdateVirtualPad calls tick() first thing, and the
// command seam and status.json tick on the same thread), so the state below needs
// no lock. The two things that come from the script lane arrive already published:
// the item in the right hand (g_rflPrimaryKind, atomics) and the animation
// snapshot (dvr::anim::snapshot(), SRW-locked, self-expiring).
//
// What the 2026-09-20 simulator run measured, and why the feed looks the way it
// does: the render presents twice per game tick (stereo: beat out/s=171 L/s=85),
// so a detector fed once per PRESENT sees every hand pose twice. The pre-VR-37
// detector read three scripted 0.68 m swings as ten 4-39 ms "flicks" with peaks
// of 8-14 m/s and never fired. tick() therefore feeds the core once per LOCATE
// (dvr::vr::locate_gen) and never a pose identical to the last one, and takes dt
// from the runtime's predicted display time.
#include "game/dishonored/swing_core.h"

namespace dvr::swing {
namespace {

struct Settings : Config {
    float pulseMs       = 120.0f;   // [Melee] PulseMs: the edge detector's attack press
    int   pulseMinPolls = 2;        // [Melee] PulseMinPolls: hold until the game polled N times
    bool  outputRb      = false;    // [Melee] Output=rb
    bool  requireSword  = true;     // [Melee] RequireSword
    float honourMs      = 600.0f;   // [Melee] HonourMs
    bool  honourHaptic  = false;    // [Melee] HonourHaptic
    bool  iniEnabled    = true;     // what [Melee] Enabled asked for, before any veto
    bool  detectorFromIni = false;
};
Settings st;
Core live, simCore;
bool force = false;                 // `swing force on`: skip the sword gate, never saved
bool speedLog = false;              // `swing log on`
volatile LONG padPollsTotal = 0;

struct Sim { bool on = false; double startMs = 0; float peak = 0, humpMs = 200; int reps = 1;
             unsigned fires0 = 0, blocked0 = 0; } sim;
struct Pulse { double openMs = 0, untilMs = 0; LONG polls0 = 0; int minPolls = 0; } pulse;
struct Honour { bool on = false; unsigned long long fireTick = 0; double fireMs = 0;
                bool upper0Attack = false, valid0 = false, realTrig = false; LONG polls0 = 0; } hon;
struct Stats { unsigned samples = 0, dups = 0, still = 0, fires = 0, blocked = 0, honoured = 0, kills = 0,
               notHonoured = 0, inconclusive = 0;
               float lastSpeed = 0, peak = 0, bucket[2] = {}; double bucketMs = 0;
               char lastBlock[160] = "none"; uint32_t closed = 0; } n;
dvr::anim::Snapshot body; double bodyMs = 0;
float realTrigger = 0.0f;

const char* detector_name() { return st.detector == kEdge ? "edge" : "sustain"; }
const char* output_name()   { return st.outputRb ? "rb" : "rt"; }
const char* veto() {
    if (g_meleeOn) return "";
    if (g_gamepadOnly) return "GamepadOnly";
    return st.iniEnabled ? "XR_SAFE" : "";
}
float peak10s() { return n.bucket[0] > n.bucket[1] ? n.bucket[0] : n.bucket[1]; }
void note_speed(float v, double now) {
    n.lastSpeed = v;
    if (v > n.peak) n.peak = v;
    if (now - n.bucketMs > 5000.0) { n.bucketMs = now; n.bucket[1] = n.bucket[0]; n.bucket[0] = 0.0f; }
    if (v > n.bucket[0]) n.bucket[0] = v;
}
int sword_kind(unsigned* ageMs) {
    const LONG tick = InterlockedCompareExchange(&g_rflPrimaryKindTick, 0, 0);
    if (ageMs) *ageMs = tick ? (unsigned)(GetTickCount() - (DWORD)tick) : 0xffffffffu;
    return (int)InterlockedCompareExchange(&g_rflPrimaryKind, 0, 0);
}

// The gates, fail closed. The sustain detector keeps exactly the gates it had
// before VR-37, so `swing mode sustain` is the old behaviour and nothing else.
uint32_t gates(double now) {
    uint32_t m = 0;
    if (!g_meleeOn) m |= kGateOff;
    if (st.detector == kSustain) {
        if (!g_handMesh) m |= kGateTracking;
        if (g_wheelHeld) m |= kGateBumper;
        if (now - g_uiEventMs <= 3000.0) m |= kGateUiMute;
        return m;
    }
    if (!DvrGameplayVerdict()) m |= kGateGameplay;
    if (st.requireSword && !force) {
        unsigned age = 0;
        // The read runs at 4 Hz on the script lane, so a second without one means
        // it stopped, and an unknown hand is not a sword hand.
        if (sword_kind(&age) != 1 || age > 1000u) m |= kGateSword;
    }
    if (InterlockedCompareExchange(&g_padBtnsPub, 0, 0) &
        (XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_RIGHT_SHOULDER)) m |= kGateBumper;
    if (g_ovlVisible) m |= kGateOverlay;
    if (body.valid && (body.game || !strcmp(body.state[0], "StatePlayerMasterMantle"))) m |= kGateBody;
    return m;
}
// The FIRST closed gate is the owner of a zero; say which, with its values.
void closed_text(uint32_t m, double now, char* out, size_t cap) {
    unsigned age = 0; const int kind = sword_kind(&age);
    if (m & kGateOff)
        _snprintf_s(out, cap, _TRUNCATE, "the gesture is off%s%s", *veto() ? ", vetoed by " : "", veto());
    else if (m & kGateGameplay)
        _snprintf_s(out, cap, _TRUNCATE, "not a gameplay view (menuOpen=%d inMenu=%d mainMenu=%d cine=%d ui=%d)",
                    (int)g_menuOpen, (int)g_inMenu, (int)g_mainMenu, (int)g_cineNow, (int)UiSurfaceBlocks());
    else if (m & kGateSword) {
        if (age == 0xffffffffu) _snprintf_s(out, cap, _TRUNCATE, "the right hand's item has never been read (rfl/state)");
        else _snprintf_s(out, cap, _TRUNCATE, "the sword is not in the right hand (primary=%s, read %u ms ago)",
                         kind == 1 ? "sword" : kind == 2 ? "another item" : "none", age);
    }
    else if (m & kGateBumper)   _snprintf_s(out, cap, _TRUNCATE, "a grip is held (the power wheel or block)");
    else if (m & kGateOverlay)  _snprintf_s(out, cap, _TRUNCATE, "the F10 overlay is up");
    else if (m & kGateBody)     _snprintf_s(out, cap, _TRUNCATE, "the game owns the body (master=%s)", body.state[0]);
    else if (m & kGateTracking) _snprintf_s(out, cap, _TRUNCATE, "the weapon is not following the hand (handMesh=0)");
    else if (m & kGateUiMute)   _snprintf_s(out, cap, _TRUNCATE, "a UI event %.0f ms ago mutes swings for 3000 ms", now - g_uiEventMs);
    else _snprintf_s(out, cap, _TRUNCATE, "open");
}

void close_pulse() { pulse = Pulse{}; g_meleeUntil = 0.0; }

void honour_begin(double now) {
    const dvr::anim::Snapshot s = dvr::anim::snapshot();
    hon = Honour{};
    hon.on = true; hon.fireTick = GetTickCount64(); hon.fireMs = now;
    hon.valid0 = s.valid;
    hon.upper0Attack = s.valid && !strcmp(s.state[1], "StatePlayerMeleeAttack");
    hon.realTrig = realTrigger > 0.5f;
    hon.polls0 = InterlockedCompareExchange(&padPollsTotal, 0, 0);
}
// A verified write is not an honoured one: the pulse reaching the pad proves
// nothing until the game is seen starting an attack because of it. This can print
// the unwelcome answer, and says INCONCLUSIVE where it cannot tell.
void honour_poll(double now) {
    if (!hon.on) return;
    if (realTrigger > 0.5f) hon.realTrig = true;
    const dvr::anim::Snapshot s = dvr::anim::snapshot();
    const unsigned long long t = GetTickCount64();
    const unsigned age = (unsigned)(t - hon.fireTick);
    const char* got = NULL; bool kill = false;
    if (s.valid) {
        if (!strcmp(s.state[0], "StatePlayerMasterAssassinate") && s.entered[0] + 50 >= hon.fireTick) { got = s.state[0]; kill = true; }
        else if (!strcmp(s.state[1], "StatePlayerGenericFatality") && s.entered[1] + 50 >= hon.fireTick) { got = s.state[1]; kill = true; }
        else if (!strcmp(s.state[1], "StatePlayerMeleeAttack") &&
                 (s.entered[1] + 50 >= hon.fireTick || (hon.upper0Attack && s.sequenceAt > hon.fireTick))) got = s.state[1];
    }
    if (got) {
        hon.on = false;
        if (hon.realTrig) {
            ++n.inconclusive;
            Log("swing: INCONCLUSIVE after %u ms - the game entered %s, but the player's own trigger was pulled in the "
                "window, so the attack has two possible owners", age, got);
            return;
        }
        ++n.honoured; if (kill) ++n.kills;
        Log("swing: HONOURED %s after %u ms (%s=%s seq=%s)", kill ? "kill" : "slash", age,
            kill && got == s.state[0] ? "master" : "upper", got, s.sequence);
        if (st.honourHaptic && g_meleeHaptic) MaimHaptic(1, 0.35f, 0.04f);
        return;
    }
    if ((double)age < st.honourMs) return;
    hon.on = false;
    const LONG polls = InterlockedCompareExchange(&padPollsTotal, 0, 0);
    if (!hon.valid0 || !s.valid) {
        ++n.inconclusive;
        Log("swing: INCONCLUSIVE after %u ms - no animation snapshot to judge by ([Anim] StateWatch off, or stale: %s)",
            age, s.reason);
    } else if (hon.upper0Attack) {
        ++n.inconclusive;
        Log("swing: INCONCLUSIVE after %u ms - the sword was already mid-attack at the fire and no new sequence "
            "started (seq=%s); the game queues or drops a press there", age, s.sequence);
    } else if (strstr(s.state[1], "Equip") || strstr(s.state[1], "TransitionItem")) {
        ++n.inconclusive;
        Log("swing: INCONCLUSIVE after %u ms - the pulse drew or changed the item instead (upper=%s)", age, s.state[1]);
    } else {
        ++n.notHonoured;
        unsigned sAge = 0; const int kind = sword_kind(&sAge);
        DVR_WARN("swing: NOT HONOURED after %u ms - master=%s upper=%s primary=%s pad polls %ld -> %ld output=%s. "
                 "Polls flat = the game never read the pad while the pulse was open (raise PulseMs or PulseMinPolls). "
                 "An upper block state = this install's pad binding maps the input to block (try 'swing output rb')",
                 age, s.state[0], s.state[1], kind == 1 ? "sword" : kind == 2 ? "another item" : "none",
                 (long)hon.polls0, (long)polls, output_name());
    }
}

void handle(const Verdict& v, const Sample& s, double now, const char* src) {
    if (v.jump)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "swing: tracking jump discarded (%.1f m/s in one sample, more than %.0f - the controller was re-acquired "
            "somewhere else, not swung)", v.roomSpeed, kMaxSpeed);
    if (!v.sampled) return;
    ++n.samples;
    note_speed(v.speed, now);
    n.closed = s.closed;
    if (speedLog)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 100,
            "swing: speed %.2f m/s (room %.2f, %s, %s) armed=%d closed=0x%x samples=%u dup=%u",
            v.speed, v.roomSpeed, src, detector_name(), (int)(sim.on ? simCore : live).armed(),
            s.closed, n.samples, n.dups);
    if (v.fired) {
        ++n.fires; g_meleeCount++; g_meleeLastMs = now;
        const bool edgeMode = st.detector == kEdge;
        const double len = edgeMode ? (double)st.pulseMs : (double)g_meleeHoldMs;
        pulse.openMs = now; pulse.untilMs = now + len;
        pulse.polls0 = InterlockedCompareExchange(&padPollsTotal, 0, 0);
        pulse.minPolls = edgeMode ? st.pulseMinPolls : 0;
        g_meleeUntil = pulse.untilMs; g_meleeNext = now + g_meleeCoolMs;
        // Timed because haptics were once a frame-spike suspect (30.24).
        const double h0 = MaimNowMs();
        if (g_meleeHaptic) MaimHaptic(1, 0.7f, 0.08f);
        const double h1 = MaimNowMs();
        if (edgeMode)
            Log("swing: FIRE #%u slash %.2f m/s (%s, detector=edge, threshold %.2f) -> %s %.0f ms, polls>=%d, haptic=%.1fms",
                n.fires, v.speed, src, st.edgeSpeed, output_name(), len, pulse.minPolls, h1 - h0);
        else
            Log("swing: FIRE #%u slash %.2f m/s (%s, detector=sustain, run %.0f ms %.2f m) -> %s %.0f ms, haptic=%.1fms",
                n.fires, v.speed, src, v.runMs, v.runDistM, output_name(), len, h1 - h0);
        honour_begin(now);
    } else if (v.block) {
        ++n.blocked;
        char why[160];
        if (v.block == kBlockGate) closed_text(v.closed, now, why, sizeof(why));
        else if (v.block == kBlockRearm)
            _snprintf_s(why, sizeof(why), _TRUNCATE, "not re-armed (the hand never slowed below %.2f m/s since the last attack)", effective_rearm(st));
        else _snprintf_s(why, sizeof(why), _TRUNCATE, "cooldown (%.0f of %.0f ms left)", v.cooldownLeftMs, st.cooldownMs);
        _snprintf_s(n.lastBlock, sizeof(n.lastBlock), _TRUNCATE, "%s", why);
        Log("swing: BLOCKED %.2f m/s (%s, detector=%s): %s", v.speed, src, detector_name(), why);
    }
    if (v.flick)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
            "swing: flick rejected (%.0f ms, %.2f m, peak %.1f m/s - detector=sustain needs %.0f ms AND %.2f m)",
            v.runMs, v.runDistM, v.runPeak, st.sustainMs, st.sustainDistM);
}

void beat(double now) {
    static double nextMs = 0.0;
    if (nextMs == 0.0) { nextMs = now + 5000.0; return; }
    if (now < nextMs) return;
    nextMs = now + 5000.0;
    char why[160]; closed_text(n.closed ? n.closed : gates(now), now, why, sizeof(why));
    Log("swing: beat detector=%s peak10s=%.2f m/s (needs %.2f) samples=%u dup=%u | gate: %s | fires=%u blocked=%u "
        "honoured=%u notHonoured=%u inconclusive=%u - fires stay 0 while a gate is closed, and the gate named is the owner; "
        "dup = presents inside one locate (about half of all presents under stereo reentry, by design), still=%u = new "
        "locates whose hand had not moved at all",
        detector_name(), peak10s(), st.detector == kEdge ? st.edgeSpeed : st.sustainSpeed, n.samples, n.dups,
        why, n.fires, n.blocked, n.honoured, n.notHonoured, n.inconclusive, n.still);
}

void report() {
    char why[160]; const double now = MaimNowMs(); closed_text(gates(now), now, why, sizeof(why));
    Log("swing: %s%s%s detector=%s output=%s | edge %.2f m/s rearm %.2f (effective %.2f) cooldown %.0f ms pulse %.0f ms "
        "polls>=%d headRel=%d requireSword=%d%s | sustain %.2f m/s %.0f ms %.2f m hold %.0f ms",
        g_meleeOn ? "ON" : "OFF", *veto() ? " vetoed by " : "", veto(), detector_name(), output_name(),
        st.edgeSpeed, st.rearmSpeed, effective_rearm(st), st.cooldownMs, st.pulseMs, st.pulseMinPolls,
        (int)st.headRel, (int)st.requireSword, force ? " (FORCED open)" : "",
        g_meleeSpeed, g_meleeSwingMs, g_meleeSwingDist, g_meleeHoldMs);
    Log("swing: gate: %s | armed=%d samples=%u dup=%u fires=%u blocked=%u (last: %s) honoured=%u kills=%u notHonoured=%u "
        "inconclusive=%u | last %.2f m/s, PEAK SINCE LAST STATUS %.2f m/s <- tune the threshold from this",
        why, (int)live.armed(), n.samples, n.dups, n.fires, n.blocked, n.lastBlock, n.honoured, n.kills,
        n.notHonoured, n.inconclusive, n.lastSpeed, n.peak);
    n.peak = 0.0f;
}
float clampf(float v, float lo, float hi) { return !(v == v) ? lo : v < lo ? lo : v > hi ? hi : v; }
void ini_path(char* out) { _snprintf(out, MAX_PATH, "%s\\dishonored_vr.ini", g_dir); out[MAX_PATH - 1] = 0; }
void set_on(bool on) {
    if (on && *veto() && strcmp(veto(), "")) {
        // g_meleeOn is false here because a veto wrote it; the veto stays the owner.
        DVR_WARN("swing: REFUSED 'on' - motion melee is vetoed by %s, which switches every motion control off together",
                 veto());
        return;
    }
    g_meleeOn = on; st.iniEnabled = on;
    if (!on) { close_pulse(); hon.on = false; }
    Log("swing: %s (live; 'swing save' or F10 writes it)", on ? "ON" : "OFF");
}
} // namespace

void note_real_trigger(float v) { realTrigger = v; }
void note_pad_poll() { InterlockedIncrement(&padPollsTotal); }
bool output_rb() { return st.outputRb; }

bool pulse_active() {
    if (pulse.openMs == 0.0) return false;
    const double now = MaimNowMs();
    if (now < pulse.untilMs) return true;
    // A hitch can swallow a short pulse whole: keep it open until the game has
    // actually polled the pad, but never past half a second.
    if (pulse.minPolls > 0 && now < pulse.openMs + 500.0 &&
        InterlockedCompareExchange(&padPollsTotal, 0, 0) - pulse.polls0 < (LONG)pulse.minPolls) return true;
    pulse.openMs = 0.0;
    return false;
}

void tick() {
    const double now = MaimNowMs();
    // The sustain detector's numbers are the pre-VR-37 keys, read into their own
    // globals further down the config load than configure() runs.
    st.sustainSpeed = g_meleeSpeed; st.sustainMs = g_meleeSwingMs; st.sustainDistM = g_meleeSwingDist;
    st.cooldownMs = g_meleeCoolMs;
    if (hon.on || now - bodyMs >= 50.0) { body = dvr::anim::snapshot(); bodyMs = now; }
    honour_poll(now);

    if (sim.on) {
        const double phase = now - sim.startMs;
        if (phase >= 2.0 * (double)sim.humpMs * (double)sim.reps) {
            sim.on = false; live.reset();
            Log("swing: sim window finished: %u fire(s), %u blocked of %d swing(s) at %.1f m/s peak",
                n.fires - sim.fires0, n.blocked - sim.blocked0, sim.reps, sim.peak);
            return;
        }
        Sample s; s.hand[0] = sim_offset(sim.peak, sim.humpMs, phase); s.hand[1] = 1.2f; s.hand[2] = -0.4f;
        s.handValid = true; s.tMs = now; s.closed = gates(now);
        handle(simCore.feed(s, st), s, now, "sim");
        return;
    }
    if (!g_meleeOn) return;
    beat(now);

    const int dev = g_ctrlIdx[1];
    Sample s;
    s.handValid = dev >= 0 && dev < 16 && g_devPoseOk[dev];
    if (s.handValid) { s.hand[0] = g_devPose[dev][0][3]; s.hand[1] = g_devPose[dev][1][3]; s.hand[2] = g_devPose[dev][2][3]; }
    s.headValid = g_devPoseOk[0];
    if (s.headValid) { s.head[0] = g_devPose[0][0][3]; s.head[1] = g_devPose[0][1][3]; s.head[2] = g_devPose[0][2][3]; }
    // One sample per LOCATE: the render presents twice per game tick, and the
    // second present carries the pose the first one did. Read as a zero step, that
    // repeat is what kept the old detector from ever completing a run. A pose that
    // is identical at a NEW locate is a real sample of a hand at rest (the
    // simulator's does this exactly) and must go through: it is what re-arms.
    static uint32_t lastGen = 0; static float lastHand[3] = { 1e9f, 1e9f, 1e9f };
    const uint32_t gen = dvr::vr::input_hand_aim_sample(1).generation;
    if (gen == lastGen) { ++n.dups; return; }
    const uint32_t genStep = gen - lastGen;
    lastGen = gen;
    if (s.handValid && !memcmp(lastHand, s.hand, sizeof(lastHand))) ++n.still;
    if (s.handValid) memcpy(lastHand, s.hand, sizeof(lastHand));
    const int64_t predicted = dvr::vr::last_predicted_time();
    s.tMs = predicted > 0 ? (double)predicted * 1e-6 : now;
    s.closed = gates(now);
    const Verdict v = live.feed(s, st);
    // `swing log on`, per sample while the hand is moving: the cadence itself is
    // the thing under test (which generation, how far, over how long).
    if (speedLog && v.roomSpeed > 0.5f)
        Log("swing: sample handGen=%u (+%u) locateGen=%u t=%.2f ms speed=%.2f room=%.2f jump=%d x=%.3f y=%.3f z=%.3f",
            gen, genStep, dvr::vr::locate_gen(), s.tMs, v.speed, v.roomSpeed, (int)v.jump, s.hand[0], s.hand[1], s.hand[2]);
    handle(v, s, now, "live");
}

void configure(const char* ini) {
    char buf[32];
    st.iniEnabled = IniFloat(ini, "Melee", "Enabled", 1) != 0.0f;
    GetPrivateProfileStringA("Melee", "Detector", "", buf, sizeof(buf), ini);
    st.detectorFromIni = buf[0] != 0;
    st.detector = !_stricmp(buf, "edge") ? kEdge : kSustain;
    st.edgeSpeed  = clampf(IniFloat(ini, "Melee", "EdgeSpeed", 3.6f), 0.3f, 10.0f);
    st.rearmSpeed = clampf(IniFloat(ini, "Melee", "RearmSpeed", 1.0f), 0.05f, 9.0f);
    st.pulseMs    = clampf(IniFloat(ini, "Melee", "PulseMs", 120.0f), 20.0f, 500.0f);
    st.pulseMinPolls = (int)clampf(IniFloat(ini, "Melee", "PulseMinPolls", 2.0f), 0.0f, 10.0f);
    st.headRel    = IniFloat(ini, "Melee", "HeadRel", 1) != 0.0f;
    st.requireSword = IniFloat(ini, "Melee", "RequireSword", 1) != 0.0f;
    GetPrivateProfileStringA("Melee", "Output", "rt", buf, sizeof(buf), ini);
    st.outputRb = !_stricmp(buf, "rb");
    st.honourMs = clampf(IniFloat(ini, "Melee", "HonourMs", 600.0f), 100.0f, 2000.0f);
    st.honourHaptic = IniFloat(ini, "Melee", "HonourHaptic", 0) != 0.0f;
    Log("config: [Melee] Detector=%s (%s) EdgeSpeed=%.2f RearmSpeed=%.2f (effective %.2f) PulseMs=%.0f PulseMinPolls=%d "
        "HeadRel=%d RequireSword=%d Output=%s HonourMs=%.0f HonourHaptic=%d - this line reports the SETTING; watch for "
        "'swing: FIRE' to know it fires, and 'swing: beat' names the closed gate when it does not",
        detector_name(), st.detectorFromIni ? "ini" : "shipped default", st.edgeSpeed, st.rearmSpeed,
        effective_rearm(st), st.pulseMs, st.pulseMinPolls, (int)st.headRel, (int)st.requireSword, output_name(),
        st.honourMs, (int)st.honourHaptic);
}

void save(const char* ini) {
    char v[32];
    WritePrivateProfileStringA("Melee", "Enabled", st.iniEnabled ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Detector", detector_name(), ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeSpeed);  WritePrivateProfileStringA("Melee", "EdgeSpeed", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.rearmSpeed); WritePrivateProfileStringA("Melee", "RearmSpeed", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_meleeCoolMs); WritePrivateProfileStringA("Melee", "CooldownMs", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.pulseMs);    WritePrivateProfileStringA("Melee", "PulseMs", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", st.pulseMinPolls); WritePrivateProfileStringA("Melee", "PulseMinPolls", v, ini);
    WritePrivateProfileStringA("Melee", "HeadRel", st.headRel ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "RequireSword", st.requireSword ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Output", output_name(), ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.honourMs);   WritePrivateProfileStringA("Melee", "HonourMs", v, ini);
    WritePrivateProfileStringA("Melee", "HonourHaptic", st.honourHaptic ? "1" : "0", ini);
}

bool command(const char* args) {
    char sub[24] = {}, a[24] = {}, b[24] = {}, c[24] = {};
    sscanf(args ? args : "", "%23s %23s %23s %23s", sub, a, b, c);
    const bool on = !strcmp(a, "on"), off = !strcmp(a, "off");
    const float f = (float)atof(a);
    bool said = false;
    if (!strcmp(sub, "on"))  { set_on(true);  return true; }
    if (!strcmp(sub, "off")) { set_on(false); return true; }
    if (!strcmp(sub, "mode") && (!strcmp(a, "edge") || !strcmp(a, "sustain"))) {
        st.detector = !strcmp(a, "edge") ? kEdge : kSustain; live.reset(); close_pulse();
        Log("swing: detector=%s (live; 'swing save' or F10 writes it). %s", detector_name(),
            st.detector == kEdge ? "Fires on the crossing of EdgeSpeed, gated on a gameplay view and the sword in hand"
                                 : "The pre-VR-37 detector and its own gates (handMesh, wheel, the 3 s UI mute)");
        said = true;
    }
    else if (!strcmp(sub, "threshold") && *a) { st.edgeSpeed = clampf(f, 0.3f, 10.0f); Log("swing: EdgeSpeed=%.2f m/s, effective re-arm %.2f (live)", st.edgeSpeed, effective_rearm(st)); said = true; }
    else if (!strcmp(sub, "rearm") && *a)     { st.rearmSpeed = clampf(f, 0.05f, 9.0f); Log("swing: RearmSpeed=%.2f m/s, effective %.2f - capped at 0.9 x the threshold (live)", st.rearmSpeed, effective_rearm(st)); said = true; }
    else if (!strcmp(sub, "cooldown") && *a)  { g_meleeCoolMs = clampf(f, 0.0f, 2000.0f); Log("swing: CooldownMs=%.0f (live, both detectors)", g_meleeCoolMs); said = true; }
    else if (!strcmp(sub, "pulse") && *a)     { st.pulseMs = clampf(f, 20.0f, 500.0f); Log("swing: PulseMs=%.0f (live, edge detector; sustain keeps HoldMs=%.0f)", st.pulseMs, g_meleeHoldMs); said = true; }
    else if (!strcmp(sub, "polls") && *a)     { st.pulseMinPolls = (int)clampf(f, 0.0f, 10.0f); Log("swing: PulseMinPolls=%d (live; 0 = time only)", st.pulseMinPolls); said = true; }
    else if (!strcmp(sub, "rel") && (on || off))   { st.headRel = on; live.reset(); Log("swing: HeadRel=%d (live) - %s", (int)on, on ? "the head's own movement is subtracted, so turning the body is not a swing" : "raw room-space hand speed"); said = true; }
    else if (!strcmp(sub, "sword") && (on || off)) { st.requireSword = on; Log("swing: RequireSword=%d (live)", (int)on); said = true; }
    else if (!strcmp(sub, "output") && (!strcmp(a, "rt") || !strcmp(a, "rb"))) { st.outputRb = !strcmp(a, "rb"); close_pulse(); Log("swing: Output=%s (live) - the input the pulse presses; the HONOURED line says whether the game took it as an attack", output_name()); said = true; }
    else if (!strcmp(sub, "honour") && *a)    { st.honourMs = clampf(f, 100.0f, 2000.0f); Log("swing: HonourMs=%.0f (live)", st.honourMs); said = true; }
    else if (!strcmp(sub, "log") && (on || off))   { speedLog = on; Log("swing: speed log %s (10 Hz)", on ? "ON" : "off"); said = true; }
    else if (!strcmp(sub, "force") && (on || off)) { force = on; if (on) DVR_WARN("swing: FORCE on - the sword gate is bypassed until 'swing force off' or the next launch; never saved"); else Log("swing: force off"); said = true; }
    else if (!strcmp(sub, "sim") && *a) {
        sim = Sim{}; sim.on = true; sim.startMs = MaimNowMs();
        sim.peak = clampf(f, 0.0f, kMaxSpeed - 1.0f);
        sim.humpMs = *b ? clampf((float)atof(b), 20.0f, 2000.0f) : 200.0f;
        sim.reps = *c ? (int)clampf((float)atof(c), 1.0f, 10.0f) : 1;
        sim.fires0 = n.fires; sim.blocked0 = n.blocked; simCore.reset();
        Log("swing: sim %d swing(s) peaking at %.1f m/s, %.0f ms each with an equal rest, through the real decision core "
            "and the real gates (detector=%s)", sim.reps, sim.peak, sim.humpMs, detector_name());
        return true;
    }
    else if (!strcmp(sub, "save")) { char ini[MAX_PATH]; ini_path(ini); save(ini); Log("swing: [Melee] written to %s", ini); said = true; }
    else if (*sub && strcmp(sub, "status"))
        Log("swing: status | on|off | mode edge|sustain | threshold <m/s> | rearm <m/s> | cooldown <ms> | pulse <ms> | "
            "polls <n> | rel on|off | sword on|off | output rt|rb | honour <ms> | log on|off | force on|off | "
            "sim <peak m/s> [humpMs] [reps] | save");
    if (!said || !strcmp(sub, "status")) report();
    return true;
}

void status(dvr::status::Writer& w) {
    const double now = MaimNowMs();
    const uint32_t m = gates(now);
    char why[160]; closed_text(m, now, why, sizeof(why));
    w.obj("swing");
    w.kv("on", (bool)g_meleeOn); w.kv("vetoedBy", veto());
    w.kv("detector", detector_name()); w.kv("output", output_name());
    w.kv("threshold", (double)(st.detector == kEdge ? st.edgeSpeed : g_meleeSpeed));
    w.kv("rearmEffective", (double)effective_rearm(st));
    w.kv("cooldownMs", (double)g_meleeCoolMs); w.kv("pulseMs", (double)(st.detector == kEdge ? st.pulseMs : g_meleeHoldMs));
    w.kv("headRel", st.headRel); w.kv("requireSword", st.requireSword); w.kv("forced", force);
    w.kv("gateOpen", m == 0); w.kv("closedBy", why);
    w.kv("armed", live.armed()); w.kv("pulse", pulse_active());
    w.kv("samples", (unsigned long)n.samples); w.kv("dupSamples", (unsigned long)n.dups); w.kv("stillSamples", (unsigned long)n.still);
    w.kv("fires", (unsigned long)n.fires); w.kv("blocked", (unsigned long)n.blocked); w.kv("lastBlock", n.lastBlock);
    w.kv("honoured", (unsigned long)n.honoured); w.kv("kills", (unsigned long)n.kills);
    w.kv("notHonoured", (unsigned long)n.notHonoured); w.kv("inconclusive", (unsigned long)n.inconclusive);
    w.kv("lastSpeed", (double)n.lastSpeed); w.kv("peakSpeed10s", (double)peak10s());
    w.kv("sim", sim.on);
    w.end_obj();
}

void draw_ui() {
    if (!ImGui::CollapsingHeader("Motion sword")) return;
    const double now = MaimNowMs();
    char v[32];
    bool onBox = g_meleeOn;
    if (ImGui::Checkbox("swinging the right controller swings the sword", &onBox)) {
        set_on(onBox);
        if (g_meleeOn == onBox) ConfigWriteKey("Melee", "Enabled", onBox ? "1" : "0", "F10 Controls");
    }
    if (*veto()) ImGui::TextDisabled("off: vetoed by %s", veto());
    int det = st.detector == kEdge ? 1 : 0;
    const char* dets[] = { "sustain (the old detector)", "edge (fires on the crossing)" };
    if (ImGui::Combo("swing detector", &det, dets, 2)) {
        st.detector = det ? kEdge : kSustain; live.reset(); close_pulse();
        ConfigWriteKey("Melee", "Detector", detector_name(), "F10 Controls");
    }
    if (st.detector == kEdge) {
        ImGui::SliderFloat("swing speed needed (m/s)", &st.edgeSpeed, 0.5f, 8.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeSpeed); ConfigWriteKey("Melee", "EdgeSpeed", v, "F10 Controls"); }
        ImGui::SliderFloat("re-arm below (m/s)", &st.rearmSpeed, 0.1f, 4.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.rearmSpeed); ConfigWriteKey("Melee", "RearmSpeed", v, "F10 Controls"); }
        ImGui::SliderFloat("attack press (ms)", &st.pulseMs, 20.0f, 300.0f, "%.0f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.pulseMs); ConfigWriteKey("Melee", "PulseMs", v, "F10 Controls"); }
    } else {
        ImGui::SliderFloat("swing speed needed (m/s)", &g_meleeSpeed, 0.5f, 6.0f, "%.2f");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_meleeSpeed); ConfigWriteKey("Melee", "SwingSpeed", v, "F10 Controls"); }
    }
    ImGui::SliderFloat("swing cooldown (ms)", &g_meleeCoolMs, 0.0f, 1000.0f, "%.0f");
    if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_meleeCoolMs); ConfigWriteKey("Melee", "CooldownMs", v, "F10 Controls"); }
    char why[160]; closed_text(gates(now), now, why, sizeof(why));
    ImGui::Text("last %.2f m/s   PEAK (10 s) %.2f m/s", n.lastSpeed, peak10s());
    ImGui::Text("gate: %s", why);
    ImGui::Text("fires %u  blocked %u  honoured %u  not honoured %u", n.fires, n.blocked, n.honoured, n.notHonoured);
    ImGui::TextDisabled("Swing, read PEAK, set the speed a little under it. The overlay itself closes the gate while it is up.");
}

} // namespace dvr::swing

// The pad bridge's two entry points, unchanged in name so nothing else moves.
static void MeleeTick() { dvr::swing::tick(); }
static bool MeleeActive() { return g_meleeOn && dvr::swing::pulse_active(); }
