# Repository Structure Design Document

**Purpose:** Define the initial layout of the Git repository for the BioSim4
GPU port. Establishes the monorepo boundaries, the shared `core` library, the
placement of OpenCL kernels, and the documentation organization. Meant to be
read before filling in any actual source or build files.
## Table of Contents

1. [Goals and Non-Goals](#1-goals-and-non-goals)
2. [Guiding Principles](#2-guiding-principles)
3. [Top-Level Tree](#3-top-level-tree)
4. [Package Layout — `packages/`](#4-package-layout--packages)
5. [The `core` Package — Rationale and Boundaries](#5-the-core-package--rationale-and-boundaries)
6. [The `params` Package](#6-the-params-package)
7. [The `sim-gpu` Package](#7-the-sim-gpu-package)
8. [The `sim-stepper` Package](#8-the-sim-stepper-package)
9. [The `viz` Package — Placeholder](#9-the-viz-package--placeholder)
10. [Documentation Organization](#10-documentation-organization)
11. [Tooling — `tools/` and `benchmarks/`](#11-tooling--tools-and-benchmarks)
12. [Data — Configurations and Snapshots](#12-data--configurations-and-snapshots)

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
`sim-gpu` package.

The kernel registry follows a two-level lookup at startup:

1. **Filesystem override (checked first):** if `.cl` files are found alongside
   the binary (same directory), they are used. This lets a developer edit a
   kernel and re-run without recompiling — no CMake involvement.
2. **Embedded fallback:** kernel sources are always embedded in the binary as
   C string literals via `EmbedKernels.cmake`. If no filesystem files are
   present, the embedded sources are used. This guarantees a self-contained
   binary with no deployment of auxiliary files.

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
├── CMakePresets.json                  ← build configurations (see 03-portable-build.md)
├── vcpkg.json                         ← dependency manifest (see 03-portable-build.md)
│
├── .github/
│   └── workflows/
│       ├── build.yml                  ← CI: see 03-portable-build.md
│       └── tests.yml                  ← CI: see 03-portable-build.md
│
├── docs/                              ← see Section 9
├── cmake/                             ← reusable CMake modules, see 03-portable-build.md
├── packages/                          ← see Section 4
├── data/                              ← see Section 11
├── third_party/                       ← see 03-portable-build.md
├── benchmarks/                        ← see Section 10
└── tools/                             ← see Section 10
```

### Top-level file conventions

| File | Purpose |
|---|---|
| `README.md` | Elevator pitch, quickstart, pointer to `docs/` |
| `LICENSE` | MIT |
| `CHANGELOG.md` | Keep-a-Changelog format, per-release entries |
| `.gitignore` | Standard C + CMake ignores plus `data/snapshots/*` |
| `.gitattributes` | Enforce LF line endings on every platform (see [`03-portable-build.md`](03-portable-build.md)) |
| `.editorconfig` | Indent, trailing whitespace, final newline for all editors |
| `.clang-format` | Formatting rules, invoked via the `format` CMake custom target |
| `.clang-tidy` | Static analysis rules, invoked via the `lint` CMake custom target |
| `CMakeLists.txt` | Project declaration, global options, `add_subdirectory` for each package |
| `CMakePresets.json` | Build configurations for IDEs and CI |
| `vcpkg.json` | Declarative list of C/C++ dependencies — resolved by vcpkg on all platforms |

The build-related files are documented in detail in [`03-portable-build.md`](03-portable-build.md).

## 4. Package Layout — `packages/`

```
packages/
├── core/                              ← shared simulation logic (static library)
├── params/                            ← CLI/TOML/parameter management (static library)
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
                   ┌─────▼────┐
                   │  params  │    (static library; PRIVATE: argtable3, tomlc17)
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
`params` depends on `core` (PUBLIC) and on argtable3 and tomlc17 (PRIVATE).
`sim-gpu` depends on `core` and `params`, and on OpenCL.
`sim-stepper` depends on `core` and `params`.
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
| Shared POD types | `Coord`, `Dir`, gene bit-layout macros (`gene.h`, shared with OpenCL kernels), and any other packed value types |
| Simulation context | `biosim_context_t` — complete simulation state: allocation-time configuration, runtime configuration, per-generation state (step counter, generation counter, mutation rate, RNG), and all resource buffers (`agents`, `grid`, `genome`, `nnet`, `signal`). `biosim_context_create` allocates heap resources from the pre-populated configuration fields; `biosim_context_free` releases them |
| Step logic | `step_agent()`: advance one simulation step for one agent — evaluate sensors, run feedforward, apply actions, finalise movement, invoke challenge step hook |
| Generation logic | `biosim_context_advance_gen()`: evaluate the challenge for all alive agents, collect generation statistics (`biosim_gen_stats_t`), reproduce survivors (asexual: copy + mutate), recompile neural networks, and respawn the full population |
| Genome operators | Mutation (point, insertion, deletion), crossover |
| Neural network compilation | Culling of dead neurons, connection reordering, per-agent NN build, phenotypic fingerprint |
| Abstract grid API | Query, neighborhood iteration, cell write — backing store provided by each simulator |
| Abstract signal API | Read, increment, fade — backing store provided by each simulator |
| Sensor and action catalogues | Enums, constants, dispatch tables common to both simulators |
| Challenge evaluation | The logic that decides survival and updates `challenge_bits` |
| Portable RNG | xorshift64, with an implementation usable verbatim on both host and device |
| Snapshot format | Serialization and deserialization of a full population to/from the versioned binary format |

### What belongs in `core`

`core` is the **single-thread reference implementation** of the simulation.
It owns the full simulation state and all logic needed to advance it. Code
that both simulators need with identical behavior (genome operators, neural
network compilation, challenge evaluation, RNG) belongs here by definition.

### What does *not* belong in `core`

- OpenCL setup, kernel compilation, buffer management → `sim-gpu`.
- The per-sim.step orchestration loop (kernel pipeline vs. single-threaded
  stepping) → each simulator's own code.
- Visualization formats, image encoders → `viz`.
- CLI argument parsing, TOML loading, parameter table management → `params`
  package; each simulator's `main.c` defines its own exhaustive entry table
  and calls `biosim_params_parse`.

### The host/device portability constraint

The modules that are shared with OpenCL kernels — the POD types and the RNG —
must compile under both the host C compiler and the OpenCL compiler. The
headers that carry them are kept in a **C99 portable subset** with no
host-specific includes (no `<stdio.h>`, no `<stdlib.h>`). The rest of `core`
may use C11 features freely. This constraint is important enough to call out
in the header prologue of any file concerned. See [`03-portable-build.md`](03-portable-build.md)
for the language-level statement of this rule.

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

## 6. The `params` Package

```
packages/params/
├── CMakeLists.txt
├── include/
│   └── biosim/params/     ← single public header: params.h
├── src/
│   ├── params.c           ← lifecycle, setters, getters, introspection
│   ├── cli.c              ← CLI parsing (argtable3); three-pass orchestration
│   └── toml.c             ← TOML loader (tomlc17)
└── tests/
    ├── CMakeLists.txt
    ├── test_params.c
    ├── test_cli.c
    └── fixtures/
        └── basic.toml
```

### Module responsibilities

| Role | Description |
|---|---|
| Parameter table | `biosim_params_t` — dynamically-sized array of `biosim_param_entry_t`; lifecycle, setters, getters, and introspection |
| TOML loading | Reads a TOML file into the parameter table via `--config`; table-driven — no hardcoded key list |
| CLI generation | Derives a full argtable3 argument table from the entries; three-pass resolution (defaults → TOML → CLI) |
| Parsing entry point | `biosim_params_parse(p, progname, version, argc, argv)` — the single call a simulator `main.c` makes |

### The "each main defines its own table" contract

Each simulator's `main.c` declares a `static const biosim_param_entry_t[]`
that contains **every** parameter it needs — simulation defaults and
simulator-specific entries alike.

### Dependencies

`params` links `PUBLIC core` (needs `biosim_status_t`) and `PRIVATE
argtable3::argtable3` and `PRIVATE tomlc17`. Consumers see only
`biosim/params/params.h`; argtable3 and tomlc17 are fully encapsulated.

## 7. The `sim-gpu` Package

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
| Per-step pipeline | The orchestration of kernels K1, K2 ... Kn for one sim.step |
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
data-model document ([05-gpu-data-model.md](05-gpu-data-model.md)), not
an implementation detail. Each `.cl` file is a named chapter of that design.
Renaming or merging kernels is a design change that deserves a design update.

### Kernel distribution

Kernel sources are always embedded in the binary as C string literals
(generated by `EmbedKernels.cmake` at build time). At startup the kernel
registry performs a two-level lookup:

1. **Filesystem override:** look for `.cl` files alongside the binary. If found,
   compile and use them. Enables the edit-run loop during kernel development
   without rebuilding the host binary.
2. **Embedded fallback:** if no filesystem files are present, compile and use
   the embedded string literals. Guarantees a self-contained binary that
   requires no auxiliary files at deployment.

The rest of the codebase calls only the kernel registry API and is unaware of
which source was used.

### Cross-simulator equivalence test

The single most important test in the entire repository lives in this
package's `tests/`. It runs one simulation step with the GPU simulator and
one with the stepper on the same input, then verifies that the per-agent
neuron outputs match within a tolerance. Without this test, behavioral drift
between the two simulators becomes invisible.

## 8. The `sim-stepper` Package

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
| Orchestration loop | `main()`: parse parameters, initialise `biosim_context_t`, run the triple-nested generation > step > agent loop by calling `step_agent` and `biosim_context_advance_gen` from `core` |
| CLI entry point | Argument parsing and TOML loading via the `params` package |

### Purpose

The stepper owns nothing but the orchestration loop: it parses parameters,
fills `biosim_context_t`, and drives the triple-nested `for` loop. All
simulation logic lives in `core`.

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

## 9. The `viz` Package — Placeholder

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

## 10. Documentation Organization

```
docs/
├── README.md                      ← index of the doc, reading order
├── design/
│   ├── 01-repository-structure.md    ← this document
│   ├── 02-implementation-conventions.md ← code layout invariants
│   ├── 03-portable-build.md          ← build chain and portability
│   ├── 04-legacy-data-model       ← analysis of the legacy CPU/AoS model
│   ├── 05-gpu-data-model.md          ← the GPU/SoA proposal
│   └── 06-external-icd.md            ← external interface formats (config, snapshot)
├── build.md                       ← prerequisites, CMake options, build guide
├── usage.md                       ← CLI reference for both simulators
├── architecture.md                ← monorepo + package explanation
└── gpu-primer.md                  ← glossary of GPU concepts
```

### The numbered `design/` files

Design documents are prefixed with a two-digit number that defines reading
order. New design documents append to the sequence. They are long-form,
narrative documents that explain **why** a design choice was made.

### `gpu-primer.md` — the shared glossary

A single file defines every GPU-specific term used in the rest of the
documentation: *warp*, *wavefront*, *coalescing*, *warp divergence*, *warp
coherence*, *pointer chasing*, *AoS/SoA*, *atomic contention*, *local memory*,
*image object*, *texture cache*.

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

## 11. Tooling — `tools/` and `benchmarks/`

Developer workflow commands (format, lint, build, test, benchmark) are
exposed as CMake custom targets, not as shell scripts. See
[`03-portable-build.md`](03-portable-build.md) for the list of targets and how
they are invoked.

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

The TOML format is documented in [`06-external-icd.md`](06-external-icd.md).
The `params` package loader handles it.

### Snapshots are not versioned

Population snapshots are binary files, potentially megabytes each, produced
on every run. They belong in the working tree but not in the commit history.
The nested `.gitignore` excludes everything except `.gitkeep`.
