#pragma once
#include <cmath>
namespace dvr::cine_fov {
inline bool valid(float fov) { return std::isfinite(fov) && fov>5 && fov<175; }
inline bool eligible(bool enabled,bool scene,bool menu,bool projection,bool state,float target) {
    return enabled && scene && !menu && projection && state && std::isfinite(target) && target>=40 && target<=160;
}
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
