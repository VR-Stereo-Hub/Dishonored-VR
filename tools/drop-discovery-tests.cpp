#include "game/dishonored/drop_discovery.h"
#include <cassert>
#include <cstdio>
int main() {
    dvr::drop::DiscoverySchedule s;
    assert(s.begin(100, 1, 100000));
    s.scan = 1024;
    assert(!s.begin(110, 1, 100000)); // no repeat on next anim tick
    assert(s.begin(120, 1, 100000));
    assert(s.scan == 1024); // bounded slices resume, not restart
    s.exhausted(200);
    assert(!s.begin(210, 1, 100000));
    assert(!s.begin(1199, 1, 100000));
    assert(s.begin(1200, 1, 100000)); // retry a context created in a reused slot
    s.exhausted(1300);
    assert(s.begin(1301, 1, 100001)); // appended context wakes immediately
    s.exhausted(1400);
    assert(s.begin(1401, 2, 100001)); // possession change wakes immediately
    assert(s.scan == 0);
    s.scan = 90000;
    s.exhausted(1500);
    assert(s.begin(1501, 2, 80000)); // smaller replacement table
    assert(s.scan == 0);
    s.exhausted(1600);
    s.invalidate();
    assert(s.begin(1601, 2, 80000)); // freed/changed owner of cached context
    unsigned slices = 0;
    dvr::drop::DiscoverySchedule absent;
    for (uint64_t t = 1; t <= 10000; t += 10) {
        if (!absent.begin(t, 3, 4096)) continue;
        ++slices;
        absent.scan += 1024;
        if (absent.scan >= absent.count) absent.exhausted(t);
    }
    assert(slices <= 40); // long absent-context period does not busy-scan
    std::printf("drop discovery: cadence, retry, load, owner and absent-context checks passed (%u slices/10s)\n", slices);
}
