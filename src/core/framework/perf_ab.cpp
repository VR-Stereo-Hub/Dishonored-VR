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
//
// ---- WHAT THE FIRST VERSION GOT WRONG (VR-67 review, 2026-09-09) ---------
//
// Every one of these produced a confident verdict that the data did not carry,
// and they are listed because the class matters more than the instances:
//
//   * The verdict tested p50 ONLY, and was then quoted as covering p99 and the
//     hitch count. The tail is what a frame drop lives in, and it was never
//     compared. The baseline's own over-2x-median share ran 1.68 -> 3.27 ->
//     4.99 % across one run, so THE TAIL NOISE FLOOR IS THREEFOLD, not the
//     3 % the p50 spread suggested. A separate tail floor is now computed and
//     the verdict needs BOTH.
//   * The printed dP99 differenced p99 against the baseline's p50. The
//     +100..140 % figures it produced were an artefact of comparing a tail
//     against a median. It compares p99 against baseline p99 now.
//   * The plan's "max frame latency 3" repeated the baseline, because the
//     baseline WAS 3 and nothing checked. A segment whose value already equals
//     the baseline is now skipped and says so.
//   * Intervals of 5 s or more were silently dropped, so the very stalls being
//     hunted could not appear. They are counted and reported instead.
//   * The samples were consecutive PRESENT intervals under an alternating-eye
//     method, where a short and a long interval alternate by construction, and
//     twice a moving median is not the 25 ms stereo deadline. Pair intervals
//     are now measured between successive completing eyes, and the hitch count
//     is against a FIXED threshold as well as the relative one.
//
// The rule, which is the project's own and was broken anyway: a counter is not
// evidence until you know its population, and an instrument that cannot fail
// its own hypothesis is not evidence.

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
// baseline last so the run ends where it started. A segment whose value already
// equals the baseline is skipped at run time - the first version swept latency
// 1/2/3 against a baseline that was already 3, so a third of the plan measured
// the baseline twice and its spread was read as a lever doing nothing.
static const AbSeg kAbPlan[] = {
    { "baseline",              kAbNone,         0, true  },
    { "frameid readback OFF",  kAbFrameId,      0, false },
    { "baseline again",        kAbNone,         0, true  },
    { "max frame latency 1",   kAbFrameLatency, 1, false },
    { "max frame latency 2",   kAbFrameLatency, 2, false },
    { "baseline last",         kAbNone,         0, true  },
};
static const int kAbSegN = (int)(sizeof(kAbPlan) / sizeof(kAbPlan[0]));

#define DVR_AB_MAX_SAMPLES 16384u

// A PAIR interval - the time between successive completing eyes - is the thing
// with a deadline. A present interval is not: under an alternating-eye method a
// short and a long alternate by construction.
struct AbResult {
    uint32_t n;
    float mean, p50, p95, p99, p999, max;
    uint32_t overTwiceMedian;   // relative, kept for continuity
    uint32_t over40, over50, over75, over100;   // FIXED ms thresholds - a deadline does not move
    uint32_t severe;            // >= 5000 ms: counted, never silently dropped
    bool  valid, skipped;
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
static uint32_t g_abSevere = 0;         // intervals >= 5 s in this segment
static bool     g_abSkipSeg = false;    // this segment repeats the baseline
static uint32_t g_abPresent = 0;        // presents seen in this segment, for pairing
static double   g_abPairPrev = 0.0;
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
    r.severe = g_abSevere;
    if (g_abSkipSeg) {
        r.skipped = true;
        g_abRes[g_abSeg] = r;
        return;
    }
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
        r.p999 = AbPct(g_abSamples, g_abN, 0.999f);
        const float twice = r.p50 * 2.0f;
        for (uint32_t i = 0; i < g_abN; ++i) {
            const float v = g_abSamples[i];
            if (v > twice) ++r.overTwiceMedian;
            // FIXED thresholds too. Twice a moving median is not a deadline, and
            // it moves with the very thing being compared.
            if (v >= 40.0f)  ++r.over40;
            if (v >= 50.0f)  ++r.over50;
            if (v >= 75.0f)  ++r.over75;
            if (v >= 100.0f) ++r.over100;
        }
        r.valid = true;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment %d of %d DONE (%s): %u PAIRS | p50 %.2f ms (%.1f pairs/s) p95 %.2f p99 %.2f "
                "p99.9 %.2f max %.2f | mean %.2f | over fixed thresholds: 40ms=%u 50ms=%u 75ms=%u 100ms=%u | "
                "over twice the median %u (%.2f%%) | severe (>=5 s, excluded from the quantiles) %u",
                g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label, r.n, r.p50,
                r.p50 > 0.0f ? 1000.0f / r.p50 : 0.0f, r.p95, r.p99, r.p999, r.max, r.mean,
                r.over40, r.over50, r.over75, r.over100,
                r.overTwiceMedian, 100.0f * (float)r.overTwiceMedian / (float)r.n, r.severe);
    } else {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: segment %d of %d (%s) collected only %u pairs after the %u ms warm-up - too few to "
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
    float t99Min = 0.0f, t99Max = 0.0f, t99Ref = 0.0f;          // the TAIL floor
    uint32_t hMin = 0, hMax = 0; double hRef = 0.0;             // the fixed-threshold hitch floor
    for (int i = 0; i < kAbSegN; ++i) {
        if (!kAbPlan[i].baseline || !g_abRes[i].valid) continue;
        const AbResult& b = g_abRes[i];
        if (!bN || b.p50 < bMin) bMin = b.p50;
        if (!bN || b.p50 > bMax) bMax = b.p50;
        if (!bN || b.p99 < t99Min) t99Min = b.p99;
        if (!bN || b.p99 > t99Max) t99Max = b.p99;
        if (!bN || b.over50 < hMin) hMin = b.over50;
        if (!bN || b.over50 > hMax) hMax = b.over50;
        bRef += b.p50; t99Ref += b.p99; hRef += (double)b.over50; ++bN;
    }
    if (!bN) {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: PLAN COMPLETE but no baseline segment produced a distribution - nothing can be compared. "
                "Re-run in steady gameplay, not in a menu or a load.");
        return;
    }
    bRef /= (float)bN; t99Ref /= (float)bN; hRef /= (double)bN;
    const float floorPct = bRef > 0.0f ? 100.0f * (bMax - bMin) / bRef : 0.0f;
    const float tailFloorPct = t99Ref > 0.0f ? 100.0f * (t99Max - t99Min) / t99Ref : 0.0f;
    DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
            "perf/ab: PLAN COMPLETE. NOISE FLOOR from %d repeated baseline segment(s) - THE TAIL AND THE MEDIAN "
            "HAVE DIFFERENT FLOORS AND BOTH MUST BE CLEARED: p50 %.2f..%.2f ms (spread %.1f%%, mean %.2f); "
            "p99 %.2f..%.2f ms (spread %.1f%%, mean %.2f); hitches over 50 ms %u..%u (mean %.1f). The tail floor "
            "is the one that matters for a frame drop, and it is usually much wider than the median's - quoting "
            "the median's spread as if it covered the tail is how the first version of this instrument "
            "over-claimed.",
            bN, bMin, bMax, floorPct, bRef, t99Min, t99Max, tailFloorPct, t99Ref, hMin, hMax, hRef);
    if (bN < 2)
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: only ONE baseline segment survived, so there is no noise floor at all and no verdict "
                "below can be trusted. Re-run.");
    for (int i = 0; i < kAbSegN; ++i) {
        const AbResult& r = g_abRes[i];
        if (r.skipped) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab:   %-22s SKIPPED - its value already equals the baseline, so it would have measured "
                    "the baseline a second time", kAbPlan[i].label);
            continue;
        }
        if (!r.valid) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab:   %-22s DISCARDED (too few pairs, or gameplay was lost inside it)", kAbPlan[i].label);
            continue;
        }
        const float dP50 = bRef > 0.0f ? 100.0f * (r.p50 - bRef) / bRef : 0.0f;
        const float dP99 = t99Ref > 0.0f ? 100.0f * (r.p99 - t99Ref) / t99Ref : 0.0f;   // p99 against baseline P99
        const bool medInside = bN >= 2 && floorPct > 0.0f && (dP50 < 0.0f ? -dP50 : dP50) <= floorPct;
        const bool tailInside = bN >= 2 && tailFloorPct > 0.0f && (dP99 < 0.0f ? -dP99 : dP99) <= tailFloorPct;
        const bool hitchInside = bN >= 2 && r.over50 >= hMin && r.over50 <= hMax;
        const char* verdict;
        if (kAbPlan[i].baseline)                verdict = "(baseline)";
        else if (bN < 2)                        verdict = "NO VERDICT - no noise floor";
        else if (medInside && tailInside && hitchInside)
                                                verdict = "NO CHANGE - median, tail and hitch count all inside the floor";
        else if (!tailInside && dP99 < 0.0f)    verdict = "TAIL IMPROVED - the only column that matters for a drop; REPEAT IT before believing it";
        else if (!tailInside)                   verdict = "TAIL WORSE";
        else if (!medInside && dP50 < 0.0f)     verdict = "median faster, TAIL UNCHANGED - this does not fix a hitch";
        else                                    verdict = "median moved, tail inside the floor";
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab:   %-22s p50 %.2f (%+.1f%% vs base p50) p95 %.2f p99 %.2f (%+.1f%% vs base p99) "
                "p99.9 %.2f max %.2f | 50ms+ %u (base %.1f) 100ms+ %u | severe %u | %s",
                kAbPlan[i].label, r.p50, dP50, r.p95, r.p99, dP99, r.p999, r.max,
                r.over50, hRef, r.over100, r.severe, verdict);
    }
    DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
            "perf/ab: baseline restored. Every interval above is a complete STEREO PAIR, which is the thing with a "
            "deadline - a present interval is not, because a short and a long alternate by construction. The "
            "frame-drop columns are p99, p99.9 and the fixed 50ms+/100ms+ counts; p50 is throughput and can "
            "improve while the hitches get worse. A segment lasts %u ms, so a rare event is counted in single "
            "digits and ONE run does not settle anything - a lever that looks better here needs the plan run "
            "again, preferably with the order reversed.",
            g_abSegMs);
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
        g_abPairPrev = 0.0;
        g_abPresent = 0;
        g_abSevere = 0;
        g_abSegDirty = false;
        g_abSkipSeg = false;
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
        g_abPairPrev = 0.0;
        g_abPresent = 0;
        g_abSevere = 0;
        g_abSegDirty = false;
        AbRestoreBaseline();                                     // one lever at a time, always from baseline
        // A segment whose value already IS the baseline measures the baseline
        // twice and reads as "the lever did nothing". Skip it and say so.
        g_abSkipSeg = (kAbPlan[g_abSeg].lever == kAbFrameLatency && kAbPlan[g_abSeg].value == g_abLatencyWas)
                   || (kAbPlan[g_abSeg].lever == kAbFrameId && (kAbPlan[g_abSeg].value != 0) == (g_abFrameIdWas != 0));
        if (g_abSkipSeg) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab: segment %d of %d SKIPPED (%s) - that value already equals the baseline, so the "
                    "segment would measure the baseline a second time and its spread would be read as the lever "
                    "doing nothing",
                    g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label);
            return;
        }
        AbApply(kAbPlan[g_abSeg].lever, kAbPlan[g_abSeg].value);
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment %d of %d: %s", g_abSeg + 1, kAbSegN, kAbPlan[g_abSeg].label);
        return;
    }

    // the warm-up: the lever's own switching cost is not charged to it
    if (g_abSkipSeg) return;
    if (nowMs - g_abSegStartMs < (uint64_t)g_abWarmMs) {
        g_abPrev = now; g_abPairPrev = 0.0; g_abPresent = 0; return;
    }

    // A STEREO PAIR is the thing with a deadline; a single present is not.
    // Consecutive PRESENT intervals alternate short and long BY CONSTRUCTION
    // under a two-present method, so their median is a mixture and twice it is
    // not a deadline. That was the first version's mistake and it made the
    // hitch column unreadable. Every second present closes a pair (the run's
    // own `untagged 0` says the alternation is unbroken), so pairing by count
    // needs no eye tag plumbed down here.
    g_abPrev = now;
    if ((++g_abPresent & 1u) != 0u) return;   // first present of a pair
    if (g_abPairPrev > 0.0) {
        const float ms = (float)(now - g_abPairPrev);
        if (ms >= 5000.0f) ++g_abSevere;   // counted, never silently dropped
        else if (ms > 0.0f && g_abN < DVR_AB_MAX_SAMPLES) g_abSamples[g_abN++] = ms;
    }
    g_abPairPrev = now;
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
