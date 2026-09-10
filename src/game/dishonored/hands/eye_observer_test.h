// game/dishonored/hands/eye_observer_test.h - the suite for eye_observer.h.
//
// Every case here exists because something went wrong without it. The first is
// the one that matters: it reproduces the exact defect that produced 105,816
// meaningless comparisons, and it must FAIL to pair.
//
// Run by tools/frame_test on the desk, and by the proxy at init so the log says
// whether the shipped logic passes without the tester running anything.
#pragma once
#include "eye_observer.h"
#include <string.h>
#include <stdio.h>

namespace dvr::eyeobs {

typedef void (*ReportFn)(void* ctx, const char* name, bool pass, const char* detail);

inline void mat_identity(float* m)
{
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// A view matrix whose translation row carries an eye displacement along right.
inline void mat_view(float* m, float rightTranslate)
{
    mat_identity(m);
    m[12] = -rightTranslate;   // row 3, column 0 - the term the recovery reads
}

inline bool suite(ReportFn report, void* ctx)
{
    bool all = true;
    char d[256];
    Observer o;

    // ---- 1. THE DEFECT THAT COST A HEADSET RUN --------------------------
    // One draw context evaluated twice (two hand ranges) must produce NO pair.
    // The old probe compared this with itself and called it 105,816 pairs.
    {
        o.reset();
        Sample s; s.drawId = 7; s.objectId = 1;
        mat_view(s.vp, 3.15f); mat_identity(s.l2w);
        Verdict v1 = o.offer(s);
        Verdict v2 = o.offer(s);          // the SAME draw, second hand range
        const bool pass = (v1 == kNoPair) && (v2 == kSameDraw) &&
                          o.c.pairs == 0 && o.c.refusedSameDraw == 1;
        _snprintf(d, sizeof(d), "v1=%s v2=%s pairs=%u refusedSameDraw=%u",
                  verdict_name(v1), verdict_name(v2), o.c.pairs, o.c.refusedSameDraw);
        report(ctx, "same draw twice forms NO pair", pass, d);
        all = all && pass;
    }

    // ---- 2. Two distinct draws of one object, eye in the VIEW matrix ----
    {
        o.reset();
        Sample a; a.drawId = 10; a.objectId = 4; mat_view(a.vp, -3.15f); mat_identity(a.l2w);
        Sample b; b.drawId = 11; b.objectId = 4; mat_view(b.vp, +3.15f); mat_identity(b.l2w);
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kVpOnly) && o.c.pairs == 1 && o.c.vpOnly == 1;
        _snprintf(d, sizeof(d), "%s maxVp=%.3f at %d maxL2w=%.3f",
                  verdict_name(v), o.lastVp.maxAbs, o.lastVp.maxIndex, o.lastL2w.maxAbs);
        report(ctx, "distinct draws, eye in VP", pass, d);
        all = all && pass;
    }

    // ---- 3. The eye in the OBJECT transform instead ---------------------
    {
        o.reset();
        Sample a; a.drawId = 20; a.objectId = 5; mat_identity(a.vp); mat_view(a.l2w, -3.15f);
        Sample b; b.drawId = 21; b.objectId = 5; mat_identity(b.vp); mat_view(b.l2w, +3.15f);
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kL2wOnly) && o.c.l2wOnly == 1;
        _snprintf(d, sizeof(d), "%s maxL2w=%.3f at %d", verdict_name(v),
                  o.lastL2w.maxAbs, o.lastL2w.maxIndex);
        report(ctx, "distinct draws, eye in L2W", pass, d);
        all = all && pass;
    }

    // ---- 4. Pairing must NOT depend on the quantity being tested --------
    // The old probe keyed on the L2W translation, so a translation change made
    // the pair fail to match - it selected against the very thing it looked for.
    {
        o.reset();
        Sample a; a.drawId = 30; a.objectId = 6; mat_identity(a.vp); mat_view(a.l2w, -50.0f);
        Sample b; b.drawId = 31; b.objectId = 6; mat_identity(b.vp); mat_view(b.l2w, +50.0f);
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kL2wOnly) && o.c.pairs == 1;
        _snprintf(d, sizeof(d), "%s - a large L2W move still PAIRS (identity is an id, not a position)",
                  verdict_name(v));
        report(ctx, "large L2W move still pairs", pass, d);
        all = all && pass;
    }

    // ---- 5. A weighted scalar signature would MISS this -----------------
    // +2 on element 0 and -1 on element 1 cancel in sum(m[i]*(i+1)).
    {
        o.reset();
        Sample a; a.drawId = 40; a.objectId = 7; mat_identity(a.vp); mat_identity(a.l2w);
        Sample b = a; b.drawId = 41;
        b.vp[0] += 2.0f; b.vp[1] -= 1.0f;      // signature unchanged, matrix changed
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kVpOnly) && o.lastVp.maxAbs > 1.0f;
        _snprintf(d, sizeof(d), "%s maxVp=%.3f at %d - a scalar signature would read NEITHER",
                  verdict_name(v), o.lastVp.maxAbs, o.lastVp.maxIndex);
        report(ctx, "cancelling signature is DETECTED", pass, d);
        all = all && pass;
    }

    // ---- 6. Distinct objects at the same place are not a pair -----------
    {
        o.reset();
        Sample a; a.drawId = 50; a.objectId = 8; mat_identity(a.vp); mat_identity(a.l2w);
        Sample b = a; b.drawId = 51; b.objectId = 9;   // same matrices, other object
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kOtherObject) && o.c.pairs == 0 && o.c.refusedOtherObject == 1;
        _snprintf(d, sizeof(d), "%s pairs=%u", verdict_name(v), o.c.pairs);
        report(ctx, "distinct objects are not a pair", pass, d);
        all = all && pass;
    }

    // ---- 7. Two distinct draws that genuinely match ---------------------
    // NEITHER must remain reachable, or the observer could only ever confirm.
    {
        o.reset();
        Sample a; a.drawId = 60; a.objectId = 10; mat_identity(a.vp); mat_identity(a.l2w);
        Sample b = a; b.drawId = 61;
        o.offer(a);
        Verdict v = o.offer(b);
        const bool pass = (v == kNeither) && o.c.neither == 1;
        _snprintf(d, sizeof(d), "%s - the mono/one-view answer stays reachable", verdict_name(v));
        report(ctx, "identical distinct draws read NEITHER", pass, d);
        all = all && pass;
    }

    // ---- 8. The population is reported, so a sample of one cannot hide --
    {
        o.reset();
        Sample s; s.drawId = 70; s.objectId = 11; mat_identity(s.vp); mat_identity(s.l2w);
        for (int i = 0; i < 100; ++i) o.offer(s);        // one draw, a hundred evaluations
        const bool pass = o.c.distinctDraws == 1 && o.c.pairs == 0 && o.c.samples == 100;
        _snprintf(d, sizeof(d), "samples=%u distinctDraws=%u pairs=%u - 100 evaluations of ONE draw",
                  o.c.samples, o.c.distinctDraws, o.c.pairs);
        report(ctx, "population distinguishes draws from samples", pass, d);
        all = all && pass;
    }

    return all;
}

} // namespace dvr::eyeobs
