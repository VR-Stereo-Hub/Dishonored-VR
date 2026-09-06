// Host integration tests for the real ownership and publication code.
// Only engine memory/services and logging are mocked by yawtest-slice.py.
//
// These once drove YawApplyBody, the raw pawn-yaw writer. That writer is gone:
// it was measured futile (4 of 186 writes survived to the next dispatch,
// because the engine re-derives the pawn heading from the controller every
// tick) and is superseded by the FaceRotation intercept, which changes the
// heading the engine is ASKED for. So the assertions now check the SNAPSHOT -
// validity, generation, body target and head contribution - which is exactly
// what the intercept consumes, and is the half of the old tests that was ever
// about ownership rather than about the write.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static double testMs = 10000;
static bool secondPass = false, offsetsReady = true;
static unsigned fullTableReads = 0;
static void* objectTable[2048] = {};
alignas(16) static uint8_t controllerA[1024], controllerB[1024], pawnA[1024], pawnB[1024];
static uint8_t* g_peCtrl = NULL;
static uint8_t* g_pePawn = NULL;
static uint8_t* g_pcObj = NULL;
static double MaimNowMs() { return testMs; }
static bool RangeReadable(const void* p, size_t n) {
    if (p == objectTable && n == sizeof(objectTable)) ++fullTableReads;
    return p != NULL;
}
static uint32_t FindPropOffset(const char*, const char* name) {
    return !offsetsReady ? 0 : !strcmp(name, "Pawn") ? 0x248 : 0xd0;
}
static const char* ObjClassName(uint8_t*) { return "DishonoredPlayerController"; }
static void Log(const char*, ...) {}
#define DVR_LOG_EVERY_MS(...) ((void)0)
namespace dvr { namespace camera {
static bool second_pass_for_current_thread() { return secondPass; }
} }

#include "yaw_owner_impl.inc"

static int failures = 0;
static void Check(const char* name, bool ok) {
    printf("yaw-owner: %s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
static int32_t& PawnYaw(uint8_t* p) { return ((int32_t*)(p + 0xd0))[1]; }
static void Possess(uint8_t* c, uint8_t* p) { *(uint8_t**)(c + 0x248) = p; }

int main() {
    // No sorted discovery snapshot is built anywhere in this harness.
    g_peCtrl = controllerA; g_pePawn = pawnA;
    objectTable[1200] = controllerA; objectTable[1300] = pawnA;
    Possess(controllerA, pawnA);
    YawPublish(1000, 100);
    Check("new gameplay objects bind without BuildLiveSet",
          g_yawValid && g_yawGen == 1 && g_yawBodyTarget == 1000 && g_yawHeadContrib == 100);
    unsigned scans = fullTableReads;
    YawPublish(1100, 0);
    Check("unchanged pair uses live slot checks without rescanning", fullTableReads == scans);

    // A removed object still has a perfectly readable header and old contents.
    objectTable[1300] = NULL;
    YawPublish(1200, 0);
    Check("removed pawn releases the snapshot", !g_yawValid);
    objectTable[1300] = pawnA;
    testMs += 501;
    Possess(controllerA, pawnB);
    YawPublish(2000, 0);
    Check("live objects with wrong possession are rejected", !g_yawValid);
    Possess(controllerA, pawnA);
    YawPublish(2000, 0);
    Check("valid possession recovers", g_yawValid);

    // A scene change must not carry the previous owner's contribution.
    objectTable[1400] = controllerB; objectTable[1500] = pawnB;
    Possess(controllerB, pawnB);
    g_peCtrl = controllerB; g_pePawn = pawnB;
    testMs += 501;
    YawPublish(3000, 50);
    Check("owner change bumps the generation and resets the contribution",
          g_yawGen == 2 && g_yawHeadContrib == 50 && g_yawBodyTarget == 3000);

    // Head-only: the view moves, the body target does not.
    const int32_t bodyBefore = g_yawBodyTarget;
    YawPublish(g_yawViewOut, 400);
    Check("head-only publication leaves the body target alone",
          g_yawBodyTarget == bodyBefore && g_yawViewOut == 3050 + 400);

    // Stick-only: the engine turned us; body and view move together.
    const int32_t view0 = g_yawViewOut, body0 = g_yawBodyTarget;
    YawPublish(view0 + 700, 0);
    Check("stick-only publication moves body and view together",
          g_yawViewOut - view0 == 700 && g_yawBodyTarget - body0 == 700);

    g_yawTriedOff = false; offsetsReady = false; testMs += 3001;
    YawPublish(4000, 0);
    Check("unavailable reflection stays retryable", !g_yawTriedOff && !g_yawValid);
    offsetsReady = true; testMs += 3001;
    YawPublish(4000, 0);
    Check("reflection recovers after engine initialization", g_yawTriedOff && g_yawValid);
    return failures ? 1 : 0;
}
