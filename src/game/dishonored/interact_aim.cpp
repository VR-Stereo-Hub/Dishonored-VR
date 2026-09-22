// game/dishonored/interact_aim.cpp - VR-166: INTERACTION AIMED BY HAND.
//
// The game picks what you can interact with (m_pCrosshairActor, controller +0x69C)
// every tick, natively, from the camera. VR-85 proved the field is a RESULT - the
// engine rewrites it after our tick - so the only way to move it is to change what
// the engine traces along. The writer chain (ENGINE_NOTES "The interaction seam,
// found"): the controller tick 0x00ABA8DE calls the wrapper 0x00AB7B80, which
//
//   1. runs a first-pass trace 0x00AA5FF0 from the camera location along a
//      direction (the camera rotation, or the aim-assist cache when an item
//      supplies one), then
//   2. calls the usable selector 0x00AB70F0 on a view struct (+0x08 location,
//      +0x14 rotator), which traces its own interact distance along that view,
//   3. and hands the winner to the setter 0x00AA6280.
//
// Two byte-verified bridges hand the engine the published hand ray instead:
//   * at the line-check call inside 0x00AA5FF0 (0x00AA60B1), ONLY when 0x00AA5FF0
//     was called from the wrapper (its return address is 0x00AB7C8E): the origin
//     and direction POINTERS are swapped for the hand's;
//   * at the selector's entry, ONLY when called from its one caller (return
//     address 0x00AB7CBB): the view-struct POINTER is swapped for a copy whose
//     location and rotation are the hand's.
// No engine field is written. The engine still traces, validates, highlights and
// prompts - it just looks along the hand. Anything it would refuse, it refuses.
//
// The ray is the ONE published ray (dvr::aim, the measured model axis when latched,
// else the controller carried by the hand trim) - the same one the crosshair,
// Blink and the crossbow/pistol seams consume. [Aim] InteractFromHand (default 1)
// and the F10 Aim table choose; [Mode] GamepadOnly=1 always leaves the head.
// Fail soft: a refused ray leaves the engine's own head aim for that call.

#define DVR_CAT ::dvr::log::Cat::script

static dvr::hooks::Detour g_iaFirstDet, g_iaSelDet;
static std::atomic<bool> g_iaOn{true};                 // [Aim] InteractFromHand
static uintptr_t g_iaTraceFn   = kInteractTraceFn;
static uintptr_t g_iaFirstBack = kInteractFirstTraceBack;
static uintptr_t g_iaSelBack   = kInteractSelectorBack;
static volatile LONG g_iaFirstSeen = 0, g_iaFirstDriven = 0, g_iaSelSeen = 0, g_iaSelDriven = 0,
                     g_iaRefused = 0;
static const char* volatile g_iaWhy = "not asked yet";
static float g_iaLastOffDeg = -1;                      // hand ray vs the engine's head ray, last drive
// The engine reads these through the pointers we hand it, after the bridge returns.
// Game thread only; the wrapper is never re-entered.
static float   g_iaOrigin[3], g_iaDir[3];
static uint8_t g_iaView[0x40];

static bool InteractAimEnabled() { return g_iaOn.load(); }

static bool IaRefuse(const char* why) { g_iaWhy = why; InterlockedIncrement(&g_iaRefused); return false; }

// The head this game tick, in world units: the GAME camera's base with the mod's own offsets
// removed, plus the mod's positional offset in the yaw-only frame (how the camera lane writes
// it). Moved here from throw_aim.cpp (VR-181, where it fixed the held object's flicker) so
// every hand-ray consumer shares it.
static bool GameCameraAnchor(float out[3])
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

// [Aim] HandRayGameAnchor=1: the hand ray is anchored on the game camera, not on
// render_pos_world. render_pos_world is c5, the camera of whichever scene draw uploaded
// last - the left eye, the right eye, or a non-eye pass such as shadow depth - so the
// ray's origin jumped 3-6 uu between game ticks (carry/anchor measured it, 2026-09-22).
// Interaction traces EVERY tick, and at the edge of an object's use range that jitter took
// the hit in and out each frame: the grab prompt flickered about 20 times a second and a
// grab could not land (the focus sampler read DishonoredMovable / none on alternate
// samples). 0 = the old render-sample anchor, for A/B.
static std::atomic<bool> g_hrGameAnchor{true};
static volatile LONG g_hrAnchorGame = 0, g_hrAnchorRender = 0;

// VR-166: the ONE published aim ray in game world units - origin at the hand, unit
// direction. Shared by every engine consumer this branch adds (interaction, throws)
// so they cannot drift apart. `why` names the refusal.
static bool HandRayWorld(float* origin, float* dir, const char** why)
{
    const auto aim = dvr::aim::fire_frame();
    float camera[3];
    if (g_hrGameAnchor.load() && GameCameraAnchor(camera)) InterlockedIncrement(&g_hrAnchorGame);
    else if (dvr::camera::render_pos_world(camera)) InterlockedIncrement(&g_hrAnchorRender);
    else { *why = "no world camera position"; return false; }
    dvr::fireaim::Solution sol;
    if (!dvr::fireaim::solve(aim, GetTickCount64(), g_viewYawRad, g_viewPitchRad,
                             camera, g_posScaleUU, camera, sol)) {
        *why = aim.ray.ok ? (aim.headValid ? "ray geometry refused" : "no head pose with the ray")
                          : aim.ray.why;
        return false;
    }
    float d[3] = { sol.target[0] - sol.origin[0], sol.target[1] - sol.origin[1],
                   sol.target[2] - sol.origin[2] };
    if (!dvr::fireaim::normalize(d)) { *why = "degenerate ray"; return false; }
    memcpy(origin, sol.origin, 12); memcpy(dir, d, 12);
    return true;
}

// Interaction's gates, then the shared ray.
static bool IaHandRay(uint8_t* self, float* origin, float* dir)
{
    if (!g_iaOn.load()) return IaRefuse("head aim selected");
    if (g_gamepadOnly) return IaRefuse("[Mode] GamepadOnly=1 keeps the head");
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow)
        return IaRefuse("not in gameplay");
    if (!self || self != g_peCtrl) return IaRefuse("not the player's controller");
    const char* why = nullptr;
    if (!HandRayWorld(origin, dir, &why)) return IaRefuse(why);
    g_iaWhy = "driving";
    return true;
}

static void IaNoteOff(const float* engineDir)
{
    if (!engineDir || !RangeReadable((void*)engineDir, 12)) return;
    float e[3] = { engineDir[0], engineDir[1], engineDir[2] };
    if (!dvr::fireaim::normalize(e)) return;
    float c = dvr::fireaim::dot(e, g_iaDir);
    c = c > 1 ? 1 : (c < -1 ? -1 : c);
    g_iaLastOffDeg = acosf(c) * 57.29578f;
}

// args: the line check's stack arguments. [2] = origin pointer, [3] = direction pointer.
extern "C" void __cdecl InteractFirstTraceHandler(uint8_t* self, uint8_t* frame, uint32_t* args)
{
    if (!frame || !RangeReadable(frame + 4, 4) || *(uint32_t*)(frame + 4) != kInteractFirstPassRet) return;
    InterlockedIncrement(&g_iaFirstSeen);
    if (!RangeReadable(args, 16)) { IaRefuse("line-check arguments unreadable"); return; }
    if (!IaHandRay(self, g_iaOrigin, g_iaDir)) return;
    IaNoteOff((const float*)args[3]);
    args[2] = (uint32_t)(uintptr_t)g_iaOrigin;
    args[3] = (uint32_t)(uintptr_t)g_iaDir;
    InterlockedIncrement(&g_iaFirstDriven);
}

// top[0] = return address, top[1] = the view struct pointer.
extern "C" void __cdecl InteractSelectorHandler(uint8_t* self, uint32_t* top)
{
    if (!top || top[0] != kInteractSelectorRet) return;
    InterlockedIncrement(&g_iaSelSeen);
    uint8_t* view = (uint8_t*)(uintptr_t)top[1];
    if (!view || !RangeReadable(view, sizeof(g_iaView))) { IaRefuse("view struct unreadable"); return; }
    if (!IaHandRay(self, g_iaOrigin, g_iaDir)) return;
    memcpy(g_iaView, view, sizeof(g_iaView));
    memcpy(g_iaView + 0x08, g_iaOrigin, 12);
    const float kU = 32768.0f / 3.14159265f;
    const float h = sqrtf(g_iaDir[0] * g_iaDir[0] + g_iaDir[1] * g_iaDir[1]);
    int32_t rot[3];
    memcpy(rot, view + 0x14, 12);                      // roll stays the engine's
    rot[0] = (int32_t)(atan2f(g_iaDir[2], h) * kU);
    rot[1] = (int32_t)(atan2f(g_iaDir[1], g_iaDir[0]) * kU);
    memcpy(g_iaView + 0x14, rot, 12);
    top[1] = (uint32_t)(uintptr_t)g_iaView;
    InterlockedIncrement(&g_iaSelDriven);
}

// Replaces `call 0x00AA2C20` at 0x00AA60B1. ECX (the controller) and the pushed
// arguments are the engine's; after the handler the original call runs and returns
// to the instruction after the patch. fxsave keeps the live x87 stack intact.
__declspec(naked) static void InteractFirstTraceThunk()
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
        lea eax, [edx+24h]          ; the line check's first stack argument
        push eax
        push ebp                    ; 0x00AA5FF0's frame: [ebp+4] is who called it
        push ecx                    ; the controller
        call InteractFirstTraceHandler
        add esp, 12
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        call dword ptr [g_iaTraceFn]
        jmp dword ptr [g_iaFirstBack]
    }
}

// Replaces the selector's first 10 bytes. [esp] is its return address, [esp+4] the
// view struct pointer, ECX the controller.
__declspec(naked) static void InteractSelectorThunk()
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
        lea eax, [edx+24h]          ; &return address, then the argument
        push eax
        push ecx
        call InteractSelectorHandler
        add esp, 8
        pop edx
        fxrstor [esp]
        mov esp, edx
        popad
        popfd
        push ebp                    ; the ten displaced bytes
        mov ebp, esp
        push -1
        push 0F4FDD0h
        jmp dword ptr [g_iaSelBack]
    }
}

static void InteractAimInstall()
{
    if (!g_iaFirstDet.on)
        dvr::hooks::detour_install(g_iaFirstDet, "interact/first-pass", kInteractFirstTraceCall,
                                   kInteractFirstTraceCallBytes, sizeof(kInteractFirstTraceCallBytes),
                                   (void*)&InteractFirstTraceThunk);
    if (!g_iaSelDet.on)
        dvr::hooks::detour_install(g_iaSelDet, "interact/selector", kInteractSelector,
                                   kInteractSelectorBytes, sizeof(kInteractSelectorBytes),
                                   (void*)&InteractSelectorThunk);
}

static void InteractAimSet(bool on, const char* who)
{
    g_iaOn.store(on);
    if (on) InteractAimInstall();
    dvr::aim::request_interact_ray(on && !g_gamepadOnly);
    Log("interact/aim: owner %s (%s) - hooks first-pass=%s selector=%s. The engine still traces, "
        "validates and highlights; only the ray it looks along changes",
        on ? "HAND (the published aim ray)" : "HEAD (the engine's own view)", who,
        g_iaFirstDet.on ? "ready" : "NOT installed", g_iaSelDet.on ? "ready" : "NOT installed");
}

static void InteractAimConfigure(const char* ini)
{
    g_hrGameAnchor.store(IniFloat(ini, "Aim", "HandRayGameAnchor", 1) != 0.0f);
    Log("aim/anchor: the hand ray's origin is anchored on the %s ([Aim] HandRayGameAnchor)",
        g_hrGameAnchor.load() ? "GAME camera (stable every tick)" : "LAST RENDER SAMPLE (the old anchor; jumps between eyes)");
    InteractAimSet(IniFloat(ini, "Aim", "InteractFromHand", 1) != 0.0f, "ini [Aim] InteractFromHand");
}

static bool InteractAimCommand(const char* args)
{
    bool b = false;
    if (DvrOnOff(args, &b)) {
        InteractAimSet(b, "seam");
        ConfigWriteKey("Aim", "InteractFromHand", b ? "1" : "0", "the seam");
        return true;
    }
    Log("interact/aim: on|off (now %s) first-pass %ld/%ld selector %ld/%ld driven/seen, refused %ld (last: %s)",
        g_iaOn.load() ? "HAND" : "HEAD", (long)g_iaFirstDriven, (long)g_iaFirstSeen,
        (long)g_iaSelDriven, (long)g_iaSelSeen, (long)g_iaRefused, g_iaWhy);
    return true;
}

// Script lane. What the engine focused, and who aimed it - logged on CHANGE.
static void InteractAimTick()
{
    static uint32_t focusOff = 0;
    const double now = MaimNowMs();
    // THE FLICKER COUNTER: the focus read once per rendered frame, and every change counted.
    // The 250 ms sampler below aliases a 20 Hz toggle into "a change on every sample"; this
    // names the rate. A steady focus reads 0-1 per second; the fault read about 20.
    {
        static uint32_t lastFrame = 0xffffffffu; static uint8_t* was = (uint8_t*)1;
        static LONG flips = 0, frames = 0; static double winStart = 0;
        const uint32_t frame = (uint32_t)dvr::frame::count();
        if (focusOff && frame != lastFrame && g_peCtrl && LooksLikeObj(g_peCtrl) &&
            RangeReadable(g_peCtrl + focusOff, 4)) {
            lastFrame = frame; ++frames;
            uint8_t* f = *(uint8_t**)(g_peCtrl + focusOff);
            if (f != was) { if (was != (uint8_t*)1) ++flips; was = f; }
            if (winStart == 0) winStart = now;
            if (now - winStart >= 1000.0) {
                if (flips >= 4)
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
                        "interact/flicker: the focused object changed %ld times in %ld frames (%.0f ms) - a "
                        "steady focus reads 0-1. Anchor %s (game %ld, render %ld hand-ray solves so far)",
                        (long)flips, (long)frames, now - winStart,
                        g_hrGameAnchor.load() ? "GAME camera" : "RENDER sample",
                        (long)g_hrAnchorGame, (long)g_hrAnchorRender);
                flips = 0; frames = 0; winStart = now;
            }
        }
    }
    static double next = 0;
    if (now < next) return;
    next = now + 250;
    if (!focusOff) focusOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
    uint8_t* pc = g_peCtrl;
    static uint8_t* lastFocus = (uint8_t*)1;
    uint8_t* focus = NULL;
    if (focusOff && pc && LooksLikeObj(pc) && RangeReadable(pc + focusOff, 4)) {
        focus = *(uint8_t**)(pc + focusOff);
        if (focus && !LooksLikeObj(focus)) focus = NULL;
    }
    if (focus != lastFocus) {
        lastFocus = focus;
        const char* cn = focus ? ObjClassName(focus) : NULL;
        DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 200,
            "interact/focus: %s | owner %s, hand ray %.1f deg off the head's on the last drive "
            "(first-pass %ld/%ld selector %ld/%ld driven/seen, refused %ld, last: %s)",
            cn ? cn : "none", g_iaOn.load() ? "HAND" : "HEAD", (double)g_iaLastOffDeg,
            (long)g_iaFirstDriven, (long)g_iaFirstSeen, (long)g_iaSelDriven, (long)g_iaSelSeen,
            (long)g_iaRefused, g_iaWhy);
    }
    static double nextBeat = 0;
    if (now >= nextBeat && g_iaOn.load()) {
        nextBeat = now + 10000;
        Log("interact/aim: beat owner HAND first-pass %ld/%ld selector %ld/%ld driven/seen, refused %ld "
            "(last: %s). Seen stays 0 if the engine never asks - both hooks fire every gameplay tick",
            (long)g_iaFirstDriven, (long)g_iaFirstSeen, (long)g_iaSelDriven, (long)g_iaSelSeen,
            (long)g_iaRefused, g_iaWhy);
    }
}
