// game/dishonored/ue3/gobj_walk.h - one pass over GObjects at the cost of a pass,
// not of 100,000 VirtualQuery calls (VR-102).
//
// The hand-rolled walks this replaces paid one RangeReadable per object plus four
// more inside ObjClassName, which is about 5 us per object and half a second per
// walk on a 100k-object level. Entering gameplay ran five of them back to back on
// the game thread (SkelControl probe, its property dumps, the offset audit, the
// graft censuses) and the game produced no frame for 1.6 s. Here readability is
// memoised per memory region (dvr::mem::RegionMemo) and a class name is resolved
// once per class pointer, the way the property index build already does.
//
// Game thread only (the script lane): the collector runs on that thread too, so no
// object the walk has reached can be freed under it.
#pragma once
#include <stdint.h>
#include <unordered_map>

struct GObjWalkStats { uint32_t num = 0, visited = 0; double ms = 0.0; bool ran = false; };

// fn(uint32_t index, uint8_t* obj, const char* className) -> bool (false stops).
// Every obj handed to fn is aligned and readable for `need` bytes (at least up to
// its class pointer); className may be NULL for an unreadable or unnamed class.
template <class F>
static GObjWalkStats GObjForEach(size_t need, F&& fn)
{
    GObjWalkStats st;
    if (need < kClassOff + 4) need = kClassOff + 4;
    if (!RangeReadable((void*)kGObjHdr, 12)) return st;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || num < 1000 || num > 4000000) return st;
    LARGE_INTEGER f, c0, c1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c0);
    st.num = num; st.ran = true;
    ::dvr::mem::RegionMemo rt, ro;
    std::unordered_map<uint8_t*, const char*> names; names.reserve(4096);
    for (uint32_t i = 0; i < num; ++i) {
        if (!rt.ok(objs + i, sizeof(void*))) break;
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !ro.ok(o, need)) continue;
        ++st.visited;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        const char* cn;
        auto it = names.find(cls);
        if (it == names.end()) { cn = ObjClassName(o); names.emplace(cls, cn); }
        else cn = it->second;
        if (!fn(i, o, cn)) break;
    }
    QueryPerformanceCounter(&c1);
    st.ms = (double)(c1.QuadPart - c0.QuadPart) * 1000.0 / (double)f.QuadPart;
    return st;
}
