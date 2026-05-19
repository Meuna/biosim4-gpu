# cmake/WebappDevWorkflow.cmake — developer workflow targets for the webapp tree
# (format, lint)
#
# Included from the root CMakeLists.txt after all packages are configured.
# Variables consumed: CMAKE_SOURCE_DIR
#
# clang-tidy for sim-wasm is attached via a separate _lint-sim-wasm target
# (add_dependencies) so it is optional — if clang-tidy is not installed the
# lint target still runs ESLint + Prettier.
#
# Targets produced:   format, lint , _lint-sim-wasm

find_program(BUN_EXECUTABLE          bun          DOC "Path to Bun")
find_program(CLANG_FORMAT_EXECUTABLE clang-format DOC "Path to clang-format")
find_program(CLANG_TIDY_EXECUTABLE   clang-tidy   DOC "Path to clang-tidy")

if(BUN_EXECUTABLE)
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "Running Prettier..."
    COMMAND ${BUN_EXECUTABLE} run --cwd ${CMAKE_SOURCE_DIR}/packages/webapp format
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting webapp source files with Prettier"
    USES_TERMINAL
    VERBATIM
  )

  add_custom_target(lint
    COMMAND ${CMAKE_COMMAND} -E echo "Running ESLint..."
    COMMAND ${BUN_EXECUTABLE} run --cwd ${CMAKE_SOURCE_DIR}/packages/webapp lint
    COMMAND ${CMAKE_COMMAND} -E echo "Checking Prettier formatting..."
    COMMAND ${BUN_EXECUTABLE} run --cwd ${CMAKE_SOURCE_DIR}/packages/webapp format:check
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Linting webapp (ESLint + Prettier)"
    USES_TERMINAL
    VERBATIM
  )
  if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(_format-sim-wasm
      COMMAND ${CMAKE_COMMAND} -E echo "Running clang-format on sim-wasm..."
      COMMAND git ls-files -- "packages/sim-wasm/src/*.c" "packages/sim-wasm/src/*.h"
              | xargs ${CLANG_FORMAT_EXECUTABLE} -i
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "Formatting packages/sim-wasm/src with clang-format"
      USES_TERMINAL
      VERBATIM
    )
    add_dependencies(format _format-sim-wasm)
  endif()
  if(CLANG_TIDY_EXECUTABLE)
    # emcc adds its sysroot headers implicitly; they are not recorded in
    # compile_commands.json.  EMSCRIPTEN_SYSROOT (set by the Emscripten
    # toolchain) points to the built cache sysroot that emscripten.h itself
    # mandates be used instead of the source-tree system/include directory.
    add_custom_target(_lint-sim-wasm
      COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy on sim-wasm..."
      COMMAND git ls-files -- "packages/sim-wasm/src/*.c"
              | xargs ${CLANG_TIDY_EXECUTABLE}
                      --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                      -p ${CMAKE_BINARY_DIR}
                      --extra-arg=-I${EMSCRIPTEN_SYSROOT}/include
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      COMMENT "clang-tidy packages/sim-wasm/src"
      USES_TERMINAL
      VERBATIM
    )
    add_dependencies(lint _lint-sim-wasm)
  endif()
endif()
