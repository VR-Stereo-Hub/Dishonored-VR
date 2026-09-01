// game/dishonored/console.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// 32.84: READ THE ANSWER. The 32.83 call worked end to end - ConsoleCommand
// dispatched on DishonoredPlayerController, the Console fired OutputText, no
// crash - and the resolution did not change. The engine REPLIED and the reply
// went in the bin, because ReturnValue was never read. The reply is the
// engine stating, in text, why it will not do this. So: run a short script of
// commands, one every 2 seconds, and log what the engine says to each.
//   setres WxHw / setres WxH  - the viewport resize, both spellings
//   scale set ResX/ResY       - FSystemSettings::Exec, the other door in
//   scale get ResX            - did the set land?
static int RunConsole(const wchar_t* wcmd, char* reply, int replyCap)
{
    if (reply) reply[0] = 0;
    if (!g_fnConsoleCmd || !g_peCtrl) return -1;
    wchar_t cmd[96];
    _snwprintf(cmd, 96, L"%s", wcmd);
    const int len = (int)wcslen(cmd) + 1;      // UE3 counts the terminator
    struct Parms {
        wchar_t* data;  int num;  int max;     // FString Command
        int      bWriteToLog;
        wchar_t* rdata; int rnum; int rmax;    // FString ReturnValue
    } parms;
    memset(&parms, 0, sizeof(parms));
    parms.data = cmd; parms.num = len; parms.max = len;
    parms.bWriteToLog = 1;
    g_peReentry = true;
    ((PFN_ProcessEventCall)kProcessEvent)(g_peCtrl, g_fnConsoleCmd, &parms, NULL);
    g_peReentry = false;
    int got = 0;
    if (reply && parms.rdata && parms.rnum > 0 && parms.rnum < 4096 &&
        RangeReadable(parms.rdata, (size_t)parms.rnum * 2)) {
        for (int i = 0; i < parms.rnum - 1 && got < replyCap - 1; i++) {
            wchar_t wc = parms.rdata[i];
            reply[got++] = (wc >= 32 && wc < 127) ? (char)wc
                          : (wc == L'\n' ? '|' : '?');
        }
        reply[got] = 0;
    }
    // ReturnValue was allocated by the engine; a handful of leaked strings
    // during a one-shot diagnostic is fine, freeing with the wrong allocator
    // is not.
    return got;
}


// 38.69: fire the developer level-transition once the intro pawn is up.
// 38.70: the NewGameClicked arm NEVER FIRED (measured - the main menu's
// click never reaches ProcessEvent as that name; the menu closed via the
// auto-start fallback and the skip sat unarmed while the boat played).
// The trigger is now the one signal that cannot miss: the intro boat's
// spawn point is a fixed world coordinate, measured at (-3901,36639,-225)
// across runs. Any fresh pawn standing near it IS a new game's intro -
// a mid-game save can never be there, and a save AT the intro start dies
// at the dock anyway, so skipping it too is correct.
static void IntroSkipApply()
{
    if (!g_introSkip || g_peReentry) return;
    double now = MaimNowMs();
    // 38.71: judge a fired skip - a transition that took gives a NEW pawn
    // latch; one that half-fired must not leave the controllers parked.
    // 38.72: the verdict is POSITIONAL. The working transition is a streaming
    // level change - the pawn pointer survives it (measured: same pawn,
    // teleported 29000 uu to a staging point within 2.5 s, then the cell),
    // so "new pawn latch" judged a successful jump as failed. TOOK = the pawn
    // is far from the boat; DID NOT TAKE = still on the boat after 15 s, and
    // then it is fired again (3 tries) - slow machines need the level's
    // Kismet to finish streaming before the receiver exists.
    const float kBoatX = -3901.0f, kBoatY = 36639.0f, kBoatZ = -225.0f;
    if (g_introSkipDone) {
        if (g_introSkipJudged || g_introSkipFiredMs <= 0.0) return;
        bool isFar = false, isNear = false;
        if (g_pePawn && g_actorLocFound &&
            RangeReadable(g_pePawn + g_actorLocOff, 12)) {
            const float* P = (const float*)(g_pePawn + g_actorLocOff);
            float ddx = P[0] - kBoatX, ddy = P[1] - kBoatY;
            float d2 = ddx * ddx + ddy * ddy;
            isFar  = d2 > 15000.0f * 15000.0f;
            isNear = d2 < 6000.0f * 6000.0f;
        }
        if (isFar || g_fbPawnMs > g_introSkipFiredMs) {
            g_introSkipJudged = true;
            Log("introskip: transition TOOK (pawn left the boat area)");
        } else if (now - g_introSkipFiredMs > 15000.0) {
            g_introSkipJudged = true;
            Log("introskip: transition DID NOT TAKE (still at the boat after "
                "15s, try %d of 3)", g_introSkipTries);
            if (g_cineNow && g_cineOnMs >= g_introSkipFiredMs) {
                g_cineNow = false;
                Log("cine: latch cleared - it was raised by the failed skip, "
                    "inputs live again");
            }
            if (isNear && g_introSkipTries < 3) {
                g_introSkipDone   = false;      // re-arm: fire again shortly
                g_introSkipJudged = false;
                g_introSkipRetryMs = now + 3000.0;
                Log("introskip: retrying in 3 s");
            }
        }
        return;
    }
    if (!g_pePawn || !CylTruthLive()) return;   // no live level yet
    if (!g_actorLocFound || !RangeReadable(g_pePawn + g_actorLocOff, 12))
        return;                       // offset not found yet - retry next tick
    if (g_introSkipTries == 0) {
        if (g_fbPawnMs <= 0.0 || now - g_fbPawnMs < 1000.0) return;
        // 38.73: POLL, don't check once. A single check at DelayMs missed a
        // run where the pawn was still at the engine's pre-placement spot
        // (0,500,-200) 6 s after latch (measured: "pawn at (0,500,-208.8) is
        // not the intro boat") - it reached the boat later and the one shot
        // was already spent. Now: every 500 ms for 90 s after the latch;
        // fire the moment the pawn is at the boat, give up (logged once) if
        // it never is. A mid-game load never comes near that coordinate.
        static double pollLatch = -1.0, pollNext = 0.0, atBoatMs = 0.0;
        static bool   gaveUp = false;
        if (g_fbPawnMs != pollLatch) {
            pollLatch = g_fbPawnMs; pollNext = 0.0; gaveUp = false; atBoatMs = 0.0;
        }
        if (gaveUp || now < pollNext) return;
        pollNext = now + 500.0;
        const float* L = (const float*)(g_pePawn + g_actorLocOff);
        float dx = L[0] - kBoatX, dy = L[1] - kBoatY, dz = L[2] - kBoatZ;
        if (dx * dx + dy * dy > 4000.0f * 4000.0f || dz > 1000.0f || dz < -1000.0f) {
            if (now - g_fbPawnMs > 90000.0) {
                gaveUp = true;
                Log("introskip: pawn at (%.0f,%.0f,%.1f) never reached the intro "
                    "boat in 90 s - not a new game, no skip", L[0], L[1], L[2]);
            }
            return;
        }
        // 38.74: the delay counts from PLACEMENT at the boat, not from the
        // latch. Firing 6 s after latch produced one run with a screen that
        // stayed black in the cell while everything else (events, stereo
        // splices, pause menu, movement) matched the working runs line for
        // line - the one thing that varies between runs is when the pawn is
        // placed (3.4 s vs >6 s after latch), i.e. where the intro's own
        // fade-in is when we fire. Waiting DelayMs after placement keeps the
        // transition clear of the intro's fade.
        if (atBoatMs <= 0.0) {
            atBoatMs = now;
            Log("introskip: pawn at the intro boat (%.0f,%.0f,%.1f) %.1f s after "
                "latch - firing in %d ms", L[0], L[1], L[2],
                (now - g_fbPawnMs) * 0.001, g_introSkipDelayMs);
        }
        if (now - atBoatMs < (double)g_introSkipDelayMs) return;
    } else {
        if (now < g_introSkipRetryMs) return;   // a retry, already at the boat
    }
    g_introSkipTries++;
    g_introSkipDone  = true;
    g_introSkipFiredMs = now;
    const wchar_t* cmd = (g_introSkip == 1)
        ? L"ce ChangeLvl_fromPrison_toTower"
        : L"ce ChangeLvl_fromTower_toPrison";
    char reply[256];
    RunConsole(cmd, reply, sizeof(reply));
    Log("introskip: FIRED dev transition %d (%s), try %d -> \"%s\" - jumping "
        "past the broken boat arrival", g_introSkip,
        g_introSkip == 1 ? "to Dunwall Tower" : "to Prison", g_introSkipTries,
        reply[0] ? reply : "(empty reply)");
}


static void SetResApply()
{
    if (g_setResDone || !g_forceResW || !g_forceResH) return;
    // 38.91: already there? Then do NOTHING. No console commands, no Resets,
    // no aspect/FOV churn seconds into play. (The client-rect lie that keeps
    // the render at this size in a small window is untouched - only the
    // redundant re-forcing goes away.)
    if (g_liveBbW == g_forceResW && g_liveBbH == g_forceResH) {
        g_setResDone = true;
        Log("setres: the game is already at %ux%u - skipping the resolution "
            "script entirely (no mid-session Resets)", g_liveBbW, g_liveBbH);
        return;
    }
    if (g_peReentry) return;
    if (!g_peCtrl || !LooksLikeObj(g_peCtrl)) return;
    static int warm = 0;
    if (++warm < 3000) return;

    static int   step = 0;
    static DWORD nextAt = 0;
    DWORD now = GetTickCount();
    if (nextAt == 0) nextAt = now + 500;
    if (now < nextAt) return;
    nextAt = now + 2000;

    if (!g_fnConsoleCmd) g_fnConsoleCmd = FindFunctionObj("ConsoleCommand");
    if (!g_fnConsoleCmd) {
        Log("setres: no UFunction named ConsoleCommand - cannot ask the engine");
        g_setResDone = true;
        return;
    }

    wchar_t cmd[96];
    char reply[512];
    switch (step) {
    case 0:
        _snwprintf(cmd, 96, L"setres %ux%uw", g_forceResW, g_forceResH);
        break;
    case 1:
        _snwprintf(cmd, 96, L"setres %ux%u", g_forceResW, g_forceResH);
        break;
    case 2:
        _snwprintf(cmd, 96, L"scale set ResX %u", g_forceResW);
        break;
    case 3:
        _snwprintf(cmd, 96, L"scale set ResY %u", g_forceResH);
        break;
    case 4:
        _snwprintf(cmd, 96, L"scale get ResX");
        break;
    default:
        g_setResDone = true;
        Log("setres: script done - if no Reset followed, the replies above "
            "are the engine's stated reasons");
        return;
    }
    step++;
    RunConsole(cmd, reply, sizeof(reply));
    char narrow[96];
    for (int i = 0; i < 96; i++) { narrow[i] = (char)cmd[i]; if (!cmd[i]) break; }
    Log("setres: [%s] -> \"%s\"", narrow, reply[0] ? reply : "(empty reply)");
}
