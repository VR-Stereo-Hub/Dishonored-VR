// RETIRED 2026-09-12, and kept only as the record of a question that got a
// definitive answer. Compiled only with -DDVR_WITH_LEGACY=ON, unwired from the
// script lane, and it has no ini key or seam word any more.
//
// IT ANSWERED ITS QUESTION, AND THE ANSWER WAS NO. The beat line read
// `wrote 1, survived to the next tick 0`: the engine recomputes the focused-actor
// field after our tick, every tick, so a write there can never be read by
// anything. The real fix has to go at the engine's writer, which is not found.
//
// IT ALSO CRASHED THE GAME, and that part is the lesson. The class-name guard
// below cannot tell a FREED object from a live one - freed memory still holds a
// plausible class pointer until it is reused - and the actor this experiment
// holds is a bolt, which the player DESTROYS by picking it up. So the success
// path of the experiment was also the path that produced a dangling pointer,
// handed back to the engine ~90 times a second. See TRAPS.
//
// Do not re-arm this. If the idea is revisited, the liveness test has to be
// IsLiveObject against the GObjects set, not a class-name comparison.
// Included by the Dishonored unity TU.
//
// VR-85, step one: can the focused-interactable field be WRITTEN, and does the
// game act on what we put there?
//
// PropWatch found the field by watching it move: DishonoredPlayerController
// m_pCrosshairActor and m_pCrosshairHighlightActor, which hold whatever the
// engine's own narrow head-aim trace is currently on, and read none otherwise.
// Knowing the field is not knowing that a write survives - the engine may
// recompute it after ours every tick, in which case the real fix has to go at
// its writer instead.
//
// This answers that ONE question and deliberately nothing else. It does not
// trace, it does not use the controller ray, and it does not decide what the
// player should be pointing at. It remembers the last thing the ENGINE itself
// focused and, while the engine has nothing focused, puts that back. So:
//
//   look at a bolt, look away, press use.
//
//   picked up  -> the write sticks AND the game acts on it. The feature is then
//                 a matter of supplying a better actor than "the last one", and
//                 the controller ray is what supplies it.
//   nothing    -> either the write is overwritten before it is read, or the use
//                 path does not read this field. The log distinguishes them: it
//                 re-reads its own write on the next tick and says whether it
//                 survived.
//
// Mixing a trace into this step would have made a failure ambiguous, which is
// the whole reason it is not here.
//
// The offsets are resolved BY NAME through the existing property resolver, not
// hardcoded. The names came from the engine and outlive a rebuild of the game;
// the offsets that were observed alongside them (0x69C and 0x6A0) are logged as
// a cross-check, so a mismatch is visible rather than silent.

#include <atomic>

static std::atomic<bool> g_ifEnabled{true};   // see the commit: a deliberate ON

static uint32_t g_ifActorOff = 0;             // m_pCrosshairActor
static uint32_t g_ifHiliteOff = 0;            // m_pCrosshairHighlightActor
static bool     g_ifResolved = false;
static bool     g_ifRefused = false;          // resolution failed; say so once

// What the engine last focused, and enough identity to notice the memory being
// reused for something else.
static uint8_t* g_ifLast = NULL;
static char     g_ifLastCls[64] = {0};
static double   g_ifLastMs = 0;

static volatile LONG g_ifHeld = 0, g_ifStuck = 0, g_ifLost = 0, g_ifDropped = 0;
static uint8_t* g_ifPending = NULL;           // what we wrote last tick
static double   g_ifNextBeatMs = 0;

// How long a remembered actor stays usable. The engine drops focus the moment
// the trace misses, and a pointer we hold past a few seconds is a pointer that
// may have been freed and reused. Short on purpose.
static const double kIfHoldMs = 3000.0;

static bool IfEnabled() { return g_ifEnabled.load(); }
static void IfSet(bool on, const char* source) {
    g_ifEnabled.store(on);
    Log("interactfocus: %s (%s). Writes DishonoredPlayerController's focused-actor "
        "field ONLY while the engine's own value is none, never over a live "
        "selection, so head aim keeps today's behaviour exactly.",
        on ? "ON" : "off", source);
}

static void IfResolve() {
    if (g_ifResolved || g_ifRefused) return;
    g_ifActorOff  = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairActor");
    g_ifHiliteOff = RflOffsetOf("DishonoredPlayerController", "m_pCrosshairHighlightActor");
    if (!g_ifActorOff) {
        g_ifRefused = true;
        Log("interactfocus: REFUSED - could not resolve "
            "DishonoredPlayerController::m_pCrosshairActor by name. Nothing is "
            "written. The property table may not be built yet, or this is not the "
            "build the name was found on.");
        return;
    }
    g_ifResolved = true;
    Log("interactfocus: resolved m_pCrosshairActor at +0x%X and "
        "m_pCrosshairHighlightActor at +0x%X, by NAME. The run that found them "
        "observed +0x69C and +0x6A0; a difference here is not an error but it "
        "does mean the layout moved, so treat any address written down elsewhere "
        "as stale.",
        g_ifActorOff, g_ifHiliteOff);
}

// SCRIPT LANE, beside the other per-tick work, where the objects are coherent.
static void InteractFocusTick() {
    if (!IfEnabled()) return;
    // Same gameplay gate the fire seams use: no menus, no cinematics, and a
    // head sample recent enough that the player is really in the world.
    if (!CylTruthLive() || g_menuOpen || g_inMenu || g_mainMenu || g_cineNow) return;
    IfResolve();
    if (!g_ifResolved) return;

    uint8_t* ctrl = g_peCtrl;
    if (!ctrl || !LooksLikeObj(ctrl) || !RangeReadable(ctrl + g_ifActorOff, 4)) return;

    const double now = MaimNowMs();
    uint8_t** slot = (uint8_t**)(ctrl + g_ifActorOff);
    uint8_t*  cur  = *slot;

    // Did the write we made last tick survive to this one? This is the actual
    // measurement, and it has to be able to come out either way.
    if (g_ifPending) {
        if (cur == g_ifPending) InterlockedIncrement(&g_ifStuck);
        else                    InterlockedIncrement(&g_ifLost);
        g_ifPending = NULL;
    }

    if (cur) {
        // The engine has its own selection. Never touch it; just remember it.
        if (LooksLikeObj(cur)) {
            const char* c = ObjClassName(cur);
            if (c) {
                if (cur != g_ifLast) {
                    strncpy(g_ifLastCls, c, sizeof(g_ifLastCls) - 1);
                    g_ifLastCls[sizeof(g_ifLastCls) - 1] = 0;
                }
                g_ifLast = cur;
                g_ifLastMs = now;
            }
        }
        return;
    }

    // The engine has nothing focused. Put the remembered actor back, if it is
    // still recent and still looks like the same object.
    if (!g_ifLast) return;
    if (now - g_ifLastMs > kIfHoldMs) {
        InterlockedIncrement(&g_ifDropped);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
            "interactfocus: dropped the remembered '%s' after %.0f ms without the "
            "engine re-focusing it. A held pointer is only safe while it is fresh.",
            g_ifLastCls, now - g_ifLastMs);
        g_ifLast = NULL; g_ifLastCls[0] = 0;
        return;
    }
    if (!LooksLikeObj(g_ifLast)) { g_ifLast = NULL; g_ifLastCls[0] = 0; return; }
    const char* c = ObjClassName(g_ifLast);
    if (!c || strcmp(c, g_ifLastCls)) {
        // The allocation is not what it was. This is the check that stops a
        // freed-and-reused actor being handed to the game as a usable.
        Log("interactfocus: dropped the remembered actor - its class now reads "
            "'%s' where it was '%s', so the memory was reused. Not written.",
            c ? c : "<unreadable>", g_ifLastCls);
        g_ifLast = NULL; g_ifLastCls[0] = 0;
        return;
    }

    *slot = g_ifLast;
    if (g_ifHiliteOff && RangeReadable(ctrl + g_ifHiliteOff, 4))
        *(uint8_t**)(ctrl + g_ifHiliteOff) = g_ifLast;
    g_ifPending = g_ifLast;
    const LONG n = InterlockedIncrement(&g_ifHeld);
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
        "interactfocus #%ld: engine focus was none, wrote back '%s' (held %.0f ms). "
        "This is the EXPERIMENT, not the feature: it re-offers the last thing the "
        "engine itself focused, so that pressing use while looking away tests "
        "whether the write survives and whether the game acts on it.",
        n, g_ifLastCls, now - g_ifLastMs);

    if (now >= g_ifNextBeatMs) {
        g_ifNextBeatMs = now + 5000.0;
        Log("interactfocus: beat - wrote %ld, survived to the next tick %ld, "
            "overwritten %ld, dropped %ld. survived=0 with wrote>0 means the engine "
            "recomputes the field after us and the real fix belongs at its writer; "
            "survived>0 with no pickup means the use path does not read this field.",
            g_ifHeld, g_ifStuck, g_ifLost, g_ifDropped);
    }
}
