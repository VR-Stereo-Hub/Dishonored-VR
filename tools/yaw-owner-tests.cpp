// Host integration tests for the real ownership, publication and body-write code.
// Only engine memory/services and logging are mocked by yawtest-slice.py.
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
    YawApplyBody(1.0f);
    Check("new gameplay objects bind without BuildLiveSet", g_yawValid && g_yawGen == 1 && PawnYaw(pawnA) == 1000 && g_yawApplied == 1);
    unsigned scans = fullTableReads;
    YawApplyBody(1.0f);
    Check("unchanged pair uses live slot checks without rescanning", fullTableReads == scans);

    // A removed object still has a perfectly readable header and old contents.
    objectTable[1300] = NULL;
    PawnYaw(pawnA) = 777;
    YawApplyBody(1.0f);
    Check("removed pawn releases snapshot before any write", !g_yawValid && PawnYaw(pawnA) == 777);
    objectTable[1300] = pawnA;
    testMs += 501;
    Possess(controllerA, pawnB);
    YawPublish(2000, 0);
    Check("live objects with wrong possession are rejected", !g_yawValid);
    Possess(controllerA, pawnA);
    YawPublish(2000, 0);
    Check("valid possession recovers", g_yawValid);

    // Publish for A, then change scene before Apply: the new pawn must not get A's target.
    objectTable[1400] = controllerB; objectTable[1500] = pawnB;
    Possess(controllerB, pawnB);
    g_peCtrl = controllerB; g_pePawn = pawnB;
    PawnYaw(pawnB) = 888;
    testMs += 501;
    YawApplyBody(1.0f);
    Check("owner change between publish and apply rejects old target", !g_yawValid && g_yawGen == 2 && PawnYaw(pawnB) == 888);
    YawPublish(3000, 50);
    YawApplyBody(1.0f);
    Check("new scene publishes a fresh reference", PawnYaw(pawnB) == 3000 && g_yawHeadContrib == 50);

    uint32_t seq = g_yawSeq, applied = g_yawApplied;
    secondPass = true;
    YawApplyBody(1.0f);
    Check("second pass cannot lead a body write", g_yawSeq == seq && g_yawApplied == applied);
    secondPass = false;
    g_yawValid = false;
    PawnYaw(pawnB) = 999;
    YawApplyBody(1.0f);
    Check("missing snapshot leaves pawn untouched", PawnYaw(pawnB) == 999);

    g_yawTriedOff = false; offsetsReady = false; testMs += 3001;
    YawPublish(4000, 0);
    Check("unavailable reflection stays retryable", !g_yawTriedOff && !g_yawValid);
    offsetsReady = true; testMs += 3001;
    YawPublish(4000, 0);
    Check("reflection recovers after engine initialization", g_yawTriedOff && g_yawValid);
    return failures ? 1 : 0;
}
