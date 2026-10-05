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
    kAbSeam,           // uncap deep dive: a plan file row - seam commands to apply and to restore
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
static const int kAbBuiltinN = (int)(sizeof(kAbPlan) / sizeof(kAbPlan[0]));
static uint32_t g_abSegMs = 20000;
static uint32_t g_abWarmMs = 2000;      // discarded at the head of every segment

// ---- the plan FILE (uncap deep dive, 2026-09-27) -----------------------------
//
// `perf ab plan <file>` (or `[Perf] AbPlan=<file>` at launch) replaces the built-in plan with
// rows read from a text file in the data dir, one segment per line:
//
//     label | apply commands | restore commands
//
// Commands are seam words, several separated by ';'. A row with no apply commands is a
// BASELINE. The restore of a row runs when its segment ends, before the next segment
// applies anything, so every alternative is measured from the baseline. `seg <ms>` and
// `warm <ms>` lines set the segment and warm-up lengths. `#` starts a comment.
// Example: `depth 2 | capture depth 2 | capture depth 1`.
//
// Everything below the plan reads rows through these accessors, so the built-in plan
// and a file plan are measured, verdicted and summarised identically.
// Pre-release audit (2026-10-04): 64 rows so one plan can hold every lever of a whole audit, and two
// more directives. `delay <ms>` waits that long in gameplay before the first segment (time to walk
// to the spot after a load). `atend <seam words>` runs once after the summary: a visible "finished"
// (`overlay on` opens the F10 panel) and the switching-off of anything a row left on.
// Second audit run (2026-10-05): the first one was measured in the wrong configuration, because the
// F10 panel was opened in the lead-in and rewrote the stereo method and DLSS under the plan, and the
// rows' fixed restore words then changed the state again. Three more directives, all optional:
//   `atstart <seam words>`  runs once when gameplay is first reached (the start of the delay), so
//                           a resize it causes has settled before the first segment;
//   `expect <key> <value>`  checked when the plan starts: keys `stereo` (a method name), `dlss`
//                           (on|off), `dlssmodel` (fast|k). A mismatch REFUSES the plan, says why
//                           with both values and runs `atend`, so a wrong run ends after a minute;
//   `holdpanel`             keeps the F10 panel closed from gameplay to the plan's end.
// And a segment is no longer discarded for gameplay lost inside its WARM-UP: the warm-up exists for
// the lever's own switching cost, and a DLSS resize is one.
#define DVR_AB_MAX_ROWS 64
static uint32_t g_abDelayMs = 0;
static uint64_t g_abFirstGameplayMs = 0;
static char     g_abAtEnd[200] = "";
static char     g_abAtStart[200] = "";
static bool     g_abAtStartRan = false;
static bool     g_abHoldPanel = false;
static uint32_t g_abPanelClosed = 0;
struct AbExpect { char key[16]; char value[32]; };
static AbExpect g_abExpect[8];
static int      g_abExpectN = 0;
struct AbRow {
    char label[48];
    char apply[200];
    char restore[200];
    bool baseline;
};
static AbRow g_abRows[DVR_AB_MAX_ROWS];
static int   g_abRowN = 0;          // 0 = the built-in plan
static char  g_abPlanName[MAX_PATH] = "";

static int         AbSegN()               { return g_abRowN ? g_abRowN : kAbBuiltinN; }
static const char* AbLabel(int i)         { return g_abRowN ? g_abRows[i].label : kAbPlan[i].label; }
static bool        AbIsBaseline(int i)    { return g_abRowN ? g_abRows[i].baseline : kAbPlan[i].baseline; }
static AbLever     AbLeverOf(int i)       { return g_abRowN ? (g_abRows[i].baseline ? kAbNone : kAbSeam) : kAbPlan[i].lever; }
static int         AbValueOf(int i)       { return g_abRowN ? 0 : kAbPlan[i].value; }

static void AbTrim(char* t)
{
    char* a = t;
    while (*a == ' ' || *a == '\t') ++a;
    if (a != t) memmove(t, a, strlen(a) + 1);
    size_t n = strlen(t);
    while (n && (t[n - 1] == ' ' || t[n - 1] == '\t' || t[n - 1] == '\r' || t[n - 1] == '\n')) t[--n] = 0;
}

// Each ';'-separated command through the seam's own dispatcher, on this (present) thread.
static volatile LONG g_abDispatching = 0;
static void AbRunCommands(const char* list, const char* why)
{
    if (!list || !list[0]) return;
    struct Guard { Guard() { InterlockedExchange(&g_abDispatching, 1); } ~Guard() { InterlockedExchange(&g_abDispatching, 0); } } guard;
    char buf[200];
    strncpy(buf, list, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
    char* ctx = NULL;
    for (char* c = strtok_s(buf, ";", &ctx); c; c = strtok_s(NULL, ";", &ctx)) {
        AbTrim(c);
        if (!c[0]) continue;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info, "perf/ab: %s -> `%s`", why, c);
        dvr::command::dispatch_line(c);
    }
}

static bool AbLoadPlan(const char* name)
{
    char path[MAX_PATH];
    if (strchr(name, ':') || name[0] == '\\' || name[0] == '/') { strncpy(path, name, MAX_PATH - 1); path[MAX_PATH - 1] = 0; }
    else dvr::paths::in_data_dir(path, name);
    FILE* f = fopen(path, "r");
    if (!f) {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: plan file %s could not be opened - the built-in plan stays", path);
        return false;
    }
    int n = 0;
    char line[512];
    g_abDelayMs = 0; g_abAtEnd[0] = 0; g_abFirstGameplayMs = 0;
    g_abAtStart[0] = 0; g_abAtStartRan = false; g_abHoldPanel = false; g_abPanelClosed = 0; g_abExpectN = 0;
    while (fgets(line, sizeof(line), f)) {
        AbTrim(line);
        if (!line[0] || line[0] == '#') continue;
        unsigned v = 0;
        if (sscanf(line, "seg %u", &v) == 1) { if (v >= 5000 && v <= 120000) g_abSegMs = v; continue; }
        if (sscanf(line, "warm %u", &v) == 1) { if (v <= 10000) g_abWarmMs = v; continue; }
        if (sscanf(line, "delay %u", &v) == 1) { if (v <= 600000) g_abDelayMs = v; continue; }
        if (!strncmp(line, "atend ", 6)) { strncpy(g_abAtEnd, line + 6, sizeof(g_abAtEnd) - 1); g_abAtEnd[sizeof(g_abAtEnd) - 1] = 0; AbTrim(g_abAtEnd); continue; }
        if (!strncmp(line, "atstart ", 8)) { strncpy(g_abAtStart, line + 8, sizeof(g_abAtStart) - 1); g_abAtStart[sizeof(g_abAtStart) - 1] = 0; AbTrim(g_abAtStart); continue; }
        if (!strcmp(line, "holdpanel")) { g_abHoldPanel = true; continue; }
        if (!strncmp(line, "expect ", 7)) {
            if (g_abExpectN < 8) {
                AbExpect& e = g_abExpect[g_abExpectN];
                memset(&e, 0, sizeof(e));
                if (sscanf(line + 7, "%15s %31s", e.key, e.value) == 2) ++g_abExpectN;
            }
            continue;
        }
        if (n >= DVR_AB_MAX_ROWS) break;
        AbRow& r = g_abRows[n];
        memset(&r, 0, sizeof(r));
        char* bar1 = strchr(line, '|');
        char* bar2 = bar1 ? strchr(bar1 + 1, '|') : NULL;
        if (bar1) *bar1 = 0;
        if (bar2) *bar2 = 0;
        strncpy(r.label, line, sizeof(r.label) - 1); AbTrim(r.label);
        if (bar1) { strncpy(r.apply, bar1 + 1, sizeof(r.apply) - 1); AbTrim(r.apply); }
        if (bar2) { strncpy(r.restore, bar2 + 1, sizeof(r.restore) - 1); AbTrim(r.restore); }
        r.baseline = r.apply[0] == 0;
        if (!r.label[0]) strcpy(r.label, r.baseline ? "baseline" : "(unnamed)");
        ++n;
    }
    fclose(f);
    int baselines = 0;
    for (int i = 0; i < n; ++i) baselines += g_abRows[i].baseline ? 1 : 0;
    if (n < 2 || baselines < 2) {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: plan %s has %d row(s) and %d baseline(s) - a plan needs at least two baselines for a noise "
                "floor; the built-in plan stays", path, n, baselines);
        g_abRowN = 0;
        return false;
    }
    g_abRowN = n;
    strncpy(g_abPlanName, path, MAX_PATH - 1); g_abPlanName[MAX_PATH - 1] = 0;
    DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
            "perf/ab: plan LOADED from %s - %d segments (%d baselines) of %u ms, %u ms warm-up each; it starts %u ms "
            "after gameplay begins and takes about %.1f minutes; at the end it runs `%s`; when gameplay is reached it "
            "runs `%s`; %d expectation(s) checked at the start; F10 panel %s:",
            path, n, baselines, g_abSegMs, g_abWarmMs, g_abDelayMs, (g_abDelayMs + (double)n * g_abSegMs) / 60000.0, g_abAtEnd,
            g_abAtStart, g_abExpectN, g_abHoldPanel ? "HELD CLOSED from gameplay to the end" : "not held");
    for (int i = 0; i < n; ++i)
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info, "perf/ab:   %2d %-24s apply `%s` restore `%s`",
                i + 1, g_abRows[i].label, g_abRows[i].apply, g_abRows[i].restore);
    return true;
}

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
static int      g_abSeg = -1;
static uint64_t g_abSegStartMs = 0;
static double   g_abPrev = 0.0;
static float*   g_abSamples = NULL;
static uint32_t g_abN = 0;
static AbResult g_abRes[DVR_AB_MAX_ROWS];
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

static void AbApplySeg(int i)
{
    if (g_abRowN) { AbRunCommands(g_abRows[i].apply, "apply"); return; }
    AbApply(kAbPlan[i].lever, kAbPlan[i].value);
}

// A file row's own restore, run when its segment ends (the built-in levers restore through
// AbRestoreBaseline below, which re-applies the values captured when the plan started).
static void AbRestoreSeg(int i)
{
    if (g_abRowN && i >= 0 && i < g_abRowN && !g_abRows[i].baseline) AbRunCommands(g_abRows[i].restore, "restore");
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
    if (g_abSeg < 0 || g_abSeg >= AbSegN()) return;
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
                g_abSeg + 1, AbSegN(), AbLabel(g_abSeg), g_abN);
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
                g_abSeg + 1, AbSegN(), AbLabel(g_abSeg), r.n, r.p50,
                r.p50 > 0.0f ? 1000.0f / r.p50 : 0.0f, r.p95, r.p99, r.p999, r.max, r.mean,
                r.over40, r.over50, r.over75, r.over100,
                r.overTwiceMedian, 100.0f * (float)r.overTwiceMedian / (float)r.n, r.severe);
    } else {
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: segment %d of %d (%s) collected only %u pairs after the %u ms warm-up - too few to "
                "quote a distribution, this segment is DISCARDED (were you in a menu or a load?)",
                g_abSeg + 1, AbSegN(), AbLabel(g_abSeg), g_abN, g_abWarmMs);
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
    for (int i = 0; i < AbSegN(); ++i) {
        if (!AbIsBaseline(i) || !g_abRes[i].valid) continue;
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
    for (int i = 0; i < AbSegN(); ++i) {
        const AbResult& r = g_abRes[i];
        if (r.skipped) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab:   %-22s SKIPPED - its value already equals the baseline, so it would have measured "
                    "the baseline a second time", AbLabel(i));
            continue;
        }
        if (!r.valid) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab:   %-22s DISCARDED (too few pairs, or gameplay was lost inside it)", AbLabel(i));
            continue;
        }
        const float dP50 = bRef > 0.0f ? 100.0f * (r.p50 - bRef) / bRef : 0.0f;
        const float dP99 = t99Ref > 0.0f ? 100.0f * (r.p99 - t99Ref) / t99Ref : 0.0f;   // p99 against baseline P99
        const bool medInside = bN >= 2 && floorPct > 0.0f && (dP50 < 0.0f ? -dP50 : dP50) <= floorPct;
        const bool tailInside = bN >= 2 && tailFloorPct > 0.0f && (dP99 < 0.0f ? -dP99 : dP99) <= tailFloorPct;
        const bool hitchInside = bN >= 2 && r.over50 >= hMin && r.over50 <= hMax;
        const char* verdict;
        if (AbIsBaseline(i))                verdict = "(baseline)";
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
                AbLabel(i), r.p50, dP50, r.p95, r.p99, dP99, r.p999, r.max,
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

// What the session is configured as, in the plan's own words. The found value is always logged, so
// a run's identity is on its PLAN STARTED line and not inferred afterwards.
static bool AbCheckExpectations(char* found, size_t foundSize)
{
    const char* stereo = dvr::stereo::active_name();
    const char* dlss = dvr::dlss::mode() != 0 ? "on" : "off";
    const char* model = dvr::dlss::model() == 1 ? "fast" : dvr::dlss::model() == 0 ? "k" : "other";
    _snprintf(found, foundSize, "stereo %s, dlss %s (mode %d, %s), dlssmodel %s (model %d preset %d)", stereo ? stereo : "?", dlss,
              dvr::dlss::mode(), dvr::dlss::quality_name(dvr::dlss::quality()), model, dvr::dlss::model(), dvr::dlss::preset());
    found[foundSize - 1] = 0;
    bool ok = true;
    for (int i = 0; i < g_abExpectN; ++i) {
        const AbExpect& e = g_abExpect[i];
        const char* have = !strcmp(e.key, "stereo") ? (stereo ? stereo : "?") : !strcmp(e.key, "dlss") ? dlss
                         : !strcmp(e.key, "dlssmodel") ? model : NULL;
        if (!have) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn, "perf/ab: expect `%s %s` - unknown key (stereo, dlss, dlssmodel)", e.key, e.value);
            ok = false;
        } else if (_stricmp(have, e.value)) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn, "perf/ab: expect `%s %s` FAILED - the session has `%s`", e.key, e.value, have);
            ok = false;
        }
    }
    return ok;
}

// Called from hkPresent, at the entry stamp, on the present thread.
void ab_tick(IDirect3DDevice9* dev)
{
    if (!g_abOn || g_abDone) return;
    g_abDevice = dev;

    // The F10 panel rewrites the configuration a plan is measuring (the first audit run lost its
    // configuration to a stick-click in the lead-in). While a plan asks for it, the panel stays shut
    // from the first gameplay to the plan's end. Before the gameplay test: an open panel is not gameplay.
    if (g_abHoldPanel && g_abRowN && (g_abFirstGameplayMs || g_abSeg >= 0) && g_ovlVisible) {
        g_ovlVisible = false;
        ++g_abPanelClosed;
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Warn,
                "perf/ab: the F10 panel was opened while a plan is %s and has been CLOSED again (%u time(s)): its tabs write the "
                "configuration the plan measures. It opens by itself when the plan ends.",
                g_abSeg >= 0 ? "running" : "in its lead-in", g_abPanelClosed);
    }

    // The plan measures GAMEPLAY. Starting it in the menu would spend every
    // segment on a screen that does not render the game, and the numbers would
    // be a comparison of menus. Losing gameplay inside a segment poisons that
    // segment rather than the whole plan.
    if (!g_abGameplay) {
        // lost in the warm-up = the lever's own switching (a DLSS resize); lost after it = a dirty segment
        if (g_abSeg >= 0 && GetTickCount64() - g_abSegStartMs >= (uint64_t)g_abWarmMs) g_abSegDirty = true;
        else if (!g_abWaitLogged) {
            g_abWaitLogged = true;
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab: armed, WAITING FOR GAMEPLAY - the plan starts at the first present after the game "
                    "is in play, not in the menu. %d segments of %u ms; just play normally when you get in.",
                    AbSegN(), g_abSegMs);
        }
        return;
    }

    const double now = dvr::clock::now_ms();
    const uint64_t nowMs = GetTickCount64();

    if (g_abSeg < 0 && g_abRowN && g_abDelayMs) {   // a file plan's start delay, counted in gameplay
        if (!g_abFirstGameplayMs) {
            g_abFirstGameplayMs = nowMs;
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab: gameplay reached - the plan starts in %u ms (get to the spot and stand still)", g_abDelayMs);
            if (g_abAtStart[0] && !g_abAtStartRan) { g_abAtStartRan = true; AbRunCommands(g_abAtStart, "plan lead-in"); }
        }
        if (nowMs - g_abFirstGameplayMs < (uint64_t)g_abDelayMs) return;
    }
    if (g_abSeg < 0) {   // the plan starts here: remember what to restore
        if (g_abRowN) {
            if (g_abAtStart[0] && !g_abAtStartRan) { g_abAtStartRan = true; AbRunCommands(g_abAtStart, "plan start"); }
            char found[200];
            const bool ok = AbCheckExpectations(found, sizeof(found));
            DVR_LOG(dvr::log::Cat::perf, ok ? dvr::log::Level::Info : dvr::log::Level::Warn,
                    "perf/ab: configuration at the start: %s | %d expectation(s) %s", found, g_abExpectN,
                    ok ? "met" : "NOT met - PLAN REFUSED: nothing is measured, because every row would be measured in a "
                         "configuration the plan was not written for. Set the configuration and arm the plan again.");
            if (!ok) {
                if (g_abAtEnd[0]) AbRunCommands(g_abAtEnd, "plan refused");
                g_abDone = true; g_abOn = false;
                return;
            }
        }
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
                AbSegN(), g_abSegMs, g_abWarmMs, g_abFrameIdWas ? "ON" : "off", g_abLatencyWas);
        if (!AbIsBaseline(0)) AbApplySeg(0);
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment 1 of %d: %s", AbSegN(), AbLabel(0));
        dvr::etw::mark("ab seg 1", 1);
        return;
    }

    // segment boundary
    if (nowMs - g_abSegStartMs >= (uint64_t)g_abSegMs) {
        AbCloseSegment();
        AbRestoreSeg(g_abSeg);
        ++g_abSeg;
        if (g_abSeg >= AbSegN()) {
            AbRestoreBaseline();
            AbSummary();
            if (g_abRowN && g_abAtEnd[0]) AbRunCommands(g_abAtEnd, "plan end");
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
        g_abSkipSeg = (AbLeverOf(g_abSeg) == kAbFrameLatency && AbValueOf(g_abSeg) == g_abLatencyWas)
                   || (AbLeverOf(g_abSeg) == kAbFrameId && (AbValueOf(g_abSeg) != 0) == (g_abFrameIdWas != 0));
        if (g_abSkipSeg) {
            DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                    "perf/ab: segment %d of %d SKIPPED (%s) - that value already equals the baseline, so the "
                    "segment would measure the baseline a second time and its spread would be read as the lever "
                    "doing nothing",
                    g_abSeg + 1, AbSegN(), AbLabel(g_abSeg));
            return;
        }
        AbApplySeg(g_abSeg);
        DVR_LOG(dvr::log::Cat::perf, dvr::log::Level::Info,
                "perf/ab: segment %d of %d: %s", g_abSeg + 1, AbSegN(), AbLabel(g_abSeg));
        {
            char etwText[96];
            _snprintf(etwText, sizeof(etwText), "ab seg %d %s", g_abSeg + 1, AbLabel(g_abSeg));
            etwText[sizeof(etwText) - 1] = 0;
            dvr::etw::mark(etwText, g_abSeg + 1);
        }
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
    if (n >= 2 && !strcmp(a, "plan")) {
        if (g_abOn && g_abSeg >= 0) { AbRestoreSeg(g_abSeg); AbRestoreBaseline(); }
        if (AbLoadPlan(b)) { g_abOn = true; g_abDone = false; g_abSeg = -1; g_abWaitLogged = false;
                             DVR_INFO("perf/ab: armed with %s - the plan starts at the next gameplay present", b); }
        return true;
    }
    if (n >= 1 && !strcmp(a, "builtin")) { g_abRowN = 0; DVR_INFO("perf/ab: the built-in plan (%d segments)", kAbBuiltinN); return true; }
    if (n >= 1 && (!strcmp(a, "on") || !strcmp(a, "restart"))) {
        g_abOn = true; g_abDone = false; g_abSeg = -1;
        DVR_INFO("perf/ab: armed - the plan starts at the next present");
        return true;
    }
    if (n >= 1 && !strcmp(a, "off")) {
        if (g_abOn && g_abSeg >= 0) AbRestoreSeg(g_abSeg);
        if (g_abOn) AbRestoreBaseline();
        g_abOn = false; g_abSeg = -1;
        DVR_INFO("perf/ab: off, baseline restored");
        return true;
    }
    DVR_INFO("perf/ab: %s | %d segments of %u ms (%s) | perf ab on|off|restart|seg <ms>|plan <file>|builtin",
             g_abOn ? "RUNNING" : (g_abDone ? "complete" : "off"), AbSegN(), g_abSegMs,
             g_abRowN ? g_abPlanName : "built-in plan");
    return true;
}

bool ab_dispatching() { return InterlockedCompareExchange(&g_abDispatching, 0, 0) != 0; }
void ab_set_enabled(bool on) { g_abOn = on; g_abDone = false; g_abSeg = -1; g_abWaitLogged = false; }
// [Perf] AbPlan=<file>: load a plan file and arm it (the plan still waits for gameplay).
void ab_load_plan(const char* name) {
    if (!name || !name[0]) return;
    if (AbLoadPlan(name)) { g_abOn = true; g_abDone = false; g_abSeg = -1; g_abWaitLogged = false; }
}

// From the present path, where the gameplay verdict is already computed.
void ab_set_gameplay(bool inPlay) { g_abGameplay = inPlay; dvr::native_profile::tick(inPlay); dvr::bridge_profile::set_gameplay(inPlay); dvr::diag_ab::tick(inPlay); }

} // namespace dvr::perf
