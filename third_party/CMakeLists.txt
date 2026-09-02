if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp")
    message(FATAL_ERROR
        "Submodules missing. Run:\n  git submodule update --init --recursive")
endif()

# --- Dear ImGui (MIT), docking branch - no upstream CMakeLists ---------------
# The in-headset settings overlay (F10). Win32 + DX11 backends; gamepad input
# disabled because the game's XInput slots are ours.
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_win32.cpp
    imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC imgui imgui/backends)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)

# --- OpenXR SDK (Apache-2.0) --------------------------------------------------
# The static loader is linked straight into d3d9.dll (41.0, the BioShock
# runtime layer): no openxr_loader.dll ships, and the loader's own discovery
# (XR_RUNTIME_JSON first, then the 32-bit ActiveRuntime key) selects the
# simulator, the SteamVR shim or VDXR. The same lib serves xr_hello32.
add_library(openxr_headers INTERFACE)
target_include_directories(openxr_headers INTERFACE OpenXR-SDK/include)

option(DVR_WITH_OPENXR "Build and link the static OpenXR loader" ON)
if(DVR_WITH_OPENXR)
    set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
    add_subdirectory(OpenXR-SDK)
    # Upstream forces dynamic CRT for static-lib builds; match our /MT.
    set_target_properties(openxr_loader PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# --- OpenVR (BSD-3) - vendored headers + the 32-bit runtime loader DLL -------
# See openvr_headers/PROVENANCE.txt. The mod includes openvr_capi.h and loads
# openvr_api.dll at runtime from the game folder.
add_library(openvr_headers INTERFACE)
target_include_directories(openvr_headers INTERFACE openvr_headers)
