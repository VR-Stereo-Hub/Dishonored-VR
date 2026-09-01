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
# the DXVK fork (dxvk_d3d9.dll) or System32 d3d9 and does all the VR work.
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
    openvr_headers
    openxr_headers
    user32 gdi32 advapi32 ole32 uuid windowscodecs dwmapi imm32
)
target_compile_options(dvr_proxy PRIVATE /W3 /permissive-)
if(DVR_WITH_LEGACY)
    target_compile_definitions(dvr_proxy PRIVATE DVR_WITH_LEGACY=1)
endif()
