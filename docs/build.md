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

> **Linux note:** only `apt` (Ubuntu/Debian) is tested. Commands for other
> distributions will differ.

| Prerequisite | Applies to | Notes |
|---|---|---|
| cmake 3.28+, ninja, gcc, pkg-config | All builds | See [`docs/build-native.md`](build-native.md) for install commands |
| vcpkg | All builds | Set `VCPKG_ROOT` env var after installing |
| clang-format, clang-tidy | Native `lint`/`format` targets | See [`docs/build-native.md`](build-native.md) |
| OpenCL runtime | Running `biosim-gpu` / its tests | See [`docs/build-opencl.md`](build-opencl.md) |
| Emscripten SDK (`emsdk`) | Webapp tree | See [`docs/build-webapp.md`](build-webapp.md) |
| Bun | Webapp tree | See [`docs/build-webapp.md`](build-webapp.md) |

## Build trees

### Native tree

Packages: `core`, `cfgparse`, `sim-ref`, `sim-gpu`. CMake presets:
`debug`, `release`, `asan`, `ci`.

- Build commands, presets, and smoke tests → [`docs/build-native.md`](build-native.md)
- OpenCL runtime installation → [`docs/build-opencl.md`](build-opencl.md)

### Webapp tree

Packages: `sim-wasm` (Emscripten WASM module) and `webapp` (Svelte SPA).
Uses a separate CMake binary dir (`build/webapp`) and the Emscripten
toolchain — does not share a build directory with the native tree.

- Emscripten SDK and Bun installation; `cmake --preset webapp` build → [`docs/build-wasm.md`](build-wasm.md)
- Dev server, Vitest, Prettier, ESLint, smoke test → [`docs/build-webapp.md`](build-webapp.md)

## CI

Two GitHub Actions workflows run the quality check sequence automatically on
every pull request and on direct pushes to `main`. Each workflow is path-filtered
so it only triggers when its relevant files change.

| Workflow | File | Triggered by changes to |
|---|---|---|
| Native CI | `.github/workflows/native.yml` | `packages/core/**`, `packages/cfgparse/**`, `packages/sim-ref/**`, `packages/sim-gpu/**`, root CMake files, `.clang-*` |
| Webapp CI | `.github/workflows/webapp.yml` | `packages/sim-wasm/**`, `packages/webapp/**`, `cmake/WebappDevWorkflow.cmake`, root CMake files, `.clang-*` |

Both workflows run the same sequence: **configure → build → test → lint →
format check**. The format check applies the formatter in place and then runs
`git diff --exit-code` to fail if any file was not already formatted.

### Native CI environment

Installed via apt: `cmake`, `ninja-build`, `clang`, `clang-format`,
`clang-tidy`, `pocl-opencl-icd` (CPU OpenCL runtime required by the GPU tests).
vcpkg is cloned from GitHub and bootstrapped on the runner. `CC=clang` is set so
`compile_commands.json` records clang invocations — keeping the build compiler
consistent with clang-tidy's frontend. Uses the `ci` preset (Release + tests
enabled). vcpkg packages are cached by `vcpkg.json` hash.

### Webapp CI environment

Installed via apt: `cmake`, `ninja-build`, `clang-format`, `clang-tidy`. Bun
1.3.14 via `oven-sh/setup-bun` (cached automatically). Emscripten SDK via
`mymindstorm/setup-emsdk` (cached in `.emsdk-cache/`). vcpkg cloned and
bootstrapped for the `wasm32-emscripten` triplet (cached separately from the
native triplet). Uses the `webapp` preset.
