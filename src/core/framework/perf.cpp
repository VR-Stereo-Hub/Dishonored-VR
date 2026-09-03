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
#include <d3d9.h>
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
    int64_t  tFirstBegin = 0;  // the first BeginScene after the game's Present (0 = none)
    int64_t  tFirstSrt = 0;    // the first SetRenderTarget after it (the fallback marker)
    uint16_t beginScenes = 0, srts = 0;   // marker populations in this present's OUT
    uint32_t idleUs = 0, rUs = 0;         // OUT split: before the marker / after it
    bool     complete = false;
    // The GPU timestamps of this present (raw ticks, resolved K presents later).
    uint64_t gpuBegin = 0, gpuEntry = 0, gpuRtdA = 0, gpuRtdB = 0, gpuPresent = 0, gpuFreq = 0;
    uint32_t gpuSpanUs = 0, gpuDmaUs = 0, gpuIdleUs = 0;
    uint8_t  gpuState = 0;     // 0 pending, 1 resolved, 2 late, 3 disjoint, 4 unmarked (no tsBegin), 5 off
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
DWORD    g_presentTid = 0;     // the thread that stamps kEntry
DWORD    g_markerTid = 0;      // the thread the marker last arrived on
bool     g_markerTidSaid = false;

// ---- the GPU query ring ---------------------------------------------------------
// Set i serves record i % kGpuRing. Each set: disjoint (BEGIN at the marker,
// END before the game's Present), freq (END before the game's Present), and
// the four timestamps. `issued` says which GetData calls are legal.
enum { kQDisjoint = 0, kQFreq, kQBegin, kQEntry, kQRtdA, kQRtdB, kQPresent, kQCount };
constexpr int kGpuRing = 8;
constexpr int kGpuReadBack = 5;   // read record N-5: past D3D9's default 3-frame queue
struct GpuSet {
    IDirect3DQuery9* q[kQCount] = {};
    bool issued[kQCount] = {};
    uint32_t record = 0;          // the record this set was issued for
    bool inUse = false;
};
GpuSet   g_gpu[kGpuRing];
IDirect3DDevice9* g_dev = nullptr;
bool     g_gpuEnabled = true;
bool     g_gpuCreated = false;
bool     g_gpuNa = false;         // the device refused a query type (said once)
uint32_t g_gpuLate = 0, g_gpuDisjoint = 0, g_gpuResolved = 0, g_gpuUnmarked = 0;
uint64_t g_lastPresentTs = 0;     // the previous resolved record's tsPresent (for idle)
uint32_t g_lastPresentRec = 0;

void gpu_release() {
    for (int i = 0; i < kGpuRing; ++i) {
        for (int k = 0; k < kQCount; ++k) {
            if (g_gpu[i].q[k]) { g_gpu[i].q[k]->Release(); g_gpu[i].q[k] = nullptr; }
            g_gpu[i].issued[k] = false;
        }
        g_gpu[i].inUse = false;
    }
    g_gpuCreated = false;
}

bool gpu_ensure() {
    if (g_gpuCreated) return true;
    if (!g_dev || !g_gpuEnabled || g_gpuNa) return false;
    static const D3DQUERYTYPE kTypes[kQCount] = {
        D3DQUERYTYPE_TIMESTAMPDISJOINT, D3DQUERYTYPE_TIMESTAMPFREQ, D3DQUERYTYPE_TIMESTAMP,
        D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMP, D3DQUERYTYPE_TIMESTAMP};
    for (int i = 0; i < kGpuRing; ++i) {
        for (int k = 0; k < kQCount; ++k) {
            const HRESULT hr = g_dev->CreateQuery(kTypes[k], &g_gpu[i].q[k]);
            if (FAILED(hr) || !g_gpu[i].q[k]) {
                g_gpuNa = true;
                DVR_WARN("perf: gpu n/a - D3DQUERYTYPE %d refused by this device (CreateQuery 0x%08lx); the tick "
                         "line still measures the render thread, the GPU/CPU split cannot be measured on this "
                         "driver", (int)kTypes[k], (unsigned long)hr);
                gpu_release();
                return false;
            }
        }
    }
    g_gpuCreated = true;
    DVR_INFO("perf: gpu timestamp ring live (%d sets, read %d presents back, never waited on)", kGpuRing,
             kGpuReadBack);
    return true;
}

inline GpuSet& gpu_set_for(uint32_t rec) { return g_gpu[rec % kGpuRing]; }

void gpu_issue_into(uint32_t recNo, int k, DWORD flags) {
    if (!gpu_ensure()) return;
    GpuSet& s = gpu_set_for(recNo);
    if (!s.inUse || s.record != recNo) {
        for (int j = 0; j < kQCount; ++j) s.issued[j] = false;
        s.record = recNo; s.inUse = true;
    }
    if (s.q[k] && SUCCEEDED(s.q[k]->Issue(flags))) s.issued[k] = true;
}
// The marker fires after record N's game Present and before record N+1's
// entry: it belongs to the frame N+1 completes.
inline void gpu_issue_next(int k, DWORD flags) { gpu_issue_into(g_records + 1, k, flags); }

// Issue one timestamp (or the END of a bracket) into the open record's set.
inline void gpu_issue(int k, DWORD flags) { if (g_open) gpu_issue_into(g_records, k, flags); }

// Resolve the set of record N - kGpuReadBack, never waiting.
void gpu_resolve(Rec* ring, int headIdx) {
    if (!g_gpuCreated || g_records < (uint32_t)kGpuReadBack + 1) return;
    const uint32_t recNo = g_records - kGpuReadBack;
    GpuSet& s = gpu_set_for(recNo);
    if (!s.inUse || s.record != recNo) return;
    const int idx = (headIdx + kRing - kGpuReadBack) % kRing;
    Rec& r = ring[idx];
    if (r.gpuState != 0) return;
    if (!s.issued[kQEntry] || !s.issued[kQPresent] || !s.issued[kQFreq] || !s.issued[kQDisjoint]) {
        r.gpuState = 4; ++g_gpuUnmarked; s.inUse = false; return;
    }
    if (!s.issued[kQBegin]) { r.gpuState = 4; ++g_gpuUnmarked; s.inUse = false; return; }
    BOOL disjoint = FALSE;
    UINT64 freq = 0, v[kQCount] = {};
    HRESULT hr = s.q[kQDisjoint]->GetData(&disjoint, sizeof(disjoint), 0);
    if (hr == S_FALSE) { r.gpuState = 2; ++g_gpuLate; s.inUse = false; return; }
    if (FAILED(hr)) { r.gpuState = 2; ++g_gpuLate; s.inUse = false; return; }
    hr = s.q[kQFreq]->GetData(&freq, sizeof(freq), 0);
    if (hr != S_OK || !freq) { r.gpuState = 2; ++g_gpuLate; s.inUse = false; return; }
    for (int k = kQBegin; k < kQCount; ++k) {
        if (!s.issued[k]) continue;
        hr = s.q[k]->GetData(&v[k], sizeof(v[k]), 0);
        if (hr != S_OK) { r.gpuState = 2; ++g_gpuLate; s.inUse = false; return; }
    }
    s.inUse = false;
    if (disjoint) { r.gpuState = 3; ++g_gpuDisjoint; return; }
    r.gpuFreq = freq; r.gpuBegin = v[kQBegin]; r.gpuEntry = v[kQEntry]; r.gpuPresent = v[kQPresent];
    r.gpuRtdA = s.issued[kQRtdA] ? v[kQRtdA] : 0; r.gpuRtdB = s.issued[kQRtdB] ? v[kQRtdB] : 0;
    auto toUs = [freq](uint64_t a, uint64_t b) -> uint32_t { return b > a ? (uint32_t)((b - a) * 1000000 / freq) : 0u; };
    r.gpuSpanUs = toUs(r.gpuBegin, r.gpuEntry);
    r.gpuDmaUs = (r.gpuRtdA && r.gpuRtdB) ? toUs(r.gpuRtdA, r.gpuRtdB) : 0;
    r.gpuIdleUs = (g_lastPresentTs && g_lastPresentRec + 1 == recNo) ? toUs(g_lastPresentTs, r.gpuBegin) : 0;
    g_lastPresentTs = r.gpuPresent; g_lastPresentRec = recNo;
    r.gpuState = 1; ++g_gpuResolved;
}

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
             copy = 0, upload = 0, blit = 0, end = 0, acquire = 0, xrCopy = 0, endFrame = 0, gamePresent = 0,
             idle = 0, r = 0, beginScenes = 0, srts = 0, withBegin = 0, withSrt = 0,
             gpuSpan = 0, gpuDma = 0, gpuIdle = 0;
    uint32_t gpuN = 0, gpuLate = 0, gpuDisjoint = 0, gpuUnmarked = 0;
    void add(const Rec& r_) {
        ++n;
        in += r_.inUs; out += r_.outUs; pre += r_.preUs; begin += r_.beginUs; wait += r_.waitUs;
        tick += r_.tickUs; method += r_.methodUs; cap += r_.capUs; lock += r_.lockUs; copy += r_.copyUs;
        upload += r_.uploadUs; blit += r_.blitUs; end += r_.endUs; acquire += r_.acquireUs;
        xrCopy += r_.xrCopyUs; endFrame += r_.endFrameUs; gamePresent += r_.gamePresentUs;
        idle += r_.idleUs; r += r_.rUs; beginScenes += r_.beginScenes; srts += r_.srts;
        if (r_.tFirstBegin) ++withBegin;
        if (r_.tFirstSrt) ++withSrt;
    }
    // The GPU numbers arrive kGpuReadBack presents after the record closed,
    // so they are added by the resolver, not by add().
    void add_gpu(const Rec& r_) {
        if (r_.gpuState == 1) { ++gpuN; gpuSpan += r_.gpuSpanUs; gpuDma += r_.gpuDmaUs; gpuIdle += r_.gpuIdleUs; }
        else if (r_.gpuState == 2) ++gpuLate;
        else if (r_.gpuState == 3) ++gpuDisjoint;
        else if (r_.gpuState == 4) ++gpuUnmarked;
    }
    float ms(uint64_t v) const { return n ? (float)v / (float)n / 1000.0f : 0.0f; }
    float gms(uint64_t v) const { return gpuN ? (float)v / (float)gpuN / 1000.0f : 0.0f; }
};
Sum      g_p1, g_p2, g_m;
uint64_t g_windowMs = 0;
uint32_t g_windowIncomplete = 0;
Window   g_last;
char     g_lastLine[1024] = "";
char     g_lastGpuLine[512] = "";

// One class as text: `in a (pre .. begin .. [wait ..] tick .. method .. [cap ..: lock .. copy ..
// up .. blit ..] end .. [acq .. xrCopy .. endFrame ..] present ..) + out b`.
int class_text(char* buf, int cap, const Sum& s) {
    return _snprintf(buf, cap,
                     "n=%u in %.1f (pre %.1f begin %.1f [wait %.1f] tick %.1f method %.1f [cap %.1f: lock %.1f "
                     "copy %.1f up %.1f blit %.1f] end %.1f [acq %.1f xrCopy %.1f endFrame %.1f] present %.1f) "
                     "+ out %.1f (idle %.1f R %.1f)",
                     s.n, s.ms(s.in), s.ms(s.pre), s.ms(s.begin), s.ms(s.wait), s.ms(s.tick), s.ms(s.method),
                     s.ms(s.cap), s.ms(s.lock), s.ms(s.copy), s.ms(s.upload), s.ms(s.blit), s.ms(s.end),
                     s.ms(s.acquire), s.ms(s.xrCopy), s.ms(s.endFrame), s.ms(s.gamePresent), s.ms(s.out),
                     s.ms(s.idle), s.ms(s.r));
}

// Which marker split OUT this window, with its population per present, and
// the starved verdict: idle above 30 % of OUT means the render thread waited
// for the game thread more than it worked (model C, the one neither "the
// capture" nor "the GPU" names).
int marker_text(char* buf, int cap, const Sum& a, const Sum& b, const Sum& c, Window& w) {
    const uint32_t n = a.n + b.n + c.n;
    const uint64_t bs = a.beginScenes + b.beginScenes + c.beginScenes;
    const uint64_t sr = a.srts + b.srts + c.srts;
    const uint64_t wb = a.withBegin + b.withBegin + c.withBegin;
    const uint64_t ws = a.withSrt + b.withSrt + c.withSrt;
    const uint64_t idle = a.idle + b.idle + c.idle, out = a.out + b.out + c.out;
    if (wb) strcpy_s(w.marker, sizeof(w.marker), "BeginScene");
    else if (ws) strcpy_s(w.marker, sizeof(w.marker), "SRT-first");
    else strcpy_s(w.marker, sizeof(w.marker), "none");
    const bool starved = out > 0 && idle * 10 > out * 3;
    return _snprintf(buf, cap, "marker=%s(BeginScene %.1f/present in %llu of %u, SRT %.1f/present)%s%s",
                     w.marker, n ? (double)bs / n : 0.0, (unsigned long long)wb, n, n ? (double)sr / n : 0.0,
                     wb == 0 && ws == 0 ? " (no marker: idle/R unsplit, R = OUT)" : "",
                     starved ? " (RENDER THREAD STARVED: idle > 30 % of OUT, the game thread is the limiter)" : "");
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
    char c1[400], c2[400], cm[400], mk[240];
    marker_text(mk, sizeof(mk), g_p1, g_p2, g_m, w);
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
        w.idleMs = g_p1.ms(g_p1.idle) + g_p2.ms(g_p2.idle);
        w.rMs = g_p1.ms(g_p1.r) + g_p2.ms(g_p2.r);
        class_text(c1, sizeof(c1), g_p1);
        class_text(c2, sizeof(c2), g_p2);
        _snprintf(g_lastLine, sizeof(g_lastLine),
                  "perf: tick %.1f ms (%.1f/s, %.0f presents/s) %s= P1[-1] %s | P2[+1] %s | untagged %u | %s%s",
                  w.tickMs, w.ticksPerS, w.presentsPerS, paced, c1, c2, g_m.n, mk,
                  g_windowIncomplete ? " | incomplete presents dropped" : "");
    } else {
        w.tickMs = g_m.ms(g_m.in) + g_m.ms(g_m.out);
        w.ticksPerS = w.presentsPerS;
        w.inMs = g_m.ms(g_m.in); w.outMs = g_m.ms(g_m.out);
        w.captureMs = g_m.ms(g_m.cap); w.lockMs = g_m.ms(g_m.lock);
        w.idleMs = g_m.ms(g_m.idle); w.rMs = g_m.ms(g_m.r);
        class_text(cm, sizeof(cm), g_m);
        _snprintf(g_lastLine, sizeof(g_lastLine),
                  "perf: present %.1f ms (%.1f/s) %s= %s | tick=n/a (one present per tick; the present line is "
                  "the tick) | %s%s",
                  w.tickMs, w.presentsPerS, paced, cm, mk,
                  g_windowIncomplete ? " | incomplete presents dropped" : "");
    }
    g_lastLine[sizeof(g_lastLine) - 1] = 0;
    // The GPU line: per tick under stereo (P1 + P2 means), per present under
    // mono. The 3d tail = the CPU's lock wait minus the readback's own GPU
    // time: what the lock spent waiting for the GPU to FINISH THE FRAME, the
    // part no capture path removes.
    {
        const uint32_t gn = g_p1.gpuN + g_p2.gpuN + g_m.gpuN;
        const uint32_t late = g_p1.gpuLate + g_p2.gpuLate + g_m.gpuLate;
        const uint32_t dis = g_p1.gpuDisjoint + g_p2.gpuDisjoint + g_m.gpuDisjoint;
        const uint32_t unm = g_p1.gpuUnmarked + g_p2.gpuUnmarked + g_m.gpuUnmarked;
        w.gpuResolved = gn; w.gpuLate = late; w.gpuDisjoint = dis; w.gpuUnmarked = unm;
        if (!g_gpuEnabled) { strcpy_s(w.gpu, sizeof(w.gpu), "off"); _snprintf(g_lastGpuLine, sizeof(g_lastGpuLine), "perf: gpu off (`perf gpu on`)"); }
        else if (g_gpuNa) { strcpy_s(w.gpu, sizeof(w.gpu), "n/a"); _snprintf(g_lastGpuLine, sizeof(g_lastGpuLine), "perf: gpu n/a (timestamp queries refused by this device; the CPU split above stands alone)"); }
        else {
            strcpy_s(w.gpu, sizeof(w.gpu), "ok");
            float spanP, dmaP, idleP, spanT, idleT, dmaT, lockT;
            if (w.stereo) {
                spanP = (g_p1.gms(g_p1.gpuSpan) + g_p2.gms(g_p2.gpuSpan)) * 0.5f;
                dmaP = (g_p1.gms(g_p1.gpuDma) + g_p2.gms(g_p2.gpuDma)) * 0.5f;
                idleP = (g_p1.gms(g_p1.gpuIdle) + g_p2.gms(g_p2.gpuIdle)) * 0.5f;
                spanT = g_p1.gms(g_p1.gpuSpan) + g_p2.gms(g_p2.gpuSpan);
                dmaT = g_p1.gms(g_p1.gpuDma) + g_p2.gms(g_p2.gpuDma);
                idleT = g_p1.gms(g_p1.gpuIdle) + g_p2.gms(g_p2.gpuIdle);
                lockT = w.lockMs;
            } else {
                spanP = spanT = g_m.gms(g_m.gpuSpan); dmaP = dmaT = g_m.gms(g_m.gpuDma);
                idleP = idleT = g_m.gms(g_m.gpuIdle); lockT = w.lockMs;
            }
            w.gpuSpanMs = spanT; w.gpuDmaMs = dmaT; w.gpuIdleMs = idleT;
            const float tail = lockT > dmaT ? lockT - dmaT : 0.0f;
            const uint32_t population = gn + late + dis + unm;
            _snprintf(g_lastGpuLine, sizeof(g_lastGpuLine),
                      "perf: gpu/present span=%.1f ms (3d %.1f + readback dma %.1f) idle(d3d9)=%.1f ms | per %s "
                      "span=%.1f dma=%.1f idle=%.1f | %u resolved, %u late, %u disjoint, %u unmarked of %u | cpu "
                      "lock=%.1f -> 3d tail = lock - dma = %.1f ms%s%s",
                      spanP, spanP > dmaP ? spanP - dmaP : 0.0f, dmaP, idleP, w.stereo ? "tick" : "present",
                      spanT, dmaT, idleT, gn, late, dis, unm, population, lockT, tail,
                      population && late * 4 > population ? " (late > 25 %: K=5 too shallow for this queue)" : "",
                      gn == 0 && population ? " (nothing resolved: no marker, or every set late)" : "");
        }
        g_lastGpuLine[sizeof(g_lastGpuLine) - 1] = 0;
    }
    g_last = w;
    if (total) { DVR_INFO("%s", g_lastLine); DVR_INFO("%s", g_lastGpuLine); }
    g_p1 = Sum(); g_p2 = Sum(); g_m = Sum();
    g_windowIncomplete = 0;
    g_windowMs = nowMs;
}

void record_close(Rec& r, int64_t tNextEntry) {
    r.outUs = us(r.tGameRet, tNextEntry);
    // The marker splits OUT: BeginScene when the game issued one after its
    // Present, else the first SetRenderTarget, else nothing (R = OUT).
    const int64_t tMark = r.tFirstBegin ? r.tFirstBegin : r.tFirstSrt;
    if (tMark && tMark >= r.tGameRet && tMark <= tNextEntry) {
        r.idleUs = us(r.tGameRet, tMark);
        r.rUs = us(tMark, tNextEntry);
    } else {
        r.idleUs = 0;
        r.rUs = r.outUs;
    }
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
        g_presentTid = GetCurrentThreadId();
        ++g_records;
        // The GPU: this present's entry stamp (the frame the GPU is finishing
        // passes here), then the set from five presents ago is read, never
        // waited on, and joins the window's class sums.
        gpu_issue(kQEntry, D3DISSUE_END);
        if (g_gpuCreated) {
            const int idx = (g_head + kRing - kGpuReadBack) % kRing;
            Rec& old = g_ring[idx];
            const uint8_t before = old.gpuState;
            gpu_resolve(g_ring, g_head);
            if (before == 0 && old.gpuState != 0 && old.complete) {
                const bool twoPerTick = dvr::stereo::active() && dvr::stereo::active()->presents_per_tick() > 1;
                if (twoPerTick && old.tag < 0) g_p1.add_gpu(old);
                else if (twoPerTick && old.tag > 0) g_p2.add_gpu(old);
                else g_m.add_gpu(old);
            }
        }
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
    case kBeforeGamePresent:
        gpu_issue(kQPresent, D3DISSUE_END);
        gpu_issue(kQFreq, D3DISSUE_END);
        gpu_issue(kQDisjoint, D3DISSUE_END);
        break;
    case kAfterGamePresent:
        r.gamePresentUs = us(g_t[kBeforeGamePresent], t);
        r.tGameRet = t;
        r.inUs = us(r.tEntry, t);
        break;
    default: break;
    }
}

void frame_start_marker(const char* which) {
    if (!g_enabled || !g_open) return;
    Rec& r = g_ring[g_head];
    if (!r.tGameRet) return;   // inside hkPresent (the mod's own draws): not the frame start
    const int64_t t = now_qpc();
    const bool bs = which && which[0] == 'B';
    if (bs) {
        if (!r.tFirstBegin) {
            r.tFirstBegin = t;
            // The GPU frame starts here: the disjoint bracket opens for the
            // NEXT record's set (this marker belongs to the frame the next
            // Present completes), and its tsBegin is issued.
            gpu_issue_next(kQDisjoint, D3DISSUE_BEGIN);
            gpu_issue_next(kQBegin, D3DISSUE_END);
        }
        if (r.beginScenes < 0xffff) ++r.beginScenes;
    }
    else    { if (!r.tFirstSrt) r.tFirstSrt = t;     if (r.srts < 0xffff) ++r.srts; }
    const DWORD tid = GetCurrentThreadId();
    if (tid != g_markerTid) {
        g_markerTid = tid;
        if (tid != g_presentTid && !g_markerTidSaid) {
            g_markerTidSaid = true;
            DVR_WARN("perf: the frame-start marker (%s) arrives on tid %lu, Present on tid %lu - idle/R are "
                     "cross-thread wall-clock deltas, read them as such", which, (unsigned long)tid,
                     (unsigned long)g_presentTid);
        }
    }
}

void set_device(IDirect3DDevice9* dev) {
    if (dev == g_dev) return;
    gpu_release();
    g_dev = dev;
    g_gpuNa = false;
}

void gpu_mark(GpuPoint p) {
    if (!g_enabled || !g_open) return;
    gpu_issue(p == kGpuRtdA ? kQRtdA : kQRtdB, D3DISSUE_END);
}

void on_reset() {
    gpu_release();
    g_lastPresentTs = 0;
}

void set_gpu_enabled(bool on) {
    if (on == g_gpuEnabled) return;
    g_gpuEnabled = on;
    if (!on) gpu_release();
    else g_gpuNa = false;
    DVR_INFO("perf: gpu timestamps %s (%s)", on ? "ON" : "off",
             on ? "a query ring on the game's device, read five presents back, never waited on"
                : "no queries issued; the tick line keeps the CPU split");
}
bool gpu_enabled() { return g_gpuEnabled; }

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
    if (g_lastLine[0]) { DVR_INFO("%s", g_lastLine); DVR_INFO("%s", g_lastGpuLine); }
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
    w.kv("idleMs", (double)g_last.idleMs);
    w.kv("rMs", (double)g_last.rMs);
    w.kv("marker", g_last.marker);
    w.kv("gpu", g_last.gpu);
    w.kv("gpuSpanMs", (double)g_last.gpuSpanMs);
    w.kv("gpuDmaMs", (double)g_last.gpuDmaMs);
    w.kv("gpuIdleMs", (double)g_last.gpuIdleMs);
    w.kv("gpuResolved", (unsigned long)g_last.gpuResolved);
    w.kv("gpuLate", (unsigned long)g_last.gpuLate);
    w.kv("gpuDisjoint", (unsigned long)g_last.gpuDisjoint);
    w.kv("paceBound", g_last.paceBound);
    w.kv("stereo", g_last.stereo);
    w.kv("incomplete", (unsigned long)g_incomplete);
}

Window last_window() { return g_last; }

} // namespace dvr::perf
