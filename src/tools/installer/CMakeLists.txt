# --- DishonoredVR-Launcher.exe - the player's installer (VR-198) ---------------------
# One 32-bit exe with the mod embedded: d3d9.dll, the SteamVR shim, Valve's
# openvr_api.dll, the tested dishonored_vr.ini and the support collector, as
# RCDATA resources. It draws with Dear ImGui in the F10 panel's own theme
# (core/ui/ovl_ui.cpp) so the launcher looks like the game it installs.
#
# The payload is staged PER CONFIG. `cmake --build --preset release` is
# RelWithDebInfo, and $<TARGET_FILE:dvr_proxy> then resolves to that d3d9.dll;
# a Debug build stages Debug DLLs into its own folder. With one shared folder a
# Debug build after a Release build would leave Debug DLLs there, newer than the
# Release inputs, and the next Release installer would ship them (the VR-160
# lesson: a build that plays is an optimised build).
#
# payload.rc names files only. rc.exe finds them through its include path, and
# CMake forwards target_include_directories to the resource compiler, so the
# per-config folder below selects the right bytes with one config-independent
# .rc. The .rc is itself copied (plain copy, not copy_if_different) by the same
# command that stages the DLLs, so its timestamp moves whenever any payload
# input changes and rc.exe recompiles under every generator (the Visual Studio
# generator does not honour OBJECT_DEPENDS for resources).
enable_language(RC)

set(DVR_SETUP_PAYLOAD_DIR "${CMAKE_BINARY_DIR}/installer-payload/$<CONFIG>")
set(DVR_SETUP_RC "${CMAKE_BINARY_DIR}/installer-payload/payload.rc")
set(DVR_OPENVR_DLL "${PROJECT_SOURCE_DIR}/third_party/openvr_headers/bin/win32/openvr_api.dll")

# Valve's loader ships inside the launcher: refuse to embed a binary whose hash
# PROVENANCE.txt does not record (the same gate install.ps1 and package.ps1 apply).
file(SHA256 "${DVR_OPENVR_DLL}" _dvr_openvr_sha)
file(READ "${PROJECT_SOURCE_DIR}/third_party/openvr_headers/PROVENANCE.txt" _dvr_prov)
string(FIND "${_dvr_prov}" "${_dvr_openvr_sha}" _dvr_prov_hit)
if(_dvr_prov_hit EQUAL -1)
    message(FATAL_ERROR "openvr_api.dll sha256 ${_dvr_openvr_sha} is not in PROVENANCE.txt - refusing to embed it in the launcher")
endif()

set(DVR_SETUP_STATIC_PAYLOAD
    "${DVR_OPENVR_DLL}|openvr_api.dll"
    "${PROJECT_SOURCE_DIR}/release/dishonored_vr.ini|dishonored_vr.ini"
    "${PROJECT_SOURCE_DIR}/tools/collect-support.ps1|collect-support.ps1"
    "${PROJECT_SOURCE_DIR}/release/HOW-TO-USE.txt|HOW-TO-USE.txt"
    "${PROJECT_SOURCE_DIR}/docs/TROUBLESHOOTING.md|TROUBLESHOOTING.md"
    "${PROJECT_SOURCE_DIR}/docs/KNOWN_ISSUES.md|KNOWN_ISSUES.md"
    "${PROJECT_SOURCE_DIR}/docs/RELEASE_NOTES.md|RELEASE_NOTES.md"
)
set(_stage_cmds "")
set(_stage_outs "")
set(_stage_deps "")
foreach(_entry IN LISTS DVR_SETUP_STATIC_PAYLOAD)
    string(REPLACE "|" ";" _pair "${_entry}")
    list(GET _pair 0 _src)
    list(GET _pair 1 _name)
    list(APPEND _stage_cmds COMMAND ${CMAKE_COMMAND} -E copy "${_src}" "${DVR_SETUP_PAYLOAD_DIR}/${_name}")
    list(APPEND _stage_outs "${DVR_SETUP_PAYLOAD_DIR}/${_name}")
    list(APPEND _stage_deps "${_src}")
endforeach()

add_custom_command(
    OUTPUT "${DVR_SETUP_PAYLOAD_DIR}/d3d9.dll"
           "${DVR_SETUP_PAYLOAD_DIR}/dvr_steamvr32.dll"
           ${_stage_outs}
           "${DVR_SETUP_RC}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DVR_SETUP_PAYLOAD_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:dvr_proxy>" "${DVR_SETUP_PAYLOAD_DIR}/d3d9.dll"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:dvr_ovr_shim>" "${DVR_SETUP_PAYLOAD_DIR}/dvr_steamvr32.dll"
    ${_stage_cmds}
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/payload.rc" "${DVR_SETUP_RC}"
    DEPENDS dvr_proxy dvr_ovr_shim "${CMAKE_CURRENT_SOURCE_DIR}/payload.rc" ${_stage_deps}
    COMMENT "Staging the launcher payload ($<CONFIG>)"
    VERBATIM)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.rc.in" "${CMAKE_BINARY_DIR}/installer-payload/version.rc" @ONLY)

set(DVR_SETUP_JSON "${PROJECT_SOURCE_DIR}/third_party/OpenXR-SDK/src/external/jsoncpp")
add_executable(dvr_setup WIN32
    sys/updates.cpp sys/updates.h sys/discovery.cpp sys/discovery.h
    "${DVR_SETUP_JSON}/src/lib_json/json_reader.cpp"
    "${DVR_SETUP_JSON}/src/lib_json/json_value.cpp"
    "${DVR_SETUP_JSON}/src/lib_json/json_writer.cpp"
    main.cpp
    sys/support.cpp         sys/support.h
    app/win32_app.cpp        app/win32_app.h
    app/offscreen.cpp        app/offscreen.h
    ui/screens.cpp           ui/screens.h
    ui/widgets.cpp           ui/widgets.h
    model/choices.cpp        model/choices.h
    model/view_state.h
    model/fake_states.cpp    model/fake_states.h
    model/installer.cpp      model/installer.h
    sys/fs.cpp               sys/fs.h
    sys/game_ini.cpp         sys/game_ini.h
    sys/profile.cpp          sys/profile.h
    sys/steam.cpp            sys/steam.h
    sys/process.cpp          sys/process.h
    sys/resources.cpp        sys/resources.h
    sys/install_record.cpp   sys/install_record.h
    sys/gpu.cpp              sys/gpu.h
    payload_ids.h
    dvr_setup.manifest
    "${DVR_SETUP_RC}"
    "${CMAKE_BINARY_DIR}/installer-payload/version.rc"
    "${PROJECT_SOURCE_DIR}/src/core/ui/ovl_ui.cpp"
    "${PROJECT_SOURCE_DIR}/src/core/ui/ovl_assets.rc"
    "${PROJECT_SOURCE_DIR}/src/core/util/log.cpp"
)
set_target_properties(dvr_setup PROPERTIES
    OUTPUT_NAME "DishonoredVR-Launcher-v${PROJECT_VERSION}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
add_dependencies(dvr_setup dvr_version)
target_include_directories(dvr_setup PRIVATE
    "${DVR_SETUP_JSON}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated"
    "${DVR_SETUP_PAYLOAD_DIR}")
# d3dcompiler_47.dll is pragma-imported by imgui_impl_dx11. The installer must
# be able to start on a machine that lacks it and say so, hence delay-load.
target_link_libraries(dvr_setup PRIVATE
    imgui
    user32 gdi32 d3d11 dxgi shell32 ole32 oleaut32 uuid comctl32 advapi32 bcrypt dwmapi shcore delayimp winhttp version)
target_link_options(dvr_setup PRIVATE /DELAYLOAD:d3dcompiler_47.dll)
target_compile_options(dvr_setup PRIVATE /W3 /permissive- /EHsc)
target_compile_definitions(dvr_setup PRIVATE DVR_BUILD_CONFIG="$<CONFIG>" UNICODE _UNICODE)
