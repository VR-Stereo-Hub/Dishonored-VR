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
} // namespace dvr::mem

// Original names.
inline bool RangeReadable(const void* p, size_t n) { return ::dvr::mem::range_readable(p, n); }
inline bool SafeRead32(uintptr_t p, uint32_t* out) { return ::dvr::mem::safe_read32(p, out); }
