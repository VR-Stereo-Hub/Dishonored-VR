// game/dishonored/hands/eye_observer.h - the eye-pair observer's PURE core.
//
// No engine, no D3D, no globals. Included by the draw path AND by
// tools/frame_test, so a pass in the suite is a statement about the SHIPPED
// logic rather than about a re-derivation of it - the same arrangement
// hand_frame.h uses.
//
// ---- WHY THIS EXISTS AS A SEPARATE, TESTABLE THING ----
//
// Four instruments in one session produced confident numbers about nothing, and
// every one failed the same way: not the arithmetic, not the threshold, but
// WHAT WAS IN THE SAMPLE.
//
//   * an audit whose two inputs both came from one pose consume, so it could
//     only ever print zero;
//   * a stamp that measured the producer when the question was the consumer;
//   * a hold whose fault never fired while the symptom continued;
//   * a probe that compared one draw context with ITSELF, because the caller
//     shares a single context across two hand ranges - 105,816 of 105,816
//     "pairs" identical with 0.00 variance, read as a finding about stereo.
//
// So this core is built to make that last one IMPOSSIBLE rather than unlikely,
// and the suite next door proves it on the shipped code before it goes near a
// headset.
//
// Three rules it enforces:
//
//   1. A pair needs two DIFFERENT draws. Identity is an explicit id supplied by
//      the caller, never a position - a position is not an identity, distinct
//      objects share one and the same object moves.
//   2. Matrices are compared ELEMENTWISE with a stated tolerance, and the
//      largest difference and its index are reported. A weighted scalar
//      signature collides: +2 on element 0 and -1 on element 1 cancel exactly.
//   3. The POPULATION is reported beside every verdict - distinct draws,
//      distinct objects, pairs formed, and pairs refused with the reason. A
//      sample of one must not be able to look like a sample of a hundred
//      thousand.
#pragma once
#include <stdint.h>
#include <math.h>

namespace dvr::eyeobs {

// One observation of one original draw. The caller supplies identity; this
// header never invents it.
struct Sample {
    uint64_t drawId;      // strictly increasing per ORIGINAL draw. Two calls
                          // sharing one draw context share this, and are refused.
    uint64_t objectId;    // stable for one object across draws; not a position
    float    vp[16];
    float    l2w[16];
};

enum Verdict {
    kNoPair = 0,     // nothing to compare yet
    kSameDraw,       // REFUSED: both samples are the same original draw
    kOtherObject,    // REFUSED: the held sample is a different object
    kVpOnly,         // the eye lives in the view matrix
    kL2wOnly,        // the eye is baked into the object transform
    kBoth,           // both moved: a camera-relative input space
    kNeither,        // genuinely identical matrices across two distinct draws
};

struct Diff {
    bool  changed;
    float maxAbs;     // largest elementwise difference
    int   maxIndex;   // where it was
};

inline Diff diff16(const float* a, const float* b, float tol)
{
    Diff d; d.changed = false; d.maxAbs = 0.0f; d.maxIndex = -1;
    for (int i = 0; i < 16; ++i) {
        const float e = a[i] - b[i];
        const float m = e < 0.0f ? -e : e;
        if (m > d.maxAbs) { d.maxAbs = m; d.maxIndex = i; }
    }
    d.changed = d.maxAbs > tol;
    return d;
}

struct Counts {
    uint32_t samples;
    uint32_t distinctDraws;
    uint32_t pairs;
    uint32_t refusedSameDraw;
    uint32_t refusedOtherObject;
    uint32_t vpOnly, l2wOnly, both, neither;
};

// Holds at most one sample and pairs the next one against it. Deliberately
// tiny: the failure being guarded against is conceptual, not capacity.
struct Observer {
    Sample   held;
    bool     haveHeld;
    uint64_t lastDrawId;
    bool     haveLastDrawId;
    float    tol;
    Counts   c;
    Diff     lastVp, lastL2w;

    void reset(float tolerance = 1e-4f)
    {
        haveHeld = false;
        haveLastDrawId = false;
        lastDrawId = 0;
        tol = tolerance;
        c.samples = c.distinctDraws = c.pairs = 0;
        c.refusedSameDraw = c.refusedOtherObject = 0;
        c.vpOnly = c.l2wOnly = c.both = c.neither = 0;
        lastVp.changed = lastL2w.changed = false;
        lastVp.maxAbs = lastL2w.maxAbs = 0.0f;
        lastVp.maxIndex = lastL2w.maxIndex = -1;
    }

    Verdict offer(const Sample& s)
    {
        ++c.samples;
        if (!haveLastDrawId || s.drawId != lastDrawId) {
            ++c.distinctDraws;
            lastDrawId = s.drawId;
            haveLastDrawId = true;
        }

        if (!haveHeld) { held = s; haveHeld = true; return kNoPair; }

        // RULE 1. The defect that produced 105,816 meaningless comparisons.
        // Two evaluations of one draw are one observation, never a pair.
        if (s.drawId == held.drawId) {
            ++c.refusedSameDraw;
            return kSameDraw;
        }
        // RULE 1b. Identity, not proximity.
        if (s.objectId != held.objectId) {
            ++c.refusedOtherObject;
            held = s;               // carry the newer one forward
            return kOtherObject;
        }

        // RULE 2. Elementwise, with the largest difference reported.
        lastVp  = diff16(s.vp,  held.vp,  tol);
        lastL2w = diff16(s.l2w, held.l2w, tol);
        ++c.pairs;
        Verdict v;
        if (lastVp.changed && !lastL2w.changed)      { ++c.vpOnly;  v = kVpOnly; }
        else if (!lastVp.changed && lastL2w.changed) { ++c.l2wOnly; v = kL2wOnly; }
        else if (lastVp.changed && lastL2w.changed)  { ++c.both;    v = kBoth; }
        else                                          { ++c.neither; v = kNeither; }
        haveHeld = false;           // one comparison per pair, not a chain
        return v;
    }
};

inline const char* verdict_name(Verdict v)
{
    switch (v) {
    case kNoPair:       return "no pair yet";
    case kSameDraw:     return "REFUSED - same original draw";
    case kOtherObject:  return "REFUSED - different object";
    case kVpOnly:       return "VP ONLY - the eye is in the view matrix";
    case kL2wOnly:      return "L2W ONLY - the eye is in the object transform";
    case kBoth:         return "BOTH - camera-relative input space";
    case kNeither:      return "NEITHER - two distinct draws, identical matrices";
    }
    return "?";
}

} // namespace dvr::eyeobs
