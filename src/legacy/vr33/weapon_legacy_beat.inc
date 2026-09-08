    Log("wa: beat v2 | routed %ld compared %ld; member comparisons L %ld R %ld; "
        "contracts %ld ambiguity %ld misses %ld; attempted %ld succeeded %ld restore-fail %ld; "
        "no-layout %ld no-source %ld no-view %ld no-bridge %ld stale-snapshot %ld over-budget %ld | "
        "ghost passes seen %ld fixed %ld (no bone decl %ld, no sibling delta %ld, bad range %ld) | "
        "probe ran %ld: shares our vertex buffer %ld (same index buffer %ld), not ours %ld, "
        "over budget %ld | "
        "contract table %d of %d | off-rig members %ld, other-instance draws %ld, view-model accepts %ld "
        "refusals %ld | "
        "other passes on known buffers %ld: corrected %ld "
        "(no bone decl %ld, no sibling delta %ld) | non-indexed %ld examined %ld: "
        "on known buffers %ld corrected %ld (no bone decl %ld, no delta %ld) | "
        "components: %d bridge anchor(s), "
        "%d member(s), %d dropped, %ld tick(s) with nothing to attach | "
        "stance %s (eye %.1f uu) | %s",
        g_waSeen, g_waCandChecked, g_waHandCompared[0], g_waHandCompared[1],
        g_waMatched, g_waAmbiguous, g_waNoCandidate, g_waAttempted, g_waSucceeded,
        g_waRestoreFail, g_waNoLayout, g_waNoSource, g_waNoCommon, g_waNoBridge, g_waStaleComp,
        g_waBudgetSkip, g_waGhostSeen, g_waGhostFixed, g_waGhostNoBone,
        g_waGhostNoDelta, g_waGhostRange, g_waProbeRan, g_waProbeVbHit,
        g_waProbeIbHit, g_waProbeMiss, g_waProbeCapped,
        g_waMeshN, (int)WA_MAX_MESH,
        g_waOffRig, g_waOffPass, g_waNearAccepted, g_waNearRejected,
        g_waIdSeen, g_waIdCorrected, g_waIdNoBone, g_waIdNoDelta,
        g_waNonIndexed, g_waPrimSeen, g_waPrimVbHit, g_waPrimFixed,
        g_waPrimNoBone, g_waPrimNoDelta,
        g_waRefN, g_waMemberN, g_waDroppedN, g_waNotReady,
        // THE STANCE. It read "standing (eye 0.0 uu)" for every sample of a run
        // in which the tester deliberately spent half the time crouched: the
        // eye height was never resolved, so the flag was 0 BY DESIGN and the
        // line said "standing" anyway. A zero that is expected has to say so ON
        // THE LINE (CLAUDE.md), and this one did not - it cost the correlation
        // the run was made to capture.
        (!g_actorLocFound || g_eyeNowUU == 0.0f)
            ? "UNKNOWN (Actor.Location unresolved - NOT a report of standing)"
            : (g_eyeCrouched ? "CROUCHED" : "standing"),
        (double)g_eyeNowUU,
        g_waWhy);
