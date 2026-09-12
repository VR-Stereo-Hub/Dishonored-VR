// Included by the Dishonored unity TU. Only modifies native firing stack locals.
#include "game/dishonored/fire_aim_math.h"
#include <atomic>

static std::atomic<bool> g_fireAimEnabled{false};
static dvr::hooks::Detour g_fireAimDet, g_pistolAimDet;
static uintptr_t g_fireAimResume = kCrossbowSpawnAim + sizeof(kCrossbowSpawnAimBytes);
static uintptr_t g_pistolAimResume = kPistolSpawnAim + sizeof(kPistolSpawnAimBytes);
static volatile LONG g_fireAimSeen=0, g_fireAimChanged=0, g_fireAimRefused=0;

static bool FireAimEnabled() { return g_fireAimEnabled.load(); }
static void FireAimSet(bool on, const char* source) {
    g_fireAimEnabled.store(on);
    dvr::aim::request_fire_ray(on);
    Log("fireaim: %s (%s), native hooks crossbow=%s pistol=%s; player firing contexts "
        "only. Camera/body rotations and the HUD cache are not written.",
        on ? "ON" : "off", source, g_fireAimDet.on ? "ready" : "not installed",
        g_pistolAimDet.on ? "ready" : "not installed");
}
static void FireAimRefuse(const char* weapon, const char* why) {
    InterlockedIncrement(&g_fireAimRefused);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
        "fireaim: original aim retained (%s): %s (seen=%ld changed=%ld refused=%ld)",
        weapon,why,g_fireAimSeen,g_fireAimChanged,g_fireAimRefused);
}

// Everything both seams must agree on before either writes a stack local. Logs
// its own reason and the values that produced it, then returns false.
// spanLocal/spanLen is the whole window of locals the caller will touch, checked
// before anything in it is read; sourceLocal is the pawn pointer inside it.
static bool FireAimGate(const char* weapon, uint8_t* context, uintptr_t wantVtable,
                        uint8_t* frame, int spanLocal, size_t spanLen, int sourceLocal,
                        uint8_t** outSource) {
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow ||
        !g_scriptHeadOK || MaimNowMs()-g_scriptHeadMs>100.0 || g_maimEnabled) {
        FireAimRefuse(weapon,"gameplay/head sample unavailable or legacy MotionAim enabled");
        return false;
    }
    if (!context || !RangeReadable(context,sizeof(void*)) ||
        *(uintptr_t*)context!=wantVtable ||
        !frame || !RangeReadable(frame+spanLocal,spanLen) ||
        !RangeReadable(frame+sourceLocal,sizeof(void*))) {
        FireAimRefuse(weapon,"native context/frame contract mismatch"); return false;
    }
    // Source is the pawn already resolved by the native firing routine. Verify
    // possession independently; the cached ProcessEvent latches are not immortal.
    uint8_t* source=*(uint8_t**)(frame+sourceLocal);
    uint8_t* ctrl=g_peCtrl;
    if (!source || source!=g_pePawn || !LooksLikeObj(source) || !LooksLikeObj(ctrl)) {
        FireAimRefuse(weapon,"not the current player pawn"); return false;
    }
    const char* cls=ObjClassName(ctrl);
    if (!cls || !strstr(cls,"PlayerController")) {
        FireAimRefuse(weapon,"controller class mismatch"); return false;
    }
    const uint32_t pawnOff=RflOffsetOf("Controller","Pawn");
    const uint32_t controllerOff=RflOffsetOf("Pawn","Controller");
    if (!pawnOff || !controllerOff || !RangeReadable(ctrl+pawnOff,4) ||
        !RangeReadable(source+controllerOff,4) || *(uint8_t**)(ctrl+pawnOff)!=source ||
        *(uint8_t**)(source+controllerOff)!=ctrl) {
        FireAimRefuse(weapon,"possession could not be verified"); return false;
    }
    if (outSource) *outSource=source;
    return true;
}

static void FireAimApply(uint8_t* context, uint8_t* frame) {
    if (!FireAimEnabled()) return;
    InterlockedIncrement(&g_fireAimSeen);
    uint8_t* source=nullptr;
    if (!FireAimGate("crossbow",context,kCrossbowContextVtable,frame,
                     kCrossbowSpawnLocal,0xB8,kCrossbowSourceLocal,&source)) return;
    const auto aim=dvr::aim::fire_frame();
    float camera[3];
    if (!dvr::camera::render_pos_world(camera)) {
        FireAimRefuse("crossbow","no world camera position"); return;
    }
    const float* spawn=(const float*)(frame+kCrossbowSpawnLocal);
    float* direction=(float*)(frame+kCrossbowDirectionLocal);
    const float original[3]={direction[0],direction[1],direction[2]};
    const float n=dvr::fireaim::dot(original,original);
    if (!std::isfinite(n) || n<0.5f || n>1.5f) {
        FireAimRefuse("crossbow","native direction is not unit"); return;
    }
    dvr::fireaim::Solution solve;
    if (!dvr::fireaim::solve(aim,GetTickCount64(),g_viewYawRad,g_viewPitchRad,
                            camera,g_posScaleUU,spawn,solve)) {
        FireAimRefuse("crossbow","stale/invalid ray, basis or origin"); return;
    }
    // This is BEFORE SpawnActor. The same stack float3 feeds the spawn rotation
    // and the native initializer's direction*speed. No projectile pointer held.
    memcpy(direction,solve.direction,sizeof(solve.direction));
    const LONG shot=InterlockedIncrement(&g_fireAimChanged);
    const float c=dvr::fireaim::dot(original,solve.direction);
    Log("fireaim #%ld crossbow: native pre-spawn direction replaced, gen=%u age=%llu ms "
        "S=(%.2f %.2f %.2f) T=(%.2f %.2f %.2f) d=(%.5f %.5f %.5f) "
        "change=%.2f deg distance=%.2f m; seen=%ld refused=%ld. "
        "This records the launch input, not a measured impact.",
        shot,aim.ray.gen,(unsigned long long)(GetTickCount64()-aim.ray.sampleMs),
        spawn[0],spawn[1],spawn[2],solve.target[0],solve.target[1],solve.target[2],
        direction[0],direction[1],direction[2],
        acosf(c<-1?-1:c>1?1:c)*57.2957795f,aim.distanceM,g_fireAimSeen,g_fireAimRefused);
}

// VR-82. The pistol's seam differs in one way that matters: its spawn POSITION
// is built from the aim direction and a tweaked standoff, so the position and
// the direction move together or the bullet starts off the line it flies along.
static void PistolAimApply(uint8_t* context, uint8_t* frame) {
    if (!FireAimEnabled()) return;
    InterlockedIncrement(&g_fireAimSeen);
    uint8_t* source=nullptr;
    // -0x60 is the lowest local read here and -0x18 the highest, so 0x4C spans
    // every one of them: the aim direction, the spawn, the direction, the tweaks.
    if (!FireAimGate("pistol",context,kPistolContextVtable,frame,
                     kPistolAimDirLocal,0x4C,kPistolSourceLocal,&source)) return;
    const auto aim=dvr::aim::fire_frame();
    float camera[3];
    if (!dvr::camera::render_pos_world(camera)) {
        FireAimRefuse("pistol","no world camera position"); return;
    }
    float* spawn=(float*)(frame+kPistolSpawnLocal);
    float* direction=(float*)(frame+kPistolDirectionLocal);
    // The direction the STANDOFF was built from, which a later native call may
    // since have rewritten in the rotation local. Reconstruction needs this one.
    const float* aimDir=(const float*)(frame+kPistolAimDirLocal);
    uint8_t* tweaks=*(uint8_t**)(frame+kPistolTweaksLocal);
    if (!tweaks || !RangeReadable(tweaks+kPistolSpawnDistOff,4)) {
        FireAimRefuse("pistol","bullet spawn distance unreadable"); return;
    }
    const float standoff=*(const float*)(tweaks+kPistolSpawnDistOff);
    const float original[3]={direction[0],direction[1],direction[2]};
    const float n=dvr::fireaim::dot(original,original);
    if (!std::isfinite(n) || n<0.5f || n>1.5f) {
        FireAimRefuse("pistol","native direction is not unit"); return;
    }
    dvr::fireaim::StandoffSolution solve;
    const char* why="unstated";
    float reach=-1;
    if (!dvr::fireaim::solve_standoff(aim,GetTickCount64(),g_viewYawRad,g_viewPitchRad,
                                     camera,g_posScaleUU,spawn,aimDir,standoff,solve,&why,&reach)) {
        InterlockedIncrement(&g_fireAimRefused);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
            "fireaim: original aim retained (pistol): %s (standoff=%.1f uu reach=%.1f uu "
            "[-1 = the ray never resolved] distance=%.2f m gen=%u seen=%ld refused=%ld)",
            why,standoff,reach,aim.distanceM,aim.ray.gen,g_fireAimSeen,g_fireAimRefused);
        return;
    }
    // BEFORE the vector->rotator call and BEFORE SpawnActor. The direction local
    // feeds both the spawn rotation and the +0x3A4 initializer; the position
    // local feeds SpawnActor. No projectile pointer is held.
    memcpy(direction,solve.ray.direction,sizeof(solve.ray.direction));
    memcpy(spawn,solve.spawn,sizeof(solve.spawn));
    const LONG shot=InterlockedIncrement(&g_fireAimChanged);
    const float c=dvr::fireaim::dot(original,solve.ray.direction);
    Log("fireaim #%ld pistol: native pre-spawn direction AND standoff position replaced, "
        "gen=%u age=%llu ms standoff=%.1f uu O=(%.2f %.2f %.2f) S=(%.2f %.2f %.2f) "
        "T=(%.2f %.2f %.2f) d=(%.5f %.5f %.5f) change=%.2f deg distance=%.2f m; "
        "seen=%ld refused=%ld. The standoff is read from the tweaks object, not "
        "assumed; a value that is not the shipped 150 means the field moved. "
        "This records the launch input, not a measured impact.",
        shot,aim.ray.gen,(unsigned long long)(GetTickCount64()-aim.ray.sampleMs),
        standoff,solve.origin[0],solve.origin[1],solve.origin[2],
        spawn[0],spawn[1],spawn[2],
        solve.ray.target[0],solve.ray.target[1],solve.ray.target[2],
        direction[0],direction[1],direction[2],
        acosf(c<-1?-1:c>1?1:c)*57.2957795f,aim.distanceM,g_fireAimSeen,g_fireAimRefused);
}

static void __cdecl FireAimHandler(uint8_t* context, uint8_t* frame) {
    // Access faults fail back to native behavior. All validation precedes the
    // single stack-local write; no shared game state needs restoring.
    __try { FireAimApply(context,frame); }
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        FireAimRefuse("crossbow","object disappeared during validation");
    }
}
static void __cdecl PistolAimHandler(uint8_t* context, uint8_t* frame) {
    __try { PistolAimApply(context,frame); }
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        FireAimRefuse("pistol","object disappeared during validation");
    }
}
#include "game/dishonored/fire_aim_stub.h"

static bool FireAimInstall() {
    bool ok=true;
    if (!g_fireAimDet.on) {
        if (!RangeReadable((void*)kCrossbowInitCall,sizeof(kCrossbowInitCallBytes)) ||
            memcmp((void*)kCrossbowInitCall,kCrossbowInitCallBytes,sizeof(kCrossbowInitCallBytes))) {
            Log("fireaim: crossbow initializer call contract mismatch, hook NOT installed");
            ok=false;
        } else if (!dvr::hooks::detour_install(g_fireAimDet,"fireaim/crossbow",kCrossbowSpawnAim,
                       kCrossbowSpawnAimBytes,sizeof(kCrossbowSpawnAimBytes),(void*)&FireAimThunk)) {
            ok=false;
        }
    }
    // Independent of the crossbow's: one weapon's contract moving must not take
    // the other's hook down with it.
    if (!g_pistolAimDet.on) {
        if (!RangeReadable((void*)kPistolInitCall,sizeof(kPistolInitCallBytes)) ||
            memcmp((void*)kPistolInitCall,kPistolInitCallBytes,sizeof(kPistolInitCallBytes))) {
            Log("fireaim: pistol initializer call contract mismatch at 0x%08X, hook NOT "
                "installed; pistol shots keep the engine's own aim",
                (unsigned)kPistolInitCall);
            ok=false;
        } else if (!dvr::hooks::detour_install(g_pistolAimDet,"fireaim/pistol",kPistolSpawnAim,
                       kPistolSpawnAimBytes,sizeof(kPistolSpawnAimBytes),(void*)&PistolAimThunk)) {
            ok=false;
        }
    }
    return ok;
}
