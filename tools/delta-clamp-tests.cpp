// Host tests for AER's delta clamp arithmetic (VR-39): the production header
// src/game/dishonored/delta_clamp_policy.h, driven the way delta_clamp.cpp drives it.
// Never touches the game. Build and run: tools\delta-clamp-host.ps1
#include "game/dishonored/delta_clamp_policy.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace dc = dvr::delta_clamp;

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* name, const char* fmt, double a, double b = 0) {
    char detail[256];
    std::snprintf(detail, sizeof(detail), fmt, a, b);
    std::printf("%-44s %s  %s\n", name, ok ? "PASS" : "FAIL", detail);
    ok ? ++g_pass : ++g_fail;
}

// Simulates the stub's loop: tick i draws eye[i] with the factor set up after tick i-1.
struct Sim {
    bool clamp = true;
    float bank = 0, factor = 1, avgDt = 0;
    int pending = 0;
    double world = 0, real = 0;
    double worldAtLeft = 0;          // world time when the last left tick ended
    double maxPairGap = 0;           // world time between a left draw and its right draw
    double maxDrift = 0;             // |world - real| at any point
    float maxFactor = 0;
    int resets = 0;
    // One tick of `dt` real seconds drawing `eye`; `nextEye` is what the stub predicts next.
    void tick(int eye, int nextEye, float dt) {
        const float f = clamp ? factor : 1.0f;
        world += (double)f * dt; real += dt;
        if (eye < 0) worldAtLeft = world;
        // The right eye renders the world as its tick left it: its gap from the left eye's world.
        if (eye > 0) { const double gap = world - worldAtLeft; if (gap > maxPairGap) maxPairGap = gap; }
        if (std::fabs(world - real) > maxDrift) maxDrift = std::fabs(world - real);
        if (!clamp) return;
        bool reset = false;
        bank = dc::update_bank(bank, pending, eye, factor, dt, &reset);
        if (reset) ++resets;
        avgDt = dc::smooth_dt(avgDt, dt);
        factor = dc::next_factor(nextEye, bank, avgDt);
        if (factor > maxFactor) maxFactor = factor;
        pending = nextEye;
    }
    void run_pairs(int pairs, float (*dt)(int)) {
        for (int i = 0; i < pairs; ++i) {
            tick(-1, +1, dt(2 * i));
            tick(+1, -1, dt(2 * i + 1));
        }
    }
};

static float steady(int) { return 0.0069f; }
static float jitter(int i) {   // 5..15 ms, deterministic
    unsigned x = (unsigned)i * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15;
    return 0.005f + 0.010f * (float)(x % 1000u) / 999.0f;
}

int main() {
    {   // steady ticks: time preserved, the pair shows one instant
        Sim s; s.run_pairs(2000, steady);
        check(std::fabs(s.world - s.real) < 0.0069 * 2, "steady: world time preserved", "world-real %.6f s (bound one tick)", s.world - s.real);
        check(s.maxPairGap < 0.0069 * 0.02, "steady: right eye shows the left's instant", "max pair gap %.6f s (1%% of a tick allowed)", s.maxPairGap);
        check(s.maxDrift < 0.0069 * 1.5, "steady: never more than a tick behind", "max drift %.6f s", s.maxDrift);
        check(s.resets == 0, "steady: bank never reset", "resets %.0f", s.resets);
    }
    {   // negative control: the same instrument, clamp off, MUST see the right eye a tick later
        Sim s; s.clamp = false; s.run_pairs(2000, steady);
        check(s.maxPairGap > 0.0069 * 0.9, "control: no clamp -> the pair is a tick apart", "max pair gap %.6f s (the test can fail)", s.maxPairGap);
    }
    {   // jittered ticks 5..15 ms: time still preserved, drift bounded
        Sim s; s.run_pairs(5000, jitter);
        check(std::fabs(s.world - s.real) < 0.015 * 2, "jitter: world time preserved", "world-real %.6f s", s.world - s.real);
        check(s.maxDrift < 0.015 * 2, "jitter: drift bounded by two long ticks", "max drift %.6f s", s.maxDrift);
        check(s.maxPairGap < 0.015 * 0.02, "jitter: pair stays one instant", "max pair gap %.6f s", s.maxPairGap);
        check(s.maxFactor <= dc::kFactorMax, "jitter: factor within its cap", "max factor %.3f", s.maxFactor);
    }
    {   // a hitch on a right tick: the bank is dropped, never paid out as one lurch
        Sim s;
        s.tick(-1, +1, 0.007f);
        s.tick(+1, -1, 0.250f);   // a quarter-second stall
        check(s.resets == 1, "hitch: bank dropped", "resets %.0f", s.resets);
        check(s.factor == 1.0f, "hitch: the next left tick is not a lurch", "factor %.3f", s.factor);
    }
    {   // a broken pair: set up for right, the tick was not stereo -> bank untouched by the policy
        bool reset = true;
        const float b = dc::update_bank(0.004f, +1, 0, 0.01f, 0.007f, &reset);
        check(b == 0.004f && !reset, "broken pair: policy leaves the bank", "bank %.4f", b);
    }
    {   // garbage input
        bool reset = false;
        const float b = dc::update_bank(0.0f, +1, +1, 0.01f, NAN, &reset);
        check(b == 0.0f && !reset, "NaN dt: treated as zero", "bank %.4f", b);
        check(dc::next_factor(-1, 0.02f, NAN) <= dc::kFactorMax, "NaN dt: factor capped", "factor %.3f", dc::next_factor(-1, 0.02f, NAN));
        check(dc::next_factor(+1, 0.02f, 0.007f) == dc::kFreeze, "right tick: freeze, never zero", "factor %.4f", dc::kFreeze);
        check(dc::kFreeze > 0.0f, "freeze is non-zero", "kFreeze %.4f", dc::kFreeze);
    }
    std::printf("delta clamp: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
