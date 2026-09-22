// core/util/mem.h - safe reads of game memory. Every pointer the mod pulls out
// of a UE3 object goes through these before it is dereferenced.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace dvr::mem {
// True when every page of [p, p+n) is committed and readable. Rejects
// p + n wrapping past zero (UE3's INDEX_NONE sentinel 0xFFFFFFFF once read as
// "readable" and the next dereference faulted).
bool range_readable(const void* p, size_t n);
// One aligned dword, or false without touching the page.
bool safe_read32(uintptr_t p, uint32_t* out);

// A readability memo for a tight loop: one VirtualQuery per memory region instead
// of one per entry. Only for a loop on one thread over memory the engine does not
// free under it (a GObjects walk on the game thread: the collector runs there too).
struct RegionMemo {
    uintptr_t lo = 0, hi = 0;
    bool ok(const void* p, size_t n);
};
} // namespace dvr::mem

// Original names.
inline bool RangeReadable(const void* p, size_t n) { return ::dvr::mem::range_readable(p, n); }
inline bool SafeRead32(uintptr_t p, uint32_t* out) { return ::dvr::mem::safe_read32(p, out); }
