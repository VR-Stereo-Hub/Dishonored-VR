// game/dishonored/power_aim.cpp - VR-44: WINDBLAST, POSSESSION AND DEVOURING SWARM AIMED BY HAND.
//
// Measured (builds 618-619, power/census and swarm/place): each power reads the head in
// its own place.
// * Windblast, once per cast: its routine 0x00BF9570 fetches the camera actor
//   (0x00B515C0 at 0x00BF9610) and reads the POV rotator +0x33C and location +0x330 off
//   it into m_vOrigin (+0xA4) and, through 0x0040DA70, m_vDirection (+0xB0).
// * Possession, every tick while held: the target pick at 0x00BF8F08.. puts the camera
//   actor's location in ebp-0x40 and its direction in ebp-0xA4, then scores every
//   candidate by distance and angle against both. The camera pointer (ebp-0x90) is used
//   afterwards only for its FOV (+0x53C).
// * Devouring Swarm, per cast: 0x00BE9310 (both callers Swarm's) asks the controller for
//   GetPlayerViewPoint (vtable +0x3C4) into ebp-0x18 / ebp-0x30, traces along it and
//   places the spawn point at the hit. Build 619 swapped the camera in UsePower's
//   aim-assist search instead; the swarm still followed the head (retired, ENGINE_NOTES).
//
// Each seam hands the engine the published hand ray (HandRayWorld) in the shape it was
// about to read, and nothing else: no engine field is written. A seam refuses when the
// engine's own view source is not at the player's eye (possessing, a scripted camera):
// build 619 saw the camera POV 7,883 uu away. [Aim] PowersFromHand (default 1) and the
// F10 Aim row choose; GamepadOnly keeps the head; any refusal logs why and leaves that
// read on the head.

#define DVR_CAT ::dvr::log::Cat::script

static std::atomic<bool> g_pwOn{true};                  // [Aim] PowersFromHand
static dvr::hooks::Detour g_pwDet[3];                   // windblast, possession, swarm
static uintptr_t g_pwBack[3] = { kWindPovSeam + sizeof(kWindPovSeamBytes),
                                 kPossPickSeam + sizeof(kPossPickSeamBytes),
                                 kSwarmViewSeam + sizeof(kSwarmViewSeamBytes) };
static volatile LONG g_pwSeen[3] = {0, 0, 0}, g_pwDriven[3] = {0, 0, 0};
static const char* volatile g_pwWhy[3] = { "not asked yet", "not asked yet", "not asked yet" };
static const char* const kPwName[3] = { "Windblast", "Possession", "Devouring Swarm" };
static const float kPwNear = 150.0f;                    // uu: the engine's view source vs the render eye
// A camera-shaped block for Windblast: the engine reads location +0x330 and rotator
// +0x33C..+0x344 off it. Game thread only, read right after the seam returns.
static uint8_t g_pwCam[0x350];
static uint32_t g_pwUse = 0;                            // eax to continue with (Windblast)

static bool PowerAimEnabled() { return g_pwOn.load(); }

static void PwRefuse(int which, const char* why, float sep)
{
    g_pwWhy[which] = why;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "power/aim: %s REFUSED: %s (engine view source %.0f uu from the render eye, limit %.0f) - "
        "the head aims it", kPwName[which], why, sep, kPwNear);
}

// The shared gates. `src` is the engine's own view location, which must be at the eye.
static bool PwGate(int which, const float* src, const char** why, float* sep)
{
    *sep = -1.0f;
    if (!g_pwOn.load()) { *why = "head aim selected"; return false; }
    if (g_gamepadOnly) { *why = "[Mode] GamepadOnly=1 keeps the head"; return false; }
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) { *why = "not in gameplay"; return false; }
    float cam[3];
    if (!dvr::camera::render_pos_world(cam)) { *why = "render eye position unknown"; return false; }
    const float v[3] = { src[0] - cam[0], src[1] - cam[1], src[2] - cam[2] };
    *sep = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (!(*sep < kPwNear)) { *why = "the engine's view source is not at the player's eye"; return false; }
    (void)which;
    return true;
}

static void PwRot(const float* d, int32_t* r)
{
    const float kU = 32768.0f / 3.14159265f;
    const float h = sqrtf(d[0] * d[0] + d[1] * d[1]);
    r[0] = (int32_t)(atan2f(d[2], h) * kU);
    r[1] = (int32_t)(atan2f(d[1], d[0]) * kU);
}

static void PwLogDrive(int which, const float* wasLoc, const int32_t* wasRot,
                       const float* o, const int32_t* rot)
{
    const float kU = 32768.0f / 3.14159265f;
    const float moved = sqrtf((o[0] - wasLoc[0]) * (o[0] - wasLoc[0]) + (o[1] - wasLoc[1]) * (o[1] - wasLoc[1]) +
                              (o[2] - wasLoc[2]) * (o[2] - wasLoc[2]));
    // One budget per power, so a busy one cannot use up the others' lines.
    static LONG told[3] = {0, 0, 0};
    if (InterlockedIncrement(&told[which]) > 20) return;
    Log("power/aim: %s aimed along the HAND - start moved %.0f uu, pitch %.1f -> %.1f deg, "
        "yaw %.1f -> %.1f deg (the engine's head aim -> ours)", kPwName[which], moved,
        (int16_t)wasRot[0] / kU * 57.29578f, rot[0] / kU * 57.29578f,
        (int16_t)wasRot[1] / kU * 57.29578f, rot[1] / kU * 57.29578f);
}

// Windblast: `cam` is the camera actor 0x00B515C0 returned. Sets g_pwUse = eax to continue with.
extern "C" void __cdecl PowerWindHandler(uint8_t* cam)
{
    InterlockedIncrement(&g_pwSeen[0]);
    g_pwUse = (uint32_t)(uintptr_t)cam;
    const char* why = nullptr; float sep = -1.0f;
    if (!cam || !RangeReadable(cam + 0x330, 0x18)) { PwRefuse(0, "camera POV unreadable", sep); return; }
    const float* pov = (const float*)(cam + 0x330);
    if (!PwGate(0, pov, &why, &sep)) { PwRefuse(0, why, sep); return; }
    float o[3], d[3];
    if (!HandRayWorld(o, d, &why)) { PwRefuse(0, why, sep); return; }
    memcpy(g_pwCam + 0x330, cam + 0x330, 0x18);
    float* L = (float*)(g_pwCam + 0x330); int32_t* R = (int32_t*)(g_pwCam + 0x33C);
    L[0] = o[0]; L[1] = o[1]; L[2] = o[2];
    PwRot(d, R);                                          // roll stays the engine's
    g_pwUse = (uint32_t)(uintptr_t)g_pwCam;
    InterlockedIncrement(&g_pwDriven[0]);
    g_pwWhy[0] = "driving";
    PwLogDrive(0, pov, (const int32_t*)(cam + 0x33C), o, R);
}

// Possession: `frame` is the pick's ebp. Overwrites its camera location and direction locals.
extern "C" void __cdecl PowerPossHandler(uint8_t* frame)
{
    InterlockedIncrement(&g_pwSeen[1]);
    const char* why = nullptr; float sep = -1.0f;
    if (!frame || !RangeReadable(frame - 0xA4, 0xA4)) { PwRefuse(1, "pick frame unreadable", sep); return; }
    float* loc = (float*)(frame - 0x40);
    float* dir = (float*)(frame - 0xA4);
    if (!PwGate(1, loc, &why, &sep)) { PwRefuse(1, why, sep); return; }
    float o[3], d[3];
    if (!HandRayWorld(o, d, &why)) { PwRefuse(1, why, sep); return; }
    int32_t wasRot[3] = {0, 0, 0}, rot[3] = {0, 0, 0};
    PwRot(dir, wasRot); PwRot(d, rot);
    const float wasLoc[3] = { loc[0], loc[1], loc[2] };
    memcpy(loc, o, 12); memcpy(dir, d, 12);
    InterlockedIncrement(&g_pwDriven[1]);
    g_pwWhy[1] = "driving";
    PwLogDrive(1, wasLoc, wasRot, o, rot);
}

// Swarm: `frame` is 0x00BE9310's ebp, just after GetPlayerViewPoint filled ebp-0x18
// (location) and ebp-0x30 (rotator). Overwrites both.
extern "C" void __cdecl PowerSwarmHandler(uint8_t* frame)
{
    InterlockedIncrement(&g_pwSeen[2]);
    const char* why = nullptr; float sep = -1.0f;
    if (!frame || !RangeReadable(frame - 0x30, 0x24)) { PwRefuse(2, "view point frame unreadable", sep); return; }
    float* loc = (float*)(frame - 0x18);
    int32_t* rot = (int32_t*)(frame - 0x30);
    if (!PwGate(2, loc, &why, &sep)) { PwRefuse(2, why, sep); return; }
    float o[3], d[3];
    if (!HandRayWorld(o, d, &why)) { PwRefuse(2, why, sep); return; }
    const float wasLoc[3] = { loc[0], loc[1], loc[2] };
    const int32_t wasRot[3] = { rot[0], rot[1], rot[2] };
    memcpy(loc, o, 12);
    PwRot(d, rot);                                        // roll stays the engine's
    InterlockedIncrement(&g_pwDriven[2]);
    g_pwWhy[2] = "driving";
    PwLogDrive(2, wasLoc, wasRot, o, rot);
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

__declspec(naked) static void PowerSwarmThunk()
{
    DVR_PW_SAVE
    __asm push ebp                         // 0x00BE9310's frame
    __asm call PowerSwarmHandler
    __asm add esp, 4
    DVR_PW_RESTORE
    __asm lea eax, [ebp-24h]               // displaced: 8d 45 dc 50 8d 4d d0
    __asm push eax
    __asm lea ecx, [ebp-30h]
    __asm jmp dword ptr [g_pwBack + 8]
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
        dvr::hooks::detour_install(g_pwDet[2], "power/aim swarm", kSwarmViewSeam, kSwarmViewSeamBytes,
                                   sizeof(kSwarmViewSeamBytes), (void*)&PowerSwarmThunk);
    }
    dvr::aim::request_power_ray(on && !g_gamepadOnly);
    Log("power/aim: owner %s (%s) - Windblast %s, Possession %s, Devouring Swarm %s",
        on ? "HAND" : "HEAD", who, g_pwDet[0].on ? "hooked" : "NOT hooked",
        g_pwDet[1].on ? "hooked" : "NOT hooked", g_pwDet[2].on ? "hooked" : "NOT hooked");
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
