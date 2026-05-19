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

### OpenCL runtime (for `sim-gpu`)

The OpenCL GPU simulator (`sim-gpu`) links against the Khronos OpenCL ICD Loader
supplied by vcpkg. To actually *run* the simulator or its tests you also need an
OpenCL runtime registered with the ICD loader.

The ICD loader finds runtimes via `.icd` files registered under
`/etc/OpenCL/vendors/` (Linux) or the Windows registry. Use `clinfo` to confirm
at least one platform is visible after installation:

```sh
sudo apt install clinfo   # diagnostic tool
clinfo
```

#### GPU on Windows

Install the official NVIDIA or AMD GPU driver. The OpenCL ICD is bundled with the
driver; no extra steps are needed.

#### GPU on Linux

On Linux the GPU kernel driver is a separate package from the userspace OpenCL
ICD. Whether the ICD is included in the driver package or must be installed
separately depends on the vendor:

- **NVIDIA** — the ICD (`nvidia.icd`) is typically included in the
  `libnvidia-compute` package that ships with the driver. See the
  [CUDA installation guide](https://developer.nvidia.com/cuda-downloads) for your
  distribution.
- **AMD** — install the [ROCm OpenCL runtime](https://rocm.docs.amd.com/) or, for
  integrated/older cards, Mesa's OpenCL implementation
  (`mesa-opencl-icd` on Debian/Ubuntu).
- **Intel** — see the
  [Intel Compute Runtime](https://github.com/intel/compute-runtime) for integrated
  and Arc graphics (`intel-opencl-icd` on Debian/Ubuntu).

If `clinfo` lists platforms but the simulator fails with permission errors, add
yourself to the `render` group and re-login:

```sh
sudo usermod -aG render $USER
```

#### CPU fallback on Linux (POCL)

[POCL](http://portablecl.org/) provides an OpenCL 3.0 CPU driver; no GPU is
required:

```sh
sudo apt install pocl-opencl-icd
```

The GPU tests gracefully `IGNORE` (not fail) when no OpenCL platform is found, so
`ctest` passes in environments without a runtime.

To build without the GPU simulator entirely:

```sh
cmake --preset debug -DBIOSIM_BUILD_GPU=OFF
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

| Preset | Tree | Purpose |
|---|---|---|
| `debug` | native | Debug info, no optimisation, assertions on |
| `release` | native | `-O3`, LTO, assertions off |
| `asan` | native | Debug + AddressSanitizer + UBSan |
| `ci` | native | Release + tests enabled |
| `webapp` | webapp | Emscripten + Bun/Vite/Svelte SPA |

## Build

```sh
cmake --build --preset debug
```

## Run the simulators

```sh
# Single-threaded CPU reference simulator
./build/debug/packages/sim-ref/biosim-ref

# OpenCL GPU simulator (requires an OpenCL runtime — see prerequisites)
./build/debug/packages/sim-gpu/biosim-gpu
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

# Apply Prettier to webapp TypeScript/Svelte files
cmake --build --preset debug --target webapp-format

# Run ESLint + Prettier check on webapp
cmake --build --preset debug --target webapp-lint
```

Lint is required to be clean before merging. See `CLAUDE.md` for the full
quality sequence.

## Webapp tests

```sh
bun run --cwd packages/webapp test
```

Runs Vitest in non-interactive mode. Individual watch mode:
`bun run --cwd packages/webapp test:watch`.

## WebAssembly + Webapp build

The webapp tree uses a separate CMake binary dir (`build/webapp`) and the
Emscripten toolchain. It does not share a build directory with the native tree.

### Prerequisites

**Emscripten (EMSDK):**

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh      # sets EMSDK; add to ~/.bashrc
```

**Bun:**

```sh
curl -fsSL https://bun.sh/install | bash
# adds ~/.bun/bin to PATH; re-open your shell or source ~/.bashrc
```

### Configure and build

```sh
cmake --preset webapp
cmake --build --preset webapp
```

Output artifacts:
- `build/webapp/packages/sim-wasm/biosim.mjs` — Emscripten ES6 loader
- `build/webapp/packages/sim-wasm/biosim.wasm` — WebAssembly binary
- `packages/webapp/dist/` — bundled Svelte SPA (served as static files)

### Dev server

```sh
cmake --build --preset webapp --target dev
```

Opens a Vite dev server at `http://localhost:5173`. Check the browser
JavaScript console for:

```
biosim wasm: hello from C
biosim wasm: structured log
[main] worker ready
```

The `dev` target is `EXCLUDE_FROM_ALL` — it does not run during
`cmake --build --preset webapp`.

## CI

No CI workflows are configured yet.
