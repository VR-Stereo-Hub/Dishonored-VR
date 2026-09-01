#define DVR_CAT ::dvr::log::Cat::core
#include "core/hooks/detour.h"
#include "core/util/log.h"
#include <windows.h>
#include <string.h>
#include <stdio.h>

namespace dvr::hooks {

static void hex(const uint8_t* p, size_t n, char* out, size_t cap)
{
    size_t k = 0;
    for (size_t i = 0; i < n && k + 4 < cap; i++)
        k += (size_t)snprintf(out + k, cap - k, "%02x ", p[i]);
    if (k) out[k - 1] = 0; else out[0] = 0;
}

bool detour_install(Detour& d, const char* tag, uintptr_t at, const uint8_t* expected, size_t len, const void* stub)
{
    if (d.on) return true;
    if (len < 5 || len > sizeof(d.saved)) { DVR_ERROR("%s: bad detour length %u", tag, (unsigned)len); return false; }
    uint8_t* p = (uint8_t*)at;
    if (memcmp(p, expected, len) != 0) {
        char have[64], want[64];
        hex(p, len, have, sizeof(have)); hex(expected, len, want, sizeof(want));
        DVR_ERROR("%s: REFUSING to patch - bytes at 0x%08x are %s, expected %s. Wrong exe build?",
                  tag, (unsigned)at, have, want);
        return false;
    }
    DWORD op = 0;
    if (!VirtualProtect(p, len, PAGE_EXECUTE_READWRITE, &op)) { DVR_ERROR("%s: VirtualProtect failed", tag); return false; }
    memcpy(d.saved, p, len);
    int32_t rel = (int32_t)((uintptr_t)stub - (at + 5));
    p[0] = 0xE9;
    memcpy(p + 1, &rel, 4);
    for (size_t i = 5; i < len; i++) p[i] = 0x90;
    VirtualProtect(p, len, op, &op);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    d.at = at; d.len = len; d.on = true;
    DVR_INFO("%s: INSTALLED at 0x%08x -> stub %p", tag, (unsigned)at, stub);
    return true;
}

void detour_remove(Detour& d, const char* tag)
{
    if (!d.on) return;
    uint8_t* p = (uint8_t*)d.at;
    DWORD op = 0;
    if (VirtualProtect(p, d.len, PAGE_EXECUTE_READWRITE, &op)) {
        memcpy(p, d.saved, d.len);
        VirtualProtect(p, d.len, op, &op);
        FlushInstructionCache(GetCurrentProcess(), p, d.len);
    }
    d.on = false;
    DVR_INFO("%s: removed (bytes restored at 0x%08x)", tag, (unsigned)d.at);
}

} // namespace dvr::hooks
