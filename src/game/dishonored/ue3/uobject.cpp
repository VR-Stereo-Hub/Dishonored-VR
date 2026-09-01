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


static bool BuildLiveSet()
{
    g_liveN = 0;
    if (!RangeReadable((void*)kGObjHdr, 12)) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return false;
    if (!RangeReadable(objs, (size_t)num * sizeof(void*))) return false;
    if (num > g_liveCap) {
        void** p = (void**)realloc(g_liveSet, (size_t)(num + 4096) * sizeof(void*));
        if (!p) return false;
        g_liveSet = p; g_liveCap = num + 4096;
    }
    for (uint32_t i = 0; i < num; i++) {
        void* o = objs[i];
        if (o && !((uintptr_t)o & 3)) g_liveSet[g_liveN++] = o;
    }
    qsort(g_liveSet, g_liveN, sizeof(void*), CmpPtr);
    return g_liveN > 1000;
}


static bool IsLiveObject(uint8_t* p)
{
    if (!p || ((uintptr_t)p & 3) || !g_liveN) return false;
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


static uint32_t FindNameIdx(const char* want)
{
    if (!RangeReadable((void*)kGNamesData, 8)) return 0xffffffffu;
    uint32_t num = *(uint32_t*)kGNamesNum;
    if (num == 0 || num > 4000000) return 0xffffffffu;
    for (uint32_t i = 1; i < num; i++) {
        const char* nm = NameFromIndex(i);
        if (nm && !strcmp(nm, want)) return i;
    }
    return 0xffffffffu;
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


static uint32_t FindPropOffset(const char* clsName, const char* propName)
{
    uint32_t ci = FindNameIdx(clsName), pi = FindNameIdx(propName);
    if (ci == 0xffffffffu || pi == 0xffffffffu) return 0;
    if (!RangeReadable((void*)kGObjHdr, 12)) return 0;
    void** objs = *(void***)kGObjHdr;
    uint32_t onum = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || onum < 1000 || onum > 4000000) return 0;
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
        return *(uint32_t*)(o + 0x5c);          // UProperty::Offset
    }
    return 0;
}


// 38.23: FindPropOffset's sibling for BOOL properties - offset + bitmask
// (UBoolProperty::BitMask at +0x6c, same layout blockhunt reads).
static bool FindBoolProp(const char* clsName, const char* propName,
                         uint32_t* off, uint32_t* mask)
{
    *off = 0; *mask = 0;
    uint32_t ci = FindNameIdx(clsName), pi = FindNameIdx(propName);
    if (ci == 0xffffffffu || pi == 0xffffffffu) return false;
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
        *off  = *(uint32_t*)(o + 0x5c);
        *mask = *(uint32_t*)(o + 0x6c);
        return *off != 0 && *mask != 0;
    }
    return false;
}
