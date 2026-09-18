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
    // VR-117: a UI owner that RIDES the HUD window does not own presentation;
    // one that does not ride still forces the mono quad, as before.
    if (UiSurfaceOwnsPresentation()) return false;
    const bool strict=DvrGameplayVerdict();
    const auto state=dvr::anim::snapshot(); // current live-object checked FSM, 150 ms expiry
    const bool pawn=CylTruthLive();
    // VR-135: a validated controlled possession. The capsule term refuses the
    // possessed pawn by design; this is its own read-only proof, not a relaxed one.
    const bool possessed=!pawn && PossessionStereoLive();
    const bool view=DvrScriptViewLive();
    const double nowMs=MaimNowMs();
    const bool note=g_uiNoteOpen && nowMs-g_uiPollMs<500.0;
    const bool menu=g_menuOpen || g_inMenu || g_mainMenu || note || g_gameExiting;
    // The render thread publishes actual scene-camera uploads. Both callers of
    // this verdict share one protected movement clock; reading a serial twice
    // must not make its second reader conclude the scene stopped drawing.
    static SRWLOCK lock=SRWLOCK_INIT;
    static uint32_t serial=0;
    static unsigned long long moved=0, movedRaw=0;
    static int last=-1, lastDialog=-2, lastStandIn=-1, lastPossessed=-1;
    static char lastState[96]={};
    static dvr::ui_ride::RideGrace grace;
    static double pendingSince=0;
    const auto now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    const uint32_t current=(uint32_t)dvr::camera::render_pos_serial();
    if (current!=serial) { serial=current; moved=now; movedRaw=now; }
    if (menu || !pawn || !state.valid) moved=0;   // possession reads the RAW clock below
    const bool sceneFresh=moved && now>=moved && now-moved<=150;
    // VR-117: the ride stand-in. While an in-game screen rides the HUD window
    // (or for 1500 ms after it closes: the resume gap, and for 300 ms after a
    // menu flag rises before the owner read has published: the open gap) the
    // ordinary terms are silent by construction - the pause stops the view
    // dispatches and the FSM snapshot expires - so eligibility is a live pawn
    // plus proof the scene is still drawing: the RAW camera-upload clock (never
    // killed by the menu term; the paused world keeps uploading, measured) or
    // a tagged projection present within the last few presents.
    const bool rides=UiSurfaceRidesHud();
    const bool sceneFreshRaw=movedRaw && now>=movedRaw && now-movedRaw<=150;
    const bool gateFresh=dvr::hud::gate_age_ms()<=250;
    const bool inGrace=grace.update(rides,UiSurfaceBlocks(),(double)now);
    const bool ridePossible=dvr::hudlayout::menu_in_window() && dvr::hudcap::enabled() &&
                            dvr::hudcap::redirect_healthy() && !g_mainMenu && UiSurfaceEnabled();
    // The open gap holds for its whole window once it has started: only the
    // owner read (blocked), the ride itself or the menu flag dropping end it.
    // A health blink must not (the first measured pause fell to the mono
    // screen exactly that way).
    if (rides || UiSurfaceBlocks() || !menu) pendingSince=0;
    else if (!pendingSince && ridePossible) pendingSince=(double)now;
    const bool pending=pendingSince>0 && (double)now-pendingSince<300.0;
    const int standIn=rides?1:inGrace?2:pending?3:0;
    const bool uiClear=UiSurfaceEnabled() && !UiSurfaceBlocks();
    const bool result=standIn ? dvr::ui_ride::ride_eligible(pawn || possessed,sceneFreshRaw,gateFresh)
                    : !StereoStateEnabled() ? strict
                    : possessed ? dvr::scene_state::possession_eligible(possessed,menu,view,sceneFreshRaw,uiClear)
                    : dvr::scene_state::eligible(strict,pawn,menu,view,state.valid,state.state[0],sceneFresh,uiClear);
    if ((int)result!=last || lastDialog!=state.dialogState || strcmp(lastState,state.state[0]) || standIn!=lastStandIn ||
        (int)possessed!=lastPossessed) {
        last=result; lastDialog=state.dialogState; lastStandIn=standIn; lastPossessed=possessed;
        strncpy_s(lastState,state.state[0],_TRUNCATE);
        static const char* const kStandIn[4]={"none","riding","resume grace","open pending"};
        Log("stereo/state: %s strict=%d pawn=%d possessed=%d menu=%d note=%d view=%d latch=%d "
            "valid=%d master=%s dialog=%d (0=listening 1=choosing -1=unknown) sceneFresh=%d c5age=%llu "
            "standIn=%s rawAge=%llu gateAge=%lu -> presentation only; input locks retained%s",
            result?"STEREO":"FALLBACK",strict,pawn,possessed,menu,note,
            view,g_cineNow,state.valid,state.state[0],state.dialogState,sceneFresh,moved?now-moved:~0ull,
            kStandIn[standIn],movedRaw?now-movedRaw:~0ull,dvr::hud::gate_age_ms(),
            standIn?" (a screen on the HUD window, the world on the projection)":"");
    }
    ReleaseSRWLockExclusive(&lock);
    return result;
}
