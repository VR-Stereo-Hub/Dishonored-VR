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
// [Aim] CarryHoldForwardCm/RightCm/UpCm and CarryHoldPitch/Yaw/Roll (degrees), in the HAND's frame.
static float g_hlAdj[6] = {0, 0, 0, 0, 0, 0};
static const char* const kHlAdjKey[6] = { "CarryHoldForwardCm", "CarryHoldRightCm", "CarryHoldUpCm",
                                          "CarryHoldPitch", "CarryHoldYaw", "CarryHoldRoll" };
static const float kHlAdjMin[6] = { -40, -40, -40, -180, -180, -180 }, kHlAdjMax[6] = { 60, 40, 40, 180, 180, 180 };
static std::atomic<bool> g_hlWorldDepth{true};           // [Aim] CarryHoldWorldDepth (see CarryHoldTick)
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
// The anchor. Build 662: the object was never moved by anything but this seam (0 outside moves),
// yet it jumped left and right for single frames, hard facing one way and hardly at all facing the
// opposite way, wherever the player stood. A WORLD-direction error. The render sample (c5) is
// uploaded by every scene draw, including passes that are not an eye (shadow depth, captures),
// whose camera is somewhere else; whichever drew last before the tick became the anchor. The GAME
// camera (its base, with the mod's own offsets removed, plus the mod's positional offset in the
// yaw-only frame, which is how the camera lane writes it) is the head this tick whatever drew.
// [Aim] CarryHoldAnchor=1 uses it (default); 0 is the render centre eye, for A/B. Both are
// computed every drive and their disagreement logged, so the next run measures the cause.
static std::atomic<bool> g_hlGameAnchor{true};
static float g_hlAnchorGap = 0, g_hlAnchorGapView[3] = {0, 0, 0}; static volatile LONG g_hlAnchorBig = 0, g_hlAnchorN = 0;
static bool CarryGameAnchor(float out[3])
{
    uint8_t* cam = g_camObj;
    float base[3], off[3];
    if (!cam || !dvr::camera::game_base_pos(cam, base)) return false;
    dvr::camera::position_offset_uu(off);                        // (right, up, forward) uu
    const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad);
    out[0] = base[0] - sy * off[0] + cy * off[2];
    out[1] = base[1] + cy * off[0] + sy * off[2];
    out[2] = base[2] + off[1];
    return true;
}

static bool CarryHandFrame(float* o, float* F, float* U, const char** why)
{
    const auto aim = dvr::aim::fire_frame();
    float cam[3], rc[3], gc[3];
    const bool haveR = dvr::camera::render_pos_world_center(rc), haveG = CarryGameAnchor(gc);
    if (haveR && haveG) {   // the disagreement, every drive
        const float d[3] = { rc[0] - gc[0], rc[1] - gc[1], rc[2] - gc[2] };
        const float g = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        InterlockedIncrement(&g_hlAnchorN);
        if (g > 1.5f) InterlockedIncrement(&g_hlAnchorBig);
        if (g > g_hlAnchorGap) {
            g_hlAnchorGap = g;
            const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
            g_hlAnchorGapView[0] = d[0] * cp * cy + d[1] * cp * sy + d[2] * sp;
            g_hlAnchorGapView[1] = -d[0] * sy + d[1] * cy;
            g_hlAnchorGapView[2] = -d[0] * sp * cy - d[1] * sp * sy + d[2] * cp;
        }
    }
    const bool useG = g_hlGameAnchor.load() ? haveG : (!haveR && haveG);   // the chosen one, else the other
    if (useG) memcpy(cam, gc, 12);
    else if (haveR) memcpy(cam, rc, 12);
    else { *why = "no world camera position"; return false; }
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
static std::atomic<bool> g_hlKeepAngle{false};          // [Aim] CarryHoldKeepPickupAngle
// Between two of our drives nothing should move the object. If it is not where we put it, some
// other path moved it (the candidate for a one-frame jump): count it, and keep how far and which
// way in the VIEW's frame. Also the frame gap, since a move per frame is what the hold does.
static float g_hlLast[3]; static bool g_hlLastOk = false;
static volatile LONG g_hlOutside = 0; static float g_hlOutMax = 0, g_hlOutView[3] = {0, 0, 0};
static uint32_t g_hlLastSerial = 0; static volatile LONG g_hlSameFrame = 0, g_hlSkipFrame = 0;
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
    {   // did anything move it since our last drive?
        const float* L0 = (const float*)(actor + kActorLocation);
        if (g_hlLastOk) {
            const float dv[3] = { L0[0] - g_hlLast[0], L0[1] - g_hlLast[1], L0[2] - g_hlLast[2] };
            const float dd = sqrtf(dv[0] * dv[0] + dv[1] * dv[1] + dv[2] * dv[2]);
            if (dd > 0.5f) {
                InterlockedIncrement(&g_hlOutside);
                if (dd > g_hlOutMax) {
                    g_hlOutMax = dd;
                    const float cy = cosf(g_viewYawRad), sy = sinf(g_viewYawRad), cp = cosf(g_viewPitchRad), sp = sinf(g_viewPitchRad);
                    g_hlOutView[0] = dv[0] * cp * cy + dv[1] * cp * sy + dv[2] * sp;
                    g_hlOutView[1] = -dv[0] * sy + dv[1] * cy;
                    g_hlOutView[2] = -dv[0] * sp * cy - dv[1] * sp * sy + dv[2] * cp;
                }
            }
        }
        const uint32_t ser = dvr::camera::render_pos_serial();
        if (g_hlLastOk && ser == g_hlLastSerial) InterlockedIncrement(&g_hlSameFrame);
        g_hlLastSerial = ser;
    }
    // position
    float* delta = (float*)(frame - 0x54);
    const float* L = (const float*)(actor + kActorLocation);
    const float cm = g_posScaleUU / 100.0f;              // cm -> uu (g_posScaleUU is uu per metre)
    const float af = g_hlAdj[0] * cm, ar = g_hlAdj[1] * cm, au = g_hlAdj[2] * cm;
    const float n[3] = { o[0] + F[0] * af + Rh[0] * ar + U[0] * au, o[1] + F[1] * af + Rh[1] * ar + U[1] * au,
                         o[2] + F[2] * af + Rh[2] * ar + U[2] * au };
    const float e[3] = { L[0] + delta[0], L[1] + delta[1], L[2] + delta[2] };   // where the game put it
    delta[0] = n[0] - L[0]; delta[1] = n[1] - L[1]; delta[2] = n[2] - L[2];
    memcpy(g_hlLast, n, 12); g_hlLastOk = true;
    g_hlMoved = sqrtf((n[0]-e[0])*(n[0]-e[0]) + (n[1]-e[1])*(n[1]-e[1]) + (n[2]-e[2])*(n[2]-e[2]));
    // rotation: latch the object's frame relative to the hand once, then carry it with the hand
    int32_t* rot = (int32_t*)(actor + kActorRotation);
    if (g_hlRotOn) {
        if (!g_hlRelOk && !g_hlKeepAngle.load()) {         // the object's X/Y/Z = the hand's F/R/U
            for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) g_hlRel[i][j] = (i == j) ? 1.0f : 0.0f;
            g_hlRelOk = true;
        }
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
        // the trim: a rotation in the hand's own frame (F, R, U as X, Y, Z), applied to the latched
        // relative frame, so pitch/yaw/roll turn the object about the hand, not the world
        const int32_t trim[3] = { (int32_t)(g_hlAdj[3] * 65536.0f / 360.0f), (int32_t)(g_hlAdj[4] * 65536.0f / 360.0f),
                                  (int32_t)(g_hlAdj[5] * 65536.0f / 360.0f) };
        float TX[3], TY[3], TZ[3]; CtRotToAxes(trim, TX, TY, TZ);
        float A[3][3];
        for (int i = 0; i < 3; ++i) {
            float l[3];
            for (int j = 0; j < 3; ++j) l[j] = TX[j] * g_hlRel[i][0] + TY[j] * g_hlRel[i][1] + TZ[j] * g_hlRel[i][2];
            for (int j = 0; j < 3; ++j) A[i][j] = l[0] * F[j] + l[1] * Rh[j] + l[2] * U[j];
        }
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
// VR-181 seventh headset run: held at the hand, the object still flickered to the LEFT whatever
// its orientation, while every move was driven and the object sat 3 uu from the hand at every
// tick. So the fault is in how it is DRAWN, not where it is. While carried, the game moves it to
// SDPG_Foreground (DishonoredItemEmpty.m_OldMovableDepthGroup keeps the old group): the depth
// group of the arms and weapons, which is drawn for a view at the head and gets the mod's hand
// and weapon treatment, not the world's per-eye view. An object really at the hand belongs in the
// WORLD group. [Aim] CarryHoldWorldDepth=1 puts it back through the engine's own
// PrimitiveComponent.SetDepthPriorityGroup (so the render proxy is re-created, which writing the
// byte would not do); the game restores its saved group itself on release. The counterprediction:
// if it still flickers with the object in SDPG_World, the depth group was not the cause.
static void CarryDepthToWorld(uint8_t* obj, bool announce)
{
    static uint8_t* fn = nullptr; static bool looked = false;
    static uint32_t oColl = 0, oHi = 0, oDpg = 0;
    if (!looked) {
        looked = true;
        fn = RainFindClassFunction("PrimitiveComponent", "SetDepthPriorityGroup");
        oColl = RflOffsetOf("Actor", "CollisionComponent");
        oHi = RflOffsetOf("DishonoredMovable", "m_pHighlightStaticMeshComponent");
        oDpg = RflOffsetOf("PrimitiveComponent", "DepthPriorityGroup");
        Log("carry/depth: SetDepthPriorityGroup %s, Actor.CollisionComponent +0x%X, highlight +0x%X, "
            "PrimitiveComponent.DepthPriorityGroup +0x%X", fn ? "found" : "MISSING", oColl, oHi, oDpg);
    }
    if (!obj || !IsLiveObject(obj) || !oDpg) return;
    const uint32_t offs[2] = { oColl, oHi };
    const char* names[2] = { "mesh", "highlight" };
    for (int i = 0; i < 2; ++i) {
        uint8_t* c = (offs[i] && RangeReadable(obj + offs[i], 4)) ? *(uint8_t**)(obj + offs[i]) : nullptr;
        if (!c || !IsLiveObject(c) || !RangeReadable(c + oDpg, 1)) continue;
        const int was = c[oDpg];
        bool set = false;
        if (was == 2 && fn && g_hlWorldDepth.load() && g_hlOn.load()) {   // SDPG_Foreground -> SDPG_World
            struct { uint8_t group; } p = { 1 };
            g_peReentry = true;
            ((PFN_ProcessEventCall)kProcessEvent)(c, fn, &p, NULL);
            g_peReentry = false;
            set = true;
        }
        if (announce || set)
            Log("carry/depth: %s %s group %d%s (0 editor bg, 1 world, 2 foreground)", names[i], ObjClassName(c),
                was, set ? (c[oDpg] == 1 ? " -> 1, WORLD" : " -> set asked, group did NOT change") : "");
    }
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
        g_hlLastOk = false; g_hlOutside = 0; g_hlOutMax = 0; g_hlSameFrame = 0;
        Log("carry/hold: carry began - object %s %p from %s; hold owner %s", obj ? ObjClassName(obj) : "-",
            obj, src, g_hlOn.load() ? "HAND" : "GAME");
        CarryDepthToWorld(obj, true);
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
        CarryDepthToWorld(obj, false);          // the game may set it again
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
            "the view. Seen 0 while carrying = the object is not moved through the seam. MOVED OUTSIDE the seam %ld times, "
            "largest %.1f uu (%.1f fwd %.1f right %.1f up in the view); two drives in one frame %ld",
            (long)(g_hlSeen - s0), (long)(g_hlDriven - d0), (long)g_hlRot, g_hlWhy, g_hlMoved, toHand, fwd, up,
            (long)g_hlOutside, g_hlOutMax, g_hlOutView[0], g_hlOutView[1], g_hlOutView[2], (long)g_hlSameFrame);
        g_hlOutMax = 0;
        Log("carry/anchor: %s anchor. Render centre eye vs game camera: %ld of %ld drives more than 1.5 uu apart, "
            "largest %.1f uu (%.1f fwd %.1f right %.1f up in the view) in the last second. Big and frequent here "
            "= the render sample was the jump", g_hlGameAnchor.load() ? "GAME-camera" : "RENDER-sample",
            (long)g_hlAnchorBig, (long)g_hlAnchorN, g_hlAnchorGap, g_hlAnchorGapView[0], g_hlAnchorGapView[1], g_hlAnchorGapView[2]);
        g_hlAnchorGap = 0; g_hlAnchorBig = 0; g_hlAnchorN = 0;
    }
    was = carry;
}

static void CarryHoldSet(bool on, const char* who)
{
    g_hlOn.store(on);
    if (!g_hlDet.on)
        dvr::hooks::detour_install(g_hlDet, "carry/hold", kMoveDeltaSeam, kMoveDeltaSeamBytes,
                                   sizeof(kMoveDeltaSeamBytes), (void*)&CarryMoveThunk);
    Log("carry/hold: owner %s (%s), offset %.0f/%.0f/%.0f cm fwd/right/up, trim %.0f/%.0f/%.0f deg p/y/r - seam %s (it stays in either way; it "
        "only ever touches the carried object)", on ? "HAND" : "GAME", who, g_hlAdj[0], g_hlAdj[1], g_hlAdj[2], g_hlAdj[3], g_hlAdj[4], g_hlAdj[5],
        g_hlDet.on ? "ready" : "NOT installed");
}
static void CarryHoldConfigure(const char* ini)
{
    for (int i = 0; i < 6; ++i) {
        const float v = IniFloat(ini, "Aim", kHlAdjKey[i], 0);
        g_hlAdj[i] = (v >= kHlAdjMin[i] && v <= kHlAdjMax[i]) ? v : 0;
    }
    g_hlWorldDepth.store(IniFloat(ini, "Aim", "CarryHoldWorldDepth", 1) != 0.0f);
    g_hlKeepAngle.store(IniFloat(ini, "Aim", "CarryHoldKeepPickupAngle", 0) != 0.0f);
    g_hlGameAnchor.store(IniFloat(ini, "Aim", "CarryHoldAnchor", 1) != 0.0f);
    g_hlRotOn = IniFloat(ini, "Aim", "CarryHoldRotate", 1) != 0.0f;
    CarryHoldSet(IniFloat(ini, "Aim", "CarryHoldAtHand", 1) != 0.0f, "ini [Aim] CarryHoldAtHand");
}
static float CarryHoldAdj(int i) { return (i >= 0 && i < 6) ? g_hlAdj[i] : 0; }
static const char* CarryHoldAdjKey(int i) { return (i >= 0 && i < 6) ? kHlAdjKey[i] : ""; }
static void CarryHoldSetAdj(int i, float v) { if (i >= 0 && i < 6 && v >= kHlAdjMin[i] && v <= kHlAdjMax[i]) g_hlAdj[i] = v; }
static bool CarryHoldGameAnchor() { return g_hlGameAnchor.load(); }
static void CarryHoldSetGameAnchor(bool on) { g_hlGameAnchor.store(on); Log("carry/anchor: %s", on ? "GAME camera" : "RENDER centre eye"); }
static bool CarryHoldKeepAngle() { return g_hlKeepAngle.load(); }
static void CarryHoldSetKeepAngle(bool on) { g_hlKeepAngle.store(on); g_hlRelOk = false; Log("carry/hold: keep the pickup angle %s", on ? "ON" : "off (fixed in the hand)"); }
static bool CarryHoldWorldDepthEnabled() { return g_hlWorldDepth.load(); }
static void CarryHoldSetWorldDepth(bool on) { g_hlWorldDepth.store(on); Log("carry/hold: world depth while held %s (takes effect on the next carry)", on ? "ON" : "off"); }

static bool CarryHoldRotateEnabled() { return g_hlRotOn; }
static void CarryHoldSetRotate(bool on) { g_hlRotOn = on; g_hlRelOk = false; Log("carry/hold: rotation with the hand %s", on ? "ON" : "off"); }