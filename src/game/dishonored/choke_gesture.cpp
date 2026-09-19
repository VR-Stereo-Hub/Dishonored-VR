// VR-145: the physical choke. Included by the unity build before pad_bridge.cpp,
// which ORs ChokeGestureTick() into RB (the game's block/choke button). Plan and
// levers: docs/dishonored/CHOKE_GESTURE.md.
#include "game/dishonored/choke_gesture.h"
#include <imgui.h>

static dvr::choke::Config g_chokeCfg;
static dvr::choke::State  g_chokeState;
static char g_chokeIni[MAX_PATH] = "";

static void ChokeConfigure(const char* ini) {
    _snprintf(g_chokeIni, sizeof(g_chokeIni), "%s", ini); g_chokeIni[sizeof(g_chokeIni) - 1] = 0;
    auto f = [&](const char* k, float d) { return IniFloat(ini, "Choke", k, d); };
    g_chokeCfg.enabled  = GetPrivateProfileIntA("Choke", "Gesture", 0, ini) != 0;
    g_chokeCfg.enterM   = f("EnterM", g_chokeCfg.enterM);
    g_chokeCfg.exitM    = f("ExitM", g_chokeCfg.exitM);
    g_chokeCfg.minSpeed = f("MinSpeed", g_chokeCfg.minSpeed);
    g_chokeCfg.windowMs = f("WindowMs", g_chokeCfg.windowMs);
    g_chokeCfg.leftM    = f("ShoulderLeftM", g_chokeCfg.leftM);
    g_chokeCfg.downM    = f("ShoulderDownM", g_chokeCfg.downM);
    g_chokeCfg.backM    = f("ShoulderBackM", g_chokeCfg.backM);
    if (g_chokeCfg.exitM < g_chokeCfg.enterM) g_chokeCfg.exitM = g_chokeCfg.enterM;
    Log("config: [Choke] Gesture=%d EnterM=%.2f ExitM=%.2f MinSpeed=%.2f m/s WindowMs=%.0f shoulder left %.2f down %.2f back %.2f "
        "(right hand to the left shoulder holds RB, the game's choke/block button)",
        g_chokeCfg.enabled, g_chokeCfg.enterM, g_chokeCfg.exitM, g_chokeCfg.minSpeed, g_chokeCfg.windowMs,
        g_chokeCfg.leftM, g_chokeCfg.downM, g_chokeCfg.backM);
}
static void ChokeSave(const char* who) {
    if (!g_chokeIni[0]) return;
    char v[32];
    WritePrivateProfileStringA("Choke", "Gesture", g_chokeCfg.enabled ? "1" : "0", g_chokeIni);
    auto w = [&](const char* k, float x) { _snprintf(v, sizeof(v), "%.3f", x); WritePrivateProfileStringA("Choke", k, v, g_chokeIni); };
    w("EnterM", g_chokeCfg.enterM); w("ExitM", g_chokeCfg.exitM); w("MinSpeed", g_chokeCfg.minSpeed);
    w("WindowMs", g_chokeCfg.windowMs); w("ShoulderLeftM", g_chokeCfg.leftM);
    w("ShoulderDownM", g_chokeCfg.downM); w("ShoulderBackM", g_chokeCfg.backM);
    Log("choke: settings saved (%s)", who);
}

// Called once per pad update. True = hold RB this update.
static bool ChokeGestureTick(bool wheelHeld) {
    float hp[3], hq[4];
    dvr::vr::HeadPose head{};
    const bool tracked = dvr::vr::input_get_hand_pose(1, false, hp, hq) && dvr::vr::peek_head_pose(head);
    const bool allowed = tracked && !wheelHeld && !g_menuOpen && !g_inMenu && !g_mainMenu &&
                         !UiSurfaceBlocks() && !g_ovlVisible;
    if (!tracked) { hp[0] = hp[1] = hp[2] = 0; }
    const float hpos[3] = {head.px, head.py, head.pz}, hquat[4] = {head.qx, head.qy, head.qz, head.qw};
    int edge = 0;
    const bool held = g_chokeState.update(g_chokeCfg, MaimNowMs(), hp, hpos, hquat, allowed, &edge);
    if (edge > 0)
        Log("choke: START - right hand %.2f m from the left shoulder (enter %.2f), peak speed within %.0f ms; RB held",
            g_chokeState.lastDist, g_chokeCfg.enterM, g_chokeCfg.windowMs);
    else if (edge < 0)
        Log("choke: RELEASE - %s (hand %.2f m from the shoulder, exit %.2f)",
            allowed ? "the hand left the shoulder" : "a menu, the wheel, the overlay or lost tracking",
            g_chokeState.lastDist, g_chokeCfg.exitM);
    else if (g_chokeCfg.enabled && tracked)
        DVR_LOG_EVERY_MS(::dvr::log::Cat::pad, ::dvr::log::Level::Debug, 2000,
            "choke: idle, hand %.2f m from the shoulder, speed %.2f m/s", g_chokeState.lastDist, g_chokeState.lastSpeed);
    return held;
}

static void ChokeDrawUi() {
    auto& c = g_chokeCfg; bool ch = false;
    ch |= ImGui::Checkbox("Physical choke: right hand to the left shoulder", &c.enabled);
    ImGui::TextDisabled("Move the right hand quickly to the left shoulder behind a target, and keep it there to hold the choke.");
    if (c.enabled) {
        ch |= ImGui::SliderFloat("Start within (m)", &c.enterM, 0.05f, 0.35f, "%.2f");
        ch |= ImGui::SliderFloat("Release beyond (m)", &c.exitM, 0.08f, 0.50f, "%.2f");
        ch |= ImGui::SliderFloat("Quick move speed (m/s)", &c.minSpeed, 0.2f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("Shoulder: left of head (m)", &c.leftM, 0.0f, 0.4f, "%.2f");
        ch |= ImGui::SliderFloat("Shoulder: below head (m)", &c.downM, 0.0f, 0.5f, "%.2f");
        ch |= ImGui::SliderFloat("Shoulder: behind head (m)", &c.backM, -0.2f, 0.3f, "%.2f");
        if (c.exitM < c.enterM) c.exitM = c.enterM;
        ImGui::TextDisabled("Now: hand %.2f m from the shoulder, %.2f m/s, choke %s",
            g_chokeState.lastDist, g_chokeState.lastSpeed, g_chokeState.held ? "HELD" : "off");
    }
    if (ch) ChokeSave("F10 Controls");
}
