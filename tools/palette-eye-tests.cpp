// Tests the production MpEyeForPresent body extracted by palette-eye-host.ps1.
// The game-thread flag is deliberately independent of the queued render draw.
#include <cstdint>
#include <cmath>
#include <cstdio>

struct MpDrawCtx { float projRight; };
static uint32_t testPresent;
namespace dvr::frame { uint32_t count() { return testPresent; } }
static int g_mpEyeState;
static uint32_t g_mpEyePresent;
static bool g_mpEyeHavePrev;
static float g_mpEyePrevFirst;
static long g_mpEyeToggles, g_mpEyeSame, g_mpEyeAmbiguous;
static float g_ipdM = 0.0631f, g_skcWorldScale = 100.0f, g_mpDriveGain = 1.0f;

// These stubs let the pre-fix decision compile too. They do not modify a draw.
static volatile long g_sdDoublingNow;
static bool g_mpEyeAlternate = false;
static bool g_mpEyePredict = false;
static int  g_mpEyePredictRun = 0;
static long g_mpEyePredicted = 0;
static double g_mpEyeSameAdSum; static long g_mpEyeSameAdN;
static long g_mpEyeMethodAgree[3], g_mpEyeMethodDisagree[3], g_mpEyeMethodNone[3];
// The VR-94 cross-check inside the production body reads the stereo method's
// record. It is READ-ONLY telemetry and cannot influence the decision under
// test, so the host stubs it as "the method published nothing".
namespace dvr { namespace desktop_eye {
struct Record { unsigned present; int draw, tag, shown; char action, source; };
static bool record_for(unsigned, Record& out) { out = Record{}; return false; }
} }
static long g_mpEyeFlipped;
static long InterlockedCompareExchange(volatile long* p, long value, long expected) {
    const long old = *p; if (old == expected) *p = value; return old;
}
static long InterlockedIncrement(long* p) { return ++*p; }
// VR-76 added this marker-history call to the production body and nothing stubbed
// it here, so this suite stopped COMPILING and therefore stopped running. A test
// that cannot build is not a passing test; it is an absent one.
static void MfOpen(uint32_t, const MpDrawCtx*, char, float, float) {}
// VR-94: the eye the classifier reported per present, so a repeat (an RR or LL
// doublet) is asserted directly rather than inferred.
static int eyeRow[64];
static bool noDoublet(int n) {
    for (int i = 2; i < n; ++i) if (eyeRow[i] != 0 && eyeRow[i] == eyeRow[i - 1]) return false;
    return true;
}
static bool heldEyeIs(int eye) {   // every doublet repeats THIS eye
    bool any = false;
    for (int i = 2; i < 64; ++i)
        if (eyeRow[i] != 0 && eyeRow[i] == eyeRow[i - 1]) { any = true; if (eyeRow[i] != eye) return false; }
    return any;
}
static void MpFlickTick() {}
static void MpFlickNote(const char*) {}
#include "palette_eye_body.inc"

static int failed;
static void check(const char* name, bool ok) {
    std::printf("palette-eye/%s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failed;
}
static void reset() {
    g_mpEyeState = 0; g_mpEyePresent = UINT32_MAX; g_mpEyeHavePrev = false;
    g_mpEyePrevFirst = 0; g_mpEyeToggles = g_mpEyeSame = g_mpEyeAmbiguous = 0;
    g_mpEyeFlipped = 0; g_sdDoublingNow = 1;
    g_mpEyePredictRun = 0; g_mpEyePredicted = 0;
    for (int i = 0; i < 64; ++i) eyeRow[i] = 0;
}
static void draw(uint32_t present, float right, long scriptDoubling) {
    testPresent = present; g_sdDoublingNow = scriptDoubling;
    MpDrawCtx c{right}; MpEyeForPresent(&c);
}
int main() {
    reset();
    draw(1, 3.155f, 1);
    check("first_observation_is_unknown", g_mpEyeState == 0 && g_mpEyeHavePrev);
    draw(2, -3.155f, 1);
    check("right_eye_step", g_mpEyeState == 1 && g_mpEyeToggles == 1);
    draw(2, 999.0f, 0);
    check("second_hand_same_present_keeps_decision", g_mpEyeState == 1 &&
          g_mpEyePrevFirst == -3.155f && g_mpEyeToggles == 1);
    draw(3, 3.155f, 0);
    check("queued_left_survives_new_script_single_tick", g_mpEyeState == -1 &&
          g_mpEyeHavePrev && g_mpEyeToggles == 2);
    draw(4, -3.155f, 0);
    check("queued_right_survives_new_script_single_tick", g_mpEyeState == 1 &&
          g_mpEyeToggles == 3);
    draw(5, -3.155f, 0);
    check("unreadable_step_holds_baseline", g_mpEyeState == 1 && g_mpEyeSame == 1);
    draw(6, 100.0f, 1);
    check("large_motion_still_refuses", g_mpEyeState == 0 && g_mpEyeAmbiguous == 1);
    draw(7, 93.69f, 1);
    check("valid_step_recovers_after_large_motion", g_mpEyeState == 1);
    reset();
    draw(UINT32_MAX - 1, 3.155f, 1);
    draw(UINT32_MAX, -3.155f, 0);
    draw(0, 3.155f, 0);
    check("present_wrap_keeps_sequence", g_mpEyeState == -1 && g_mpEyeToggles == 2);
    // ---- VR-94: the measured left-eye robbery --------------------------------
    //
    // Replays the shape recorded on the headset. projRight walks with the head and
    // the eye step rides on top: entering a right present the jump is about -5.60,
    // entering a left present about +5.09, against a 2.84 uu band. A head roll adds
    // a drift to every jump, which shrinks the smaller crossing first.
    auto runStream = [](float drift, int presents) {
        reset();
        float pr = 0.0f;
        for (int i = 0; i < presents; ++i) {
            pr += (i % 2 == 0 ? -5.60f : +5.09f) + drift;
            draw((uint32_t)(i + 1), pr, 1);
            eyeRow[i] = g_mpEyeState;
        }
    };
    g_mpEyePredict = false; runStream(0.0f, 20);
    check("vr94_no_drift_alternates_without_the_fix", noDoublet(20) && g_mpEyeSame == 0);
    g_mpEyePredict = true; runStream(0.0f, 20);
    check("vr94_no_drift_unchanged_by_the_fix", noDoublet(20) && g_mpEyePredicted == 0);

    // A roll drift of -2.6 uu: the +5.09 crossing falls to +2.49, under the band,
    // while -5.60 grows to -8.20 and stays readable. THE OLD BEHAVIOUR MUST FAIL
    // HERE, and must fail by repeating the RIGHT eye, or this proves nothing.
    g_mpEyePredict = false; runStream(-2.6f, 20);
    check("vr94_roll_drift_robs_the_left_eye_without_the_fix",
          !noDoublet(20) && g_mpEyeSame > 0 && heldEyeIs(+1));
    g_mpEyePredict = true; runStream(-2.6f, 20);
    check("vr94_roll_drift_is_corrected_by_the_fix",
          noDoublet(20) && g_mpEyePredicted > 0);

    // The mirror-image drift too: the fault is one-sided because the jumps are,
    // not because the fix is allowed to be.
    g_mpEyePredict = true; runStream(+2.9f, 20);
    check("vr94_opposite_drift_also_corrected", noDoublet(20));

    // A genuinely flat stream: every jump unreadable. The cap must stop after two
    // predictions so a real mono run is not shredded.
    {
        g_mpEyePredict = true; reset();
        float pr = 0.0f;
        draw(1, pr, 1); pr -= 5.60f; draw(2, pr, 1);
        const long before = g_mpEyePredicted;
        for (int i = 0; i < 8; ++i) { pr += 0.05f; draw((uint32_t)(3 + i), pr, 1); }
        check("vr94_flat_stream_stops_predicting_after_two",
              g_mpEyePredicted - before == 2 && g_mpEyeSame >= 8);
    }
    std::printf("palette-eye: %d failures\n", failed);
    return failed ? 1 : 0;
}
