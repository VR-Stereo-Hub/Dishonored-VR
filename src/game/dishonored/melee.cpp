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
// of 8-14 m/s and never fired. tick() therefore feeds the core once per HAND
// SAMPLE GENERATION (input_hand_aim_sample(1).generation - the head's locate_gen
// is a different cadence, and a pose compared with the last one drops a hand at
// rest for ever), and takes dt from the runtime's predicted display time.
#include "game/dishonored/swing_core.h"

namespace dvr::swing {
namespace {

// VR-170. 3.6 was one player's number: their 47 headset swings ran 3.70 to 6.35 m/s
// (median 4.18), so it sat just under THEIR slowest swing, and players who swing
// softer reported misses. 3.0 is 1.9 x the fastest non-swing measured through the
// median (a smooth reach, 1.61) and 0.72 x that player's median. The hump census
// below is what moves it next, from a log instead of a feeling.
constexpr float kShippedEdgeSpeed = 3.0f;
constexpr float kOldShippedEdgeSpeed = 3.6f;

struct Settings : Config {
    float pulseMs       = 120.0f;   // [Melee] PulseMs: the edge detector's attack press
    int   pulseMinPolls = 2;        // [Melee] PulseMinPolls: hold until the game polled N times
    bool  outputRb      = false;    // [Melee] Output=rb
    bool  requireSword  = true;     // [Melee] RequireSword
    float honourMs      = 600.0f;   // [Melee] HonourMs
    bool  honourHaptic  = true;     // [Melee] HonourHaptic
    bool  iniEnabled    = true;     // what [Melee] Enabled asked for, before any veto
    int   stabArm       = 0;        // [Melee] StabArm: 0 sneak (crouched), 1 always
    bool  detectorFromIni = false;
    // VR-170: what ships. The core's own 3.6 stays, because the host tests pin it;
    // the shipped number is the adapter's, and configure() reads the same default.
    Settings() { edgeSpeed = kShippedEdgeSpeed; }
};
Settings st;
Core live, simCore;
bool force = false;                 // `swing force on`: skip the sword gate, never saved
// The thrust (VR-155): who armed it, and what the last few seconds looked like.
struct StabStats { unsigned fires = 0, rejects = 0; char armedBy[64] = "none", lastReject[240] = "none";
                   float peak[2] = {}, travel[2] = {}, ratio[2] = {}; double bucketMs = 0; bool armed = false; } sb;
bool simStab = false;               // `swing stab sim`: the simulated hand thrusts instead of sweeping
float headFwd[2] = { 0.0f, -1.0f }; // where the head faces, flattened; the last good one is kept
bool speedLog = false;              // `swing log on`
volatile LONG padPollsTotal = 0;

struct Sim { bool on = false; double startMs = 0; float peak = 0, humpMs = 200; int reps = 1;
             unsigned fires0 = 0, blocked0 = 0; } sim;
struct Pulse { double openMs = 0, untilMs = 0; LONG polls0 = 0; int minPolls = 0; } pulse;
struct Honour { bool on = false; unsigned long long fireTick = 0; double fireMs = 0;
                bool upper0Attack = false, valid0 = false, realTrig = false; LONG polls0 = 0;
                char sawBlock[96] = ""; } hon;   // a block state the upper lane passed through in the window
struct Stats { unsigned samples = 0, dups = 0, still = 0, fires = 0, blocked = 0, honoured = 0, kills = 0,
               notHonoured = 0, inconclusive = 0;
               float lastSpeed = 0, peak = 0, bucket[2] = {}; double bucketMs = 0;
               char lastBlock[160] = "none"; uint32_t closed = 0; } n;
// The hump census (VR-170): every live hand movement that rose above the re-arm
// level, binned by its peak in 0.5 m/s steps (the last bin is 6.0 and up), split
// into the ones that attacked and the ones that did not. nearMiss counts movements
// with the gates open that peaked within 20 % under the threshold and did NOT
// fire: if a player says swings are being missed, this is the number that agrees
// or disagrees with them. Simulated swings are never counted.
constexpr int kCensusBins = 13;
struct Census { unsigned fired[kCensusBins] = {}, quiet[kCensusBins] = {}, humps = 0, nearMiss = 0;
                float lastPeak = 0, lastTravel = 0, lastMs = 0; int lastFired = 0;
                float minFirePeak = 0, minFireTravel = 0, maxQuietPeak = 0; double printedAt = 0; unsigned printedHumps = 0; } cen;
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

// Is a thrust allowed to count? Sneaking is the game's own crouch, read from the
// collision capsule the crouch module already samples every 50 ms (87.5 standing,
// 65 crouched): it is the one reading that covers the crouch button AND a physical
// crouch, because the physical crouch presses that same button. Under 50 is a vent
// or a crawlspace, where the game owns the stance and a stab has no room. A stale
// capsule is unknown, and unknown is not sneaking. Edges are logged, state is not.
bool stab_armed(double now) {
    char why[64]; bool armed = false;
    if (!st.stab) _snprintf_s(why, sizeof(why), _TRUNCATE, "none (the thrust is off)");
    else if (st.stabArm == 1) { armed = true; _snprintf_s(why, sizeof(why), _TRUNCATE, "always (StabArm=always)"); }
    else if (g_cylOkMs <= 0.0 || now - g_cylOkMs > 1000.0)
        _snprintf_s(why, sizeof(why), _TRUNCATE, "none (the stance has not been read for %.0f ms)", g_cylOkMs > 0.0 ? now - g_cylOkMs : -1.0);
    else if (g_cylLast < 50.0f) _snprintf_s(why, sizeof(why), _TRUNCATE, "none (crawlspace, capsule %.1f)", g_cylLast);
    else if (g_cylLast < 76.0f) { armed = true; _snprintf_s(why, sizeof(why), _TRUNCATE, "crouch (capsule %.1f)", g_cylLast); }
    else _snprintf_s(why, sizeof(why), _TRUNCATE, "none (standing, capsule %.1f)", g_cylLast);
    if (armed != sb.armed) {
        Log("stab: %s - %s", armed ? "ARMED" : "DISARMED", why);
        sb.armed = armed;
    }
    _snprintf_s(sb.armedBy, sizeof(sb.armedBy), _TRUNCATE, "%s", why);
    return armed;
}
float two(const float* b) { return b[0] > b[1] ? b[0] : b[1]; }
const char* style_name() { return st.stabStyle == kPlunge ? "plunge" : "thrust"; }

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
    // The block is over long before the window closes, so it has to be caught in
    // passing: at the timeout the upper lane reads idle again and says nothing.
    if (s.valid && !hon.sawBlock[0] && strstr(s.state[1], "Block"))
        _snprintf_s(hon.sawBlock, sizeof(hon.sawBlock), _TRUNCATE, "%s", s.state[1]);
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
        if (hon.sawBlock[0])
            DVR_WARN("swing: NOT HONOURED after %u ms - the game BLOCKED instead (upper passed through %s): this install's "
                     "pad binding puts block on %s, so the attack is on the other input - try 'swing output %s'",
                     age, hon.sawBlock, output_name(), st.outputRb ? "rt" : "rb");
        else
            DVR_WARN("swing: NOT HONOURED after %u ms - master=%s upper=%s primary=%s pad polls %ld -> %ld output=%s, and no "
                     "block state was seen. Polls flat = the game never read the pad while the pulse was open (raise "
                     "PulseMs or PulseMinPolls); polls moving = the game read the press and did nothing with it",
                     age, s.state[0], s.state[1], kind == 1 ? "sword" : kind == 2 ? "another item" : "none",
                     (long)hon.polls0, (long)polls, output_name());
    }
}

bool live_src(const char* src) { return src && !strcmp(src, "live"); }
void census_text(char* out, size_t cap) {
    int at = _snprintf_s(out, cap, _TRUNCATE, "no attack by peak m/s:");
    for (int i = 0; i < kCensusBins && at > 0 && (size_t)at < cap; ++i)
        if (cen.quiet[i]) at += _snprintf_s(out + at, cap - at, _TRUNCATE, " %.1f%s:%u", i * 0.5f, i == kCensusBins - 1 ? "+" : "", cen.quiet[i]);
    if (at > 0 && (size_t)at < cap) at += _snprintf_s(out + at, cap - at, _TRUNCATE, " | attacked:");
    for (int i = 0; i < kCensusBins && at > 0 && (size_t)at < cap; ++i)
        if (cen.fired[i]) at += _snprintf_s(out + at, cap - at, _TRUNCATE, " %.1f%s:%u", i * 0.5f, i == kCensusBins - 1 ? "+" : "", cen.fired[i]);
}
void census_report(const char* who) {
    char t[512]; census_text(t, sizeof(t));
    Log("swing: census (%s) %u hand movement(s) above the %.2f m/s re-arm level since launch - %s | slowest attack peaked "
        "%.2f m/s (least travel at a fire %.2f m), fastest non-attack %.2f m/s, %u near miss(es) (gates open, peak "
        "within 20 %% under the %.2f threshold). Bins are 0.5 m/s wide and named by their lower edge; 0.00 means none yet. "
        "The threshold belongs in the gap between the two lists; no gap means they overlap for this player and the travel "
        "guard is the next lever",
        who, cen.humps, effective_rearm(st), t, cen.minFirePeak, cen.minFireTravel, cen.maxQuietPeak, cen.nearMiss, st.edgeSpeed);
}
void note_hump(const Verdict& v, const Sample& s, const char* src) {
    const bool open = !v.humpBlock && !s.closed;
    // A near miss is UNDER the threshold by less than 20 %. A cut hump's peak is a
    // lower bound, so it cannot be called one; a hump at or over the threshold that
    // did not attack was held by the travel guard, and its line already says so.
    const bool nearMiss = !v.humpFired && open && !v.humpCut && v.humpPeak >= 0.8f * st.edgeSpeed && v.humpPeak < st.edgeSpeed;
    // The census is the PLAYER's hand. A simulated swing gets its line below and
    // is never counted: a threshold must not be set from movements nobody made.
    if (live_src(src)) {
        ++cen.humps;
        int bin = (int)(v.humpPeak * 2.0f); if (bin < 0) bin = 0; if (bin >= kCensusBins) bin = kCensusBins - 1;
        cen.lastPeak = v.humpPeak; cen.lastTravel = v.humpTravel; cen.lastMs = v.humpMs; cen.lastFired = v.humpFired;
        if (v.humpFired) {
            ++cen.fired[bin];
            if (cen.minFirePeak == 0.0f || v.humpPeak < cen.minFirePeak) cen.minFirePeak = v.humpPeak;
        } else {
            ++cen.quiet[bin];
            if (open && v.humpPeak > cen.maxQuietPeak) cen.maxQuietPeak = v.humpPeak;
            if (nearMiss) ++cen.nearMiss;
        }
    }
    // One line per movement worth reading: anything that reached half the threshold.
    // Slower ones are in the census only. Bounded by the re-arm latch: a hump cannot
    // end more often than the hand can slow down.
    if (v.humpPeak >= 0.5f * st.edgeSpeed)
        Log("swing: hump (%s%s) peak=%.2f m/s travel=%.2f m over %.0f ms%s -> %s (threshold %.2f, travel guard %.2f m)%s",
            src, live_src(src) ? "" : ", not counted in the census", v.humpPeak, v.humpTravel, v.humpMs,
            v.humpCut ? " (CUT SHORT: tracking was lost, jumped, or no sample arrived for 100 ms - peak and travel are what was seen before the gap)" : "",
            v.humpFired == kFiredStab ? "STAB" : v.humpFired ? "ATTACK"
                : v.humpBlock == kBlockGate ? "no attack: a gate was closed" : v.humpBlock == kBlockRearm ? "no attack: not re-armed"
                : v.humpBlock == kBlockCooldown ? "no attack: cooldown"
                : v.humpPeak >= st.edgeSpeed ? "no attack: fast enough, the travel guard held it" : "no attack: under the threshold",
            st.edgeSpeed, st.edgeTravelM,
            nearMiss ? " - a NEAR MISS: if this was meant as a swing, the threshold is too high for this player" : "");
}

void handle(const Verdict& v, const Sample& s, double now, const char* src) {
    if (v.jump)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "swing: tracking jump discarded (%.1f m/s in one sample, more than %.0f - the controller was re-acquired "
            "somewhere else, not swung)", v.roomSpeed, kMaxSpeed);
    // Before the early return: a hump cut short by a tracking gap arrives on a
    // verdict that carries no speed.
    if (v.humpEnd) note_hump(v, s, src);
    if (!v.sampled) return;
    ++n.samples;
    note_speed(v.speed, now);
    n.closed = s.closed;
    if (speedLog)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 100,
            "swing: speed %.2f m/s (room %.2f, %s, %s) armed=%d closed=0x%x samples=%u dup=%u",
            v.speed, v.roomSpeed, src, detector_name(), (int)(sim.on ? simCore : live).armed(),
            s.closed, n.samples, n.dups);
    if (now - sb.bucketMs > 5000.0) { sb.bucketMs = now; sb.peak[1] = sb.peak[0]; sb.travel[1] = sb.travel[0];
        sb.ratio[1] = sb.ratio[0]; sb.peak[0] = sb.travel[0] = sb.ratio[0] = 0.0f; }
    if (v.radial > sb.peak[0]) sb.peak[0] = v.radial;
    if (v.stabTravel > sb.travel[0]) { sb.travel[0] = v.stabTravel; sb.ratio[0] = v.stabRatio; }
    if (v.stabReject) {
        ++sb.rejects;
        const char* bar = v.stabReject == kStabTravel ? "travel" : v.stabReject == kStabRatio ? "ratio"
                        : v.stabReject == kStabStart ? "start" : "forward";
        if (st.stabStyle == kPlunge)
            _snprintf_s(sb.lastReject, sizeof(sb.lastReject), _TRUNCATE,
                "%s: started %+.2f m from the shoulder line (needs %+.2f or higher) travel %.2f (needs %.2f) ratio %.2f "
                "(needs %.2f) aim %.2f (needs %.2f)", bar, v.stabStartDy, -st.stabStartBelowM, v.stabTravel,
                st.stabTravelM, v.stabRatio, st.stabRatio, v.stabForward, st.stabForward);
        else
        _snprintf_s(sb.lastReject, sizeof(sb.lastReject), _TRUNCATE,
            "%s: travel %.2f m (needs %.2f) ratio %.2f (needs %.2f) forward %.2f (needs %.2f)", bar,
            v.stabTravel, st.stabTravelM, v.stabRatio, st.stabRatio, v.stabForward, st.stabForward);
        // The tuning feedback: which bar, and by how much. One line per thrust.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 500,
            "stab: REJECTED %s %s (%s) - peak %.2f m/s over %.0f ms", style_name(), sb.lastReject, src, v.stabPeak, v.stabMs);
    }
    if (v.fired == kFiredStab) {
        ++sb.fires;
        Log("stab: FIRE %s %.2f m/s travel %.2f m ratio %.2f aim %.2f start %+.2f m in %.0f ms (%s, armed by %s)",
            style_name(), v.stabPeak, v.stabTravel, v.stabRatio, v.stabForward, v.stabStartDy, v.stabMs, src, sb.armedBy);
    }
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
            Log("swing: FIRE #%u %s %.2f m/s after %.2f m of travel (%s, detector=edge, threshold %.2f, travel guard %.2f) -> %s %.0f ms, polls>=%d, haptic=%.1fms",
                n.fires, v.fired == kFiredStab ? "stab" : "slash", v.speed, v.travelAtFire, src, st.edgeSpeed, st.edgeTravelM,
                output_name(), len, pulse.minPolls, h1 - h0);
        else
            Log("swing: FIRE #%u slash %.2f m/s (%s, detector=sustain, run %.0f ms %.2f m) -> %s %.0f ms, haptic=%.1fms",
                n.fires, v.speed, src, v.runMs, v.runDistM, output_name(), len, h1 - h0);
        if (live_src(src) && v.fired == kFiredSlash && (cen.minFireTravel == 0.0f || v.travelAtFire < cen.minFireTravel))
            cen.minFireTravel = v.travelAtFire;
        honour_begin(now);
    } else if (v.block) {
        ++n.blocked;
        char why[160];
        if (v.block == kBlockGate) closed_text(v.closed, now, why, sizeof(why));
        else if (v.block == kBlockRearm)
            _snprintf_s(why, sizeof(why), _TRUNCATE, "not re-armed (the hand never slowed below %.2f m/s since the last attack)", effective_rearm(st));
        else _snprintf_s(why, sizeof(why), _TRUNCATE, "cooldown (%.0f of %.0f ms left)", v.cooldownLeftMs, st.cooldownMs);
        _snprintf_s(n.lastBlock, sizeof(n.lastBlock), _TRUNCATE, "%s", why);
        Log("swing: BLOCKED %.2f m/s (%s, detector=%s%s): %s", v.speed, src, detector_name(),
            v.stabTravel > 0.0f ? ", a thrust" : "", why);
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
    if (now - cen.printedAt >= 60000.0 && cen.humps != cen.printedHumps) {
        cen.printedAt = now; cen.printedHumps = cen.humps;
        census_report("once a minute while it grows");
    }
}

void report() {
    char why[160]; const double now = MaimNowMs(); closed_text(gates(now), now, why, sizeof(why));
    Log("swing: %s%s%s detector=%s output=%s | edge %.2f m/s rearm %.2f (effective %.2f) cooldown %.0f ms pulse %.0f ms "
        "polls>=%d travel guard %.2f m headRel=%d median=%d requireSword=%d%s | sustain %.2f m/s %.0f ms %.2f m hold %.0f ms",
        g_meleeOn ? "ON" : "OFF", *veto() ? " vetoed by " : "", veto(), detector_name(), output_name(),
        st.edgeSpeed, st.rearmSpeed, effective_rearm(st), st.cooldownMs, st.pulseMs, st.pulseMinPolls, st.edgeTravelM,
        (int)st.headRel, (int)st.median, (int)st.requireSword, force ? " (FORCED open)" : "",
        g_meleeSpeed, g_meleeSwingMs, g_meleeSwingDist, g_meleeHoldMs);
    Log("swing: gate: %s | armed=%d samples=%u dup=%u fires=%u blocked=%u (last: %s) honoured=%u kills=%u notHonoured=%u "
        "inconclusive=%u | last %.2f m/s, PEAK SINCE LAST STATUS %.2f m/s <- tune the threshold from this",
        why, (int)live.armed(), n.samples, n.dups, n.fires, n.blocked, n.lastBlock, n.honoured, n.kills,
        n.notHonoured, n.inconclusive, n.lastSpeed, n.peak);
    n.peak = 0.0f;
    census_report("status");
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
        // A thrust sim counts THRUSTS: the hand alternates direction to stay in reach,
        // so every thrust out is followed by the hand coming back, which is not one.
        if (phase >= 2.0 * (double)sim.humpMs * (double)(simStab ? sim.reps * 2 : sim.reps)) {
            sim.on = false; live.reset();
            Log("swing: sim window finished: %u fire(s), %u blocked of %d swing(s) at %.1f m/s peak",
                n.fires - sim.fires0, n.blocked - sim.blocked0, sim.reps, sim.peak);
            return;
        }
        // A slash sweeps across the body; a thrust goes out along where the head
        // faces from a forearm in front of the modelled shoulder.
        Sample s; s.handValid = true; s.tMs = now; s.closed = gates(now);
        if (simStab) {
            s.headValid = true; s.head[1] = 1.6f; s.headFwd[0] = 0.0f; s.headFwd[1] = -1.0f;
            const float o = sim_offset(sim.peak, sim.humpMs, phase);
            if (st.stabStyle == kPlunge) { s.hand[0] = 0.20f; s.hand[1] = 1.55f - 0.940f * o; s.hand[2] = -0.25f - 0.342f * o; }
            else                         { s.hand[0] = 0.20f; s.hand[1] = 1.25f;              s.hand[2] = -0.25f - o; }
            s.stabArmed = stab_armed(now);
        } else { s.hand[0] = sim_offset(sim.peak, sim.humpMs, phase); s.hand[1] = 1.2f; s.hand[2] = -0.4f; }
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
    if (s.headValid) {
        s.head[0] = g_devPose[0][0][3]; s.head[1] = g_devPose[0][1][3]; s.head[2] = g_devPose[0][2][3];
        // Forward is the pose's -Z column, flattened. Looking straight up or down
        // leaves nothing to flatten, so the last good heading is kept.
        const float fx = -g_devPose[0][0][2], fz = -g_devPose[0][2][2], fl = sqrtf(fx * fx + fz * fz);
        if (fl > 0.2f) { headFwd[0] = fx / fl; headFwd[1] = fz / fl; }
    }
    s.headFwd[0] = headFwd[0]; s.headFwd[1] = headFwd[1];
    s.stabArmed = st.detector == kEdge && stab_armed(now);
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
        Log("swing: sample handGen=%u (+%u) locateGen=%u t=%.2f ms speed=%.2f raw=%.2f room=%.2f jump=%d x=%.3f y=%.3f z=%.3f",
            gen, genStep, dvr::vr::locate_gen(), s.tMs, v.speed, v.rawSpeed, v.roomSpeed, (int)v.jump, s.hand[0], s.hand[1], s.hand[2]);
    if (speedLog && st.stab && v.radial > 0.5f)
        Log("stab: sample extension=%.2f m/s armed=%d fwd=(%.2f,%.2f) travel=%.2f ratio=%.2f", v.radial, (int)s.stabArmed,
            s.headFwd[0], s.headFwd[1], v.stabTravel, v.stabRatio);
    handle(v, s, now, "live");
}

void configure(const char* ini) {
    char buf[32];
    st.iniEnabled = IniFloat(ini, "Melee", "Enabled", 1) != 0.0f;
    GetPrivateProfileStringA("Melee", "Detector", "", buf, sizeof(buf), ini);
    st.detectorFromIni = buf[0] != 0;
    st.detector = !_stricmp(buf, "sustain") ? kSustain : kEdge;   // edge since the 2026-09-20 headset verdict
    st.edgeSpeed  = clampf(IniFloat(ini, "Melee", "EdgeSpeed", kShippedEdgeSpeed), 0.3f, 10.0f);
    st.edgeTravelM = clampf(IniFloat(ini, "Melee", "EdgeTravelM", 0.0f), 0.0f, 1.0f);
    // VR-170: the old default is WRITTEN in every installed ini, so a new compiled
    // default alone reaches nobody, and a config version bump would rewrite the
    // whole file and drop the machine's tuning (VR-159). So: once per ini, a stored
    // value that is exactly the old default moves to the new one. EdgeSpeedRev is
    // written either way, which is what lets a player type 3.6 back and keep it.
    if (GetPrivateProfileIntA("Melee", "EdgeSpeedRev", 0, ini) < 1) {
        if (fabsf(st.edgeSpeed - kOldShippedEdgeSpeed) < 0.005f) {
            st.edgeSpeed = kShippedEdgeSpeed;
            WritePrivateProfileStringA("Melee", "EdgeSpeed", "3.0", ini);
            Log("config: [Melee] EdgeSpeed %.2f -> %.2f (one-time: the stored value was the old shipped default, EdgeSpeedRev=1 "
                "written; type %.2f back in F10 or the ini and it stays)", kOldShippedEdgeSpeed, kShippedEdgeSpeed, kOldShippedEdgeSpeed);
        } else
            Log("config: [Melee] EdgeSpeed=%.2f kept (it is not the old shipped default %.2f, so it is this machine's own "
                "tuning; EdgeSpeedRev=1 written)", st.edgeSpeed, kOldShippedEdgeSpeed);
        WritePrivateProfileStringA("Melee", "EdgeSpeedRev", "1", ini);
    }
    st.rearmSpeed = clampf(IniFloat(ini, "Melee", "RearmSpeed", 1.0f), 0.05f, 9.0f);
    st.pulseMs    = clampf(IniFloat(ini, "Melee", "PulseMs", 120.0f), 20.0f, 500.0f);
    st.pulseMinPolls = (int)clampf(IniFloat(ini, "Melee", "PulseMinPolls", 2.0f), 0.0f, 10.0f);
    st.headRel    = IniFloat(ini, "Melee", "HeadRel", 1) != 0.0f;
    st.median     = IniFloat(ini, "Melee", "Median", 1) != 0.0f;
    st.requireSword = IniFloat(ini, "Melee", "RequireSword", 1) != 0.0f;
    GetPrivateProfileStringA("Melee", "Output", "rt", buf, sizeof(buf), ini);
    st.outputRb = !_stricmp(buf, "rb");
    st.honourMs = clampf(IniFloat(ini, "Melee", "HonourMs", 600.0f), 100.0f, 2000.0f);
    st.honourHaptic = IniFloat(ini, "Melee", "HonourHaptic", 1) != 0.0f;
    st.stab         = IniFloat(ini, "Melee", "Stab", 1) != 0.0f;
    st.stabSpeed    = clampf(IniFloat(ini, "Melee", "StabSpeed", 1.5f), 0.3f, 6.0f);
    st.stabTravelM  = clampf(IniFloat(ini, "Melee", "StabTravelM", 0.20f), 0.05f, 0.8f);
    st.stabRatio    = clampf(IniFloat(ini, "Melee", "StabRatio", 0.75f), 0.0f, 1.0f);
    st.stabForward  = clampf(IniFloat(ini, "Melee", "StabForward", 0.5f), -1.0f, 1.0f);
    st.stabWindowMs = clampf(IniFloat(ini, "Melee", "StabWindowMs", 400.0f), 100.0f, 1500.0f);
    st.shoulder[0]  = clampf(IniFloat(ini, "Melee", "ShoulderRightM", 0.17f), -0.5f, 0.5f);
    st.shoulder[1]  = clampf(IniFloat(ini, "Melee", "ShoulderDownM", 0.22f), -0.5f, 0.5f);
    st.shoulder[2]  = clampf(IniFloat(ini, "Melee", "ShoulderBackM", 0.04f), -0.5f, 0.5f);
    GetPrivateProfileStringA("Melee", "StabArm", "sneak", buf, sizeof(buf), ini);
    st.stabArm = !_stricmp(buf, "always") ? 1 : 0;
    GetPrivateProfileStringA("Melee", "StabStyle", "plunge", buf, sizeof(buf), ini);
    st.stabStyle = !_stricmp(buf, "thrust") ? kThrust : kPlunge;
    st.stabStartBelowM = clampf(IniFloat(ini, "Melee", "StabStartBelowM", 0.05f), -0.3f, 0.6f);
    Log("config: [Melee] StabStyle=%s StabStartBelowM=%.2f - plunge = a raised fist driven down (the blade in a reverse "
        "grip), thrust = straight out from the shoulder", style_name(), st.stabStartBelowM);
    Log("config: [Melee] Stab=%d StabArm=%s StabSpeed=%.2f StabTravelM=%.2f StabRatio=%.2f StabForward=%.2f "
        "StabWindowMs=%.0f Shoulder R/D/B=%.2f/%.2f/%.2f - the sneak-kill thrust; it needs Detector=edge, and "
        "'stab: ARMED' in the log says when a thrust can count", (int)st.stab, st.stabArm ? "always" : "sneak",
        st.stabSpeed, st.stabTravelM, st.stabRatio, st.stabForward, st.stabWindowMs, st.shoulder[0], st.shoulder[1],
        st.shoulder[2]);
    Log("config: [Melee] Detector=%s (%s) EdgeSpeed=%.2f EdgeTravelM=%.2f (0 = the travel guard is off) RearmSpeed=%.2f (effective %.2f) PulseMs=%.0f PulseMinPolls=%d "
        "HeadRel=%d Median=%d RequireSword=%d Output=%s HonourMs=%.0f HonourHaptic=%d - this line reports the SETTING; watch for "
        "'swing: FIRE' to know it fires, and 'swing: beat' names the closed gate when it does not",
        detector_name(), st.detectorFromIni ? "ini" : "shipped default", st.edgeSpeed, st.edgeTravelM, st.rearmSpeed,
        effective_rearm(st), st.pulseMs, st.pulseMinPolls, (int)st.headRel, (int)st.median, (int)st.requireSword,
        output_name(), st.honourMs, (int)st.honourHaptic);
}

void save(const char* ini) {
    char v[32];
    WritePrivateProfileStringA("Melee", "Enabled", st.iniEnabled ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Detector", detector_name(), ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeSpeed);  WritePrivateProfileStringA("Melee", "EdgeSpeed", v, ini);
    WritePrivateProfileStringA("Melee", "EdgeSpeedRev", "1", ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeTravelM); WritePrivateProfileStringA("Melee", "EdgeTravelM", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.rearmSpeed); WritePrivateProfileStringA("Melee", "RearmSpeed", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_meleeCoolMs); WritePrivateProfileStringA("Melee", "CooldownMs", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.pulseMs);    WritePrivateProfileStringA("Melee", "PulseMs", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%d", st.pulseMinPolls); WritePrivateProfileStringA("Melee", "PulseMinPolls", v, ini);
    WritePrivateProfileStringA("Melee", "HeadRel", st.headRel ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Median", st.median ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "RequireSword", st.requireSword ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Output", output_name(), ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.honourMs);   WritePrivateProfileStringA("Melee", "HonourMs", v, ini);
    WritePrivateProfileStringA("Melee", "HonourHaptic", st.honourHaptic ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "Stab", st.stab ? "1" : "0", ini);
    WritePrivateProfileStringA("Melee", "StabArm", st.stabArm ? "always" : "sneak", ini);
    WritePrivateProfileStringA("Melee", "StabStyle", style_name(), ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabStartBelowM); WritePrivateProfileStringA("Melee", "StabStartBelowM", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabSpeed);    WritePrivateProfileStringA("Melee", "StabSpeed", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabTravelM);  WritePrivateProfileStringA("Melee", "StabTravelM", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabRatio);    WritePrivateProfileStringA("Melee", "StabRatio", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabForward);  WritePrivateProfileStringA("Melee", "StabForward", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.stabWindowMs); WritePrivateProfileStringA("Melee", "StabWindowMs", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.shoulder[0]);  WritePrivateProfileStringA("Melee", "ShoulderRightM", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.shoulder[1]);  WritePrivateProfileStringA("Melee", "ShoulderDownM", v, ini);
    _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.shoulder[2]);  WritePrivateProfileStringA("Melee", "ShoulderBackM", v, ini);
}

bool command(const char* args) {
    char sub[24] = {}, a[24] = {}, b[24] = {}, c[24] = {};
    sscanf(args ? args : "", "%23s %23s %23s %23s", sub, a, b, c);
    const bool on = !strcmp(a, "on"), off = !strcmp(a, "off");
    const float f = (float)atof(a);
    bool said = false;
    if (!strcmp(sub, "stab")) {
        const float g = (float)atof(b);
        if (!strcmp(a, "on") || !strcmp(a, "off")) {
            st.stab = !strcmp(a, "on"); live.reset();
            Log("stab: %s (live; 'swing save' or F10 writes it)%s", st.stab ? "ON" : "OFF",
                st.stab && st.detector != kEdge ? " - it needs 'swing mode edge' and does nothing under sustain" : "");
        }
        else if (!strcmp(a, "style") && (!strcmp(b, "plunge") || !strcmp(b, "thrust"))) {
            st.stabStyle = !strcmp(b, "thrust") ? kThrust : kPlunge; live.reset();
            Log("stab: StabStyle=%s (live) - %s", style_name(), st.stabStyle == kPlunge
                ? "a fist raised to about the shoulder and driven down, a little forward; it must START high"
                : "straight out from the shoulder, the way the head faces");
        }
        else if (!strcmp(a, "start") && *b)   { st.stabStartBelowM = clampf(g, -0.3f, 0.6f); Log("stab: StabStartBelowM=%.2f - a plunge may start this far below the shoulder line (live)", st.stabStartBelowM); }
        else if (!strcmp(a, "speed") && *b)   { st.stabSpeed = clampf(g, 0.3f, 6.0f);     Log("stab: StabSpeed=%.2f m/s of extension (live)", st.stabSpeed); }
        else if (!strcmp(a, "travel") && *b)  { st.stabTravelM = clampf(g, 0.05f, 0.8f);  Log("stab: StabTravelM=%.2f (live)", st.stabTravelM); }
        else if (!strcmp(a, "ratio") && *b)   { st.stabRatio = clampf(g, 0.0f, 1.0f);     Log("stab: StabRatio=%.2f, extension gained over path travelled (live)", st.stabRatio); }
        else if (!strcmp(a, "forward") && *b) { st.stabForward = clampf(g, -1.0f, 1.0f);  Log("stab: StabForward=%.2f, dot with where the head faces (live)", st.stabForward); }
        else if (!strcmp(a, "window") && *b)  { st.stabWindowMs = clampf(g, 100.0f, 1500.0f); Log("stab: StabWindowMs=%.0f (live)", st.stabWindowMs); }
        else if (!strcmp(a, "arm") && (!strcmp(b, "sneak") || !strcmp(b, "always"))) {
            st.stabArm = !strcmp(b, "always") ? 1 : 0;
            Log("stab: StabArm=%s (live) - %s", b, st.stabArm ? "a thrust counts standing up too; for isolating the arming from the detector"
                                                              : "a thrust counts only while crouched");
        }
        else if (!strcmp(a, "shoulder") && *b && *c) {
            char d4[24] = {}; sscanf(args, "%*s %*s %*s %*s %23s", d4);
            st.shoulder[0] = clampf(g, -0.5f, 0.5f); st.shoulder[1] = clampf((float)atof(c), -0.5f, 0.5f);
            if (*d4) st.shoulder[2] = clampf((float)atof(d4), -0.5f, 0.5f);
            Log("stab: shoulder right/down/back = %.2f/%.2f/%.2f m from the head (live)", st.shoulder[0], st.shoulder[1], st.shoulder[2]);
        }
        else if (!strcmp(a, "sim") && *b) {
            sim = Sim{}; sim.on = true; simStab = true; sim.startMs = MaimNowMs();
            sim.peak = clampf(g, 0.0f, kMaxSpeed - 1.0f);
            sim.humpMs = *c ? clampf((float)atof(c), 20.0f, 2000.0f) : 200.0f;
            char d4[24] = {}; sscanf(args, "%*s %*s %*s %*s %23s", d4);
            sim.reps = *d4 ? (int)clampf((float)atof(d4), 1.0f, 10.0f) : 1;
            sim.fires0 = n.fires; sim.blocked0 = n.blocked; simCore.reset();
            Log("stab: sim %d thrust(s) peaking at %.1f m/s of extension, %.0f ms each, straight out from the shoulder, "
                "through the real core, the real gates and the real arming (armed now: %s)", sim.reps, sim.peak, sim.humpMs, sb.armedBy);
            return true;
        }
        else if (*a && strcmp(a, "status"))
            Log("swing stab: status | on|off | style plunge|thrust | start <m below the shoulder> | speed <m/s> | travel <m> | ratio <0-1> | forward <-1..1> | window <ms> | "
                "arm sneak|always | shoulder <right> <down> [back] | sim <peak m/s> [humpMs] [reps]");
        Log("stab: %s style=%s startBelow=%.2f arm=%s armed now: %s | speed %.2f m/s travel %.2f m ratio %.2f forward %.2f window %.0f ms shoulder "
            "%.2f/%.2f/%.2f | fires=%u rejects=%u (last: %s) | 10 s: PEAK extension %.2f m/s, best travel %.2f m at ratio %.2f "
            "<- lower the bar the REJECTED line names",
            st.stab ? "ON" : "OFF", style_name(), st.stabStartBelowM, st.stabArm ? "always" : "sneak", sb.armedBy, st.stabSpeed, st.stabTravelM, st.stabRatio,
            st.stabForward, st.stabWindowMs, st.shoulder[0], st.shoulder[1], st.shoulder[2], sb.fires, sb.rejects,
            sb.lastReject, two(sb.peak), two(sb.travel), two(sb.ratio));
        return true;
    }
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
    else if (!strcmp(sub, "travel") && *a)    { st.edgeTravelM = clampf(f, 0.0f, 1.0f); Log("swing: EdgeTravelM=%.2f m (live) - %s", st.edgeTravelM, st.edgeTravelM > 0.0f ? "a crossing fires only once the hand has covered this much in the same movement; it delays, it never blocks. Read 'least travel at a fire' in the census before choosing a value" : "the travel guard is off"); said = true; }
    else if (!strcmp(sub, "census"))          { if (!strcmp(a, "reset")) { cen = Census{}; Log("swing: census cleared"); } census_report("asked"); return true; }
    else if (!strcmp(sub, "rearm") && *a)     { st.rearmSpeed = clampf(f, 0.05f, 9.0f); Log("swing: RearmSpeed=%.2f m/s, effective %.2f - capped at 0.9 x the threshold (live)", st.rearmSpeed, effective_rearm(st)); said = true; }
    else if (!strcmp(sub, "cooldown") && *a)  { g_meleeCoolMs = clampf(f, 0.0f, 2000.0f); Log("swing: CooldownMs=%.0f (live, both detectors)", g_meleeCoolMs); said = true; }
    else if (!strcmp(sub, "pulse") && *a)     { st.pulseMs = clampf(f, 20.0f, 500.0f); Log("swing: PulseMs=%.0f (live, edge detector; sustain keeps HoldMs=%.0f)", st.pulseMs, g_meleeHoldMs); said = true; }
    else if (!strcmp(sub, "polls") && *a)     { st.pulseMinPolls = (int)clampf(f, 0.0f, 10.0f); Log("swing: PulseMinPolls=%d (live; 0 = time only)", st.pulseMinPolls); said = true; }
    else if (!strcmp(sub, "rel") && (on || off))   { st.headRel = on; live.reset(); Log("swing: HeadRel=%d (live) - %s", (int)on, on ? "the head's own movement is subtracted, so turning the body is not a swing" : "raw room-space hand speed"); said = true; }
    else if (!strcmp(sub, "filter") && (!strcmp(a, "raw") || !strcmp(a, "median"))) {
        st.median = !strcmp(a, "median"); live.reset();
        Log("swing: Median=%d (live) - %s", (int)st.median, st.median
            ? "the decision uses the median of the last 3 speeds: one lying sample decides nothing, at one sample of latency"
            : "the decision uses each sample's own speed, as the sibling BioShock mod does");
        said = true;
    }
    else if (!strcmp(sub, "sword") && (on || off)) { st.requireSword = on; Log("swing: RequireSword=%d (live)", (int)on); said = true; }
    else if (!strcmp(sub, "output") && (!strcmp(a, "rt") || !strcmp(a, "rb"))) { st.outputRb = !strcmp(a, "rb"); close_pulse(); Log("swing: Output=%s (live) - the input the pulse presses; the HONOURED line says whether the game took it as an attack", output_name()); said = true; }
    else if (!strcmp(sub, "honour") && *a)    { st.honourMs = clampf(f, 100.0f, 2000.0f); Log("swing: HonourMs=%.0f (live)", st.honourMs); said = true; }
    else if (!strcmp(sub, "log") && (on || off))   { speedLog = on; Log("swing: speed log %s (10 Hz)", on ? "ON" : "off"); said = true; }
    else if (!strcmp(sub, "force") && (on || off)) { force = on; if (on) DVR_WARN("swing: FORCE on - the sword gate is bypassed until 'swing force off' or the next launch; never saved"); else Log("swing: force off"); said = true; }
    else if (!strcmp(sub, "sim") && *a) {
        sim = Sim{}; sim.on = true; simStab = false; sim.startMs = MaimNowMs();
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
        Log("swing: status | on|off | mode edge|sustain | threshold <m/s> | travel <m> | census [reset] | rearm <m/s> | cooldown <ms> | pulse <ms> | "
            "polls <n> | rel on|off | filter raw|median | sword on|off | output rt|rb | honour <ms> | log on|off | force on|off | "
            "sim <peak m/s> [humpMs] [reps] | save | stab ... (the sneak-kill thrust: 'swing stab' lists it)");
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
    w.kv("headRel", st.headRel); w.kv("median", st.median); w.kv("requireSword", st.requireSword); w.kv("forced", force);
    w.kv("gateOpen", m == 0); w.kv("closedBy", why);
    w.kv("armed", live.armed()); w.kv("pulse", pulse_active());
    w.kv("samples", (unsigned long)n.samples); w.kv("dupSamples", (unsigned long)n.dups); w.kv("stillSamples", (unsigned long)n.still);
    w.kv("fires", (unsigned long)n.fires); w.kv("blocked", (unsigned long)n.blocked); w.kv("lastBlock", n.lastBlock);
    w.kv("honoured", (unsigned long)n.honoured); w.kv("kills", (unsigned long)n.kills);
    w.kv("notHonoured", (unsigned long)n.notHonoured); w.kv("inconclusive", (unsigned long)n.inconclusive);
    w.kv("lastSpeed", (double)n.lastSpeed); w.kv("peakSpeed10s", (double)peak10s());
    w.kv("sim", sim.on);
    w.kv("travelGuardM", (double)st.edgeTravelM);
    w.obj("census");
    w.kv("humps", (unsigned long)cen.humps); w.kv("nearMisses", (unsigned long)cen.nearMiss);
    w.kv("lastPeak", (double)cen.lastPeak); w.kv("lastTravelM", (double)cen.lastTravel); w.kv("lastMs", (double)cen.lastMs);
    w.kv("lastFired", cen.lastFired != 0);
    w.kv("slowestAttackPeak", (double)cen.minFirePeak); w.kv("leastTravelAtFireM", (double)cen.minFireTravel);
    w.kv("fastestNonAttackPeak", (double)cen.maxQuietPeak);
    w.end_obj();
    w.obj("stab");
    w.kv("on", st.stab); w.kv("style", style_name()); w.kv("arm", st.stabArm ? "always" : "sneak"); w.kv("armed", sb.armed); w.kv("armedBy", sb.armedBy);
    w.kv("fires", (unsigned long)sb.fires); w.kv("rejects", (unsigned long)sb.rejects); w.kv("lastReject", sb.lastReject);
    w.kv("peakExtension10s", (double)two(sb.peak)); w.kv("bestTravel10s", (double)two(sb.travel));
    w.kv("bestRatio10s", (double)two(sb.ratio));
    w.end_obj();
    w.end_obj();
}

void draw_ui() {
    namespace ov = dvr::ovl;
    // VR-196: a Basic section, closed by default like every other.
    if (!ov::section("Motion sword", ov::Basic, "Swing the right controller to swing the sword.")) return;
    const double now = MaimNowMs();
    char v[32];
    bool onBox = g_meleeOn;
    if (ImGui::Checkbox("Swinging the right controller swings the sword", &onBox)) {
        set_on(onBox);
        if (g_meleeOn == onBox) ConfigWriteKey("Melee", "Enabled", onBox ? "1" : "0", "F10 Controls");
    }
    ov::tip("Off: attack with the trigger only.");
    if (*veto()) ImGui::TextDisabled("off: vetoed by %s", veto());
    if (ov::show(ov::Debug)) {
        int det = st.detector == kEdge ? 1 : 0;
        const char* dets[] = { "sustain (the old detector)", "edge (fires on the crossing)" };
        if (ImGui::Combo("Swing detector", &det, dets, 2)) {
            st.detector = det ? kEdge : kSustain; live.reset(); close_pulse();
            ConfigWriteKey("Melee", "Detector", detector_name(), "F10 Controls");
        }
        ov::tip("Edge is the tested detector. Sustain is the old one, kept for comparison.");
    }
    if (st.detector == kEdge) {
        ImGui::SliderFloat("Swing speed needed (m/s)", &st.edgeSpeed, 0.5f, 8.0f, "%.2f");
        ov::tip("How fast the controller must move to count as a swing. Lower if swings are missed, "
                "higher if walking or reaching attacks.");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeSpeed); ConfigWriteKey("Melee", "EdgeSpeed", v, "F10 Controls"); }
        if (ov::show(ov::Advanced)) {
            ImGui::SliderFloat("Swing must travel first (m, 0 = off)", &st.edgeTravelM, 0.0f, 0.60f, "%.2f");
            ov::tip("A swing also has to cover this distance. Stops small flicks attacking.");
            if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.edgeTravelM); ConfigWriteKey("Melee", "EdgeTravelM", v, "F10 Controls"); }
            ImGui::SliderFloat("Re-arm below (m/s)", &st.rearmSpeed, 0.1f, 4.0f, "%.2f");
            ov::tip("The controller must slow below this before the next swing can count.");
            if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.rearmSpeed); ConfigWriteKey("Melee", "RearmSpeed", v, "F10 Controls"); }
            ImGui::SliderFloat("Attack press (ms)", &st.pulseMs, 20.0f, 300.0f, "%.0f");
            ov::tip("How long a swing holds the attack button down.");
            if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", st.pulseMs); ConfigWriteKey("Melee", "PulseMs", v, "F10 Controls"); }
            if (ImGui::Checkbox("Ignore one bad tracking sample (median of 3)", &st.median)) {
                live.reset(); ConfigWriteKey("Melee", "Median", st.median ? "1" : "0", "F10 Controls");
            }
            ov::tip("Ignores a single tracking glitch that would look like a fast swing.");
            if (ImGui::Checkbox("Turning my body is not a swing (head-relative)", &st.headRel)) {
                live.reset(); ConfigWriteKey("Melee", "HeadRel", st.headRel ? "1" : "0", "F10 Controls");
            }
            ov::tip("Measures the swing relative to your head, so turning around does not attack.");
            if (ImGui::Checkbox("Only with the sword in my hand", &st.requireSword))
                ConfigWriteKey("Melee", "RequireSword", st.requireSword ? "1" : "0", "F10 Controls");
            ov::tip("Swings do nothing unless the sword is out.");
            int out = st.outputRb ? 1 : 0;
            const char* outs[] = { "right trigger (usual)", "right shoulder" };
            if (ImGui::Combo("A swing presses", &out, outs, 2)) {
                st.outputRb = out != 0; close_pulse();
                ConfigWriteKey("Melee", "Output", output_name(), "F10 Controls");
            }
        }
    } else {
        ImGui::SliderFloat("Swing speed needed (m/s)", &g_meleeSpeed, 0.5f, 6.0f, "%.2f");
        ov::tip("How fast the controller must move to count as a swing (sustain detector).");
        if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", g_meleeSpeed); ConfigWriteKey("Melee", "SwingSpeed", v, "F10 Controls"); }
    }
    ImGui::SliderFloat("Swing cooldown (ms)", &g_meleeCoolMs, 0.0f, 1000.0f, "%.0f");
    ov::tip("The shortest time between two swings.");
    if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.0f", g_meleeCoolMs); ConfigWriteKey("Melee", "CooldownMs", v, "F10 Controls"); }
    if (ov::show(ov::Debug)) {
        // VR-171: the game's swoosh follows its own animation, not the hand-held blade.
        bool trailHide = SwordTrailHideEnabled();
        if (ImGui::Checkbox("Hide the sword's swing trail", &trailHide)) {
            SwordTrailHideSet(trailHide, "F10 Controls");
            ConfigWriteKey("SwordTrail", "Hide", trailHide ? "1" : "0", "F10 Controls");
        }
        ov::tip("The game's swoosh follows its own animation, not your hand (VR-171). Leave on.");
    }
    if (st.detector == kEdge) {
        if (ImGui::Checkbox("A stab while sneaking is the stealth kill", &st.stab)) {
            live.reset(); ConfigWriteKey("Melee", "Stab", st.stab ? "1" : "0", "F10 Controls");
        }
        ov::tip("While sneaking, a stabbing motion does the stealth kill.");
        if (st.stab) {
            int sty = st.stabStyle == kPlunge ? 0 : 1;
            const char* stys[] = { "plunge: raised fist driven down (reverse grip)", "thrust: straight out from the shoulder" };
            if (ImGui::Combo("Stab motion", &sty, stys, 2)) {
                st.stabStyle = sty == 0 ? kPlunge : kThrust; live.reset();
                ConfigWriteKey("Melee", "StabStyle", style_name(), "F10 Controls");
            }
            if (ov::show(ov::Advanced)) {
                if (st.stabStyle == kPlunge) {
                    ImGui::SliderFloat("May start this far below the shoulder (m)", &st.stabStartBelowM, -0.20f, 0.40f, "%.2f");
                    ov::tip("How low the fist may start and still count as a plunge.");
                    if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabStartBelowM); ConfigWriteKey("Melee", "StabStartBelowM", v, "F10 Controls"); }
                }
                ImGui::SliderFloat("Thrust speed needed (m/s)", &st.stabSpeed, 0.5f, 4.0f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabSpeed); ConfigWriteKey("Melee", "StabSpeed", v, "F10 Controls"); }
                ImGui::SliderFloat("Thrust reach needed (m)", &st.stabTravelM, 0.05f, 0.50f, "%.2f");
                if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabTravelM); ConfigWriteKey("Melee", "StabTravelM", v, "F10 Controls"); }
                ImGui::SliderFloat("How straight (0-1)", &st.stabRatio, 0.3f, 1.0f, "%.2f");
                ov::tip("How straight the path must be. Higher rejects curved swings.");
                if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabRatio); ConfigWriteKey("Melee", "StabRatio", v, "F10 Controls"); }
                ImGui::SliderFloat("How forward (0-1)", &st.stabForward, 0.0f, 1.0f, "%.2f");
                ov::tip("How much of the motion must point forward (thrust) or down (plunge).");
                if (ImGui::IsItemDeactivatedAfterEdit()) { _snprintf_s(v, sizeof(v), _TRUNCATE, "%.2f", st.stabForward); ConfigWriteKey("Melee", "StabForward", v, "F10 Controls"); }
            }
            if (ov::show(ov::Debug)) {
                bool always = st.stabArm == 1;
                if (ImGui::Checkbox("Count a stab standing up too (testing)", &always)) {
                    st.stabArm = always ? 1 : 0; ConfigWriteKey("Melee", "StabArm", always ? "always" : "sneak", "F10 Controls");
                }
                ov::tip("For testing the stab without sneaking.");
                ImGui::TextDisabled("stab: %s", sb.armedBy);
                ImGui::TextDisabled("PEAK extension (10 s) %.2f m/s, best reach %.2f m at %.2f straight", two(sb.peak), two(sb.travel), two(sb.ratio));
                ImGui::TextDisabled("stabs %u  rejected %u: %s", sb.fires, sb.rejects, sb.lastReject);
            }
        }
    }
    if (ov::show(ov::Debug)) {
        char why[160]; closed_text(gates(now), now, why, sizeof(why));
        ImGui::TextDisabled("last %.2f m/s   PEAK (10 s) %.2f m/s", n.lastSpeed, peak10s());
        ov::tip("Swing, read PEAK, and set the swing speed a little under it.");
        ImGui::TextDisabled("last movement: peak %.2f m/s over %.2f m -> %s", cen.lastPeak, cen.lastTravel, cen.lastFired ? "attack" : "no attack");
        ImGui::TextDisabled("slowest attack %.2f m/s   fastest non-attack %.2f m/s   near misses %u", cen.minFirePeak, cen.maxQuietPeak, cen.nearMiss);
        ov::tip("Near misses rising means the speed is too high for you. Attacks while walking or reaching mean too low.");
        ImGui::TextDisabled("gate: %s", why);
        ov::tip("Why swings are not being accepted right now. This panel being open closes the gate.");
        ImGui::TextDisabled("fires %u  blocked %u  honoured %u  not honoured %u", n.fires, n.blocked, n.honoured, n.notHonoured);
    }
}

} // namespace dvr::swing

// The pad bridge's two entry points, unchanged in name so nothing else moves.
static void MeleeTick() { dvr::swing::tick(); }
static bool MeleeActive() { return g_meleeOn && dvr::swing::pulse_active(); }
