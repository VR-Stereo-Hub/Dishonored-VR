// core/hooks/iat.h - import-table hooks. The mod wraps GetSystemMetrics,
// SetWindowPos and friends this way (to hold the 4032x2268 render window
// against the window manager) and the game's XInput imports (to serve the VR
// controllers as a gamepad).
#pragma once
#include <windows.h>

namespace dvr::hooks {
// Address of the IAT entry `mod` uses to call dllName!funcName, or null.
void** find_iat_slot_in(HMODULE mod, const char* dllName, const char* funcName);
// Same for the main exe.
void** find_iat_slot(const char* dllName, const char* funcName);
// Writes hook into an IAT slot (unprotect, swap, reprotect). Returns the
// previous target, or null on failure. Loader-lock safe (kernel32 only).
void* patch_iat_slot(void** slot, void* hook);
} // namespace dvr::hooks

inline void** FindIatSlotIn(HMODULE mod, const char* dllName, const char* funcName)
{ return ::dvr::hooks::find_iat_slot_in(mod, dllName, funcName); }
inline void** FindIatSlot(const char* dllName, const char* funcName)
{ return ::dvr::hooks::find_iat_slot(dllName, funcName); }
