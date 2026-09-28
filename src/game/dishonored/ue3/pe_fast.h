// game/dishonored/ue3/pe_fast.h - the script lane's own cost (PERFORMANCE.md, route 2), unity build.
//
// The ProcessEvent hook runs for every script event on the GAME thread, which is the frame-rate
// ceiling on a CPU-bound PC (~7 ms per tick on a Ryzen 5 5600X). Two costs of ours repeat on every
// event and never change their answer:
//   1. readability checks: RangeReadable is a VirtualQuery system call, several per event (the
//      object, its function, the name table entry);
//   2. event-name tests: RealName plus dozens of strstr/strcmp to classify an FName that is the
//      same string every time it recurs.
// The fast path answers both from caches: a small table of committed regions (cleared every
// second, so a region freed since is re-checked within a second; the hook only asks about the
// objects the engine is dispatching on) and an FName-index -> traits table (FName indices and
// their strings never change). The DECISIONS are the same functions either way; only the cost
// differs. [Perf] PeFast=1 (default), `pe fast on|off` A/B live, and the `pe/cost` line every
// 5 s splits the hook's time by section so the next cost can be named, not guessed.
#pragma once
#include <stdint.h>

// ---- the instrument ----------------------------------------------------------------------
enum PeSection { kPeFront = 0, kPeMid, kPeCam, kPeNames, kPeTail, kPeSections };
static const char* const kPeSectionName[kPeSections] = {"front ticks", "mid ticks", "camera/aim", "name tests", "view rotation+hands"};
static volatile LONG g_peFast = 1;              // [Perf] PeFast
static void PeFastSet(bool on) {
    if ((InterlockedExchange(&g_peFast, on ? 1 : 0) != 0) != on)
        Log("pe: script-lane fast path %s ([Perf] PeFast)", on ? "ON" : "off");
}
static LONGLONG g_peSecTicks[kPeSections] = {};
static uint64_t g_peCostCalls = 0, g_peVq = 0, g_peVqSaved = 0, g_peNameMiss = 0, g_peNameHit = 0;
static LONGLONG g_peCostT0 = 0;

// ---- the heavy writers' cadence ----------------------------------------------------------------
// FovLeverApply (the FOV lever and the eye clamp) and camera::apply_offsets write engine fields on
// every script event so that OUR value is the last one written before the draw: the engine
// recomputes those fields during the tick. The measured cost was ~210 ms of the game thread per
// second (pe/cost-fn, simulator 2026-09-27) - ~45 repeats per tick of a write only the draw reads.
// With the re-entry's draw hook installed the same guarantee holds with far fewer repeats: at most
// every PeHeavyMs outside the draw, ALWAYS once at the viewport-draw entry (after every script
// event of the tick, DvrViewportDrawStub), and ALWAYS for events inside the draw. Without the hook
// (another method, a menu) every event runs them, as before. [Perf] PeHeavyMs=2, `pe heavy <ms>`,
// 0 = every event.
static volatile LONG g_peHeavyUs = 2000;
// Events INSIDE the viewport draw run the heavy writers every time unless this is off: the draw
// dispatches ~2,400 script events a second of its own (PostRender, the HUD), and whether the engine
// recomputes the FOV or the camera there is what `pe heavydraw off` measures. [Perf] PeHeavyInDraw=1.
static volatile LONG g_peHeavyInDraw = 1;
static void PeHeavyInDrawSet(bool every) { InterlockedExchange(&g_peHeavyInDraw, every ? 1 : 0); }
static LONGLONG g_peHeavyLast = 0;
static uint64_t g_peHeavyRuns = 0, g_peHeavySkips = 0, g_peHeavyAtDraw = 0;
static void PeHeavySet(int ms) {
    if (ms < 0) ms = 0;
    if (ms > 50) ms = 50;
    if (InterlockedExchange(&g_peHeavyUs, ms * 1000) != ms * 1000)
        Log("pe: heavy script-lane writers at most every %d ms outside the draw%s (always at the draw entry and inside it)",
            ms, ms ? "" : " - 0 = every event, the old cadence");
}
static bool PeHeavyThrottled()
{
    return InterlockedCompareExchange(&g_peHeavyUs, 0, 0) > 0 && g_sdInstalled &&
           InterlockedCompareExchange(&g_sdArmed, 0, 0) != 0 && !g_sdPoisoned;
}
static bool PeHeavyDue()
{
    if (!PeHeavyThrottled() ||
        (InterlockedCompareExchange(&g_peHeavyInDraw, 0, 0) && InterlockedCompareExchange(&g_sdDepth, 0, 0) > 0)) {
        ++g_peHeavyRuns; return true;
    }
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    const LONGLONG period = (LONGLONG)InterlockedCompareExchange(&g_peHeavyUs, 0, 0) * (g_qpcFreq ? g_qpcFreq : 1) / 1000000;
    if (t.QuadPart - g_peHeavyLast >= period) { g_peHeavyLast = t.QuadPart; ++g_peHeavyRuns; return true; }
    ++g_peHeavySkips;
    return false;
}
// The draw entry: game thread, after the tick's script events, before pass 1's camera is read.
static void PeHeavyAtDraw()
{
    if (!PeHeavyThrottled()) return;
    ++g_peHeavyAtDraw;
    FovLeverApply();
    dvr::camera::apply_offsets(g_camObj);
}

// ---- readability ---------------------------------------------------------------------------
struct PeRegion { uintptr_t lo, hi; };
static PeRegion g_peRegions[32];
static int g_peRegionN = 0, g_peRegionNext = 0;
static uint64_t g_peRegionMs = 0;

static bool PeReadable(const void* p, size_t n)
{
    if (!InterlockedCompareExchange(&g_peFast, 0, 0)) { ++g_peVq; return RangeReadable(p, n); }
    const uintptr_t a = (uintptr_t)p;
    if (!p || a < 0x10000 || a + n < a) return false;
    const uint64_t now = GetTickCount64();
    if (now - g_peRegionMs > 1000) { g_peRegionN = 0; g_peRegionNext = 0; g_peRegionMs = now; }
    for (int i = 0; i < g_peRegionN; ++i)
        if (a >= g_peRegions[i].lo && a + n <= g_peRegions[i].hi) { ++g_peVqSaved; return true; }
    ++g_peVq;
    if (!RangeReadable(p, n)) return false;
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(p, &m, sizeof(m)) && a + n <= (uintptr_t)m.BaseAddress + m.RegionSize) {
        PeRegion& r = g_peRegions[g_peRegionNext];
        r.lo = (uintptr_t)m.BaseAddress; r.hi = r.lo + m.RegionSize;
        g_peRegionNext = (g_peRegionNext + 1) & 31;
        if (g_peRegionN < 32) ++g_peRegionN;
    }
    return true;
}

// ---- event-name traits ---------------------------------------------------------------------
enum : uint32_t {
    kPtFireArm      = 1u << 0,   // UseSecondaryItem | Fire
    kPtMainVerb     = 1u << 1,   // Start OnFocusGained BackToStartScreen Req_CanContinueGame Unregister.. OnFocusLost
    kPtMainLeave    = 1u << 2,   // UnregisterControllerDelegates | OnFocusLost
    kPtUiActivity   = 1u << 3,   // PauseMenu PauseGame CanLoadGame CanSaveGame SaveSlotInfos LoadGameClicked MessageBox BackToWindows Wheel_Open
    kPtLoadClicked  = 1u << 4,   // LoadGameClicked
    kPtMenuClose    = 1u << 5,   // MenuClosed | ResumeGameClicked | NewGameClicked
    kPtMenuOpen     = 1u << 6,   // OpenPauseMenu MessageBox CanSaveGame SaveSlotInfos BackToWindows LoadGameClicked
    kPtCloseJournal = 1u << 7,
    kPtToggleJournal= 1u << 8,
    kPtShop         = 1u << 9,   // Shop | Upgrade | PurchasesList
    kPtShopClose    = 1u << 10,  // Close | Exit | Leave
    kPtCineToggle   = 1u << 11,  // OnToggleCinematicMode
    kPtPreExit      = 1u << 12,  // PreExit
    kPtBoatF        = 1u << 13,  // the dev-only forensic list
    kPtVolume       = 1u << 14,  // PawnEnteredVolume | PawnLeavingVolume
    kPtVersus       = 1u << 15,
    kPtFinisher     = 1u << 16,  // (Assassin|Takedown|Execut|Fatal|Kill) and not Particle
    kPtVocab        = 1u << 17,  // Dis_* / On* / Req_*
    kPtPostRender   = 1u << 18,  // PostRender
    kPtNamed        = 1u << 31,  // the index resolved to a real name
};

static uint32_t PeTraitsOf(const char* nm)
{
    if (!nm) return 0;
    uint32_t t = kPtNamed;
    if (strstr(nm, "UseSecondaryItem") || strstr(nm, "Fire")) t |= kPtFireArm;
    if (!strcmp(nm, "Start") || !strcmp(nm, "OnFocusGained") || !strcmp(nm, "BackToStartScreen") ||
        !strcmp(nm, "Req_CanContinueGame") || !strcmp(nm, "UnregisterControllerDelegates") || !strcmp(nm, "OnFocusLost"))
        t |= kPtMainVerb;
    if (!strcmp(nm, "UnregisterControllerDelegates") || !strcmp(nm, "OnFocusLost")) t |= kPtMainLeave;
    if (strstr(nm, "PauseMenu") || strstr(nm, "PauseGame") || strstr(nm, "CanLoadGame") || strstr(nm, "CanSaveGame") ||
        strstr(nm, "SaveSlotInfos") || strstr(nm, "LoadGameClicked") || strstr(nm, "MessageBox") ||
        strstr(nm, "BackToWindows") || strstr(nm, "Wheel_Open"))
        t |= kPtUiActivity;
    if (strstr(nm, "LoadGameClicked")) t |= kPtLoadClicked;
    if (strstr(nm, "MenuClosed") || strstr(nm, "ResumeGameClicked") || strstr(nm, "NewGameClicked")) t |= kPtMenuClose;
    if (strstr(nm, "OpenPauseMenu") || strstr(nm, "MessageBox") || strstr(nm, "CanSaveGame") ||
        strstr(nm, "SaveSlotInfos") || strstr(nm, "BackToWindows") || strstr(nm, "LoadGameClicked"))
        t |= kPtMenuOpen;
    if (strstr(nm, "CloseJournal")) t |= kPtCloseJournal;
    if (strstr(nm, "ToggleJournal")) t |= kPtToggleJournal;
    if (strstr(nm, "Shop") || strstr(nm, "Upgrade") || strstr(nm, "PurchasesList")) t |= kPtShop;
    if (strstr(nm, "Close") || strstr(nm, "Exit") || strstr(nm, "Leave")) t |= kPtShopClose;
    if (!strcmp(nm, "OnToggleCinematicMode")) t |= kPtCineToggle;
    if (!strcmp(nm, "PreExit")) t |= kPtPreExit;
    if (!strcmp(nm, "PawnEnteredVolume") || !strcmp(nm, "PawnLeavingVolume") || !strcmp(nm, "ChooseAndTriggerDeathEvent") ||
        !strcmp(nm, "PlayDying") || !strcmp(nm, "NotifyKilled") || !strcmp(nm, "PreventDeath") || !strcmp(nm, "BaseChange") ||
        !strcmp(nm, "Destroyed") || !strcmp(nm, "OnToggleHidden") || !strcmp(nm, "OnNPCMarkForVanish") ||
        !strcmp(nm, "Dis_ExitKeyhole") || !strcmp(nm, "OnObjectiveAction") || !strcmp(nm, "OnToggleCinematicMode"))
        t |= kPtBoatF;
    if (!strcmp(nm, "PawnEnteredVolume") || !strcmp(nm, "PawnLeavingVolume")) t |= kPtVolume;
    if (strstr(nm, "Versus")) t |= kPtVersus;
    if ((strstr(nm, "Assassin") || strstr(nm, "Takedown") || strstr(nm, "Execut") || strstr(nm, "Fatal") ||
         strstr(nm, "Kill")) && !strstr(nm, "Particle"))
        t |= kPtFinisher;
    if ((nm[0] == 'D' && nm[1] == 'i' && nm[2] == 's' && nm[3] == '_') || (nm[0] == 'O' && nm[1] == 'n') ||
        (nm[0] == 'R' && nm[1] == 'e' && nm[2] == 'q' && nm[3] == '_'))
        t |= kPtVocab;
    if (!strcmp(nm, "PostRender")) t |= kPtPostRender;
    return t;
}

struct PeName { uint32_t idx; uint32_t traits; const char* nm; bool vocabDone; };
static PeName g_peNames[8192];   // open addressing, index 0xffffffff = empty
static bool g_peNamesInit = false;
static uint32_t g_peNamesUsed = 0;

// The name and traits of an FName index. Fast: resolved once per index for the session (GNames
// entries never move or change). Off: resolved and classified on every call, as before.
static uint32_t PeNameTraits(uint32_t idx, const char** nmOut)
{
    if (idx == 0xffffffffu) { if (nmOut) *nmOut = NULL; return 0; }
    if (!InterlockedCompareExchange(&g_peFast, 0, 0)) {
        const char* nm = RealName(idx);
        if (nmOut) *nmOut = nm;
        ++g_peNameMiss;
        return PeTraitsOf(nm);
    }
    if (!g_peNamesInit) { for (auto& e : g_peNames) e.idx = 0xffffffffu; g_peNamesInit = true; }
    uint32_t h = (idx * 2654435761u) >> 19;   // 13 bits
    for (int probe = 0; probe < 16; ++probe, h = (h + 1) & 8191) {
        PeName& e = g_peNames[h];
        if (e.idx == idx) { ++g_peNameHit; if (nmOut) *nmOut = e.nm; return e.traits; }
        if (e.idx == 0xffffffffu) {
            const char* nm = RealName(idx);
            ++g_peNameMiss;
            const uint32_t t = PeTraitsOf(nm);
            if (nm) { e.idx = idx; e.traits = t; e.nm = nm; e.vocabDone = false; ++g_peNamesUsed; }   // an unresolved index is retried
            if (nmOut) *nmOut = nm;
            return t;
        }
    }
    const char* nm = RealName(idx);   // a full probe run: classify without caching
    ++g_peNameMiss;
    if (nmOut) *nmOut = nm;
    return PeTraitsOf(nm);
}

// The UI-vocabulary log (process_event.cpp, 34.4) needs "first sight of this index": answered from
// the cache entry when the fast path holds one, so the 512-entry scan behind it runs once per
// index instead of once per On*/Dis_*/Req_* event. False only when it has already run.
static bool PeVocabUnlogged(uint32_t idx)
{
    if (!InterlockedCompareExchange(&g_peFast, 0, 0) || !g_peNamesInit) return true;
    uint32_t h = (idx * 2654435761u) >> 19;
    for (int probe = 0; probe < 16; ++probe, h = (h + 1) & 8191) {
        PeName& e = g_peNames[h];
        if (e.idx == idx) { if (e.vocabDone) return false; e.vocabDone = true; return true; }
        if (e.idx == 0xffffffffu) return true;
    }
    return true;
}

// ---- the cost line ---------------------------------------------------------------------------
static void PeSubReport(double s);   // process_event.cpp: the per-statement split (pe/cost-fn)
static void PeCostTick()
{
    static uint64_t lastMs = 0;
    const uint64_t now = GetTickCount64();
    if (!lastMs) { lastMs = now; return; }
    if (now - lastMs < 5000) return;
    const double s = (now - lastMs) / 1000.0;
    LONGLONG tot = 0;
    for (auto v : g_peSecTicks) tot += v;
    const double f = g_qpcFreq ? (double)g_qpcFreq : 1.0;
    char parts[200]; int m = 0;
    for (int i = 0; i < kPeSections; ++i)
        m += _snprintf_s(parts + m, sizeof(parts) - m, _TRUNCATE, " %s %.2f", kPeSectionName[i], g_peSecTicks[i] * 1000.0 / f / s);
    Log("pe/cost: %.0f script events/s, %.2f us each, %.2f ms of the game thread per second (%s) | ms/s by section:%s | "
        "VirtualQuery %.0f/s (%.0f/s answered from the region cache) | names resolved %.0f/s, cached hits %.0f/s, %u cached | "
        "heavy writers %.0f/s run, %.0f/s skipped, %.0f/s at the draw entry (%s)",
        g_peCostCalls / s, g_peCostCalls ? tot * 1e6 / f / g_peCostCalls : 0.0, tot * 1000.0 / f / s,
        InterlockedCompareExchange(&g_peFast, 0, 0) ? "fast path ON" : "fast path off",
        parts, g_peVq / s, g_peVqSaved / s, g_peNameMiss / s, g_peNameHit / s, g_peNamesUsed,
        g_peHeavyRuns / s, g_peHeavySkips / s, g_peHeavyAtDraw / s,
        PeHeavyThrottled() ? "throttled" : "every event");
    g_peHeavyRuns = g_peHeavySkips = g_peHeavyAtDraw = 0;
    PeSubReport(s);
    for (auto& v : g_peSecTicks) v = 0;
    g_peCostCalls = g_peVq = g_peVqSaved = g_peNameMiss = g_peNameHit = 0;
    lastMs = now;
}

// A section boundary: charges the time since the previous boundary to `sec`.
static int g_peCostDepth = 0;   // a ProcessEvent our own code makes re-enters: timed inside the outer one
static inline void PeMark(int sec)
{
    if (g_peCostDepth != 1) return;
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    if (g_peCostT0) g_peSecTicks[sec] += t.QuadPart - g_peCostT0;
    g_peCostT0 = t.QuadPart;
}
struct PeCostScope {
    int sec = kPeTail;
    PeCostScope() {
        if (++g_peCostDepth != 1) return;
        LARGE_INTEGER t; QueryPerformanceCounter(&t); g_peCostT0 = t.QuadPart; ++g_peCostCalls;
    }
    ~PeCostScope() {
        if (g_peCostDepth == 1) { PeMark(sec); g_peCostT0 = 0; PeCostTick(); }
        --g_peCostDepth;
    }
};
