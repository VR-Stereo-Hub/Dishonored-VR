// State chunk 22 for core/gfx/present - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ----------------------------------------------------------------------------
// UE3 introspection probe (Stage 3 groundwork — READ ONLY, no gameplay writes)
//
// Static analysis of Dishonored.exe (no ASLR, base 0x400000) found:
//   GNames.Data @ 0x1435674, GNames.Num @ 0x1435678 (TArray<FNameEntry*>)
//   FNameEntry: +0x08 = Index<<1, +0x0C = HashNext, +0x10 = inline ANSI name
//   Name hash table (65536 buckets) @ 0x13B6048
// This probe verifies GNames at runtime, then scans the module's .data/BSS
// for the GObjObjects array (validated by resolving object names through
// GNames), auto-discovers the UObject Name-field offset, and hex-dumps
// camera-related objects so the next build can drive the camera (stereo/6DoF).
// Runs automatically twice, or on demand with F9 (press it while IN a level).
// ----------------------------------------------------------------------------
static const uintptr_t kGNamesData = 0x1435674;
