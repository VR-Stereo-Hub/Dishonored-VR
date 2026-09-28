// VR-229: fixed-size history across method and actual XR tail, diagnostic builds only.
#include "core/gfx/flicker_diagnostic.h"
#ifdef DVR_FLICKER_DIAGNOSTICS
#define DVR_CAT ::dvr::log::Cat::present
#include "core/util/log.h"
#include "core/gfx/capture.h"
#include "core/gfx/frame_id.h"
#include "core/vr/openxr_runtime.h"
#include <windows.h>
#include <stdio.h>
namespace dvr::flicker {
namespace {
struct Frame { Method m; Runtime r; dvr::vr::PairProbe p; uint32_t events = 0; };
Frame history[64];
Method pending;
CameraUploads uploads;
Window window;
uint32_t count = 0, lastPrinted = 0, totals[8] = {};
uint32_t printNext = 0, printEnd = 0;
bool draining = false;
double recorderMaxMs=0;
dvr::vr::PairProbe previous;
int previousEye = 0;
uint32_t prevReadTimeout = 0, prevWriteTimeout = 0;
void print(uint32_t id) {
    const auto& f=history[(id-1)%64];const auto& m=f.m;const auto& r=f.r;const auto& p=f.p;
    DVR_INFO("flicker/frame: id%u P%u at%.3f ev%x methodP%u ring%ld/%ld frontD%u/%+d popD%u/%+d rec%u age%.3f "
        "c5s%u ok%d=(%.3f,%.3f,%.3f) basis%d=(%.4f,%.4f,%.4f) written%d=(%.3f,%.3f,%.3f) "
        "ipd%.3f step%.3f/%.3f inv%+d act%x expire%d owed%+d removed%d=[%u,%u,%u,%u,%u,%u] final%+d "
        "methodMs%.3f out%d fresh%d grab%u delivery%u/%+d slot%d rec%u pose%d/%d/%d pair%u eye%+d gen%u writer%d age%.3f q=(%.5f,%.5f,%.5f,%.5f)",
        id,r.present,m.ms,f.events,m.present,m.tail,m.head,m.frontDraw,m.frontEye,m.draw,m.ringEye,m.rec,
        m.pushMs>0?m.ms-m.pushMs:-1.,m.c5serial,m.c5ok,m.c5[0],m.c5[1],m.c5[2],m.basisok,m.right[0],m.right[1],m.right[2],
        m.writtenok,m.written[0],m.written[1],m.written[2],m.ipd,m.along,m.other,m.inv,m.action,m.expire,m.owed,
        m.removedN,m.removed[0],m.removed[1],m.removed[2],m.removed[3],m.removed[4],m.removed[5],m.eye,
        m.methodMs,m.out,m.fresh,m.grab,m.deliveredSerial,m.delivered,m.slot,m.deliveredRec,m.poseOk,m.trackOk,m.camOk,m.pair,m.poseEye,m.gen,m.writer,
        m.poseAgeMs,m.poseQ[0],m.poseQ[1],m.poseQ[2],m.poseQ[3]);
    DVR_INFO("flicker/xr: id%u P%u outcome%d open%d render%d projection%d eye%+d target%d index%u "
        "attempts%d/%d/%d copy%d results%d/%d/%d end%d layers%u new%d stereo%d content%u/%u gen%u/%u "
        "releaseContent%u/%u atP%u/%u index%u/%u qL=(%.5f,%.5f,%.5f,%.5f) qR=(%.5f,%.5f,%.5f,%.5f) fov%.3f xrMs%.3f "
        "ages%u/%u stale%u/%u captures%u/%u aborts%u/%u/%u eaten%u submits%u ends%u phaseUs%lld periodNs%lld",
        id,r.present,r.outcome,r.frameOpen,r.shouldRender,r.projection,r.eye,r.target,r.index,
        r.acquired,r.waited,r.released,r.copied,r.acq,r.wait,r.release,r.end,r.layers,r.newLayer,r.stereo,
        r.serial[0],r.serial[1],r.gen[0],r.gen[1],r.releasedSerial[0],r.releasedSerial[1],r.releasedPresent[0],r.releasedPresent[1],r.releasedIndex[0],r.releasedIndex[1],r.q[0][0],r.q[0][1],r.q[0][2],r.q[0][3],
        r.q[1][0],r.q[1][1],r.q[1][2],r.q[1][3],r.fov,r.durationMs,p.agePresL,p.agePresR,
        p.stalePresL,p.stalePresR,p.cap[0],p.cap[1],p.abortLeft,p.abortExpired,p.abortUntagged,p.eatenNoFrame,
        p.stereoSubmits,p.endFrames,(long long)p.phaseLastUs,(long long)p.displayPeriodNs);
    DVR_INFO("flicker/pose: id%u P%u deliveredRec%u valid%d/%d/%d reuse%d sourceXR=(%.5f,%.5f,%.5f) "
        "sourceUE=(%.3f,%.3f,%.3f) yawPitchRoll=(%.3f,%.3f,%.3f) submittedXR L=(%.5f,%.5f,%.5f) R=(%.5f,%.5f,%.5f); "
        "submitted values valid only for projection end; UE and XR coordinates are not directly comparable",
        id,r.present,m.deliveredRec,m.poseOk,m.trackOk,m.camOk,m.secondPassReuse,
        m.trackPos[0],m.trackPos[1],m.trackPos[2],m.camPos[0],m.camPos[1],m.camPos[2],
        m.camAngles[0],m.camAngles[1],m.camAngles[2],r.pos[0][0],r.pos[0][1],r.pos[0][2],r.pos[1][0],r.pos[1][1],r.pos[1][2]);
    char cameras[700]={};size_t at=0;
    for(unsigned i=0;i<m.cameras.used;++i) {
        const auto& v=m.cameras.values[i];
        const int n=_snprintf(cameras+at,sizeof(cameras)-at," %u*(%.3f,%.3f,%.3f)@c%u/%u",
            v.votes,v.xyz[0],v.xyz[1],v.xyz[2],v.start,v.count);
        if(n<0 || (size_t)n>=sizeof(cameras)-at) break;at+=(size_t)n;
    }
    cameras[sizeof(cameras)-1]=0;
    DVR_INFO("flicker/cameras: id%u P%u uploads%u distinctStored%u overflowUploads%u%s; "
        "all c5 uploads before capture, not a world-pass classifier; compare final c5 with votes and raw written position",
        id,r.present,m.cameras.uploads,m.cameras.used,m.cameras.overflow,cameras);
    lastPrinted=id;
}
}
void camera_upload(const float* xyz,unsigned start,unsigned count) { uploads.add(xyz,start,count); }
void method(const Method& m) { pending=m;pending.cameras=uploads;uploads=CameraUploads{}; }
void finish(const Runtime& r) {
    struct Cost {
        LARGE_INTEGER start={},freq={};
        Cost() {QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&start);}
        ~Cost(){LARGE_INTEGER n;QueryPerformanceCounter(&n);double ms=freq.QuadPart?1000.*(n.QuadPart-start.QuadPart)/freq.QuadPart:0.;if(ms>recorderMaxMs)recorderMaxMs=ms;}
    } cost;
    Frame f={};f.r=r;
    if(pending.present==r.present) f.m=pending;
    pending=Method{};uploads=CameraUploads{};
    dvr::vr::pair_probe_peek(&f.p);
    const auto& p=f.p; const auto& m=f.m;
    const uint32_t rt=dvr::capture::read_timeouts(),wt=dvr::capture::fence_timeouts();
    // These are observations, not causes. A duplicate tag alone never proves a stale submit.
    if(p.stalePresL!=previous.stalePresL || p.stalePresR!=previous.stalePresR) f.events|=1;
    if(m.expire) f.events|=2;
    if(m.fresh && m.delivered && m.delivered==previousEye) f.events|=4;
    if(m.present && (m.out!=0 || !m.fresh)) f.events|=8;
    if((r.acquired && r.acq<0)||(r.waited && r.wait<0)||(r.released && r.release<0)||
       (r.outcome==3 && r.end<0)||rt!=prevReadTimeout||wt!=prevWriteTimeout) f.events|=16;
    if(p.eatenNoFrame!=previous.eatenNoFrame) f.events|=32;
    if(m.action & (2|4|8|16|32)) f.events|=64; // arbitration overrides/invention/refusal (see schema)
    if(r.outcome==3 && (!r.layers || !r.newLayer)) f.events|=128;
    for(unsigned i=0;i<8;++i) if(f.events&(1u<<i)) ++totals[i];
    previous=p;prevReadTimeout=rt;prevWriteTimeout=wt;
    if(m.fresh && m.delivered) previousEye=m.delivered;
    const uint32_t id=++count;history[(id-1)%64]=f;
    const double now=(double)GetTickCount64();
    if(!draining && window.open(now,f.events!=0)) {
        dvr::frameid::diagnostic_burst();
        DVR_INFO("flicker/window: id%u event%x totals(stale,expiry,duplicate,hold-or-missing,api-or-fence,eaten,arbitration,fallback)="
            "%u/%u/%u/%u/%u/%u/%u/%u capture=%s wait%d fenceTimeouts%u/%u; 12 before + 16 after, max one window/5s; "
            "recorderMaxMs%.3f history output one frame/present; healthy heartbeat/10s; counts cover suppressed frames too; ages are last submit, not this copy; pose is delivered record",
            id,f.events,totals[0],totals[1],totals[2],totals[3],totals[4],totals[5],totals[6],totals[7],
            dvr::capture::mode_name(),dvr::capture::shared_wait(),wt,rt,recorderMaxMs);
        // Preserve the same 12-before/16-after records, but format only one
        // per Present. The oldest pending record stays at most 12 frames old
        // in the 64-frame history. Do not reopen while output is pending, even
        // if a very low frame rate takes longer than the five-second interval.
        printNext = id>12 ? id-12 : 1;
        if(printNext<=lastPrinted) printNext=lastPrinted+1;
        printEnd=id+15;draining=true;
    }
    window.take();
    if(draining && printNext<=id) {
        print(printNext++);
        if(printNext>printEnd) draining=false;
    }
}
}
#endif
