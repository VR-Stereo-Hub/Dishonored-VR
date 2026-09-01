# Regenerates dvr_version.h so the logged version can never drift from the tree.
# Run via `cmake -P` from a build-time custom target with DVR_VERSION, DVR_SRC,
# DVR_IN and DVR_OUT defined.
#
# Writes through a temp file and copy_if_different so an unchanged git state does
# not force a rebuild of every TU that includes the header.

set(DVR_BUILD_ID "nogit")

find_package(Git QUIET)
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
        WORKING_DIRECTORY "${DVR_SRC}"
        OUTPUT_VARIABLE _describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _rc)
    if(_rc EQUAL 0 AND _describe)
        set(DVR_BUILD_ID "${_describe}")
    endif()
endif()

configure_file("${DVR_IN}" "${DVR_OUT}.tmp" @ONLY)
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DVR_OUT}.tmp" "${DVR_OUT}")
file(REMOVE "${DVR_OUT}.tmp")
