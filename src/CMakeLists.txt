# --- generated version header --------------------------------------------------
# Regenerated before every build of the proxy, so `dishonoredvr X.Y.Z (build id)`
# in dishonored_vr.log always matches the tree it was built from.
set(DVR_VERSION_HEADER "${CMAKE_BINARY_DIR}/generated/dvr_version.h")
execute_process(COMMAND ${CMAKE_COMMAND}
        -DDVR_VERSION=${PROJECT_VERSION}
        -DDVR_SRC=${PROJECT_SOURCE_DIR}
        -DDVR_IN=${PROJECT_SOURCE_DIR}/cmake/dvr_version.h.in
        -DDVR_OUT=${DVR_VERSION_HEADER}
        -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake)
add_custom_target(dvr_version ALL
    BYPRODUCTS "${DVR_VERSION_HEADER}"
    COMMAND ${CMAKE_COMMAND}
            -DDVR_VERSION=${PROJECT_VERSION}
            -DDVR_SRC=${PROJECT_SOURCE_DIR}
            -DDVR_IN=${PROJECT_SOURCE_DIR}/cmake/dvr_version.h.in
            -DDVR_OUT=${DVR_VERSION_HEADER}
            -P ${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake
    COMMENT "Stamping dvr_version.h (${PROJECT_VERSION} + git describe)"
    VERBATIM)

# --- d3d9.dll - the mod (proxy + VR core + Dishonored integration) ------------
# Dropped next to Dishonored.exe; the game's d3d9 import loads it. It chains to
# System32 d3d9 and does all the VR work.
# Phase 1 of the refactor: one UNITY translation unit (mod/dishonoredvr.cpp)
# that includes the module files in src/proxy, src/core, src/game and
# src/legacy. tools/split-source.py produced the tree from the original single
# file with every function body verbatim. Modules leave the unity list as they
# get their own headers.
file(GLOB_RECURSE DVR_MODULE_SOURCES CONFIGURE_DEPENDS
    proxy/*.cpp core/*.cpp game/*.cpp legacy/*.cpp mod/state/*.inc mod/fwd.h core/*.h game/*.h)
# Modules that already have their own header and translation unit.
set(DVR_REAL_SOURCES
    core/util/log.cpp        core/util/log.h
    core/util/crash.cpp      core/util/crash.h
    core/util/clock.cpp      core/util/clock.h
    core/util/mem.cpp        core/util/mem.h
    core/util/ini.cpp        core/util/ini.h
    core/util/paths.cpp      core/util/paths.h
    core/util/diag.h
    core/hooks/vtable.cpp    core/hooks/vtable.h
    core/hooks/iat.cpp       core/hooks/iat.h
    core/hooks/detour.cpp    core/hooks/detour.h
    core/framework/command.cpp core/framework/command.h
    core/framework/status.cpp  core/framework/status.h
    core/framework/frame_hooks.cpp core/framework/frame_hooks.h
    core/framework/perf.cpp    core/framework/perf.h
    core/gfx/stereo.cpp        core/gfx/stereo.h
    core/gfx/capture.cpp       core/gfx/capture.h
    core/gfx/frame_id.cpp      core/gfx/frame_id.h

    core/gfx/device_census.cpp core/gfx/device_census.h
    core/gfx/draw_census.cpp   core/gfx/draw_census.h
    core/gfx/hud_capture.cpp   core/gfx/hud_capture.h
    core/gfx/d3d9ex.cpp        core/gfx/d3d9ex.h
    core/gfx/blit_quad.cpp     core/gfx/blit_quad.h
    core/gfx/mono_screen.cpp
    core/gfx/aer.cpp
    core/gfx/reentry.cpp
    game/dishonored/camera.cpp game/dishonored/camera.h
    core/vr/openxr_runtime.cpp core/vr/openxr_runtime.h
    core/vr/openxr_input.cpp   core/vr/openxr_input.h
    core/vr/hud_stub.cpp       core/vr/hud_stub.h
    core/util/xr_math.h
)
foreach(_f ${DVR_REAL_SOURCES})
    list(REMOVE_ITEM DVR_MODULE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
endforeach()
add_library(dvr_proxy SHARED
    mod/dishonoredvr.cpp
    proxy/d3d9.def
    ${DVR_REAL_SOURCES}
    ${DVR_MODULE_SOURCES}
)
# The unity-included module files are not compiled on their own.
set_source_files_properties(${DVR_MODULE_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)
set_target_properties(dvr_proxy PROPERTIES OUTPUT_NAME "d3d9" PREFIX "")
add_dependencies(dvr_proxy dvr_version)
target_include_directories(dvr_proxy PRIVATE . "${CMAKE_BINARY_DIR}/generated")
# d3d11.dll and d3dcompiler_47.dll are LoadLibrary'd at runtime on purpose: the
# proxy must add no static import that changes the game's DLL load order.
target_link_libraries(dvr_proxy PRIVATE
    imgui
    openxr_headers
    user32 gdi32 advapi32 ole32 uuid windowscodecs dwmapi imm32
)
target_compile_options(dvr_proxy PRIVATE /W3 /permissive-)
if(DVR_WITH_OPENXR)
    # Static loader linked straight into the proxy: no extra DLL ships and the
    # game's import table is untouched (the loader is not a DLL import).
    target_link_libraries(dvr_proxy PRIVATE openxr_loader)
    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_OPENXR=1)
else()
    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_OPENXR=0)
endif()
if(DVR_WITH_LEGACY)
    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=1)
else()
    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=0)
endif()

# --- xr_hello32 - standalone OpenXR smoke client -------------------------------
# A 32-bit console app with NO mod code in it. tools\xrsim-selftest.ps1 runs it
# against the simulated runtime: if it fails, the simulator is broken, not the
# mod. Not shipped.
# --- dvr_steamvr32: the SteamVR OpenXR shim (BioShock's ovrshim, adopted 41.0) ---
# SteamVR ships no 32-bit OpenXR runtime, so a 32-bit game reaches it through
# this shim: an OpenXR runtime DLL that talks OpenVR to SteamVR. The proxy's
# init_instance falls back to it (a manifest written under %LOCALAPPDATA%)
# when no native 32-bit runtime exists or [VR] Runtime=steamvr. Ships beside
# d3d9.dll with Valve's openvr_api.dll (hash-gated by install/package).
option(DVR_WITH_OVRSHIM "Build the SteamVR OpenXR shim runtime (shipped)" ON)
if(DVR_WITH_OPENXR AND DVR_WITH_OVRSHIM)
    add_library(dvr_ovr_shim SHARED
        tools/ovrshim/ovrshim.h
        tools/ovrshim/ovrshim_negotiate.cpp
        tools/ovrshim/ovrshim_main.cpp
        tools/ovrshim/ovrshim_render.cpp
        tools/ovrshim/ovrshim_input.cpp
        tools/ovrshim/ovrshim.def
    )
    set_target_properties(dvr_ovr_shim PROPERTIES PREFIX "" OUTPUT_NAME "dvr_steamvr32")
    target_include_directories(dvr_ovr_shim PRIVATE tools/ovrshim)
    target_link_libraries(dvr_ovr_shim PRIVATE openxr_headers openvr_headers d3d11)
    # openvr_api.dll and d3dcompiler are LoadLibrary'd at runtime - only the
    # D3D11 the compositing pipeline needs is linked.
endif()

if(DVR_WITH_OPENXR)
    add_executable(xr_hello32 tools/xr_hello32/main.cpp)
    target_link_libraries(xr_hello32 PRIVATE openxr_loader d3d11 dxgi)
endif()

# --- dvr_xrsim32 - the simulated OpenXR runtime --------------------------------
# A Quest-3-shaped 32-bit OpenXR runtime so an agent can drive head, hands and
# controllers and capture per-eye compositor output with no headset. Selected
# per process with XR_RUNTIME_JSON (tools\xrsim-launch.ps1). NOT part of the
# mod and NOT in the release zip. It links NOTHING from the mod and must not
# link openxr_loader either - it IS the runtime the loader loads.
if(DVR_WITH_XRSIM)
    add_library(dvr_xrsim32 SHARED
        tools/xrsim/xrsim_common.h
        tools/xrsim/xrsim_internal.h
        tools/xrsim/xrsim_math.h
        tools/xrsim/xrsim_log.cpp
        tools/xrsim/xrsim_instance.cpp
        tools/xrsim/xrsim_session.cpp
        tools/xrsim/xrsim_frame.cpp
        tools/xrsim/xrsim_actions.cpp
        tools/xrsim/xrsim_control.cpp
        tools/xrsim/xrsim_compositor.cpp
        tools/xrsim/xrsim.def
    )
    set_target_properties(dvr_xrsim32 PROPERTIES PREFIX "" OUTPUT_NAME "dvr_xrsim32")
    target_include_directories(dvr_xrsim32 PRIVATE tools/xrsim)
    target_link_libraries(dvr_xrsim32 PRIVATE openxr_headers d3d11 dxgi d3dcompiler windowscodecs)
    # SEH shims around every entry point need unwind semantics off; the shims
    # deliberately hold no unwindable objects.
    target_compile_options(dvr_xrsim32 PRIVATE /EHa)
endif()
