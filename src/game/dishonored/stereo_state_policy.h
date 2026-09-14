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
                     bool valid, const char* state, bool sceneFresh) {
    if (!pawn || menu) return false;
    if (strict) return true;
    if (!valid) return false;
    if (cinematic(state)) return viewLive || sceneFresh;
    return viewLive && !std::strcmp(state,"StatePlayerMasterWalk");
}
}
