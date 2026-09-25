// VR-229: test-build-only flight recorder. Present thread only; no engine writes.
#pragma once
#include <stdint.h>
namespace dvr::flicker {
// Bounded recurring windows, never a lifetime budget. Production code and host tests
// use this policy. Every event is counted even when its detailed window is suppressed.
struct Window {
    double next = 0, heartbeat = 0;
    unsigned remaining = 0;
    bool open(double now, bool event) {
        if (remaining || now < next || (!event && now < heartbeat)) return false;
        next = now + 5000; heartbeat = now + 10000; remaining = 16; return true;
    }
    bool take() { if (!remaining) return false; --remaining; return true; }
};
inline bool pixel_sample(uint32_t serial) { return ((serial - 1u) % 128u) < 8u; }
// Bounded exact-value census. Overflow is explicit, never silently called one view.
struct CameraUploads {
    struct Value { float xyz[3]={}; uint32_t votes=0; unsigned start=0, count=0; };
    Value values[6];
    uint32_t uploads=0, overflow=0; unsigned used=0;
    void add(const float* p,unsigned start,unsigned count) {
        ++uploads;
        for(unsigned i=0;i<used;++i)
            if(values[i].xyz[0]==p[0] && values[i].xyz[1]==p[1] && values[i].xyz[2]==p[2]) {
                ++values[i].votes;return;
            }
        if(used==6) { ++overflow;return; }
        auto& v=values[used++];for(int j=0;j<3;++j) v.xyz[j]=p[j];
        v.votes=1;v.start=start;v.count=count;
    }
};
#ifdef DVR_FLICKER_DIAGNOSTICS
struct Method {
    uint32_t present = 0, draw = 0, rec = 0, c5serial = 0;
    double ms = 0, pushMs = 0, methodMs = 0;
    int ringEye = 0, eye = 0, inv = 0, action = 0, expire = 0, owed = 0;
    int frontEye = 0; uint32_t frontDraw = 0;
    long head = 0, tail = 0;
    bool c5ok = false, basisok = false, writtenok = false;
    float c5[3] = {}, right[3] = {}, written[3] = {}, ipd = 0, along = 0, other = 0;
    uint32_t removed[6] = {}; int removedN = 0;
    int out = 0, delivered = 0, slot = -1;
    bool fresh = false;
    uint32_t grab = 0, deliveredSerial = 0, deliveredRec = 0;
    bool poseOk = false, trackOk = false, camOk = false;
    uint32_t pair = 0, gen = 0; int poseEye = 0, writer = 0;
    double poseAgeMs = 0;
    float poseQ[4] = {};
    float trackPos[3]={},camPos[3]={},camAngles[3]={};bool secondPassReuse=false;
    CameraUploads cameras;
};
struct Runtime {
    uint32_t present = 0;
    int outcome = 0; // 0 early return, 1 no XR frame, 2 pair held open, 3 end called
    int eye = 0, target = -1; uint32_t index = 0;
    bool frameOpen = false, shouldRender = false, projection = false;
    bool acquired = false, waited = false, released = false, copied = false;
    int acq = 0, wait = 0, release = 0, end = 0;
    uint32_t layers = 0; bool newLayer = false, stereo = false;
    uint32_t serial[2] = {}, gen[2] = {};
    uint32_t releasedSerial[2]={},releasedPresent[2]={},releasedIndex[2]={};
    float q[2][4] = {}, pos[2][3]={}, fov = 0;
    double durationMs = 0;
};
void camera_upload(const float* xyz,unsigned start,unsigned count);
void method(const Method& m);
void finish(const Runtime& r);
#endif
}
