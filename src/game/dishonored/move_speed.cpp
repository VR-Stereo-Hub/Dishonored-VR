// game/dishonored/move_speed.cpp - included by src/mod/dishonoredvr.cpp (unity build), after
// anim_state.cpp and reflect.cpp.
//
// VR-204: every walking direction at one speed. A crouched walk slowed as the LEFT stick
// swung off straight ahead. The 2026-09-22 move/trace run named two causes, both the game's:
//
// 1. Diagonals walked at half speed. The game flags bIsWalking (half speed) when its stick
//    reading is under 0.85, and reads each axis through its own per-axis deadzone, so a full
//    diagonal push read short. Fixed at the source, in pad_bridge's GameMoveStick.
// 2. Sideways was slower. The player's attribute tweak (DisTweaks_PlayerPawn_Attributes)
//    carries strafe and backward multipliers for Run and Sneak, one base value per difficulty
//    (Normal ships Strafe 0.70/0.50, Backward 0.80/0.50). Standing, they scale the run speed,
//    so Run's two are raised to 1.0. CROUCHED, the strafe multiplier also scales the RUN speed
//    (0.9 x 0.5 = 0.450 sideways, measured), not the crouch speed that forward and backward
//    use (0.619). Raising it to 1.0 made a crouched strafe run at the full standing 0.900
//    (the second headset run, 2026-09-22: far too fast). So Sneak's strafe multiplier is set
//    to GroundSpeedCrouch / GroundSpeed for its difficulty, which makes a crouched strafe
//    exactly as fast as a crouched walk forward. The backward Sneak multiplier had no
//    measurable effect in either run (crouched backward read 0.619 both times) and is left
//    as shipped, and so are Sprint's: a sprint only runs forward.
//    A closed loop that matched the settled sideways speed to forward was tried and removed:
//    it never fired in the headset run that judged the ratio alone good, and it cost a
//    per-tick collision read on the game thread.
//
// COST: NO OBJECT-TABLE SCAN. Two builds found the tweak objects by walking GObjects on the
// game thread and both cost frames: 512 slots every 10 ms took the stereo present rate from
// 236/s to 177/s, and a "one pass per level" version re-ran forever (an archetype stays all
// zeros, so its retry never ended) at 4096 slots a tick and was worse. The pawn names its own
// objects: DishonoredPawn.m_pPawnTweaks -> DisTweaks_Pawn.m_pAttributeTweaks[EDifficulty],
// one attribute object per difficulty. Four pointer reads, once a second at most.
//
// The move/trace line ([Anim] MoveTrace, read-only) stays: it prints the game's speed answer,
// the input, and the stick angle, so a direction that is still slower is one line to find.
// Every field is resolved by name in one GObjects walk.
#include "game/dishonored/move_speed.h"

namespace dvr::movespeed {
namespace {

bool enabled = false;         // [Anim] MoveTrace (diagnostic, default off: it costs frames)
bool resolvedOk = false, triedResolve = false;
unsigned long long nextTry = 0, nextLine = 0;

enum F { fRot, fVel, fAcc, fInput, fFwd, fStr, fBaseY, fTurn, fGround, fWalkPct, fCrouchPct,
         fMsm, fSpeedMod, fOldFwd, fOldStr, fCrouched, fWalking, fBack, fCount };
RflWant want[fCount] = {
    { "Actor", "Rotation", false }, { "Actor", "Velocity", false }, { "Actor", "Acceleration", false },
    { "PlayerController", "PlayerInput", false },
    { "PlayerInput", "aForward", false }, { "PlayerInput", "aStrafe", false },
    { "PlayerInput", "aBaseY", false }, { "PlayerInput", "aTurn", false },
    { "Pawn", "GroundSpeed", false }, { "Pawn", "WalkingPct", false }, { "Pawn", "CrouchedPct", false },
    { "Pawn", "MovementSpeedModifier", false },
    { "DishonoredPawn", "m_fLastSpeedModifier", false },
    { "DishonoredPlayerInput", "m_fOld_aForward", false }, { "DishonoredPlayerInput", "m_fOld_aStrafe", false },
    { "Pawn", "bIsCrouched", true }, { "Pawn", "bIsWalking", true },
    { "DishonoredPawn", "m_bMovingBackwards", true },
};

// ---- the strafe/backward multipliers (fix 2) ----
const char* const kMulNames[4] = { "m_GroundStrafeMultiplierRun", "m_GroundStrafeMultiplierSneak",
                                   "m_GroundBackwardMultiplierRun", "m_GroundBackwardMultiplierSneak" };
const char* const kBaseNames[4] = { "m_fBaseValue1_Easy", "m_fBaseValue2_Normal",
                                    "m_fBaseValue3_Hard", "m_fBaseValue4_VeryHard" };
RflWant mulWant[12] = {
    { "DisTweaks_Pawn_Attributes", kMulNames[0], false }, { "DisTweaks_Pawn_Attributes", kMulNames[1], false },
    { "DisTweaks_Pawn_Attributes", kMulNames[2], false }, { "DisTweaks_Pawn_Attributes", kMulNames[3], false },
    { "DisAttribute", kBaseNames[0], false }, { "DisAttribute", kBaseNames[1], false },
    { "DisAttribute", kBaseNames[2], false }, { "DisAttribute", kBaseNames[3], false },
    { "DisTweaks_Pawn_Attributes", "m_GroundSpeed", false },
    { "DisTweaks_PlayerPawn_Attributes", "m_GroundSpeedCrouch", false },
    { "DishonoredPawn", "m_pPawnTweaks", false },
    { "DisTweaks_Pawn", "m_pAttributeTweaks", false },
};
bool mulResolved = false, mulRefused = false;
unsigned long long mulNextTry = 0;
unsigned long long mulNextLook = 0;
uint8_t* mulPawn = nullptr;
uint8_t* mulDone[16] = {};
int mulDoneN = 0;

const char* tweakName(uint8_t* o) {
    const char* n = RangeReadable(o + kNameOff, 4) ? RealName(*(uint32_t*)(o + kNameOff)) : nullptr;
    return n ? n : "?";
}

float base(uint8_t* tweak, int attr, int d) {
    const float* v = (const float*)(tweak + mulWant[attr].off + mulWant[4 + d].off);
    return RangeReadable(v, 4) ? *v : NAN;
}

bool mul_fix(uint8_t* tweak) {
    if (!(base(tweak, 8, 1) > 0.0f)) return false;   // not loaded yet: the next pass takes it
    char shipped[320] = {}; size_t used = 0; int changed = 0;
    float target[4][4];
    for (int d = 0; d < 4; ++d) {
        const float run = base(tweak, 8, d), crouch = base(tweak, 9, d);
        target[0][d] = target[2][d] = 1.0f;                        // standing: every direction runs
        // Crouched strafe = run speed x this, so crouch / run is a crouched walk forward.
        target[1][d] = (run > 0.0f && crouch > 0.0f && crouch <= run) ? crouch / run : NAN;
        target[3][d] = NAN;                                        // backward sneak: as shipped
    }
    used += _snprintf_s(shipped + used, sizeof(shipped) - used, _TRUNCATE, "GroundSpeed=%.0f/%.0f/%.0f/%.0f "
        "GroundSpeedCrouch=%.0f/%.0f/%.0f/%.0f", base(tweak, 8, 0), base(tweak, 8, 1), base(tweak, 8, 2),
        base(tweak, 8, 3), base(tweak, 9, 0), base(tweak, 9, 1), base(tweak, 9, 2), base(tweak, 9, 3));
    for (int m = 0; m < 4; ++m) {
        used += _snprintf_s(shipped + used, sizeof(shipped) - used, _TRUNCATE, " %s=", kMulNames[m] + 8);
        for (int d = 0; d < 4; ++d) {
            float* v = (float*)(tweak + mulWant[m].off + mulWant[4 + d].off);
            if (!RangeReadable(v, 4)) continue;
            const float want = target[m][d];
            const bool set = *v > 0.0f && std::isfinite(want) && (m == 1 ? *v != want : *v < want);
            used += _snprintf_s(shipped + used, sizeof(shipped) - used, _TRUNCATE, "%s%.3f", d ? "/" : "", *v);
            if (set) {
                *v = want; ++changed;
                used += _snprintf_s(shipped + used, sizeof(shipped) - used, _TRUNCATE, "->%.3f", *v);
            }
        }
    }
    Log("move/speed: %s '%s' (easy/normal/hard/very hard) %s | %d value(s) changed. Run strafe/backward "
        "-> 1.0; Sneak strafe -> GroundSpeedCrouch / GroundSpeed; Sneak backward and Sprint as shipped. "
        "Honoured check (move/trace): crouched, every stickAng shows the forward mod= (VR-204)",
        ObjClassName(tweak), tweakName(tweak), shipped, changed);
    return true;
}

// Script lane, bounded: 512 object slots per call, cycling for the whole session, so a tweak
// object a level load brings in is found within a second or two.
void mul_tick() {
    if (mulRefused) return;
    const auto now = GetTickCount64();
    if (!mulResolved) {
        if (now < mulNextTry) return;
        mulNextTry = now + 5000;
        if (!RflResolveBatch(mulWant, 12)) return;
        mulResolved = true;
        for (int k = 0; k < 12; ++k) mulResolved = mulResolved && mulWant[k].found;
        if (!mulResolved) {
            mulRefused = true;
            Log("move/speed: REFUSED - the attribute layout did not resolve by name (%s%s%s%s | %s%s%s%s | %s%s | %s%s); "
                "sideways stays at the game's strafe speed",
                mulWant[0].found?"+":"-", mulWant[1].found?"+":"-", mulWant[2].found?"+":"-", mulWant[3].found?"+":"-",
                mulWant[4].found?"+":"-", mulWant[5].found?"+":"-", mulWant[6].found?"+":"-", mulWant[7].found?"+":"-",
                mulWant[8].found?"+":"-", mulWant[9].found?"+":"-", mulWant[10].found?"+":"-", mulWant[11].found?"+":"-");
            return;
        }
        Log("move/speed: attribute layout resolved (strafe run +0x%x, sneak +0x%x, backward run +0x%x, sneak "
            "+0x%x; base values +0x%x..+0x%x)", mulWant[0].off, mulWant[1].off, mulWant[2].off, mulWant[3].off,
            mulWant[4].off, mulWant[7].off);
    }
    // The pawn's own four attribute objects. Once a second at most, and nothing to do once all
    // four are held for this pawn.
    if (mulDoneN >= 4 || now < mulNextLook || !mulPawn) return;
    mulNextLook = now + 1000;
    uint8_t* tweaks = nullptr;
    if (!RangeReadable(mulPawn + mulWant[10].off, 4)) return;
    tweaks = *(uint8_t**)(mulPawn + mulWant[10].off);
    if (!tweaks || !IsLiveObject(tweaks)) return;
    for (int d = 0; d < 4; ++d) {
        uint8_t* const* slot = (uint8_t* const*)(tweaks + mulWant[11].off + 4 * d);
        if (!RangeReadable(slot, 4)) continue;
        uint8_t* o = *slot;
        if (!o || !IsLiveObject(o)) continue;
        bool done = false;
        for (int k = 0; k < mulDoneN; ++k) done = done || mulDone[k] == o;
        if (done) continue;
        const char* cls = ObjClassName(o);
        if (!cls || !strstr(cls, "PlayerPawn_Attributes")) {
            DVR_LOG_ONCE(DVR_CAT, ::dvr::log::Level::Warn,
                "move/speed: the pawn's attribute tweak [%d] is a '%s', not a player attribute object - left alone",
                d, cls ? cls : "?");
            continue;
        }
        if (mul_fix(o) && mulDoneN < 16) mulDone[mulDoneN++] = o;
    }
}

bool rd(uint8_t* o, int f, void* dst, size_t n) {
    if (!o || !want[f].found || !RangeReadable(o + want[f].off, n)) return false;
    memcpy(dst, o + want[f].off, n); return true;
}
float rf(uint8_t* o, int f) { float v = NAN; rd(o, f, &v, 4); return v; }
int rb(uint8_t* o, int f) {
    uint32_t v = 0;
    if (!want[f].mask || !rd(o, f, &v, 4)) return -1;
    return (v & want[f].mask) ? 1 : 0;
}
int yawOf(uint8_t* o) { int32_t r[3] = {}; return rd(o, fRot, r, 12) ? r[1] : INT_MIN; }
float deg(int32_t u) { return (float)(int16_t)(u & 0xFFFF) * (360.0f / 65536.0f); }   // -180..180
float wrap(float d) { while (d > 180.0f) d -= 360.0f; while (d < -180.0f) d += 360.0f; return d; }
float heading(const float* v) { return atan2f(v[1], v[0]) * (180.0f / 3.14159265f); }

void resolve() {
    if (resolvedOk) return;
    const auto now = GetTickCount64();
    if (triedResolve && now < nextTry) return;
    triedResolve = true; nextTry = now + 5000;   // one walk every 5 s at most, until it lands
    if (!RflResolveBatch(want, fCount)) return;
    char miss[256] = {}; size_t used = 0; int missing = 0;
    for (int k = 0; k < fCount; ++k) if (!want[k].found) {
        ++missing;
        used += _snprintf_s(miss + used, sizeof(miss) - used, _TRUNCATE, "%s%s::%s",
                            missing > 1 ? ", " : "", want[k].cls, want[k].prop);
        if (used >= sizeof(miss) - 1) break;
    }
    // The three the question cannot be asked without: the rotations and the velocity.
    resolvedOk = want[fRot].found && want[fVel].found;
    Log("move/trace: resolved %d of %d fields by name%s%s%s", fCount - missing, fCount,
        missing ? " - unresolved (their columns read nan / -1): " : "", missing ? miss : "",
        resolvedOk ? "" : " - REFUSED: Actor::Rotation or Actor::Velocity missing, the trace stays off");
}

} // namespace

void configure(const char* ini) {
    enabled = GetPrivateProfileIntA("Anim", "MoveTrace", 0, ini) != 0;
    Log("config: [Anim] MoveTrace=%d (VR-204: read-only; while the pawn moves, 10 lines/s "
        "crouched, 2/s standing, plus one line on every change of the game's speed answer)", enabled);
}

// Script lane, from dvr::anim's bounded sample (about every 10 ms). `pawn` is the validated
// possessed DishonoredPlayerPawn or null.
void sample(uint8_t* ctrl, uint8_t* pawn) {
    if (pawn && pawn != mulPawn) { mulPawn = pawn; mulDoneN = 0; mulNextLook = 0; }   // a level load
    if (pawn) mul_tick();   // the fix: returns at once when the pawn's four objects are held
    if (!enabled || !pawn || !ctrl || !IsLiveObject(ctrl) || !IsLiveObject(pawn)) return;
    resolve();
    if (!resolvedOk) return;
    const auto now = GetTickCount64();

    float vel[3] = {}, acc[3] = {};
    rd(pawn, fVel, vel, 12); rd(pawn, fAcc, acc, 12);
    const float spd = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
    const float accMag = sqrtf(acc[0] * acc[0] + acc[1] * acc[1]);
    const int crouched = rb(pawn, fCrouched), back = rb(pawn, fBack), walking = rb(pawn, fWalking);
    const float mod = rf(pawn, fSpeedMod);

    // The game's answer changing is the event this whole trace exists to catch: log it at
    // once (bounded), whatever the cadence below says.
    static float lastMod = NAN; static int lastBack = -2, lastWalk = -2, lastCrouch = -2;
    const bool changed = (mod != lastMod && !(std::isnan(mod) && std::isnan(lastMod))) ||
                         back != lastBack || walking != lastWalk || crouched != lastCrouch;
    lastMod = mod; lastBack = back; lastWalk = walking; lastCrouch = crouched;

    const bool moving = spd > 10.0f || accMag > 1.0f;
    if (!moving && !changed) return;
    if (!changed && now < nextLine) return;
    // The game eases its answer over about 0.5 s, so CHANGE fires every tick of a transition:
    // at most one line per 50 ms.
    static unsigned long long nextChange = 0;
    if (changed && now < nextChange && now < nextLine) return;
    if (changed) nextChange = now + 50;
    nextLine = now + (crouched == 1 ? 100 : 500);

    const int cy = yawOf(ctrl), py = yawOf(pawn);
    const float ctrlYaw = cy == INT_MIN ? NAN : deg(cy), pawnYaw = py == INT_MIN ? NAN : deg(py);
    const float velYaw = spd > 10.0f ? heading(vel) : NAN, accYaw = accMag > 1.0f ? heading(acc) : NAN;
    uint8_t* input = nullptr;
    if (want[fInput].found && RangeReadable(ctrl + want[fInput].off, 4)) {
        input = *(uint8_t**)(ctrl + want[fInput].off);
        if (input && !IsLiveObject(input)) input = nullptr;
    }
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 1,
        "move/trace: columns - crouch/walk/back are the pawn's bools (-1 unresolved); mod = "
        "DishonoredPawn m_fLastSpeedModifier (the game's answer); gs = GroundSpeed, msm = "
        "MovementSpeedModifier; yaws in degrees, world frame; vel-pawn / acc-pawn / pawn-ctrl / "
        "vel-ctrl are the gaps any strafe or backward rule could be measuring; stickAng = the delivered "
        "left stick's angle from straight ahead; in = PlayerInput "
        "aForward/aStrafe (engine-scaled) and aBaseY/aTurn; old = DishonoredPlayerInput "
        "m_fOld_aForward/_aStrafe; stick = the pad's delivered left stick. A speed drop with every "
        "gap under 20 deg refutes the angle theory.");
    Log("move/trace:%s crouch=%d walk=%d back=%d mod=%.3f spd=%.0f gs=%.0f msm=%.2f wpct=%.2f cpct=%.2f"
        " | ctrl=%.1f pawn=%.1f vel=%.1f acc=%.1f | vel-pawn=%+.1f acc-pawn=%+.1f pawn-ctrl=%+.1f"
        " vel-ctrl=%+.1f | stickAng=%+.0f | in fwd=%.0f str=%.0f baseY=%.2f turn=%.0f old=(%.0f,%.0f) stick=(%.2f,%.2f)",
        changed ? " CHANGE" : "", crouched, walking, back, mod, spd, rf(pawn, fGround), rf(pawn, fMsm),
        rf(pawn, fWalkPct), rf(pawn, fCrouchPct),
        ctrlYaw, pawnYaw, velYaw, accYaw,
        wrap(velYaw - pawnYaw), wrap(accYaw - pawnYaw), wrap(pawnYaw - ctrlYaw), wrap(velYaw - ctrlYaw),
        (g_dbgOutLx || g_dbgOutLy) ? atan2f((float)g_dbgOutLx, (float)g_dbgOutLy) * (180.0f / 3.14159265f) : NAN,
        rf(input, fFwd), rf(input, fStr), rf(input, fBaseY), rf(input, fTurn),
        rf(input, fOldFwd), rf(input, fOldStr),
        g_dbgOutLx / 32767.0f, g_dbgOutLy / 32767.0f);
}

} // namespace dvr::movespeed
