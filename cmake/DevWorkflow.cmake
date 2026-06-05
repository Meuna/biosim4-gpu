# cmake/DevWorkflow.cmake — developer workflow targets for the native tree
# (format, lint, benchmark)
#
# Included from the root CMakeLists.txt after all packages are configured.
# Variables consumed: BIOSIM_BUILD_BENCHMARKS, CMAKE_SOURCE_DIR, CMAKE_BINARY_DIR.
#
# Targets produced:   format, lint, benchmark

find_program(CLANG_FORMAT_EXECUTABLE clang-format DOC "Path to clang-format")
find_program(CLANG_TIDY_EXECUTABLE   clang-tidy   DOC "Path to clang-tidy")

if(CLANG_FORMAT_EXECUTABLE)
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "Running clang-format..."
    COMMAND git ls-files -- "*.c" "*.h" "*.cl" ":(exclude)third_party/**"
            | xargs ${CLANG_FORMAT_EXECUTABLE} -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting source files with clang-format"
    USES_TERMINAL
    VERBATIM
  )
endif()

if(CLANG_TIDY_EXECUTABLE)
  add_custom_target(lint
    COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy..."
    COMMAND git ls-files -- "*.c" ":(exclude)third_party/**" ":(exclude)packages/sim-wasm/**"
            | xargs ${CLANG_TIDY_EXECUTABLE}
                    --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                    -p ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy static analysis"
    USES_TERMINAL
    VERBATIM
  )
endif()

if(BIOSIM_BUILD_BENCHMARKS)
  add_custom_target(benchmark
    COMMAND ${CMAKE_COMMAND} -E echo "Running benchmarks..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running benchmarks"
    USES_TERMINAL
    VERBATIM
  )
endif()
