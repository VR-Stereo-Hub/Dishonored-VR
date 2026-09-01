// State chunk 00 for mod/prelude - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.

// ============================================================================
// Dishonored VR
// d3d9.dll proxy, plus a forked dxvk_d3d9.dll that does the stereo work.
//
//   SCENE MODE (default): we are a real SteamVR app. Each frame is captured
//   from the game's D3D9 backbuffer, uploaded to D3D11, drawn onto a virtual
//   surface with correct per-eye asymmetric projection, and submitted to the
//   VR compositor. HMD yaw/pitch drives the game camera 1:1.
//
//   STEREO IS REAL, NOT MONO. The old header here still said "depth is still
//   mono - true stereo is Stage 3", which has been false since the DXVK fork
//   landed. Qualifying draws are replayed once per eye into half-width
//   viewports with a per-eye shifted view-projection (dxvk_d3d9.dll, gated on
//   dxvk_stereo.txt), so the scene has genuine binocular depth. Comments that
//   describe a stage we left behind are worse than no comment - this one was
//   read as current, which is exactly the harm.
//
//   THEATER MODE (fallback / ForceTheater=1 in ini): flat overlay screen, no
//   head-driven camera.
//
// Config: dishonored_vr.ini next to the DLL (auto-created with defaults).
// Kill switch: create disable_vr.txt.
// Keys: F4 positional tracking, F5 re-centre, F8 blink-marker draw isolation,
//       F9 force gameplay mode (unstick head tracking), Pause per-eye
//       screen-UV fix on/off, Scroll Lock frame dump.
// Blink aims by redirecting the engine's own aim vector at its source
// (0xbf55a3), so the engine traces, validates and draws along your controller.
// Diagnostics: dishonored_vr.log — send it back after every test.
// ============================================================================

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <wincodec.h>
#include <objbase.h>
#include <d3dcommon.h>
#include <tlhelp32.h>
#include <xinput.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

// 30.38: in-game settings overlay (Dear ImGui, pattern from bioshock-vr).
// Rendered into both eye render targets so it is readable in the headset;
// software cursor drawn by ImGui (the OS cursor is invisible in VR).
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#include <setjmp.h>

// 37.0 / XR-1: OpenXR headers, prototypes OFF - we load the runtime
// ourselves through the loader-negotiation interface (no openxr_loader.dll
// to ship, no api-layer machinery). See the XR BENCH section.
#define XR_NO_PROTOTYPES
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_loader_negotiation.h>

// openvr_capi.h (C API). MSVC defines only _WIN32, so no MinGW __WIN32 fixup is needed.
#include "openvr_capi.h"

// ----------------------------------------------------------------------------
// Globals: proxy plumbing
// ----------------------------------------------------------------------------
static HMODULE g_realD3D9 = NULL;
