// Exercise the production half-slot service and Present entry/exit under real mutex contention.
// No OpenXR runtime or game is started. The synthetic cycle stub records ownership/order.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

namespace dvr::clock { double now_ms() { static std::atomic<int> t{1}; return t.fetch_add(1) * 0.1; } }
std::recursive_mutex g_cycleMx;
thread_local unsigned g_cycleDepth = 0;
bool g_mswHalfPending = false;
std::atomic<bool> g_mswRun{true}, g_mswWanted{true}, g_mswHalfRate{true};
std::atomic<uint32_t> g_mswNotReady{0}, g_mswHalfAssist{0}, g_mswHalfWorker{0};
const char* g_mswNotReadyWhy = "";
std::atomic<int64_t> g_gameNaturalUs{0};
int64_t g_hookPrevEnterUs = 0, g_hookPrevBlockedUs = 0, g_hookBlockedUs = 0;
enum { kPhWait = 0 };
std::atomic<uint32_t> g_phaseLastUs[1]{};
bool blocked = false;
std::vector<char> order;
const char* msw_blocker() { return blocked ? "a pace wait is outstanding" : nullptr; }
void msw_cycle() { order.push_back('S'); }
#include "msw_slot_body.inc"

int pass = 0, fail = 0;
void check(const char* name, bool ok) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", name); ok ? ++pass : ++fail; }
void reset() { order.clear(); g_mswHalfPending = false; blocked = false; g_mswRun = g_mswWanted = g_mswHalfRate = true; }
// This is the worker's production entry: never touch the token until try_lock succeeded.
bool worker() {
#ifdef OLD_CONTROL
    // Build 242 consumed seenEnds before try_lock: this models that exact lost obligation.
    const bool pending = g_mswHalfPending;
    g_mswHalfPending = false;
#endif
    if (!g_cycleMx.try_lock()) return false;
#ifdef OLD_CONTROL
    g_mswHalfPending = pending;
#endif
    const bool did = msw_half_slot(false);
    g_cycleMx.unlock();
    return did;
}
int main() {
    reset();
    g_cycleMx.lock(); g_mswHalfPending = true;
    std::thread contending([] { worker(); }); contending.join();
    check("worker losing Present's lock preserves the owed slot", g_mswHalfPending && order.empty());
    g_cycleMx.unlock();
    check("worker retries the same real frame after unlock", worker() && order.size() == 1);
    check("the serviced token cannot synthesize twice", !worker() && order.size() == 1);

    reset(); g_mswHalfPending = true;
    cycle_enter();
    check("fast Present services an owed slot before its real frame", order.size() == 1 && order[0] == 'S');
    g_mswHalfPending = true; // a nested hook cannot begin a synthetic cycle inside the outer Present
    cycle_enter(); check("nested Present does not synthesize", order.size() == 1 && g_mswHalfPending); cycle_leave();
    cycle_leave();
    check("outer release retains a newly ended real frame's token", worker() && order.size() == 2);

    reset();
    for (int i = 0; i < 200; ++i) {
        cycle_enter(); order.push_back('R'); g_mswHalfPending = true;
        std::thread loser([] { worker(); }); loser.join();
        cycle_leave();
        if (i % 2 == 0) worker(); // alternate a winning worker and a fast next Present
    }
    worker();
    bool alternating = order.size() == 400;
    for (size_t i = 0; i < order.size(); ++i) alternating &= order[i] == (i % 2 ? 'S' : 'R');
    check("200 contended real frames retain exact real/synthetic alternation", alternating);
    check("both worker and Present-assist paths ran", g_mswHalfWorker > 0 && g_mswHalfAssist > 0);

    reset(); blocked = true; g_mswHalfPending = true;
    cycle_enter(); cycle_leave();
    check("outstanding runtime wait cancels synthetic ownership without waiting", order.empty() && !g_mswHalfPending && g_mswNotReady > 0);
    reset(); g_mswWanted = false; g_mswHalfPending = true;
    check("toggle off cancels an owed slot", !worker() && order.empty() && !g_mswHalfPending);
    reset(); g_mswRun = false; g_mswHalfPending = true;
    check("stop cancels an owed slot", !worker() && order.empty() && !g_mswHalfPending);
    reset(); g_mswHalfRate = false; g_mswHalfPending = true;
    check("adaptive mode does not inherit a half-rate token", !worker() && order.empty() && !g_mswHalfPending);
    std::printf("%d PASS, %d FAIL\n", pass, fail);
    return fail ? 1 : 0;
}
