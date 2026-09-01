// State chunk 59 for core/vr/openxr_loader - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


#define XRB_FN(inst, name) \
    PFN_##name name = NULL; \
    g_xrGipa(inst, #name, (PFN_xrVoidFunction*)&name)


// ============================================================================
// 37.3 / XR-2: THE PERSISTENT OPENXR BACKEND (real rendering)
//
// XR-1 proved the bring-up chain 4x (3x xrsim in-process, 1x real Quest 3 on
// VDXR: 2112x2304/eye, 30 frames). This is the same chain made persistent
// and wired into the live pipeline: the session lives on g_dev11 (the eye-
// render device), XrRtFrameBegin replaces WaitGetPoses as the pose source,
// and delivery is swapchain copy + xrEndFrame instead of Compositor Submit.
// Everything between - capture, eye quads, hands, wrist HUD, overlay - is
// untouched and arrives in the headset for free.
// ============================================================================
static XrInstance  g_xriInst  = XR_NULL_HANDLE;
