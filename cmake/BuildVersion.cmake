# BuildVersion.cmake — provides biosim_target_git_version(target)
#
# Injects three compile definitions into target at configure time:
#   BIOSIM_GIT_VERSION      — output of: git describe --tags --always --dirty
#   BIOSIM_BUILD_TIMESTAMP  — UTC ISO-8601 timestamp of the cmake run
#   BIOSIM_BUILD_TYPE       — cmake build configuration (Debug, Release, …)
#
# Note: values are captured when cmake runs, not on each incremental build.
# Re-run cmake to refresh the dirty flag after staging or unstaging changes.

find_package(Git QUIET)

function(biosim_target_git_version target)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --tags --always --dirty
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE GIT_RESULT
        )
        if(NOT GIT_RESULT EQUAL 0)
            set(GIT_VERSION "unknown")
        endif()
    else()
        set(GIT_VERSION "unknown")
    endif()

    string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

    target_compile_definitions(${target} PRIVATE
        BIOSIM_GIT_VERSION="${GIT_VERSION}"
        BIOSIM_BUILD_TIMESTAMP="${BUILD_TIMESTAMP}"
        BIOSIM_BUILD_TYPE="$<CONFIG>"
    )
endfunction()
