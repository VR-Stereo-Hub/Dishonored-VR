// VR-126: pure wheel geometry/input, shared by production and host tests.
#pragma once
#include <cmath>
#include "core/util/xr_math.h"
#include "core/vr/hud_anchor.h"
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
    dvr::hudanchor::OpeningOrientation opening;
    void reset() { held = valid = lost = false; opening.reset(); }
    bool update(bool down, bool tracked, const float hand[3], const float head[3],
                float radius, float deadM, float& x, float& y, const float* cameraQ = nullptr, bool directionOnly = false, bool entryTilt = false, bool entryYaw = false) {
        x = y = 0;
        if (!down) { reset(); return false; }
        for (int i=0;i<3;++i) tracked = tracked && std::isfinite(hand[i]) && std::isfinite(head[i]);
        if (!held) { held = true; lost = false; valid = tracked;
            if (valid) { for (int i=0;i<3;++i) center[i]=hand[i];
                if(cameraQ) {
                    // Face the opening eye position, independent of head rotation.
                    const float dx=head[0]-center[0],dz=head[2]-center[2];
                    if(dx*dx+dz*dz<.0001f) valid=false;
                    else {
                        const float yaw=std::atan2(dx,dz);
                        const float q[4]={0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
                        if(!entryTilt && !entryYaw) valid=opening.capture(q);
                        else {
                            dvr::hudanchor::OpeningOrientation camera,flat;
                            valid=camera.capture(cameraQ) && flat.capture_upright(cameraQ);
                            if(valid) {
                                const float* base=entryYaw?flat.q:q;
                                if(entryTilt) {
                                    const float inverse[4]={-flat.q[0],-flat.q[1],-flat.q[2],flat.q[3]};
                                    float tilt[4],result[4];
                                    dvr::xrmath::quat_mul(inverse,camera.q,tilt);
                                    dvr::xrmath::quat_mul(base,tilt,result);
                                    valid=opening.capture(result);
                                } else valid=opening.capture(base);
                            }
                        }
                    }
                } } }
        if (!tracked) { valid = false; lost = true; }
        // Never re-seed mid-gesture after a tracking loss or invalid opening.
        if (!valid || lost || radius <= deadM || radius <= 0) return false;
        float n[3] = {head[0]-center[0],head[1]-center[1],head[2]-center[2]};
        const float len=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        if (len < .05f) return false;
        for (float& v:n) v/=len;
        const float rl=std::sqrt(n[2]*n[2]+n[0]*n[0]);
        if (rl < .2f && !cameraQ) return false;
        float right[3]={rl>.0001f ? n[2]/rl : 1,0,rl>.0001f ? -n[0]/rl : 0};
        float up[3]={n[1]*right[2], n[2]*right[0]-n[0]*right[2], -n[1]*right[0]};
        if (opening.valid) {
            cameraQ=opening.q;
            const float rx[3]={1,0,0},uy[3]={0,1,0};
            dvr::xrmath::quat_rotate(cameraQ[0],cameraQ[1],cameraQ[2],cameraQ[3],rx,right);
            dvr::xrmath::quat_rotate(cameraQ[0],cameraQ[1],cameraQ[2],cameraQ[3],uy,up);
        }
        float dx=0,dy=0;
        for(int i=0;i<3;++i) { dx+=(hand[i]-center[i])*right[i]; dy+=(hand[i]-center[i])*up[i]; }
        if (directionOnly) {
            const float distance=std::sqrt(dx*dx+dy*dy);
            if (distance>deadM) { x=dx/distance; y=dy/distance; }
        } else radial(dx/radius,dy/radius,deadM/radius,x,y);
        return true;
    }
};
}
