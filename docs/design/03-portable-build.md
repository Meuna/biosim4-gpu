# Portable Build Design Document

**Purpose:** Describe how the build chain is organized so the project
compiles, tests, and runs identically across the supported platforms. Covers
the choice of build system, dependency management, target matrix, developer
workflow, CI, and the rules for keeping platform-specific code contained.

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [Target Platforms and Compilers](#2-target-platforms-and-compilers)
3. [Language Standards](#3-language-standards)
4. [Build System — CMake](#4-build-system--cmake)
5. [Third-Party Dependencies — vcpkg](#5-third-party-dependencies--vcpkg)
6. [Unit Testing Framework](#6-unit-testing-framework)
7. [Developer Workflow](#7-developer-workflow)
8. [Platform-Specific Concerns](#8-platform-specific-concerns)
9. [Continuous Integration](#9-continuous-integration)
10. [Library Portability and Non-Portable Code Quarantine](#10-library-portability-and-non-portable-code-quarantine)

## 1. Goals and Non-Goals

### Goals

- One build procedure that works on Linux, Windows, and eventually ARM64,
  with no per-platform forks or conditional branches beyond what a handful of
  isolated CMake modules can handle.
- One dependency manifest that pins the same library versions on every
  platform.
- A CI pipeline that continuously exercises every supported configuration,
  making drift immediately visible.
- Developer workflow commands (format, lint, build, test, benchmark) invoked
  uniformly on every platform, without separate `.sh` / `.ps1` variants.
- Platform-specific code, when unavoidable, contained in a single location
  with a portable wrapper — never spread across modules.

### Non-Goals

- Package manager alternatives beyond vcpkg. Conan, system package managers,
  and hand-maintained third-party sources are deliberately excluded.
- Shell script wrappers for developer workflows. CMake custom targets replace
  them.
- Installation scripts that modify the user's system. Prerequisites are
  documented, not scripted, so expert users stay in control.
- Multi-language packaging. No Python or JavaScript in the build chain at
  this stage.

## 2. Target Platforms and Compilers

### 2.1 Platform matrix

| Platform | CPU | Status |
|---|---|---|
| Linux | x86-64 | Primary development target (w/o GPU) |
| Windows | x86-64 | First-class target (local testing w/ GPU) |
| Linux | ARM64 | Second-wave target (cloud instances) |
| Windows | ARM64 | Best-effort; CI-enabled when runners become mature |
| macOS | x86-64 / ARM64 | Not targeted initially; Apple deprecated OpenCL |

Adding a platform to the matrix is confined to changes in
`CMakePresets.json`, `vcpkg.json`, and the GitHub Actions workflows. No
directory moves, no package splits.

### 2.2 Compiler matrix

| Compiler | Minimum version | Notes |
|---|---|---|
| GCC | 9 | Full C11, AddressSanitizer, UBSan, TSan |
| Clang | 10 | Same as GCC, plus usable on both Linux and Windows |
| MSVC | 2019 16.8 | Full C11; AddressSanitizer since 16.9; no UBSan / TSan |

Compiler flags are not portable. `cmake/CompilerWarnings.cmake` and
`cmake/Sanitizers.cmake` branch on `CMAKE_C_COMPILER_ID` to emit the right
flags for each compiler and to advertise which sanitizers are actually
available.

## 3. Language Standards

### 3.1 C standard

**C11** is the declared minimum for host code. C17 is identical semantically
(a bugfix release), so it would also be acceptable; C23 is ruled out for now
due to uneven compiler support.

### 3.2 OpenCL C

**OpenCL 1.2** is the required minimum for device code. This is the
broadest-compatibility option. OpenCL 2.0+ features (generic atomics, shared
virtual memory) are used conditionally via `#ifdef __OPENCL_VERSION__` guards
when present, but not required.

### 3.3 Host/device portability

The small subset of `core` that is shared with OpenCL kernels — the POD types
and the RNG — must compile as both C11 host code and OpenCL C (which is based
on C99). The headers concerned are kept in a **C99 portable subset**: no
`<stdio.h>`, `<stdlib.h>`, `<string.h>`, or host-only macros. The rest of
`core` uses C11 freely. This constraint is enforced by convention (prologue
comment in each concerned header) and verified by the RNG equivalence test
in the `core` test suite, which compares host-side xorshift output against
the OpenCL kernel's output bit-for-bit.

## 4. Build System — CMake

### 4.1 Why CMake

CMake is chosen for three concrete reasons:

- It generates native Visual Studio, Xcode, Ninja, and Makefile projects from
  a single source, so every IDE on every platform consumes the same build
  definition.
- `find_package(OpenCL)` works identically on Linux, Windows, and ARM64 — no
  per-platform dependency glue.
- vcpkg integrates natively via `CMAKE_TOOLCHAIN_FILE`. Combined, CMake and
  vcpkg remove the "works on my Linux" trap at the root.

Hand-written Makefiles were considered and rejected: they would lose
cross-platform project generation, require manual dependency location for
OpenCL on Windows, and not integrate with vcpkg's manifest mode.

### 4.2 CMake file layout

- **Top-level `CMakeLists.txt`:** declares the project, C standard, global
  options (build type, sanitizers, embed kernels, build tests, build
  benchmarks), includes modules from `cmake/`, and does
  `add_subdirectory(packages/...)` for each package.
- **Per-package `CMakeLists.txt`:** defines the library or executable target,
  its public/private includes, its link dependencies. Tests live in a nested
  `tests/CMakeLists.txt`.

### 4.3 Reusable modules under `cmake/`

```
cmake/
├── Dependencies.cmake             ← FetchContent for third-party dependencies w/o vcpkg port
├── FindOpenCL.cmake               ← fallback if the system-provided one is insufficient
├── CompilerWarnings.cmake         ← strict warnings, branching on compiler (MSVC vs GCC/Clang)
├── Sanitizers.cmake               ← opt-in sanitizers, advertising availability per compiler
├── EmbedKernels.cmake             ← helper for packaging .cl files
└── Version.cmake                  ← manage injection of git commit data
```

Each module encapsulates one concern. `CompilerWarnings.cmake` branches on
`CMAKE_C_COMPILER_ID` because warning flags are not portable: GCC/Clang
accept `-Wall -Wextra -Wpedantic`, while MSVC accepts `/W4 /permissive-`.
`Sanitizers.cmake` similarly advertises which sanitizers work on the current
compiler — AddressSanitizer is supported by all three, UBSan and TSan by
GCC and Clang only.

### 4.4 CMake presets

Canonical build configurations so contributors (and CI) never need to
remember the right flag combinations:

| Preset | Purpose |
|---|---|
| `debug` | Debug info, no optimization, asserts on |
| `release` | O3, LTO, asserts off, kernels embedded |
| `asan` | Debug info + AddressSanitizer + UBSan |
| `ci` | Release + tests enabled |

### 4.5 Modern CMake — target-first, no globals

The following rules apply to every `CMakeLists.txt` in the repository:

- Every property (includes, defines, compile flags, features) is attached
  to a target via `target_*` commands.
- `include_directories()`, `add_definitions()`, and `link_directories()` at
  the top level are **banned**.
- Dependencies between targets are expressed with `target_link_libraries`
  using `PUBLIC` / `PRIVATE` / `INTERFACE` qualifiers; transitive propagation
  is the mechanism, not variable passing.

These rules eliminate the pathologies that make old CMake codebases
unreadable: global state, hidden dependencies, accidental leakage of private
includes into public APIs.

## 5. Third-Party Dependencies

### 5.1 Strategy

Three mechanisms are used depending on the library:

- **vcpkg manifest mode** for libraries that have a vcpkg port. vcpkg reads
  `vcpkg.json` at configure time and provides dependencies through standard
  `find_package` calls. The build system is agnostic of how dependencies are
  resolved.
- **`third_party/` vendoring** for small libraries (one or two files) with no
  upstream CMake support. The source files are copied into `third_party/<name>/`
  alongside a minimal handwritten `CMakeLists.txt`. Sources and licences are
  committed to the repository. The subdirectory is included via
  `cmake/Dependencies.cmake`.
- **CMake `FetchContent`** for libraries that are not in the vcpkg registry and
  are too large to vendor. Downloaded at configure time into `build/`; nothing
  committed. Declarations live in `cmake/Dependencies.cmake`.

### 5.2 vcpkg manifest

```json
{
  "name": "biosim4-gpu",
  "version-string": "0.1.0",
  "dependencies": [
    "opencl",
    "argtable3"
  ],
  "builtin-baseline": "<pinned commit hash of the vcpkg registry>"
}
```

`opencl` pulls both `opencl-headers` and `opencl-icd-loader`, which on
Windows removes the most common build failure (missing ICD loader). A CMake
preset activates vcpkg by pointing `CMAKE_TOOLCHAIN_FILE` at
`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`.

Note: some vcpkg portfiles call `vcpkg_fixup_pkgconfig` internally to generate
`.pc` files for pkg-config consumers. This requires `pkg-config` to be installed
as a system tool (it is listed in the build prerequisites in `docs/build.md`).
This is a vcpkg toolchain requirement, not a requirement of our own CMake files.

### 5.3 Vendored dependencies (`third_party/`)

Small libraries with no upstream CMake support are copied into `third_party/`:

Example with tomlc17:

```
third_party/
└── tomlc17/
    ├── CMakeLists.txt   ← handwritten; defines the tomlc17 static library target
    ├── tomlc17.c        ← vendored from https://github.com/cktan/tomlc17 tag R260414
    └── tomlc17.h
```

The subdirectory is included from `cmake/Dependencies.cmake` before the
packages, so the `tomlc17` target is available to any package that links it.

### 5.4 FetchContent

`FetchContent` declarations are centralized in `cmake/Dependencies.cmake`:

```cmake
include(FetchContent)

if(BIOSIM_BUILD_TESTS)
  FetchContent_Declare(unity
      GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
      GIT_TAG        v2.6.1
  )
  FetchContent_MakeAvailable(unity)
  target_compile_definitions(unity PUBLIC UNITY_INCLUDE_DOUBLE)
endif()
```

## 6. Unit Testing Framework

Candidates considered:

| Framework | Pros | Cons |
|---|---|---|
| Unity | Minimal, single-header, trivial CMake integration | Spartan features |
| cmocka | Supports mocks, more assertion forms | Heavier dependency |
| Criterion | Parameterized tests, nice output | Linux-focused |
| Check | Classic, mature | Verbose, fork-based runner |

**Choice: Unity.** Single-header, zero heavy dependencies, trivial CMake
integration, runs identically on every target platform — which is exactly
what the portability requirements demand. Unity is pulled via vcpkg.

## 7. Developer Workflow

### 7.1 CMake custom targets

Common developer workflows are exposed as **CMake custom targets** on the
top-level build:

| Custom target | Effect |
|---|---|
| `format` | Runs `clang-format` across every tracked source file |
| `lint` | Runs `clang-tidy` across every tracked source file |
| `check` | Builds everything, runs all unit tests |
| `benchmark` | Builds and runs the benchmarks in `benchmarks/` |

Invoked uniformly on every platform:

```sh
cmake --build --preset debug --target format
cmake --build --preset debug --target check
```

### 7.2 Environment prerequisites — documented, not scripted

Installing platform-level prerequisites (an OpenCL SDK, a C compiler, Git,
CMake, vcpkg) is **documented in `docs/build.md`** as explicit command-line
invocations for each supported platform. No bootstrap script is provided.

Rationale: side-effects that modify the user's system (package manager
invocations, environment variable changes) should be a conscious choice of
the user, not hidden behind a script. Expert users in particular want full
visibility into what is being installed and how.

For repeatable CI environments, the GitHub Actions workflows in
`.github/workflows/` play the role of an executable reference for what a
clean setup looks like on each OS.

## 8. Platform-Specific Concerns

Each concern below represents a real difference between the supported
platforms. The rule is always the same: contain the difference in one place,
expose a portable interface to the rest of the code.

### 8.1 Path handling

The runtime kernel loader must resolve the path to the `.cl` files at
startup. It does so by querying the executable's own location —
`GetModuleFileNameW` on Windows, `/proc/self/exe` on Linux. The
platform-specific code is isolated to one function inside the kernel registry
module; everything else works in terms of the resolved absolute path.
CMake's `configure_file` injects the install-relative path constant.

### 8.2 Line endings

`.gitattributes` declares `* text=auto` plus explicit `*.sh text eol=lf` (in
case any shell script is ever added) and `*.cl text eol=lf` for kernel
sources. Text files are normalized to LF in the repository and checked out
in the platform's native form.

### 8.3 Endianness

x86-64 and ARM64 are both little-endian in practice under Linux and Windows.
The binary snapshot format specifies little-endian explicitly, which keeps
it valid on the entire target matrix. Any future big-endian platform (not
planned) would need an explicit byte-swap pass in the snapshot module.

### 8.4 OpenCL installation

Per-platform copy-pastable command lines live in `docs/build.md`:

- **Linux:** system package manager plus an ICD (PoCL for CPU-only testing,
  vendor driver for GPU).
- **Windows:** vcpkg (recommended) or vendor SDK. The vcpkg manifest handles
  headers and ICD loader automatically once the toolchain file is set.

### 8.5 ARM64 cloud instances — typically no discrete GPU

Cloud ARM instances (AWS Graviton, Ampere Altra, Oracle Ampere A1) are pure
CPU. Install **PoCL** (an OpenCL CPU runtime that fully supports ARM64) and
pass `--device cpu` when invoking the simulator.

## 9. Continuous Integration

### 9.1 CI matrix

GitHub Actions workflows exercise the portability claims continuously:

| Workflow job | OS runner | Compiler | OpenCL runtime |
|---|---|---|---|
| `linux-x64-gcc` | `ubuntu-latest` | GCC | PoCL (CPU) |
| `linux-x64-clang` | `ubuntu-latest` | Clang | PoCL (CPU) |
| `windows-x64-msvc` | `windows-latest` | MSVC | vcpkg-provided ICD |
| `linux-arm64-gcc` | `ubuntu-24.04-arm` | GCC | PoCL (CPU) |

### 9.2 What every job verifies

Every CI job runs three things:

1. A full build of all packages.
2. The unit test suite, including the RNG equivalence test from `core`.
3. The **cross-simulator equivalence test** described in
   [`01-repository-structure.md`](01-repository-structure.md)
   — this is the ultimate portability check: if the GPU kernel under PoCL
   (invoked with `--device cpu`) and the stepper on the host CPU produce
   matching outputs across all four configurations, the port is behaviorally
   consistent on the entire matrix.

## 10. Library Portability and Non-Portable Code Quarantine

### 10.1 Cross-platform by construction

Every third-party dependency chosen above is inherently cross-platform:

- **OpenCL** — Khronos standard; same API on NVIDIA, AMD, Intel, Mali, and
  PoCL. The vcpkg `opencl` package pulls `opencl-headers` and
  `opencl-icd-loader` so Windows doesn't need a vendor SDK.
- **Unity** (test framework) — pure C, single-header; runs anywhere.
- **TOML parser** (e.g. `tomlc99`) — pure C; runs anywhere.
- **CMake**, **vcpkg**, **clang-format**, **clang-tidy** — all officially
  support Linux, Windows, and ARM64.

### 10.2 Quarantining non-portable code

Platform-specific code, when unavoidable, follows two rules:

1. **It is wrapped behind a small function in a single `.c` file**, never
   spread across modules.
2. **The surrounding code is written against the wrapper's portable
   signature**, not the underlying platform API.

The only expected platform-specific code under this rule is the
executable-path resolution mentioned in Section 8.1. If a second location
ever needs it, it moves to a dedicated `core/platform.h` with a clean
portable API.
