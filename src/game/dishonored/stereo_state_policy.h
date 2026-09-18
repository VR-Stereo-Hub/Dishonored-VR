#pragma once
#include <cstring>
namespace dvr::scene_state {
inline bool cinematic(const char* state) {
    return !std::strcmp(state,"StatePlayerMasterSoiree") ||
           !std::strcmp(state,"StatePlayerMasterInDialog") ||
           !std::strcmp(state,"StatePlayerMasterInScriptedChoice");
}
// VR-133: the door keyhole. The game seats the player at the hole, hands the
// camera to its look influence (a +-35/+-17.6 deg cone) and zooms; the mod
// treats it like a scripted camera: the head owns the view through the draw
// scope and the render FOV is held. Measured on the simulator, 2026-09-18.
inline bool keyhole(const char* state) {
    return !std::strcmp(state,"StatePlayerMasterHolePeeking");
}
// Presentation permission is separate from player input permission. No latch,
// timeout grace or previous state can override a menu or a missing live pawn.
inline bool eligible(bool strict, bool pawn, bool menu, bool viewLive,
                     bool valid, const char* state, bool sceneFresh, bool uiClear = false) {
    if (!pawn || menu) return false;
    if (strict) return true;
    if (!valid) return false;
    if (cinematic(state) || keyhole(state)) return viewLive || sceneFresh;
    // A verified closed UI plus current scene uploads supersedes the old
    // loading heuristic based on successful head writes. Never use this
    // fallback with the UI guard off, unknown, or blocked.
    return (viewLive || (uiClear && sceneFresh)) && !std::strcmp(state,"StatePlayerMasterWalk");
}
}
