// core/vr/pose_record.cpp - the pose an image was actually rendered with (VR-65).
//
// A ring of immutable records. The argument for why it exists is in the header.
//
// LANES: opened on the GAME thread (one per viewport pass, from the same place
// the eye tag is pushed), read on the PRESENT thread at submission. There is no
// lock. The ring is written once per pass and read a few presents later, so a
// reader can only ever race a writer that has already moved several slots past
// it - and that case is not silently tolerated: every record carries its own id
// and a lookup that finds a different id in the slot reports EXPIRED rather than
// returning the wrong pose. That is the whole reliability argument, and it is
// why the id is written LAST when a record is filled.

#include "core/vr/pose_record.h"
#include "core/util/log.h"
#include "core/util/clock.h"

#include <windows.h>
#include <string.h>

#define DVR_CAT ::dvr::log::Cat::openxr

namespace dvr::pose {
namespace {

// 64 records is about a third of a second of both eyes at 90 Hz pairs, which is
// far longer than the one or two presents capture can hold a frame back. If a
// lookup ever finds a wrapped slot, the delay is much larger than the design
// assumes and the EXPIRED counter is the finding.
constexpr uint32_t kRing = 64;

Record        g_ring[kRing];
volatile LONG g_next = 1;        // the id to hand out next; 0 means "no record"
volatile LONG g_pair = 0;
float         g_injectYaw = 0.0f;

uint32_t g_opened = 0, g_hits = 0, g_expired = 0, g_missing = 0;

// The self test's window, in records opened. Defaults chosen so it lands a few
// seconds into a run, after the first load has settled: about 1200 records is
// roughly seven seconds of pairs at 90 Hz, and 180 records is about a second.
uint32_t g_stStart = 1200, g_stLen = 180;
float    g_stDeg = 4.0f;
int      g_stPhase = 0;      // 0 waiting, 1 armed, 2 done

// The most recent record a lookup answered, kept for the beat line so the join
// can be printed without the caller having to hold anything.
Record g_lastHit = {};
bool   g_haveLastHit = false;

} // namespace


uint32_t next_pair()
{
    return (uint32_t)InterlockedIncrement(&g_pair);
}


uint32_t open(int eye, uint32_t pairId, const Sample& s,
              const float camPos[3], bool camPosOk)
{
    // The self test, driven off the count of records rather than a clock, so it
    // cannot fire during a loading screen when nothing is being rendered.
    if (g_stPhase != 2 && g_stLen) {
        if (g_stPhase == 0 && g_opened >= g_stStart) {
            g_stPhase = 1;
            set_inject_yaw_deg(g_stDeg);
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pose/rec: SELF TEST STARTING - for the next %u record(s), about a "
                "second, every image is recorded with a head yaw %+.1f deg away "
                "from the one it was rendered with. The submission join MUST "
                "report that error and MUST return to zero when this ends. You "
                "may see a small wobble; it is this test and it stops by itself. "
                "If the join keeps reporting zero through this window, the join "
                "is not measuring what it claims and no zero from it counts.",
                g_stLen, g_stDeg);
        } else if (g_stPhase == 1 && g_opened >= g_stStart + g_stLen) {
            g_stPhase = 2;
            set_inject_yaw_deg(0.0f);
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pose/rec: SELF TEST OVER - records carry the real sample again. "
                "Read the posejoin lines either side of this: the error should "
                "have been about %+.1f deg during the window and near zero "
                "outside it.", g_stDeg);
        }
    }

    const uint32_t id = (uint32_t)InterlockedIncrement(&g_next) - 1;
    Record& r = g_ring[id & (kRing - 1)];

    // THE ID GOES LAST. A reader tests the id to decide whether the slot still
    // holds the record it asked for, so writing it first would let a reader
    // accept a half-written record as valid.
    r.id = 0;
    r.pairId = pairId;
    r.eye = eye;
    r.yawDeg   = s.yawDeg + g_injectYaw;   // the negative control, see the header
    r.pitchDeg = s.pitchDeg;
    r.rollDeg  = s.rollDeg;
    r.gen      = s.gen;
    r.locateMs = s.locateMs;
    r.headPosOk = s.headPosOk;
    if (s.headPosOk) memcpy(r.headPos, s.headPos, sizeof(r.headPos));
    else             memset(r.headPos, 0, sizeof(r.headPos));
    r.camPosOk = camPosOk && camPos != nullptr;
    if (r.camPosOk) memcpy(r.camPos, camPos, sizeof(r.camPos));
    else            memset(r.camPos, 0, sizeof(r.camPos));
    r.injectedYawDeg = g_injectYaw;
    r.openedMs = dvr::clock::now_ms();
    _ReadWriteBarrier();
    r.id = id;

    ++g_opened;
    return id;
}


const Record* get(uint32_t id)
{
    if (!id) { ++g_missing; return nullptr; }
    const Record* r = &g_ring[id & (kRing - 1)];
    if (r->id != id) {
        // Either the ring wrapped past it, or nothing ever filled this slot.
        // Those are different findings and the counters keep them apart.
        if (r->id) ++g_expired; else ++g_missing;
        return nullptr;
    }
    ++g_hits;
    g_lastHit = *r;
    g_haveLastHit = true;
    return r;
}


void set_inject_yaw_deg(float deg)
{
    if (deg < -180.0f) deg = -180.0f;
    if (deg >  180.0f) deg =  180.0f;
    g_injectYaw = deg;
    if (deg != 0.0f)
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
            "pose/rec: NEGATIVE CONTROL ARMED at %+.2f deg of yaw. Every record "
            "opened from now on carries a deliberately WRONG head yaw by that "
            "much, and the submission audit must report an error of exactly that "
            "size. If it keeps reporting zero, the audit is not measuring the "
            "association it claims to measure and none of its reassuring numbers "
            "mean anything. `posetrace inject 0` disarms it.", deg);
    else
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
            "pose/rec: negative control disarmed - records carry the real sample "
            "again.");
}

float inject_yaw_deg() { return g_injectYaw; }


void configure_self_test(uint32_t startAfter, uint32_t records, float deg)
{
    g_stStart = startAfter; g_stLen = records; g_stDeg = deg;
    if (!records)
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
            "pose/rec: the self test is disabled, so nothing will prove the "
            "submission join can report a wrong pose. Any zero it prints is "
            "unverified.");
}


Stats stats()
{
    Stats s;
    s.opened = g_opened; s.hits = g_hits; s.expired = g_expired;
    s.missing = g_missing; s.pairs = (uint32_t)InterlockedCompareExchange(&g_pair, 0, 0);
    return s;
}


void log_beat()
{
    const Stats s = stats();
    if (!g_haveLastHit) {
        // A zero here is NOT "everything agrees" - it is "nothing was ever
        // joined", and the two must never read the same.
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
            "pose/rec: %u record(s) opened over %u pair(s), and NONE has been "
            "looked up yet - the submission side is not asking, so the join is "
            "not being tested. missing=%u expired=%u.",
            s.opened, s.pairs, s.missing, s.expired);
        return;
    }
    const Record& r = g_lastHit;
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
        "pose/rec: opened %u over %u pair(s) | lookups hit %u, EXPIRED %u (the "
        "ring wrapped before submission asked - the pipeline is deeper than %u "
        "records), MISSING %u (an id nobody set: an untagged present, or a "
        "capture slot that carried no record) | last join: rec %u pair %u eye "
        "%+d, sample yaw %+.2f pitch %+.2f roll %+.2f deg gen %u, opened %.1f ms "
        "before it was read%s",
        s.opened, s.pairs, s.hits, s.expired, (unsigned)kRing, s.missing,
        r.id, r.pairId, r.eye, r.yawDeg, r.pitchDeg, r.rollDeg, r.gen,
        dvr::clock::now_ms() - r.openedMs,
        r.injectedYawDeg != 0.0f ? " [NEGATIVE CONTROL ARMED: this yaw is deliberately wrong]" : "");
}

} // namespace dvr::pose
