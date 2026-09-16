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
static bool g_mpEyeMenuHalfStep=false;static int testMenuContext=-1;
static int UiSurfaceContext(){return testMenuContext;}
static int  g_mpEyePredictRun = 0;
static long g_mpEyePredicted = 0;
static double g_mpEyeSameAdSum; static long g_mpEyeSameAdN;
static long g_mpEyeMethodAgree[3], g_mpEyeMethodDisagree[3], g_mpEyeMethodNone[3];
// The VR-95 cross-check inside the production body reads the stereo method's
// record. It is READ-ONLY telemetry and cannot influence the decision under
// test, so the host stubs it as "the method published nothing".
namespace dvr { namespace desktop_eye {
struct Record { unsigned present; int draw, tag, shown; char action, source; };
static unsigned completedPresent=0;static int completedEye=0;
static bool record_for(unsigned p, Record& out) { out = Record{};out.draw=completedEye;return p==completedPresent; }
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
// VR-95: the eye the classifier reported per present, so a repeat (an RR or LL
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
#define DVR_LOG_EVERY_MS(...) ((void)0)
namespace dvr::stereo { struct Output{int eyeSign=0;};static Output last_output(){return {};}}
struct MfRec {uint32_t present=0,poseGen=0;int menuContext=-1;int8_t eye=0;char why='T';float d=0,ipdUU=0;uint8_t refused[2]{},waMiss=0;};
static constexpr int kMfRing=1024;
static MfRec g_mfRing[kMfRing];
static uint32_t g_mfTagCount[kMfRing];static int8_t g_mfTagSign[kMfRing];
static int g_mfHead=-1,g_mfTagHead=-1;
#include "palette_tag_body.inc"

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
    // Replay observed385 menu left-eye jumps below the old 0.45-IPD band.
    const float measured[]={2.797f,2.831f,2.666f,2.678f,2.541f,2.482f,2.496f,2.291f,2.388f,2.820f};
    for(float jump:measured) {
        reset();testMenuContext=6;g_mpEyeMenuHalfStep=false;
        draw(1,0,1);draw(2,-6.808f,1);draw(3,-6.808f+jump,1);
        check("measured_menu_negative_control_holds_wrong_right",g_mpEyeState==1);
        reset();g_mpEyeMenuHalfStep=true;
        draw(1,0,1);draw(2,-6.808f,1);draw(3,-6.808f+jump,1);
        check("signed_menu_half_step_recognizes_left",g_mpEyeState==-1);
    }
    reset();testMenuContext=3;draw(1,0,1);draw(2,-6.808f,1);
    for(unsigned i=3;i<30;++i) draw(i,-6.808f+.001f*(i-2),0);
    check("stationary_menu_does_not_predict_alternation",g_mpEyeState==1 && g_mpEyePredicted==0);
    reset();testMenuContext=-1;draw(1,0,1);draw(2,-6.808f,1);draw(3,-4.2f,1);
    check("gameplay_keeps_original_band",g_mpEyeState==1);
    g_mpEyeMenuHalfStep=false;testMenuContext=-1;

    g_mfHead=0;g_mfRing[0].present=10;g_mfRing[0].eye=1;
    dvr::desktop_eye::completedPresent=11;dvr::desktop_eye::completedEye=1;
    testPresent=11;MfNoteTag();
    check("does_not_query_future_present",g_mpEyeMethodAgree[0]==0&&g_mpEyeMethodNone[0]==0);
    testPresent=12;MfNoteTag();
    check("joins_hand_N_to_completed_present_N_plus_1",g_mpEyeMethodAgree[0]==1);
    g_mfRing[0].eye=-1;MfNoteTag();
    check("completed_present_mismatch_is_observable",g_mpEyeMethodDisagree[0]==1);
    dvr::desktop_eye::completedEye=0;MfNoteTag();
    check("untagged_present_is_unknown_not_agreement",g_mpEyeMethodNone[0]==1);
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
    // ---- VR-95: the measured left-eye robbery --------------------------------
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
    check("vr95_no_drift_alternates_without_the_fix", noDoublet(20) && g_mpEyeSame == 0);
    g_mpEyePredict = true; runStream(0.0f, 20);
    check("vr95_no_drift_unchanged_by_the_fix", noDoublet(20) && g_mpEyePredicted == 0);

    // A roll drift of -2.6 uu: the +5.09 crossing falls to +2.49, under the band,
    // while -5.60 grows to -8.20 and stays readable. THE OLD BEHAVIOUR MUST FAIL
    // HERE, and must fail by repeating the RIGHT eye, or this proves nothing.
    g_mpEyePredict = false; runStream(-2.6f, 20);
    check("vr95_roll_drift_robs_the_left_eye_without_the_fix",
          !noDoublet(20) && g_mpEyeSame > 0 && heldEyeIs(+1));
    g_mpEyePredict = true; runStream(-2.6f, 20);
    check("vr95_roll_drift_is_corrected_by_the_fix",
          noDoublet(20) && g_mpEyePredicted > 0);

    // The mirror-image drift too: the fault is one-sided because the jumps are,
    // not because the fix is allowed to be.
    g_mpEyePredict = true; runStream(+2.9f, 20);
    check("vr95_opposite_drift_also_corrected", noDoublet(20));

    // A genuinely flat stream: every jump unreadable. The cap must stop after two
    // predictions so a real mono run is not shredded.
    {
        g_mpEyePredict = true; reset();
        float pr = 0.0f;
        draw(1, pr, 1); pr -= 5.60f; draw(2, pr, 1);
        const long before = g_mpEyePredicted;
        for (int i = 0; i < 8; ++i) { pr += 0.05f; draw((uint32_t)(3 + i), pr, 1); }
        check("vr95_flat_stream_stops_predicting_after_two",
              g_mpEyePredicted - before == 2 && g_mpEyeSame >= 8);
    }
    std::printf("palette-eye: %d failures\n", failed);
    return failed ? 1 : 0;
}
