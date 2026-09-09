// game/dishonored/startup.cpp - score a load, phase by phase (VR-62).
//
// Read-only. It samples booleans that already exist, records when each first
// became true, and prints one summary per load. It changes no behaviour and
// gates nothing; the state block carries the argument for why it exists.
//
// LANE: the present thread, from GameStateTick, once per frame. Every sample is
// a boolean read of something the state machine reads anyway, so this cannot be
// the reason a phase is slow.


// LAST false-to-true, not first. These terms TOGGLE, and recording the first
// time each went true made the summary lie: the clock starts when the game
// leaves gameplay, at which point the outgoing pawn is still alive, so
// pawn-ptr and cyl both recorded +0.00 s and the report blamed whichever term
// happened to be sampled last. A term that goes true, false, then true again
// settled at the LAST transition, and that is the number that explains a wait.
static void SuSample(int idx, bool nowTrue)
{
    if (!g_suArmed || idx < 0 || idx >= SU_COUNT) return;
    if (!nowTrue) { g_su[idx].ms = 0.0; return; }
    if (g_su[idx].ms == 0.0) g_su[idx].ms = MaimNowMs() - g_suT0;
}

// One-way milestones: an event that happened rather than a state that holds.
static void SuMark(int idx)
{
    if (!g_suArmed || idx < 0 || idx >= SU_COUNT) return;
    if (g_su[idx].ms == 0.0) g_su[idx].ms = MaimNowMs() - g_suT0;
}


// A new load. Called when the state machine leaves GAMEPLAY, which is the only
// honest start for the clock: it is the last moment we know the picture was
// right.
static void SuBeginLoad(void)
{
    if (!g_suOn) return;
    g_suT0 = MaimNowMs();
    g_suArmed = true;
    g_suReported = false;
    g_suStereoSeen = false;
    for (int i = 0; i < SU_COUNT; ++i) g_su[i].ms = 0.0;
    g_su[SU_LOAD].ms = 0.0001;   // non-zero so "seen" and "at t=0" differ
    ++g_suLoads;
}


// THE SUMMARY. Printed once per load, when the mono window ends.
//
// It names the term that cleared LAST before the verdict, because that is the
// one that owns the wait - and it prints the gap between the earliest evidence
// the world was live and the moment we admitted it, which is the number this
// whole ticket is about.
static void SuReport(void)
{
    if (!g_suArmed || g_suReported) return;
    g_suReported = true;

    const double stereo = g_su[SU_STEREO].ms;

    // The earliest evidence that a real, live player existed. These are
    // INDEPENDENT of the state machine's own terms, which is what makes the
    // comparison meaningful rather than circular.
    double earliest = 0.0;
    const char* earliestKey = "none";
    // ONLY genuine live-player signals. GNames being populated is deliberately
    // excluded: it proves the name pool is up, not that a player exists, and
    // including it would manufacture a gap that is not there.
    const int evidence[] = { SU_PAWN_PTR, SU_INV_OK };
    for (int i = 0; i < 2; ++i) {
        const double t = g_su[evidence[i]].ms;
        if (t > 0.0 && (earliest == 0.0 || t < earliest)) {
            earliest = t; earliestKey = g_su[evidence[i]].key;
        }
    }

    // Which of the state machine's own terms cleared last. That is the fix
    // target: everything else was ready and waiting on it.
    double lastTerm = 0.0;
    const char* lastKey = "none";
    const int terms[] = { SU_CYL, SU_NOMENU, SU_VIEW, SU_NOCINE };
    for (int i = 0; i < 4; ++i) {
        const double t = g_su[terms[i]].ms;
        if (t > lastTerm) { lastTerm = t; lastKey = g_su[terms[i]].key; }
    }

    Log("startup: load #%d scored - the mono window was %.2f s.", g_suLoads,
        stereo > 0.0 ? stereo / 1000.0 : -1.0);
    for (int i = 0; i < SU_COUNT; ++i)
        Log("startup:   %-10s %8s  %s", g_su[i].key,
            g_su[i].ms > 0.0 ? "" : "NEVER", g_su[i].what);
    for (int i = 0; i < SU_COUNT; ++i)
        if (g_su[i].ms > 0.0)
            Log("startup:   %-10s %+8.2f s", g_su[i].key, g_su[i].ms / 1000.0);

    // THE VERDICT ON OURSELVES, and it has to be able to say we are not at
    // fault. A gap near zero means every signal arrived together and the wait
    // belongs to the game's own load, which is a real answer and not a failure
    // of this instrument.
    const double gap = (earliest > 0.0 && lastTerm > earliest)
                       ? (lastTerm - earliest) : 0.0;
    if (earliest <= 0.0) {
        Log("startup: NO INDEPENDENT EVIDENCE of a live player was seen this "
            "load, so this instrument cannot say whether the wait was ours. "
            "That is a gap in the instrument, not a finding.");
    } else if (gap < 250.0) {
        Log("startup: the earliest evidence of a live player ('%s') and the last "
            "state term to clear ('%s') are %.2f s apart. **The wait is NOT "
            "ours** - our terms cleared as soon as there was anything to clear, "
            "so the mono window is the game's own load and shortening it means "
            "finding an earlier signal, not fixing a late one.",
            earliestKey, lastKey, gap / 1000.0);
    } else {
        Log("startup: **%.2f s OF THE MONO WINDOW IS OURS.** A live player was "
            "already provable at %+.2f s ('%s' - we were reading that pawn), but "
            "'%s' did not clear until %+.2f s, and the verdict waited on it. "
            "That term is the fix target: every other signal was ready and "
            "waiting. This says nothing about whether the term is WRONG - only "
            "that it is last.",
            gap / 1000.0, earliest / 1000.0, earliestKey, lastKey,
            lastTerm / 1000.0);
    }
}


// Sampled once per frame from GameStateTick, on the present thread. `cyl`,
// `view` and the menu flags are passed in rather than re-read, so this scores
// exactly the values the state machine decided on and cannot disagree with it.
static void SuTick(bool cyl, bool noMenu, bool view, bool noCine, bool verdict)
{
    if (!g_suOn || !g_suArmed) return;

    SuSample(SU_PAWN_PTR, FpPawn() != NULL);
    // GNames populated is a FLOOR on everything that resolves by name, recorded so
    // a late signal can be told apart from a late name pool. It is NOT evidence of
    // a live pawn: resolving a property offset walks GObjects and needs no pawn,
    // which is why the summary below ignores it.
    if (RflNamesReady()) SuMark(SU_NAMES);   // one-way: the pool never empties
    // The equipment read IS live-player evidence: it calls FpPawn(), refuses
    // without one, and only succeeds once a real inventory reads back.
    SuSample(SU_INV_OK, g_rflState.ok);

    SuSample(SU_CYL,     cyl);
    SuSample(SU_NOMENU,  noMenu);
    SuSample(SU_VIEW,    view);
    SuSample(SU_NOCINE,  noCine);
    SuSample(SU_VERDICT, verdict);

    if (g_suStereoSeen) {
        SuMark(SU_STEREO);
        SuReport();
    }
}


static bool SuCommand(const char* args)
{
    if (!args) args = "";
    if (!strcmp(args, "status") || !args[0]) {
        Log("startup: %s, %d load(s) scored, %s",
            g_suOn ? "on" : "OFF", g_suLoads,
            g_suArmed ? (g_suReported ? "this load reported" : "scoring a load now")
                      : "idle (no load seen since the mod started)");
        for (int i = 0; i < SU_COUNT; ++i)
            Log("startup:   %-10s %s", g_su[i].key,
                g_su[i].ms > 0.0 ? "seen" : "not yet");
        return true;
    }
    bool b = false;
    if (DvrOnOff(args, &b)) { g_suOn = b; Log("startup: timing %s", b ? "on" : "off"); return true; }
    Log("startup: status | on | off");
    return true;
}
