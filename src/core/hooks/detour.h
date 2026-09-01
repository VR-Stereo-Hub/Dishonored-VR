// core/hooks/detour.h - byte-verified 5-byte jump detours into engine code.
//
// Dishonored.exe has no ASLR, so every engine hook is an absolute address
// (src/game/dishonored/patterns.h) paired with the bytes expected there. A
// detour refuses to patch when the bytes differ (wrong exe build) - that
// refusal is the only build check the mod has, so keep it loud.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace dvr::hooks {

struct Detour {
    uintptr_t   at = 0;        // first byte of the patched instruction(s)
    uint8_t     saved[16] = {}; // the original bytes
    size_t      len = 0;       // bytes replaced (>= 5)
    bool        on = false;
};

// Verifies `expected` (len bytes) at `at`, then writes jmp rel32 to `stub`
// (plus nop padding when len > 5). Logs the refusal with the bytes found.
// `tag` names the hook in the log ("blinkdir").
bool detour_install(Detour& d, const char* tag, uintptr_t at, const uint8_t* expected, size_t len, const void* stub);
// Restores the saved bytes. Safe to call when not installed.
void detour_remove(Detour& d, const char* tag);

} // namespace dvr::hooks
