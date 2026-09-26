// VR-229: publish the stereo displacement axis, independently of position axes.
#pragma once
#include <atomic>
#include <cstdint>
namespace dvr::camera {
// One script-lane writer; bounded, allocation-free render-lane snapshots.
class EyeBasis {
    std::atomic<uint32_t> sequence_{0};
    std::atomic<float> right_[3]{};
public:
    void publish(const float nativeRight[3], const float* scopedRight) {
        const float* right=scopedRight ? scopedRight : nativeRight;
        sequence_.fetch_add(1,std::memory_order_acq_rel);
        for(int i=0;i<3;++i) right_[i].store(right[i],std::memory_order_release);
        sequence_.fetch_add(1,std::memory_order_release);
    }
    bool read(float out[3]) const {
        for(int attempt=0;attempt<2;++attempt) {
            const auto before=sequence_.load(std::memory_order_acquire);
            if(!before || (before&1)) continue;
            float candidate[3];
            for(int i=0;i<3;++i) candidate[i]=right_[i].load(std::memory_order_acquire);
            if(before==sequence_.load(std::memory_order_acquire)) {
                for(int i=0;i<3;++i) out[i]=candidate[i];
                return true;
            }
        }
        return false;
    }
};
}
