// game/dishonored/scene_draw.cpp - the game side of SequentialReentry (S2b):
// the second scene draw per tick. Included by the unity build.
//
// THE MECHANISM. UGameEngine::Tick calls the viewport draw root ONCE per tick
// (`push 1; call kViewportDraw` at kViewportDrawCallSite, ecx = the
// viewport; patterns.h, derived live in ENGINE_NOTES). The re-entry patches
// that one CALL SITE - not the root's prologue - to call DvrViewportDrawStub,
// which calls the root with the original arguments (pass 1, the engine's own
// draw, unguarded so a vanilla crash keeps its real dump), then, when every
// gate passes, writes the other eye into the camera field and calls the root
// a SECOND time under an SEH guard (pass 2). Patching the site instead of the
// root means: no trampoline, the root and its other two callers run pristine
// code, and the deny-by-default caller gate is a byte in the image (only the
// gameplay dispatcher reaches the stub; _ReturnAddress() is still checked and
// counted). The site is patched and restored on the GAME thread (the only
// thread that executes it), at the next ProcessEvent dispatch after `stereo
// reentry` / `stereo mono`.
//
// THE CAMERA. ProcessViewRotation is dispatched in the WORLD TICK (before the
// draw; the census in ENGINE_NOTES), so nothing recomputes the camera between
// the two passes: pass 1 draws from the eye the tick's dispatches wrote (-1,
// the seam's eye_for_next_frame under reentry), and the stub writes +1 into
// the camera field right before pass 2 through the same writer (the seam's
// per-thread fork keeps any dispatch INSIDE pass 2, the HUD's PostRender,
// writing +1 too). The eye tags ride the method's ring (core/gfx/reentry.cpp:
// game thread push per draw, present thread pop per present), each with the
// camera position the writer produced so a present can prove which draw it
// carries.
//
// ONE PUSH PER DRAW (41.1, session 9). The ring pairs by ORDER, and the game
// thread runs a frame ahead of the render thread that presents, so a single
// draw that pushed nothing let its present pop the NEXT tick's -1 and every
// tag after it rode the other draw: the eyes SWAPPED across every
// single -> double transition where the game thread was ahead (measured on
// the simulator with `reentry rearm 2` and the frameid line's c5 per
// present: the -1 tag's c5 jumped from the left eye's position to the
// right's). An armed single draw now pushes a 0 tag (its present pops it and
// goes out untagged, as before), so pushes equal presents while the method
// is popping.
//
// GATES: decided ONCE per tick, at depth 0, BEFORE pass 1's tag is pushed,
// and the same decision drives pass 2 (SceneDrawDecide -> g_sdTick). That is
// what makes tags and doubles unable to go one-sided: until 41.1 pass 1's tag
// checked five gates and pass 2 re-evaluated them after the draw plus four
// more (exiting, a test running, the c5 serial, a present since the previous
// draw), so the resume window - the game thread's catch-up burst, the verdict
// flapping through LOADING - produced -1 tags with no +1, and the runtime
// showed the RIGHT eye's held image against a fresh LEFT (the headset run 40
// report). The gates: armed (or a pulse credit) and not poisoned; the
// gameplay caller; not exiting; an XR session live; the game state strictly
// GAMEPLAY; no eyetest/postest running; a c5 upload since the previous tick's
// draws (a load screen draws no scene); a present since the previous tick's
// draw (liveness only - with a render thread it sequences nothing). A fault
// in pass 2 poisons the method for the session (the method drops to mono on
// the next present) and stands the VR path down with a line that says the
// game MAY not survive: an access violation between the root's begin/end
// enqueues is not something this code can undo.
//
// `reentry pulse [n]` doubles the next n gameplay draws with the same gates
// (armed or not) - the "make it MOVE" instrument: presents must advance by
// one per pulse and the pass-2 present's c5 must sit +ipd*scale along right
// from pass 1's (logged by the method as the tagged presents arrive).

#include <intrin.h>

typedef void (__fastcall* DvrViewportDrawFn)(void* self, void* edx, int bShouldPresent);

static bool           g_sdInstalled = false;   // the site is patched
static volatile LONG  g_sdWant = 0;            // 1 = patch requested, 0 = restore requested
static volatile LONG  g_sdArmed = 0;           // the doubling runs per gameplay tick
static uint32_t       g_sdLastPairsTotal = 0;
static volatile LONG  g_sdPoisoned = 0;
static volatile LONG  g_sdPulse = 0;           // one-shot doubles for the A/B
// `reentry skip2 <n>`: skip pass 2 for n ticks AFTER pass 1 pushed its tag -
// the one-sided -1 stream the 2026-09-03 headset desync was made of, on
// demand, so the stale-eye instrument and the strict-pair fallback can be
// shown to fail and to hold on the simulator ("make it MOVE").
static volatile LONG  g_sdForceSkip2 = 0;
static uint32_t       g_sdSkipForced = 0;
// 41.1 (session 9): `reentry rearm [n]` - n ticks decided SINGLE at the gate
// (no tag pushed, no pass 2), then the doubling resumes. The isolated half of
// the user's remedy for the one-view state (a pause/resume, a mode switch
// followed by a pause): does re-arming the second draw ALONE fix it, with
// the capture untouched? A diagnostic word; it never fires on its own.
static volatile LONG  g_sdForceSingle = 0;
static uint32_t       g_sdRearms = 0;
static volatile LONG  g_sdDepth = 0;
static DWORD          g_sdDrawTid = 0;
static uint32_t       g_sdDraws = 0, g_sdSecondDraws = 0;
static uint32_t       g_sdSkipForeign = 0, g_sdSkipState = 0, g_sdSkipSilent = 0, g_sdSkipStall = 0,
                      g_sdSkipSession = 0, g_sdSkipTest = 0, g_sdSkipExit = 0;
static uint32_t       g_sdLastExcCode = 0, g_sdLastExcAddr = 0;
static uint32_t       g_sdCall2Us = 0, g_sdCall2MaxUs = 0;
// 41.1 (session 9): pass-2 writes that the camera seam REFUSED (a null or
// stale camera object, no basis row, no field): the second draw still ran,
// from pass 1's camera, and its +1 tag carried no position - the one silent
// way both eyes end up with one view. Counted per beat window, lifetime too.
static uint32_t       g_sdP2WriteRefused = 0, g_sdBeatP2Refused = 0;
// 41.1 (session 8): the game thread's side of the tick budget. Per tick:
// the period between draw-root entries, pass 1's own call time (call1), the
// time from the previous tick's last pass returning to this entry (outside =
// the world tick plus the engine's render-thread sync, which this stub cannot
// split; the perf line's idle says whether the render thread waits for THIS
// thread). Window min/max/sum, printed on the beat line.
static LARGE_INTEGER  g_sdT0Prev = {};          // the previous tick's entry
static LARGE_INTEGER  g_sdRetPrev = {};         // the previous tick's last pass returning
static uint32_t       g_sdCall1Us = 0, g_sdCall1MaxUs = 0;
static uint64_t       g_sdSumPeriodUs = 0, g_sdSumCall1Us = 0, g_sdSumCall2Us = 0, g_sdSumOutsideUs = 0;
static uint32_t       g_sdMinPeriodUs = UINT32_MAX, g_sdMaxPeriodUs = 0, g_sdBeatTicks = 0;
static uint32_t       g_sdLastDrawPresent = 0;
static uint32_t       g_sdLastDrawC5Serial = 0;
static uint64_t       g_sdBeatMs = 0;
static uint32_t       g_sdBeatDraws = 0, g_sdBeatSecond = 0, g_sdBeatPresents = 0;
static uint8_t        g_sdSaved[7];
static char           g_sdRefuse[160] = "";

// Byte-verify the root's prologue and the call site; false = a different exe
// build, and the reason is the refusal the seam prints.
static bool SceneDrawVerify()
{
    const uint8_t* root = (const uint8_t*)kViewportDraw;
    const uint8_t* site = (const uint8_t*)kViewportDrawCallSite;
    if (!RangeReadable((void*)root, 16) || !RangeReadable((void*)site, 7)) {
        _snprintf(g_sdRefuse, sizeof(g_sdRefuse), "root 0x%08x or call site 0x%08x unreadable",
                  (unsigned)kViewportDraw, (unsigned)kViewportDrawCallSite);
        return false;
    }
    if (memcmp(root, kViewportDrawPrologue, sizeof(kViewportDrawPrologue)) != 0) {
        _snprintf(g_sdRefuse, sizeof(g_sdRefuse), "prologue at 0x%08x is %02x %02x %02x %02x %02x %02x, expected "
                  "55 8b ec 6a ff 68 (wrong exe build?)", (unsigned)kViewportDraw, root[0], root[1], root[2], root[3], root[4], root[5]);
        return false;
    }
    if (g_sdInstalled) return true;
    if (memcmp(site, kViewportDrawCallSiteOrig, sizeof(kViewportDrawCallSiteOrig)) != 0) {
        _snprintf(g_sdRefuse, sizeof(g_sdRefuse), "call site at 0x%08x is %02x %02x %02x %02x %02x %02x %02x, expected "
                  "6a 01 e8 cf 94 fc ff (wrong exe build?)", (unsigned)kViewportDrawCallSite,
                  site[0], site[1], site[2], site[3], site[4], site[5], site[6]);
        return false;
    }
    const int32_t rel = *(const int32_t*)(site + 3);
    const uint32_t target = (uint32_t)((int32_t)(kViewportDrawCallSite + 7) + rel);
    if (target != kViewportDraw) {
        _snprintf(g_sdRefuse, sizeof(g_sdRefuse), "the call at 0x%08x targets 0x%08x, not the root 0x%08x",
                  (unsigned)(kViewportDrawCallSite + 2), target, (unsigned)kViewportDraw);
        return false;
    }
    g_sdRefuse[0] = 0;
    return true;
}

static bool SceneDrawAvailable(char* why, size_t cap)
{
    const bool ok = SceneDrawVerify();
    if (why && cap) { strncpy(why, g_sdRefuse, cap - 1); why[cap - 1] = 0; }
    return ok;
}

// SEH filter for the SECOND call only. C++ throws and stack overflow pass
// through; everything else is recorded and handled.
static int SceneDrawFilter(unsigned code, EXCEPTION_POINTERS* ep)
{
    if (code == 0xE06D7363u || code == EXCEPTION_STACK_OVERFLOW) return EXCEPTION_CONTINUE_SEARCH;
    g_sdLastExcCode = code;
    g_sdLastExcAddr = ep && ep->ExceptionRecord ? (uint32_t)(uintptr_t)ep->ExceptionRecord->ExceptionAddress : 0;
    return EXCEPTION_EXECUTE_HANDLER;
}

// No C++ objects in this frame (SEH + unwinding = C2712).
static bool SceneDrawCallGuarded(DvrViewportDrawFn fn, void* self, int b)
{
    __try {
        fn(self, NULL, b);
        return true;
    } __except (SceneDrawFilter(GetExceptionCode(), GetExceptionInformation())) {
        return false;
    }
}

static void SceneDrawBeat()
{
    const uint64_t now = GetTickCount64();
    if (g_sdBeatMs == 0) { g_sdBeatMs = now; g_sdBeatPresents = g_frame; return; }
    if (now - g_sdBeatMs < 3000) return;
    const double s = (double)(now - g_sdBeatMs) / 1000.0;
    const uint32_t presents = g_frame;
    const uint32_t n = g_sdBeatTicks ? g_sdBeatTicks : 1;
    Log("reentry: beat draws/s=%.0f 2nd/s=%.0f presents/s=%.0f call2=%u us (max %u) skips foreign=%lu state=%lu "
        "silent=%lu stall=%lu session=%lu test=%lu exit=%lu drawTid=%lu presentTid=%lu%s%s"
        " | p2write refused=%lu of %lu (lifetime %lu; a refused write = pass 2 drew from pass 1's camera) cam=%p"
        " | game: period %.1f ms (min %.1f max %.1f) call1=%.1f ms (max %.1f) call2=%.2f (max %.2f) "
        "outside=%.1f (the world tick + the render-thread sync, unsplit here; the perf line's idle says whether "
        "the render thread waits for this thread)%s",
        g_sdBeatDraws / s, g_sdBeatSecond / s, (presents - g_sdBeatPresents) / s, g_sdCall2Us, g_sdCall2MaxUs,
        (unsigned long)g_sdSkipForeign, (unsigned long)g_sdSkipState, (unsigned long)g_sdSkipSilent,
        (unsigned long)g_sdSkipStall, (unsigned long)g_sdSkipSession, (unsigned long)g_sdSkipTest,
        (unsigned long)g_sdSkipExit, (unsigned long)g_sdDrawTid, (unsigned long)g_presentTid,
        g_sdArmed ? " gates=once-per-tick" : " (doubling OFF: 2nd/s reads 0 by design)",
        g_sdPoisoned ? " POISONED" : "",
        (unsigned long)g_sdBeatP2Refused, (unsigned long)g_sdBeatSecond, (unsigned long)g_sdP2WriteRefused, (void*)g_camObj,
        g_sdSumPeriodUs / 1000.0 / n, g_sdMinPeriodUs == UINT32_MAX ? 0.0 : g_sdMinPeriodUs / 1000.0,
        g_sdMaxPeriodUs / 1000.0, g_sdSumCall1Us / 1000.0 / n, g_sdCall1MaxUs / 1000.0,
        g_sdSumCall2Us / 1000.0 / n, g_sdCall2MaxUs / 1000.0, g_sdSumOutsideUs / 1000.0 / n,
        g_sdCall1MaxUs >= 5000 ? " (call1 large: the game thread blocks INSIDE its own draw - render-command "
                                 "back-pressure)" : "");
    g_sdBeatMs = now;
    // 41.2: THE STAND-DOWN IS GONE, and the reason is worth keeping.
    //
    // Two versions of a guard here tried to disarm the doubling on the theory
    // that it ran on only ~30 % of gameplay ticks and that the singles were
    // going out as left-offset mono frames. Review of the surrounding log
    // killed that: the window it was built from straddled a pause menu, an
    // xrEndFrame failure and session teardown. The windows either side of it
    // read draws/s=78 2nd/s=78, 86/86, 87/87, 81/81 - the renderer doubles
    // essentially every gameplay tick, and the refusals in the bad window were
    // "no XR session", after the session had already gone.
    //
    // The desktop alternation those guards were chasing had a different cause
    // entirely: every eye draw reaches the game's own Present and nothing
    // pinned the window to one eye (core/gfx/desktop_eye.cpp). A guard that
    // disarms a healthy renderer whenever a session drops would have been a
    // booby trap, so it is removed rather than tuned.
    g_sdBeatDraws = g_sdBeatSecond = 0;
    g_sdBeatP2Refused = 0;
    g_sdBeatPresents = presents;
    g_sdCall2MaxUs = 0;
    g_sdCall1MaxUs = 0;
    g_sdSumPeriodUs = g_sdSumCall1Us = g_sdSumCall2Us = g_sdSumOutsideUs = 0;
    g_sdMinPeriodUs = UINT32_MAX; g_sdMaxPeriodUs = 0; g_sdBeatTicks = 0;
}

// One tick's decision, made at depth 0 BEFORE pass 1's tag (game thread only).
struct SdDecision { bool doubleIt; bool pulse; const char* why; bool gameplay; };
static SdDecision g_sdTick = { false, false, "", false };

static SdDecision SceneDrawDecide(uint32_t callerRet)
{
    SdDecision d = { false, false, "", false };
    if (InterlockedCompareExchange(&g_sdPulse, 0, 0) > 0) d.pulse = InterlockedDecrement(&g_sdPulse) >= 0;
    const bool armed = InterlockedCompareExchange(&g_sdArmed, 0, 0) != 0;
    if (!d.pulse && !armed) { d.why = "not armed"; return d; }
    if (InterlockedCompareExchange(&g_sdForceSingle, 0, 0) > 0) {
        if (InterlockedDecrement(&g_sdForceSingle) == 0)
            Log("reentry/rearm: done - the doubling resumes at the next gameplay tick; the frameid line's next pairs "
                "say whether a re-arm ALONE made two pictures of one");
        d.why = "rearm by request"; return d;
    }
    if (g_sdPoisoned) { d.why = "poisoned"; return d; }
    if (callerRet != kViewportDrawGameplayRet) { ++g_sdSkipForeign; d.why = "foreign caller"; return d; }
    if (InterlockedCompareExchange(&g_gameExiting, 0, 0)) { ++g_sdSkipExit; d.why = "exiting"; return d; }
    if (!dvr::vr::session_live() && !d.pulse) { ++g_sdSkipSession; d.why = "no XR session"; return d; }
    if (!DvrGameplayVerdict()) { ++g_sdSkipState; d.why = "state not GAMEPLAY"; return d; }
    d.gameplay = true;   // from here on the draw presents once (a menu's draws outnumber its presents)
    if (dvr::camera::eyetest_active() || dvr::camera::postest_active()) { ++g_sdSkipTest; d.why = "eyetest/postest running"; return d; }
    // The camera-silent hole: a c5 upload must have arrived since the previous
    // tick's draws (a load screen draws no scene). The serial counts uploads.
    if (dvr::camera::render_pos_serial() == g_sdLastDrawC5Serial) { ++g_sdSkipSilent; d.why = "camera silent (no c5 upload since the previous draw)"; return d; }
    // Present-stall guard (liveness only): at least one present since the
    // previous tick's draw; pulses bypass it so the A/B works while paused.
    if (g_frame == g_sdLastDrawPresent && !d.pulse) { ++g_sdSkipStall; d.why = "no present since the previous draw"; return d; }
    d.doubleIt = true;
    d.why = "all gates pass";
    return d;
}

// The decision's CHANGES only: a spell of single draws is the mono path in the
// headset (both eyes the same image), and its reason is the thing to read.
static void SceneDrawDecisionLog(const SdDecision& d)
{
    static bool wasDouble = false, said = false;
    static uint32_t singleTicks = 0;
    if (!d.doubleIt) ++singleTicks;
    if (said && d.doubleIt == wasDouble) return;
    said = true; wasDouble = d.doubleIt;
    if (d.doubleIt) {
        if (singleTicks)
            Log("reentry: gates -> DOUBLE draw after %lu single tick(s) - both eyes tagged again", (unsigned long)singleTicks);
        singleTicks = 0;
    } else {
        DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Info, 1000,
                         "reentry: gates -> SINGLE draw (%s): no tag this tick, the runtime shows the untagged "
                         "present on the mono path until the gates pass", d.why);
    }
}

// The second draw, taking the tick's decision (never re-deciding: that is what
// made the tags one-sided). Only the poison is re-read - a fault poisons
// mid-tick.
static void SceneDrawMaybeSecond(void* self, int b, const SdDecision& d)
{
    if (!d.doubleIt || g_sdPoisoned) return;
    if (InterlockedCompareExchange(&g_sdForceSkip2, 0, 0) > 0) {
        // The instrument: the -1 tag is already in the ring, pass 2 is not
        // coming. Exactly what the pre-41.1 gates did by accident.
        if (InterlockedDecrement(&g_sdForceSkip2) == 0)
            Log("reentry/skip2: done - pass 2 skipped after pass 1's tag on %lu tick(s); the eyes line "
                "and the STALE EYE line say what the runtime made of the one-sided stream",
                (unsigned long)g_sdSkipForced + 1);
        ++g_sdSkipForced;
        return;
    }
    const bool pulse = d.pulse;

    // Pass 2: the other eye into the camera field, on this thread, through the
    // seam's writer (its per-thread fork keeps in-draw dispatches on +1 too).
    float wrotePos[3] = {0, 0, 0};
    dvr::camera::set_second_pass(true);
    const bool wrote = dvr::camera::apply_offsets(g_camObj) && dvr::camera::last_written_pos(wrotePos);
    if (!wrote) {
        ++g_sdP2WriteRefused; ++g_sdBeatP2Refused;
        DVR_LOG_EVERY_MS(dvr::log::Cat::present, dvr::log::Level::Warn, 3000,
                         "reentry: pass 2's eye write REFUSED by the camera seam (cam=%p, field %s) - the second draw "
                         "runs from pass 1's camera this tick: both eyes carry one view (counted on the beat line)",
                         (void*)g_camObj, dvr::camera::eye_field());
    }
    dvr::stereo::reentry_push_tag(+1, wrote ? wrotePos : NULL);
    dvr::vr::set_draw_stage("secondDraw");
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);
    const bool ok = SceneDrawCallGuarded((DvrViewportDrawFn)kViewportDraw, self, b);
    QueryPerformanceCounter(&t1);
    dvr::vr::set_draw_stage(NULL);
    dvr::camera::set_second_pass(false);
    g_sdCall2Us = (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / (g_qpcFreq ? g_qpcFreq : 1));
    if (g_sdCall2Us > g_sdCall2MaxUs) g_sdCall2MaxUs = g_sdCall2Us;
    if (!ok) {
        InterlockedExchange(&g_sdPoisoned, 1);
        InterlockedExchange(&g_sdArmed, 0);
        dvr::frame::set_disabled(true);
        DVR_LOG(dvr::log::Cat::present, dvr::log::Level::Error,
                "reentry: second draw FAULTED code=0x%08x at 0x%08x - POISONED for the session, the VR path stands "
                "down (the method drops to mono on the next present); the game MAY not survive a fault inside "
                "the viewport draw. `reentry reset` clears the poison for a retry by hand.",
                g_sdLastExcCode, g_sdLastExcAddr);
        LogFlush();
        return;
    }
    ++g_sdSecondDraws; ++g_sdBeatSecond;
    if (pulse)
        Log("reentry/pulse: second draw ok, call2=%u us, wrote eye +1 %s (pos %.1f %.1f %.1f); the next two "
            "presents carry tags -1/+1 - the method logs their c5 travel", g_sdCall2Us,
            wrote ? "into the camera field" : "NOT WRITTEN (no camera/field)", wrotePos[0], wrotePos[1], wrotePos[2]);
}

// The stub the patched call site reaches: ecx = the viewport, one stack arg.
static void __fastcall DvrViewportDrawStub(void* self, void* edx, int bShouldPresent)
{
    (void)edx;
    const uint32_t callerRet = (uint32_t)(uintptr_t)_ReturnAddress();
    const LONG depth = InterlockedIncrement(&g_sdDepth) - 1;
    LARGE_INTEGER t0 = {}, t1 = {};
    if (depth == 0) {
        g_sdDrawTid = GetCurrentThreadId();
        ++g_sdDraws; ++g_sdBeatDraws;
        QueryPerformanceCounter(&t0);
        const LONGLONG f = g_qpcFreq ? g_qpcFreq : 1;
        if (g_sdT0Prev.QuadPart) {
            const uint32_t period = (uint32_t)((t0.QuadPart - g_sdT0Prev.QuadPart) * 1000000 / f);
            const uint32_t outside = g_sdRetPrev.QuadPart ? (uint32_t)((t0.QuadPart - g_sdRetPrev.QuadPart) * 1000000 / f) : 0;
            g_sdSumPeriodUs += period; g_sdSumOutsideUs += outside; ++g_sdBeatTicks;
            if (period < g_sdMinPeriodUs) g_sdMinPeriodUs = period;
            if (period > g_sdMaxPeriodUs) g_sdMaxPeriodUs = period;
        }
        g_sdT0Prev = t0;
        // The tick's ONE decision, before pass 1's tag: the tag is pushed iff
        // pass 2 will run, so a present can never carry a -1 whose +1 sibling
        // was skipped (41.1: the resume-window one-sided stream).
        g_sdTick = SceneDrawDecide(callerRet);
        if (callerRet == kViewportDrawGameplayRet) SceneDrawDecisionLog(g_sdTick);
        if (g_sdTick.doubleIt) {
            float pos[3];
            dvr::stereo::reentry_push_tag(-1, dvr::camera::last_written_pos(pos) ? pos : NULL);
        } else if (g_sdTick.gameplay && InterlockedCompareExchange(&g_sdArmed, 0, 0) && !g_sdPoisoned) {
            // A single GAMEPLAY draw while the method pops: one push per draw,
            // so its present cannot eat the next tick's -1 (the header's ONE
            // PUSH). Not in menus: their draws outnumber their presents and
            // the ring would only fill with junk (measured: cleared every 3 s).
            dvr::stereo::reentry_push_tag(0, NULL);
        }

    }
    ((DvrViewportDrawFn)kViewportDraw)(self, NULL, bShouldPresent);
    if (depth == 0) {
        QueryPerformanceCounter(&t1);
        g_sdCall1Us = (uint32_t)((t1.QuadPart - t0.QuadPart) * 1000000 / (g_qpcFreq ? g_qpcFreq : 1));
        if (g_sdCall1Us > g_sdCall1MaxUs) g_sdCall1MaxUs = g_sdCall1Us;
        g_sdSumCall1Us += g_sdCall1Us;
        const uint32_t call2Before = g_sdSecondDraws;
        SceneDrawMaybeSecond(self, bShouldPresent, g_sdTick);
        if (g_sdSecondDraws != call2Before) g_sdSumCall2Us += g_sdCall2Us;
        QueryPerformanceCounter(&g_sdRetPrev);
        g_sdLastDrawPresent = g_frame;
        g_sdLastDrawC5Serial = dvr::camera::render_pos_serial();
        SceneDrawBeat();
    }
    InterlockedDecrement(&g_sdDepth);
}

// Patch / restore the call site. GAME THREAD ONLY (PeHandler applies the
// request): the site is executed by this thread alone.
static void SceneDrawApply()
{
    const bool want = InterlockedCompareExchange(&g_sdWant, 0, 0) != 0;
    if (want == g_sdInstalled) return;
    uint8_t* site = (uint8_t*)kViewportDrawCallSite;
    if (want) {
        if (!SceneDrawVerify()) {
            Log("reentry: hook NOT installed - %s", g_sdRefuse);
            InterlockedExchange(&g_sdWant, 0);
            return;
        }
        DWORD op;
        if (!VirtualProtect(site, 7, PAGE_EXECUTE_READWRITE, &op)) { Log("reentry: VirtualProtect failed on the call site"); InterlockedExchange(&g_sdWant, 0); return; }
        memcpy(g_sdSaved, site, 7);
        const int32_t rel = (int32_t)((uintptr_t)&DvrViewportDrawStub - (kViewportDrawCallSite + 7));
        memcpy(site + 3, &rel, 4);   // the E8 stays, only its target moves
        VirtualProtect(site, 7, op, &op);
        FlushInstructionCache(GetCurrentProcess(), site, 7);
        g_sdInstalled = true;
        Log("reentry: hook installed at the gameplay call site 0x%08x -> root 0x%08x now reached through the stub "
            "(bytes verified; doubling %s)", (unsigned)kViewportDrawCallSite, (unsigned)kViewportDraw,
            g_sdArmed ? "ARMED" : "OFF until stereo reentry arms");
    } else {
        DWORD op;
        if (!VirtualProtect(site, 7, PAGE_EXECUTE_READWRITE, &op)) { Log("reentry: VirtualProtect failed restoring the call site"); return; }
        memcpy(site, g_sdSaved, 7);
        VirtualProtect(site, 7, op, &op);
        FlushInstructionCache(GetCurrentProcess(), site, 7);
        g_sdInstalled = false;
        Log("reentry: hook removed - the call site at 0x%08x is the original again", (unsigned)kViewportDrawCallSite);
    }
}

// The method arms / disarms (present thread): the patch request goes to the
// game thread, the arm flag is immediate.
static void SceneDrawSetArmed(bool on)
{
    if (on && g_sdPoisoned) { Log("reentry: arm refused - POISONED (reentry reset to clear)"); return; }
    InterlockedExchange(&g_sdArmed, on ? 1 : 0);
    InterlockedExchange(&g_sdWant, on ? 1 : 0);
    Log("reentry: %s - the call site is %s at the next script dispatch", on ? "ARMED" : "disarmed",
        on ? "patched" : "restored");
}

static bool SceneDrawPoisoned() { return InterlockedCompareExchange(&g_sdPoisoned, 0, 0) != 0; }
static uint32_t SceneDrawDraws() { return g_sdDraws; }

// The pass-2 skip counters for the method's stale-eye line (present thread
// reads what the game thread counts: diagnostics, a torn read costs one).
static void SceneDrawGates(uint32_t out[dvr::stereo::kReentryGateCount])
{
    out[0] = g_sdSkipForeign; out[1] = g_sdSkipState; out[2] = g_sdSkipSilent; out[3] = g_sdSkipStall;
    out[4] = g_sdSkipSession; out[5] = g_sdSkipTest;  out[6] = g_sdSkipExit;   out[7] = g_sdSkipForced;
}

static void SceneDrawStatus(dvr::status::Writer& w)
{
    w.kv("hook", g_sdInstalled);
    w.kv("armed", (bool)(g_sdArmed != 0));
    w.kv("poisoned", (bool)(g_sdPoisoned != 0));
    w.kv("draws", (unsigned long)g_sdDraws);
    w.kv("secondDraws", (unsigned long)g_sdSecondDraws);
    w.kv("call2Us", (int)g_sdCall2Us);
    w.kv("p2WriteRefused", (unsigned long)g_sdP2WriteRefused);

    w.kv("skipForeign", (unsigned long)g_sdSkipForeign);
    w.kv("skipState", (unsigned long)g_sdSkipState);
    w.kv("skipSilent", (unsigned long)g_sdSkipSilent);
    w.kv("skipStall", (unsigned long)g_sdSkipStall);
    w.kv("skipSession", (unsigned long)g_sdSkipSession);
    w.kv("skipTest", (unsigned long)g_sdSkipTest);
    w.kv("skipExit", (unsigned long)g_sdSkipExit);
    w.kv("skipForced", (unsigned long)g_sdSkipForced);
    w.kv("rearms", (unsigned long)g_sdRearms);

    w.kv("lastExc", (unsigned long)g_sdLastExcCode);
}

static bool SceneDrawCommand(const char* args)
{
    char sub[16] = "", a1[16] = "";
    const int n = sscanf(args, "%15s %15s", sub, a1);
    if (n >= 1 && !strcmp(sub, "pulse")) {
        int k = a1[0] ? atoi(a1) : 1;
        if (k < 1) k = 1;
        if (k > 300) k = 300;
        if (!g_sdInstalled) {
            InterlockedExchange(&g_sdWant, 1);
            Log("reentry/pulse: the hook is not installed yet - requested; send the pulse again once "
                "`reentry: hook installed` is in the log");
            return true;
        }
        InterlockedExchange(&g_sdPulse, k);
        Log("reentry/pulse: doubling the next %d gameplay draw(s) with eye +1 on pass 2", k);
        return true;
    }
    if (n >= 1 && !strcmp(sub, "skip2")) {
        int k = a1[0] ? atoi(a1) : 60;
        if (k < 1) k = 1;
        if (k > 3000) k = 3000;
        InterlockedExchange(&g_sdForceSkip2, k);
        Log("reentry/skip2: the next %d armed gameplay tick(s) push pass 1's -1 tag and SKIP pass 2 - a one-sided "
            "stream on purpose; expect `stereo: STALE R EYE` (strict off) or `xr: strict pair - stereo submit "
            "REFUSED` (strict on)", k);
        return true;
    }
    if (n >= 1 && !strcmp(sub, "c5pair")) {   // 41.1 (session 9): the within-tick invariant's A/B
        bool on;
        if (DvrOnOff(a1, &on)) { dvr::stereo::set_reentry_c5_pair(on); return true; }
        Log("reentry: c5pair on|off (now %s)", dvr::stereo::reentry_c5_pair() ? "on" : "off");
        return true;
    }
    if (n >= 1 && !strcmp(sub, "rearm")) {

        int k = a1[0] ? atoi(a1) : 2;
        if (k < 1) k = 1;
        if (k > 300) k = 300;
        if (!g_sdArmed) { Log("reentry/rearm: the doubling is not armed - nothing to re-arm (stereo reentry first)"); return true; }
        InterlockedExchange(&g_sdForceSingle, k);
        ++g_sdRearms;
        Log("reentry/rearm: the next %d gameplay tick(s) draw SINGLE (no tag, no pass 2), then the doubling resumes - "
            "the capture is untouched; expect `gates -> SINGLE draw (rearm by request)` then `DOUBLE draw after %d single "
            "tick(s)` (rearm #%lu)", k, k, (unsigned long)g_sdRearms);
        return true;
    }
    if (n >= 1 && !strcmp(sub, "reset")) {
        InterlockedExchange(&g_sdPoisoned, 0);
        g_sdLastExcCode = g_sdLastExcAddr = 0;
        dvr::frame::set_disabled(false);
        Log("reentry: poison cleared");
        return true;
    }
    if (n >= 1 && !strcmp(sub, "hook")) {
        if (!strcmp(a1, "on")) { InterlockedExchange(&g_sdWant, 1); Log("reentry: hook requested (observation only until armed)"); return true; }
        if (!strcmp(a1, "off")) { InterlockedExchange(&g_sdWant, 0); Log("reentry: hook removal requested"); return true; }
    }
    if (n >= 1 && !strcmp(sub, "status")) {
        char why[160];
        const bool avail = SceneDrawAvailable(why, sizeof(why));
        Log("reentry: patterns %s%s%s | hook=%d armed=%d poisoned=%d draws=%lu 2nd=%lu call2=%u us | skips foreign=%lu "
            "state=%lu silent=%lu stall=%lu session=%lu test=%lu | lastExc=0x%08x@0x%08x drawTid=%lu presentTid=%lu",
            avail ? "VERIFIED" : "REFUSED", avail ? "" : ": ", avail ? "" : why,
            (int)g_sdInstalled, (int)g_sdArmed, (int)g_sdPoisoned, (unsigned long)g_sdDraws, (unsigned long)g_sdSecondDraws,
            g_sdCall2Us, (unsigned long)g_sdSkipForeign, (unsigned long)g_sdSkipState, (unsigned long)g_sdSkipSilent,
            (unsigned long)g_sdSkipStall, (unsigned long)g_sdSkipSession, (unsigned long)g_sdSkipTest,
            g_sdLastExcCode, g_sdLastExcAddr, (unsigned long)g_sdDrawTid, (unsigned long)g_presentTid);
        return true;
    }
    return false;
}
