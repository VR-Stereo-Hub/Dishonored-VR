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

#define DVR_CAT ::dvr::log::Cat::script

static dvr::hooks::Detour g_thDet;
static std::atomic<bool> g_thOn{true};                  // [Aim] ThrowFromHand
static std::atomic<bool> g_gdOn{true};                  // [Aim] GadgetFromHand (below)
static std::atomic<bool> g_ctOn{true};                  // [Aim] CarryThrowFromHand (VR-181, below)
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
    Log("carry/aim: on|off, lt on|off, hold on|off (left-trigger throw %s). Aim now %s, %ld/%ld driven/seen, refused %ld (last: %s). Seen moves "
        "only when a carried object is thrown; a plain drop never reaches the seam",
        g_ctLeft.load() ? "ON" : "off", g_ctOn.load() ? "HAND" : "HEAD", (long)g_ctDriven, (long)g_ctSeen, (long)g_ctRefused, g_ctWhy);
    return true;
}

// ---- where the carried object is HELD (VR-181, second headset run) ------------------------
// The throw leaves along the hand, but until then the object sits where the game holds it: in
// front of the view. The pawn holds it with an RB_Handle (DishonoredPawn.m_pMovable_Handle)
// whose target is set through SetLocation / SetSmoothLocation (patterns.h). Both are only ever
// called through the vtable, so their ENTRIES see every writer. While the player carries a
// movable and the handle is the player's, the target moves to the hand: the ray origin plus
// [Aim] CarryHoldForwardCm along the ray. [Aim] CarryHoldAtHand=0 leaves the game's target.
// The calls are counted either way, so a carry with zero calls says plainly that the hold does
// not go through these setters, which is the answer that would send the search elsewhere.
static dvr::hooks::Detour g_hlLocDet, g_hlSmDet;
static uintptr_t g_hlLocBack = kHandleSetLocBack, g_hlSmBack = kHandleSmoothLocBack;
static std::atomic<bool> g_hlOn{true};                   // [Aim] CarryHoldAtHand
static float g_hlFwdCm = 15.0f;                          // [Aim] CarryHoldForwardCm
static volatile uint32_t g_hlHandleOff = 0;              // resolved on the script lane
static volatile LONG g_hlCalls[2] = {0, 0}, g_hlMine[2] = {0, 0}, g_hlDriven = 0;
static const char* volatile g_hlWhy = "not asked yet";
static uintptr_t g_hlRet[2] = {0, 0};                    // a caller of each, for the record
static float g_hlEngDist = -1, g_hlMoved = -1;           // last engine target vs the eye; our move
static volatile bool g_hlCarry = false;                  // script lane's view of the carry

static bool CarryHoldEnabled() { return g_hlOn.load(); }

static bool CarryingMovable()
{
    const auto s = dvr::anim::snapshot();
    if (!s.valid) return false;
    for (int i = 0; i < 3; ++i)
        if (!strcmp(s.state[i], "StatePlayerGrabMovable")) return true;
    return false;
}

// which: 0 SetLocation, 1 SetSmoothLocation. loc points at the by-value FVector argument.
extern "C" void __cdecl CarryHoldHandler(int which, uint8_t* handle, float* loc, uintptr_t ret)
{
    InterlockedIncrement(&g_hlCalls[which]);
    uint8_t* pawn = g_pePawn;
    const uint32_t off = g_hlHandleOff;
    if (!pawn || !off || !handle || !RangeReadable(pawn + off, 4) || *(uint8_t**)(pawn + off) != handle) return;
    InterlockedIncrement(&g_hlMine[which]);
    if (!g_hlRet[which]) g_hlRet[which] = ret;
    const char* why = nullptr;
    float cam[3];
    if (!g_hlOn.load()) why = "game hold selected";
    else if (g_gamepadOnly) why = "[Mode] GamepadOnly=1 keeps the head";
    else if (!g_hlCarry) why = "not carrying a movable";
    else if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) why = "not in gameplay";
    else if (!RangeReadable(loc, 12)) why = "target unreadable";
    else if (!dvr::camera::render_pos_world(cam)) why = "render eye position unknown";
    float o[3], d[3];
    if (!why && !HandRayWorld(o, d, &why)) {}
    if (why) { g_hlWhy = why; return; }
    const float e[3] = { loc[0] - cam[0], loc[1] - cam[1], loc[2] - cam[2] };
    g_hlEngDist = sqrtf(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    const float f = g_hlFwdCm * g_posScaleUU / 100.0f;   // cm -> uu (g_posScaleUU is uu per metre)
    const float n[3] = { o[0] + d[0] * f, o[1] + d[1] * f, o[2] + d[2] * f };
    const float m[3] = { n[0] - loc[0], n[1] - loc[1], n[2] - loc[2] };
    g_hlMoved = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
    loc[0] = n[0]; loc[1] = n[1]; loc[2] = n[2];
    InterlockedIncrement(&g_hlDriven);
    g_hlWhy = "driving";
}

__declspec(naked) static void CarryHoldLocThunk()
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
        mov eax, [edx + 36]         ; the caller's return address (above pushad + pushfd)
        push eax
        lea eax, [edx + 40]         ; the FVector argument
        push eax
        push ecx                    ; the RB_Handle
        push 0
        call CarryHoldHandler
        add esp, 16
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp                    ; the five displaced bytes
        mov ebp, esp
        push -1
        jmp dword ptr [g_hlLocBack]
    }
}

__declspec(naked) static void CarryHoldSmoothThunk()
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
        mov eax, [edx + 36]
        push eax
        lea eax, [edx + 40]
        push eax
        push ecx
        push 1
        call CarryHoldHandler
        add esp, 16
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp                    ; the six displaced bytes
        mov ebp, esp
        mov eax, [ebp + 8]
        jmp dword ptr [g_hlSmBack]
    }
}

// Script lane: resolve the handle's offset by name, follow the carry, and report once a
// second while carrying - which setter the game used, how often, and what we did.
static void CarryHoldTick()
{
    if (!g_hlHandleOff) {
        static double nextTry = 0; const double now = MaimNowMs();
        if (now >= nextTry) { nextTry = now + 5000; g_hlHandleOff = RflOffsetOf("DishonoredPawn", "m_pMovable_Handle"); }
    }
    const bool carry = CarryingMovable();
    static bool was = false;
    static LONG c0 = 0, c1 = 0, m0 = 0, m1 = 0, dv = 0;
    if (carry && !was) { c0 = g_hlCalls[0]; c1 = g_hlCalls[1]; m0 = g_hlMine[0]; m1 = g_hlMine[1]; dv = g_hlDriven; }
    g_hlCarry = carry;
    static double nextLog = 0; const double now = MaimNowMs();
    if ((carry && now >= nextLog) || (was && !carry)) {
        nextLog = now + 1000;
        Log("carry/hold: %s - the player's handle (+0x%X) took SetLocation %ld, SetSmoothLocation %ld "
            "(all handles %ld/%ld); driven %ld (last: %s). Engine target %.0f uu from the eye, moved "
            "%.0f uu to the hand. Callers %08X / %08X. Zero of both while carrying = the hold is not "
            "set through these", carry ? "carrying" : "carry ended", (unsigned)g_hlHandleOff,
            (long)(g_hlMine[0] - m0), (long)(g_hlMine[1] - m1), (long)(g_hlCalls[0] - c0),
            (long)(g_hlCalls[1] - c1), (long)(g_hlDriven - dv), g_hlWhy, g_hlEngDist, g_hlMoved,
            (unsigned)g_hlRet[0], (unsigned)g_hlRet[1]);
    }
    was = carry;
}

static void CarryHoldSet(bool on, const char* who)
{
    g_hlOn.store(on);
    if (!g_hlLocDet.on)
        dvr::hooks::detour_install(g_hlLocDet, "carry/hold loc", kHandleSetLoc, kHandleSetLocBytes,
                                   sizeof(kHandleSetLocBytes), (void*)&CarryHoldLocThunk);
    if (!g_hlSmDet.on)
        dvr::hooks::detour_install(g_hlSmDet, "carry/hold smooth", kHandleSmoothLoc, kHandleSmoothLocBytes,
                                   sizeof(kHandleSmoothLocBytes), (void*)&CarryHoldSmoothThunk);
    Log("carry/hold: owner %s (%s), %.0f cm along the ray - hooks %s/%s (they stay in to count "
        "calls even with the game's hold)", on ? "HAND" : "GAME", who, g_hlFwdCm,
        g_hlLocDet.on ? "ready" : "NOT installed", g_hlSmDet.on ? "ready" : "NOT installed");
}
static void CarryHoldConfigure(const char* ini)
{
    g_hlFwdCm = IniFloat(ini, "Aim", "CarryHoldForwardCm", 15);
    if (!(g_hlFwdCm >= 0 && g_hlFwdCm <= 100)) g_hlFwdCm = 15;
    CarryHoldSet(IniFloat(ini, "Aim", "CarryHoldAtHand", 1) != 0.0f, "ini [Aim] CarryHoldAtHand");
}
static float CarryHoldForwardCm() { return g_hlFwdCm; }
static void CarryHoldSetForwardCm(float cm) { if (cm >= 0 && cm <= 100) g_hlFwdCm = cm; }