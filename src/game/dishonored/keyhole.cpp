// VR-133: the door keyhole. After cinematic_fov.cpp in the unity TU.
//
// Holding Use on a door seats the player at the keyhole. The game then owns the
// camera through its look influence (influence anim/player/look = 0/0/1, a
// +-35 / +-17.6 deg cone measured on the simulator, 2026-09-18) and blends its
// own FOV sensor from 108 down to 75 deg over ~8 s. In the headset that read as
// the picture shrinking, the world gluing to the head at the cone's edge, and
// on exit a yaw offset equal to the head's travel during the peek (the game
// snaps the controller back to the pre-entry heading, the head is elsewhere).
//
// This module is the STATE side only: one predicate, the edge lines a tester's
// log needs, and the lever. The behaviour lives where the scripted-camera
// behaviour already lives: cinematic_fov.cpp holds the render FOV through the
// scoped CameraCache.POV.FOV write, cinematic_trace.cpp lets the head own the
// view through the draw scope and carries the yaw once on the way out.
//
// [Cine] KeyholeHold=0 ships OFF (every new lever does); `keyhole on|off|status`
// switches it live, the F10 Cine block has the checkbox. The edge lines print
// with the lever off too, so a run that never enabled it still documents what
// the game did.
#include "game/dishonored/stereo_state_policy.h"
namespace {
std::atomic<bool> g_keyholeHold{false};
bool g_khIn=false;                 // script lane: are we in HolePeeking right now
double g_khEnterMs=0, g_khLogMs=0;
float g_khEntryHeadYaw=0, g_khEntryCtrlYaw=0, g_khEntrySensor=0;
uint32_t g_khPeeks=0;
float KhCtrlYawDeg() {
    int32_t rot[3]={};
    uint8_t* pc=g_ctLayout && IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    return pc && g_ctActorRot && CtRead(pc,g_ctActorRot,rot,sizeof(rot)) ? rot[1]*360.0f/65536 : 0.0f;
}
float KhCacheFovDeg() {
    float fov=0;
    uint8_t* pc=g_ctLayout && IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam=CtObject(pc,g_ctPcCamera);
    return g_cfOffset && CtRead(cam,g_cfOffset,&fov,sizeof(fov)) ? fov : 0.0f;
}
}
static bool KeyholeHoldEnabled() { return g_keyholeHold.load(); }
// THE predicate. Freshness (150 ms) and the live-object/class cross-check are
// the snapshot's own, so a stale or unresolved pawn reads as "not peeking" and
// every consumer does nothing.
static bool KeyholeActive(const dvr::anim::Snapshot& s) {
    return g_keyholeHold.load() && s.valid && dvr::scene_state::keyhole(s.state[0]);
}
static void KeyholeHoldSet(bool on) {
    g_keyholeHold.store(on);
    Log("keyhole: hold %s (live; ON = the render FOV is held at the gameplay target and the head owns the view through the draw scope while peeking, yaw carried once on exit)",
        on ? "ON" : "off");
}
static void KeyholeConfigure(const char* ini) {
    g_keyholeHold.store(GetPrivateProfileIntA("Cine","KeyholeHold",0,ini)!=0);
    Log("keyhole: hold %s ([Cine] KeyholeHold)",g_keyholeHold.load() ? "ON" : "off");
}
// Script lane, after CineTraceTick(): the state edges and a 1 s line while in
// the hole. Reads only; the numbers are the ones the exit residue is judged by.
static void KeyholeTick() {
    const auto s=dvr::anim::snapshot();
    const bool in=s.valid && dvr::scene_state::keyhole(s.state[0]);
    const double now=MaimNowMs();
    uint8_t* pc=g_ctLayout && IsLiveObject(g_peCtrl) ? g_peCtrl : nullptr;
    uint8_t* cam=CtObject(pc,g_ctPcCamera);
    // The HUD's keyhole mask row follows the state, lever or not: hiding the
    // mask is the element table's decision ([Hud] Element.keyhole, ships off).
    dvr::hudlayout::set_keyhole_active(in);
    if (in && !g_khIn) {
        g_khIn=true; g_khEnterMs=now; g_khLogMs=now+1000; ++g_khPeeks;
        g_khEntryHeadYaw=g_hmdYaw*57.29578f; g_khEntryCtrlYaw=KhCtrlYawDeg();
        g_khEntrySensor=dvr::camera::rendered_fov_deg();
        Log("keyhole: ENTER #%u lever=%d headLook=%d influence(anim/player/look)=%.3f/%.3f/%.3f sensorFov=%.1f cacheFov=%.2f claim=%.1f headYaw=%.1f ctrlYaw=%.1f body=%d",
            g_khPeeks,(int)g_keyholeHold.load(),(int)CineHeadEnabled(),CtWeight(cam,0),CtWeight(cam,1),CtWeight(cam,2),
            g_khEntrySensor,KhCacheFovDeg(),CineFovClaim(),g_khEntryHeadYaw,g_khEntryCtrlYaw,s.bodyMode);
        return;
    }
    if (!in && g_khIn) {
        g_khIn=false;
        const float headYaw=g_hmdYaw*57.29578f;
        Log("keyhole: EXIT #%u -> master=%s after %.0f ms sensorFov=%.1f (entry %.1f) headYaw=%.1f (entry %.1f, travel %.1f) ctrlYaw=%.1f (entry %.1f) influence=%.3f/%.3f/%.3f; the travel is the yaw the walking view must carry",
            g_khPeeks,s.valid ? s.state[0] : "unknown",now-g_khEnterMs,dvr::camera::rendered_fov_deg(),g_khEntrySensor,
            headYaw,g_khEntryHeadYaw,(float)std::remainder((double)headYaw-g_khEntryHeadYaw,360.0),
            KhCtrlYawDeg(),g_khEntryCtrlYaw,CtWeight(cam,0),CtWeight(cam,1),CtWeight(cam,2));
        return;
    }
    if (in && now>=g_khLogMs) {
        g_khLogMs=now+1000;
        Log("keyhole: active %.0f ms lever=%d fov(sensor=%.1f cache=%.2f claim=%.1f) influence=%.3f/%.3f/%.3f headYaw=%.1f ctrlYaw=%.1f headOwnsInput=%d",
            now-g_khEnterMs,(int)g_keyholeHold.load(),dvr::camera::rendered_fov_deg(),KhCacheFovDeg(),CineFovClaim(),
            CtWeight(cam,0),CtWeight(cam,1),CtWeight(cam,2),g_hmdYaw*57.29578f,KhCtrlYawDeg(),(int)CineHeadOwnsInput());
    }
}
static bool KeyholeInState() { return g_khIn; }
