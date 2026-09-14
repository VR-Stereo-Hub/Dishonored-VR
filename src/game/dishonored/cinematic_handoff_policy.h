#pragma once
#include <cstdint>
inline bool CineDispatchRecent(bool enabled,uint64_t seen,uint64_t now) {
    return enabled && seen && now>=seen && now-seen<750;
}
