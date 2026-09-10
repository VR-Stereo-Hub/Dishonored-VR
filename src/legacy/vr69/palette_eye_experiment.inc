// Retired PR #26 eye-decision experiment, retained for inspection.
// Included only in legacy builds; the active path uses the pre-regression decision.
static void MpFlickNote(const char* why)
{
    const double now = MaimNowMs();
    g_mpFlickLastMs = now;
    ++g_mpFlickCount;
    if (!g_mpFlickOn) {
        g_mpFlickOn = true;
        g_mpFlickStartMs = now;
        g_mpFlickCount = 1;
        g_mpFlickYaw0 = g_hmdYaw;
        ++g_mpFlickWindows;
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
            "ms/palette/flicker: window %d OPEN at head yaw %+.1f deg - %s. While "
            "this is open the eye behind the weapon correction is being guessed, "
            "and a wrong guess is one frame of every weapon displaced by a full "
            "IPD, mirrored between the eyes.", g_mpFlickWindows, g_mpFlickYaw0 * 57.29578f, why);
    }
}


// Closed from the per-present path once the unreadable presents stop.
static void MpFlickTick(void)
{
    if (!g_mpFlickOn) return;
    const double now = MaimNowMs();
    if (now - g_mpFlickLastMs < 500.0) return;
    g_mpFlickOn = false;
    DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
        "ms/palette/flicker: window %d CLOSED after %.2f s - %ld unreadable "
        "present(s), head yaw %+.1f -> %+.1f deg. %ld of them were ALTERNATED "
        "rather than held (PaletteEyeAlternate=%d). If the flicker stopped when "
        "this window closed, the eye guess is the mechanism; if it kept going, "
        "it is not.", g_mpFlickWindows,
        (g_mpFlickLastMs - g_mpFlickStartMs) / 1000.0, g_mpFlickCount,
        g_mpFlickYaw0 * 57.29578f, g_hmdYaw * 57.29578f, g_mpEyeFlipped,
        (int)g_mpEyeAlternate);
}


static void MpEyeForPresentLegacy(const MpDrawCtx* c)
{
    const uint32_t pres = (uint32_t)dvr::frame::count();
    if (pres == g_mpEyePresent) return;          // same Present, decision stands
    g_mpEyePresent = pres;
    MpFlickTick();               // does the open window end at this present?

    // MONO HAS NO EYE, so there is nothing here to decide and nothing to offset.
    // Without this the inference reads every mono present as "the step was too
    // small to tell" - correctly, the step is zero - and the alternation then
    // flips the hands by a full inter-pupillary distance every single frame.
    // Measured as four windows of 565, 396, 350 and 293 consecutive unreadable
    // presents, every one of them mono, and seen as the hands flickering on a
    // loading screen. Holding was harmless here; alternating is not, so the
    // guard belongs with the decision rather than inside the alternation.
    if (!InterlockedCompareExchange(&g_sdDoublingNow, 0, 0)) {
        g_mpEyeState = 0;        // no offset at all, which is the mono placement
        g_mpEyeHavePrev = false; // and the next pair starts from a clean compare
        return;
    }

    const float ipdUU = g_ipdM * ((g_skcWorldScale > 1.0f ? g_skcWorldScale : 100.0f)
                                  * g_mpDriveGain);
    if (!g_mpEyeHavePrev) {
        g_mpEyeHavePrev = true; g_mpEyePrevFirst = c->projRight;
        g_mpEyeState = 0;                        // nothing to compare against yet
        return;
    }
    const float d = c->projRight - g_mpEyePrevFirst;
    const float ad = fabsf(d);
    if (ad > 0.45f * ipdUU && ad < 2.0f * ipdUU) {
        // The eye changed. The SIGN gives it absolutely, with no vote: the
        // smaller right-axis projection is the right eye.
        g_mpEyeState = (d < 0.0f) ? +1 : -1;
        g_mpEyeToggles++;
    } else if (ad <= 0.45f * ipdUU) {
        // TOO SMALL TO TELL APART - which is not the same thing as the same eye,
        // and the difference is the whole bug. Holding the previous answer is
        // the one choice guaranteed to be wrong when the method alternates, and
        // it alternates: pass 1 pushes the left tag, pass 2 the right, measured
        // at 148 tags for 147 presents.
        g_mpEyeSame++;
        if (g_mpEyeAlternate && g_mpEyeState != 0) {
            g_mpEyeState = -g_mpEyeState;
            InterlockedIncrement(&g_mpEyeFlipped);
        }
        MpFlickNote("the eye step was too small to read");
    } else {
        g_mpEyeState = 0;                        // head moved too far to judge
        g_mpEyeAmbiguous++;
        MpFlickNote("the head moved further than an eye step between presents");
    }
    g_mpEyePrevFirst = c->projRight;
}


static int MpEyeForDrawLegacy(const MpDrawCtx* c)
{
    // ---- WHICH EYE IS THIS DRAW? ------------------------------------------
    //
    // Asked, not inferred. The re-entry method draws the world twice per game
    // tick with the camera moved between the passes, so a draw running inside
    // one of them already knows its eye - the code that moved the camera is one
    // stack frame up, on this thread.
    //
    // Both halves of that sentence are checked, because the first version of
    // this audit checked neither and its numbers meant nothing. g_sdInDrawTid
    // carries THIS thread's id only while a doubled viewport draw is on the
    // stack, and the camera seam's second-pass latch is per-thread too. A draw
    // that fails either test is in a pass we do not own - a shadow or depth pass
    // fed by the same palette - and it stays UNKNOWN and keeps the inference.
    //
    // MEASURED, first run: on the population that IS inside a pass, the
    // inference disagreed with the drawing pass 39% of the time, steadily, all
    // through gameplay. That is the flicker's mechanism - a full inter-pupillary
    // offset applied in the wrong direction for one frame, on every weapon at
    // once, mirrored between the eyes, which is exactly what was reported.
    //
    // The inference has two documented ways to produce that. It HOLDS the
    // previous answer whenever the sideways jump is too small to read (1922
    // presents in the first run), and it is keyed on the present counter, which
    // runs at roughly twice the doubled-draw rate.
    const LONG inDraw = InterlockedCompareExchange(&g_sdInDrawTid, 0, 0);
    const int truth = (inDraw && (DWORD)inDraw == GetCurrentThreadId())
                          ? (dvr::camera::second_pass_for_current_thread() ? +1 : -1)
                          : 0;
    int eyeUse = g_mpEyeState;
    if (!truth) {
        InterlockedIncrement(&g_mpEyeNoTruth);
    } else {
        if (truth == g_mpEyeState) InterlockedIncrement(&g_mpEyeAgree);
        else {
            InterlockedIncrement(&g_mpEyeDisagree);
            if (InterlockedIncrement(&g_mpEyeSaid) <= 12)
                DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                    "ms/palette/eyeaudit: the pass drawing this says %s, the "
                    "inference says %s (projRight %.3f, prev %.3f, delta %.3f). "
                    "Uncorrected that is one frame of every weapon displaced by a "
                    "full IPD the wrong way, mirrored between the eyes.",
                    truth < 0 ? "LEFT" : "RIGHT",
                    g_mpEyeState < 0 ? "LEFT" : g_mpEyeState > 0 ? "RIGHT" : "unknown",
                    c->projRight, g_mpEyePrevFirst, c->projRight - g_mpEyePrevFirst);
        }
        // THE FIX. Take the eye from the pass. 'unknown' stops existing inside a
        // doubled draw, so the half-IPD placement the inference fell back to
        // when it could not tell goes away with it.
        if (g_mpEyeFromPass) eyeUse = truth;
    }

    return eyeUse;
}
