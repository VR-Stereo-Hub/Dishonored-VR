// core/util/mem.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static bool RangeReadable(const void* p, size_t n)
{
    const uint8_t* cur = (const uint8_t*)p;
    const uint8_t* end = cur + n;
    if (!p || (uintptr_t)p < 0x10000) return false;
    // 30.11: p + n wrapping past zero made the while loop below never run,
    // so a field holding 0xFFFFFFFF (UE3's INDEX_NONE sentinel) came back
    // "readable" and the very next dereference faulted. THE hole behind the
    // bone-probe aborts - and quite possibly the old FpCollect crash too.
    if ((uintptr_t)p + n < (uintptr_t)p) return false;
    while (cur < end) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(cur, &mbi, sizeof(mbi))) return false;
        if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            return false;
        cur = (const uint8_t*)mbi.BaseAddress + mbi.RegionSize;
    }
    return true;
}


static bool SafeRead32(uintptr_t p, uint32_t* out)
{
    if (p < 0x10000 || (p & 3)) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)p, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    *out = *(uint32_t*)p;
    return true;
}
