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

Two build trees share some prerequisites and each adds its own. Install only
what you need.

> **Linux note:** only `apt` (Ubuntu/Debian) is tested. Commands for other
> distributions will differ.

### Core build tools

Required for any build in this repo.

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

Install [Visual Studio](https://visualstudio.microsoft.com/) with the following workload:
- "Desktop development with C++"
- "MSVC Build Tools for x64/x86"
- "C++ CMake tools for Windows"
- "vcpkg package manager"
- "MSVC v143 - VS 2022 C++ x64/x86 build tools"

### vcpkg for Linux

Both build trees use vcpkg for C/C++ dependencies.

```sh
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh       # Linux/macOS
# ~/vcpkg/bootstrap-vcpkg.bat    # Windows
export VCPKG_ROOT=~/vcpkg          # add to ~/.bashrc or ~/.profile
```

The `VCPKG_ROOT` environment variable tells CMake where to find the vcpkg
toolchain file. Without it, configure fails immediately with a
missing-toolchain error.

### Native build extras

Required for the `lint` and `format` targets in the native tree.

**Linux (apt):**

```sh
sudo apt install clang-format clang-tidy
```

**Windows:**

Install the [LLVM toolchain](https://llvm.org/builds/) (includes
`clang-format` and `clang-tidy`). Add the LLVM `bin/` directory to `PATH`.

### OpenCL runtime

Required to *run* `biosim-gpu` or its tests. The build itself does not need a
runtime — the Khronos OpenCL ICD Loader is supplied by vcpkg and links at build
time. Use `clinfo` to confirm at least one platform is visible:

```sh
sudo apt install clinfo   # diagnostic tool
clinfo
```

#### Linux (apt)

The GPU kernel driver and the userspace OpenCL ICD are separate packages on
Linux.

- **NVIDIA** — the ICD (`nvidia.icd`) ships with the `libnvidia-compute`
  package that comes with the driver. See the
  [CUDA installation guide](https://developer.nvidia.com/cuda-downloads).
- **AMD** — install the [ROCm OpenCL runtime](https://rocm.docs.amd.com/) or,
  for integrated/older cards, Mesa's implementation:
  `sudo apt install mesa-opencl-icd`
- **Intel** — `sudo apt install intel-opencl-icd` (see the
  [Intel Compute Runtime](https://github.com/intel/compute-runtime)).

**CPU fallback (no GPU required):**

[POCL](http://portablecl.org/) provides an OpenCL 3.0 CPU driver:

```sh
sudo apt install pocl-opencl-icd
```

#### Windows

Install the official NVIDIA or AMD GPU driver. The OpenCL ICD is bundled with
the driver — no extra steps are needed.

The GPU tests gracefully `IGNORE` (not fail) when no OpenCL platform is found,
so `ctest` passes in environments without a runtime. To build without the GPU
simulator entirely:

```sh
cmake --preset debug -DBIOSIM_BUILD_GPU=OFF
```

### WebAssembly (Emscripten)

Required only for the webapp build tree.

**Linux:**

```sh
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
~/emsdk/emsdk install latest
~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh      # sets EMSDK; add to ~/.bashrc
```

**Windows:**

Follow the [Emscripten SDK guide](https://emscripten.org/docs/getting_started/downloads.html).
Use the same `emsdk install` / `emsdk activate` steps; run `emsdk_env.bat`
instead of the `source` command.

### Webapp (Bun)

Required only for the webapp build tree.

**Linux:**

```sh
curl -fsSL https://bun.sh/install | bash
# adds ~/.bun/bin to PATH; re-open your shell or source ~/.bashrc
```

**Windows:**

```sh
powershell -c "irm bun.sh/install.ps1 | iex"
```

## Native build tree

Presets: `debug`, `release`, `asan`, `ci`

| Preset | Purpose |
|---|---|
| `debug` | Debug info, no optimisation, assertions on |
| `release` | `-O3`, LTO, assertions off |
| `asan` | Debug + AddressSanitizer + UBSan |
| `ci` | Release + tests enabled |

### Configure

```sh
cmake --preset debug
```

Resolves dependencies (vcpkg installs missing packages; `FetchContent`
downloads the rest) and writes build files into `build/debug/`. Also generates
`compile_commands.json` for editor tooling.

### Build

```sh
cmake --build --preset debug
```

### Test

```sh
ctest --preset debug
```

### Format and lint

```sh
cmake --build --preset debug --target format   # clang-format (C/H/CL files)
cmake --build --preset debug --target lint     # clang-tidy

### Quick smoke tests

```sh
# CPU reference simulator (no extra runtime needed)
./build/debug/packages/sim-ref/biosim-ref

# OpenCL GPU simulator (install pocl-opencl-icd if no physical GPU)
./build/debug/packages/sim-gpu/biosim-gpu
```

Both should exit cleanly with default parameters.

## Webapp build tree

Presets: `wasm`, `webapp`

Packages: `sim-wasm` (Emscripten WASM module) and `webapp` (Svelte SPA).
Uses a separate CMake binary dir (`build/webapp`) and the Emscripten toolchain
— does not share a build directory with the native tree.

### Configure and build

```sh
cmake --preset webapp
cmake --build --preset webapp
```

Output artifacts:
- `build/webapp/packages/sim-wasm/biosim.mjs` — Emscripten ES6 loader
- `build/webapp/packages/sim-wasm/biosim.wasm` — WebAssembly binary
- `packages/webapp/dist/` — bundled Svelte SPA (static files)

### Dev server

```sh
cmake --build --preset webapp --target dev
```

Opens a Vite dev server at `http://localhost:5173`.

### Test

```sh
bun run --cwd packages/webapp test
```

Runs Vitest in non-interactive mode. Watch mode:
`bun run --cwd packages/webapp test:watch`.

### Format and lint

```sh
cmake --build --preset webapp --target format  # Prettier
cmake --build --preset webapp --target lint    # ESLint + Prettier check
```

### Quick smoke test

```sh
python3 -m http.server 8080 --directory build/webapp/packages/webapp/dist
```

Open a browser at `http://localhost:8080`.

## CI

No CI workflows are configured yet.
