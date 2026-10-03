// game/dishonored/delta_clamp_policy.h - the delta clamp's arithmetic (VR-39), engine-free so the
// host test (tools/delta-clamp-tests.cpp) runs the same code delta_clamp.cpp does.
//
// Units: real seconds. The world advances base * factor * realDt per tick. A right-eye tick is
// given kFreeze; the time it withheld is banked and the next left-eye tick is given enough extra
// to pay it back, predicted from the last tick's length; whatever the left tick over- or
// under-paid stays in the bank for the next pair. Over a run the world advances base * real time.
#pragma once
#include <cmath>

namespace dvr::delta_clamp {

constexpr float kFreeze = 0.01f;       // never 0: a script dividing by TimeDilation must not see it
constexpr float kBankMax = 0.1f;       // a hitch or a load: dropped, never paid out as one lurch
constexpr float kBankMin = -0.05f;
constexpr float kFactorMax = 4.0f;

// The tick that just ran drew `eyeDrawn` with `factor`, having been set up for `pendingEye`.
// Returns the new bank; `reset` is set when the bank left its range and was dropped.
inline float update_bank(float bank, int pendingEye, int eyeDrawn, float factor, float realDt, bool* reset) {
    if (reset) *reset = false;
    if (!std::isfinite(realDt) || realDt < 0.0f) realDt = 0.0f;
    if (pendingEye > 0 && eyeDrawn > 0) bank += realDt * (1.0f - kFreeze);
    else if (pendingEye < 0 && eyeDrawn < 0) bank -= (factor - 1.0f) * realDt;
    if (!std::isfinite(bank) || bank > kBankMax || bank < kBankMin) {
        if (reset) *reset = true;
        bank = 0.0f;
    }
    return bank;
}

// The tick-length predictor: a smoothed tick length, not the last tick's. Predicting the left
// tick from the single tick before it overshoots whenever a short tick precedes a long one (the
// host test measured the factor pinned at its cap and the world 33 ms ahead under 5..15 ms jitter).
inline float smooth_dt(float prev, float dt) {
    if (!std::isfinite(dt) || dt <= 0.0f) return prev;
    if (dt > 0.05f) dt = 0.05f;
    return prev > 0.0f ? prev * 0.9f + dt * 0.1f : dt;
}

// The factor for the NEXT tick, which draws `nextEye`. `predictedDt` is smooth_dt's estimate.
inline float next_factor(int nextEye, float bank, float predictedDt) {
    if (nextEye > 0) return kFreeze;
    float predicted = std::isfinite(predictedDt) ? predictedDt : 0.0f;
    if (predicted < 0.002f) predicted = 0.002f;
    if (predicted > 0.05f) predicted = 0.05f;
    float f = 1.0f + (bank > 0.0f ? bank : 0.0f) / predicted;
    if (f > kFactorMax) f = kFactorMax;
    return f;
}

} // namespace dvr::delta_clamp
