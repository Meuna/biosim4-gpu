# Native Build Guide

Covers packages `core`, `cfgparse`, `sim-ref`, and `sim-gpu`.

## Prerequisites

### Core build tools

**Linux (apt):**

```sh
sudo apt install cmake ninja-build gcc pkg-config
```

CMake 3.28 or newer is required. Check your version:

```sh
cmake --version
```

If the system package is older than 3.28, install a newer one from
[cmake.org](https://cmake.org/download/) or via `pip install cmake`.

**Windows MSVC:**

Install [Visual Studio](https://visualstudio.microsoft.com/) with the following
workloads:

- "Desktop development with C++"
- "MSVC Build Tools for x64/x86"
- "C++ CMake tools for Windows"
- "vcpkg package manager"
- "MSVC v143 - VS 2022 C++ x64/x86 build tools"

### vcpkg

Both build trees use vcpkg for C/C++ dependencies.

**Linux** — manual setup required:

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg          # add to ~/.bashrc or ~/.profile
```

The `VCPKG_ROOT` environment variable tells CMake where to find the vcpkg
toolchain file. Without it, configure fails immediately with a
missing-toolchain error.

**Windows** — vcpkg is bundled with Visual Studio when the "vcpkg package
manager" workload is selected. CMake picks it up automatically via the Visual
Studio integration; no manual `VCPKG_ROOT` setup is needed. A standalone
install is still possible using the commands above (`bootstrap-vcpkg.bat`)
if you prefer an explicit location.

### Lint and format tools

Required for the `lint` and `format` targets.

**Linux (apt):**

```sh
sudo apt install clang-format clang-tidy
```

**Windows:**

Install the [LLVM toolchain](https://llvm.org/builds/) (includes
`clang-format` and `clang-tidy`). Add the LLVM `bin/` directory to `PATH`.

## Presets

| Preset | Purpose |
|---|---|
| `debug` | Debug info, no optimisation, assertions on |
| `release` | `-O3`, LTO, assertions off |
| `asan` | Debug + AddressSanitizer + UBSan |
| `ci` | Release + tests enabled |

## Configure

```sh
cmake --preset debug
```

Resolves dependencies (vcpkg installs missing packages; `FetchContent` downloads
the rest) and writes build files into `build/debug/`. Also generates
`compile_commands.json` for editor tooling.

## Build

```sh
cmake --build --preset debug
```

## Test

```sh
ctest --preset debug
```

## Format and lint

```sh
cmake --build --preset debug --target format   # clang-format (C/H/CL files)
cmake --build --preset debug --target lint     # clang-tidy
```

## Smoke tests

```sh
# CPU reference simulator (no extra runtime needed)
./build/debug/packages/sim-ref/biosim-ref

# OpenCL GPU simulator (requires an OpenCL runtime — see docs/build-opencl.md)
./build/debug/packages/sim-gpu/biosim-gpu
```

Both should exit cleanly with default parameters.
