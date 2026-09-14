// VR-103: read-only presentation policy. Included after anim_state.cpp.
#include "game/dishonored/stereo_state_policy.h"
#include <atomic>
static std::atomic<bool> g_stereoState{false};
static bool StereoStateEnabled() { return g_stereoState.load(); }
static void StereoStateSet(bool on) {
    g_stereoState.store(on);
    Log("stereo/state: enabled=%d (live; gameplay input gates unchanged)",on?1:0);
}
static void StereoStateConfigure(const char* ini) {
    StereoStateSet(GetPrivateProfileIntA("Cine","StereoState",1,ini)!=0);
}
static bool DvrSceneVerdict() {
    if (UiSurfaceBlocks()) return false;
    const bool strict=DvrGameplayVerdict();
    if (!StereoStateEnabled()) return strict;
    const auto state=dvr::anim::snapshot(); // current live-object checked FSM, 150 ms expiry
    const bool pawn=CylTruthLive();
    const bool view=DvrScriptViewLive();
    const double nowMs=MaimNowMs();
    const bool note=g_uiNoteOpen && nowMs-g_uiPollMs<500.0;
    const bool menu=g_menuOpen || g_inMenu || g_mainMenu || note || g_gameExiting;
    // The render thread publishes actual scene-camera uploads. Both callers of
    // this verdict share one protected movement clock; reading a serial twice
    // must not make its second reader conclude the scene stopped drawing.
    static SRWLOCK lock=SRWLOCK_INIT;
    static uint32_t serial=0;
    static unsigned long long moved=0;
    static int last=-1, lastDialog=-2;
    static char lastState[96]={};
    const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    const uint32_t current=(uint32_t)dvr::camera::render_pos_serial();
    if (current!=serial) { serial=current; moved=now; }
    if (menu || !pawn || !state.valid) moved=0;
    const bool sceneFresh=moved && now>=moved && now-moved<=150;
    const bool result=dvr::scene_state::eligible(strict,pawn,menu,view,state.valid,state.state[0],sceneFresh,UiSurfaceEnabled() && !UiSurfaceBlocks());
    if ((int)result!=last || lastDialog!=state.dialogState || strcmp(lastState,state.state[0])) {
        last=result; lastDialog=state.dialogState;
        strncpy_s(lastState,state.state[0],_TRUNCATE);
        Log("stereo/state: %s strict=%d pawn=%d menu=%d note=%d view=%d latch=%d "
            "valid=%d master=%s dialog=%d (0=listening 1=choosing -1=unknown) sceneFresh=%d c5age=%llu "
            "-> presentation only; input locks retained",result?"STEREO":"FALLBACK",strict,pawn,menu,note,
            view,g_cineNow,state.valid,state.state[0],state.dialogState,sceneFresh,moved?now-moved:~0ull);
    }
    ReleaseSRWLockExclusive(&lock);
    return result;
}
