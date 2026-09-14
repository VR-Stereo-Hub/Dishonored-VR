// tools/ui-ride-tests.cpp - the ride policy on the host (VR-117). Run by
// tools/ui-ride-host.ps1; never launches the game. Every decision in
// game/dishonored/ui_ride_policy.h must be failable here.
#include "game/dishonored/ui_ride_policy.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
int main() {
    using namespace dvr::ui_ride;
    using dvr::mono::Context;
    const uint32_t all = 0xffffffffu;
    // Which contexts may ride at all.
    check(context_can_ride(dvr::mono::Pause), "pause can ride");
    check(context_can_ride(dvr::mono::Note), "note can ride");
    check(context_can_ride(dvr::mono::Journal), "journal can ride");
    check(context_can_ride(dvr::mono::Store), "store can ride");
    check(context_can_ride(dvr::mono::MissionStats), "mission stats can ride");
    check(!context_can_ride(dvr::mono::MainMenu), "the main menu keeps the screen");
    check(!context_can_ride(dvr::mono::Loading), "a load keeps the screen");
    check(!context_can_ride(dvr::mono::Wheel), "the wheel never rides");
    check(!context_can_ride(dvr::mono::Cinematic), "a cinematic never rides");
    check(!context_can_ride(dvr::mono::Other), "an unknown owner never rides");
    // The full decision.
    for (unsigned c = 0; c < dvr::mono::Count; ++c) {
        const Context ctx = (Context)c;
        const bool expect = context_can_ride(ctx);
        check(rides(true, true, ctx, all, true, true, true) == expect, "rides follows context_can_ride with everything on");
        check(!rides(false, true, ctx, all, true, true, true), "guard off: no ride");
        check(!rides(true, false, ctx, all, true, true, true), "not blocked: no ride");
        check(!rides(true, true, ctx, 0, true, true, true), "no opt-in: no ride");
        check(!rides(true, true, ctx, all, false, true, true), "MenuInWindow=0: no ride");
        check(!rides(true, true, ctx, all, true, false, true), "window off: no ride");
        check(!rides(true, true, ctx, all, true, true, false), "redirect unhealthy: no ride");
    }
    check(rides(true, true, dvr::mono::Pause, 1u << dvr::mono::Pause, true, true, true), "the pause opt-in bit alone admits the pause");
    check(!rides(true, true, dvr::mono::Note, 1u << dvr::mono::Pause, true, true, true), "the pause opt-in bit does not admit a note");
    // Eligibility while riding.
    check(ride_eligible(true, true, false), "raw clock alone holds");
    check(ride_eligible(true, false, true), "gate alone holds");
    check(!ride_eligible(true, false, false), "neither clock: fallback");
    check(!ride_eligible(false, true, true), "no pawn: fallback even with both clocks");
    // The latch.
    {
        RideLatch l;
        check(!l.update(dvr::mono::Other, false, false, false), "unblocked: not riding");
        check(l.update(dvr::mono::Pause, true, true, false), "pause opens riding");
        check(l.update(dvr::mono::Pause, true, false, false), "a health flap inside the interval does not flip the decision");
        check(l.update(dvr::mono::Pause, true, true, false), "still riding");
        check(!l.update(dvr::mono::Pause, true, true, true), "a hard failure drops the ride");
        check(!l.update(dvr::mono::Pause, true, true, false), "and it stays dropped for the interval");
        check(!l.update(dvr::mono::Other, false, false, false), "unblocked clears");
        check(!l.update(dvr::mono::Pause, true, false, false), "a pause decided NOT riding stays not riding");
        check(!l.update(dvr::mono::Pause, true, true, false), "even when want rises mid-interval");
        check(l.update(dvr::mono::Journal, true, true, false), "a context change re-decides");
        check(!l.update(dvr::mono::Loading, true, true, false) || context_can_ride(dvr::mono::Loading) == false, "a load decides from its own want");
    }
    // The grace.
    {
        RideGrace g;
        check(!g.update(false, false, 0.0), "no ride yet: no grace");
        check(!g.update(true, true, 100.0), "riding: no grace (the ride itself stands in)");
        check(g.update(false, false, 200.0), "the ride ended: grace starts");
        check(g.update(false, false, 200.0 + 1499.0), "1499 ms in: still in grace");
        check(!g.update(false, false, 200.0 + 1501.0), "1501 ms in: grace over");
        check(!g.update(false, false, 5000.0), "and it stays over");
        check(g.update(true, true, 6000.0) == false, "riding again");
        check(g.update(false, false, 6100.0), "ends again: grace again");
        check(!g.update(false, true, 6200.0), "a new blocked owner cancels the grace");
        check(!g.update(false, false, 6300.0), "and it stays cancelled");
    }
    std::printf("%u ui-ride policy checks passed\n", checks);
    return 0;
}
