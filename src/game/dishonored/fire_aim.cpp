// Included by the Dishonored unity TU. Only modifies a native firing stack local.
#include "game/dishonored/fire_aim_math.h"
#include <atomic>

static std::atomic<bool> g_fireAimEnabled{false};
static dvr::hooks::Detour g_fireAimDet;
static uintptr_t g_fireAimResume = kCrossbowSpawnAim + sizeof(kCrossbowSpawnAimBytes);
static volatile LONG g_fireAimSeen=0, g_fireAimChanged=0, g_fireAimRefused=0;

static bool FireAimEnabled() { return g_fireAimEnabled.load(); }
static void FireAimSet(bool on, const char* source) {
    g_fireAimEnabled.store(on);
    dvr::aim::request_fire_ray(on);
    Log("fireaim: %s (%s), native hook %s; player crossbow firing context only. "
        "Camera/body rotations and the HUD cache are not written.",
        on ? "ON" : "off", source, g_fireAimDet.on ? "ready" : "not installed yet");
}
static void FireAimRefuse(const char* why) {
    InterlockedIncrement(&g_fireAimRefused);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 2000,
        "fireaim: original aim retained: %s (seen=%ld changed=%ld refused=%ld)",
        why,g_fireAimSeen,g_fireAimChanged,g_fireAimRefused);
}

static void FireAimApply(uint8_t* context, uint8_t* frame) {
    if (!FireAimEnabled()) return;
    InterlockedIncrement(&g_fireAimSeen);
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow ||
        !g_scriptHeadOK || MaimNowMs()-g_scriptHeadMs>100.0 || g_maimEnabled) {
        FireAimRefuse("gameplay/head sample unavailable or legacy MotionAim enabled"); return;
    }
    if (!context || !RangeReadable(context,sizeof(void*)) ||
        *(uintptr_t*)context!=kCrossbowContextVtable ||
        !frame || !RangeReadable(frame+kCrossbowSpawnLocal,0xB8)) {
        FireAimRefuse("native context/frame contract mismatch"); return;
    }
    // Source is the pawn already resolved by the native firing routine. Verify
    // possession independently; the cached ProcessEvent latches are not immortal.
    uint8_t* source=*(uint8_t**)(frame+kCrossbowSourceLocal);
    uint8_t* ctrl=g_peCtrl;
    if (!source || source!=g_pePawn || !LooksLikeObj(source) || !LooksLikeObj(ctrl)) {
        FireAimRefuse("not the current player pawn"); return;
    }
    const char* cls=ObjClassName(ctrl);
    if (!cls || !strstr(cls,"PlayerController")) { FireAimRefuse("controller class mismatch"); return; }
    const uint32_t pawnOff=RflOffsetOf("Controller","Pawn");
    const uint32_t controllerOff=RflOffsetOf("Pawn","Controller");
    if (!pawnOff || !controllerOff || !RangeReadable(ctrl+pawnOff,4) ||
        !RangeReadable(source+controllerOff,4) || *(uint8_t**)(ctrl+pawnOff)!=source ||
        *(uint8_t**)(source+controllerOff)!=ctrl) {
        FireAimRefuse("possession could not be verified"); return;
    }
    const auto aim=dvr::aim::fire_frame();
    float camera[3];
    if (!dvr::camera::render_pos_world(camera)) { FireAimRefuse("no world camera position"); return; }
    const float* spawn=(const float*)(frame+kCrossbowSpawnLocal);
    float* direction=(float*)(frame+kCrossbowDirectionLocal);
    const float original[3]={direction[0],direction[1],direction[2]};
    const float n=dvr::fireaim::dot(original,original);
    if (!std::isfinite(n) || n<0.5f || n>1.5f) { FireAimRefuse("native direction is not unit"); return; }
    dvr::fireaim::Solution solve;
    if (!dvr::fireaim::solve(aim,GetTickCount64(),g_viewYawRad,g_viewPitchRad,
                            camera,g_posScaleUU,spawn,solve)) {
        FireAimRefuse("stale/invalid ray, basis or origin"); return;
    }
    // This is BEFORE SpawnActor. The same stack float3 feeds the spawn rotation
    // and the native initializer's direction*speed. No projectile pointer held.
    memcpy(direction,solve.direction,sizeof(solve.direction));
    const LONG shot=InterlockedIncrement(&g_fireAimChanged);
    const float c=dvr::fireaim::dot(original,solve.direction);
    Log("fireaim #%ld: native pre-spawn direction replaced, gen=%u age=%llu ms "
        "S=(%.2f %.2f %.2f) T=(%.2f %.2f %.2f) d=(%.5f %.5f %.5f) "
        "change=%.2f deg distance=%.2f m; seen=%ld refused=%ld. "
        "This records the launch input, not a measured impact.",
        shot,aim.ray.gen,(unsigned long long)(GetTickCount64()-aim.ray.sampleMs),
        spawn[0],spawn[1],spawn[2],solve.target[0],solve.target[1],solve.target[2],
        direction[0],direction[1],direction[2],
        acosf(c<-1?-1:c>1?1:c)*57.2957795f,aim.distanceM,g_fireAimSeen,g_fireAimRefused);
}

static void __cdecl FireAimHandler(uint8_t* context, uint8_t* frame) {
    // Access faults fail back to native behavior. All validation precedes the
    // single stack-local write; no shared game state needs restoring.
    __try { FireAimApply(context,frame); }
    __except(GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        FireAimRefuse("object disappeared during validation");
    }
}
#include "game/dishonored/fire_aim_stub.h"

static bool FireAimInstall() {
    if (g_fireAimDet.on) return true;
    if (!RangeReadable((void*)kCrossbowInitCall,sizeof(kCrossbowInitCallBytes)) ||
        memcmp((void*)kCrossbowInitCall,kCrossbowInitCallBytes,sizeof(kCrossbowInitCallBytes))) {
        Log("fireaim: initializer call contract mismatch, hook NOT installed"); return false;
    }
    return dvr::hooks::detour_install(g_fireAimDet,"fireaim",kCrossbowSpawnAim,
        kCrossbowSpawnAimBytes,sizeof(kCrossbowSpawnAimBytes),(void*)&FireAimThunk);
}
