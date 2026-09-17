// VR-70: drawing cadence must never redefine the physical gaze reference.
#pragma once
#include <cstring>
#include <cmath>
#include <cstdint>
namespace dvr::cine {
// Native special views own translation; physical gaze belongs to the final draw.
inline int special_camera(const char* state) {
    if(!state)return 0;
    if(!std::strcmp(state,"StatePlayerMasterLeaning"))return 1;
    if(!std::strcmp(state,"StatePlayerMasterHolePeeking"))return 2;
    return 0;
}
inline bool special_resume_delta(double reference,double current,int32_t& delta) {
    if(!std::isfinite(reference) || !std::isfinite(current))return false;
    delta=(int32_t)std::lround(std::remainder(current-reference,6.2831853071795864769)*(65536.0/6.2831853071795864769));
    return true;
}
enum class Action { Reset, Hold, Track };
struct Conditions {
    bool enabled, menu, ownerChanged;
    bool ownershipKnown, animationOwns, fullyAuthored;
    bool sceneDraw, runtimeReady, poseReady;
};
inline bool owns_rotation(bool scripted,float anim,float player,float look) {
    if (!(anim>=0 && anim<=1 && player>=0 && player<=1 && look>=0 && look<=1)) return false;
    return scripted || (anim>=0.999f && player<=0.001f && look<=0.001f);
}
inline Action action(const Conditions& c) {
    if (!c.enabled || c.menu || c.ownerChanged) return Action::Reset;
    if (!c.ownershipKnown) return Action::Hold;
    if (!c.animationOwns) return Action::Reset;
    if (!c.fullyAuthored || !c.sceneDraw || !c.runtimeReady || !c.poseReady) return Action::Hold;
    return Action::Track;
}
}
