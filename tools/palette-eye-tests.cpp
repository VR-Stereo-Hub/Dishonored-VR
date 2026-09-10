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
static long g_mpEyeFlipped;
static long InterlockedCompareExchange(volatile long* p, long value, long expected) {
    const long old = *p; if (old == expected) *p = value; return old;
}
static long InterlockedIncrement(long* p) { return ++*p; }
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
    std::printf("palette-eye: %d failures\n", failed);
    return failed ? 1 : 0;
}
