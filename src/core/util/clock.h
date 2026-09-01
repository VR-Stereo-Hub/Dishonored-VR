// core/util/clock.h - one monotonic millisecond clock (QueryPerformanceCounter).
#pragma once

namespace dvr::clock {
void      init();          // reads the QPC frequency; idempotent, loader-lock safe
double    now_ms();        // 0.0 before init()
long long qpc_freq();
} // namespace dvr::clock

// Original name, used throughout the game-side code.
inline double MaimNowMs() { return ::dvr::clock::now_ms(); }
