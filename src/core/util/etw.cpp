// core/util/etw.cpp - see etw.h.
#include "core/util/etw.h"

#include <windows.h>
#include <TraceLoggingProvider.h>
#include <winmeta.h>   // WINEVENT_OPCODE_START / _STOP

// {6b3c1f4e-2d6a-4f7c-9a51-0d2e8c7b4a19}
TRACELOGGING_DEFINE_PROVIDER(g_dvrEtw, "DishonoredVR",
    (0x6b3c1f4e, 0x2d6a, 0x4f7c, 0x9a, 0x51, 0x0d, 0x2e, 0x8c, 0x7b, 0x4a, 0x19));

namespace dvr::etw {
namespace {
volatile LONG g_registered = 0;
volatile LONG g_killed = 0;
}

bool enabled() {
    return g_registered && !g_killed && TraceLoggingProviderEnabled(g_dvrEtw, 0, 0);
}

void begin(Phase p, int64_t a, int64_t b) {
    if (!enabled()) return;
    TraceLoggingWrite(g_dvrEtw, "Phase", TraceLoggingOpcode(WINEVENT_OPCODE_START),
                      TraceLoggingInt32((int32_t)p, "id"), TraceLoggingInt64(a, "a"), TraceLoggingInt64(b, "b"));
}

void end(Phase p, int64_t a, int64_t b) {
    if (!enabled()) return;
    TraceLoggingWrite(g_dvrEtw, "Phase", TraceLoggingOpcode(WINEVENT_OPCODE_STOP),
                      TraceLoggingInt32((int32_t)p, "id"), TraceLoggingInt64(a, "a"), TraceLoggingInt64(b, "b"));
}

void mark(const char* text, int64_t a) {
    if (!enabled()) return;
    TraceLoggingWrite(g_dvrEtw, "Mark", TraceLoggingString(text ? text : "", "text"), TraceLoggingInt64(a, "a"));
}

void frame_start(uint32_t present) {
    if (!enabled()) return;
    TraceLoggingWrite(g_dvrEtw, "FrameStart", TraceLoggingUInt32(present, "present"));
}

void set_killed(bool killed) { InterlockedExchange(&g_killed, killed ? 1 : 0); }

void init() {
    if (g_registered) return;
    if (TraceLoggingRegister(g_dvrEtw) == ERROR_SUCCESS) InterlockedExchange(&g_registered, 1);
}

void shutdown() {
    if (!InterlockedExchange(&g_registered, 0)) return;
    TraceLoggingUnregister(g_dvrEtw);
}

} // namespace dvr::etw
