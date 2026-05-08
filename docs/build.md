# Build Guide

## How this project is built

This project uses **CMake** as its build system and **vcpkg** for third-party
dependencies. CMake does not compile code directly — it reads the build
description and generates the actual build files for whatever tool you have
(Ninja, Make, Visual Studio, …). vcpkg is a package manager for C/C++
libraries: it downloads, compiles, and makes libraries available to CMake so
you do not need to install them system-wide or copy source into the repo.

A few dependencies have no vcpkg port yet; those are downloaded automatically
at configure time by CMake's built-in `FetchContent` mechanism. You do not need
to do anything extra for them.

## Prerequisites

You need a C compiler, CMake 3.28+, Ninja (or make), pkg-config, and optionally
clang-format and clang-tidy. Install them through your system package manager.

On Ubuntu/Debian:

```sh
sudo apt install cmake ninja-build gcc pkg-config clang-format clang-tidy
```

Check your CMake version with `cmake --version`. If the system package is older
than 3.28, install a newer one from [cmake.org](https://cmake.org/download/) or
via `uv install cmake`.

## Set up vcpkg

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg          # add to ~/.bashrc or ~/.profile
```

vcpkg is distributed as source rather than as a system package, so the
canonical way to install it is a plain `git clone`. The bootstrap script
compiles the `vcpkg` binary into that source tree.

The `VCPKG_ROOT` environment variable tells CMake where to find the vcpkg
toolchain file. CMake reads that file at the start of every configure run and
uses it to locate the packages listed in `vcpkg.json`. Without this variable
set, configure will fail immediately with a missing-toolchain error.

## Configure

```sh
cmake --preset debug
```

The configure step reads all `CMakeLists.txt` files, resolves dependencies
(vcpkg installs any missing packages; `FetchContent` downloads the rest), and
writes the actual build files into `build/debug/`. It also generates
`compile_commands.json` there, which editors and clang-tidy use for
autocompletion and analysis.

Available presets are defined in `CMakePresets.json`:

| Preset | Purpose |
|--------|---------|
| `debug` | Debug info, no optimisation, assertions on |
| `release` | `-O3`, LTO, assertions off |
| `asan` | Debug + AddressSanitizer + UBSan |
| `ci` | Release + tests enabled |

## Build

```sh
cmake --build --preset debug
```

## Run the simulator

```sh
./build/debug/packages/sim-stepper/biosim-stepper
```

See [`docs/usage.md`](usage.md) for the full parameter reference.

## Run tests

```sh
ctest --preset debug
```

## Format and lint

```sh
# Apply clang-format to all source files
cmake --build --preset debug --target format

# Run clang-tidy static analysis
cmake --build --preset debug --target lint
```

Lint is required to be clean before merging. See `CLAUDE.md` for the full
quality sequence.

## CI

No CI workflows are configured yet.
