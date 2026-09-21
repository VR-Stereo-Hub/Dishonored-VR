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
    dvr::aim::request_throw_ray((on || g_gdOn.load()) && !g_gamepadOnly);
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

// ---- the gadget projectile (spring razor) -------------------------------------
// 0x00C30040 is shared by seven vtable slots: it spawns a gadget projectile at the hand
// (SpawnActor at 0x00C300AB), then builds the throw direction from the SOURCE PAWN's
// rotation (+0xD0) at 0x00C300E6 - the head, in VR. The 9 bytes at 0x00C300DD
// (mov ecx,[ebp-4]; add ecx,0D0h) become "ECX = the rotator to use": the pawn's, or a
// hand-ray rotator for the player's own throw. [Aim] GadgetFromHand (default 1).
static dvr::hooks::Detour g_gdDet;
static uintptr_t g_gdBack = kGadgetRotBack;
static uint32_t g_gdUse = 0;                          // ECX the conversion receives
static int32_t g_gdRot[3];
static volatile LONG g_gdSeen = 0, g_gdDriven = 0;
static const char* volatile g_gdWhy = "not asked yet";

static bool GadgetAimEnabled() { return g_gdOn.load(); }

extern "C" void __cdecl GadgetAimHandler(uint8_t* frame, uint8_t* self)
{
    InterlockedIncrement(&g_gdSeen);
    uint8_t* pawn = (frame && RangeReadable(frame - 4, 4)) ? *(uint8_t**)(frame - 4) : nullptr;
    g_gdUse = (uint32_t)(uintptr_t)(pawn + 0xD0);        // the engine's own choice
    const char* why = nullptr;
    if (!g_gdOn.load()) why = "head aim selected";
    else if (g_gamepadOnly) why = "[Mode] GamepadOnly=1 keeps the head";
    else if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) why = "not in gameplay";
    else if (!pawn || pawn != g_pePawn) why = "not the player's throw";
    else if (!RangeReadable(pawn + 0xD0, 12)) why = "pawn rotation unreadable";
    float o[3], d[3];
    if (!why && !HandRayWorld(o, d, &why)) {}
    if (why) { g_gdWhy = why; return; }
    const float kU = 32768.0f / 3.14159265f;
    const int32_t* was = (const int32_t*)(pawn + 0xD0);
    const float h = sqrtf(d[0] * d[0] + d[1] * d[1]);
    g_gdRot[0] = (int32_t)(atan2f(d[2], h) * kU);
    g_gdRot[1] = (int32_t)(atan2f(d[1], d[0]) * kU);
    g_gdRot[2] = was[2];
    g_gdUse = (uint32_t)(uintptr_t)g_gdRot;
    InterlockedIncrement(&g_gdDriven);
    g_gdWhy = "driving";
    const char* cn = (self && LooksLikeObj(self)) ? ObjClassName(self) : "?";
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 40,
        "gadget/aim: %s thrown along the HAND - pitch %.1f -> %.1f deg, yaw %.1f -> %.1f deg "
        "(the pawn's rotation -> ours; spawn point, speed and arc stay the game's)",
        cn, was[0] / kU * 57.29578f, g_gdRot[0] / kU * 57.29578f,
        was[1] / kU * 57.29578f, g_gdRot[1] / kU * 57.29578f);
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
        push esi                    ; the routine's object
        push ebp                    ; its frame: [ebp-4] is the source pawn
        call GadgetAimHandler
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        mov ecx, dword ptr [g_gdUse] ; replaces: mov ecx,[ebp-4]; add ecx,0D0h
        jmp dword ptr [g_gdBack]
    }
}

static void GadgetAimSet(bool on, const char* who)
{
    g_gdOn.store(on);
    if (on && !g_gdDet.on)
        dvr::hooks::detour_install(g_gdDet, "gadget/aim", kGadgetRotSeam, kGadgetRotSeamBytes,
                                   sizeof(kGadgetRotSeamBytes), (void*)&GadgetAimThunk);
    dvr::aim::request_throw_ray((on || g_thOn.load()) && !g_gamepadOnly);
    Log("gadget/aim: owner %s (%s) - hook %s. Spring razors (the shared gadget routine) leave "
        "along the published hand ray; spawn point, speed and arc stay the game's",
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
    Log("gadget/aim: on|off (now %s) %ld/%ld driven/seen (last: %s). Seen moves only when a "
        "gadget is thrown", g_gdOn.load() ? "HAND" : "HEAD", (long)g_gdDriven, (long)g_gdSeen, g_gdWhy);
    return true;
}
