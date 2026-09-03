// core/framework/perf.cpp - see perf.h.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/framework/perf.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/gfx/capture.h"
#include "core/gfx/stereo.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace dvr::perf {
namespace {

// ---- one record per present ---------------------------------------------------
// All times in microseconds. A record opens at kEntry and closes at the NEXT
// kEntry (that is when OUT is known); an entry that arrives with the previous
// record still missing its game Present (the disabled or exiting path took
// the short way out of hkPresent) drops that record as incomplete.
struct Rec {
    uint32_t present = 0;      // frame::count() as of this present
    int      tag = 0;          // the eye the method attached (-1, +1, 0)
    int64_t  tEntry = 0;
    int64_t  tGameRet = 0;     // the game's Present returned
    uint32_t preUs = 0;        // pre_tick + the FpsCap wait
    uint32_t beginUs = 0;      // vr::on_present_begin (holds the wait)
    uint32_t waitUs = 0;       //   the runtime's wait phase (handoff or inline xrWaitFrame)
    uint32_t tickUs = 0;       // game_tick
    uint32_t methodUs = 0;     // stereo::end_frame (holds the capture)
    uint32_t capUs = 0;        //   capture::grab, the cost line's total
    uint32_t lockUs = 0, copyUs = 0, uploadUs = 0, blitUs = 0;
    uint32_t endUs = 0;        // vr::on_present_end (holds acquire, the swapchain copy, xrEndFrame)
    uint32_t acquireUs = 0, xrCopyUs = 0, endFrameUs = 0;
    uint32_t gamePresentUs = 0;
    uint32_t inUs = 0;         // entry -> the game's Present returned
    uint32_t outUs = 0;        // the game's Present returned -> the next entry
    bool     complete = false;
};

constexpr int kRing = 256;
Rec      g_ring[kRing];
int      g_head = 0;           // the open record
uint32_t g_records = 0;        // records ever opened
bool     g_open = false;
int64_t  g_t[kPointCount] = {};
bool     g_enabled = true;
uint32_t g_incomplete = 0;
long long g_qpcFreq = 0;

inline int64_t now_qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
inline uint32_t us(int64_t from, int64_t to) {
    if (!g_qpcFreq) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_qpcFreq = f.QuadPart ? f.QuadPart : 1;
    }
    if (to <= from) return 0;
    return (uint32_t)((to - from) * 1000000 / g_qpcFreq);
}

// ---- the 3 s window -----------------------------------------------------------
// Sums per attribution class: P1 (tag -1), P2 (tag +1), M (untagged, or every
// present under a one-present-per-tick method).
struct Sum {
    uint32_t n = 0;
    uint64_t in = 0, out = 0, pre = 0, begin = 0, wait = 0, tick = 0, method = 0, cap = 0, lock = 0,
             copy = 0, upload = 0, blit = 0, end = 0, acquire = 0, xrCopy = 0, endFrame = 0, gamePresent = 0;
    void add(const Rec& r) {
        ++n;
        in += r.inUs; out += r.outUs; pre += r.preUs; begin += r.beginUs; wait += r.waitUs;
        tick += r.tickUs; method += r.methodUs; cap += r.capUs; lock += r.lockUs; copy += r.copyUs;
        upload += r.uploadUs; blit += r.blitUs; end += r.endUs; acquire += r.acquireUs;
        xrCopy += r.xrCopyUs; endFrame += r.endFrameUs; gamePresent += r.gamePresentUs;
    }
    float ms(uint64_t v) const { return n ? (float)v / (float)n / 1000.0f : 0.0f; }
};
Sum      g_p1, g_p2, g_m;
uint64_t g_windowMs = 0;
uint32_t g_windowIncomplete = 0;
Window   g_last;
char     g_lastLine[1024] = "";

// One class as text: `in a (pre .. begin .. [wait ..] tick .. method .. [cap ..: lock .. copy ..
// up .. blit ..] end .. [acq .. xrCopy .. endFrame ..] present ..) + out b`.
int class_text(char* buf, int cap, const Sum& s) {
    return _snprintf(buf, cap,
                     "n=%u in %.1f (pre %.1f begin %.1f [wait %.1f] tick %.1f method %.1f [cap %.1f: lock %.1f "
                     "copy %.1f up %.1f blit %.1f] end %.1f [acq %.1f xrCopy %.1f endFrame %.1f] present %.1f) "
                     "+ out %.1f",
                     s.n, s.ms(s.in), s.ms(s.pre), s.ms(s.begin), s.ms(s.wait), s.ms(s.tick), s.ms(s.method),
                     s.ms(s.cap), s.ms(s.lock), s.ms(s.copy), s.ms(s.upload), s.ms(s.blit), s.ms(s.end),
                     s.ms(s.acquire), s.ms(s.xrCopy), s.ms(s.endFrame), s.ms(s.gamePresent), s.ms(s.out));
}

void window_close(uint64_t nowMs) {
    const double sec = (double)(nowMs - g_windowMs) / 1000.0;
    const uint32_t total = g_p1.n + g_p2.n + g_m.n;
    Window w;
    w.presentsPerS = sec > 0.0 ? (float)(total / sec) : 0.0f;
    // Is the runtime's wait the owner of the present? Above a quarter of the
    // display period (2 ms when the period is unknown) the present thread is
    // sitting on the headset's cadence and the split below is a budget.
    const int64_t periodNs = dvr::vr::display_period_ns();
    const float periodMs = periodNs > 0 ? (float)periodNs / 1.0e6f : 0.0f;
    const float waitThreshMs = periodMs > 0.0f ? periodMs * 0.25f : 2.0f;
    uint64_t waitSum = g_p1.wait + g_p2.wait + g_m.wait;
    w.waitMs = total ? (float)waitSum / (float)total / 1000.0f : 0.0f;
    w.paceBound = total > 0 && w.waitMs > waitThreshMs;
    w.stereo = (g_p1.n + g_p2.n) > 0;
    char paced[160] = "";
    if (w.paceBound)
        _snprintf(paced, sizeof(paced), "PACE-BOUND (wait %.1f ms/present = the headset's cadence at %.2f ms; the "
                  "split is a budget, not a bottleneck) ", w.waitMs, periodMs);
    char c1[400], c2[400], cm[400];
    if (w.stereo) {
        // The tick: one P1 and one P2 present. Its rate is the P1 count (the
        // P2 count when a window opened on a P2).
        const uint32_t ticks = g_p1.n ? g_p1.n : g_p2.n;
        w.ticksPerS = sec > 0.0 ? (float)(ticks / sec) : 0.0f;
        w.tickMs = (g_p1.ms(g_p1.in) + g_p1.ms(g_p1.out)) + (g_p2.ms(g_p2.in) + g_p2.ms(g_p2.out));
        w.inMs = (g_p1.ms(g_p1.in) + g_p2.ms(g_p2.in));
        w.outMs = (g_p1.ms(g_p1.out) + g_p2.ms(g_p2.out));
        w.captureMs = g_p1.ms(g_p1.cap) + g_p2.ms(g_p2.cap);
        w.lockMs = g_p1.ms(g_p1.lock) + g_p2.ms(g_p2.lock);
        class_text(c1, sizeof(c1), g_p1);
        class_text(c2, sizeof(c2), g_p2);
        _snprintf(g_lastLine, sizeof(g_lastLine),
                  "perf: tick %.1f ms (%.1f/s, %.0f presents/s) %s= P1[-1] %s | P2[+1] %s | untagged %u%s",
                  w.tickMs, w.ticksPerS, w.presentsPerS, paced, c1, c2, g_m.n,
                  g_windowIncomplete ? " | incomplete presents dropped" : "");
    } else {
        w.tickMs = g_m.ms(g_m.in) + g_m.ms(g_m.out);
        w.ticksPerS = w.presentsPerS;
        w.inMs = g_m.ms(g_m.in); w.outMs = g_m.ms(g_m.out);
        w.captureMs = g_m.ms(g_m.cap); w.lockMs = g_m.ms(g_m.lock);
        class_text(cm, sizeof(cm), g_m);
        _snprintf(g_lastLine, sizeof(g_lastLine),
                  "perf: present %.1f ms (%.1f/s) %s= %s | tick=n/a (one present per tick; the present line is "
                  "the tick)%s",
                  w.tickMs, w.presentsPerS, paced, cm,
                  g_windowIncomplete ? " | incomplete presents dropped" : "");
    }
    g_lastLine[sizeof(g_lastLine) - 1] = 0;
    g_last = w;
    if (total) DVR_INFO("%s", g_lastLine);
    g_p1 = Sum(); g_p2 = Sum(); g_m = Sum();
    g_windowIncomplete = 0;
    g_windowMs = nowMs;
}

void record_close(Rec& r, int64_t tNextEntry) {
    r.outUs = us(r.tGameRet, tNextEntry);
    r.complete = true;
    const bool twoPerTick = dvr::stereo::active() && dvr::stereo::active()->presents_per_tick() > 1;
    if (twoPerTick && r.tag < 0) g_p1.add(r);
    else if (twoPerTick && r.tag > 0) g_p2.add(r);
    else g_m.add(r);
}

} // namespace

void stamp(Point p) {
    if (!g_enabled || p < 0 || p >= kPointCount) return;
    const int64_t t = now_qpc();
    g_t[p] = t;
    if (p == kEntry) {
        Rec& prev = g_ring[g_head];
        if (g_open) {
            if (prev.tGameRet) record_close(prev, t);
            else { ++g_incomplete; ++g_windowIncomplete; }
            g_head = (g_head + 1) % kRing;
        }
        Rec& r = g_ring[g_head];
        r = Rec();
        r.tEntry = t;
        r.present = dvr::frame::count() + 1;
        g_open = true;
        ++g_records;
        const uint64_t nowMs = GetTickCount64();
        if (g_windowMs == 0) g_windowMs = nowMs;
        else if (nowMs - g_windowMs >= 3000) window_close(nowMs);
        return;
    }
    if (!g_open) return;
    Rec& r = g_ring[g_head];
    switch (p) {
    case kAfterPre:   r.preUs = us(g_t[kEntry], t); break;
    case kAfterBegin: r.beginUs = us(g_t[kAfterPre], t); break;
    case kAfterTick:  r.tickUs = us(g_t[kAfterBegin], t); break;
    case kAfterEnd: {
        r.methodUs = us(g_t[kAfterTick], t);
        r.tag = dvr::stereo::last_output().eyeSign;
        const dvr::capture::Cost c = dvr::capture::last_grab();
        r.capUs = c.totalUs; r.lockUs = c.lockUs; r.copyUs = c.copyUs; r.uploadUs = c.uploadUs; r.blitUs = c.blitUs;
        break;
    }
    case kAfterPresentEnd: {
        r.endUs = us(g_t[kAfterEnd], t);
        // The runtime's own phase timers for this present: the wait sits in
        // the present-head, acquire / the swapchain copy / xrEndFrame in the
        // tail. A frame-less present leaves them stale, so each is clamped to
        // the half it lives in.
        uint32_t ph[16] = {};
        const int n = dvr::vr::present_phases_last(ph, 16);
        if (n > 2) r.waitUs = ph[2] < r.beginUs ? ph[2] : r.beginUs;
        if (n > 7) {
            r.acquireUs = ph[5] < r.endUs ? ph[5] : r.endUs;
            r.xrCopyUs = ph[6] < r.endUs ? ph[6] : r.endUs;
            r.endFrameUs = ph[7] < r.endUs ? ph[7] : r.endUs;
        }
        break;
    }
    case kBeforeGamePresent: break;
    case kAfterGamePresent:
        r.gamePresentUs = us(g_t[kBeforeGamePresent], t);
        r.tGameRet = t;
        r.inUs = us(r.tEntry, t);
        break;
    default: break;
    }
}

void set_enabled(bool on) {
    if (on == g_enabled) return;
    g_enabled = on;
    g_open = false;
    DVR_INFO("perf: instruments %s (%s)", on ? "ON" : "off",
             on ? "eight QPC stamps per present; the tick line prints every 3 s"
                : "no stamps kept, no tick line; `perf on` restores");
}
bool enabled() { return g_enabled; }

void log_status() {
    if (!g_enabled) { DVR_INFO("perf: instruments off (`perf on`)"); return; }
    if (g_lastLine[0]) DVR_INFO("%s", g_lastLine);
    else DVR_INFO("perf: no window closed yet (the first line comes 3 s after the first present)");
    // The ring's tail: the last 8 closed presents.
    char buf[512];
    int n = _snprintf(buf, sizeof(buf), "perf: last presents (present:tag in/out ms, cap lock ms):");
    int idx = g_head;
    for (int k = 0; k < 8 && n > 0 && n < (int)sizeof(buf) - 48; ++k) {
        idx = (idx + kRing - 1) % kRing;
        const Rec& r = g_ring[idx];
        if (!r.complete) break;
        n += _snprintf(buf + n, sizeof(buf) - n, " #%u:%+d %.1f/%.1f %.1f", r.present, r.tag,
                       r.inUs / 1000.0f, r.outUs / 1000.0f, r.lockUs / 1000.0f);
    }
    DVR_INFO("%s", buf);
    if (g_incomplete)
        DVR_INFO("perf: %u presents left hkPresent by the short path (disabled or exiting) and were not counted",
                 g_incomplete);
}

void status(dvr::status::Writer& w) {
    w.kv("enabled", g_enabled);
    w.kv("presentsPerS", (double)g_last.presentsPerS);
    w.kv("ticksPerS", (double)g_last.ticksPerS);
    w.kv("tickMs", (double)g_last.tickMs);
    w.kv("inMs", (double)g_last.inMs);
    w.kv("outMs", (double)g_last.outMs);
    w.kv("waitMs", (double)g_last.waitMs);
    w.kv("captureMs", (double)g_last.captureMs);
    w.kv("lockMs", (double)g_last.lockMs);
    w.kv("paceBound", g_last.paceBound);
    w.kv("stereo", g_last.stereo);
    w.kv("incomplete", (unsigned long)g_incomplete);
}

Window last_window() { return g_last; }

} // namespace dvr::perf
