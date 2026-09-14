#pragma once
#include <cmath>
namespace dvr::cine_fov {
inline bool valid(float fov) { return std::isfinite(fov) && fov>5 && fov<175; }
inline bool eligible(bool enabled,bool scene,bool menu,bool projection,bool state,float target) {
    return enabled && scene && !menu && projection && state && std::isfinite(target) && target>=40 && target<=160;
}
// A cinematic may end before the native zoom blend returns to the VR FOV.
// Keep the same owner's override until readback converges, with a bounded
// escape for a stalled sensor. The observed exit recovered in1125ms.
struct ExitBridge {
    bool primed=false;
    unsigned long long exiting=0;
    bool update(bool cinematic,bool tailAllowed,float sensor,float target,unsigned long long now) {
        if (cinematic) { primed=true; exiting=0; return true; }
        if (!primed || !tailAllowed || !valid(sensor) || std::fabs(sensor-target)<=0.5f) { *this={}; return false; }
        if (!exiting) exiting=now;
        if (now<exiting || now-exiting>=3000) { *this={}; return false; }
        return true;
    }
};
struct Scope {
    float* field=nullptr;
    float before=0, written=0;
    bool begin(float* p,float target,bool identity) {
        if (field || !identity || !p || !valid(*p) || !valid(target)) return false;
        field=p; before=*p; written=target; *p=target; return true;
    }
    bool end(bool identity) {
        if (!field) return false;
        bool restore=identity && *field==written;
        if (restore) *field=before;
        field=nullptr; return restore;
    }
};
}
