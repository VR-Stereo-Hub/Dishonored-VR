#pragma once
#include <cstring>
namespace dvr::scene_state {
inline bool cinematic(const char* state) {
    return !std::strcmp(state,"StatePlayerMasterSoiree") ||
           !std::strcmp(state,"StatePlayerMasterInDialog") ||
           !std::strcmp(state,"StatePlayerMasterInScriptedChoice");
}
// Presentation permission is separate from player input permission. No latch,
// timeout grace or previous state can override a menu or a missing live pawn.
inline bool eligible(bool strict, bool pawn, bool menu, bool viewLive,
                     bool valid, const char* state, bool sceneFresh, bool uiClear = false) {
    if (!pawn || menu) return false;
    if (strict) return true;
    if (!valid) return false;
    if (cinematic(state)) return viewLive || sceneFresh;
    // A verified closed UI plus current scene uploads supersedes the old
    // loading heuristic based on successful head writes. Never use this
    // fallback with the UI guard off, unknown, or blocked.
    return (viewLive || (uiClear && sceneFresh)) && !std::strcmp(state,"StatePlayerMasterWalk");
}
// VR-135: a validated controlled possession stands in for the player pawn and
// its FSM (neither exists while possessing). Every other term still applies:
// no menu, a UI surface that does not block, the view pipeline dispatching and
// the scene camera uploading now. Nothing here can outlive the validation.
inline bool possession_eligible(bool possessed, bool menu, bool viewLive,
                                bool sceneFreshRaw, bool uiClear) {
    return possessed && !menu && uiClear && viewLive && sceneFreshRaw;
}
}
