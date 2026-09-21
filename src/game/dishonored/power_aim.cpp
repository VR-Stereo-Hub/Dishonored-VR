// game/dishonored/power_aim.cpp - VR-44: WINDBLAST, POSSESSION AND DEVOURING SWARM AIMED BY HAND.
//
// Measured (build 618, power/census): each power reads the head in its own place.
// * Windblast, once per cast: its routine 0x00BF9570 fetches the camera actor
//   (0x00B515C0 at 0x00BF9610) and reads the POV rotator +0x33C and location +0x330 off
//   it into m_vOrigin (+0xA4) and, through 0x0040DA70, m_vDirection (+0xB0).
// * Possession, every tick while held: the target pick at 0x00BF8F08.. puts the camera
//   actor's location in ebp-0x40 and its direction in ebp-0xA4, then scores every
//   candidate by distance and angle against both. The camera pointer (ebp-0x90) is used
//   afterwards only for its FOV (+0x53C).
// * Devouring Swarm never fetches the camera. Every cast runs UsePower's aim-assist search
//   0x00C12B00 (from slot +0x184, return 0x00C4B8AE), which reads the camera's POV
//   location and rotator through [controller+0x384]. The swarm spawns at its spawn point
//   (+0x94); the prediction is that the point comes from this search. swarm/place: lines
//   (aim_source.cpp) measure it against both rays.
//
// Each seam hands the engine the published hand ray (HandRayWorld) in the shape it was
// about to read, and nothing else: no engine field is written. [Aim] PowersFromHand
// (default 1) and the F10 Aim row choose; GamepadOnly keeps the head; any refusal logs
// why and leaves that read on the head.

#define DVR_CAT ::dvr::log::Cat::script

static std::atomic<bool> g_pwOn{true};                  // [Aim] PowersFromHand
static dvr::hooks::Detour g_pwDet[4];                   // windblast, possess, assist loc, assist rot
static uintptr_t g_pwBack[4] = { kWindPovSeam + sizeof(kWindPovSeamBytes),
                                 kPossPickSeam + sizeof(kPossPickSeamBytes),
                                 kAssistLocSeam + sizeof(kAssistLocSeamBytes),
                                 kAssistRotSeam + sizeof(kAssistRotSeamBytes) };
static volatile LONG g_pwSeen[3] = {0, 0, 0}, g_pwDriven[3] = {0, 0, 0};
static const char* volatile g_pwWhy[3] = { "not asked yet", "not asked yet", "not asked yet" };
static const char* const kPwName[3] = { "Windblast", "Possession", "aim-assist (Swarm)" };
// A camera-shaped block: the engine reads location +0x330 and rotator +0x33C off it.
// Game thread only, read by the engine right after the seam returns.
static uint8_t g_pwCam[0x350];
static uint32_t g_pwUse = 0;                            // the register value to continue with

static bool PowerAimEnabled() { return g_pwOn.load(); }

static bool PwGate(int which, const char** why)
{
    if (!g_pwOn.load()) { *why = "head aim selected"; return false; }
    if (g_gamepadOnly) { *why = "[Mode] GamepadOnly=1 keeps the head"; return false; }
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) { *why = "not in gameplay"; return false; }
    (void)which;
    return true;
}

// Fill g_pwCam's POV from the hand ray, keeping the engine's roll. `pov` is the real
// POV (location +0, rotator +0xC) or null.
static bool PwFillCam(const uint8_t* pov, float* o, float* d, const char** why)
{
    if (!HandRayWorld(o, d, why)) return false;
    const float kU = 32768.0f / 3.14159265f;
    const float h = sqrtf(d[0] * d[0] + d[1] * d[1]);
    if (pov) memcpy(g_pwCam + 0x330, pov, 0x18);
    float* L = (float*)(g_pwCam + 0x330); int32_t* R = (int32_t*)(g_pwCam + 0x33C);
    L[0] = o[0]; L[1] = o[1]; L[2] = o[2];
    R[0] = (int32_t)(atan2f(d[2], h) * kU);
    R[1] = (int32_t)(atan2f(d[1], d[0]) * kU);
    if (!pov) R[2] = 0;
    return true;
}

static void PwLogDrive(int which, const uint8_t* pov, const float* o)
{
    const float kU = 32768.0f / 3.14159265f;
    const int32_t* was = pov ? (const int32_t*)(pov + 0xC) : nullptr;
    const int32_t* now = (const int32_t*)(g_pwCam + 0x33C);
    const float* wl = pov ? (const float*)pov : nullptr;
    const float moved = wl ? sqrtf((o[0] - wl[0]) * (o[0] - wl[0]) + (o[1] - wl[1]) * (o[1] - wl[1]) +
                                   (o[2] - wl[2]) * (o[2] - wl[2])) : -1.0f;
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 30,
        "power/aim: %s aimed along the HAND - start moved %.0f uu, pitch %.1f -> %.1f deg, "
        "yaw %.1f -> %.1f deg (the engine's head aim -> ours)", kPwName[which], moved,
        was ? (int16_t)was[0] / kU * 57.29578f : 0.0f, now[0] / kU * 57.29578f,
        was ? (int16_t)was[1] / kU * 57.29578f : 0.0f, now[1] / kU * 57.29578f);
}

static void PwRefuse(int which, const char* why)
{
    g_pwWhy[which] = why;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "power/aim: %s REFUSED: %s - the head aims it", kPwName[which], why);
}

// Windblast: `cam` is the camera actor 0x00B515C0 returned. Sets g_pwUse = eax to continue with.
extern "C" void __cdecl PowerWindHandler(uint8_t* cam)
{
    InterlockedIncrement(&g_pwSeen[0]);
    g_pwUse = (uint32_t)(uintptr_t)cam;
    const char* why = nullptr;
    if (!PwGate(0, &why)) { PwRefuse(0, why); return; }
    if (!cam || !RangeReadable(cam + 0x330, 0x18)) { PwRefuse(0, "camera POV unreadable"); return; }
    float o[3], d[3];
    if (!PwFillCam(cam + 0x330, o, d, &why)) { PwRefuse(0, why); return; }
    g_pwUse = (uint32_t)(uintptr_t)g_pwCam;
    InterlockedIncrement(&g_pwDriven[0]);
    g_pwWhy[0] = "driving";
    PwLogDrive(0, cam + 0x330, o);
}

// Possession: `frame` is the pick's ebp. Overwrites its camera location and direction locals.
extern "C" void __cdecl PowerPossHandler(uint8_t* frame)
{
    InterlockedIncrement(&g_pwSeen[1]);
    const char* why = nullptr;
    if (!PwGate(1, &why)) { PwRefuse(1, why); return; }
    if (!frame || !RangeReadable(frame - 0xA4, 0xA4)) { PwRefuse(1, "pick frame unreadable"); return; }
    float o[3], d[3];
    if (!HandRayWorld(o, d, &why)) { PwRefuse(1, why); return; }
    float* loc = (float*)(frame - 0x40);
    float* dir = (float*)(frame - 0xA4);
    static float was[6]; memcpy(was, loc, 12); memcpy(was + 3, dir, 12);
    memcpy(loc, o, 12); memcpy(dir, d, 12);
    InterlockedIncrement(&g_pwDriven[1]);
    g_pwWhy[1] = "driving";
    const float dot = was[3] * d[0] + was[4] * d[1] + was[5] * d[2];
    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 12,
        "power/aim: Possession picks along the HAND - start moved %.0f uu, %.1f deg off the "
        "head's direction", sqrtf((o[0] - was[0]) * (o[0] - was[0]) + (o[1] - was[1]) * (o[1] - was[1]) +
                                  (o[2] - was[2]) * (o[2] - was[2])),
        acosf(dot > 1 ? 1 : (dot < -1 ? -1 : dot)) * 57.29578f);
}

// UsePower's aim-assist: `ctrl` is esi (the controller), `frame` its ebx (return at +4).
// `which` 0 = the location read, 1 = the rotator read; both continue with g_pwUse.
extern "C" void __cdecl PowerAssistHandler(uint8_t* ctrl, uint8_t* frame, int rotRead)
{
    uint8_t* real = (ctrl && RangeReadable(ctrl + 0x384, 4)) ? *(uint8_t**)(ctrl + 0x384) : nullptr;
    g_pwUse = (uint32_t)(uintptr_t)real;
    const uint32_t ret = (frame && RangeReadable(frame + 4, 4)) ? *(uint32_t*)(frame + 4) : 0;
    if (ret != kAssistFromUsePower) return;              // not a power cast: untouched
    if (!rotRead) InterlockedIncrement(&g_pwSeen[2]);
    const char* why = nullptr;
    if (!PwGate(2, &why)) { if (!rotRead) PwRefuse(2, why); return; }
    if (!real || !RangeReadable(real + 0x330, 0x18)) { if (!rotRead) PwRefuse(2, "camera POV unreadable"); return; }
    float o[3], d[3];
    if (!rotRead) {                                      // fill once per search, at the first read
        if (!PwFillCam(real + 0x330, o, d, &why)) { PwRefuse(2, why); g_pwCam[0] = 0; return; }
        g_pwCam[0] = 1;                                  // marks the block as filled this search
        InterlockedIncrement(&g_pwDriven[2]);
        g_pwWhy[2] = "driving";
        PwLogDrive(2, real + 0x330, o);
    } else if (g_pwCam[0] != 1) {
        return;                                          // the location read refused: stay whole
    }
    g_pwUse = (uint32_t)(uintptr_t)g_pwCam;
    if (rotRead) g_pwCam[0] = 0;
}

#define DVR_PW_SAVE                                                                         \
    __asm pushfd                                                                            \
    __asm pushad                                                                            \
    __asm mov edx, esp                                                                      \
    __asm sub esp, 528                                                                      \
    __asm and esp, -16                                                                      \
    __asm fxsave [esp]                                                                      \
    __asm fninit                                                                            \
    __asm cld                                                                               \
    __asm push edx
#define DVR_PW_RESTORE                                                                      \
    __asm pop edx                                                                           \
    __asm fxrstor [esp]                                                                     \
    __asm mov esp, edx                                                                      \
    __asm popad                                                                             \
    __asm popfd

__declspec(naked) static void PowerWindThunk()
{
    DVR_PW_SAVE
    __asm push eax                         // the camera actor
    __asm call PowerWindHandler
    __asm add esp, 4
    DVR_PW_RESTORE
    __asm mov eax, dword ptr [g_pwUse]
    __asm mov ecx, dword ptr [eax+33Ch]    // displaced: 8b 88 3c 03 00 00
    __asm jmp dword ptr [g_pwBack + 0]
}

__declspec(naked) static void PowerPossThunk()
{
    DVR_PW_SAVE
    __asm push ebp                         // the pick's frame
    __asm call PowerPossHandler
    __asm add esp, 4
    DVR_PW_RESTORE
    __asm mov eax, dword ptr ds:[0126B0E0h] // displaced: a1 e0 b0 26 01
    __asm jmp dword ptr [g_pwBack + 4]
}

__declspec(naked) static void PowerAssistLocThunk()
{
    DVR_PW_SAVE
    __asm push 0
    __asm push ebx                         // the search's frame: [ebx+4] its return address
    __asm push esi                         // the controller
    __asm call PowerAssistHandler
    __asm add esp, 12
    DVR_PW_RESTORE
    __asm mov eax, dword ptr [g_pwUse]     // replaces: mov eax,[esi+384h]
    __asm jmp dword ptr [g_pwBack + 8]
}

__declspec(naked) static void PowerAssistRotThunk()
{
    DVR_PW_SAVE
    __asm push 1
    __asm push ebx
    __asm push esi
    __asm call PowerAssistHandler
    __asm add esp, 12
    DVR_PW_RESTORE
    __asm mov ecx, dword ptr [g_pwUse]     // replaces: mov ecx,[esi+384h]
    __asm jmp dword ptr [g_pwBack + 12]
}
#undef DVR_PW_SAVE
#undef DVR_PW_RESTORE

static void PowerAimSet(bool on, const char* who)
{
    g_pwOn.store(on);
    if (on && !g_pwDet[0].on) {
        dvr::hooks::detour_install(g_pwDet[0], "power/aim windblast", kWindPovSeam, kWindPovSeamBytes,
                                   sizeof(kWindPovSeamBytes), (void*)&PowerWindThunk);
        dvr::hooks::detour_install(g_pwDet[1], "power/aim possession", kPossPickSeam, kPossPickSeamBytes,
                                   sizeof(kPossPickSeamBytes), (void*)&PowerPossThunk);
        // The two aim-assist reads go together or not at all: half a swap would trace from
        // the hand along the head.
        if (dvr::hooks::detour_install(g_pwDet[2], "power/aim assist loc", kAssistLocSeam,
                                       kAssistLocSeamBytes, sizeof(kAssistLocSeamBytes),
                                       (void*)&PowerAssistLocThunk) &&
            !dvr::hooks::detour_install(g_pwDet[3], "power/aim assist rot", kAssistRotSeam,
                                        kAssistRotSeamBytes, sizeof(kAssistRotSeamBytes),
                                        (void*)&PowerAssistRotThunk))
            dvr::hooks::detour_remove(g_pwDet[2], "power/aim assist loc");
    }
    dvr::aim::request_power_ray(on && !g_gamepadOnly);
    Log("power/aim: owner %s (%s) - Windblast %s, Possession %s, aim-assist (Swarm) %s",
        on ? "HAND" : "HEAD", who, g_pwDet[0].on ? "hooked" : "NOT hooked",
        g_pwDet[1].on ? "hooked" : "NOT hooked",
        g_pwDet[2].on && g_pwDet[3].on ? "hooked" : "NOT hooked");
}

static void PowerAimConfigure(const char* ini)
{
    PowerAimSet(IniFloat(ini, "Aim", "PowersFromHand", 1) != 0.0f, "ini [Aim] PowersFromHand");
}

static bool PowerAimCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        PowerAimSet(b, "seam");
        ConfigWriteKey("Aim", "PowersFromHand", b ? "1" : "0", "the seam");
        return true;
    }
    for (int i = 0; i < 3; ++i)
        Log("power/aim: %s %s - %ld/%ld driven/seen (last: %s)", kPwName[i],
            g_pwOn.load() ? "HAND" : "HEAD", (long)g_pwDriven[i], (long)g_pwSeen[i], g_pwWhy[i]);
    return true;
}
