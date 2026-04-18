# Repository Structure Design Document

**Status:** Initial proposal for the new GPU port project.
**Companion documents:**
- `biosim4-data-model-design.md` (current CPU/AoS model — reference).
- `biosim4-gpu-data-model-design.md` (Step 1 GPU/SoA proposal).

**Purpose:** Define the initial layout of the Git repository for the BioSim4
GPU port. Establishes the monorepo boundaries, the shared `core` library, the
placement of OpenCL kernels, and the documentation organization. Meant to be
read before filling in any actual source or build files.

**Scope:** Directory tree, package boundaries, documentation structure,
build-system scaffolding, tooling.

**Out of scope:** Concrete `CMakeLists.txt` content, source code, test code,
kernel source. These are produced in follow-up steps once this structure is
agreed upon.

## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [Guiding Principles](#2-guiding-principles)
3. [Top-Level Tree](#3-top-level-tree)
4. [Package Layout — `packages/`](#4-package-layout--packages)
5. [The `core` Package — Rationale and Boundaries](#5-the-core-package--rationale-and-boundaries)
6. [The `sim-gpu` Package](#6-the-sim-gpu-package)
7. [The `sim-stepper` Package](#7-the-sim-stepper-package)
8. [The `viz` Package — Placeholder](#8-the-viz-package--placeholder)
9. [Documentation Organization](#9-documentation-organization)
10. [Build System Scaffolding](#10-build-system-scaffolding)
11. [Tooling — CMake Custom Targets, `tools/`, `benchmarks/`](#11-tooling--cmake-custom-targets-tools-benchmarks)
12. [Data — Configurations and Snapshots](#12-data--configurations-and-snapshots)
13. [Third-Party Dependencies](#13-third-party-dependencies)
14. [Open Design Decisions](#14-open-design-decisions)
15. [Portability](#15-portability)
16. [Implementation Conventions](#16-implementation-conventions)

## 1. Goals and Non-Goals

### Goals

- Provide a repository skeleton that can accommodate the two simulators (GPU
  batch + step-by-step CPU) plus a future visualization front-end without
  restructuring later.
- Enforce a clear separation between *shared simulation logic* and
  *execution-specific* code (OpenCL orchestration vs. single-threaded stepping).
- Keep the build system modular: each package has its own `CMakeLists.txt` and
  can be built and tested in isolation.
- Make the OpenCL kernels first-class citizens of the repository, with a path
  for both runtime-loaded and embedded-at-compile-time distribution.
- Make the design documents part of the repository, versioned alongside the
  code they describe.

### Non-Goals

- Multi-language packaging (no Python, JavaScript, or other languages at this
  stage beyond auxiliary build scripts).
- A plugin architecture for custom challenges, sensors, or actions. Features
  are added by editing the `core` package directly.
- Support for a package manager other than what CMake's `FetchContent` /
  `find_package` mechanism provides.

## 2. Guiding Principles

Three principles drove every choice below. They are restated here because each
later section refers back to them.

### 2.1 Monorepo with explicit package boundaries

All code lives in a single repository, but each buildable unit (library or
executable) lives under its own directory in `packages/`. This avoids the
single-flat-source-directory trap where the GPU simulator's OpenCL integration
gradually leaks into code that should stay pure and portable.

A package is a directory containing:
- Its own `CMakeLists.txt`.
- A public header tree under `include/biosim/<package-name>/`.
- A private source tree under `src/`.
- A test tree under `tests/` with its own `CMakeLists.txt`.

Dependencies between packages are explicit: a package links against another
package's library target, it does not reach into its source tree.

### 2.2 A shared `core` library

The GPU simulator and the step-by-step simulator must produce biologically
equivalent behavior (modulo the documented non-determinism of contested moves).
If the simulation model — genome representation, mutation operators, neural
network culling, challenge evaluation, RNG — is duplicated across the two
simulators, the two will drift apart.

Consequence: every piece of logic that is *not* about "how the simulation is
scheduled" belongs in `core`. The two simulator packages are thin shells
around `core`, differing only in their execution model.

### 2.3 OpenCL kernels are runtime resources, not compilation artifacts

OpenCL `.cl` files are compiled by the OpenCL driver at program startup, not
by the host C compiler. They live in a `kernels/` subdirectory of the
`sim-gpu` package and are shipped alongside the binary. A CMake install rule
copies them next to the executable. Optionally, a CMake helper
(`EmbedKernels.cmake`) can generate a C translation unit that embeds the kernel
sources as string literals for a single-file distribution — controlled by a
CMake option (`BIOSIM_EMBED_KERNELS`).

## 3. Top-Level Tree

```
biosim4-gpu/
├── README.md                          ← project overview + pointers to docs
├── LICENSE
├── CHANGELOG.md
├── .gitignore
├── .gitattributes
├── .editorconfig
├── .clang-format
├── .clang-tidy
├── CMakeLists.txt                     ← top-level orchestration (add_subdirectory)
├── CMakePresets.json                  ← debug/release/asan/ocl-cpu/ocl-gpu presets
├── vcpkg.json                         ← dependency manifest (OpenCL, Unity, TOML)
│
├── .github/
│   └── workflows/
│       ├── build.yml                  ← CI: Linux x64, Windows x64 (ARM64 later)
│       └── tests.yml                  ← CI: unit tests across the same matrix
│
├── docs/                              ← see Section 9
├── cmake/                             ← reusable CMake modules, see Section 10
├── packages/                          ← see Section 4
├── data/                              ← see Section 12
├── third_party/                       ← see Section 13
├── benchmarks/                        ← performance measurements, see Section 11
└── tools/                             ← auxiliary CLI utilities, see Section 11
```

### Top-level file conventions

| File | Purpose |
|---|---|
| `README.md` | Elevator pitch, quickstart, pointer to `docs/` |
| `LICENSE` | License of choice (to be picked) |
| `CHANGELOG.md` | Keep-a-Changelog format, per-release entries |
| `.gitignore` | Standard C + CMake ignores plus `data/snapshots/*` |
| `.gitattributes` | Normalize line endings, mark `.cl` as C-like for diffs |
| `.editorconfig` | Indent, trailing whitespace, final newline for all editors |
| `.clang-format` | Formatting rules, invoked via the `format` CMake custom target |
| `.clang-tidy` | Static analysis rules, invoked via the `lint` CMake custom target |
| `CMakeLists.txt` | Project declaration, global options, `add_subdirectory` for each package |
| `CMakePresets.json` | Build configurations for IDEs and CI |
| `vcpkg.json` | Declarative list of C/C++ dependencies — resolved by vcpkg on all platforms |

## 4. Package Layout — `packages/`

```
packages/
├── core/                              ← shared simulation logic (static library)
├── sim-gpu/                           ← OpenCL batch simulator (executable)
├── sim-stepper/                       ← step-by-step CPU simulator (executable)
└── viz/                               ← placeholder for future visualization
```

### Package dependency graph

```
                   ┌──────────┐
                   │   core   │    (static library, no external deps beyond libc)
                   └─────┬────┘
                         │
              ┌──────────┴──────────┐
              │                     │
        ┌─────▼─────┐         ┌─────▼───────┐
        │  sim-gpu  │         │ sim-stepper │   (executables)
        └───────────┘         └─────────────┘
                                    │
                              (step output)
                                    │
                              ┌─────▼────┐
                              │   viz    │    (future — consumes step data)
                              └──────────┘
```

`core` depends on nothing other than the C standard library.
`sim-gpu` depends on `core` and on OpenCL.
`sim-stepper` depends on `core` only.
`viz` eventually depends on `core` (for types) and a rendering library.

This graph is **acyclic by construction**. Any attempt to introduce a reverse
dependency (for example, `core` calling into `sim-gpu`) is an architectural
regression and should be rejected at review time.

## 5. The `core` Package — Rationale and Boundaries

```
packages/core/
├── CMakeLists.txt
├── include/
│   └── biosim/core/     ← public headers (the API other packages consume)
├── src/                 ← private sources and private headers
└── tests/
    └── CMakeLists.txt
```

The detailed breakdown below describes the **module roles** the package must
cover. The mapping to actual `.h` / `.c` files is an implementation decision,
taken when the code is written with concrete size and cohesion in mind.

### Module responsibilities

| Role | Description |
|---|---|
| Shared POD types | `Coord`, `Dir`, `Gene`, and any other packed value types shared with OpenCL kernels |
| Simulation parameters | The `SimParams` struct plus its TOML loader |
| Genome operators | Mutation (point, insertion, deletion), crossover, fingerprint computation |
| Neural network compilation | Culling of dead neurons, connection reordering, per-agent NN build |
| Abstract grid API | Query, neighborhood iteration, cell write — backing store provided by each simulator |
| Abstract signal API | Read, increment, fade — backing store provided by each simulator |
| Sensor and action catalogues | Enums, constants, dispatch tables common to both simulators |
| Challenge evaluation | The logic that decides survival and updates `challenge_bits` |
| Portable RNG | xorshift64, with an implementation usable verbatim on both host and device |
| Snapshot format | Serialization and deserialization of a full population to/from the versioned binary format |

### What belongs in `core` (the test)

A function belongs in `core` if and only if **both** simulators need exactly
the same behavior from it. Genome mutation is the canonical example: the GPU
simulator uses it at the generation boundary on the host; the step-by-step
simulator uses it identically. If the two diverge, the two simulators are no
longer comparable.

### What does *not* belong in `core`

- OpenCL setup, kernel compilation, buffer management → `sim-gpu`.
- The per-simStep orchestration loop (kernel pipeline vs. single-threaded
  stepping) → each simulator's own code.
- Visualization formats, image encoders → `viz`.
- CLI argument parsing → each executable's `main.c`.

### The host/device portability constraint

The modules that are shared with OpenCL kernels — the POD types and the RNG —
must compile under both the host C compiler and the OpenCL compiler. The
headers that carry them are kept in a **C99 portable subset** with no
host-specific includes (no `<stdio.h>`, no `<stdlib.h>`). The rest of `core`
may use C11 features freely. This constraint is important enough to call out
in the header prologue of any file concerned.

### The abstract grid and signal APIs

The grid and signal modules declare the operations each simulator needs
(query a cell, iterate a neighborhood, emit a signal) without committing to a
backing store. The GPU simulator backs them with `__global` OpenCL buffers;
the stepper backs them with flat host arrays. Shared logic in `core` — for
example challenge evaluation — calls only the abstract API.

### Testability

Every module in `core` is testable in isolation because it has no execution
dependencies. The test suite is the main quality gate for cross-simulator
consistency: a regression in mutation that breaks the stepper will also break
the GPU simulator, and will be caught here before it reaches either. A key
test worth mentioning by name is the **RNG equivalence test**: it verifies
that the host-side xorshift produces bit-identical output to the OpenCL
kernel's xorshift for the same seed and stream, which is a prerequisite for
any GPU-vs-stepper equivalence check.

## 6. The `sim-gpu` Package

```
packages/sim-gpu/
├── CMakeLists.txt
├── include/
│   └── biosim/gpu/      ← public headers (for tests and tools)
├── src/                 ← host-side OpenCL orchestration
├── kernels/             ← OpenCL sources (compiled at runtime, see Section 6.x)
└── tests/
    └── CMakeLists.txt
```

### Module responsibilities

| Role | Description |
|---|---|
| OpenCL context | Device selection, platform queries, `cl_context` and `cl_command_queue` lifetime |
| SoA buffers | Allocation, resizing, host/device transfer of the per-agent SoA buffers described in the GPU data-model document |
| Kernel registry | Loading of `.cl` sources (file mode or embedded mode), build-time compilation, kernel argument binding |
| Per-step pipeline | The orchestration of kernels K1, K2 ... Kn for one simStep |
| CLI entry point | Argument parsing, generation loop, snapshot read/write |

### Kernel file naming — a deliberate exception

Unlike the `src/` tree, the `kernels/` directory **does** use a prescribed
file-per-kernel naming scheme:

```
common.cl         ← shared types, helpers, and the portable RNG
feedforward.cl    ← K1 (sensors + feedForward + actions)
movement.cl       ← K2 (atomic CAS-based movement resolution)
grid_cleanup.cl   ← K3 (post-death grid cell clearing)
signal_fade.cl    ← K4 (signal decay pass)
challenges.cl     ← K5 (per-agent challenge evaluation)
```

Rationale: the five-kernel decomposition is a design artifact of the GPU
data-model document (Section 13 of [02-gpu-data-model.md](02-gpu-data-model.md)), not
an implementation detail. Each `.cl` file is a named chapter of that design.
Renaming or merging kernels is a design change that deserves a new ADR, not
a casual refactor. The file-per-kernel layout locks in this correspondence.

### Kernel distribution: file vs. embedded

Two modes, selected by a CMake option:

- **File mode (default for development):** the `.cl` files are copied by a
  CMake install rule to `<install_prefix>/share/biosim/kernels/`, and the
  binary finds them at runtime relative to its own path. Advantage: edit a
  kernel, re-run the binary, no recompile of host code.
- **Embedded mode (default for release):** a CMake helper generates a C
  translation unit with the kernel sources as string literals. Advantage:
  single self-contained binary, no deployment of auxiliary files.

Both modes go through the same kernel registry API, so the rest of the
codebase is unaware of which mode is active.

### Cross-simulator equivalence test

The single most important test in the entire repository lives in this
package's `tests/`. It runs one simulation step with the GPU simulator and
one with the stepper on the same input, then verifies that the per-agent
neuron outputs match within a tolerance. Without this test, behavioral drift
between the two simulators becomes invisible.

## 7. The `sim-stepper` Package

```
packages/sim-stepper/
├── CMakeLists.txt
├── include/
│   └── biosim/stepper/  ← public headers (for tests, viz, tools)
├── src/                 ← single-threaded CPU implementation
└── tests/
    └── CMakeLists.txt
```

### Module responsibilities

| Role | Description |
|---|---|
| Step engine | `step()` + `inspect()` API; runs one simulation step on host arrays |
| Trace recorder | Emits a versioned per-step trace for consumption by `viz` |
| CLI entry point | Loads a snapshot, runs N steps or accept stdin/socket controls, dumps state or trace |

### Purpose

The stepper is the **reference implementation** for per-step behavior. It
runs purely on the CPU, single-threaded, with no OpenMP, no queues, and no
parallelism at all. Its role is:

1. To serve as the ground truth for cross-simulator tests.
2. To be the execution engine for visualization — the user loads a population
   from a GPU snapshot and replays a generation step by step.

### Why single-threaded and not the original OpenMP model

The original CPU BioSim4 uses OpenMP with move/death queues. That model is
*also* a behavioral approximation (queue drain order is thread-schedule
dependent). For the stepper we want a single, deterministic reference
ordering. We pick: "process agents in increasing index order, apply all
effects immediately." Conflicts are resolved first-come-first-served by
increasing index. This is deterministic, trivial to reason about, and
sufficient for visualization purposes.

If the single-thread stepper happens to be too slow, this single-thread design
can be reverted.

### Trace format

The stepper writes a tracefile per run: a per-step record of every agent's
position, orientation, and a compact neural-state snapshot. The format is
versioned and documented in `docs/design/` as a small ADR. The visualization
package consumes this format.

## 8. The `viz` Package — Placeholder

```
packages/viz/
├── README.md                  ← intent + trace format pointer
└── .gitkeep
```

The visualization is not implemented yet. The package exists so that:
- The monorepo structure does not need to change when it is added.
- The trace format produced by the stepper has a named consumer from day one,
  preventing it from being designed in isolation.

Multiple visualization packages are planned: tty, web

## 9. Documentation Organization

```
docs/
├── README.md                      ← index of the doc, reading order
├── design/
│   ├── 01-repository-structure.md ← this document
│   ├── 02-gpu-data-model.md       ← the GPU/SoA proposal
│   └── adr/                       ← Architecture Decision Records
│       └── README.md
├── build.md                       ← prerequisites, CMake options, build guide
├── usage.md                       ← CLI reference for both simulators
├── architecture.md                ← monorepo + package explanation
└── gpu-primer.md                  ← glossary of GPU concepts
```

### The numbered `design/` files

Design documents are prefixed with a two-digit number that defines reading
order. New design documents append to the sequence. They are long-form,
narrative documents that explain **why** a design choice was made.

### ADRs — Architecture Decision Records

Under `design/adr/`. Each ADR is a short document (1–3 pages) capturing a
single decision: context, options considered, choice, consequences.
ADRs are born as Step 2 refinements crystallize — e.g. *"ADR-001: fingerprint
hash function for GENETIC_SIM_FWD"*, *"ADR-002: sort strategy for warp
coherence"*. Without ADRs, the rationale for a decision ends up buried in
commit messages or lost.

### `gpu-primer.md` — the shared glossary

A single file defines every GPU-specific term used in the rest of the
documentation: *warp*, *wavefront*, *coalescing*, *warp divergence*, *warp
coherence*, *pointer chasing*, *AoS/SoA*, *atomic contention*, *local memory*,
*image object*, *texture cache*. Every other design document links to
anchors in this file (for example, `gpu-primer.md#warp-divergence`) rather
than redefining the term. Section 2 of the GPU design document is the
starting content for this file.

### `build.md` and `usage.md`

- `build.md`: how to install prerequisites (OpenCL SDK, test framework), how
  to select a preset, how to build on Linux/macOS/Windows, how to enable
  sanitizers.
- `usage.md`: command-line reference for each simulator, file formats, example
  workflows ("run 100 generations on the GPU, snapshot generation 42, replay
  it in the stepper").

### `architecture.md`

A shorter, higher-level view than this design document: the packaging model,
the dependency graph, the data-flow between simulators and viz, intended as a
five-minute onboarding read for someone new to the project.

## 10. Build System Scaffolding

### CMake file layout

- **Top-level `CMakeLists.txt`:** declares the project, C standard, global
  options (build type, sanitizers, embed kernels, build tests, build
  benchmarks), includes modules from `cmake/`, and does
  `add_subdirectory(packages/...)` for each package.
- **Per-package `CMakeLists.txt`:** defines the library or executable target,
  its public/private includes, its link dependencies. Tests live in a nested
  `tests/CMakeLists.txt`.

### Reusable modules under `cmake/`

```
cmake/
├── FindOpenCL.cmake               ← fallback if the system-provided one is insufficient
├── CompilerWarnings.cmake         ← strict warnings, branching on compiler (MSVC vs GCC/Clang)
├── Sanitizers.cmake               ← opt-in sanitizers, advertising availability per compiler
└── EmbedKernels.cmake             ← helper for packaging .cl files
```

Isolating these in modules keeps the main `CMakeLists.txt` files readable and
lets each concern be revised independently.

**Cross-compiler note.** `CompilerWarnings.cmake` must branch on
`CMAKE_C_COMPILER_ID` because warning flags are not portable: GCC and Clang
accept `-Wall -Wextra -Wpedantic`, while MSVC accepts `/W4 /permissive-`.
`Sanitizers.cmake` similarly advertises which sanitizers are available on the
current compiler — AddressSanitizer is supported by all three, UBSan and TSan
by GCC and Clang only.

**vcpkg integration.** When the project is configured with
`-DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake`,
vcpkg reads `vcpkg.json` at the repository root and provides the declared
dependencies via standard `find_package`. No code in `cmake/` needs to know
about vcpkg specifically — it is plugged in transparently at configure time.
The `CMakePresets.json` exposes a preset that wires the toolchain file
automatically when the `VCPKG_ROOT` environment variable is set.

### `CMakePresets.json`

Canonical build configurations so contributors (and CI) never need to
remember the right flag combinations:

| Preset | Purpose |
|---|---|
| `debug` | Debug info, no optimization, asserts on |
| `release` | O3, LTO, asserts off |
| `asan` | Debug info + AddressSanitizer + UBSan |
| `ocl-cpu` | Release, forces the OpenCL CPU runtime (PoCL, Intel OpenCL) |
| `ocl-gpu` | Release, forces a discrete GPU runtime |

The `ocl-cpu` preset is important: it lets the GPU simulator be developed and
tested on a machine with no discrete GPU, which accelerates iteration.

## 11. Tooling — CMake Custom Targets, `tools/`, `benchmarks/`

### Developer workflow via CMake custom targets

No `scripts/` directory is provided. Common developer workflows are instead
exposed as **CMake custom targets** on the top-level build:

| Custom target | Effect |
|---|---|
| `format` | Runs `clang-format` across every tracked source file |
| `lint` | Runs `clang-tidy` across every tracked source file |
| `check` | Builds everything, runs all unit tests |
| `benchmark` | Builds and runs the benchmarks in `benchmarks/` |

Invoked uniformly on every platform:

```
cmake --build build --target format
cmake --build build --target check
```

This removes the need for separate `.sh` and `.ps1` script variants and keeps
the tool invocation inside the build system where the paths and compiler
settings are already known.

### Environment prerequisites — documented, not scripted

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

### `tools/` — auxiliary CLI utilities

```
tools/
├── snapshot-inspect/              ← read a snapshot, print the population
├── genome-dump/                   ← decode a genome into human-readable form
└── README.md
```

Each tool is its own tiny package with the same `CMakeLists.txt` + `src/`
structure. They depend on `core`. They are not simulators; they are
diagnostic instruments for debugging snapshots and genomes.

### `benchmarks/` — performance measurements

```
benchmarks/
├── CMakeLists.txt
└── README.md
```

Each benchmark is a small standalone executable that exercises one part of
the system (e.g. feedForward throughput, movement resolution contention,
signal fade). They live here rather than in `tests/` because their success
criterion is different: tests pass/fail, benchmarks produce numbers that
are compared to a baseline. Keeping them in a separate directory avoids
polluting the test output and makes them easy to disable in CI.

## 12. Data — Configurations and Snapshots

```
data/
├── configs/                       ← versioned simulation parameter files
│   ├── default.toml
│   ├── challenge-corner.toml
│   └── README.md
└── snapshots/                     ← population snapshots (not versioned)
    ├── .gitkeep
    └── .gitignore                 ← ignores large binary files
```

### Configurations are versioned

Running a reproducible experiment requires a stable, committed parameter
file. `data/configs/` holds named configurations (one per challenge scenario,
typically), and every configuration file is under version control.

Format choice is deferred (see Section 14) — likely TOML or INI. The
`core/params.h` loader handles whatever is chosen.

### Snapshots are not versioned

Population snapshots are binary files, potentially megabytes each, produced
on every run. They belong in the working tree but not in the commit history.
The nested `.gitignore` excludes everything except `.gitkeep`.

## 13. Third-Party Dependencies

```
third_party/
└── README.md                      ← documents the dependency strategy
```

### Strategy — vcpkg manifest at the repository root

The project uses **vcpkg** in manifest mode as its primary dependency
mechanism. A `vcpkg.json` file at the repository root declares every
third-party library the project needs. vcpkg reads this manifest at configure
time and provides the dependencies through standard CMake `find_package`
calls — no code needs to know whether a dependency came from vcpkg or from
the system.

Rationale:
- **Cross-platform uniformity.** The same manifest resolves dependencies on
  Linux, Windows, and (later) macOS and ARM64. This matters most on Windows,
  where there is no system package manager that covers the C/C++ ecosystem.
- **Version pinning.** The manifest pins versions via a `builtin-baseline`
  field, so every contributor and every CI run gets the same library
  versions.
- **Zero commits to `third_party/`.** Source code of dependencies never
  enters the Git history.

Typical `vcpkg.json` content:

```json
{
  "name": "biosim4-gpu",
  "version-string": "0.1.0",
  "dependencies": [
    "opencl",
    "unity",
    "tomlc99"
  ],
  "builtin-baseline": "<pinned commit hash of the vcpkg registry>"
}
```

### When `third_party/` is used

The in-tree `third_party/` directory is reserved for dependencies that vcpkg
cannot or should not handle: typically single-header libraries where the
round-trip through vcpkg is heavier than the library itself, or small
patches to upstream code that need to be carried in-tree. Its README
documents each vendored item's origin, version, and reason for vendoring.

### CMake integration

A CMake preset activates vcpkg by pointing `CMAKE_TOOLCHAIN_FILE` at
`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`. After that, `cmake/` modules
and package `CMakeLists.txt` files call `find_package(OpenCL)`, `find_package(unity)`,
etc., exactly as they would for system-provided libraries. This keeps the
build system agnostic of how dependencies are ultimately resolved.

## 14. Open Design Decisions

These must be settled before the skeleton is populated with actual content.
Each is a small but meaningful choice.

### 14.1 C standard version

The realistic options are **C11** or **C17** (the latter is a bugfix release
of the former, identical semantically in practice). C23 is possible but
compiler support is uneven. Recommendation: **C11** as the declared minimum.

Note: `core/types.h` and `core/rng.h` must compile as OpenCL C (based on
C99). This does not constrain the rest of `core`, which can use C11 freely.

### 14.2 Unit test framework

Candidates:

| Framework | Pros | Cons |
|---|---|---|
| Unity | Minimal, single-header, trivial CMake integration | Spartan features |
| cmocka | Supports mocks, more assertion forms | Heavier dependency |
| Criterion | Parameterized tests, nice output | Linux-focused |
| Check | Classic, mature | Verbose, fork-based runner |

Recommendation: **Unity** for its minimalism and clean integration.

### 14.3 Configuration file format

TOML (via a small library like `tomlc99`) or INI (simpler, parseable by hand).
Biosim4 originally used INI. Recommendation: **TOML** — more expressive,
still human-readable, widely understood.

### 14.4 OpenCL version target

The GPU design document assumes OpenCL 1.2 at minimum, with 2.0 preferred for
generic atomics. The repository needs to commit to a specific level. OpenCL
1.2 remains the broadest-compatibility option; OpenCL 2.0+ opens up
shared-virtual-memory and richer atomics but reduces device support.

Recommendation: **OpenCL 1.2** as required, 2.0 features used conditionally
at kernel-compile time via `#ifdef __OPENCL_VERSION__` guards.

### 14.5 Snapshot binary format

A small ADR is needed. Proposed: magic number, version header, per-buffer
length-prefixed blocks in SoA order. Byte order is little-endian (the
dominant platform). The format is consumed by both simulators and by `viz`,
so it is a cross-package contract.

### 14.6 License

Not decided. BioSim4's original license is a starting point if the code is
derived from it; a permissive license (MIT, Apache-2.0) is the natural
choice otherwise.

## 15. Portability

Portability is a first-class concern of the repository structure, not an
afterthought to be retrofitted.

### 15.1 Target platform matrix

| Platform | CPU | Status |
|---|---|---|
| Linux | x86-64 | Primary development target |
| Windows | x86-64 | First-class target (local testing) |
| Linux | ARM64 | Second-wave target (cloud instances) |
| Windows | ARM64 | Best-effort; CI-enabled when runners become mature |
| macOS | x86-64 / ARM64 | Not targeted initially; Apple deprecated OpenCL |

The structure is designed so that adding a platform to the matrix is a
change to `CMakePresets.json`, `vcpkg.json`, and the GitHub Actions
workflows only — no directory needs to move, no package needs to be split.

### 15.2 Compiler matrix

| Compiler | Minimum version | Notes |
|---|---|---|
| GCC | 9 | Full C11, AddressSanitizer, UBSan, TSan |
| Clang | 10 | Same as GCC, plus usable on both Linux and Windows |
| MSVC | 2019 16.8 | Full C11; AddressSanitizer since 16.9; no UBSan / TSan |

The `cmake/CompilerWarnings.cmake` and `cmake/Sanitizers.cmake` modules
branch on `CMAKE_C_COMPILER_ID` to emit the appropriate flags and to
advertise which sanitizers are actually available.

### 15.3 Library portability

Every third-party dependency chosen so far is inherently cross-platform:

- **OpenCL** — Khronos standard; same API on NVIDIA, AMD, Intel, Mali, and
  PoCL. On Windows, the ICD loader is provided by vcpkg's `opencl` package
  (which pulls `opencl-headers` and `opencl-icd-loader`), eliminating the
  common "works on my Linux" trap.
- **Unity** (test framework) — pure C, single-header; runs anywhere.
- **TOML parser** (e.g. `tomlc99`) — pure C; runs anywhere.
- **CMake**, **vcpkg**, **clang-format**, **clang-tidy** — all officially
  support Linux, Windows, and ARM64.

### 15.4 Platform-specific concerns and how they are contained

**Path handling.** The runtime kernel loader (`kernels_registry.c`) must
resolve the path to the `.cl` files at startup. It does so by querying the
executable's own location — `GetModuleFileNameW` on Windows, `/proc/self/exe`
on Linux. The platform-specific code is isolated to one function inside
`kernels_registry.c`; everything else works in terms of the resolved
absolute path. CMake's `configure_file` injects the install-relative path
constant.

**Line endings.** `.gitattributes` declares `* text=auto` plus explicit
`*.sh text eol=lf` (in case any shell script is ever added) and
`*.cl text eol=lf` for kernel sources. Text files are normalized to LF in
the repository and checked out in the platform's native form.

**Endianness.** x86-64 and ARM64 are both little-endian in practice under
Linux and Windows. The binary snapshot format specifies little-endian
explicitly, which keeps it valid on the entire target matrix. Any future
big-endian platform (not planned) would need an explicit byte-swap pass in
`snapshot.c`.

**OpenCL installation.** Rather than scripting installation, `docs/build.md`
gives copy-pastable command lines per platform:
- Linux: system package manager plus an ICD (PoCL for CPU-only testing,
  vendor driver for GPU).
- Windows: either vcpkg (recommended) or vendor SDK.
  The vcpkg manifest handles headers and ICD loader automatically once the
  toolchain file is set.

**ARM64 caveat — typically no discrete GPU.** Cloud ARM instances
(AWS Graviton, Ampere Altra, Oracle Ampere A1) are pure CPU. The `ocl-cpu`
preset described in Section 10 uses **PoCL**, an OpenCL CPU runtime that
fully supports ARM64, making the GPU simulator testable on ARM servers with
no GPU present. This is also the recommended setup for CI on non-GPU
runners.

### 15.5 CI matrix

The GitHub Actions workflows exercise the portability claims continuously:

| Workflow job | OS runner | Compiler | OpenCL runtime |
|---|---|---|---|
| `linux-x64-gcc` | `ubuntu-latest` | GCC | PoCL (CPU) |
| `linux-x64-clang` | `ubuntu-latest` | Clang | PoCL (CPU) |
| `windows-x64-msvc` | `windows-latest` | MSVC | vcpkg-provided ICD |
| `linux-arm64-gcc` | `ubuntu-24.04-arm` | GCC | PoCL (CPU) |

The last row is enabled when ARM support becomes a priority. Every job
builds all packages, runs the test suite, and runs the cross-simulator
equivalence test (Section 6) — which is the ultimate portability check: if
the GPU kernel under PoCL and the stepper on the host CPU produce matching
outputs on all four configurations, the port is behaviorally consistent
across the entire matrix.

### 15.6 Non-portable code is quarantined

Platform-specific code, when unavoidable, follows two rules:

1. **It is wrapped behind a small function in a single `.c` file**, never
   spread across modules.
2. **The surrounding code is written against the wrapper's portable
   signature**, not the underlying platform API.

The only expected platform-specific code under this rule is the
executable-path resolution in `kernels_registry.c` (Section 15.4). If a
second location ever needs it, it moves to `core/platform.h` with a clean
portable API.

## 16. Implementation Conventions

This section lists the structural rules that constrain how code is laid out
inside each package. They are **invariants** — stable across the life of the
project — and are meant to be respected by every contributor, human or
otherwise. Whenever possible, a rule is also enforced by the build system or
the CI, not just by prose.

### 16.1 Package boundaries and dependency direction

- `core` depends only on the C standard library.
- `sim-gpu` depends on `core` + OpenCL.
- `sim-stepper` depends on `core` only.
- `viz` (future) depends on `core` and on a rendering library.
- Reverse dependencies are forbidden. `core` must never `#include` anything
  from `sim-gpu`, `sim-stepper`, or `viz`.

**Enforcement.** The CMake `target_link_libraries` graph expresses this
directly. A violation fails the link step. A lightweight CI check can also
`grep` for disallowed includes to give a clearer error message earlier.

### 16.2 Public vs. private headers

- **Public headers** live under `include/biosim/<package>/`. They form the
  API consumed by other packages and by tests.
- **Private headers** live alongside their implementation in `src/`. They
  are never visible outside the package.
- A header in `include/` must compile standalone (every symbol it uses is
  either declared locally or included explicitly).

### 16.3 Host/device portability of shared modules

- Modules whose symbols are called from OpenCL kernels (the POD types and
  the RNG, plus any future sharing) must compile both as C11 host code and
  as OpenCL C.
- This means: no `<stdio.h>`, no `<stdlib.h>`, no `<string.h>`, no host-only
  macros inside those headers. Only fixed-width integer types, arithmetic,
  and inline functions.
- Every concerned header carries a prologue comment flagging the constraint
  so future edits don't silently break OpenCL compilation.

### 16.4 No mutable global state in `core`

- `core` functions take their state by parameter. No file-scope mutable
  variables, no singletons, no thread-local pseudo-globals.
- Rationale: `core` is called from two different execution contexts (GPU
  host thread, single-threaded stepper) and may later be called from a
  third (GPU kernel compilation). Hidden global state would silently make
  the two simulators behave differently.

### 16.5 Tests mirror sources

- When a source module `foo` in a package's `src/` has non-trivial logic, a
  corresponding `test_foo.c` lives in the package's `tests/`.
- Tests that span modules (integration-style) are named by intent, not by
  source file (e.g. `test_cross_simulator_equivalence.c`).
- The mirror rule is a soft convention, not a build-enforced rule — it
  applies where it aids navigation, not dogmatically.

### 16.6 Modern CMake — target-first, no globals

- Every property (includes, defines, compile flags, features) is attached
  to a target via `target_*` commands.
- `include_directories()`, `add_definitions()`, and
  `link_directories()` at the top level are banned.
- Dependencies between targets are expressed with `target_link_libraries`
  using `PUBLIC` / `PRIVATE` / `INTERFACE` qualifiers; transitive propagation
  is the mechanism, not variable passing.

### 16.7 Naming conventions

- **Files and directories:** `snake_case.c`, `snake_case.h`. No hyphens in C
  source file names (they confuse some build tools and debuggers). Package
  directory names can use hyphens (`sim-gpu`, `sim-stepper`) because they
  are not C identifiers.
- **Public API symbols:** prefixed with `biosim_` to avoid collisions in
  consumer code. Example: `biosim_genome_mutate`, not `mutate_genome`.
- **Types:** the same prefix, `biosim_coord_t`, `biosim_gene_t`, etc., with
  a `_t` suffix for typedef'd aggregates.
- **OpenCL kernel entry points:** prefixed `k_` to visually distinguish them
  from host functions in logs and diagnostics. Example: `k_feedforward`,
  `k_movement`.

### 16.8 File length and cohesion

- No hard line limit. A file should contain exactly one cohesive module's
  worth of code. If a `.c` file drifts past ~800 lines or accumulates
  unrelated responsibilities, it is a candidate for splitting.
- Splitting is an implementation decision made when the evidence is in the
  code, not preemptively.

### 16.9 Error handling

- All functions that can fail return a status code (`biosim_status_t` or an
  equivalent enum in `core`). No asserts as a substitute for error
  handling.
- Asserts are permitted for invariants that would indicate a bug
  (`assert(alive[i] == 0 || alive[i] == 1)`), not for recoverable runtime
  conditions.

### 16.10 What this section deliberately does not prescribe

- Specific `.c` / `.h` file names inside each package. That is an
  implementation decision made at coding time, guided by the module roles
  described in Sections 5, 6, and 7.
- The exact content of any given file.
- The order of functions within a file.
- Comment style beyond the portability-prologue rule of Section 16.3.
