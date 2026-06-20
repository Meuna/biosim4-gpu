# CMake 3.21+ required for CMakePresets.json v3 support
cmake_minimum_required(VERSION 3.21)

project(biosim4-gpu
  VERSION 1.1.0
  LANGUAGES C
  DESCRIPTION "GPU port of biosim4 using OpenCL"
)

# 32-bit Windows is not supported: OpenCL ICDs ship x64-only on modern systems
# and 32-bit atomics behave differently. Fail early with a clear message.
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "32-bit Windows builds are not supported. "
        "Please open an x64 Visual Studio Developer Command Prompt "
        "and re-run cmake --preset <name>."
    )
endif()

# ── Language standard ──────────────────────────────────────────────────────────

# Host code targets C11; C17 would also be acceptable (it is a bugfix release)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Disable compiler-specific GNU extensions — enforces strict ISO C portability
set(CMAKE_C_EXTENSIONS OFF)

# ── Global options (user-facing knobs) ────────────────────────────────────────

# Build unit test executables (requires Unity via FetchContent)
option(BIOSIM_BUILD_TESTS      "Build the test suite" ON)

# Build the OpenCL GPU simulator (requires OpenCL headers; runtime requires POCL or another ICD)
option(BIOSIM_BUILD_GPU        "Build the OpenCL GPU simulator (sim-gpu)" ON)

# Enable AddressSanitizer + UBSan (GCC/Clang only; MSVC supports ASan but not UBSan)
option(BIOSIM_SANITIZE         "Enable address and undefined-behaviour sanitizers" OFF)

# Build the Svelte webapp (requires Bun; only meaningful in the EMSCRIPTEN tree)
option(BIOSIM_BUILD_WEBAPP     "Build the Svelte webapp" OFF)

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
# Built in both the native and the webapp trees.
add_subdirectory(packages/core)

if(NOT BIOSIM_BUILD_WEBAPP)
  # CLI/TOML configuration parsing library (depends on core + argtable3 + tomlc17)
  add_subdirectory(packages/cfgparse)
  # Single-threaded CPU reference simulator executable
  add_subdirectory(packages/sim-ref) # depends on core + cfgparse
  # OpenCL GPU simulator (depends on core + cfgparse + OpenCL; optional at configure time)
  if(BIOSIM_BUILD_GPU)
    add_subdirectory(packages/sim-gpu)
  endif()
  # Developer workflow targets for the native tree (format, lint, benchmark)
  include(DevWorkflow)
else()
  if(EMSCRIPTEN)
    # WASM bindings: ES6 module + Web Worker target
    add_subdirectory(packages/sim-wasm)
    # Svelte SPA that loads sim-wasm inside a Web Worker
    add_subdirectory(packages/webapp)
  else()
    message(FATAL_ERROR "webapp preset requires the Emscripten toolchain")
  endif()
  # Developer workflow targets for the webapp tree (format, lint)
  include(WebappDevWorkflow)
endif()
