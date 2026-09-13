// game/dishonored/hands/menu_keep.cpp - VR-93, included by src/mod/dishonoredvr.cpp
// (unity build). The lifecycle and the validation rules are menu_keep.h, which a
// host test drives; this file is the engine side: reading an object's identity,
// the two lanes' ticks, and the one-line resume report.
//
// LANES. The machine and every contract invalidation are the PRESENT lane's
// (GameStateTick, the thread that draws). The validation and every candidate
// invalidation it causes are the SCRIPT lane's (ApplyHandToMesh, first thing).
// They meet through epochs: the present lane bumps g_mkWant after writing the
// pawn and controller identities it wants checked; the script lane validates
// once per epoch and publishes the verdict epoch LAST, after the reason text.

static dvr::menukeep::Machine  g_mkMachine;           // present lane only
static dvr::menukeep::Identity g_mkPawnId, g_mkCtrlId; // present writes, then bumps g_mkWant
static LONG          g_mkLoadAt       = 0;            // present: g_mkLoadEvents at the suspend
static volatile LONG g_mkWant         = 0;            // present: the validation asked for
static LONG          g_mkDone         = 0;            // script: the last one validated
static volatile LONG g_mkVerdictKeep  = 0;            // script, written before the epoch
static volatile LONG g_mkVerdictEpoch = 0;            // script, written last
static LONG          g_mkConsumed     = 0;            // present
static char          g_mkWhy[240]     = "";           // script, before the epoch
static char          g_mkLast[240]    = "no menu yet";// present: what the last transition did
// g_mkOn, g_mkKept and g_mkInvalidated live with the weapon-attach state (57b),
// where the config reader and the F10 panel can see them.

// The resume report's clock. Present lane only.
static struct {
    bool     armed;
    bool     seenGameplay;       // at least one GAMEPLAY this process
    bool     lastGameplay;
    double   t0;
    uint32_t compGen;
    LONG     succeeded, matched;
    int      cands, contracts;
    bool     dblAt0, dblSawOff;  // the DOUBLE flag is the script lane's latest
    double   dbl, snap, corr;    // ms after t0, 0 = not yet
    char     kind[48];
} g_mkRes;


// What an object IS right now: its class and FName, read off the object.
// Unreadable leaves obj null, which the validation treats as a failure for
// anything that was retained.
static void MkReadIdentity(void* obj, dvr::menukeep::Identity* out)
{
    *out = dvr::menukeep::Identity{};
    if (!obj || ((uintptr_t)obj & 3) || !RangeReadable(obj, kClassOff + 4)) return;
    const uint8_t* o = (const uint8_t*)obj;
    out->obj     = obj;
    out->name[0] = *(uint32_t*)(o + kNameOff);
    out->name[1] = *(uint32_t*)(o + kNameOff + 4);
    out->cls     = *(void**)(o + kClassOff);
}

static bool MkLive(void*, void* obj) { return IsLiveObject((uint8_t*)obj); }
static bool MkRead(void*, void* obj, dvr::menukeep::Identity* out)
{
    MkReadIdentity(obj, out);
    return out->obj != nullptr;
}


// ---- script lane --------------------------------------------------------------

static void MkFail(dvr::menukeep::Verdict* v, const char* fmt, const char* what, void* obj)
{
    v->keep = false; ++v->failed;
    if (!v->why[0]) snprintf(v->why, sizeof(v->why), fmt, what, obj);
}

static void MkScriptTick(void)
{
    const LONG want = g_mkWant;
    if (want == g_mkDone) return;
    g_mkDone = want;

    const double t0 = MaimNowMs();
    dvr::menukeep::Verdict v;
    dvr::menukeep::Reader  r;
    r.live = MkLive;
    r.read = MkRead;
    // THE TABLE MUST POSTDATE THE MENU. IsLiveObject answers "was this alive
    // when the table was built", and the table is otherwise a main-menu
    // snapshot (TRAPS, VR-88) - so it is rebuilt here, for this question.
    const bool built = BuildLiveSet();
    const double tBuilt = MaimNowMs();
    if (!built) {
        v.keep = false; ++v.failed;
        snprintf(v.why, sizeof(v.why), "the live-object table rebuild was refused (object "
                 "table busy or implausible), so nothing retained can be shown alive");
    } else {
        dvr::menukeep::check(r, g_mkPawnId, "the pawn", true, &v);
        if (v.keep && g_mkPawnId.obj != (void*)g_pePawn)
            MkFail(&v, "%s: the latched pawn %p is no longer the one recorded", "the pawn", (void*)g_pePawn);
        if (g_mkCtrlId.obj) {
            dvr::menukeep::check(r, g_mkCtrlId, "the controller", false, &v);
            if (v.keep && g_mkCtrlId.obj != (void*)g_peCtrl)
                MkFail(&v, "%s: the latched controller %p is no longer the one recorded",
                       "the controller", (void*)g_peCtrl);
        }
        char what[96];
        for (int i = 0; i < g_fpCandN && i < 24; ++i) {
            const FpCand* k = &g_fpCand[i];
            if (!k->obj) continue;
            _snprintf(what, sizeof(what), "candidate '%s' (%s)", k->name, k->asset);
            what[sizeof(what) - 1] = 0;
            if (k->id.obj != (void*)k->obj) { MkFail(&v, "%s %p has no recorded identity", what, k->obj); continue; }
            dvr::menukeep::check(r, k->id, what, false, &v);
        }
        // The contracts are the present lane's; this only READS their component
        // fields. A contract adopted mid-validation can show a component with no
        // identity yet, and that fails - the safe direction.
        for (int i = 0; i < g_waMeshN && i < WA_MAX_MESH; ++i) {
            const WaMesh* w = &g_waMesh[i];
            if (!w->vb || !w->compObj) continue;
            _snprintf(what, sizeof(what), "contract '%s' hand %d", w->asset, w->hand);
            what[sizeof(what) - 1] = 0;
            if (w->compId.obj != w->compObj) { MkFail(&v, "%s component %p has no recorded identity", what, w->compObj); continue; }
            dvr::menukeep::check(r, w->compId, what, false, &v);
        }
    }
    const int cands = g_fpCandN, contracts = g_waMeshN;
    if (!v.keep) FpInvalidateCandidates("a menu's retained records failed validation");

    _snprintf(g_mkWhy, sizeof(g_mkWhy), "%s", v.keep ? "" : v.why);
    g_mkWhy[sizeof(g_mkWhy) - 1] = 0;
    InterlockedExchange(&g_mkVerdictKeep, v.keep ? 1 : 0);
    InterlockedExchange(&g_mkVerdictEpoch, want);

    Log("menukeep: validation #%ld %s - %d object(s) checked (pawn, controller, %d candidate(s), "
        "%d contract(s)), %d failed%s%s | live-object table rebuilt in %.1f ms (%u entries), "
        "validation %.1f ms. A pass means every retained object is in a table built NOW and still "
        "carries the class and FName it had when recorded; a reused address fails on either.",
        (long)want, v.keep ? "PASSED" : "FAILED", v.checked, cands, contracts, v.failed,
        v.keep ? "" : ": ", v.keep ? "" : v.why,
        tBuilt - t0, (unsigned)g_liveN, MaimNowMs() - tBuilt);
}


// ---- present lane -------------------------------------------------------------

static void MkInvalidate(const char* reason, const char* detail, bool candidates)
{
    // Exactly the pre-VR-93 transition, in its order.
    UiNoteLoad();
    WaInvalidateContracts(reason);
    if (candidates) FpInvalidateCandidates(reason);
    ++g_mkInvalidated;
    _snprintf(g_mkLast, sizeof(g_mkLast), "INVALIDATED - %s%s%s", reason,
              detail && detail[0] ? ": " : "", detail ? detail : "");
    g_mkLast[sizeof(g_mkLast) - 1] = 0;
    if (g_mkOn)
        Log("menukeep: %s. The weapons re-identify from scratch.", g_mkLast);
}

static void MkResumeTick(const char* state, bool gameplay)
{
    const double now = MaimNowMs();
    if (gameplay && !g_mkRes.lastGameplay) {
        _snprintf(g_mkRes.kind, sizeof(g_mkRes.kind), "%s",
                  g_mkRes.seenGameplay ? "back to GAMEPLAY" : "first GAMEPLAY");
        g_mkRes.armed = true;
        g_mkRes.seenGameplay = true;
        g_mkRes.t0 = now;
        g_mkRes.compGen = g_waCompGen;
        g_mkRes.succeeded = g_waSucceeded;
        g_mkRes.matched = g_waMatched;
        g_mkRes.cands = g_fpCandN;
        g_mkRes.contracts = g_waMeshN;
        g_mkRes.dbl = g_mkRes.snap = g_mkRes.corr = 0.0;
        g_mkRes.dblAt0 = g_sdDoublingNow != 0;
        g_mkRes.dblSawOff = false;
    }
    g_mkRes.lastGameplay = gameplay;
    if (!g_mkRes.armed) return;

    const double dt = now - g_mkRes.t0;
    const double stamp = dt > 0.0 ? dt : 0.001;
    // THE DOUBLE FLAG IS A LATEST VALUE, and a pause with no script tick can
    // leave it set from before the menu. Only an off-to-on edge after t0 is a
    // first DOUBLE; a flag that never dropped is reported as exactly that.
    if (!g_mkRes.dbl) {
        if (!g_sdDoublingNow)        g_mkRes.dblSawOff = true;
        else if (g_mkRes.dblSawOff)  g_mkRes.dbl = stamp;
    }
    const bool dblNeverDropped = g_mkRes.dblAt0 && !g_mkRes.dblSawOff;
    if (!g_mkRes.snap && g_waCompGen != g_mkRes.compGen)    g_mkRes.snap = stamp;
    if (!g_mkRes.corr && g_waSucceeded != g_mkRes.succeeded) g_mkRes.corr = stamp;
    const bool all = (g_mkRes.dbl || dblNeverDropped) && g_mkRes.snap && g_mkRes.corr;
    if (!all && gameplay && dt < 5000.0) return;
    g_mkRes.armed = false;

    char a[40], b[40], c[40];
    auto fmt = [&](char* out, double v) {
        if (v) _snprintf(out, 40, "+%.0f ms", v);
        else   _snprintf(out, 40, "NONE in %.0f ms", dt);
        out[39] = 0;
    };
    fmt(a, g_mkRes.dbl); fmt(b, g_mkRes.snap); fmt(c, g_mkRes.corr);
    if (!g_mkRes.dbl && dblNeverDropped) _snprintf(a, 40, "never dropped (on at t0)");
    Log("menukeep/resume: %s%s | first DOUBLE %s, first fresh component snapshot %s, first corrected "
        "weapon draw %s | candidates %d -> %d, contracts %d -> %d, adoptions in the window %ld | "
        "AttachKeepOnMenu=%d, last transition: %s. Contracts that FELL to 0 and adoptions above 0 "
        "are a relearn; contracts that held with 0 adoptions are retention. A NONE for the weapon "
        "draw cannot tell 'no weapon drawn' from 'nothing could be placed'.",
        g_mkRes.kind, gameplay ? "" : " (left gameplay before all three were seen)",
        a, b, c, g_mkRes.cands, g_fpCandN, g_mkRes.contracts, g_waMeshN,
        (long)(g_waMatched - g_mkRes.matched), g_mkOn ? 1 : 0, g_mkLast);
    (void)state;
}

// Once per frame from GameStateTick, after the state is decided.
static void MkPresentTick(const char* state, bool pawnLive)
{
    using namespace dvr::menukeep;
    const bool gameplay = !strcmp(state, "GAMEPLAY");

    // 1. The script lane's verdict, if it answered the validation asked for.
    const LONG ve = g_mkVerdictEpoch;
    if (ve != g_mkConsumed && ve == g_mkWant) {
        g_mkConsumed = ve;
        const bool keep = g_mkVerdictKeep != 0;
        const Phase before = g_mkMachine.phase;
        const Step s = g_mkMachine.verdict(keep, gameplay);
        if (s.action == Action::Invalidate) {
            MkInvalidate(s.reason, g_mkWhy, false);   // the candidates went on the script lane
        } else if (keep && before != Phase::Active) {
            ++g_mkKept;
            _snprintf(g_mkLast, sizeof(g_mkLast), "RETAINED - validation #%ld passed %s",
                      (long)ve, gameplay ? "on resume" : "during the menu");
            g_mkLast[sizeof(g_mkLast) - 1] = 0;
            Log("menukeep: %s; %d contract(s) and %d candidate(s) kept (%ld kept, %ld invalidated "
                "this session).", g_mkLast, g_waMeshN, g_fpCandN, (long)g_mkKept, (long)g_mkInvalidated);
        }
    }

    // 2. The transition.
    Frame f;
    f.leverOn     = g_mkOn;
    f.gameplay    = gameplay;
    f.menu        = !strcmp(state, "MENU");
    f.noPawn      = !strcmp(state, "NO_PAWN");
    f.pawnLive    = pawnLive && g_pePawn != NULL;
    f.pawnChanged = g_mkMachine.phase != Phase::Active && (void*)g_pePawn != g_mkPawnId.obj;
    f.loadAsked   = g_mkMachine.phase != Phase::Active && g_mkLoadEvents != g_mkLoadAt;
    const Step s = g_mkMachine.tick(f);
    switch (s.action) {
    case Action::Suspend:
        g_mkLoadAt = g_mkLoadEvents;
        MkReadIdentity(g_pePawn, &g_mkPawnId);
        MkReadIdentity(g_peCtrl, &g_mkCtrlId);
        UiNoteLoad();   // B1 leaves the observer rescan exactly as it was
        InterlockedIncrement(&g_mkWant);
        _snprintf(g_mkLast, sizeof(g_mkLast), "SUSPENDED - %s", s.reason);
        g_mkLast[sizeof(g_mkLast) - 1] = 0;
        Log("menukeep: %s (state %s). %d contract(s) and %d candidate(s) held, not dropped; "
            "pawn %p FName %u_%u, controller %p FName %u_%u recorded. The script lane publishes no "
            "component snapshot from them before validation #%ld passes, and every correction "
            "needs a snapshot under 100 ms old.", g_mkLast, state, g_waMeshN, g_fpCandN,
            g_mkPawnId.obj, g_mkPawnId.name[0], g_mkPawnId.name[1],
            g_mkCtrlId.obj, g_mkCtrlId.name[0], g_mkCtrlId.name[1], (long)g_mkWant);
        break;
    case Action::Validate:
        InterlockedIncrement(&g_mkWant);
        Log("menukeep: %s - validation #%ld asked of the script lane.", s.reason, (long)g_mkWant);
        break;
    case Action::Invalidate:
        MkInvalidate(s.reason, "", true);
        break;
    default:
        break;
    }

    MkResumeTick(state, gameplay);
}
