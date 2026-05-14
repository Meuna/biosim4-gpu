# CMake 3.21+ required for CMakePresets.json v3 support
cmake_minimum_required(VERSION 3.21)

project(biosim4-gpu
  VERSION 0.1.0
  LANGUAGES C
  DESCRIPTION "GPU port of biosim4 using OpenCL"
)

# ── Language standard ──────────────────────────────────────────────────────────

# Host code targets C11; C17 would also be acceptable (it is a bugfix release)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Disable compiler-specific GNU extensions — enforces strict ISO C portability
set(CMAKE_C_EXTENSIONS OFF)

# ── vcpkg integration ──────────────────────────────────────────────────────────

# vcpkg toolchain file is injected by CMakePresets.json via CMAKE_TOOLCHAIN_FILE;
# VCPKG_ROOT must be set in the environment before running cmake --preset <name>

# ── Global options (user-facing knobs) ────────────────────────────────────────

# Build unit test executables (requires Unity via FetchContent)
option(BIOSIM_BUILD_TESTS      "Build the test suite" ON)

# Build standalone benchmark executables under benchmarks/
option(BIOSIM_BUILD_BENCHMARKS "Build benchmark executables" OFF)

# Build the OpenCL GPU simulator (requires OpenCL headers; runtime requires POCL or another ICD)
option(BIOSIM_BUILD_GPU        "Build the OpenCL GPU simulator (sim-gpu)" ON)

# Enable AddressSanitizer + UBSan (GCC/Clang only; MSVC supports ASan but not UBSan)
option(BIOSIM_SANITIZE         "Enable address and undefined-behaviour sanitizers" OFF)

# ── Testing ────────────────────────────────────────────────────────────────────
enable_testing()

# ── CMake modules ──────────────────────────────────────────────────────────────

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Per-compiler warning flags; branches on CMAKE_C_COMPILER_ID (GCC/Clang/MSVC)
# include(CompilerWarnings)  # uncomment once cmake/CompilerWarnings.cmake exists

# Opt-in sanitizers; advertises which are available on the current compiler
# include(Sanitizers)        # uncomment once cmake/Sanitizers.cmake exists

# External dependencies without a vcpkg port (FetchContent)
include(Dependencies)

# Build metadata (git version, timestamp, build type) injected via compile definitions
include(BuildVersion)

# Kernel embedding: biosim_embed_opencl_source() generates a C translation unit
# containing a .cl or .h file as a NUL-terminated string literal.
include(EmbedKernels)

# ── Packages ──────────────────────────────────────────────────────────────────

# Shared simulation logic: genome, neural net, RNG, challenges (no deps)
add_subdirectory(packages/core)

# CLI/TOML configuration parsing library (depends on core + argtable3 + tomlc17)
add_subdirectory(packages/cfgparse)

# OpenCL GPU simulator (depends on core + cfgparse + OpenCL; optional at configure time)
if(BIOSIM_BUILD_GPU)
  add_subdirectory(packages/sim-gpu)
endif()

# Single-threaded CPU reference simulator executable
add_subdirectory(packages/sim-ref) # depends on core + cfgparse

# WebAssembly PoC: hardcoded sim, no params, generates standalone HTML
add_subdirectory(packages/wasm-sim-stepper)

# ── Benchmarks ────────────────────────────────────────────────────────────────

if(BIOSIM_BUILD_BENCHMARKS)
  # add_subdirectory(benchmarks)         # uncomment once the directory is scaffolded
endif()

# ── Developer workflow custom targets ─────────────────────────────────────────

# format: reformat every tracked C/H/CL file in place
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "Running clang-format..."
    COMMAND git ls-files -- "*.c" "*.h" "*.cl" ":(exclude)third_party/**"
            | xargs ${CLANG_FORMAT_EXECUTABLE} -i
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Formatting source files with clang-format"
    VERBATIM
  )
endif()

# lint: run clang-tidy static analysis across all tracked C sources
find_program(CLANG_TIDY_EXECUTABLE clang-tidy)
if(CLANG_TIDY_EXECUTABLE)
  add_custom_target(lint
    COMMAND ${CMAKE_COMMAND} -E echo "Running clang-tidy..."
    COMMAND git ls-files -- "*.c" ":(exclude)third_party/**"
            | xargs ${CLANG_TIDY_EXECUTABLE}
                    --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                    -p ${CMAKE_BINARY_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy static analysis"
    VERBATIM
  )
endif()

# benchmark: build and run all benchmarks; results go to stdout (no pass/fail)
if(BIOSIM_BUILD_BENCHMARKS)
  add_custom_target(benchmark
    COMMAND ${CMAKE_COMMAND} -E echo "Running benchmarks..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running benchmarks"
    VERBATIM
  )
endif()
