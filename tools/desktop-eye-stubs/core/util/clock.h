#pragma once
namespace dvr::clock {
inline double fakeClockMs = 0;
inline double now_ms() { return fakeClockMs += 0.01; }
}
