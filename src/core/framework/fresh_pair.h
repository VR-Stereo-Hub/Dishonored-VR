// Submission identity, independent of Present parity and compositor reprojection.
#pragma once
#include <stdint.h>
namespace dvr::perf {
struct FreshPair {
    uint32_t left = 0, right = 0;
    bool accept(bool submittedStereo, uint32_t l, uint32_t r) {
        if (!submittedStereo || !l || !r || l == r || (left && int32_t(l - left) <= 0) || (right && int32_t(r - right) <= 0))
            return false;
        left = l; right = r;
        return true;
    }
};
} // namespace dvr::perf
