// core/framework/perf_ab.cpp - the performance A/B (41.1, VR-67). Included by
// src/mod/dishonoredvr.cpp (unity build).
//
// WHY THIS EXISTS. `perf:` reports WINDOW MEANS, and a mean cannot answer the
// question the tester actually asks, which is about frame DROPS - a run that
// averages 13 ms and a run that averages 13 ms with a 90 ms stall every second
// print the same line and feel nothing alike. This module records the interval
// between consecutive presents and reports a DISTRIBUTION per segment:
// p50, p95, p99 and max, plus how many intervals exceeded twice the median.
//
// AND IT SWITCHES THE LEVER ITSELF. The project's rule is that an instrument
// which cannot fail its own hypothesis is not evidence, and the VR-65 lesson
// is that a comparison averaged across a transition destroys the signal the
// transition exists to produce. So the plan is a fixed, announced sequence of
// segments; the BASELINE RETURNS between alternatives, because an improvement
// that does not come back when the baseline returns is a coincidence; the
// first two seconds of every segment are DISCARDED so a lever's own switching
// cost is not charged to it; and the summary prints every segment against the
// baseline's own median so the reader can see the baseline repeat.
//
// A segment that changes nothing prints "no change" rather than a small
// number dressed up as a result: the run-to-run spread of the two baseline
// segments IS the noise floor, and it is printed as such.
//
// One run answers the whole plan. Nothing here changes what is rendered.

#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::perf

namespace dvr::perf {

static IDirect3DDevice9* g_abDevice = NULL;   // identity and frame-latency only; never released by us

// ---- levers ---------------------------------------------------------------
//
// Each lever is one thing that can be switched on the present thread with no
// device recreation. Adding one means adding an apply() case and a plan row.
enum AbLever {
    kAbNone = 0,
    kAbFrameId,        // [Perf] FrameId - the 64x64 GetRenderTargetData readback
    kAbFrameLatency,   // IDirect3DDevice9Ex::SetMaximumFrameLatency
};

struct AbSeg {
    const char* label;
    AbLever     lever;
    int         value;
    bool        baseline;
};

// The plan. Baseline, alternative, BASELINE AGAIN, then the latency sweep with
// the readback restored so only one thing differs from baseline at a time, and
// baseline last so the run ends where it started.
static const AbSeg kAbPlan[] = {
    { "baseline",              kAbNone,         0, true  },
    { "frameid readback OFF",  kAbFrameId,      0, false },
    { "baseline again",        kAbNone,         0, true  },
    { "max frame latency 1",   kAbFrameLatency, 1, false },
    { "max frame latency 2",   kAbFrameLatency, 2, false },
    { "max frame latency 3",   kAbFrameLatency, 3, false },
    { "baseline last",         kAbNone,         0, true  },
};
static const int kAbSegN = (int)(sizeof(kAbPlan) / sizeof(kAbPlan[0]));

#define DVR_AB_MAX_SAMPLES 16384u

struct AbResult {
    uint32_t n;
    float mean, p50, p95, p99, max;
    uint32_t overTwiceMedian;
    bool  valid;
};

static bool     g_abOn = false;
static bool     g_abDone = false;
static uint32_t g_abSegMs = 20000;
static uint32_t g_abWarmMs = 2000;      // discarded at the head of every segment
static int      g_abSeg = -1;
static uint64_t g_abSegStartMs = 0;
static double   g_abPrev = 0.0;
static float*   g_abSamples = NULL;
static uint32_t g_abN = 0;
static AbResult g_abRes[kAbSegN];
static int      g_abFrameIdWas = -1;    // restored when the plan ends
static int      g_abLatencyWas = -1;
static bool     g_abGameplay = false;   // the plan only runs in gameplay
static bool     g_abSegDirty = false;   // gameplay was lost inside this segment
static bool     g_abWaitLogged = false;

static void AbApply(AbLever lever, int value)
{
    switch (lever) {
    case kAbFrameId:
        dvr::frameid::set_enabled(value != 0);
        break;
    case kAbFrameLatency:
        if (g_abDevice) {
            IDirect3DDevice9Ex* ex = NULL;
            if (SUCCEEDED(g_abDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&ex)) && ex) {
                const HRESULT hr = ex->SetMaximumFrameLatency((UINT)value);
                UINT got = 0;
                ex->GetMaximumFrameLatency(&got);
                ex->Release();
                DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                        "perf/ab: SetMaximumFrameLatency(%d) hr=0x%08lx, the device now reports %u",
                        value, (unsigned long)hr, got);
            } else {
                DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                        "perf/ab: max frame latency REFUSED - the device is not a IDirect3DDevice9Ex, so this "
                        "segment is a duplicate of the baseline and its numbers mean nothing");
            }
        }
        break;
    default:
        break;
    }
}

// The baseline: whatever the run was configured with. Captured once so the
// plan restores it, and re-applied at the head of every baseline segment.
static void AbRestoreBaseline()
{
    if (g_abFrameIdWas >= 0) dvr::frameid::set_enabled(g_abFrameIdWas != 0);
    if (g_abLatencyWas >= 0 && g_abDevice) {
        IDirect3DDevice9Ex* ex = NULL;
        if (SUCCEEDED(g_abDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&ex)) && ex) {
            ex->SetMaximumFrameLatency((UINT)g_abLatencyWas);
            ex->Release();
        }
    }
}

static int AbCmpFloat(const void* a, const void* b)
{
    const float x = *(const float*)a, y = *(const float*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static float AbPct(const float* sorted, uint32_t n, float p)
{
    if (!n) return 0.0f;
    uint32_t i = (uint32_t)(p * (float)(n - 1) + 0.5f);
    if (i >= n) i = n - 1;
    return sorted[i];
}

static void AbCloseSegment()
{
    if (g_abSeg < 0 || g_abSeg >= kAbSegN) return;
    AbResult r; memset(&r, 0, sizeof(r));
    if (g_abSegDirty) {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: segment %d of %d (%s) DISCARDED - gameplay was lost inside it (a menu, a load or a "
                "cutscene). Its %u samples are not comparable with the others and are thrown away.",
                g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label, g_abN);
        g_abRes[g_abSeg] = r;
        return;
    }
    if (g_abN >= 32) {
        qsort(g_abSamples, g_abN, sizeof(float), AbCmpFloat);
        double sum = 0.0;
        for (uint32_t i = 0; i < g_abN; ++i) sum += g_abSamples[i];
        r.n = g_abN;
        r.mean = (float)(sum / (double)g_abN);
        r.p50 = AbPct(g_abSamples, g_abN, 0.50f);
        r.p95 = AbPct(g_abSamples, g_abN, 0.95f);
        r.p99 = AbPct(g_abSamples, g_abN, 0.99f);
        r.max = g_abSamples[g_abN - 1];
        const float twice = r.p50 * 2.0f;
        for (uint32_t i = 0; i < g_abN; ++i) if (g_abSamples[i] > twice) ++r.overTwiceMedian;
        r.valid = true;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment %d of %d DONE (%s): %u presents | p50 %.2f ms (%.1f/s) p95 %.2f p99 %.2f "
                "max %.2f | mean %.2f | %u intervals over twice the median (%.2f%%)",
                g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label, r.n, r.p50,
                r.p50 > 0.0f ? 1000.0f / r.p50 : 0.0f, r.p95, r.p99, r.max, r.mean,
                r.overTwiceMedian, 100.0f * (float)r.overTwiceMedian / (float)r.n);
    } else {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: segment %d of %d (%s) collected only %u presents after the %u ms warm-up - too few to "
                "quote a distribution, this segment is DISCARDED (were you in a menu or a load?)",
                g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label, g_abN, g_abWarmMs);
    }
    g_abRes[g_abSeg] = r;
}

static void AbSummary()
{
    // The noise floor first: the baselines are the same configuration measured
    // at different times, so the spread between them is what "no change" looks
    // like. Any alternative inside that band has not been shown to do anything.
    float bMin = 0.0f, bMax = 0.0f; int bN = 0; float bRef = 0.0f;
    for (int i = 0; i < kAbSegN; ++i) {
        if (!kAbPlan[i].baseline || !g_abRes[i].valid) continue;
        const float v = g_abRes[i].p50;
        if (!bN || v < bMin) bMin = v;
        if (!bN || v > bMax) bMax = v;
        bRef += v; ++bN;
    }
    if (!bN) {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: PLAN COMPLETE but no baseline segment produced a distribution - nothing can be compared. "
                "Re-run in steady gameplay, not in a menu or a load.");
        return;
    }
    bRef /= (float)bN;
    const float floorPct = bRef > 0.0f ? 100.0f * (bMax - bMin) / bRef : 0.0f;
    DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
            "perf/ab: PLAN COMPLETE. NOISE FLOOR: %d baseline segment(s), p50 %.2f..%.2f ms, mean %.2f - a change "
            "smaller than %.1f%% is inside the spread of the SAME configuration and is not a result.",
            bN, bMin, bMax, bRef, floorPct);
    for (int i = 0; i < kAbSegN; ++i) {
        const AbResult& r = g_abRes[i];
        if (!r.valid) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab:   %-22s DISCARDED (too few presents)", kAbPlan[i].label);
            continue;
        }
        const float dP50 = bRef > 0.0f ? 100.0f * (r.p50 - bRef) / bRef : 0.0f;
        const float dP99 = bRef > 0.0f ? 100.0f * (r.p99 - bRef) / bRef : 0.0f;
        const bool inside = floorPct > 0.0f ? (dP50 < 0.0f ? -dP50 : dP50) <= floorPct : false;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab:   %-22s p50 %.2f ms (%+.1f%% vs baseline) p95 %.2f p99 %.2f (%+.1f%%) max %.2f | "
                "%u/%u over twice median | %s",
                kAbPlan[i].label, r.p50, dP50, r.p95, r.p99, dP99, r.max, r.overTwiceMedian, r.n,
                kAbPlan[i].baseline ? "(baseline)"
                : inside ? "NO CHANGE - inside the noise floor"
                : dP50 < 0.0f ? "FASTER than baseline - worth pursuing" : "SLOWER than baseline");
    }
    DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
            "perf/ab: baseline restored. p50 is the interval between consecutive PRESENTS, so under a two-present "
            "stereo method a complete pair is twice it. p99 and 'over twice median' are the frame-drop columns - "
            "a lever that lowers p50 and raises p99 has moved a wait, not removed it.");
}

// Called from hkPresent, at the entry stamp, on the present thread.
void ab_tick(IDirect3DDevice9* dev)
{
    if (!g_abOn || g_abDone) return;
    g_abDevice = dev;

    // The plan measures GAMEPLAY. Starting it in the menu would spend every
    // segment on a screen that does not render the game, and the numbers would
    // be a comparison of menus. Losing gameplay inside a segment poisons that
    // segment rather than the whole plan.
    if (!g_abGameplay) {
        if (g_abSeg >= 0) g_abSegDirty = true;
        else if (!g_abWaitLogged) {
            g_abWaitLogged = true;
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab: armed, WAITING FOR GAMEPLAY - the plan starts at the first present after the game "
                    "is in play, not in the menu. %d segments of %u ms; just play normally when you get in.",
                    kAbSegN, g_abSegMs);
        }
        return;
    }

    const double now = dvr::clock::now_ms();
    const uint64_t nowMs = GetTickCount64();

    if (g_abSeg < 0) {   // the plan starts here: remember what to restore
        if (!g_abSamples) {
            g_abSamples = (float*)malloc(DVR_AB_MAX_SAMPLES * sizeof(float));
            if (!g_abSamples) {
                DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                        "perf/ab: could not allocate the sample buffer - the A/B is off");
                g_abOn = false;
                return;
            }
        }
        g_abFrameIdWas = dvr::frameid::enabled() ? 1 : 0;
        g_abLatencyWas = -1;
        if (dev) {
            IDirect3DDevice9Ex* ex = NULL;
            if (SUCCEEDED(dev->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&ex)) && ex) {
                UINT got = 0;
                if (SUCCEEDED(ex->GetMaximumFrameLatency(&got))) g_abLatencyWas = (int)got;
                ex->Release();
            }
        }
        memset(g_abRes, 0, sizeof(g_abRes));
        g_abSeg = 0;
        g_abSegStartMs = nowMs;
        g_abN = 0;
        g_abPrev = 0.0;
        g_abSegDirty = false;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: PLAN STARTED - %d segments of %u ms (%u ms discarded at the head of each). Baseline as "
                "found: frameid %s, max frame latency %d. PLAY NORMALLY AND DO NOT PAUSE; a menu or a load inside "
                "a segment discards it. Nothing about what is rendered changes.",
                kAbSegN, g_abSegMs, g_abWarmMs, g_abFrameIdWas ? "ON" : "off", g_abLatencyWas);
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment 1 of %d: %s", kAbSegN, kAbPlan[0].label);
        return;
    }

    // segment boundary
    if (nowMs - g_abSegStartMs >= (uint64_t)g_abSegMs) {
        AbCloseSegment();
        ++g_abSeg;
        if (g_abSeg >= kAbSegN) {
            AbRestoreBaseline();
            AbSummary();
            g_abDone = true;
            g_abOn = false;
            free(g_abSamples); g_abSamples = NULL;
            return;
        }
        g_abSegStartMs = nowMs;
        g_abN = 0;
        g_abPrev = 0.0;
        g_abSegDirty = false;
        AbRestoreBaseline();                                     // one lever at a time, always from baseline
        AbApply(kAbPlan[g_abSeg].lever, kAbPlan[g_abSeg].value);
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment %d of %d: %s", g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label);
        return;
    }

    // the warm-up: the lever's own switching cost is not charged to it
    if (nowMs - g_abSegStartMs < (uint64_t)g_abWarmMs) { g_abPrev = now; return; }

    if (g_abPrev > 0.0) {
        const float ms = (float)(now - g_abPrev);
        if (ms > 0.0f && ms < 5000.0f && g_abN < DVR_AB_MAX_SAMPLES) g_abSamples[g_abN++] = ms;
    }
    g_abPrev = now;
}

// `perf ab on|off|status|seg <ms>`
bool ab_command(const char* args)
{
    char a[32] = "", b[32] = "";
    const int n = args ? sscanf(args, "%31s %31s", a, b) : 0;
    if (n >= 1 && !strcmp(a, "seg") && n >= 2) {
        const unsigned v = (unsigned)atoi(b);
        if (v >= 5000 && v <= 120000) { g_abSegMs = v; DVR_INFO("perf/ab: segment length %u ms", g_abSegMs); }
        else DVR_INFO("perf/ab: seg <5000..120000 ms> (now %u)", g_abSegMs);
        return true;
    }
    if (n >= 1 && (!strcmp(a, "on") || !strcmp(a, "restart"))) {
        g_abOn = true; g_abDone = false; g_abSeg = -1;
        DVR_INFO("perf/ab: armed - the plan starts at the next present");
        return true;
    }
    if (n >= 1 && !strcmp(a, "off")) {
        if (g_abOn) AbRestoreBaseline();
        g_abOn = false; g_abSeg = -1;
        DVR_INFO("perf/ab: off, baseline restored");
        return true;
    }
    DVR_INFO("perf/ab: %s%s | %d segments of %u ms | perf ab on|off|restart|seg <ms>",
             g_abOn ? "RUNNING" : (g_abDone ? "complete" : "off"),
             g_abOn && g_abSeg >= 0 && g_abSeg < kAbSegN ? "" : "", kAbSegN, g_abSegMs);
    return true;
}

void ab_set_enabled(bool on) { g_abOn = on; g_abDone = false; g_abSeg = -1; g_abWaitLogged = false; }

// From the present path, where the gameplay verdict is already computed.
void ab_set_gameplay(bool inPlay) { g_abGameplay = inPlay; }

} // namespace dvr::perf
