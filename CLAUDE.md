# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with
code in this repository.

## Project

GPU port of [biosim4](https://github.com/davidrmiller/biosim4) (by David R.
Miller) using OpenCL. The project is in the **design phase** — no source code
exists yet. Design documents in `docs/design/` are the authoritative
specification; implementation follows them.

## Build System (not yet implemented)

Once scaffolded, the build will use **CMake + vcpkg**:

```sh
# Configure (VCPKG_ROOT must be set)
cmake --preset debug    # or: release, asan, ocl-cpu, ocl-gpu

# Build
cmake --build build

# Common targets
cmake --build build --target check      # build + run all tests
cmake --build build --target format     # run clang-format
cmake --build build --target lint       # run clang-tidy
cmake --build build --target benchmark  # run benchmarks
```

The `ocl-cpu` preset (uses PoCL) is the primary dev/CI path on machines
without a discrete GPU.

## Architecture

Monorepo under `packages/` with four packages and a strict acyclic dependency
graph:

```
core (static lib, libc only)
  ├── sim-gpu   (executable — OpenCL batch simulator)
  └── sim-stepper (executable — single-threaded CPU reference)
                    └── viz (future — consumes stepper trace)
```

**`core`** — all shared simulation logic: genome operators, neural network
compilation, sensor/action catalogues, challenge evaluation, portable xorshift64
RNG, snapshot serialization. Nothing about *how* the simulation is scheduled.

**`sim-gpu`** — OpenCL host orchestration + 5-kernel-per-step pipeline:
`k_feedforward` → `k_movement` → `k_grid_cleanup` → `k_signal_fade` →
`k_challenges`. Kernel sources live in `packages/sim-gpu/kernels/` and are
compiled at runtime by the OpenCL driver (not the host compiler).

**`sim-stepper`** — deterministic single-threaded CPU reference. Processes
agents in increasing-index order with immediate effect application. Serves as
ground truth for the cross-simulator equivalence test.

The **cross-simulator equivalence test** in `sim-gpu/tests/` is the most
critical test: runs one step on both simulators from the same input and verifies
per-agent neuron outputs match within tolerance.

## GPU Data Model

Per-agent data uses **Structure of Arrays (SoA)** for memory coalescing. Genome
storage is transposed: `genome[gene_slot][agent_i]`, not `genome[agent_i][gene_j]`.

Movement conflict resolution: atomic CAS (no move queues). Death writes:
idempotent (no death queues). Generation boundary (reproduction, mutation,
spawn) stays on the host.

## Code Conventions (§12 of `docs/design/01-repository-structure.md`)

- **Files:** `snake_case.c`, `snake_case.h`
- **Public API:** `biosim_` prefix — e.g. `biosim_genome_mutate`
- **Types:** `biosim_*_t` — e.g. `biosim_coord_t`
- **OpenCL kernels:** `k_` prefix — e.g. `k_feedforward`
- **No mutable global state** in `core` — all state passed by parameter
- **Error handling:** functions return `biosim_status_t`; asserts only for
  invariants that indicate bugs
- **Host/device portability:** headers shared with OpenCL kernels (POD types,
  RNG) must compile as both C11 and OpenCL C — no `<stdio.h>`, `<stdlib.h>`,
  `<string.h>`. Mark such headers with a prologue comment.
- **CMake:** target-first (`target_*` commands only); no `include_directories()`
  or `add_definitions()` at top level

## Key Files

- `docs/design/01-repository-structure.md` — monorepo layout, package
  boundaries, build conventions, naming rules
- `docs/design/02-gpu-data-model.md` — SoA layout, kernel decomposition,
  conflict resolution strategy
- `docs/design/03-portable-build.md` — CMake/vcpkg setup, preset list,
  CI matrix, platform-specific concerns
- `docs/design/04-external-icd.md` — open decisions: config file format
  (TOML recommended) and snapshot binary format
- `docs/design/adr/` — Architecture Decision Records for specific choices
