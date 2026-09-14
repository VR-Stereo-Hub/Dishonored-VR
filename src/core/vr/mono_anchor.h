#pragma once
#include <cmath>
#include <cstdint>
namespace dvr::mono {
enum Context : unsigned { Other, MainMenu, Loading, Pause, Note, Journal, Wheel, Store, MissionStats, Cinematic, Count };
inline constexpr const char* names[Count]={"Other","MainMenu","Loading","Pause","Note","Journal","Wheel","Store","MissionStats","Cinematic"};
struct Pose { float x=0,y=0,z=0,qx=0,qy=0,qz=0,qw=1; };
// One upright LOCAL-space panel per contiguous mono interval. Head motion never
// updates a seeded pose; recenter and reference-space changes invalidate it.
struct Anchor {
    bool valid=false;
    Pose pose;
    void reset() { valid=false; }
    bool seed(const Pose& h,float distance) {
        const float n=h.qx*h.qx+h.qy*h.qy+h.qz*h.qz+h.qw*h.qw;
        if (!std::isfinite(n) || n<0.5f || n>1.5f || !std::isfinite(distance) || distance<=0 ||
            !std::isfinite(h.x) || !std::isfinite(h.y) || !std::isfinite(h.z)) return false;
        const float x=2*(h.qx*h.qz+h.qw*h.qy)/n;
        const float z=1-2*(h.qx*h.qx+h.qy*h.qy)/n;
        if (x*x+z*z<0.0001f) return false;
        const float yaw=std::atan2(x,z);
        pose={h.x-std::sin(yaw)*distance,h.y,h.z-std::cos(yaw)*distance,
              0,std::sin(yaw/2),0,std::cos(yaw/2)};
        valid=true; return true;
    }
};
// Loading-start can clear before Continue. Keep the lease through the movie's
// lifetime; save notifications alone never acquire it. Unknown cannot release.
struct LoadingLease {
    bool active=false;
    bool update(bool known,bool loading,bool started,bool movie) {
        if (known) active=loading || started || (active && movie);
        return active;
    }
};
}
