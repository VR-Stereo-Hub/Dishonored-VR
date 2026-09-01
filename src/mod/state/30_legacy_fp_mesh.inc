// State chunk 30 for legacy/fp_mesh - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.



// ============================================================================
// CAMERA MATRIX HOOK (BioShock-style native view injection)
//
// The tracer found the camera-to-world matrix builder at 0x56d7e0; it writes
// the rotation rows (+0x50/+0x60/+0x70) and position (+0x80) into the camera
// object, with ecx = camera 'this'. We splice a 5-byte detour at its clean
// epilogue (0x56dd36 = pop esi;mov esp,ebp;pop ebp;ret) so that AFTER the game
// builds the matrix we can overwrite the rotation — making the ENGINE render
// from our chosen orientation. Milestone: a fixed 20 deg test yaw (F3) to
// confirm the render actually follows this matrix. Default OFF / not installed.
// ============================================================================
static const uintptr_t kCamHookAt = 0x56dd36; // epilogue (5 bytes: 5E 8B E5 5D C3)
