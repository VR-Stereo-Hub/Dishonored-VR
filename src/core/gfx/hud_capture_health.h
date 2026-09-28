// Capture readiness is independent of whether this frame contains visible HUD.
#pragma once
#include <atomic>
#include <cstdint>
namespace dvr::hudcap {
class CaptureHealth {
    std::atomic<bool> proven{false};
    std::atomic<uint32_t> lastReady{0};
public:
    void reset() { proven.store(false); lastReady.store(0); }
    void frame(bool armed, bool ready, bool failed, bool drew, uint32_t now) {
        if (!ready || failed) { reset(); return; }
        if (!armed) return;
        if (drew) proven.store(true);
        // Empty HUD is normal after its widgets fade. Renew only while the
        // previously exercised capture path remains armed and operational.
        if (proven.load()) lastReady.store(now);
    }
    void native(bool success, bool enabled, bool ready, bool failed, uint32_t now) {
        if (!success || !enabled || !ready || failed) return;
        lastReady.store(now); proven.store(true);
    }
    bool healthy(bool enabled, bool ready, bool failed, uint32_t now) const {
        return enabled && ready && !failed && proven.load() &&
               uint32_t(now-lastReady.load()) < 500;
    }
};
}
