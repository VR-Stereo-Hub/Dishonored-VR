// VR-126: pure wheel geometry/input, shared by production and host tests.
#pragma once
#include <cmath>
namespace dvr::weapon_dial {
inline void radial(float x, float y, float dead, float& ox, float& oy) {
    ox = oy = 0;
    const float r = std::sqrt(x*x + y*y);
    if (!std::isfinite(r) || r <= dead || dead >= 1) return;
    const float m = r >= 1 ? 1 : (r-dead)/(1-dead);
    ox = x/r*m; oy = y/r*m;
}
inline bool step_menu(bool menu, bool wheel) { return menu && !wheel; }
struct State {
    bool held = false, valid = false, lost = false;
    float center[3] = {};
    void reset() { held = valid = lost = false; }
    bool update(bool down, bool tracked, const float hand[3], const float head[3],
                float radius, float deadM, float& x, float& y) {
        x = y = 0;
        if (!down) { reset(); return false; }
        for (int i=0;i<3;++i) tracked = tracked && std::isfinite(hand[i]) && std::isfinite(head[i]);
        if (!held) { held = true; lost = false; valid = tracked;
            if (valid) for (int i=0;i<3;++i) center[i]=hand[i]; }
        if (!tracked) { valid = false; lost = true; }
        // Never re-seed mid-gesture after a tracking loss or invalid opening.
        if (!valid || lost || radius <= deadM || radius <= 0) return false;
        float n[3] = {head[0]-center[0],head[1]-center[1],head[2]-center[2]};
        const float len=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (len < .05f) return false;
        for (float& v:n) v/=len;
        const float rl=std::sqrt(n[2]*n[2]+n[0]*n[0]);
        if (rl < .2f) return false;
        const float right[3]={n[2]/rl,0,-n[0]/rl};
        const float up[3]={n[1]*right[2], n[2]*right[0]-n[0]*right[2], -n[1]*right[0]};
        float dx=0,dy=0;
        for(int i=0;i<3;++i) { dx+=(hand[i]-center[i])*right[i]; dy+=(hand[i]-center[i])*up[i]; }
        radial(dx/radius,dy/radius,deadM/radius,x,y);
        return true;
    }
};
}
