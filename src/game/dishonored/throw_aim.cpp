// game/dishonored/throw_aim.cpp - VR-166: GRENADES (AND WHATEVER SHARES THE THROW) AIMED BY HAND.
//
// DisItemContext_ThrowGrenade's firing slot (+0x1B0 -> 0x00C3AC50) is a wrapper around
// the throw routine 0x00C38F70 (its other caller is a second wrapper at 0x00C3ABEB).
// After spawning the projectile at the hand (SpawnActor 0x00C66070 at 0x00C39053) the
// routine takes a ROTATOR by address into ebp-0x78 - the argument's +0x14, or the
// source pawn's +0xD0 when there is no argument - and converts it to the throw
// direction at ebp-0x74 (0x0040DA70 at 0x00C39093). That direction drives the launch
// velocity, is normalised (0x00C3980C) and handed on to the projectile; the rotator
// pointer is read again at 0x00C39828. So ONE substitution - the rotator pointer, just
// before the conversion - carries the hand through every later use in the routine.
//
// The bridge replaces the 7 bytes at 0x00C3908C (mov ecx,[ebp-78h]; lea edx,[ebp-74h];
// push edx - no relative operand, nothing jumps inside them) and points ebp-0x78 at a
// rotator built from the published hand ray. The spawn point stays where the game put
// it (in the hand); speed, arc and cooking stay the game's. Gated on the source pawn
// being the player's (ebp-0x58), so NPC throws that reach the routine are untouched.
// [Aim] ThrowFromHand (default 1) and the F10 Aim table choose; GamepadOnly keeps head.

#include <tlhelp32.h>
#define DVR_CAT ::dvr::log::Cat::script

static dvr::hooks::Detour g_thDet;
static std::atomic<bool> g_thOn{true};                  // [Aim] ThrowFromHand
static std::atomic<bool> g_gdOn{true};                  // [Aim] GadgetFromHand (below)
static std::atomic<bool> g_ctOn{true};                  // [Aim] CarryThrowFromHand (VR-181, below)
static uint8_t* volatile g_hlObj = nullptr;             // VR-181: the carried actor, for the move seam
static bool g_cwWanted = false;                         // VR-181 `carryaim watch`: arm on the next carry
static uintptr_t g_thBack = kThrowRotBack;
static volatile LONG g_thSeen = 0, g_thDriven = 0, g_thRefused = 0;
static const char* volatile g_thWhy = "not asked yet";
static int32_t g_thRot[3];                              // the engine reads it after we return

static bool ThrowAimEnabled() { return g_thOn.load(); }
static bool ThRefuse(const char* why) { g_thWhy = why; InterlockedIncrement(&g_thRefused); return false; }

extern "C" void __cdecl ThrowAimHandler(uint8_t* frame)
{
    InterlockedIncrement(&g_thSeen);
    // VR-166: the grenade COOK indicator has never been measured, and it has to be to put
    // it on our reticle. This is the release, when its draws are still in the census
    // window: dump the table (only if the census is on - [Draws] Census=1 - and only for
    // the first few throws). The cluster that exists only while cooking is the indicator.
    static LONG dumps = 0;
    if (dvr::hudclass::census_enabled() && InterlockedIncrement(&dumps) <= 4)
        dvr::hudclass::log_regions("grenade released - the cook indicator is the cluster seen only while cooking");
    if (!frame || !RangeReadable(frame - 0x7c, 0x28)) { ThRefuse("throw frame unreadable"); return; }
    uint8_t* ctx  = *(uint8_t**)(frame - 0x7c);
    uint8_t* pawn = *(uint8_t**)(frame - 0x58);
    int32_t* src  = *(int32_t**)(frame - 0x78);
    if (!g_thOn.load()) { ThRefuse("head aim selected"); return; }
    if (g_gamepadOnly) { ThRefuse("[Mode] GamepadOnly=1 keeps the head"); return; }
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) { ThRefuse("not in gameplay"); return; }
    if (!pawn || pawn != g_pePawn) { ThRefuse("not the player's throw"); return; }
    if (!src || !RangeReadable(src, 12)) { ThRefuse("source rotator unreadable"); return; }
    float o[3], d[3]; const char* why = nullptr;
    if (!HandRayWorld(o, d, &why)) { ThRefuse(why); return; }
    const float kU = 32768.0f / 3.14159265f;
    const float h = sqrtf(d[0] * d[0] + d[1] * d[1]);
    g_thRot[0] = (int32_t)(atan2f(d[2], h) * kU);
    g_thRot[1] = (int32_t)(atan2f(d[1], d[0]) * kU);
    g_thRot[2] = src[2];                                // roll stays the engine's
    const int32_t wasP = src[0], wasY = src[1];
    *(int32_t**)(frame - 0x78) = g_thRot;
    InterlockedIncrement(&g_thDriven);
    g_thWhy = "driving";
    const char* cn = (ctx && LooksLikeObj(ctx)) ? ObjClassName(ctx) : "?";
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 40,
        "throw/aim: %s thrown along the HAND - pitch %.1f -> %.1f deg, yaw %.1f -> %.1f deg "
        "(the engine's head aim -> ours; spawn point, speed and arc stay the game's)",
        cn, wasP / kU * 57.29578f, g_thRot[0] / kU * 57.29578f,
        wasY / kU * 57.29578f, g_thRot[1] / kU * 57.29578f);
}

__declspec(naked) static void ThrowAimThunk()
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        push ebp                    ; the throw routine's frame
        call ThrowAimHandler
        add esp, 4
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        mov ecx, [ebp-78h]          ; the seven displaced bytes
        lea edx, [ebp-74h]
        push edx
        jmp dword ptr [g_thBack]
    }
}

static void ThrowAimSet(bool on, const char* who)
{
    g_thOn.store(on);
    if (on && !g_thDet.on)
        dvr::hooks::detour_install(g_thDet, "throw/aim", kThrowRotSeam, kThrowRotSeamBytes,
                                   sizeof(kThrowRotSeamBytes), (void*)&ThrowAimThunk);
    dvr::aim::request_throw_ray((on || g_gdOn.load() || g_ctOn.load()) && !g_gamepadOnly);
    Log("throw/aim: owner %s (%s) - hook %s. Grenades (and any throw through the same "
        "routine) leave along the published hand ray; spawn point, speed and arc stay the game's",
        on ? "HAND" : "HEAD", who, g_thDet.on ? "ready" : "NOT installed");
}

static void ThrowAimConfigure(const char* ini)
{
    ThrowAimSet(IniFloat(ini, "Aim", "ThrowFromHand", 1) != 0.0f, "ini [Aim] ThrowFromHand");
}

static bool ThrowAimCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        ThrowAimSet(b, "seam");
        ConfigWriteKey("Aim", "ThrowFromHand", b ? "1" : "0", "the seam");
        return true;
    }
    Log("throw/aim: on|off (now %s) %ld/%ld driven/seen, refused %ld (last: %s). Seen moves "
        "only when something is thrown", g_thOn.load() ? "HAND" : "HEAD",
        (long)g_thDriven, (long)g_thSeen, (long)g_thRefused, g_thWhy);
    return true;
}

// ---- the spring razor's placement (VR-166) ------------------------------------------
// A razor is PLACED, not thrown. Its wall-placement trace 0x00C32C30 (the placement
// routine 0x00C3B570 calls it at 0x00C3B5BF, and spawns from the context's +0xB8/+0xC4
// it fills) takes the trace start and rotator from the camera POV via esi:
// [[owner+0x26C]+0x384]+0x330 is a location and +0x33C a rotator. esi holds that
// pointer only until 0x00C32CC8 (then xor esi,esi at 0x00C330E5), so replacing
// `add esi,330h` at 0x00C32C91 with "esi = the POV to use" moves the start, the
// direction and the wall/floor/ceiling pitch test (+-0x1FFF) to the hand in one step.
// Everything after (reach, surface fit, the preview) stays the game's. Found statically
// by following the lea of +0xB4 into this callee: the writes to +0xB8 are [reg+4] off
// that pointer, which is why a displacement search for 0xB8 never found them.
// The source must be within kRzNear of the rendered view or it is not the POV and we
// refuse (fail soft, reason logged). [Aim] GadgetFromHand (default 1) and F10 choose.
static dvr::hooks::Detour g_gdDet;
static uintptr_t g_gdBack = kRazorTraceBack;
static uint32_t g_gdUse = 0;                          // esi the trace continues with
static uint8_t g_gdPov[0x40];                         // location +0, rotator +0xC
static volatile LONG g_gdSeen = 0, g_gdDriven = 0;
static const char* volatile g_gdWhy = "not asked yet";
static const float kRzNear = 150.0f;                  // uu between the POV and the render eye

static bool GadgetAimEnabled() { return g_gdOn.load(); }

extern "C" void __cdecl GadgetAimHandler(uint8_t* base, uint8_t* frame)
{
    InterlockedIncrement(&g_gdSeen);
    uint8_t* pov = base + 0x330;
    g_gdUse = (uint32_t)(uintptr_t)pov;               // the engine's own choice
    const char* why = nullptr;
    float cam[3] = {0, 0, 0}, sep = -1.0f;
    if (!g_gdOn.load()) why = "head aim selected";
    else if (g_gamepadOnly) why = "[Mode] GamepadOnly=1 keeps the head";
    else if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) why = "not in gameplay";
    else if (!base || !RangeReadable(pov, sizeof(g_gdPov))) why = "POV unreadable";
    else if (!dvr::camera::render_pos_world(cam)) why = "render eye position unknown";
    else {
        const float* p = (const float*)pov;
        const float v[3] = { p[0] - cam[0], p[1] - cam[1], p[2] - cam[2] };
        sep = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        if (!(sep < kRzNear)) why = "the source is not the view location";
    }
    float o[3], d[3];
    if (!why && !HandRayWorld(o, d, &why)) {}
    const uint8_t* ctxCfg = (frame && RangeReadable(frame - 0x3c, 4)) ? *(uint8_t**)(frame - 0x3c) : nullptr;
    const char* cn = (ctxCfg && LooksLikeObj((uint8_t*)ctxCfg)) ? ObjClassName((uint8_t*)ctxCfg) : "?";
    if (why) {
        g_gdWhy = why;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 20,
            "gadget/aim: razor placement trace REFUSED: %s (POV %.0f uu from the render eye, "
            "limit %.0f; config %s) - the head places it", why, sep, kRzNear, cn);
        return;
    }
    memcpy(g_gdPov, pov, sizeof(g_gdPov));
    const int32_t* was = (const int32_t*)(pov + 0xC);
    const float kU = 32768.0f / 3.14159265f;
    const float h = sqrtf(d[0] * d[0] + d[1] * d[1]);
    float* L = (float*)g_gdPov; int32_t* R = (int32_t*)(g_gdPov + 0xC);
    L[0] = o[0]; L[1] = o[1]; L[2] = o[2];
    R[0] = (int32_t)(atan2f(d[2], h) * kU);
    R[1] = (int32_t)(atan2f(d[1], d[0]) * kU);
    R[2] = was[2];                                    // roll stays the engine's
    g_gdUse = (uint32_t)(uintptr_t)g_gdPov;
    InterlockedIncrement(&g_gdDriven);
    g_gdWhy = "driving";
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 12,
        "gadget/aim: razor placement traced from the HAND - start moved %.0f uu, pitch %.1f -> %.1f "
        "deg, yaw %.1f -> %.1f deg (POV was %.0f uu from the render eye; config %s)",
        sqrtf((o[0] - ((float*)pov)[0]) * (o[0] - ((float*)pov)[0]) + (o[1] - ((float*)pov)[1]) * (o[1] - ((float*)pov)[1]) +
              (o[2] - ((float*)pov)[2]) * (o[2] - ((float*)pov)[2])),
        (int16_t)was[0] / kU * 57.29578f, R[0] / kU * 57.29578f,
        (int16_t)was[1] / kU * 57.29578f, R[1] / kU * 57.29578f, sep, cn);
}

__declspec(naked) static void GadgetAimThunk()
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        push ebp                    ; the trace's frame: [ebp-3Ch] its config object
        push esi                    ; the POV's owner (before add esi,330h)
        call GadgetAimHandler
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        mov esi, dword ptr [g_gdUse] ; replaces: add esi,330h
        jmp dword ptr [g_gdBack]
    }
}

static void GadgetAimSet(bool on, const char* who)
{
    g_gdOn.store(on);
    if (on && !g_gdDet.on)
        dvr::hooks::detour_install(g_gdDet, "gadget/aim", kRazorTraceSeam, kRazorTraceSeamBytes,
                                   sizeof(kRazorTraceSeamBytes), (void*)&GadgetAimThunk);
    dvr::aim::request_throw_ray((on || g_thOn.load() || g_ctOn.load()) && !g_gamepadOnly);
    Log("gadget/aim: owner %s (%s) - hook %s. Spring razors are placed along the published hand "
        "ray (the placement trace 0x00C32C30); reach and surface fit stay the game's",
        on ? "HAND" : "HEAD", who, g_gdDet.on ? "ready" : "NOT installed");
}

static void GadgetAimConfigure(const char* ini)
{
    GadgetAimSet(IniFloat(ini, "Aim", "GadgetFromHand", 1) != 0.0f, "ini [Aim] GadgetFromHand");
}

static bool GadgetAimCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        GadgetAimSet(b, "seam");
        ConfigWriteKey("Aim", "GadgetFromHand", b ? "1" : "0", "the seam");
        return true;
    }
    Log("gadget/aim: on|off (now %s) %ld/%ld driven/seen (last: %s). Seen moves while a "
        "spring razor is out", g_gdOn.load() ? "HAND" : "HEAD", (long)g_gdDriven, (long)g_gdSeen, g_gdWhy);
    return true;
}

// ---- a carried object's throw (VR-181) ------------------------------------------------
// Bottles, rocks, crates: anything StatePlayerGrabMovable holds. Picking one up is already
// hand-aimed (InteractFromHand). The throw is native: the state's drop 0x00A698E0 calls the
// held item's release 0x00C45340 with m_bThrowOnDrop, which asks the pawn for its aim rotator
// (vtable +0x3E8), writes the unit direction to [ebp-0x24] (0x0040DA70 at 0x00C4552C) and
// then sets the body's linear velocity to dir * speed + the pawn's velocity (0x00C45641).
// Replacing the direction between the two moves the throw to the hand; the release point
// (where the object is held), the speed tweak, the random spin and the damage stay the
// game's. The engine's own direction must be a unit vector, or the frame is not the one
// this was derived from and we refuse. [Aim] CarryThrowFromHand (default 1) and F10 choose.
static dvr::hooks::Detour g_ctDet;
static uintptr_t g_ctBack = kCarryThrowBack;
// The displaced push imm32, replayed from the verified bytes so the thunk carries no address.
static uint32_t g_ctPushImm = (uint32_t)kCarryThrowSeamBytes[1] | ((uint32_t)kCarryThrowSeamBytes[2] << 8) |
                              ((uint32_t)kCarryThrowSeamBytes[3] << 16) | ((uint32_t)kCarryThrowSeamBytes[4] << 24);
static volatile LONG g_ctSeen = 0, g_ctDriven = 0, g_ctRefused = 0;
static const char* volatile g_ctWhy = "not asked yet";

static bool CarryThrowAimEnabled() { return g_ctOn.load(); }

// VR-181: the left trigger throws while something is carried. Carrying is the upper-body or
// left-arm FSM sitting in StatePlayerGrabMovable (there is no separate carry-idle state for
// movables, unlike corpses). Pad thread; the anim snapshot is lock-protected and says when it
// is stale, and a stale or disabled watch never swaps.
static std::atomic<bool> g_ctLeft{true};               // [Aim] CarryThrowLeftTrigger
static bool CarryThrowLeftEnabled() { return g_ctLeft.load(); }
static bool CarryThrowTriggersSwapped()
{
    bool carrying = false;
    if (g_ctLeft.load() && !g_gamepadOnly) {
        const auto s = dvr::anim::snapshot();
        if (s.valid)
            for (int i = 0; i < 3; ++i)
                if (!strcmp(s.state[i], "StatePlayerGrabMovable")) { carrying = true; break; }
    }
    static bool was = false;
    if (carrying != was) {
        was = carrying;
        Log("carry/aim: %s - triggers %s", carrying ? "carrying a movable" : "carry ended",
            carrying ? "SWAPPED (left trigger throws, right does the left's job)" : "back to the game's layout");
    }
    return carrying;
}
static void CarryThrowLeftSet(bool on, const char* who)
{
    g_ctLeft.store(on);
    Log("carry/aim: left-trigger throw %s (%s)", on ? "ON" : "off", who);
}

// VR-181 first headset run: the seam drove both throws and the object still went along the
// head. So the direction written here is not what decides the flight, or something steers it
// after. This follow-up MEASURES the flight: the movable ([component+0x58], the object half of
// the m_pMovable interface 0x00A46740 dispatches through) is sampled on the script lane and its
// travel direction logged against the hand and the engine's own direction. It can print the
// unwelcome answer: 'flight follows ENGINE' is what a lost write looks like.
static uint8_t* g_ctObj = nullptr;
static float g_ctFrom[3], g_ctHand[3], g_ctEng[3];
static double g_ctAt = 0; static int g_ctStep = 0;
static const uint32_t kActorLocation = 0xC4, kActorVelocity = 0x1B4;   // as 0x00A46740 / 0x00C455E5 read them
static const uint32_t kActorRotation = 0xD0;   // the actor move compares NewRotation against it (0x0064CE0F)

static float CtDeg(const float* a, const float* b)
{
    const float la = sqrtf(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]), lb = sqrtf(b[0]*b[0]+b[1]*b[1]+b[2]*b[2]);
    if (!(la > 1e-4f) || !(lb > 1e-4f)) return -1.0f;
    float c = (a[0]*b[0]+a[1]*b[1]+a[2]*b[2]) / (la * lb); c = c > 1 ? 1 : c < -1 ? -1 : c;
    return acosf(c) * 57.29578f;
}

static void CarryThrowAimTick()
{
    if (!g_ctObj) return;
    const double now = MaimNowMs();
    static const double kAt[3] = { 60, 200, 450 };
    if (now - g_ctAt < kAt[g_ctStep]) return;
    uint8_t* o = g_ctObj;
    if (!IsLiveObject(o) || !RangeReadable(o + kActorVelocity, 12)) {
        Log("carry/aim: flight check at %.0f ms: the thrown object is gone (freed or unreadable)", now - g_ctAt);
        g_ctObj = nullptr; return;
    }
    const float* L = (const float*)(o + kActorLocation);
    const float* V = (const float*)(o + kActorVelocity);
    const float mv[3] = { L[0] - g_ctFrom[0], L[1] - g_ctFrom[1], L[2] - g_ctFrom[2] };
    const float dh = CtDeg(V, g_ctHand), de = CtDeg(V, g_ctEng), mh = CtDeg(mv, g_ctHand), me = CtDeg(mv, g_ctEng);
    Log("carry/aim: flight check %s at %.0f ms: velocity %.0f uu/s is %.1f deg from the HAND, %.1f "
        "from the ENGINE aim; moved %.0f uu, %.1f deg from hand, %.1f from engine (the two aims were "
        "%.1f apart) -> the path flown follows %s (judged on the distance moved: velocity turns after a hit)", ObjClassName(o), now - g_ctAt,
        sqrtf(V[0]*V[0]+V[1]*V[1]+V[2]*V[2]), dh, de, sqrtf(mv[0]*mv[0]+mv[1]*mv[1]+mv[2]*mv[2]), mh, me,
        CtDeg(g_ctHand, g_ctEng), CtDeg(g_ctHand, g_ctEng) < 10 ? "(aims too close to tell)" : mh < me ? "the HAND" : "the ENGINE");
    if (++g_ctStep >= 3) g_ctObj = nullptr;
}

extern "C" void __cdecl CarryThrowAimHandler(uint8_t* frame, uint8_t* pawn, uint8_t* item)
{
    InterlockedIncrement(&g_ctSeen);
    const char* why = nullptr;
    float* dir = frame ? (float*)(frame - 0x24) : nullptr;
    float len = -1.0f;
    if (!g_ctOn.load()) why = "head aim selected";
    else if (g_gamepadOnly) why = "[Mode] GamepadOnly=1 keeps the head";
    else if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) why = "not in gameplay";
    else if (!pawn || pawn != g_pePawn) why = "not the player's throw";
    else if (!dir || !RangeReadable(dir, 12)) why = "throw frame unreadable";
    else {
        len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        if (!(len > 0.98f && len < 1.02f)) why = "engine direction is not a unit vector (frame mismatch)";
    }
    float o[3], d[3];
    if (!why && !HandRayWorld(o, d, &why)) {}
    if (why) {
        g_ctWhy = why; InterlockedIncrement(&g_ctRefused);
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 20,
            "carry/aim: throw REFUSED: %s (engine dir length %.3f) - the head aims it", why, len);
        return;
    }
    const float r2d = 57.29578f;
    const float wasP = atan2f(dir[2], sqrtf(dir[0] * dir[0] + dir[1] * dir[1])) * r2d;
    const float wasY = atan2f(dir[1], dir[0]) * r2d;
    g_hlObj = nullptr;                                  // released: the move seam lets it go now
    memcpy(g_ctEng, dir, 12); memcpy(g_ctHand, d, 12);
    dir[0] = d[0]; dir[1] = d[1]; dir[2] = d[2];
    InterlockedIncrement(&g_ctDriven);
    {   // arm the flight check (see CarryThrowAimTick)
        uint8_t* comp = (item && RangeReadable(item + 0x114, 4)) ? *(uint8_t**)(item + 0x114) : nullptr;
        uint8_t* obj = (comp && RangeReadable(comp + 0x58, 4)) ? *(uint8_t**)(comp + 0x58) : nullptr;
        if (obj && IsLiveObject(obj) && RangeReadable(obj + kActorLocation, 12)) {
            memcpy(g_ctFrom, obj + kActorLocation, 12); g_ctObj = obj; g_ctAt = MaimNowMs(); g_ctStep = 0;
        } else Log("carry/aim: flight check NOT armed: held item %p component %p object %p", item, comp, obj);
    }
    g_ctWhy = "driving";
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 40,
        "carry/aim: carried object thrown along the HAND - pitch %.1f -> %.1f deg, yaw %.1f -> "
        "%.1f deg (release point, speed and spin stay the game's)",
        wasP, atan2f(d[2], sqrtf(d[0] * d[0] + d[1] * d[1])) * r2d, wasY, atan2f(d[1], d[0]) * r2d);
}

__declspec(naked) static void CarryThrowAimThunk()
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        push edi                    ; the held item ([edi+114h] its DisMovableComponent)
        push esi                    ; the pawn (the release's GetOwner result)
        push ebp                    ; the release's frame: [ebp-24h] the direction
        call CarryThrowAimHandler
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push dword ptr [g_ctPushImm] ; the five displaced bytes (push imm32)
        jmp dword ptr [g_ctBack]
    }
}

static void CarryThrowAimSet(bool on, const char* who)
{
    g_ctOn.store(on);
    if (on && !g_ctDet.on)
        dvr::hooks::detour_install(g_ctDet, "carry/aim", kCarryThrowSeam, kCarryThrowSeamBytes,
                                   sizeof(kCarryThrowSeamBytes), (void*)&CarryThrowAimThunk);
    dvr::aim::request_throw_ray((on || g_thOn.load() || g_gdOn.load()) && !g_gamepadOnly);
    Log("carry/aim: owner %s (%s) - hook %s. Carried objects are thrown along the published hand "
        "ray (the release 0x00C45340); release point, speed and spin stay the game's",
        on ? "HAND" : "HEAD", who, g_ctDet.on ? "ready" : "NOT installed");
}

static void CarryThrowAimConfigure(const char* ini)
{
    CarryThrowAimSet(IniFloat(ini, "Aim", "CarryThrowFromHand", 1) != 0.0f, "ini [Aim] CarryThrowFromHand");
    CarryThrowLeftSet(IniFloat(ini, "Aim", "CarryThrowLeftTrigger", 1) != 0.0f, "ini [Aim] CarryThrowLeftTrigger");
    CarryHoldConfigure(ini);
}

static bool CarryThrowAimCommand(const char* args)
{
    bool b = false;
    if (args && !strcmp(args, "watch")) {
        g_cwWanted = true;
        Log("carry/watch: armed for the next carry (one second of Location writes)");
        return true;
    }
    if (args && !strncmp(args, "hold", 4) && DvrOnOff(args + 4 + strspn(args + 4, " "), &b)) {
        CarryHoldSet(b, "seam");
        ConfigWriteKey("Aim", "CarryHoldAtHand", b ? "1" : "0", "the seam");
        return true;
    }
    if (args && !strncmp(args, "lt", 2) && DvrOnOff(args + 2 + strspn(args + 2, " "), &b)) {
        CarryThrowLeftSet(b, "seam");
        ConfigWriteKey("Aim", "CarryThrowLeftTrigger", b ? "1" : "0", "the seam");
        return true;
    }
    if (DvrOnOff(args, &b)) {
        CarryThrowAimSet(b, "seam");
        ConfigWriteKey("Aim", "CarryThrowFromHand", b ? "1" : "0", "the seam");
        return true;
    }
    Log("carry/aim: on|off, lt on|off, hold on|off, watch (left-trigger throw %s). Aim now %s, %ld/%ld driven/seen, refused %ld (last: %s). Seen moves "
        "only when a carried object is thrown; a plain drop never reaches the seam",
        g_ctLeft.load() ? "ON" : "off", g_ctOn.load() ? "HAND" : "HEAD", (long)g_ctDriven, (long)g_ctSeen, (long)g_ctRefused, g_ctWhy);
    return true;
}

// ---- where the carried object is HELD (VR-181) ---------------------------------------------
// The throw leaves along the hand; until then the game keeps the object in front of the VIEW.
// How, measured over four headset runs (ENGINE_NOTES "The carried-object throw seam"): it is
// not simulated (Physics 0), not attached to the player, and not held through the pawn's
// RB_Handle (zero setter calls). A hardware write-watch on its Location caught ONE writer, every
// frame: the engine's actor move, which adds the move delta at [ebp-0x54] to Location at
// 0x0064D584 with esi = the actor. That instruction is the seam: when esi is the carried object
// the delta becomes (hand target - Location), so the object lands on the hand and the engine
// updates its components from the new Location as it always does. Every actor move in the game
// passes here, so the first test is one pointer compare. Gates: the object the carry state names,
// still PHYS_None (a thrown or dropped object is simulated and is never pulled back), and the
// carry still on. [Aim] CarryHoldAtHand (default 1), CarryHoldForwardCm (15) along the ray.
static dvr::hooks::Detour g_hlDet;
static uintptr_t g_hlBack = kMoveDeltaBack;
static std::atomic<bool> g_hlOn{true};                   // [Aim] CarryHoldAtHand
static float g_hlFwdCm = 0.0f;                           // [Aim] CarryHoldForwardCm (negative pulls it in)
static bool g_hlRotOn = true;                            // [Aim] CarryHoldRotate
static volatile uint32_t g_hlPhysOff = 0;                // Actor::Physics, resolved by name
static volatile LONG g_hlSeen = 0, g_hlDriven = 0;
static const char* volatile g_hlWhy = "not asked yet";
static float g_hlMoved = -1;                             // the last correction, uu

static bool CarryHoldEnabled() { return g_hlOn.load(); }

static bool CarryingMovable()
{
    const auto s = dvr::anim::snapshot();
    if (!s.valid) return false;
    for (int i = 0; i < 3; ++i)
        if (!strcmp(s.state[i], "StatePlayerGrabMovable")) return true;
    return false;
}

// The hand's full frame in game world terms: origin, forward (the published ray) and up (the
// controller's own up, carried into the game the same way fireaim::solve carries the ray: XR
// vector -> the head's basis -> the view's basis). Anchored on the CENTRE eye: the last render
// sample is one eye or the other under re-entry, and the held object flickered sideways by half
// an IPD with it (headset, 2026-09-22).
static bool CarryHandFrame(float* o, float* F, float* U, const char** why)
{
    const auto aim = dvr::aim::fire_frame();
    float cam[3];
    if (!dvr::camera::render_pos_world_center(cam)) { *why = "no world camera position"; return false; }
    dvr::fireaim::Solution sol;
    if (!dvr::fireaim::solve(aim, GetTickCount64(), g_viewYawRad, g_viewPitchRad, cam, g_posScaleUU, cam, sol)) {
        *why = aim.ray.ok ? (aim.headValid ? "ray geometry refused" : "no head pose with the ray") : aim.ray.why;
        return false;
    }
    float d[3] = { sol.target[0] - sol.origin[0], sol.target[1] - sol.origin[1], sol.target[2] - sol.origin[2] };
    if (!dvr::fireaim::normalize(d)) { *why = "degenerate ray"; return false; }
    // the head basis, as solve builds it
    float q[4], n = 0;
    for (int i = 0; i < 4; ++i) { q[i] = aim.headQuat[i]; n += q[i] * q[i]; }
    for (float& x : q) x /= sqrtf(n);
    const float fwd[3] = {0, 0, -1}, wup[3] = {0, 1, 0};
    float hf[3], hr[3], hu[3];
    dvr::xrmath::quat_rotate(q[0], q[1], q[2], q[3], fwd, hf);
    dvr::fireaim::cross(hf, wup, hr);
    if (!dvr::fireaim::normalize(hr)) { *why = "head basis degenerate"; return false; }
    dvr::fireaim::cross(hr, hf, hu);
    const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
    const float Fv[3] = { cp * cy, cp * sy, sp }, Rv[3] = { -sy, cy, 0 }, Uv[3] = { -sp * cy, -sp * sy, cp };
    const float* ux = aim.ray.upXr;
    const float rel[3] = { dvr::fireaim::dot(ux, hr), dvr::fireaim::dot(ux, hu), dvr::fireaim::dot(ux, hf) };
    float u[3];
    for (int i = 0; i < 3; ++i) u[i] = Rv[i] * rel[0] + Uv[i] * rel[1] + Fv[i] * rel[2];
    const float k = dvr::fireaim::dot(u, d);          // square it against the ray (it may be the trimmed axis)
    for (int i = 0; i < 3; ++i) u[i] -= d[i] * k;
    if (!dvr::fireaim::normalize(u)) { *why = "hand up is along the ray"; return false; }
    memcpy(o, sol.origin, 12); memcpy(F, d, 12); memcpy(U, u, 12);
    return true;
}

// UE3 FRotator (Pitch, Yaw, Roll; 65536 = one turn) <-> the rotation matrix's axes X/Y/Z.
static void CtRotToAxes(const int32_t* r, float X[3], float Y[3], float Z[3])
{
    const float k = 6.28318531f / 65536.0f;
    const float P = r[0] * k, Yw = r[1] * k, R = r[2] * k;
    const float SP = sinf(P), CP = cosf(P), SY = sinf(Yw), CY = cosf(Yw), SR = sinf(R), CR = cosf(R);
    X[0] = CP * CY; X[1] = CP * SY; X[2] = SP;
    Y[0] = SR * SP * CY - CR * SY; Y[1] = SR * SP * SY + CR * CY; Y[2] = -SR * CP;
    Z[0] = -(CR * SP * CY + SR * SY); Z[1] = CY * SR - CR * SP * SY; Z[2] = CR * CP;
}
static void CtAxesToRot(const float X[3], const float Y[3], const float Z[3], int32_t* r)
{
    const float k = 65536.0f / 6.28318531f;
    r[0] = (int32_t)(atan2f(X[2], sqrtf(X[0] * X[0] + X[1] * X[1])) * k);
    r[1] = (int32_t)(atan2f(X[1], X[0]) * k);
    r[2] = (int32_t)(atan2f(-Y[2], Z[2]) * k);
}

// The object's rotation in the hand's frame, taken at the first drive of each carry, so it keeps
// the orientation it was picked up in and then turns with the wrist.
static bool g_hlRelOk = false;
static float g_hlRel[3][3];                              // rows: object X/Y/Z in hand (F, R, U) terms
static volatile LONG g_hlRot = 0;

extern "C" void __cdecl CarryMoveHandler(uint8_t* frame, uint8_t* actor)
{
    if (!actor || actor != g_hlObj) return;              // every actor move in the game comes here
    InterlockedIncrement(&g_hlSeen);
    const char* why = nullptr;
    const uint32_t po = g_hlPhysOff;
    float o[3], F[3], U[3];
    if (!g_hlOn.load()) why = "game hold selected";
    else if (g_gamepadOnly) why = "[Mode] GamepadOnly=1 keeps the head";
    else if (!po || !RangeReadable(actor + po, 1) || actor[po] != 0) why = "object is simulated (thrown or dropped)";
    else if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) why = "not in gameplay";
    else if (!frame || !RangeReadable(frame - 0x54, 12) || !RangeReadable(actor + kActorLocation, 12) ||
             !RangeReadable(actor + kActorRotation, 12)) why = "move frame unreadable";
    else CarryHandFrame(o, F, U, &why);
    if (why) { g_hlWhy = why; return; }
    float Rh[3]; dvr::fireaim::cross(U, F, Rh);           // UE3: Y = Z x X
    // position
    float* delta = (float*)(frame - 0x54);
    const float* L = (const float*)(actor + kActorLocation);
    const float f = g_hlFwdCm * g_posScaleUU / 100.0f;   // cm -> uu (g_posScaleUU is uu per metre)
    const float n[3] = { o[0] + F[0] * f, o[1] + F[1] * f, o[2] + F[2] * f };
    const float e[3] = { L[0] + delta[0], L[1] + delta[1], L[2] + delta[2] };   // where the game put it
    delta[0] = n[0] - L[0]; delta[1] = n[1] - L[1]; delta[2] = n[2] - L[2];
    g_hlMoved = sqrtf((n[0]-e[0])*(n[0]-e[0]) + (n[1]-e[1])*(n[1]-e[1]) + (n[2]-e[2])*(n[2]-e[2]));
    // rotation: latch the object's frame relative to the hand once, then carry it with the hand
    int32_t* rot = (int32_t*)(actor + kActorRotation);
    if (g_hlRotOn) {
        if (!g_hlRelOk) {
            float X[3], Y[3], Z[3]; CtRotToAxes(rot, X, Y, Z);
            const float* ax[3] = { X, Y, Z };
            for (int i = 0; i < 3; ++i) {
                g_hlRel[i][0] = dvr::fireaim::dot(ax[i], F);
                g_hlRel[i][1] = dvr::fireaim::dot(ax[i], Rh);
                g_hlRel[i][2] = dvr::fireaim::dot(ax[i], U);
            }
            g_hlRelOk = true;
        }
        float A[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) A[i][j] = g_hlRel[i][0] * F[j] + g_hlRel[i][1] * Rh[j] + g_hlRel[i][2] * U[j];
        int32_t nr[3]; CtAxesToRot(A[0], A[1], A[2], nr);
        rot[0] = nr[0]; rot[1] = nr[1]; rot[2] = nr[2];
        // the move's own NewRotation (by pointer at [ebp-3Ch]) must agree, or it puts the old one back
        int32_t** pNew = (int32_t**)(frame - 0x3C);
        if (RangeReadable(pNew, 4) && *pNew && RangeReadable(*pNew, 12)) { (*pNew)[0] = nr[0]; (*pNew)[1] = nr[1]; (*pNew)[2] = nr[2]; }
        InterlockedIncrement(&g_hlRot);
    }
    InterlockedIncrement(&g_hlDriven);
    g_hlWhy = "driving";
}
__declspec(naked) static void CarryMoveThunk()
{
    __asm {
        pushfd
        pushad
        mov edx, esp
        sub esp, 528
        and esp, -16
        fxsave [esp]
        fninit
        cld
        push edx
        push esi                    ; the actor being moved
        push ebp                    ; the move's frame: [ebp-54h] the delta
        call CarryMoveHandler
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        movss xmm0, dword ptr [ebp-54h] ; the five displaced bytes (re-read after the handler)
        jmp dword ptr [g_hlBack]
    }
}
// ---- who writes the carried object's Location (VR-181; on demand: `carryaim watch`) --------
// Build 658 measured the carried DishonoredMovable: Physics=0 (not simulated), Base = the static
// mesh it sat on (not attached to the player), and its Location FIXED in the view's frame (about
// 100 uu ahead, 45 below) while its distance from the hand swung 38..112 uu. Something writes
// Location every tick from the camera. This finds WHAT: a hardware write-watch (DR3, 4 bytes,
// Location.X) for about a second of the carry, reporting each writing instruction with the
// return addresses above it. Armed from a HELPER thread that suspends each other thread first:
// the retired watch (legacy/aim_watch.cpp) set its own thread's context with
// GetCurrentThread(), which Windows does not honour reliably (ENGINE_NOTES, the razor seam).
struct CwRec { uint32_t eip, n, ret[4]; };
static CwRec g_cwRecs[8];
static volatile LONG g_cwRecN = 0, g_cwHits = 0, g_cwThreads = 0;
static volatile uintptr_t g_cwAddr = 0;
static uintptr_t g_cwSelfLo = 0, g_cwSelfHi = 0;
static PVOID g_cwVeh = nullptr;
static double g_cwArmedAt = 0;

static LONG CALLBACK CarryWatchVeh(PEXCEPTION_POINTERS ep)
{
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    if (!(c->Dr6 & 0x8)) return EXCEPTION_CONTINUE_SEARCH;       // not our DR3
    c->Dr6 &= ~0xFu;
    InterlockedIncrement(&g_cwHits);
    const uint32_t eip = (uint32_t)c->Eip;
    if (eip >= g_cwSelfLo && eip < g_cwSelfHi) return EXCEPTION_CONTINUE_EXECUTION;   // our own
    LONG n = g_cwRecN; if (n > 8) n = 8;
    for (LONG i = 0; i < n; i++)
        if (g_cwRecs[i].eip == eip) { InterlockedIncrement((volatile LONG*)&g_cwRecs[i].n); return EXCEPTION_CONTINUE_EXECUTION; }
    const LONG idx = InterlockedIncrement(&g_cwRecN) - 1;
    if (idx < 8) {
        CwRec* r = &g_cwRecs[idx];
        r->eip = eip; r->n = 1; memset(r->ret, 0, sizeof(r->ret));
        const uint32_t* sp = (const uint32_t*)c->Esp; int got = 0;
        for (int k = 0; k < 48 && got < 4; k++) {
            if (!RangeReadable(sp + k, 4)) break;
            const uint32_t v = sp[k];
            if (v >= 0x401000 && v < 0xF40000) r->ret[got++] = v;   // inside the exe's code
        }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}

static DWORD WINAPI CarryWatchApplyThread(LPVOID arg)
{
    const uintptr_t addr = (uintptr_t)arg;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    THREADENTRY32 te; te.dwSize = sizeof(te);
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    LONG done = 0;
    if (Thread32First(snap, &te)) do {
        if (te.th32OwnerProcessID != pid || te.th32ThreadID == self) continue;
        HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (!th) continue;
        if (SuspendThread(th) != (DWORD)-1) {
            CONTEXT c; memset(&c, 0, sizeof(c));
            c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (GetThreadContext(th, &c)) {
                if (addr) { c.Dr3 = (DWORD)addr; c.Dr7 = (c.Dr7 & ~((0x3u << 6) | (0xFu << 28))) | (0x1u << 6) | (0x1u << 28) | (0x3u << 30); }
                else      { c.Dr3 = 0;           c.Dr7 &= ~((0x3u << 6) | (0xFu << 28)); }
                c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (SetThreadContext(th, &c)) ++done;
            }
            ResumeThread(th);
        }
        CloseHandle(th);
    } while (Thread32Next(snap, &te));
    CloseHandle(snap);
    g_cwThreads = done;
    return 0;
}

static void CarryWatchApply(uintptr_t addr)
{
    HANDLE h = CreateThread(nullptr, 0, CarryWatchApplyThread, (LPVOID)addr, 0, nullptr);
    if (h) { WaitForSingleObject(h, 500); CloseHandle(h); }
}

static void CarryWatchArm(uint8_t* obj, uint32_t locOff)
{
    if (!g_cwSelfLo) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void*)&CarryWatchArm, &mbi, sizeof(mbi))) {
            g_cwSelfLo = (uintptr_t)mbi.AllocationBase; g_cwSelfHi = g_cwSelfLo + 0x800000;
        }
    }
    if (!g_cwVeh) g_cwVeh = AddVectoredExceptionHandler(1, CarryWatchVeh);
    g_cwRecN = 0; g_cwHits = 0;
    g_cwAddr = (uintptr_t)obj + locOff;
    CarryWatchApply(g_cwAddr);
    Log("carry/watch: ARMED on %s %p Location.X (+0x%X) in %ld threads (from a helper thread)",
        ObjClassName(obj), obj, locOff, (long)g_cwThreads);
}

static void CarryWatchReport(const char* why)
{
    if (!g_cwAddr) return;
    CarryWatchApply(0);
    g_cwAddr = 0;
    LONG n = g_cwRecN; if (n > 8) n = 8;
    Log("carry/watch: report (%s) - %ld writes seen, %ld distinct writer(s) outside the mod, cleared in %ld threads:",
        why, (long)g_cwHits, (long)n, (long)g_cwThreads);
    for (LONG i = 0; i < n; i++)
        Log("carry/watch:   eip=0x%08X hits=%u  callers 0x%08X 0x%08X 0x%08X 0x%08X", g_cwRecs[i].eip,
            g_cwRecs[i].n, g_cwRecs[i].ret[0], g_cwRecs[i].ret[1], g_cwRecs[i].ret[2], g_cwRecs[i].ret[3]);
    if (!n) Log("carry/watch:   no writer caught. With hits=0 too the watch never fired: it was not honoured, "
                "or Location is not written (a write of the whole vector through a wider store still fires)");
}
static uint8_t* CarryProbeFocus()
{
    static uint32_t focusOff = 0;
    if (!focusOff) focusOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
    uint8_t* pc = g_peCtrl;
    if (!focusOff || !pc || !LooksLikeObj(pc) || !RangeReadable(pc + focusOff, 4)) return nullptr;
    uint8_t* f = *(uint8_t**)(pc + focusOff);
    return (f && IsLiveObject(f)) ? f : nullptr;
}
// Script lane: follow the carry, publish the carried object to the move seam, and report once a
// second while carrying. The object is the one StatePlayerGrabMovable names (+0x70, its
// DisMovableComponent, whose +0x58 is the actor); the focused actor is only a fallback.
static void CarryHoldTick()
{
    const double now = MaimNowMs();
    if (!g_hlPhysOff) {
        static double nextTry = 0;
        if (now >= nextTry) { nextTry = now + 5000; g_hlPhysOff = RflOffsetOf("Actor", "Physics"); }
    }
    static uint8_t* lastFocus = nullptr; static double lastFocusAt = 0;
    const bool carry = CarryingMovable();
    static bool was = false;
    static LONG s0 = 0, d0 = 0;
    if (!carry) { uint8_t* fo = CarryProbeFocus(); if (fo) { lastFocus = fo; lastFocusAt = now; } }
    if (carry && !was) {
        uint8_t* st = nullptr; uint8_t* comp = nullptr; uint8_t* obj = nullptr;
        const auto s = dvr::anim::snapshot();
        for (int i = 0; i < 3; ++i)
            if (!strcmp(s.state[i], "StatePlayerGrabMovable")) { st = (uint8_t*)(uintptr_t)s.stateAddress[i]; break; }
        if (st && RangeReadable(st + 0x70, 4)) comp = *(uint8_t**)(st + 0x70);
        if (comp && RangeReadable(comp + 0x58, 4)) obj = *(uint8_t**)(comp + 0x58);
        const char* src = "the carry state";
        if (!(obj && IsLiveObject(obj))) {
            obj = (lastFocus && now - lastFocusAt < 3000 && IsLiveObject(lastFocus)) ? lastFocus : nullptr;
            src = obj ? "the focused actor (the state named none)" : "NOTHING";
        }
        g_hlObj = obj; s0 = g_hlSeen; d0 = g_hlDriven; g_hlWhy = "not moved yet"; g_hlRelOk = false;
        Log("carry/hold: carry began - object %s %p from %s; hold owner %s", obj ? ObjClassName(obj) : "-",
            obj, src, g_hlOn.load() ? "HAND" : "GAME");
        if (obj && g_cwWanted) { g_cwWanted = false; CarryWatchArm(obj, kActorLocation); g_cwArmedAt = now; }
    }
    if (g_cwAddr && (now - g_cwArmedAt > 1000 || !carry)) CarryWatchReport(carry ? "one second of carry" : "carry ended");
    if (!carry && was) {
        g_hlObj = nullptr;
        Log("carry/hold: carry ended - %ld moves of the object seen, %ld moved to the hand (last: %s)",
            (long)(g_hlSeen - s0), (long)(g_hlDriven - d0), g_hlWhy);
    }
    static double nextLog = 0;
    if (carry && now >= nextLog) {
        nextLog = now + 1000;
        uint8_t* obj = g_hlObj;
        float cam[3] = {0, 0, 0}, fwd = 0, up = 0, toHand = -1;
        float o[3], d[3]; const char* why = nullptr;
        if (obj && IsLiveObject(obj) && dvr::camera::render_pos_world(cam)) {
            const float* L = (const float*)(obj + kActorLocation);
            const float r[3] = { L[0] - cam[0], L[1] - cam[1], L[2] - cam[2] };
            const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
            fwd = r[0] * cp * cy + r[1] * cp * sy + r[2] * sp;
            up  = -r[0] * sp * cy - r[1] * sp * sy + r[2] * cp;
            if (HandRayWorld(o, d, &why)) { const float h[3] = { L[0] - o[0], L[1] - o[1], L[2] - o[2] }; toHand = sqrtf(h[0]*h[0]+h[1]*h[1]+h[2]*h[2]); }
        }
        Log("carry/hold: carrying - %ld moves of the object seen, %ld moved to the hand, %ld turned with it (last: %s, %.0f uu "
            "from where the game put it). Object now %.0f uu from the hand ray origin, %.0f fwd %.0f up in "
            "the view. Seen 0 while carrying = the object is not moved through the seam",
            (long)(g_hlSeen - s0), (long)(g_hlDriven - d0), (long)g_hlRot, g_hlWhy, g_hlMoved, toHand, fwd, up);
    }
    was = carry;
}

static void CarryHoldSet(bool on, const char* who)
{
    g_hlOn.store(on);
    if (!g_hlDet.on)
        dvr::hooks::detour_install(g_hlDet, "carry/hold", kMoveDeltaSeam, kMoveDeltaSeamBytes,
                                   sizeof(kMoveDeltaSeamBytes), (void*)&CarryMoveThunk);
    Log("carry/hold: owner %s (%s), %.0f cm along the ray - seam %s (it stays in either way; it "
        "only ever touches the carried object)", on ? "HAND" : "GAME", who, g_hlFwdCm,
        g_hlDet.on ? "ready" : "NOT installed");
}
static void CarryHoldConfigure(const char* ini)
{
    g_hlFwdCm = IniFloat(ini, "Aim", "CarryHoldForwardCm", 0);
    if (!(g_hlFwdCm >= -40 && g_hlFwdCm <= 60)) g_hlFwdCm = 0;
    g_hlRotOn = IniFloat(ini, "Aim", "CarryHoldRotate", 1) != 0.0f;
    CarryHoldSet(IniFloat(ini, "Aim", "CarryHoldAtHand", 1) != 0.0f, "ini [Aim] CarryHoldAtHand");
}
static float CarryHoldForwardCm() { return g_hlFwdCm; }
static void CarryHoldSetForwardCm(float cm) { if (cm >= -40 && cm <= 60) g_hlFwdCm = cm; }
static bool CarryHoldRotateEnabled() { return g_hlRotOn; }
static void CarryHoldSetRotate(bool on) { g_hlRotOn = on; g_hlRelOk = false; Log("carry/hold: rotation with the hand %s", on ? "ON" : "off"); }