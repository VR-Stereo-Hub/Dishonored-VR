// game/dishonored/delta_clamp.cpp - AER's delta clamp (VR-39): one world advance per eye pair.
// Included by the unity build after cinematic_trace.cpp (the identity helpers).
//
// THE PROBLEM. Under AlternateEye (`stereo aer`) every game tick draws ONE eye: left, right,
// left, right. Each tick also advances the world, so the right eye's picture is one tick later
// than the left one it is paired with. Anything that moves (a guard walking, a thrown bottle,
// the player strafing) sits in a different place in each eye: the classic AER ghosting.
//
// THE PORT (BioShock Remastered VR, CameraHook.cpp `DeltaClamp`). BRVR hooks the engine's
// frame-delta function, passes ~0 on the right-eye frame and banks the delta it withheld, then
// pays the bank out on the next left-eye frame. The world advances once per PAIR, both eyes of a
// pair show the same instant, and total game time is preserved.
//
// THE DISHONORED LEVERS. No hook: the game's own time-dilation fields, resolved by NAME (no
// address or offset is hardcoded), selected by [Stereo] DeltaClampLever / `aer lever <name>`:
//   bendtime      (default) the Bend Time power's own outputs, GameInfo.m_fCurrentWorldTimeDilation
//                 and m_fCurrentPlayerTimeDilation - the "slow time" mechanic, world and player
//                 together. Both are TRANSIENT: a save can never capture a value this module wrote.
//   timedilation  WorldInfo.TimeDilation, UE3's global time scale (the Slomo cheat's lever).
//                 NOT transient, and Dishonored's scripts declare no SaveGame flags at all, so a
//                 save taken mid-pair may store a frozen or doubled value: the fallback only.
// Both compose with the game: a value in the field that is not this module's is the game's base
// (1, Bend Time, Slomo, a scripted slow-motion), and the clamp multiplies it, never replaces it.
//
// PER TICK (game thread, scene_draw's stub, after the tick's draw), for the NEXT tick:
//   next is the RIGHT eye:  field = base * kFreeze; the real seconds that tick withholds are banked
//                           (measured afterwards from WorldInfo.RealtimeDeltaSeconds).
//   next is the LEFT eye:   field = base * (1 + bank / smoothed dt), paying the bank out; what it
//                           over- or under-paid stays banked (delta_clamp_policy.h, host-tested).
// kFreeze is not 0: a script dividing by a dilation must never see a zero.
//
// ACCEPTANCE IS MEASURED (the beat line): WorldInfo.DeltaSeconds - what the world actually
// advanced - per eye (right near 0, left near twice the real dt); world time against real time
// (equal when time is preserved); and INTEREYE, how far the game's camera base moved between a
// left draw and the right draw after it, the quantity the clamp exists to drive to zero. INTEREYE is
// measured with the clamp OFF too, so the A/B reads straight from the log.
//
// FAIL SOFT. A lever the engine does not honour is stood down, not trusted: while every base is 1,
// two consecutive beats of clamped pairs whose right ticks advance more than half as far as their
// left ticks, or whose world/real ratio leaves 0.6..1.5, switch the clamp off for the session with
// a Warn naming the lever. Every engine write validates the owner's identity (IsLiveObject against a
// live set rebuilt on every load/UI edge, its GObjects slot and class/name), and the game's own
// value is written back whenever the clamp stops (AER off, a non-stereo tick, the toggle).

#include <atomic>
#include "game/dishonored/delta_clamp_policy.h"

namespace {
enum DtcLever { kLeverBendTime = 0, kLeverTimeDilation = 1 };
std::atomic<bool> g_dtcOn{false};               // [Stereo] DeltaClamp, `aer clamp on|off`, F10
std::atomic<int>  g_dtcLever{kLeverBendTime};   // [Stereo] DeltaClampLever, `aer lever <name>`
const char* DtcLeverName(int l) { return l == kLeverTimeDilation ? "timedilation" : "bendtime"; }

// One field the clamp scales, on one owner object.
struct DtcChannel {
    const char* name = "";
    int owner = 0;                                 // 0 WorldInfo, 1 GameInfo
    uint32_t off = 0;
    float base = 1.0f;                             // the game's own value
    uint32_t writtenBits = 0;                      // what this module last put there
    bool written = false;
    uint32_t history[16] = {};                     // recent values of ours (a script restoring one is not a base)
    uint32_t historyAt = 0;
};
DtcChannel g_dtcCh[2];
int g_dtcChN = 0;
int g_dtcChLever = -1;                             // the lever the channel list was built for

uint32_t g_dtcWorldOff = 0, g_dtcGameOff = 0, g_dtcDilOff = 0, g_dtcDeltaOff = 0, g_dtcRealDeltaOff = 0;
uint32_t g_dtcBendWorldOff = 0, g_dtcBendPlayerOff = 0;
bool g_dtcResolved = false;
double g_dtcResolveAt = 0;
CtIdentity g_dtcWorld, g_dtcGame;
bool g_dtcHaveOwners = false;
LONG g_dtcLoad = -1;
unsigned g_dtcEpoch = 0;
double g_dtcRetryAt = 0;
float g_dtcFactor = 1.0f;                          // the factor the tick now running was given
int g_dtcPendingEye = 0;                           // the eye that tick was set up for (0 = none)
float g_dtcAvgDt = 0.0f;                           // smoothed real tick length (the left tick's prediction)
float g_dtcBank = 0.0f;                            // real seconds owed to the next left tick
const char* g_dtcWhy = "startup";
// the beat (reset every 3 s)
struct DtcBeat {
    uint32_t ticks[2] = {}, clamped[2] = {};       // [0] left, [1] right
    float dMin[2] = {1e9f, 1e9f}, dMax[2] = {0, 0};
    double dSumClamped[2] = {};                    // world advance of CLAMPED ticks, per eye (the honour check)
    double worldSum = 0, realSum = 0;
    bool basesUnit = true;                         // every base read 1 all beat (the checks are meaningful)
    uint32_t releases = 0, bankResets = 0, baseChanges = 0, staleRestores = 0, refused = 0;
    double ieSum = 0, ieMax = 0; uint32_t ieN = 0;
};
DtcBeat g_dtcBeat;
uint64_t g_dtcBeatMs = 0;
uint32_t g_dtcWrites = 0, g_dtcRestores = 0, g_dtcBadBeats = 0;
bool g_dtcLeftBaseOk = false;
float g_dtcLeftBase[3] = {};

uint32_t DtcBits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
bool DtcSane(float f) { return std::isfinite(f) && f >= 0.00001f && f <= 20.0f; }
bool DtcOurs(const DtcChannel& c, uint32_t bits) {
    for (uint32_t b : c.history) if (b && b == bits) return true;
    return false;
}

bool DtcResolve() {
    if (g_dtcResolved) return true;
    const double now = MaimNowMs();
    if (now < g_dtcResolveAt) return false;
    g_dtcResolveAt = now + 5000;
    if (!FindPropOffsetChecked("Actor", "WorldInfo", &g_dtcWorldOff) ||
        !FindPropOffsetChecked("WorldInfo", "Game", &g_dtcGameOff) ||
        !FindPropOffsetChecked("WorldInfo", "TimeDilation", &g_dtcDilOff) ||
        !FindPropOffsetChecked("WorldInfo", "DeltaSeconds", &g_dtcDeltaOff) ||
        !FindPropOffsetChecked("WorldInfo", "RealtimeDeltaSeconds", &g_dtcRealDeltaOff) ||
        !FindPropOffsetChecked("GameInfo", "m_fCurrentWorldTimeDilation", &g_dtcBendWorldOff) ||
        !FindPropOffsetChecked("GameInfo", "m_fCurrentPlayerTimeDilation", &g_dtcBendPlayerOff)) {
        g_dtcWhy = "reflection unavailable (WorldInfo.Game/TimeDilation/DeltaSeconds/RealtimeDeltaSeconds, "
                   "GameInfo.m_fCurrentWorld/PlayerTimeDilation) - retry in 5 s";
        return false;
    }
    g_dtcResolved = true;
    Log("aer/clamp: reflected Actor.WorldInfo +0x%x WorldInfo.Game +0x%x .TimeDilation +0x%x .DeltaSeconds +0x%x "
        ".RealtimeDeltaSeconds +0x%x GameInfo.m_fCurrentWorldTimeDilation +0x%x .m_fCurrentPlayerTimeDilation +0x%x "
        "(resolved by name; no address is hardcoded)", g_dtcWorldOff, g_dtcGameOff, g_dtcDilOff, g_dtcDeltaOff,
        g_dtcRealDeltaOff, g_dtcBendWorldOff, g_dtcBendPlayerOff);
    return true;
}

void DtcBuildChannels(int lever) {
    g_dtcCh[0] = DtcChannel{}; g_dtcCh[1] = DtcChannel{};
    if (lever == kLeverTimeDilation) {
        g_dtcCh[0].name = "WorldInfo.TimeDilation"; g_dtcCh[0].owner = 0; g_dtcCh[0].off = g_dtcDilOff;
        g_dtcChN = 1;
    } else {
        g_dtcCh[0].name = "GameInfo.m_fCurrentWorldTimeDilation"; g_dtcCh[0].owner = 1; g_dtcCh[0].off = g_dtcBendWorldOff;
        g_dtcCh[1].name = "GameInfo.m_fCurrentPlayerTimeDilation"; g_dtcCh[1].owner = 1; g_dtcCh[1].off = g_dtcBendPlayerOff;
        g_dtcChN = 2;
    }
    g_dtcChLever = lever;
}

uint8_t* DtcOwner(int which) {
    const CtIdentity& id = which ? g_dtcGame : g_dtcWorld;
    return g_dtcHaveOwners && ChSlot(id) ? (uint8_t*)id.value.obj : nullptr;
}

// The live WorldInfo and GameInfo behind the event controller, identity-checked. Refreshes the live
// set on a load/UI edge or a lost identity before trusting IsLiveObject (a stale table is the crash class).
bool DtcOwners() {
    if (!DtcResolve()) return false;
    const unsigned epoch = UiSurfaceEpoch();
    const bool edge = !g_dtcHaveOwners || g_dtcLoad != g_mkLoadEvents || g_dtcEpoch != epoch ||
                      !ChSlot(g_dtcWorld) || !ChSlot(g_dtcGame);
    if (!edge) return true;
    const double now = MaimNowMs();
    if (now < g_dtcRetryAt) { g_dtcWhy = "waiting for the owner refresh"; return false; }
    g_dtcRetryAt = now + 1000;
    void* const prevWorld = g_dtcHaveOwners ? g_dtcWorld.value.obj : nullptr;
    void* const prevGame = g_dtcHaveOwners ? g_dtcGame.value.obj : nullptr;
    // A load edge needs the table rebuilt NOW (the new level's objects); a UI edge or a lost slot
    // can share another caller's rebuild from the last half second (VR-160), so opening the wheel
    // mid-pair does not pay a full rebuild of its own.
    const bool loadEdge = !g_dtcHaveOwners || g_dtcLoad != g_mkLoadEvents;
    g_dtcHaveOwners = false;
    if (!(loadEdge ? BuildLiveSet() : RefreshLiveSet(500))) { g_dtcWhy = "live-object refresh failed"; return false; }
    uint8_t* pc = IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* world = CtObject(pc, g_dtcWorldOff);
    uint8_t* game = CtObject(world, g_dtcGameOff);
    const char* wn = world ? ObjClassName(world) : nullptr;
    const char* gn = game ? ObjClassName(game) : nullptr;
    if (!world || !wn || !strstr(wn, "WorldInfo") || !game || !gn || !strstr(gn, "GameInfo") ||
        !RangeReadable(world + g_dtcDilOff, 4) || !RangeReadable(world + g_dtcRealDeltaOff, 4) ||
        !RangeReadable(game + g_dtcBendWorldOff, 4) || !RangeReadable(game + g_dtcBendPlayerOff, 4)) {
        g_dtcWhy = "no live WorldInfo/GameInfo behind the event controller"; return false;
    }
    if (!ChCapture(world, &g_dtcWorld) || !ChCapture(game, &g_dtcGame)) { g_dtcWhy = "owner identity capture failed"; return false; }
    g_dtcHaveOwners = true; g_dtcLoad = g_mkLoadEvents; g_dtcEpoch = epoch; g_dtcRetryAt = 0;
    // NEW owners start on their own values; nothing of ours survives into them. The SAME owners
    // (a menu or UI edge) may still hold our last write, so their bookkeeping is kept.
    if (world != prevWorld || game != prevGame) {
        DtcBuildChannels(g_dtcLever.load());
        g_dtcBank = 0; g_dtcFactor = 1; g_dtcPendingEye = 0; g_dtcLeftBaseOk = false;
    }
    Log("aer/clamp: owners revalidated WorldInfo=%p GameInfo=%p (controller %p, load %ld, UI epoch %u)",
        world, game, pc, g_dtcLoad, g_dtcEpoch);
    return true;
}

bool DtcWrite(DtcChannel& c, float value) {
    uint8_t* o = DtcOwner(c.owner);
    if (!o) return false;
    memcpy(o + c.off, &value, 4);
    return true;
}

void DtcDropPair() { g_dtcBank = 0; g_dtcFactor = 1; g_dtcPendingEye = 0; }
} // namespace

static bool DeltaClampEnabled() { return g_dtcOn.load(); }

// Put the game's own values back wherever a field still holds ours. Game thread.
static void DeltaClampRelease(const char* why) {
    bool any = false;
    for (int i = 0; i < g_dtcChN; ++i) {
        DtcChannel& c = g_dtcCh[i];
        if (!c.written) continue;
        any = true;
        uint8_t* o = DtcOwner(c.owner);
        float cur = 0;
        if (o && CtRead(o, c.off, &cur, 4) && DtcBits(cur) == c.writtenBits && DtcWrite(c, c.base)) {
            ++g_dtcRestores;
            DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Info, 2000,
                "aer/clamp: released (%s) - %s back to the game's %.4f; bank %.4f s dropped",
                why, c.name, c.base, g_dtcBank);
        }
        c.written = false;
    }
    if (any) ++g_dtcBeat.releases;
    DtcDropPair();
}

static void DeltaClampSet(bool on, const char* who) {
    g_dtcOn.store(on);
    g_dtcBadBeats = 0;
    Log("aer/clamp: %s (%s, lever %s) - %s", on ? "ON" : "off", who, DtcLeverName(g_dtcLever.load()),
        on ? "one world advance per eye pair under `stereo aer`: the right-eye tick runs at 1% time and the left-eye "
             "tick pays the rest back; the game's own dilation (Bend Time, Slomo) is multiplied, never replaced"
           : "every tick advances the world; under `stereo aer` the right eye is one tick later than the left");
    ConfigWriteKey("Stereo", "DeltaClamp", on ? "1" : "0", who);
}

// The lever switch. Game-thread state is rebuilt at the next tick (the old lever's values are
// released first by the tick that sees the change).
static void DeltaClampSetLever(int lever, const char* who) {
    lever = lever == kLeverTimeDilation ? kLeverTimeDilation : kLeverBendTime;
    g_dtcLever.store(lever);
    g_dtcBadBeats = 0;
    Log("aer/clamp: lever -> %s (%s) - %s", DtcLeverName(lever), who,
        lever == kLeverTimeDilation ? "WorldInfo.TimeDilation (NOT transient: a save taken mid-pair may store a "
                                      "clamped value - the fallback only)"
                                    : "Bend Time's GameInfo world and player dilation (transient: never saved)");
    ConfigWriteKey("Stereo", "DeltaClampLever", DtcLeverName(lever), who);
}

static void DeltaClampConfigure(const char* ini) {
    g_dtcOn.store(GetPrivateProfileIntA("Stereo", "DeltaClamp", 0, ini) != 0);
    char lv[24] = "";
    GetPrivateProfileStringA("Stereo", "DeltaClampLever", "bendtime", lv, sizeof(lv), ini);
    const int lever = !_stricmp(lv, "timedilation") ? kLeverTimeDilation : kLeverBendTime;
    if (_stricmp(lv, "timedilation") && _stricmp(lv, "bendtime"))
        Log("config: [Stereo] DeltaClampLever='%s' unknown (bendtime|timedilation) - bendtime", lv);
    g_dtcLever.store(lever);
    Log("config: [Stereo] DeltaClamp=%d DeltaClampLever=%s (%s)", g_dtcOn.load() ? 1 : 0, DtcLeverName(lever),
        g_dtcOn.load() ? "one world advance per eye pair under `stereo aer`" : "off: each AER tick advances the world");
}

static void DeltaClampBeat() {
    const uint64_t now = GetTickCount64();
    if (!g_dtcBeatMs) { g_dtcBeatMs = now; return; }
    if (now - g_dtcBeatMs < 3000) return;
    g_dtcBeatMs = now;
    DtcBeat& b = g_dtcBeat;
    const double rMean = b.clamped[1] ? b.dSumClamped[1] / b.clamped[1] : 0.0;
    const double lMean = b.clamped[0] ? b.dSumClamped[0] / b.clamped[0] : 0.0;
    const double ratio = lMean > 0 ? rMean / lMean : 0.0;
    const double timeRatio = b.realSum > 0 ? b.worldSum / b.realSum : 0.0;
    if (b.ticks[0] + b.ticks[1] + b.ieN) {
        Log("aer/clamp: beat %s lever %s | ticks L=%u R=%u (clamped L=%u R=%u) | world advance L %.5f..%.5f s, "
            "R %.5f..%.5f s, clamped R/L mean %.3f (honoured: well under 0.5) | world/real %.3f (time preserved at 1 "
            "while every base is 1: %s) | INTEREYE camera travel left->right avg %.2f max %.2f uu over %u pairs "
            "(the world sliding between the eyes of a pair; the clamp's goal is 0) | bank %.4f s, resets %u, releases %u, "
            "base changes %u, stale restores %u, refused %u (%s) | writes %u restores %u",
            g_dtcOn.load() ? "ON" : "off", DtcLeverName(g_dtcLever.load()), b.ticks[0], b.ticks[1], b.clamped[0],
            b.clamped[1], b.dMin[0] > 1e8f ? 0.0f : b.dMin[0], b.dMax[0], b.dMin[1] > 1e8f ? 0.0f : b.dMin[1], b.dMax[1],
            ratio, timeRatio, b.basesUnit ? "yes" : "no, a base moved", b.ieN ? b.ieSum / b.ieN : 0.0, b.ieMax, b.ieN,
            g_dtcBank, b.bankResets, b.releases, b.baseChanges, b.staleRestores, b.refused, g_dtcWhy, g_dtcWrites,
            g_dtcRestores);
    }
    // FAIL SOFT: a lever the engine does not honour, or one that bends total time, is stood down.
    if (g_dtcOn.load() && b.basesUnit && b.clamped[0] >= 30 && b.clamped[1] >= 30) {
        const bool bad = ratio > 0.5 || timeRatio < 0.6 || timeRatio > 1.5;
        g_dtcBadBeats = bad ? g_dtcBadBeats + 1 : 0;
        if (g_dtcBadBeats >= 2) {
            g_dtcOn.store(false);
            g_dtcBadBeats = 0;
            DVR_LOG(dvr::log::Cat::present, dvr::log::Level::Warn,
                "aer/clamp: STOOD DOWN for this session - lever %s is not honoured the way the clamp needs (clamped "
                "R/L world advance %.3f, want well under 0.5; world/real %.3f, want 0.6..1.5) two beats running. The "
                "game's values are restored; `aer lever timedilation` or `aer lever bendtime` then `aer clamp on` "
                "tries the other lever. The ini is unchanged.", DtcLeverName(g_dtcLever.load()), ratio, timeRatio);
        }
    }
    b = DtcBeat{};
}

// Game thread, scene_draw's stub at depth 0 after the tick's draw. `eyeDrawn`: the eye this tick
// drew under AER (-1/+1; 0 = not an AER stereo tick). `nextEye`: the eye the next stereo tick will
// draw (0 = AER is off). `cam`: the live camera, for the INTEREYE instrument.
static void DeltaClampAfterDraw(int eyeDrawn, int nextEye, uint8_t* cam) {
    DeltaClampBeat();
    if (!eyeDrawn || !nextEye) {
        g_dtcLeftBaseOk = false;
        DeltaClampRelease(!nextEye ? "AER off" : "a tick without an AER eye");
        return;
    }
    // INTEREYE, clamp on or off: the game's camera base (our offsets removed) at the left draw
    // against the right draw that follows it.
    float basePos[3];
    const bool baseOk = cam && dvr::camera::game_base_pos(cam, basePos);
    if (eyeDrawn < 0) {
        g_dtcLeftBaseOk = baseOk;
        if (baseOk) memcpy(g_dtcLeftBase, basePos, sizeof(basePos));
    } else if (baseOk && g_dtcLeftBaseOk) {
        const double dx = basePos[0] - g_dtcLeftBase[0], dy = basePos[1] - g_dtcLeftBase[1], dz = basePos[2] - g_dtcLeftBase[2];
        const double d = sqrt(dx * dx + dy * dy + dz * dz);
        g_dtcBeat.ieSum += d; ++g_dtcBeat.ieN;
        if (d > g_dtcBeat.ieMax) g_dtcBeat.ieMax = d;
        g_dtcLeftBaseOk = false;
    }
    if (!g_dtcOn.load()) { DeltaClampRelease("toggled off"); return; }
    if (g_dtcChLever != g_dtcLever.load()) {   // a lever switch: the old fields go back first
        DeltaClampRelease("lever switched");
        if (g_dtcResolved) DtcBuildChannels(g_dtcLever.load());
    }
    if (!DtcOwners()) { ++g_dtcBeat.refused; DeltaClampRelease(g_dtcWhy); return; }
    if (g_dtcChLever != g_dtcLever.load()) DtcBuildChannels(g_dtcLever.load());
    uint8_t* world = DtcOwner(0);
    float worldDt = 0, realDt = 0, cur[2] = {};
    bool ok = world && CtRead(world, g_dtcDeltaOff, &worldDt, 4) && CtRead(world, g_dtcRealDeltaOff, &realDt, 4) &&
              std::isfinite(worldDt) && std::isfinite(realDt);
    for (int i = 0; ok && i < g_dtcChN; ++i) {
        uint8_t* o = DtcOwner(g_dtcCh[i].owner);
        ok = o && CtRead(o, g_dtcCh[i].off, &cur[i], 4);
    }
    if (!ok) { ++g_dtcBeat.refused; g_dtcWhy = "owner fields unreadable"; DeltaClampRelease(g_dtcWhy); return; }
    // Whose value is in each field? Ours; the game's own value back again (a field the game rewrites
    // every tick, as Bend Time's outputs may be); a value of ours a script restored; or a NEW game value.
    for (int i = 0; i < g_dtcChN; ++i) {
        DtcChannel& c = g_dtcCh[i];
        const uint32_t bits = DtcBits(cur[i]);
        if (c.written && bits == c.writtenBits) continue;                       // ours, as left
        if (c.written && bits == DtcBits(c.base)) { c.written = false; continue; }   // the game rewrote its own value
        if (DtcOurs(c, bits) && DtcSane(c.base)) {                              // an old value of ours came back
            ++g_dtcBeat.staleRestores;
            DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Warn, 5000,
                "aer/clamp: %s holds %.5f, a value this module wrote earlier (a script restoring a saved dilation) - "
                "the base stays %.4f", c.name, cur[i], c.base);
            c.written = true; c.writtenBits = bits;   // overwritten below like any of ours
            DtcDropPair();
            continue;
        }
        if (!DtcSane(cur[i])) {
            ++g_dtcBeat.refused; g_dtcWhy = "a game dilation is out of range";
            DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Warn, 5000,
                "aer/clamp: REFUSED - %s reads %.6f, outside 0.00001..20; hands off until it is sane", c.name, cur[i]);
            DeltaClampRelease(g_dtcWhy);
            return;
        }
        if (cur[i] != c.base) {
            ++g_dtcBeat.baseChanges;
            Log("aer/clamp: the game set %s %.4f -> %.4f (Bend Time, Slomo or a scripted slow-motion) - the new base; "
                "the pair in flight is dropped", c.name, c.base, cur[i]);
            DtcDropPair();
        }
        c.base = cur[i];
        c.written = false;
    }
    for (int i = 0; i < g_dtcChN; ++i) if (g_dtcCh[i].base != 1.0f) g_dtcBeat.basesUnit = false;
    // The tick that just ran, measured.
    const int k = eyeDrawn < 0 ? 0 : 1;
    ++g_dtcBeat.ticks[k];
    if (g_dtcPendingEye == eyeDrawn) { ++g_dtcBeat.clamped[k]; g_dtcBeat.dSumClamped[k] += worldDt; }
    if (worldDt < g_dtcBeat.dMin[k]) g_dtcBeat.dMin[k] = worldDt;
    if (worldDt > g_dtcBeat.dMax[k]) g_dtcBeat.dMax[k] = worldDt;
    g_dtcBeat.worldSum += worldDt; g_dtcBeat.realSum += realDt;
    // The bank: what the right tick withheld, less what the left tick paid (delta_clamp_policy.h).
    bool bankReset = false;
    const float bankBefore = g_dtcBank;
    g_dtcBank = dvr::delta_clamp::update_bank(g_dtcBank, g_dtcPendingEye, eyeDrawn, g_dtcFactor, realDt, &bankReset);
    if (bankReset) {   // a hitch or a load: never pay a lurch out in one tick
        ++g_dtcBeat.bankResets;
        DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Info, 2000,
            "aer/clamp: bank out of range after %.4f s (a hitch or a load; this tick %.4f s real) - dropped rather "
            "than paid out in one tick", bankBefore, realDt);
    }
    // The next tick.
    g_dtcAvgDt = dvr::delta_clamp::smooth_dt(g_dtcAvgDt, realDt);
    const float factor = dvr::delta_clamp::next_factor(nextEye, g_dtcBank, g_dtcAvgDt);
    for (int i = 0; i < g_dtcChN; ++i) {
        DtcChannel& c = g_dtcCh[i];
        const float value = c.base * factor;
        if (!DtcWrite(c, value)) {
            ++g_dtcBeat.refused; g_dtcWhy = "owner identity changed before the write";
            DeltaClampRelease(g_dtcWhy);
            return;
        }
        c.written = true; c.writtenBits = DtcBits(value);
        // Only values that differ from the base are ours to recognise: a factor of exactly 1 writes
        // the game's own value, and remembering it would refuse the game's legitimate return to it.
        if (factor != 1.0f && !DtcOurs(c, c.writtenBits)) c.history[c.historyAt++ % 16] = c.writtenBits;
    }
    g_dtcFactor = factor; g_dtcPendingEye = nextEye;
    ++g_dtcWrites; g_dtcWhy = "clamping";
}

static bool DeltaClampCommand(const char* sub, const char* v) {
    if (!strcmp(sub, "lever")) {
        if (!_stricmp(v, "bendtime")) { DeltaClampSetLever(kLeverBendTime, "the seam"); return true; }
        if (!_stricmp(v, "timedilation")) { DeltaClampSetLever(kLeverTimeDilation, "the seam"); return true; }
    }
    return false;
}

static void DeltaClampStatus(dvr::status::Writer& w) {
    w.kv("deltaClamp", g_dtcOn.load());
    w.kv("clampLever", DtcLeverName(g_dtcLever.load()));
    w.kv("clampBank", (double)g_dtcBank);
    w.kv("clampWrites", (unsigned long)g_dtcWrites);
    w.kv("clampRestores", (unsigned long)g_dtcRestores);
    w.kv("clampWhy", g_dtcWhy);
}
