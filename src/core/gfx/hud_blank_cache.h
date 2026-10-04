#pragma once
#include "core/gfx/blit_quad.h"
#include <cstring>
namespace dvr::hudcap {
// Only successful copies of a proven clear target can seed this cache.
// Reset with shared/output resource lifetime. Nonblank frames always run normally.
struct BlankCache {
    bool slot[2] = {}, output = false;
    dvr::gfx::AlphaParams params;
    bool copy_needed(int index, bool clear) const { return !clear || !slot[index]; }
    void copied(int index, bool clear, bool success) { slot[index] = clear && success; }
    bool reuse_output(int index, const dvr::gfx::AlphaParams& next, bool parts) const {
        return slot[index] && output && !parts && std::memcmp(&params,&next,sizeof(params))==0;
    }
    void converted(int index, const dvr::gfx::AlphaParams& next, bool parts) {
        output=slot[index] && !parts;params=next;
    }
};
}
