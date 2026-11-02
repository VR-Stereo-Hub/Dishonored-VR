// game/dishonored/choke_assist.cpp - included by src/mod/dishonoredvr.cpp (unity build), after
// physical_pickup.cpp (PpPawnReadings) and interact_aim.cpp (GameCameraAnchor).
//
// THE CHOKE ASSIST (2026-11-01). The game chokes on RB (its Block) only while its interaction focus is
// on a guard it lets you choke; otherwise RB blocks. That focus follows the hand's ray (VR-166), so a
// guard you stand behind was rarely focused when the grip went down: of 58 Choke-grip presses in one
// headset run, one choked (the guard focused 312 ms before it, RB held 94 ms); in the next run, 15
// presses, the guard focused at none of them, every one a block. The decisions are choke_core.h:
//   script lane, every frame: the guards (WorldInfo.PawnList), the one to choke, the engine's focus,
//                             where the steered guard is;
//   interaction hooks:        while steering, the trace runs from the camera to that guard (ChokeRay);
//   present lane:             the Choke grip and the ranged hand's swing (melee.cpp) through the
//                             Sequencer: RB held back until the game focuses the guard, then pressed.
// A grip with no guard ready is the block it always was, at once.
#include "game/dishonored/choke_core.h"

static std::atomic<bool>  g_caGripOn{true};     // [Choke] Assist
static std::atomic<bool>  g_caSwingOn{true};    // [Choke] RangedSwing
static std::atomic<float> g_caReachCm{150.0f};  // [Choke] ReachCm
static std::atomic<float> g_caBehindDeg{90.0f}; // [Choke] BehindDeg
static std::atomic<float> g_caFrontDeg{60.0f};  // [Choke] FrontDeg
static std::atomic<float> g_caWaitMs{250.0f};   // [Choke] FocusWaitMs
static std::atomic<float> g_caHoldMs{250.0f};   // [Choke] HoldToChokeMs: with something to take too, a hold this long chokes

// Script lane -> present lane.
static std::atomic<uintptr_t> g_caCand{0};                  // the guard to choke now, 0 = none
static std::atomic<unsigned long long> g_caCandMs{0};       // GetTickCount64 of the last confirmation
static std::atomic<uintptr_t> g_caFocus{0};                 // DishonoredPlayerController.m_pCrosshairActor
static std::atomic<float> g_caCandDistCm{0}, g_caCandBackDeg{0};
// Present lane -> game thread.
static std::atomic<uintptr_t> g_caSteer{0};                 // the guard being steered onto, 0 = none
static std::atomic<bool> g_caNearReq{false};                // `choke near`: the next gameplay tick lists the guards
// Game thread only.
static float g_caSteerLoc[3] = {};
static bool  g_caSteerLocOk = false;
static volatile LONG g_caRayDriven = 0;

struct CaStats { unsigned grips = 0, swings = 0, swingNoGuard = 0, steered = 0, focused = 0, timeouts = 0,
                 passthrough = 0, honoured = 0, blocked = 0, nothing = 0; };
static CaStats g_caN;   // present lane

static constexpr unsigned long long kCaFreshMs = 150;

static bool ChokeCandidateNow()
{
    return g_caGripOn.load() && g_caCand.load() && GetTickCount64() - g_caCandMs.load() <= kCaFreshMs;
}
static bool ChokeAssistRangedOn() { return g_caSwingOn.load() && !g_gamepadOnly; }
static double ChokeHoldMs() { return (double)g_caHoldMs.load(); }

static bool CaGameplay()
{
    return !g_gamepadOnly && g_pePawn && g_peCtrl && CylTruthLive() && !g_menuOpen && !g_inMenu && !g_mainMenu && !g_cineNow;
}

// ---- script lane ------------------------------------------------------------------------------------------

// Is this pawn class a person the game could let you choke? By class name, cached per class pointer.
static bool CaIsNpcClass(uint8_t* cls)
{
    static uint8_t* seen[16] = {}; static bool verdict[16] = {}; static int next = 0;
    for (int i = 0; i < 16; ++i) if (seen[i] == cls) return verdict[i];
    const char* cn = RealName(*(uint32_t*)(cls + kNameOff));
    const bool npc = cn && strstr(cn, "NPCPawn") != NULL;
    seen[next] = cls; verdict[next] = npc; next = (next + 1) & 15;
    return npc;
}

static void ChokeAssistTick()
{
    static uint32_t lastFrame = 0xffffffffu;
    const uint32_t frame = (uint32_t)dvr::frame::count();
    if (frame == lastFrame) return;
    lastFrame = frame;
    g_caSteerLocOk = false;
    if ((!g_caGripOn.load() && !g_caSwingOn.load()) || !CaGameplay()) { g_caCand.store(0); g_caFocus.store(0); return; }

    static uint32_t locOff = 0, rotOff = 0, wiOff = 0, listOff = 0, nextOff = 0, focusOff = 0;
    static bool resolved = false, usable = false;
    if (!resolved) {
        resolved = true;
        locOff = RflOffsetOf("Actor", "Location");
        rotOff = RflOffsetOf("Actor", "Rotation");
        wiOff = RflOffsetOf("Actor", "WorldInfo");
        listOff = RflOffsetOf("WorldInfo", "PawnList");
        nextOff = RflOffsetOf("Pawn", "NextPawn");
        focusOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
        usable = locOff && rotOff && wiOff && listOff && nextOff && focusOff;
        Log("choke: offsets Actor.Location +0x%X Actor.Rotation +0x%X Actor.WorldInfo +0x%X WorldInfo.PawnList +0x%X "
            "Pawn.NextPawn +0x%X DishonoredPlayerController.m_pCrosshairActor +0x%X - %s", locOff, rotOff, wiOff, listOff,
            nextOff, focusOff, usable ? "the assist can run" : "ONE IS 0 (not resolved by name): the assist is OFF and the grip blocks as before");
    }
    if (!usable) return;

    using namespace dvr::choke;
    static Guard guards[64]; static uint8_t* who[64];
    int n = 0; unsigned walked = 0;
    float player[3] = {}; bool playerOk = false; uint8_t* focus = NULL;
    const uintptr_t steer = g_caSteer.load();
    dvr::crash::probe_begin();
    __try {
        focus = *(uint8_t**)(g_peCtrl + focusOff);
        memcpy(player, g_pePawn + locOff, 12);
        playerOk = player[0] == player[0];
        uint8_t* wi = *(uint8_t**)(g_pePawn + wiOff);
        uint8_t* p = wi && !((uintptr_t)wi & 3) ? *(uint8_t**)(wi + listOff) : NULL;
        while (p && !((uintptr_t)p & 3) && (uintptr_t)p >= 0x10000 && walked < 512) {
            ++walked;
            uint8_t* cls = *(uint8_t**)(p + kClassOff);
            if (p != g_pePawn && cls && !((uintptr_t)cls & 3) && (uintptr_t)cls >= 0x10000 && CaIsNpcClass(cls)) {
                if ((uintptr_t)p == steer) { memcpy(g_caSteerLoc, p + locOff, 12); g_caSteerLocOk = g_caSteerLoc[0] == g_caSteerLoc[0]; }
                if (n < 64) {
                    Guard& g = guards[n];
                    memcpy(g.loc, p + locOff, 12);
                    g.yaw = *(int*)(p + rotOff + 4);              // FRotator: Pitch, Yaw, Roll
                    PpPawnReadings(p, &g.health, &g.controller);
                    who[n++] = p;
                }
            }
            p = *(uint8_t**)(p + nextOff);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { n = 0; g_caSteerLocOk = false; }
    dvr::crash::probe_end();
    g_caFocus.store((uintptr_t)focus);
    if (!playerOk) { g_caCand.store(0); return; }

    Config c;
    c.reachUU = g_caReachCm.load() / 100.0f * g_posScaleUU;
    c.heightUU = 0.9f * g_posScaleUU;
    c.behindDeg = g_caBehindDeg.load();
    c.frontDeg = g_caFrontDeg.load();
    const Pick pk = pick(player, g_viewYawRad, guards, n, c);
    uint8_t* cand = pk.index >= 0 ? who[pk.index] : NULL;
    if (g_caNearReq.exchange(false)) {   // `choke near`: where each person stands and faces, so a test can stand behind one
        Log("choke/near: you at (%.0f %.0f %.0f) looking %.1f deg (rotator yaw %d); %d people in the pawn list (%u pawns walked)",
            player[0], player[1], player[2], g_viewYawRad * 57.29578f, (int)(g_viewYawRad / kYawUnitsToRad), n, walked);
        int order[64]; for (int i = 0; i < n; ++i) order[i] = i;
        for (int i = 1; i < n; ++i) {   // nearest first
            const int k = order[i]; const float dk = judge(player, g_viewYawRad, guards[k], c).distUU; int j = i - 1;
            while (j >= 0 && judge(player, g_viewYawRad, guards[order[j]], c).distUU > dk) { order[j + 1] = order[j]; --j; }
            order[j + 1] = k;
        }
        for (int i = 0; i < n && i < 6; ++i) {
            const Guard& g = guards[order[i]];
            const Reading r = judge(player, g_viewYawRad, g, c);
            Log("choke/near: #%d %s at (%.0f %.0f %.0f) facing %.1f deg (rotator yaw %d) Health %d controller %d | %.2f m, %.0f deg off "
                "its back, %.0f deg off your view -> %s", i, ObjClassName(who[order[i]]) ? ObjClassName(who[order[i]]) : "?",
                g.loc[0], g.loc[1], g.loc[2], wrap_deg(g.yaw * kYawUnitsToRad * 57.29578f), g.yaw & 0xffff, g.health, g.controller,
                r.distUU / g_posScaleUU, r.offBackDeg, r.offViewDeg, refusal_text(r.refusal));
        }
    }
    g_caCand.store((uintptr_t)cand);
    if (cand) {
        g_caCandMs.store(GetTickCount64());
        g_caCandDistCm.store(pk.taken.distUU / g_posScaleUU * 100.0f);
        g_caCandBackDeg.store(pk.taken.offBackDeg);
    }
    static uint8_t* said = (uint8_t*)1;
    if (cand != said) {
        said = cand;
        const float m = g_posScaleUU > 1 ? g_posScaleUU : 50.0f;
        if (cand)
            Log("choke: a guard to choke - %s %.2f m away, you are %.0f deg off its back and it is %.0f deg off where you look "
                "(Health %d) | the Choke grip or a ranged-hand swing chokes it", ObjClassName(cand) ? ObjClassName(cand) : "?",
                pk.taken.distUU / m, pk.taken.offBackDeg, pk.taken.offViewDeg, guards[pk.index].health);
        else if (pk.nearestRefused >= 0)
            Log("choke: no guard to choke - the nearest living one (%u pawns walked, %d people) is %.2f m away: %s "
                "(%.0f deg off its back, %.0f deg off your view; reach %.0f cm, behind within %.0f deg, ahead within %.0f deg)",
                walked, n, pk.refused.distUU / m, refusal_text(pk.refused.refusal), pk.refused.offBackDeg,
                pk.refused.offViewDeg, g_caReachCm.load(), c.behindDeg, c.frontDeg);
        else
            Log("choke: no guard to choke - no living person in the level's pawn list (%u pawns walked)", walked);
    }
}

// interact_aim.cpp asks this first: while a guard is being steered onto, the engine's interaction trace
// runs from the camera to it, so the game focuses the guard you are behind whichever way the hands point.
static bool ChokeRay(float* origin, float* dir)
{
    if (!g_caSteer.load() || !g_caSteerLocOk) return false;
    float cam[3];
    if (!GameCameraAnchor(cam)) return false;
    float d[3] = { g_caSteerLoc[0] - cam[0], g_caSteerLoc[1] - cam[1], g_caSteerLoc[2] + 0.1f * g_posScaleUU - cam[2] };
    if (!dvr::fireaim::normalize(d)) return false;
    memcpy(origin, cam, 12); memcpy(dir, d, 12);
    InterlockedIncrement(&g_caRayDriven);
    return true;
}

// ---- present lane -----------------------------------------------------------------------------------------

static bool   g_caSwingPending = false;
static double g_caSwingAtMs = 0.0;
static float  g_caSwingSpeed = 0.0f, g_caSwingThreshold = 0.0f;
static dvr::choke::Sequencer g_caSeq;
struct CaHonour { bool on = false, watching = false; unsigned long long pressTick = 0, chokeTick = 0; char sawBlock[96] = ""; };
static CaHonour g_caHon;
static unsigned long long g_caHeldMs = 0, g_caHeldSince = 0;   // RB down for the choke alone (the grip let go)
struct CaOutcome { unsigned won = 0, lost = 0, cancelled = 0; } g_caOut;

// The game is choking and has not reached the ending (Win, Lose, Cancel): the hold keeps RB down.
static bool CaChoking(const dvr::anim::Snapshot& a)
{
    return a.valid && !strcmp(a.state[0], "StatePlayerMasterChoke") &&
           !strstr(a.sequence, "Win") && !strstr(a.sequence, "Lose") && !strstr(a.sequence, "Cancel");
}

// melee.cpp: the ranged hand's swing crossed the sword's own threshold.
static void ChokeAssistSwing(float speed, float threshold)
{
    g_caSwingPending = true; g_caSwingAtMs = MaimNowMs();
    g_caSwingSpeed = speed; g_caSwingThreshold = threshold;
}

static void CaHonourPoll(const dvr::anim::Snapshot& s)
{
    const unsigned long long t = GetTickCount64();
    if (g_caHon.watching) {   // how the choke ended, with how long RB was held for it alone
        const bool ending = s.valid && !strcmp(s.state[0], "StatePlayerMasterChoke") &&
                            (strstr(s.sequence, "Win") || strstr(s.sequence, "Lose") || strstr(s.sequence, "Cancel"));
        const bool left = s.valid && strcmp(s.state[0], "StatePlayerMasterChoke") != 0;
        if (!ending && !left && t - g_caHon.chokeTick < 12000) return;
        g_caHon.watching = false;
        const char* how = ending ? (strstr(s.sequence, "Win") ? "WON - the guard is unconscious" : strstr(s.sequence, "Lose")
                                    ? "LOST - the guard broke free (the game's choke is a hold of Block: was RB still down?)"
                                    : "CANCELLED") : left ? "ended without a Win/Lose/Cancel move" : "not ended after 12 s";
        if (ending && strstr(s.sequence, "Win")) ++g_caOut.won; else if (ending && strstr(s.sequence, "Lose")) ++g_caOut.lost; else ++g_caOut.cancelled;
        Log("choke: %s after %llu ms (seq=%s; RB held for the choke alone %llu ms; won %u lost %u other %u this session)", how,
            t - g_caHon.chokeTick, s.valid ? s.sequence : "?", g_caHeldMs + (g_caHeldSince ? t - g_caHeldSince : 0),
            g_caOut.won, g_caOut.lost, g_caOut.cancelled);
        return;
    }
    if (!g_caHon.on) return;
    if (s.valid && !g_caHon.sawBlock[0] && strstr(s.state[1], "Block"))
        _snprintf_s(g_caHon.sawBlock, sizeof(g_caHon.sawBlock), _TRUNCATE, "%s", s.state[1]);
    if (s.valid && !strcmp(s.state[0], "StatePlayerMasterChoke") && s.entered[0] + 50 >= g_caHon.pressTick) {
        g_caHon.on = false; g_caHon.watching = true; g_caHon.chokeTick = t; ++g_caN.honoured;
        g_caHeldMs = 0; g_caHeldSince = 0;
        Log("choke: HONOURED after %llu ms - the game entered StatePlayerMasterChoke (seq=%s); RB stays down until its "
            "ending, whatever the grip does (at most 8 s)", t - g_caHon.pressTick, s.sequence);
        MaimHaptic(0, 0.35f, 0.05f);
        return;
    }
    if (t - g_caHon.pressTick < 800) return;
    g_caHon.on = false;
    if (g_caHon.sawBlock[0]) {
        ++g_caN.blocked;
        Log("choke: NOT HONOURED after 800 ms - the game BLOCKED (upper passed through %s) with the guard focused: it does not "
            "let you choke this one now (it saw you, it is in a fight, or the angle is not its own 'behind')", g_caHon.sawBlock);
    } else {
        ++g_caN.nothing;
        Log("choke: NOT HONOURED after 800 ms - master=%s upper=%s, no block either", s.valid ? s.state[0] : "?", s.valid ? s.state[1] : "?");
    }
}

// pad_bridge.cpp, every virtual-pad update: whether RB goes down. `grip` is the Choke action held.
static bool ChokeAssistRb(bool grip)
{
    using namespace dvr::choke;
    const double now = MaimNowMs();
    // The animation snapshot only while a press is in flight or a choke is being watched (it takes a lock).
    dvr::anim::Snapshot anim;
    const bool needAnim = g_caSeq.phase() == kPressing || g_caHon.on || g_caHon.watching;
    if (needAnim) anim = dvr::anim::snapshot();
    CaHonourPoll(anim);
    const bool choking = needAnim && CaChoking(anim);
    const bool swing = g_caSwingPending && now - g_caSwingAtMs <= 100.0;
    g_caSwingPending = false;
    const uintptr_t cand = g_caCand.load();
    const bool fresh = cand && GetTickCount64() - g_caCandMs.load() <= kCaFreshMs;
    const uintptr_t steering = g_caSteer.load();
    const bool focused = steering && g_caFocus.load() == steering;
    Timing t; t.focusWaitMs = g_caWaitMs.load();
    const Step st = g_caSeq.step(now, grip, swing, fresh && g_caGripOn.load(), fresh && g_caSwingOn.load(), focused, t, choking);
    {   // how long RB was down for the choke alone, for the ending's line
        const unsigned long long tk = GetTickCount64();
        if (st.held && !g_caHeldSince) g_caHeldSince = tk;
        if (!st.held && g_caHeldSince) { g_caHeldMs += tk - g_caHeldSince; g_caHeldSince = 0; }
    }
    if (st.event == kEvSteer) g_caSteer.store(cand);
    if (!st.steer) g_caSteer.store(0);
    const char* src = g_caSeq.source() == kSrcSwing ? "ranged-hand swing" : "Choke grip";
    switch (st.event) {
    case kEvSteer:
        ++g_caN.steered; if (g_caSeq.source() == kSrcSwing) ++g_caN.swings; else ++g_caN.grips;
        if (g_caSeq.source() == kSrcSwing)
            Log("choke: ranged-hand swing %.2f m/s (needs %.2f, the sword's [Melee] EdgeSpeed) with a guard %.2f m away, %.0f deg "
                "off its back - steering the game's focus onto it, RB on the focus", g_caSwingSpeed, g_caSwingThreshold,
                g_caCandDistCm.load() / 100.0f, g_caCandBackDeg.load());
        else
            Log("choke: Choke grip pressed with a guard %.2f m away, %.0f deg off its back - steering the game's focus onto it, "
                "RB held back until it is focused (at most %.0f ms)", g_caCandDistCm.load() / 100.0f, g_caCandBackDeg.load(), t.focusWaitMs);
        break;
    case kEvFocused:
        ++g_caN.focused;
        Log("choke: the game focuses the guard after %.0f ms - RB pressed (%s)", now - g_caSeq.since(), src);
        g_caHon = CaHonour{}; g_caHon.on = true; g_caHon.pressTick = GetTickCount64();
        break;
    case kEvTimeoutBlock:
        ++g_caN.timeouts;
        Log("choke: the game did not focus the guard in %.0f ms (focus %p, ray driven %ld times) - the grip blocks as it always did",
            t.focusWaitMs, (void*)g_caFocus.load(), (long)g_caRayDriven);
        break;
    case kEvTimeoutDrop:
        ++g_caN.timeouts;
        Log("choke: the game did not focus the guard in %.0f ms (focus %p, ray driven %ld times) - %s dropped, nothing pressed",
            t.focusWaitMs, (void*)g_caFocus.load(), (long)g_caRayDriven, src);
        break;
    case kEvPassthrough:
        ++g_caN.passthrough; ++g_caN.grips;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "choke: Choke grip pressed with no guard to choke - RB at once (block)%s", g_caGripOn.load() ? "" : " ([Choke] Assist=0)");
        break;
    case kEvSwingNoGuard:
        ++g_caN.swingNoGuard;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 2000,
            "choke: ranged-hand swing %.2f m/s (needs %.2f) with no guard to choke - nothing pressed%s", g_caSwingSpeed,
            g_caSwingThreshold, g_caSwingOn.load() ? "" : " ([Choke] RangedSwing=0)");
        break;
    default: break;
    }
    return st.rb;
}

// ---- settings, seam, F10 ----------------------------------------------------------------------------------

static void CaSay(const char* who)
{
    Log("choke: assist %s, ranged-hand swing %s (%s) - reach %.0f cm, behind within %.0f deg of its back, ahead within %.0f deg "
        "of your view, focus wait %.0f ms, hold to choke %.0f ms. The Choke grip near a guard you are behind turns the game's "
        "focus onto it and presses RB once it is focused (with something to take there too, a tap takes it and a hold chokes); "
        "anywhere else it blocks at once.", g_caGripOn.load() ? "ON" : "off", g_caSwingOn.load() ? "ON" : "off", who,
        g_caReachCm.load(), g_caBehindDeg.load(), g_caFrontDeg.load(), g_caWaitMs.load(), g_caHoldMs.load());
}
static float CaClamp(float v, float lo, float hi) { return !(v == v) ? lo : v < lo ? lo : v > hi ? hi : v; }

static void ChokeAssistConfigure(const char* ini)
{
    g_caGripOn.store(IniFloat(ini, "Choke", "Assist", 1) != 0.0f);
    g_caSwingOn.store(IniFloat(ini, "Choke", "RangedSwing", 1) != 0.0f);
    g_caReachCm.store(CaClamp(IniFloat(ini, "Choke", "ReachCm", 150), 50, 300));
    g_caBehindDeg.store(CaClamp(IniFloat(ini, "Choke", "BehindDeg", 90), 10, 180));
    g_caFrontDeg.store(CaClamp(IniFloat(ini, "Choke", "FrontDeg", 60), 10, 180));
    g_caWaitMs.store(CaClamp(IniFloat(ini, "Choke", "FocusWaitMs", 250), 50, 1000));
    g_caHoldMs.store(CaClamp(IniFloat(ini, "Choke", "HoldToChokeMs", 250), 100, 1000));
    CaSay("ini [Choke]");
}

static bool ChokeAssistCommand(const char* args)
{
    bool b = false; float v = 0; char num[16];
    auto save = [&](const char* key, float value) { _snprintf(num, sizeof(num), "%.0f", value); num[sizeof(num) - 1] = 0; ConfigWriteKey("Choke", key, num, "the seam"); };
    if (!*args || !strcmp(args, "status")) {
        CaSay("status");
        Log("choke: status | candidate %p (%.2f m, %.0f deg off its back) focus %p steering %p phase %s | grips %u swings %u "
            "(no guard %u) steered %u focused %u timeouts %u passthrough %u | honoured %u blocked %u nothing %u | ray driven %ld",
            (void*)g_caCand.load(), g_caCandDistCm.load() / 100.0f, g_caCandBackDeg.load(), (void*)g_caFocus.load(),
            (void*)g_caSteer.load(), dvr::choke::phase_text(g_caSeq.phase()), g_caN.grips, g_caN.swings, g_caN.swingNoGuard,
            g_caN.steered, g_caN.focused, g_caN.timeouts, g_caN.passthrough, g_caN.honoured, g_caN.blocked, g_caN.nothing,
            (long)g_caRayDriven);
        return true;
    }
    if (!strcmp(args, "near")) { g_caNearReq.store(true); Log("choke: near - the next gameplay tick lists the nearest people"); return true; }
    if (!strncmp(args, "assist ", 7) && DvrOnOff(args + 7, &b)) { g_caGripOn.store(b); ConfigWriteKey("Choke", "Assist", b ? "1" : "0", "the seam"); CaSay("the seam"); return true; }
    if (!strncmp(args, "swing ", 6) && DvrOnOff(args + 6, &b)) { g_caSwingOn.store(b); ConfigWriteKey("Choke", "RangedSwing", b ? "1" : "0", "the seam"); CaSay("the seam"); return true; }
    if (sscanf(args, "reach %f", &v) == 1) { g_caReachCm.store(CaClamp(v, 50, 300)); save("ReachCm", g_caReachCm.load()); CaSay("the seam"); return true; }
    if (sscanf(args, "behind %f", &v) == 1) { g_caBehindDeg.store(CaClamp(v, 10, 180)); save("BehindDeg", g_caBehindDeg.load()); CaSay("the seam"); return true; }
    if (sscanf(args, "front %f", &v) == 1) { g_caFrontDeg.store(CaClamp(v, 10, 180)); save("FrontDeg", g_caFrontDeg.load()); CaSay("the seam"); return true; }
    if (sscanf(args, "wait %f", &v) == 1) { g_caWaitMs.store(CaClamp(v, 50, 1000)); save("FocusWaitMs", g_caWaitMs.load()); CaSay("the seam"); return true; }
    if (sscanf(args, "hold %f", &v) == 1) { g_caHoldMs.store(CaClamp(v, 100, 1000)); save("HoldToChokeMs", g_caHoldMs.load()); CaSay("the seam"); return true; }
    Log("choke: status | near | assist on|off | swing on|off | reach <cm> | behind <deg> | front <deg> | wait <ms> | hold <ms>");
    return true;
}

static void ChokeAssistDrawUi()
{
    namespace ov = dvr::ovl;
    if (!ov::section("Choke (non-lethal)", ov::Basic, "Choke a guard you stand behind with the grip or by swinging the ranged hand.")) return;
    bool grip = g_caGripOn.load();
    if (ImGui::Checkbox("The Choke grip chokes the guard you are behind", &grip)) {
        g_caGripOn.store(grip); ConfigWriteKey("Choke", "Assist", grip ? "1" : "0", "F10 Controls");
    }
    ov::tip("The game only chokes while it focuses the guard. On: the grip turns its focus onto the guard first. "
            "Off: the grip is the plain block, as before. With something to take there too (a key on its belt), "
            "a tap of the grip takes it and a hold chokes.");
    {
        float hold = g_caHoldMs.load();
        if (ImGui::SliderFloat("Hold to choke (ms)", &hold, 100.0f, 1000.0f, "%.0f")) g_caHoldMs.store(hold);
        if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf(v, sizeof(v), "%.0f", hold); v[15] = 0; ConfigWriteKey("Choke", "HoldToChokeMs", v, "F10 Controls"); }
        ov::tip("Only when the grip has both jobs: let go sooner than this to take the thing, hold longer to choke.");
    }
    bool swing = g_caSwingOn.load();
    if (ImGui::Checkbox("Swinging the ranged hand chokes too", &swing)) {
        g_caSwingOn.store(swing); ConfigWriteKey("Choke", "RangedSwing", swing ? "1" : "0", "F10 Controls");
    }
    ov::tip("Swing the hand that holds the pistol or the powers, as fast as a sword swing (Motion sword > swing speed), "
            "behind a guard. With no guard behind it the swing does nothing.");
    if (ov::show(ov::Advanced)) {
        float reach = g_caReachCm.load();
        if (ImGui::SliderFloat("Choke reach (cm)", &reach, 50.0f, 300.0f, "%.0f")) g_caReachCm.store(reach);
        if (ImGui::IsItemDeactivatedAfterEdit()) { char v[16]; _snprintf(v, sizeof(v), "%.0f", reach); v[15] = 0; ConfigWriteKey("Choke", "ReachCm", v, "F10 Controls"); }
        ov::tip("How close a guard must be. The game keeps its own limit as well.");
    }
    if (g_caCand.load()) ImGui::TextDisabled("a guard to choke: %.2f m, %.0f deg off its back", g_caCandDistCm.load() / 100.0f, g_caCandBackDeg.load());
    else ImGui::TextDisabled("no guard to choke right now");
}
