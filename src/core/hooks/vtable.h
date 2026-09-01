// core/hooks/vtable.h - COM vtable slot patching (the D3D9 device and
// interface hooks: CreateDevice, Present, Reset, SetVertexShaderConstantF...).
#pragma once

namespace dvr::hooks {
// Replaces slot `index` of comObject's vtable with newFn. Returns the previous
// entry, or null when the slot already held newFn or the page could not be
// unprotected.
void* patch_vtable(void* comObject, int index, void* newFn);
} // namespace dvr::hooks

inline void* PatchVtable(void* comObject, int index, void* newFn)
{ return ::dvr::hooks::patch_vtable(comObject, index, newFn); }
