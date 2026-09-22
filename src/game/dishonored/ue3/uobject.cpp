// game/dishonored/ue3/uobject.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static const char* NameFromIndex(uint32_t idx)
{
    void**   nameData = *(void***)kGNamesData;
    uint32_t nameNum  = *(uint32_t*)kGNamesNum;
    if (!nameData || idx >= nameNum) return NULL;
    if (!RangeReadable(nameData + idx, sizeof(void*))) return NULL;
    uint8_t* entry = (uint8_t*)nameData[idx];
    if (!entry || !RangeReadable(entry, 0x50)) return NULL;
    if ((*(uint32_t*)(entry + 8) >> 1) != idx) return NULL;
    return (const char*)(entry + 0x10);
}


static bool PrintableName(const char* s)
{
    if (!s || !s[0]) return false;
    for (int i = 0; i < 64; i++) {
        if (!s[i]) return true;
        if ((unsigned char)s[i] < 0x20 || (unsigned char)s[i] > 0x7E) return false;
    }
    return false; // no terminator within 64 chars
}


// A "real" name = valid, printable, and NOT index 0 ("None"). Zero-filled
// object fields resolve to GNames[0]="None", which fooled the first detector.
static const char* RealName(uint32_t idx)
{
    if (idx == 0) return NULL;
    const char* nm = NameFromIndex(idx);
    return (nm && PrintableName(nm)) ? nm : NULL;
}


static void HexDumpObject(const char* label, uint8_t* o, size_t bytes)
{
    Log("probe: dump %s @ %p", label, (void*)o);
    for (size_t off = 0; off < bytes; off += 16) {
        if (!RangeReadable(o + off, 16)) break;
        uint32_t* d = (uint32_t*)(o + off);
        float*    f = (float*)(o + off);
        Log("probe:   +0x%03x  %08x %08x %08x %08x   | % .2f % .2f % .2f % .2f",
            (unsigned)off, d[0], d[1], d[2], d[3], f[0], f[1], f[2], f[3]);
    }
}


// ----------------------------------------------------------------------------
// STEREO: locate the live camera, then offset it per-eye each frame.
// ----------------------------------------------------------------------------
static const char* ObjClassName(uint8_t* o)
{
    if (!RangeReadable(o, kClassOff + 4)) return NULL;
    uint8_t* cls = *(uint8_t**)(o + kClassOff);
    if (!cls || ((uintptr_t)cls & 3) || !RangeReadable(cls, kNameOff + 8)) return NULL;
    return RealName(*(uint32_t*)(cls + kNameOff));
}


static int CmpPtr(const void* a, const void* b)
{
    uintptr_t x = (uintptr_t)*(void* const*)a, y = (uintptr_t)*(void* const*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}


// VR-88: the table is rebuilt from the PRESENT thread (RotInjectTick's
// controller scan) and read from the SCRIPT lane (the animation reader, the
// restore paths). A rebuild zeroes the count, may realloc the buffer and sorts
// in place, so an unlocked reader can search a half-built or freed array.
static SRWLOCK g_liveLock = SRWLOCK_INIT;

// VR-160: a rebuild is a copy and a sort of about 115,000 pointers. It used to
// run with the table's lock held for all of it (about 12 ms, `perf parts`:
// gs.uiSurfacePoll), several times a second from independent timers on the
// present thread and the script lane, so every rebuild was a stall on the
// thread that called it AND a block on every IsLiveObject reader on the other.
// Now: the copy and the sort run into a SCRATCH buffer with no reader lock held
// (std::sort on integers, not qsort through a function pointer), and the table
// is swapped in under the lock in microseconds. One builder at a time.
// A failed rebuild still empties the table: IsLiveObject then refuses
// everything, which is the fail-safe this table has always had.
static SRWLOCK   g_liveBuildLock = SRWLOCK_INIT;
static void**    g_liveScratch = NULL;
static uint32_t  g_liveScratchCap = 0;
static ULONGLONG g_liveBuiltMs = 0;      // GetTickCount64 of the last successful rebuild
static uint32_t  g_liveRebuilds = 0, g_liveReuses = 0;

static bool BuildLiveSet()
{
    AcquireSRWLockExclusive(&g_liveBuildLock);
    struct UnlockBuild { ~UnlockBuild() { ReleaseSRWLockExclusive(&g_liveBuildLock); } } unlockBuild;
    uint32_t n = 0;
    bool ok = false;
    do {
        if (!RangeReadable((void*)kGObjHdr, 12)) break;
        void**   objs = *(void***)kGObjHdr;
        uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
        if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) break;
        if (!RangeReadable(objs, (size_t)num * sizeof(void*))) break;
        if (num > g_liveScratchCap) {
            void** p = (void**)realloc(g_liveScratch, (size_t)(num + 4096) * sizeof(void*));
            if (!p) break;
            g_liveScratch = p; g_liveScratchCap = num + 4096;
        }
        for (uint32_t i = 0; i < num; i++) {
            void* o = objs[i];
            if (o && !((uintptr_t)o & 3)) g_liveScratch[n++] = o;
        }
        std::sort(g_liveScratch, g_liveScratch + n,
                  [](void* a, void* b) { return (uintptr_t)a < (uintptr_t)b; });
        ok = true;
    } while (0);

    AcquireSRWLockExclusive(&g_liveLock);
    if (ok) {
        void** t = g_liveSet; g_liveSet = g_liveScratch; g_liveScratch = t;
        uint32_t c = g_liveCap; g_liveCap = g_liveScratchCap; g_liveScratchCap = c;
        g_liveN = n;
        g_liveBuiltMs = GetTickCount64();
        ++g_liveRebuilds;
    } else {
        g_liveN = 0;
    }
    const bool good = ok && g_liveN > 1000;
    ReleaseSRWLockExclusive(&g_liveLock);
    return good;
}

// VR-160: for the PERIODIC callers. Each of them rebuilt on its own timer, so
// the table was rebuilt five or six times a second to satisfy five "no older
// than my period" contracts that one rebuild satisfies. A caller passes the age
// it already tolerated (its own period) and gets exactly that guarantee.
// A caller that needs the table fresh NOW keeps calling BuildLiveSet().
static bool RefreshLiveSet(uint32_t maxAgeMs)
{
    AcquireSRWLockShared(&g_liveLock);
    const bool fresh = g_liveN > 1000 && GetTickCount64() - g_liveBuiltMs < maxAgeMs;
    ReleaseSRWLockShared(&g_liveLock);
    if (fresh) { ++g_liveReuses; return true; }
    return BuildLiveSet();
}


static bool IsLiveObject(uint8_t* p)
{
    if (!p || ((uintptr_t)p & 3)) return false;
    AcquireSRWLockShared(&g_liveLock);
    struct Unlock { ~Unlock() { ReleaseSRWLockShared(&g_liveLock); } } unlock;
    if (!g_liveN) return false;
    uint32_t lo = 0, hi = g_liveN - 1;
    while (lo <= hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        uintptr_t v = (uintptr_t)g_liveSet[mid], t = (uintptr_t)p;
        if (v == t) return true;
        if (v < t) lo = mid + 1; else { if (!mid) break; hi = mid - 1; }
    }
    return false;
}


static bool LooksLikeObject(uint8_t* o)
{
    if (!IsLiveObject(o)) return false;                 // must be in GObjects
    if (!RangeReadable(o, kClassOff + 8)) return false;
    const char* cn = ObjClassName(o);
    if (!cn) return false;
    int n = 0;
    for (const char* p = cn; *p; ++p, ++n) {
        if (n > 60) return false;
        unsigned char ch = (unsigned char)*p;
        if (ch < '0' || ch > 'z') return false;
    }
    return n > 2;
}


// Runs on the game thread for every script event. Must be cheap: this fires
// thousands of times a second.
// A UFunction is itself a UObject whose CLASS is named "Function" - that is an
// exact test, so instead of assuming which argument holds it, check all three.
static uint8_t* AsUFunction(void* p)
{
    if (!p || ((uintptr_t)p & 3)) return NULL;
    if (!RangeReadable(p, kClassOff + 8)) return NULL;
    const char* cn = ObjClassName((uint8_t*)p);
    if (!cn) return NULL;
    if (strcmp(cn, "Function") && strcmp(cn, "State") && strcmp(cn, "DisFunction"))
        return NULL;
    return (uint8_t*)p;
}

// (g_viewPitchRad / g_viewYawRad moved up beside the HMD state - both camera
// writers publish them now, see 32.92)

// Say WHY we came up empty, once per distinct reason. Build 26.0 failed in
// total silence, which told us nothing; that is a worse bug than the failure.
static void FpWhy(const char* reason)
{
    static const char* last = NULL;
    if (last == reason) return;
    last = reason;
    Log("handmesh: not found yet - %s", reason);
}


static bool LooksLikeObj(uint8_t* p)
{
    if (!p || ((uintptr_t)p & 3)) return false;
    if (!RangeReadable(p, kClassOff + 8)) return false;
    const char* cn = ObjClassName(p);
    if (!cn) return false;
    int n = 0;
    for (const char* q = cn; *q; ++q, ++n) {
        if (n > 60) return false;
        unsigned char ch = (unsigned char)*q;
        if (ch < '0' || ch > 'z') return false;
    }
    return n > 2;
}


#include "game/dishonored/ue3/name_index_cache.h"
#include <string>
#include <unordered_map>

// ---- VR-102: what the startup freeze was made of --------------------------------------
// Every name lookup walked the whole GNames table, and every property lookup the whole
// GObjects table, each entry behind a VirtualQuery (RangeReadable). About 100 ms per
// lookup on this build, and the mod does well over a hundred one-time lookups in its
// first minute - the 10-15 s freeze after launch (measured 2026-09-22: 14.5 s without a
// game frame, the log advancing one `rfl:` line per ~100 ms). The counters below make that
// arithmetic on every run, lever on or off; LookupCostReport prints them at each game-state
// change. With [Menu] CacheNameLookups=1 a lookup is a hash probe into indexes built ONCE.
static LONG   g_luNameScans = 0, g_luPropWalks = 0, g_luIndexHits = 0, g_luIndexBuilds = 0;
static double g_luNameMs = 0.0, g_luPropMs = 0.0, g_luBuildMs = 0.0;
static double LuNowMs()
{
    static LARGE_INTEGER f = {}; if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

// The readability memo moved to core/util/mem.h (dvr::mem::RegionMemo) so the
// other GObjects walks can share it (VR-102).
using LuRegion = ::dvr::mem::RegionMemo;

// The full name index: every printable GNames entry once, extended (never rescanned)
// when the table grows. The FIRST index a string appears at wins, which is what the
// legacy scan from index 1 upward returned. A hit is re-read from GNames before use.
static SRWLOCK g_luNameLock = SRWLOCK_INIT;
static std::unordered_map<std::string, uint32_t>* g_luNames = nullptr;
static uint32_t g_luNamesBuiltTo = 1;

static void LuExtendNames(uint32_t num)
{
    if (!g_luNames) { g_luNames = new std::unordered_map<std::string, uint32_t>(); g_luNames->reserve(262144); }
    if (num <= g_luNamesBuiltTo) return;
    const double t0 = LuNowMs();
    void** nameData = *(void***)kGNamesData;
    LuRegion rd, re;
    const uint32_t from = g_luNamesBuiltTo;
    for (uint32_t i = from; i < num; ++i) {
        if (!rd.ok(nameData + i, sizeof(void*))) continue;
        uint8_t* e = (uint8_t*)nameData[i];
        if (!e || !re.ok(e, 0x50) || (*(uint32_t*)(e + 8) >> 1) != i) continue;
        const char* nm = (const char*)(e + 0x10);
        if (PrintableName(nm)) g_luNames->emplace(nm, i);
    }
    g_luNamesBuiltTo = num;
    InterlockedIncrement(&g_luIndexBuilds);
    const double ms = LuNowMs() - t0; g_luBuildMs += ms;
    Log("lookup/index: names %u..%u indexed in %.0f ms (%zu distinct) - [Menu] CacheNameLookups=1", from, num, ms, g_luNames->size());
}

static uint32_t FindNameIdx(const char* want)
{
    if (!want) return 0xffffffffu;
    if (!RangeReadable((void*)kGNamesData, 8)) return 0xffffffffu;
    uint32_t num = *(uint32_t*)kGNamesNum;
    if (num == 0 || num > 4000000) return 0xffffffffu;
    if (g_nameIndexCacheOn) {
        AcquireSRWLockExclusive(&g_luNameLock);
        struct Unlock { ~Unlock() { ReleaseSRWLockExclusive(&g_luNameLock); } } unlock;
        for (int pass = 0; pass < 2; ++pass) {
            if (g_luNames) {
                auto it = g_luNames->find(want);
                if (it != g_luNames->end()) {
                    const char* nm = NameFromIndex(it->second);
                    if (nm && !strcmp(nm, want)) { InterlockedIncrement(&g_luIndexHits); return it->second; }
                }
            }
            if (pass == 0) LuExtendNames(num);   // absent: index what the table gained, then ask once more
        }
        return 0xffffffffu;   // the whole table is indexed: absent means absent
    }
    // Legacy: a full scan per lookup (the freeze). Timed, so the A/B is arithmetic.
    const double t0 = LuNowMs();
    uint32_t found = 0xffffffffu;
    for (uint32_t i = 1; i < num; i++) {
        const char* nm = NameFromIndex(i);
        if (nm && !strcmp(nm, want)) { found = i; break; }
    }
    InterlockedIncrement(&g_luNameScans); g_luNameMs += LuNowMs() - t0;
    return found;
}

// The property index: (declaring outer's name index, property name index) -> the FIRST
// matching property in GObjects order, as the legacy walks return, for any property class
// and separately for BoolProperty (FindBoolProp's rule). Built in one walk; the class
// verdict is cached per class object, so ObjClassName runs once per distinct class. Only
// offsets and masks are kept, never object pointers, so a later GC cannot stale an answer.
// A miss falls back to the legacy walk (a class loaded after the build), and a table
// that has grown by more than 2000 objects since the build is re-indexed first.
struct LuProp { uint32_t off = 0; bool any = false; uint32_t boolOff = 0, boolMask = 0; bool isBool = false; };
static SRWLOCK g_luPropLock = SRWLOCK_INIT;
static std::unordered_map<uint64_t, LuProp>* g_luProps = nullptr;
static uint32_t g_luPropsBuiltAt = 0;
static bool g_luPropsBad = false;   // the self-check disagreed: legacy only for the session

static void LuBuildProps(void** objs, uint32_t onum)
{
    const double t0 = LuNowMs();
    auto* m = new std::unordered_map<uint64_t, LuProp>(); m->reserve(65536);
    std::unordered_map<uint8_t*, uint8_t> clsVerdict;   // 0 not a property, 1 a property, 2 BoolProperty
    LuRegion ra, ro;
    for (uint32_t i = 0; i < onum; i++) {
        if (!ra.ok(objs + i, sizeof(void*))) continue;
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !ro.ok(o, 0x80)) continue;
        uint8_t* cls = *(uint8_t**)(o + kClassOff);
        auto cv = clsVerdict.find(cls);
        uint8_t v;
        if (cv == clsVerdict.end()) {
            const char* pc = ObjClassName(o);
            v = !pc || !strstr(pc, "Property") ? 0 : !strcmp(pc, "BoolProperty") ? 2 : 1;
            clsVerdict.emplace(cls, v);
        } else v = cv->second;
        if (!v) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        const uint64_t key = ((uint64_t)*(uint32_t*)(ou + kNameOff) << 32) | *(uint32_t*)(o + kNameOff);
        LuProp& e = (*m)[key];
        if (!e.any) { e.any = true; e.off = *(uint32_t*)(o + kUPropOffset); }
        if (v == 2 && !e.isBool) { e.isBool = true; e.boolOff = *(uint32_t*)(o + kUPropOffset); e.boolMask = *(uint32_t*)(o + kUBoolBitMask); }
    }
    delete g_luProps; g_luProps = m; g_luPropsBuiltAt = onum;
    InterlockedIncrement(&g_luIndexBuilds);
    const double ms = LuNowMs() - t0; g_luBuildMs += ms;
    Log("lookup/index: %zu properties from %u objects indexed in %.0f ms (%zu classes seen) - [Menu] CacheNameLookups=1",
        m->size(), onum, ms, clsVerdict.size());
}

// 0 = use the index answer in *e, 1 = not indexed (the caller walks). Takes the name
// indexes the caller already has.
static __declspec(thread) bool t_luBypass = false;   // the self-check's legacy half
static int LuPropLookup(uint32_t ci, uint32_t pi, LuProp* out)
{
    if (!g_nameIndexCacheOn || g_luPropsBad || t_luBypass) return 1;
    if (!RangeReadable((void*)kGObjHdr, 12)) return 1;
    void** objs = *(void***)kGObjHdr;
    const uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return 1;
    AcquireSRWLockExclusive(&g_luPropLock);
    struct Unlock { ~Unlock() { ReleaseSRWLockExclusive(&g_luPropLock); } } unlock;
    const uint64_t key = ((uint64_t)ci << 32) | pi;
    for (int pass = 0; pass < 2; ++pass) {
        if (!g_luProps) LuBuildProps(objs, onum);
        auto it = g_luProps->find(key);
        if (it != g_luProps->end()) { *out = it->second; InterlockedIncrement(&g_luIndexHits); return 0; }
        if (pass == 0 && onum > g_luPropsBuiltAt + 2000) { delete g_luProps; g_luProps = nullptr; continue; }
        break;
    }
    return 1;
}

// One line of totals since launch; called on every [game] state change.
static void LookupCostReport(const char* why)
{
    Log("lookup/cost (%s): legacy name scans %ld = %.0f ms, legacy object walks %ld = %.0f ms, index builds %ld = %.0f ms, "
        "index hits %ld | CacheNameLookups=%d. Each legacy scan or walk is paid on the game thread; their sum is the "
        "startup freeze the mod causes (VR-102).",
        why ? why : "?", g_luNameScans, g_luNameMs, g_luPropWalks, g_luPropMs, g_luIndexBuilds, g_luBuildMs, g_luIndexHits,
        (int)g_nameIndexCacheOn);
}


static uint8_t* FindFunctionObj(const char* fname)
{
    uint32_t idx = FindNameIdx(fname);
    if (idx == 0xffffffffu) {
        Log("arms: '%s' is not in GNames at all", fname);
        return NULL;
    }
    if (!RangeReadable((void*)kGObjHdr, 12)) return NULL;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return NULL;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3)) continue;
        if (!RangeReadable(o, kNameOff + 8)) continue;
        if (*(uint32_t*)(o + kNameOff) != idx) continue;
        const char* cn = ObjClassName(o);
        if (cn && !strcmp(cn, "Function")) return o;
    }
    Log("arms: no UFunction named '%s' in GObjects", fname);
    return NULL;
}


static void LuSelfCheck();
static bool FindPropOffsetChecked(const char* clsName, const char* propName, uint32_t* result)
{
    LuSelfCheck();
    uint32_t ci = FindNameIdx(clsName), pi = FindNameIdx(propName);
    if (ci == 0xffffffffu || pi == 0xffffffffu) return false;
    { LuProp e; if (LuPropLookup(ci, pi, &e) == 0 && e.any) { *result = e.off; return true; } }
    const double luT0 = LuNowMs();
    struct LuTime { double t0; ~LuTime() { InterlockedIncrement(&g_luPropWalks); g_luPropMs += LuNowMs() - t0; } } luTime{luT0};
    if (!RangeReadable((void*)kGObjHdr, 12)) return false;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return false;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (*(uint32_t*)(o + kNameOff) != pi) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        if (*(uint32_t*)(ou + kNameOff) != ci) continue;
        const char* pc = ObjClassName(o);
        if (!pc || !strstr(pc, "Property")) continue;
        *result = *(uint32_t*)(o + kUPropOffset); return true;          // UProperty::Offset
    }
    return false;
}


static uint32_t FindPropOffset(const char* clsName, const char* propName)
{
    uint32_t result=0;
    FindPropOffsetChecked(clsName,propName,&result);
    return result;
}

// 38.23: FindPropOffset's sibling for BOOL properties - offset + bitmask
// (UBoolProperty::BitMask at +0x6c, same layout blockhunt reads).
static bool FindBoolProp(const char* clsName, const char* propName,
                         uint32_t* off, uint32_t* mask)
{
    *off = 0; *mask = 0;
    LuSelfCheck();
    uint32_t ci = FindNameIdx(clsName), pi = FindNameIdx(propName);
    if (ci == 0xffffffffu || pi == 0xffffffffu) return false;
    { LuProp e; if (LuPropLookup(ci, pi, &e) == 0 && e.isBool) { *off = e.boolOff; *mask = e.boolMask; return *off != 0 && *mask != 0; } }
    const double luT0 = LuNowMs();
    struct LuTime { double t0; ~LuTime() { InterlockedIncrement(&g_luPropWalks); g_luPropMs += LuNowMs() - t0; } } luTime{luT0};
    if (!RangeReadable((void*)kGObjHdr, 12)) return false;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return false;
    for (uint32_t i = 0; i < onum; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = onum - i;
            if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x80)) continue;
        if (*(uint32_t*)(o + kNameOff) != pi) continue;
        uint8_t* ou = *(uint8_t**)(o + kOuterOff);
        if (!ou || ((uintptr_t)ou & 3) || !RangeReadable(ou, kNameOff + 4)) continue;
        if (*(uint32_t*)(ou + kNameOff) != ci) continue;
        const char* pc = ObjClassName(o);
        if (!pc || strcmp(pc, "BoolProperty")) continue;
        *off  = *(uint32_t*)(o + kUPropOffset);
        *mask = *(uint32_t*)(o + kUBoolBitMask);
        return *off != 0 && *mask != 0;
    }
    return false;
}

// The index must return exactly what the legacy walks return. Once, the first time the
// index exists: five known properties (three offsets, two bools) through both paths.
// Any disagreement switches the property index off for the session and says so.
static void LuSelfCheck()
{
    static LONG done = 0;
    if (!g_nameIndexCacheOn || !g_luProps || g_luPropsBad || t_luBypass || InterlockedExchange(&done, 1)) return;
    struct Pair { const char* c; const char* p; bool b; };
    const Pair pairs[] = { {"Actor", "Location", false}, {"Pawn", "EyeHeight", false},
                           {"DishonoredCameraInfluence", "m_Weight", false},
                           {"Pawn", "bWantsToCrouch", true}, {"DishonoredPlayerCamera", "m_bSmoothingSuddenCollision", true} };
    int agree = 0, n = 0; char line[400] = ""; int at = 0;
    for (const Pair& q : pairs) {
        uint32_t io = 0, im = 0, lo = 0, lm = 0; bool iok = false, lok = false;
        if (q.b) { iok = FindBoolProp(q.c, q.p, &io, &im); t_luBypass = true; lok = FindBoolProp(q.c, q.p, &lo, &lm); t_luBypass = false; }
        else     { iok = FindPropOffsetChecked(q.c, q.p, &io); t_luBypass = true; lok = FindPropOffsetChecked(q.c, q.p, &lo); t_luBypass = false; }
        const bool same = iok == lok && io == lo && im == lm;
        agree += same; ++n;
        if (at < (int)sizeof(line) - 80)
            at += _snprintf(line + at, sizeof(line) - at, " %s::%s %s(+0x%x/0x%x vs +0x%x/0x%x)", q.c, q.p, same ? "ok" : "MISMATCH", io, im, lo, lm);
    }
    if (agree != n) g_luPropsBad = true;
    Log("lookup/selfcheck: index vs legacy walk - %d of %d agree%s |%s", agree, n,
        agree == n ? "" : " - the PROPERTY INDEX IS OFF for this session (the legacy walks answer)", line);
}
