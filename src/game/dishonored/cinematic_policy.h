// VR-70: drawing cadence must never redefine the physical gaze reference.
#pragma once
namespace dvr::cine {
enum class Action { Reset, Hold, Track };
struct Conditions {
    bool enabled, menu, ownerChanged;
    bool ownershipKnown, animationOwns, fullyAuthored;
    bool sceneDraw, runtimeReady, poseReady;
};
inline Action action(const Conditions& c) {
    if (!c.enabled || c.menu || c.ownerChanged) return Action::Reset;
    if (!c.ownershipKnown) return Action::Hold;
    if (!c.animationOwns) return Action::Reset;
    if (!c.fullyAuthored || !c.sceneDraw || !c.runtimeReady || !c.poseReady) return Action::Hold;
    return Action::Track;
}
}
